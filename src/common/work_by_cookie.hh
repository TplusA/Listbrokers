/*
 * Copyright (C) 2022, 2023  T+A elektroakustik GmbH & Co. KG
 *
 * This file is part of T+A List Brokers.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */

#ifndef WORK_BY_COOKIE_HH
#define WORK_BY_COOKIE_HH

#include "dbus_async_workqueue.hh"
#include "de_tahifi_lists_errors.hh"

#include <unordered_map>
#include <future>

#include <gio/gio.h>

namespace DBusAsync
{

class BadCookieError: public std::exception
{
  private:
    const char *how_bad_;

  public:
    explicit BadCookieError(const char *how_bad): how_bad_(how_bad) {}
    const char *what() const noexcept final override { return how_bad_; }
};

struct TimeoutError: public std::exception {};

enum class WaitForMode
{
    ALLOW_SYNC_PROCESSING,
    NO_SYNC,
};

/*!
 * Asynchronous work associated with a magic cookie.
 *
 * Objects of classes derived from this base class should be managed by the
 * #DBusAsync::CookieJar singleton which can be obtained via
 * #DBusAsync::get_cookie_jar_singleton().
 *
 * Derived classes must put an error code by calling
 * #DBusAsync::CookiedWorkBase::put_error() when the result is available. This
 * is usually the case in the #DBusAsync::Work::do_run() implementation.
 */
class CookiedWorkBase: public DBusAsync::Work
{
  private:
    ListError error_on_done_;

  protected:
    explicit CookiedWorkBase(const std::string &name):
        DBusAsync::Work(name)
    {}

  public:
    CookiedWorkBase(CookiedWorkBase &&) = delete;
    CookiedWorkBase &operator=(CookiedWorkBase &&) = delete;
    virtual ~CookiedWorkBase() = default;

    bool success() const
    {
        return get_state() == State::DONE && !error_on_done_.failed();
    }

    ListError::Code get_error_code() const
    {
        switch(get_state())
        {
          case State::RUNNABLE:
            return ListError::Code::BUSY;

          case State::RUNNING:
            return ListError::Code::BUSY_500;

          case State::DONE:
            return error_on_done_.get();

          case State::CANCELING:
          case State::CANCELED:
            return ListError::Code::INTERRUPTED;
        }

        return ListError::Code::INTERNAL;
    }

    /*!
     * Emit D-Bus signal about data availibility.
     */
    virtual void notify_data_available(uint32_t cookie) const = 0;

    /*!
     * Emit D-Bus signal about data error.
     */
    virtual void notify_data_error(uint32_t cookie, ListError::Code error) const = 0;

  protected:
    /* must be called by derived class when result is available */
    void put_error(ListError error) { error_on_done_ = error; }
};

/*!
 * Asynchronous work associated with a magic cookie that can be waited for.
 *
 * Derived classes must place their result into the
 * #DBusAsync::CookiedWorkWithFutureResultBase::promise_ member, which it
 * usually is in its #DBusAsync::Work::do_run() implementation.
 *
 * Derived classes must call
 * #DBusAsync::CookiedWorkWithFutureResultBase::begin_cancel_request() from
 * their #DBusAsync::Work::do_cancel() implementation.
 *
 * Please also check the additional requirements for classes derived from
 * #DBusAsync::CookiedWorkBase.
 */
template <typename RT>
class CookiedWorkWithFutureResultBase: public CookiedWorkBase
{
  public:
    using ResultType = RT;

  protected:
    /* must be set by derived class when result is available */
    std::promise<ResultType> promise_;

  private:
    std::future<ResultType> future_;
    bool cancellation_requested_;

  public:
    explicit CookiedWorkWithFutureResultBase(const std::string &name):
        CookiedWorkBase(name),
        promise_(std::promise<ResultType>()),
        future_(promise_.get_future()),
        cancellation_requested_(false)
    {}

    /*!
     * Wait for completion of work.
     *
     * \param timeout
     *     Wait for up to this duration. In case the timeout expires, a
     *     #DBusAsync::TimeoutError is thrown.
     *
     * \param mode
     *     How to wait. In case of #WaitForMode::NO_SYNC, the work is expected
     *     to be done by some thread in the background; a timeout may occur
     *     while waiting. In case of #WaitForMode::ALLOW_SYNC_PROCESSING, the
     *     caller of this function is going to process the work if it is in
     *     runnable state. Note that this mode must not be used for work which
     *     has been put into a #DBusAsync::WorkQueue.
     */
    auto wait_for(const std::chrono::milliseconds &timeout,
                  DBusAsync::WaitForMode mode)
    {
        LOGGED_LOCK_CONTEXT_HINT;
        with_reply_path_tracker<void>(
            [] (auto &work_lock, auto &rpt) { rpt.set_waiting_for_result(work_lock); });

        switch(future_.wait_for(timeout))
        {
          case std::future_status::timeout:
            if(mode != DBusAsync::WaitForMode::ALLOW_SYNC_PROCESSING)
                break;

            /* fall-through */

          case std::future_status::deferred:
            if(mode == DBusAsync::WaitForMode::NO_SYNC)
                break;

            {
            LOGGED_LOCK_CONTEXT_HINT;
            LoggedLock::UniqueLock<LoggedLock::Mutex> work_lock(lock_);

            switch(get_state())
            {
              case State::RUNNABLE:
                run(std::move(work_lock));
                break;

              case State::RUNNING:
              case State::CANCELING:
                work_lock.unlock();
                break;

              case State::DONE:
                MSG_BUG("Work deferred, but marked DONE");
                work_lock.unlock();
                break;

              case State::CANCELED:
                MSG_BUG("Work deferred, but marked CANCELED");
                work_lock.unlock();
                break;
            }
            }

            /* fall-through */

          case std::future_status::ready:
            return future_.get();
        }

        throw DBusAsync::TimeoutError();
    }

    auto take_result_from_fast_path()
    {
        msg_log_assert(future_.valid());
        future_.wait();
        return future_.get();
    }

  protected:
    /*!
     * Set up for cancelation.
     *
     * Must be called from the #DBusAsync::Work::do_cancel() implementation of
     * the derived class.
     */
    bool begin_cancel_request()
    {
        if(cancellation_requested_)
        {
            MSG_BUG("Multiple cancellation requests");
            return false;
        }

        cancellation_requested_ = true;
        return true;
    }

    bool was_canceled() const { return cancellation_requested_; }
};

class CookieJar
{
  private:
    LoggedLock::Mutex lock_;
    std::atomic<uint32_t> next_free_cookie_;
    std::unordered_map<uint32_t, std::shared_ptr<CookiedWorkBase>> work_by_cookie_;

  public:
    CookieJar(const CookieJar &) = delete;
    CookieJar(CookieJar &&) = delete;
    CookieJar &operator=(const CookieJar &) = delete;
    CookieJar &operator=(CookieJar &&) = delete;

    explicit CookieJar():
        next_free_cookie_(1)
    {
        LoggedLock::configure(lock_, "CookieJar", MESSAGE_LEVEL_DEBUG);
    }

    enum class DataAvailableNotificationMode
    {
        NEVER,          /* never notify (for pure synchronous interfaces) */
        AFTER_TIMEOUT,  /* for interfaces with fast path option */
        ALWAYS,         /* always notify (for pure asynchronous interfaces) */
    };

    /*!
     * Promise to do work in exchange for a cookie.
     *
     * The cookie is associated with the work, i.e., it identifies the work
     * item passed this function. It may be eaten as soon as the work has been
     * completed.
     *
     * \param work
     *     A work item to be processed.
     *
     * \param mode
     *     How to notify D-Bus clients about finished work. This parameter is
     *     passed on to #DBusAsync::CookieJar::work_done_notification(). In
     *     case of
     *     #DBusAsync::CookieJar::DataAvailableNotificationMode::ALWAYS, the
     *     \c de.tahifi.Lists.Navigation.DataAvailable D-Bus signal is always
     *     emitted when the work has been processed. In case of
     *     #DBusAsync::CookieJar::DataAvailableNotificationMode::AFTER_TIMEOUT,
     *     the signal is emitted when the work has been finished, but only if
     *     the work has been waited for before and a timeout occurred (this
     *     mode is required for fast path implementations). In case of
     *     #DBusAsync::CookieJar::DataAvailableNotificationMode::NEVER, the
     *     D-Bus signal is never emitted automatically (this mode is required
     *     for implementations without fast path provisions).
     *
     * \returns
     *     A sufficiently unique value which identifies \p work. It can be used
     *     to retrieve the results later, or cancel the work.
     */
    uint32_t pick_cookie_for_work(std::shared_ptr<CookiedWorkBase> &&work,
                                  DataAvailableNotificationMode mode);

    /*!
     * The work associated with the cookie is not going to be done.
     *
     * The work item the cookie was given out for is canceled. Standard
     * cancelation mechanisms apply and the work item is going to be removed
     * from our cookie jar later.
     *
     * \param cookie
     *     No one is going to eat this.
     */
    void cookie_not_wanted(uint32_t cookie);

    enum class EatMode
    {
        WILL_WORK_FOR_COOKIES,
        MY_SLAVE_DOES_THE_ACTUAL_WORK,
    };

    /*!
     * Try to eat the cookie and get the result of the work.
     *
     * The work result is obtained by calling the \c wait_for() function of the
     * work item. We wait for the result for up to 150 ms so that results of
     * very small amounts of work can be made available in the context of the
     * caller which has just added the work item. In case the result is
     * unavailable after 150 ms, a #DBusAsync::TimeoutError exception is
     * thrown.
     *
     * A #DBusAsync::TimeoutError is also thrown if the work item is not
     * processed in a thread and \p allow_sync is set to false.
     *
     * In general, this function should be called when the work is known to be
     * have done. Whether or not the timeout feature is useful depends entirely
     * on the context; its intended use is the optimization of D-Bus traffic.
     *
     * \tparam WorkType
     *     The work item is cast to this type, which must be derived from
     *     #DBusAsync::CookiedWorkBase, and the result is returned using its
     *     \c wait_for() function member.
     *
     * \param cookie
     *     A cookie previously returned by
     *     #DBusAsync::CookieJar::pick_cookie_for_work(). In case the cookie is
     *     invalid or the work item associated with the cookie is not of type
     *     \p WorkType, a #DBusAsync::BadCookieError exception will be thrown.
     *
     * \param eat_mode
     *     If the work item is not processed by a worker thread, the caller
     *     must decide whether or not it wants to process the work in its own
     *     context. In case of
     *     #DBusAsync::CookieJar::EatMode::WILL_WORK_FOR_COOKIES, the caller
     *     will do the work and the function call will block until the work has
     *     been completed or canceled. In case of
     *     #DBusAsync::CookieJar::EatMode::MY_SLAVE_DOES_THE_ACTUAL_WORK (the
     *     default), the work will remain unprocessed and this function will
     *     throw a #DBusAsync::TimeoutError without actually running into a
     *     timeout.
     *
     * \param on_timeout
     *     In case the work couldn't be finished within the short time granted
     *     for fast path work, this function will be called with its parameter
     *     to the \p cookie. May be \c nullptr (do this in the \c ByCookie
     *     result fetcher methods).
     *
     * \returns
     *     The return value of #NavListsWork<WorkType>::wait_for().
     */
    template <typename WorkType>
    typename WorkType::ResultType
    try_eat(uint32_t cookie, EatMode eat_mode,
            const std::function<void(uint32_t)> &on_timeout)
    {
        if(cookie == 0)
            throw BadCookieError("bad value");

        LOGGED_LOCK_CONTEXT_HINT;
        LoggedLock::UniqueLock<LoggedLock::Mutex> jar_lock(lock_);

        auto work_iter(work_by_cookie_.find(cookie));
        if(work_iter == work_by_cookie_.end())
            throw BadCookieError("unknown");

        msg_log_assert(work_iter->second != nullptr);

        /*
         * IMPORTANT: Do not call any \p WorkType function members while
         *            holding the cookie jar lock to avoid deadlocks!
         */
        auto work = std::dynamic_pointer_cast<WorkType>(work_iter->second);
        if(work == nullptr)
            throw BadCookieError("wrong type");

        jar_lock.unlock();

        try
        {
            /* this may throw a #DBusAsync::TimeoutError */
            auto result(work->wait_for(std::chrono::milliseconds(150),
                                       eat_mode == EatMode::WILL_WORK_FOR_COOKIES
                                       ? WaitForMode::ALLOW_SYNC_PROCESSING
                                       : WaitForMode::NO_SYNC));

            /* we have our result for fast path, so we "eat" our cookie now and
             * remove its associated work item */
            LOGGED_LOCK_CONTEXT_HINT;
            jar_lock.lock();
            work_by_cookie_.erase(cookie);
            return result;
        }
        catch(const TimeoutError &)
        {
            /* the work may have just completed in the background at this
             * point, before the work and cookie jar locks could be
             * re-acquired (and it is necessary to take the work lock *before*
             * taking the jar lock to avoid deadlocks); we need to check if
             * taking the slow path is still possible or if we have timed out
             * after the fact */

            LOGGED_LOCK_CONTEXT_HINT;
            static_cast<DBusAsync::Work *>(work.get())->
                with_reply_path_tracker<bool>(
                    [this, &jar_lock, cookie, &on_timeout]
                    (auto &work_lock, auto &rpt)
                    {
                        return this->try_eat_quickly(jar_lock, cookie,
                                                     on_timeout, work_lock, rpt);
                    });

            /* function has not re-thrown, which means our work has completed
             * just in this moment and its result is available, ready for
             * processing on the fast path; jar lock is still taken at this
             * point */
            LOGGED_LOCK_CONTEXT_HINT;
            auto result(work->take_result_from_fast_path());
            work_by_cookie_.erase(cookie);
            return result;
        }
        catch(...)
        {
            LOGGED_LOCK_CONTEXT_HINT;
            static_cast<DBusAsync::Work *>(work.get())->
                with_reply_path_tracker<DBusAsync::ReplyPathTracker::TakePathResult>(
                    [] (auto &work_lock, auto &rpt)
                    { return rpt.try_take_fast_path(work_lock); });
            throw;
        }
    }

  private:
    bool try_eat_quickly(LoggedLock::UniqueLock<LoggedLock::Mutex> &jar_lock,
                         uint32_t cookie,
                         const std::function<void(uint32_t)> &on_timeout,
                         LoggedLock::UniqueLock<LoggedLock::Mutex> &work_lock,
                         ReplyPathTracker &rpt)
    {
        LOGGED_LOCK_CONTEXT_HINT;
        jar_lock.lock();

        const auto take_path_result = rpt.try_take_slow_path();

        switch(take_path_result)
        {
          case DBusAsync::ReplyPathTracker::TakePathResult::ALREADY_ON_FAST_PATH:
            /* nice, we are done already; DO NOT UNLOCK THE JAR LOCK! */
            return true;

          case DBusAsync::ReplyPathTracker::TakePathResult::TAKEN:
            /*
             * OK, so we are taking the slow path here and announce the cookie
             * to the D-Bus client; the #DBusAsync::ReplyPathTracker is taking
             * care of correct ordering so that the done notification cannot
             * emit the result before we have announced the cookie.
             *
             * It is *still* possible for the work to complete at this time or
             * a millisecond later, very shortly after we have made the
             * decision to take the slow path. This is OK, though, because
             * #DBusAsync::CookieJar::work_done_notification() will have to
             * wait for the jar lock which protects our effort below of sending
             * the cookie to the client, which that function will need to send
             * a notification.
             *
             * This would be bad luck because we'll waste some time on the
             * cookie round trip, but it's entirely possible and valid.
             */
            break;

          case DBusAsync::ReplyPathTracker::TakePathResult::ALREADY_ON_SLOW_PATH_COOKIE_NOT_ANNOUNCED_YET:
            MSG_BUG("Requesting slow path due to timeout, but already taking slow path (phase 1)");
            break;

          case DBusAsync::ReplyPathTracker::TakePathResult::ALREADY_ON_SLOW_PATH_COOKIE_ANNOUNCED:
            MSG_BUG("Requesting slow path due to timeout, but already taking slow path (phase 2)");
            break;

          case DBusAsync::ReplyPathTracker::TakePathResult::ALREADY_ON_SLOW_PATH_READY_ANNOUNCED:
            MSG_BUG("Requesting slow path due to timeout, but already taking slow path (phase 3)");
            break;

          case DBusAsync::ReplyPathTracker::TakePathResult::ALREADY_ON_SLOW_PATH_FETCHING:
            break;

          case DBusAsync::ReplyPathTracker::TakePathResult::INVALID:
            MSG_BUG("Requesting slow path due to timeout, but this is an invalid transition");
            break;
        }

        if(on_timeout != nullptr)
            on_timeout(cookie);

        LOGGED_LOCK_CONTEXT_HINT;
        if(!rpt.slow_path_cookie_sent_to_client(work_lock))
            MSG_BUG("Bad reply path tracker state");

        throw;
    }

    /*!
     * Callback for letting us know that some work item is done.
     *
     * This function is called when the work has been processed or has been
     * canceled. It is frequently called from a work queue, either in the
     * context of a worker thread or of the caller which has generated the work
     * item. In general, no assumptions about context of execution must be made
     * here.
     *
     * \param work_lock
     *     Lock for the work notified. It will have been locked by the caller,
     *     and we are allowed to make use of it where required.
     *
     * \param cookie
     *     Cookie for the work which has been processed.
     *
     * \param mode
     *     How to notify the D-Bus client about completion.
     *
     * \param has_completed
     *     True if the work has completed successfully, false if the work has
     *     been canceled.
     */
    void work_done_notification(
            LoggedLock::UniqueLock<LoggedLock::Mutex> &work_lock,
            uint32_t cookie, DataAvailableNotificationMode mode,
            bool has_completed);

    uint32_t bake_cookie();
};

CookieJar &get_cookie_jar_singleton();

/*!
 * Generic implementation of RNF-style D-Bus methods with fast path.
 *
 * For those D-Bus methods of the request/notify/fetch flavor which have a fast
 * path designed in, this function template should be used to implement it. It
 * handles timeouts correctly and also works for synchronous work queues.
 *
 * Method handlers should check their parameters before calling this function,
 * and error out early in the way specified for the respective handler.
 *
 * \param object, invocation
 *     D-Bus stuff.
 *
 * \param queue
 *     The work queue the work which is triggered by the D-Bus method call
 *     should be put into.
 *
 * \param work
 *     A piece of work to be done.
 *
 * \param fast_path_succeeded
 *     In case the work was finished after a short waiting time, this function
 *     will be called with the result of the work moved to it. In case the work
 *     wasn't finished on time, this function will \e not be called.
 */
template <typename IfaceType, typename WorkType>
static inline void try_fast_path(
        IfaceType *object, GDBusMethodInvocation *invocation,
        DBusAsync::WorkQueue &queue, std::shared_ptr<WorkType> &&work,
        std::function<void(IfaceType *, GDBusMethodInvocation *,
                           typename WorkType::ResultType &&)> &&fast_path_succeeded)
{
    const uint32_t cookie =
        DBusAsync::get_cookie_jar_singleton().pick_cookie_for_work(
            work,
            DBusAsync::CookieJar::DataAvailableNotificationMode::AFTER_TIMEOUT);

    const auto eat_mode =
        queue.add_work(std::move(work), nullptr)
        ? DBusAsync::CookieJar::EatMode::MY_SLAVE_DOES_THE_ACTUAL_WORK
        : DBusAsync::CookieJar::EatMode::WILL_WORK_FOR_COOKIES;

    try
    {
        auto result(DBusAsync::get_cookie_jar_singleton().try_eat<WorkType>(cookie, eat_mode,
            [object, invocation] (uint32_t c)
            {
                WorkType::fast_path_failure(object, invocation, c, ListError::BUSY);
            }));

        /* fast path answer */
        fast_path_succeeded(object, invocation, std::move(result));
    }
    catch(const DBusAsync::TimeoutError &)
    {
        /* handled above */
    }
    catch(const std::exception &e)
    {
        MSG_BUG("Unexpected failure (%d)", __LINE__);
        g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR,
                                              G_DBUS_ERROR_INVALID_ARGS,
                                              "Internal error (%s)", e.what());
    }
    catch(...)
    {
        MSG_BUG("Unexpected failure (%d)", __LINE__);
        g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR,
                                              G_DBUS_ERROR_INVALID_ARGS,
                                              "Internal error (*unknown*)");
    }
}

/*!
 * Generic implementation of the fetch part of RNF-style D-Bus methods.
 *
 * This is the counterpart to #try_fast_path(), executed in a later D-Bus call
 * when the work result is available.
 *
 * Note that it is possible to use this function template for methods which
 * don't implement a fast path. This function template is independent of any
 * fast path treatments.
 *
 * \param object, invocation
 *     D-Bus stuff.
 *
 * \param cookie
 *     The cookie the D-Bus client is asking for.
 *
 * \param finish_call
 *     This function is called if a work result for the given cookie is
 *     available. The result is moved to it. In case the work hasn't been
 *     finished yet, the \c slow_path_failure() function, a static function
 *     defined in \p WorkType, is called. This function is expected to either
 *     complete the D-Bus method call or return a D-Bus error. In case the
 *     cookie is invalid or in case any exception is thrown, a corresponding
 *     D-Bus error is returned to the caller of the method.
 */
template <typename IfaceType, typename WorkType>
static inline void finish_slow_path(
        IfaceType *object, GDBusMethodInvocation *invocation,
        uint32_t cookie,
        std::function<void(IfaceType *, GDBusMethodInvocation *,
                           typename WorkType::ResultType &&)> &&finish_call)
{
    try
    {
        auto result(DBusAsync::get_cookie_jar_singleton().try_eat<WorkType>(
                        cookie,
                        DBusAsync::CookieJar::EatMode::MY_SLAVE_DOES_THE_ACTUAL_WORK,
                        nullptr));

        finish_call(object, invocation, std::move(result));
    }
    catch(const DBusAsync::BadCookieError &e)
    {
        g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR,
                                              G_DBUS_ERROR_INVALID_ARGS,
                                              "Invalid cookie (%s)", e.what());
    }
    catch(const DBusAsync::TimeoutError &)
    {
        WorkType::slow_path_failure(object, invocation, ListError::BUSY);
    }
    catch(const std::exception &e)
    {
        MSG_BUG("Unexpected failure (%d)", __LINE__);
        g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR,
                                              G_DBUS_ERROR_INVALID_ARGS,
                                              "Internal error (%s)", e.what());
    }
    catch(...)
    {
        MSG_BUG("Unexpected failure (%d)", __LINE__);
        g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR,
                                              G_DBUS_ERROR_INVALID_ARGS,
                                              "Internal error (*unknown*)");
    }
}

}

#endif /* !WORK_BY_COOKIE_HH */

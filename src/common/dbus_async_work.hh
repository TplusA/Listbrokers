/*
 * Copyright (C) 2016, 2019  T+A elektroakustik GmbH & Co. KG
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

#ifndef DBUS_ASYNC_WORK_HH
#define DBUS_ASYNC_WORK_HH

#include <mutex>
#include <condition_variable>
#include <thread>
#include <inttypes.h>

#include <gio/gio.h>

#include "messages.h"

namespace DBusAsync
{

using AsyncMask = uint32_t;

/* constants for \c de.tahifi.Lists.Navigation interface */
enum class NavLists
{
    GET_LIST_CONTEXTS,
    GET_RANGE,
    GET_RANGE_WITH_META_DATA,
    CHECK_RANGE,
    GET_LIST_ID,
    GET_PARAMETRIZED_LIST_ID,
    GET_PARENT_LINK,
    GET_URIS,
    REALIZE_LOCATION,
    DISCARD_LIST,
    KEEP_ALIVE,
};

/* constants for \c net.connman.Manager interface */
enum class ConnmanManager
{
    PROPERTY_CHANGED__STATE,
};

template <typename T>
static constexpr inline AsyncMask mk_async_mask(const T iface_id)
{
    return AsyncMask(1U) << static_cast<unsigned int>(iface_id);
}

template <typename T>
static constexpr inline bool is_async(const T iface_id, const AsyncMask mask)
{
    return (mk_async_mask(iface_id) & mask) != 0;
}

/*!
 * Base class for work that may have to be processed in a worker thread.
 */
class Work
{
  public:
    enum class State
    {
        IDLE,
        RUNNABLE,
        RUNNING,
        DONE,
        CANCELING,
        CANCELED,
    };

  private:
    State state_;

  protected:
    std::mutex lock_;
    std::condition_variable work_finished_;
    GObject *dbus_proxy_;
    GDBusMethodInvocation *dbus_invocation_;

    explicit Work():
        state_(State::IDLE),
        dbus_proxy_(nullptr),
        dbus_invocation_(nullptr)
    {}

    void set_work_state(State state) { state_ = state; }

  public:
    Work(const Work &) = delete;
    Work &operator=(const Work &) = delete;

    virtual ~Work() {}

    State get_state() const { return state_; }

    bool has_finished() const
    {
        return state_ == State::IDLE || state_ == State::DONE || state_ == State::CANCELED;
    }

    /*!
     * Run prepared work, synchronously.
     *
     * This function may only be called if the object is in runnable state,
     * i.e., after a call of #DBusAsync::Work::setup() through a derived class.
     * The #DBusAsync::Work::do_run() implementation of a derived class is
     * called internally.
     *
     * May be called from any context.
     */
    void run()
    {
        std::unique_lock<std::mutex> lock(lock_);

        switch(state_)
        {
          case State::IDLE:
          case State::RUNNING:
          case State::DONE:
          case State::CANCELED:
            BUG("Run async work in state %u", static_cast<unsigned int>(state_));
            break;

          case State::RUNNABLE:
            set_work_state(State::RUNNING);
            lock.unlock();
            do_run();
            log_assert(state_ == State::DONE || state_ == State::CANCELED);
            work_finished_.notify_all();
            break;

          case State::CANCELING:
            break;
        }
    }

    /*!
     * Cancel work in progress, block until the work has finished.
     *
     * This function calls #DBusAsync::Work::cleanup() if necessary. The
     * #DBusAsync::Work::do_cancel() implementation of a derived class is
     * called internally.
     *
     * May be called from any context.
     */
    void cancel_sync_and_cleanup()
    {
        std::unique_lock<std::mutex> lock(lock_);

        switch(state_)
        {
          case State::CANCELING:
            return;

          case State::RUNNABLE:
            set_work_state(State::CANCELED);
            cleanup();
            return;

          case State::RUNNING:
            set_work_state(State::CANCELING);
            do_cancel(lock);
            cleanup();
            return;

          case State::IDLE:
          case State::DONE:
          case State::CANCELED:
            cleanup();
            return;
        }

        BUG("Cancel async work in state %u", static_cast<unsigned int>(state_));
    }

    /*!
     * Clean up work state.
     *
     * Must be called while holding #DBusAsync::Work::lock_.
     */
    void cleanup()
    {
        log_assert(has_finished());

        if(state_ == State::IDLE)
            return;

        log_assert((dbus_proxy_ != nullptr && dbus_invocation_ != nullptr) ||
                   (dbus_proxy_ == nullptr && dbus_invocation_ == nullptr));

        if(dbus_proxy_ != nullptr)
        {
            g_object_unref(dbus_proxy_);
            g_object_unref(G_OBJECT(dbus_invocation_));
            dbus_proxy_ = nullptr;
            dbus_invocation_ = nullptr;
        }

        set_work_state(State::IDLE);
    }

  protected:
    /*!
     * Set up work state.
     *
     * Must be called from D-Bus method handler context. May only be called if
     * the object is in idle state.
     */
    void setup(GObject *dbus_proxy, GDBusMethodInvocation *dbus_invocation)
    {
        log_assert(state_ == State::IDLE);
        log_assert(dbus_proxy_ == nullptr);
        log_assert(dbus_invocation_ == nullptr);
        log_assert((dbus_proxy != nullptr && dbus_invocation != nullptr) ||
                   (dbus_proxy == nullptr && dbus_invocation == nullptr));

        set_work_state(State::RUNNABLE);
        dbus_proxy_ = dbus_proxy;
        dbus_invocation_ = dbus_invocation;

        if(dbus_proxy != nullptr)
        {
            /* for methods */
            g_object_ref(dbus_proxy_);
            g_object_ref(G_OBJECT(dbus_invocation_));
        }
        else
        {
            /* for signals */
        }
    }

    /*!
     * Do the actual work, synchronously.
     *
     * Called with #DBusAsync::Work::lock_ unlocked.
     *
     * Contract: Implementations must return with the object state set to
     *     either #DBusAsync::Work::State::DONE (in case of successful
     *     execution), or #DBusAsync::Work::State::CANCELED (in case of
     *     canceled operation).
     */
    virtual void do_run() = 0;

    /*!
     * Cancel the work in progress and wait for cancelation.
     *
     * Called with #DBusAsync::Work::lock_ locked. The \p lock parameter
     * references that lock and may be used for synchronization purposes in
     * conjunction with #DBusAsync::Work::work_finished_ condition variable.
     *
     * Contract: Implementations must return with the object state set to
     *     either #DBusAsync::Work::State::CANCELED (in case of successful
     *     cancelation), or one of #DBusAsync::Work::State::DONE or
     *     #DBusAsync::Work::State::IDLE (in case of cancelation after
     *     successful completion). The function must hold the \p lock when it
     *     returns.
     */
    virtual void do_cancel(std::unique_lock<std::mutex> &lock) = 0;
};

class ThreadData
{
  private:
    std::mutex lock_;
    std::condition_variable work_available_;
    std::thread thread_;
    bool is_thread_running_;

    Work *next_work_;
    Work *running_work_;

  public:
    ThreadData(const ThreadData &) = delete;
    ThreadData &operator=(const ThreadData &) = delete;

    explicit ThreadData():
        is_thread_running_(false),
        next_work_(nullptr),
        running_work_(nullptr)
    {}

    void init();
    void shutdown();

    using LockData = std::pair<std::unique_lock<std::mutex>, const bool>;

    LockData lock(const bool run_async)
    {
        if(run_async)
            return std::make_pair(std::unique_lock<std::mutex>(lock_), true);
        else
            return std::make_pair(std::unique_lock<std::mutex>(), false);
    }

    /*!
     * Cancel work if running, block until work finishes.
     *
     * This function may only be called after the call of
     * #DBusAsync::ThreadData::lock().
     */
    void cancel_work(Work &work)
    {
        work.cancel_sync_and_cleanup();

        /* Note that the code below works even in case
         * #DBusAsync::ThreadData::lock() has not locked the worker thread.
         * This is because the lock function only locks for work to be
         * processed asynchronously, and only such kind of work is ever posted
         * to the worker. Therefore, synchronously processed work can never end
         * up in neither the #DBusAsync::ThreadData::next_work_ nor the
         * #DBusAsync::ThreadData::running_work_ pointer. Thus, the comparisons
         * below are always going to be false for synchronously processed work
         * (as long as pointer comparisons are atomic). */
        if(&work == next_work_)
            next_work_ = nullptr;

        if(&work == running_work_)
            running_work_ = nullptr;
    }

    /*!
     * Process work directly or in thread.
     *
     * This function may only be called after the call of
     * #DBusAsync::ThreadData::lock(). Simply pass the structure returned by
     * #DBusAsync::ThreadData::lock() as the second parameter of this function.
     */
    void process_work(Work &work, const LockData &lock_data)
    {
        if(std::get<1>(lock_data))
        {
            post_work(work);
        }
        else
        {
            work.run();
            work.cleanup();
        }
    }

  private:
    /*!
     * Put work to be processed asynchronously.
     *
     * Must be called while holding #DBusAsync::ThreadData::lock_.
     */
    void post_work(Work &work)
    {
        if(running_work_ != nullptr)
        {
            log_assert(&work != running_work_);
            log_assert(running_work_->get_state() != Work::State::IDLE);
            running_work_->cancel_sync_and_cleanup();
            log_assert(running_work_->has_finished());
            running_work_ = nullptr;
        }

        if(next_work_ != nullptr)
        {
            /* new work has been posted already while the current work is in
             * progress; we are going to replace the previously posted work by
             * the new work item */
            log_assert(&work != next_work_);
            log_assert(next_work_->get_state() == Work::State::RUNNABLE);
            next_work_->cancel_sync_and_cleanup();
        }

        next_work_ = &work;
        work_available_.notify_one();
    }

    /*!
     * Thread main function.
     */
    static void worker(ThreadData *data);
};

extern ThreadData dbus_async_worker_data;

template <typename T>
static std::pair<std::unique_lock<std::mutex>, const bool>
lock_worker_if_async(const T iface_id, const DBusAsync::AsyncMask async_mask)
{
    return DBusAsync::dbus_async_worker_data.lock(DBusAsync::is_async(iface_id, async_mask));
}

};

#endif /* !DBUS_ASYNC_WORK_HH */

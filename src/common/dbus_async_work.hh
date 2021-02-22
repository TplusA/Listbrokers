/*
 * Copyright (C) 2016, 2019, 2020, 2021  T+A elektroakustik GmbH & Co. KG
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

#include "logged_lock.hh"
#include "messages.h"

#include <memory>
#include <functional>
#include <atomic>

namespace DBusAsync
{

class ReplyPathTracker
{
  public:
    enum class TakePathResult
    {
        TAKEN,
        ALREADY_ON_FAST_PATH,
        ALREADY_ON_SLOW_PATH_COOKIE_NOT_ANNOUNCED_YET,
        ALREADY_ON_SLOW_PATH_COOKIE_ANNOUNCED,
        ALREADY_ON_SLOW_PATH_READY_ANNOUNCED,
        ALREADY_ON_SLOW_PATH_FETCHING,
        INVALID,
    };

  private:
    /*!
     * For synchronization of messages sent to D-Bus caller during slow path.
     */
    enum class ReplyPath
    {
        NONE,
        SCHEDULED,                /*!< Work scheduled and should be running very soon */
        WAITING,                  /*!< Scheduled, possibly running, waiting completion or timeout */
        FAST_PATH,                /*!< Finished on time, reply to be sent via fast path */
        SLOW_PATH_ENTERED,        /*!< Most probably running, slow path, cookie not announced yet */
        SLOW_PATH_COOKIE_SENT,    /*!< Almost surely running, slow path cookie was announced */
        SLOW_PATH_READY_NOTIFIED, /*!< Done, slow path cookie ready notification sent */
        SLOW_PATH_FETCHING,       /*!< Done, client is fetching the slow path result */
    };

    ReplyPath reply_path_;

    LoggedLock::ConditionVariable state_changed_;

  public:
    ReplyPathTracker(const ReplyPathTracker &) = delete;
    ReplyPathTracker(ReplyPathTracker &&) = default;
    ReplyPathTracker &operator=(const ReplyPathTracker &) = delete;
    ReplyPathTracker &operator=(ReplyPathTracker &&) = default;

    explicit ReplyPathTracker(unsigned int idx):
        reply_path_(ReplyPath::NONE)
    {
        LoggedLock::configure(
            state_changed_,
            std::string("DBusAsync::ReplyPathTracker-") + std::to_string(idx) + "-cv",
            MESSAGE_LEVEL_DEBUG);
    }

  private:
    void set_state(ReplyPath target)
    {
        reply_path_ = target;
        state_changed_.notify_all();
    }

    void synchronize(LoggedLock::UniqueLock<LoggedLock::Mutex> &work_lock, ReplyPath target)
    {
        state_changed_.wait(work_lock, [this, target] { return reply_path_ == target; });
    }

  public:
    TakePathResult try_take_fast_path(LoggedLock::UniqueLock<LoggedLock::Mutex> &work_lock)
    {
        switch(reply_path_)
        {
          case ReplyPath::NONE:
            BUG("Requesting fast path before execution");
            break;

          case ReplyPath::SCHEDULED:
            synchronize(work_lock, ReplyPath::WAITING);

            /* fall-through */

          case ReplyPath::WAITING:
            set_state(ReplyPath::FAST_PATH);
            return TakePathResult::TAKEN;

          case ReplyPath::FAST_PATH:
            return TakePathResult::ALREADY_ON_FAST_PATH;

          case ReplyPath::SLOW_PATH_ENTERED:
            return TakePathResult::ALREADY_ON_SLOW_PATH_COOKIE_NOT_ANNOUNCED_YET;

          case ReplyPath::SLOW_PATH_COOKIE_SENT:
            return TakePathResult::ALREADY_ON_SLOW_PATH_COOKIE_ANNOUNCED;

          case ReplyPath::SLOW_PATH_READY_NOTIFIED:
            return TakePathResult::ALREADY_ON_SLOW_PATH_READY_ANNOUNCED;

          case ReplyPath::SLOW_PATH_FETCHING:
            return TakePathResult::ALREADY_ON_SLOW_PATH_FETCHING;
        }

        return TakePathResult::INVALID;
    }

    TakePathResult try_take_slow_path(LoggedLock::UniqueLock<LoggedLock::Mutex> &work_lock)
    {
        switch(reply_path_)
        {
          case ReplyPath::NONE:
            BUG("Requesting slow path before execution");
            break;

          case ReplyPath::SCHEDULED:
          case ReplyPath::WAITING:
            set_state(ReplyPath::SLOW_PATH_ENTERED);
            return TakePathResult::TAKEN;

          case ReplyPath::FAST_PATH:
            return TakePathResult::ALREADY_ON_FAST_PATH;

          case ReplyPath::SLOW_PATH_ENTERED:
            return TakePathResult::ALREADY_ON_SLOW_PATH_COOKIE_NOT_ANNOUNCED_YET;

          case ReplyPath::SLOW_PATH_COOKIE_SENT:
            return TakePathResult::ALREADY_ON_SLOW_PATH_COOKIE_ANNOUNCED;

          case ReplyPath::SLOW_PATH_READY_NOTIFIED:
            return TakePathResult::ALREADY_ON_SLOW_PATH_READY_ANNOUNCED;

          case ReplyPath::SLOW_PATH_FETCHING:
            return TakePathResult::ALREADY_ON_SLOW_PATH_FETCHING;
        }

        return TakePathResult::INVALID;
    }

    bool slow_path_cookie_sent_to_client(LoggedLock::UniqueLock<LoggedLock::Mutex> &work_lock)
    {
        switch(reply_path_)
        {
          case ReplyPath::SLOW_PATH_ENTERED:
            set_state(ReplyPath::SLOW_PATH_COOKIE_SENT);
            return true;

          case ReplyPath::NONE:
          case ReplyPath::SCHEDULED:
          case ReplyPath::WAITING:
          case ReplyPath::FAST_PATH:
          case ReplyPath::SLOW_PATH_COOKIE_SENT:
          case ReplyPath::SLOW_PATH_READY_NOTIFIED:
          case ReplyPath::SLOW_PATH_FETCHING:
            BUG("Cannot set reply path tracker to slow path phase 2 in state %d", int(reply_path_));
            break;
        }

        return false;
    }

    bool slow_path_ready_notified_client(LoggedLock::UniqueLock<LoggedLock::Mutex> &work_lock)
    {
        switch(reply_path_)
        {
          case ReplyPath::SLOW_PATH_COOKIE_SENT:
            set_state(ReplyPath::SLOW_PATH_READY_NOTIFIED);
            return true;

          case ReplyPath::NONE:
          case ReplyPath::SCHEDULED:
          case ReplyPath::WAITING:
          case ReplyPath::FAST_PATH:
          case ReplyPath::SLOW_PATH_ENTERED:
          case ReplyPath::SLOW_PATH_READY_NOTIFIED:
          case ReplyPath::SLOW_PATH_FETCHING:
            BUG("Shouldn't have notified client about completion in state %d", int(reply_path_));
            break;
        }

        return false;
    }

    void set_scheduled_for_execution(LoggedLock::UniqueLock<LoggedLock::Mutex> &work_lock)
    {
        switch(reply_path_)
        {
          case ReplyPath::NONE:
            set_state(ReplyPath::SCHEDULED);
            break;

          case ReplyPath::SCHEDULED:
          case ReplyPath::WAITING:
          case ReplyPath::FAST_PATH:
          case ReplyPath::SLOW_PATH_ENTERED:
          case ReplyPath::SLOW_PATH_COOKIE_SENT:
          case ReplyPath::SLOW_PATH_READY_NOTIFIED:
          case ReplyPath::SLOW_PATH_FETCHING:
            BUG("Cannot set reply path tracker to scheduled state in state %d", int(reply_path_));
            break;
        }
    }

    void set_waiting_for_result(LoggedLock::UniqueLock<LoggedLock::Mutex> &work_lock)
    {
        switch(reply_path_)
        {
          case ReplyPath::SCHEDULED:
            set_state(ReplyPath::WAITING);
            break;

          case ReplyPath::SLOW_PATH_READY_NOTIFIED:
            set_state(ReplyPath::SLOW_PATH_FETCHING);
            break;

          case ReplyPath::NONE:
          case ReplyPath::WAITING:
          case ReplyPath::FAST_PATH:
          case ReplyPath::SLOW_PATH_ENTERED:
          case ReplyPath::SLOW_PATH_COOKIE_SENT:
          case ReplyPath::SLOW_PATH_FETCHING:
            BUG("Cannot set reply path tracker to waiting state in state %d", int(reply_path_));
            break;
        }
    }
};

/*!
 * Base class for D-Bus work meant to be passed to a work queue.
 *
 * This class manages the work item state and declares some basic members
 * required by all D-Bus work items (e.g., state ID, locking, ...).
 */
class Work
{
  public:
    enum class State
    {
        RUNNABLE,  /*!< Idle work item, not processing yet. */
        RUNNING,   /*!< Work in progress. */
        DONE,      /*!< Finished work, result is available. */
        CANCELING, /*!< Cancellation in progress. */
        CANCELED,  /*!< Canceled work, no result available. */
    };

    const std::string &name_;

  private:
    static std::atomic_uint next_free_idx_;
    unsigned int idx_;

    /*! Current work item state. */
    State state_;

    ReplyPathTracker reply_path_tracker_;

    /*! Called when work has completed (done or canceled). */
    std::function<void(LoggedLock::UniqueLock<LoggedLock::Mutex> &, bool)> notify_done_fn_;

    class Times
    {
      private:
        const std::chrono::time_point<std::chrono::steady_clock> created_;
        std::chrono::time_point<std::chrono::steady_clock> scheduled_;
        std::chrono::time_point<std::chrono::steady_clock> started_;
        std::chrono::time_point<std::chrono::steady_clock> finished_;
        bool was_scheduled_;
        bool was_started_;

      public:
        explicit Times():
            created_(std::chrono::steady_clock::now()),
            was_scheduled_(false),
            was_started_(false)
        {}

        void scheduled() { scheduled_ = std::chrono::steady_clock::now(); was_scheduled_ = true; }
        void started()   { started_   = std::chrono::steady_clock::now(); was_started_ = true; }
        void finished()  { finished_  = std::chrono::steady_clock::now(); }

        void show(State state, const std::string &name) const;
    };

    Times times_;

  protected:
    LoggedLock::Mutex lock_;

    explicit Work(const std::string &name):
        name_(name),
        idx_(next_free_idx_++),
        state_(State::RUNNABLE),
        reply_path_tracker_(idx_)
    {
        LoggedLock::configure(lock_, "DBusAsync::Work-" + std::to_string(idx_),
                              MESSAGE_LEVEL_DEBUG);
    }

  public:
    Work(Work &&) = default;
    Work &operator=(Work &&) = default;

    virtual ~Work()
    {
        switch(state_)
        {
          case State::RUNNABLE:
          case State::DONE:
          case State::CANCELED:
            break;

          case State::RUNNING:
          case State::CANCELING:
            BUG("Destroying async work in state %u (will cause Use-After-Free)",
                static_cast<unsigned int>(state_));
            break;
        }

        times_.show(state_, name_);
    }

    /*!
     * Set callback function to be called when work has finished.
     */
    void set_done_notification_function(std::function<void(LoggedLock::UniqueLock<LoggedLock::Mutex> &, bool)> &&fn)
    {
        notify_done_fn_ = std::move(fn);
        times_.scheduled();
    }

    State get_state() const { return state_; }

    ReplyPathTracker &reply_path_tracker__unlocked() { return reply_path_tracker_; }

    template <typename T>
    auto with_reply_path_tracker(
            const std::function<T(LoggedLock::UniqueLock<LoggedLock::Mutex> &, ReplyPathTracker &)> &fn)
    {
        LoggedLock::UniqueLock<LoggedLock::Mutex> work_lock(lock_);
        return fn(work_lock, reply_path_tracker_);
    }

    template <typename T>
    auto with_reply_path_tracker__already_locked(
            LoggedLock::UniqueLock<LoggedLock::Mutex> &work_lock,
            const std::function<T(LoggedLock::UniqueLock<LoggedLock::Mutex> &, ReplyPathTracker &)> &fn)
    {
        return fn(work_lock, reply_path_tracker_);
    }

    /*!
     * Run prepared work, synchronously.
     *
     * This function may only be called if the object is in runnable state, in
     * which case the #DBusAsync::Work::do_run() implementation of a derived
     * class is called internally. When done, the work item state is set to
     * either #DBusAsync::Work::State::DONE or
     * #DBusAsync::Work::State::CANCELED, depending on the return value of
     * #DBusAsync::Work::do_run().
     *
     * May be called from any context.
     */
    void run()
    {
        run(LoggedLock::UniqueLock<LoggedLock::Mutex>(lock_));
    }

  protected:
    void run(LoggedLock::UniqueLock<LoggedLock::Mutex> &&work_lock)
    {
        switch(state_)
        {
          case State::RUNNABLE:
            {
                set_work_state(work_lock, State::RUNNING);
                times_.started();

                work_lock.unlock();
                const auto success = do_run();
                LOGGED_LOCK_CONTEXT_HINT;
                work_lock.lock();

                /* state may have changed in the meantime */
                switch(state_)
                {
                  case State::RUNNING:
                    /* state hasn't changed, so we are done here */
                    set_work_state(work_lock, success ? State::DONE : State::CANCELED);
                    break;

                  case State::CANCELING:
                    /* fix up for the case that #DBusAsync::Work::do_run() has
                     * completed successfully, but the work item has been
                     * canceled in the meantime */
                    set_work_state(work_lock, State::CANCELED);
                    break;

                  case State::RUNNABLE:
                    MSG_UNREACHABLE();
                    set_work_state(work_lock, State::CANCELED);
                    break;

                  case State::DONE:
                  case State::CANCELED:
                    BUG("Unexpected final work state %d after run", int(state_));
                    break;
                }

                times_.finished();
            }

            return;

          case State::RUNNING:
          case State::DONE:
          case State::CANCELING:
          case State::CANCELED:
            break;
        }

        BUG("Run async work in state %u", static_cast<unsigned int>(state_));
    }

  public:
    /*!
     * Cancel work in progress.
     *
     * The #DBusAsync::Work::do_cancel() implementation of the derived class is
     * called internally.
     *
     * May be called from any context.
     */
    void cancel()
    {
        LOGGED_LOCK_CONTEXT_HINT;
        LoggedLock::UniqueLock<LoggedLock::Mutex> work_lock(lock_);

        switch(state_)
        {
          case State::CANCELING:
            return;

          case State::RUNNABLE:
            set_work_state(work_lock, State::CANCELED);
            times_.finished();
            return;

          case State::RUNNING:
            set_work_state(work_lock, State::CANCELING);
            do_cancel(work_lock);
            return;

          case State::DONE:
          case State::CANCELED:
            return;
        }

        BUG("Cancel async work in state %u", static_cast<unsigned int>(state_));
    }

  protected:
    /*!
     * Do the actual work, synchronously.
     *
     * Called with #DBusAsync::Work::lock_ unlocked.
     *
     * \returns
     *     True of the work has been processed (regardless of success or
     *     failure), false  if the work has been canceled.
     */
    virtual bool do_run() = 0;

    /*!
     * Initiate cancellation of the work in progress.
     *
     * Called with #DBusAsync::Work::lock_ locked. The \p work_lock parameter
     * references that lock.
     *
     * Contract: Implementations must return with the object state set to
     *     either #DBusAsync::Work::State::CANCELED (in case of successful
     *     cancellation), #DBusAsync::Work::State::CANCELING (in case immediate
     *     cancellation is not possible), or #DBusAsync::Work::State::DONE (in
     *     case of cancellation after successful completion).
     *
     * Contract: This function must return with \p work_lock locked.
     */
    virtual void do_cancel(LoggedLock::UniqueLock<LoggedLock::Mutex> &work_lock) = 0;

  private:
    void set_work_state(LoggedLock::UniqueLock<LoggedLock::Mutex> &work_lock, State state)
    {
        if(state == state_)
            return;

        switch(state_)
        {
          case State::RUNNABLE:
            BUG_IF(state == State::CANCELING, "%p RUNNABLE -> CANCELING", static_cast<const void *>(this));
            break;

          case State::RUNNING:
            BUG_IF(state == State::RUNNABLE, "%p RUNNING -> RUNNABLE", static_cast<const void *>(this));
            break;

          case State::CANCELING:
            BUG_IF(state == State::RUNNABLE, "%p CANCELING -> RUNNABLE", static_cast<const void *>(this));
            BUG_IF(state == State::RUNNING,  "%p CANCELING -> RUNNING", static_cast<const void *>(this));
            BUG_IF(state == State::DONE,     "%p CANCELING -> DONE", static_cast<const void *>(this));
            break;

          case State::DONE:
          case State::CANCELED:
            BUG("%p going from final state %d to %d", static_cast<const void *>(this), int(state_), int(state));
            break;
        }

        state_ = state;

        if(notify_done_fn_ != nullptr)
        {
            switch(state_)
            {
              case State::DONE:
                notify_done_fn_(work_lock, true);
                break;

              case State::CANCELED:
                notify_done_fn_(work_lock, false);
                break;

              case State::RUNNABLE:
              case State::RUNNING:
              case State::CANCELING:
                break;
            }
        }
    }
};

}

#endif /* !DBUS_ASYNC_WORK_HH */

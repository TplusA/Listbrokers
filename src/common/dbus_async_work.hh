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

#include <memory>
#include <mutex>
#include <functional>

#include <gio/gio.h>

#include "messages.h"

namespace DBusAsync
{

/*!
 * Base class for D-Bus work meant to be passed to a work queue.
 *
 * This class manages the work item state and declares some basic members
 * required by all D-Bus work items (e.g., state ID, D-Bus proxy, locking,
 * ...).
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

  private:
    /*! Current work item state. */
    State state_;

    /*! Called when work has completed (done or canceled). */
    std::function<void(bool)> notify_done_fn_;

  protected:
    std::mutex lock_;

    explicit Work():
        state_(State::RUNNABLE)
    {}

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
    }

    /*!
     * Set callback function to be called when work has finished.
     */
    void set_done_notification_function(std::function<void(bool)> &&fn)
    {
        notify_done_fn_ = std::move(fn);
    }

    State get_state() const { return state_; }

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
        std::unique_lock<std::mutex> lock(lock_);

        switch(state_)
        {
          case State::RUNNABLE:
            {
                set_work_state(State::RUNNING);

                lock.unlock();
                const auto state = do_run() ? State::DONE : State::CANCELED;
                lock.lock();

                set_work_state(state);
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
        std::unique_lock<std::mutex> lock(lock_);

        switch(state_)
        {
          case State::CANCELING:
            return;

          case State::RUNNABLE:
            set_work_state(State::CANCELED);
            return;

          case State::RUNNING:
            set_work_state(State::CANCELING);
            do_cancel(lock);
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
     * Called with #DBusAsync::Work::lock_ locked. The \p lock parameter
     * references that lock.
     *
     * Contract: Implementations must return with the object state set to
     *     either #DBusAsync::Work::State::CANCELED (in case of successful
     *     cancellation), #DBusAsync::Work::State::CANCELING (in case immediate
     *     cancellation is not possible), or #DBusAsync::Work::State::DONE (in
     *     case of cancellation after successful completion).
     *
     * Contract: This function must return with \p lock locked.
     */
    virtual void do_cancel(std::unique_lock<std::mutex> &lock) = 0;

  private:
    void set_work_state(State state)
    {
        if(state == state_)
            return;

        state_ = state;

        if(notify_done_fn_ != nullptr)
        {
            switch(state_)
            {
              case State::DONE:
                notify_done_fn_(true);
                break;

              case State::CANCELED:
                notify_done_fn_(false);
                break;

              case State::RUNNABLE:
              case State::RUNNING:
              case State::CANCELING:
                break;
            }
        }
    }
};

};

#endif /* !DBUS_ASYNC_WORK_HH */

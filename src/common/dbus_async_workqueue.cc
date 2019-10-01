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

#include "dbus_async_workqueue.hh"

void DBusAsync::WorkQueue::shutdown()
{
    std::unique_lock<std::mutex> lock(lock_);

    is_accepting_work_ = false;
    cancel_all_work();

    lock.unlock();

    if(thread_.joinable())
        thread_.join();
}

void DBusAsync::WorkQueue::clear()
{
    std::lock_guard<std::mutex> lock(lock_);
    cancel_all_work();
}

bool DBusAsync::WorkQueue::add_work(std::shared_ptr<Work> &&work,
                                    std::function<void(bool, bool)> &&work_accepted)
{
    std::unique_lock<std::mutex> lock(lock_);

    if(!is_accepting_work_)
        return false;

    switch(mode_)
    {
      case Mode::ASYNC:
        if(queue_work(std::move(work)))
        {
            if(work_accepted != nullptr)
                work_accepted(true, false);

            work_finished_.notify_one();
        }
        else if(work_accepted != nullptr)
            work_accepted(true, false);

        return true;

      case Mode::SYNCHRONOUS:
        break;
    }

    queue_work(work);

    if(work_accepted != nullptr)
        work_accepted(false, false);

    process_work_item(lock, std::move(work));

    if(work_accepted != nullptr)
        work_accepted(false, true);

    return false;
}

bool DBusAsync::WorkQueue::queue_work(std::shared_ptr<Work> work)
{
    log_assert(work != nullptr);
    log_assert(work->get_state() == Work::State::RUNNABLE);

    if(work_in_progress_ != nullptr)
    {
        if(queue_.size() < maximum_queue_length_)
        {
            /* have work in progress, queue is not full yet */
            queue_.emplace_back(std::move(work));
            return false;
        }

        /* have work in progress, and queue is full */
        work_in_progress_->cancel();
        work_in_progress_ = nullptr;
    }

    /* have no work in progress */
    if(queue_.empty())
    {
        /* nothing in queue, new work can be processed right now */
        work_in_progress_ = std::move(work);
        return true;
    }
    else
    {
        /* take next element from queue, put new work to the end */
        work_in_progress_ = queue_.front();
        queue_.pop_front();
        queue_.emplace_back(std::move(work));
        return false;
    }
}

bool DBusAsync::WorkQueue::process_work_item(std::unique_lock<std::mutex> &lock,
                                             std::shared_ptr<Work> &&work)
{
    if(work != nullptr)
        work_finished_.wait(lock,
            [this, work = std::move(work)] ()
            { return !is_accepting_work_ || work_in_progress_ == work; });
    else
        work_finished_.wait(lock,
            [this] ()
            { return !is_accepting_work_ || work_in_progress_ != nullptr; });

    if(!is_accepting_work_)
    {
        cancel_all_work();
        return false;
    }

    log_assert(work_in_progress_ != nullptr);
    work = work_in_progress_;

    switch(work->get_state())
    {
      case Work::State::RUNNABLE:
        lock.unlock();
        work->run();
        lock.lock();
        break;

      case Work::State::RUNNING:
        BUG("Queued work item RUNNING");
        break;

      case Work::State::DONE:
        BUG("Queued work item DONE");
        break;

      case Work::State::CANCELING:
      case Work::State::CANCELED:
        break;
    }

    if(work_in_progress_ == work)
    {
        work = nullptr;

        if(queue_.empty())
            work_in_progress_ = nullptr;
        else
        {
            work_in_progress_ = queue_.front();
            queue_.pop_front();
        }
    }

    work_finished_.notify_all();

    return true;
}

void DBusAsync::WorkQueue::cancel_all_work()
{
    for(auto &w : queue_)
        w->cancel();

    queue_.clear();

    if(work_in_progress_ != nullptr)
    {
        work_in_progress_->cancel();
        work_in_progress_ = nullptr;
    }

    work_finished_.notify_all();
}

void DBusAsync::WorkQueue::worker(WorkQueue *q)
{
    std::unique_lock<std::mutex> lock(q->lock_);

    while(q->process_work_item(lock, nullptr))
        ;
}

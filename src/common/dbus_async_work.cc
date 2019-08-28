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

#include "dbus_async_work.hh"

DBusAsync::ThreadData DBusAsync::dbus_async_worker_data;

void DBusAsync::ThreadData::init()
{
    is_thread_running_ = true;
    thread_ = std::thread(worker, this);
}

void DBusAsync::ThreadData::shutdown()
{
    std::unique_lock<std::mutex> work_lock(lock_);

    is_thread_running_ = false;
    work_available_.notify_one();

    work_lock.unlock();

    thread_.join();
}

void DBusAsync::ThreadData::worker(DBusAsync::ThreadData *data)
{
    std::unique_lock<std::mutex> lock(data->lock_);

    while(data->is_thread_running_)
    {
        data->work_available_.wait(lock,
                                   [&data] ()
                                   {
                                       return data->next_work_ != nullptr || !data->is_thread_running_;
                                   });

        if(!data->is_thread_running_)
            continue;

        data->running_work_ = data->next_work_;
        data->next_work_ = nullptr;

        lock.unlock();

        data->running_work_->run();

        lock.lock();

        if(data->running_work_ != nullptr)
        {
            data->running_work_->cleanup();
            data->running_work_ = nullptr;
        }
        else
        {
            /* was canceled by #DBusAsync::ThreadData::cancel_work() */
        }
    }
}

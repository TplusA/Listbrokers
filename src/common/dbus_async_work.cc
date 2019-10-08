/*
 * Copyright (C) 2016, 2019, 2020  T+A elektroakustik GmbH & Co. KG
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

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "dbus_async_work.hh"

#include <sstream>

template <typename T>
static inline auto us(const T &diff)
{
    return std::chrono::duration_cast<std::chrono::microseconds>(diff);
}

void DBusAsync::Work::Times::show(State state, const std::string &name) const
{
    using namespace std::chrono_literals;

    const auto destroyed(std::chrono::steady_clock::now());
    const auto life_time(destroyed - created_);

    std::ostringstream os;
    os << "Work item " << (name.empty() ? "(unknown)" : name) << " timings:\n"
       << "- Life time: " << us(life_time).count() << " us";

    if(life_time >= 1s)
    {
        using fs = std::chrono::duration<float, std::chrono::seconds::period>;
        os << " (" << fs(life_time).count() << " s)";
    }

    os << ", ";

    if(!was_scheduled_)
        os << "never scheduled, ";

    if(!was_started_)
        os << "never started, ";

    switch(state)
    {
      case State::RUNNABLE:
        os << "stillbirth\n";
        break;

      case State::DONE:
        os << "completed\n";
        break;

      case State::CANCELED:
        os << "canceled\n";
        break;

      case State::RUNNING:
        os << "?still running?\n";
        break;

      case State::CANCELING:
        os << "?still canceling?\n";
        break;
    }

    if(was_started_)
    {
        os << "- Idle     : " << us(started_ - created_).count() << " us\n";

        if(was_scheduled_)
            os << "- In queue : " << us(started_ - scheduled_).count() << " us\n";
    }

    if(state == State::DONE || state == State::CANCELED)
    {
        os << "- Busy     : " << (was_started_ ? us(finished_ - started_).count() : 0) << " us\n";
        os << "- Dispatch : " << us(destroyed - finished_).count() << " us\n";
    }

    msg_info("%s", os.str().c_str());
}

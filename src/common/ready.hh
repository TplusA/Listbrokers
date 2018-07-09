/*
 * Copyright (C) 2017  T+A elektroakustik GmbH & Co. KG
 *
 * This file is part of T+A List Brokers.
 *
 * T+A List Brokers is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 3 as
 * published by the Free Software Foundation.
 *
 * T+A List Brokers is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with T+A List Brokers.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef READY_HH
#define READY_HH

#include <functional>
#include <vector>

#include "readyprobe.hh"

/*!
 * \addtogroup ready_state Daemon ready state.
 */
/*!@{*/

namespace Ready
{

using Watcher = std::function<void(bool)>;

class Manager: public ProbeChangedIface
{
  private:
    const std::vector<Probe *> probes_;
    std::vector<Watcher> watchers_;
    std::atomic_bool is_ready_;

  public:
    Manager(const Manager &) = delete;
    Manager &operator=(const Manager &) = delete;

    explicit Manager(std::vector<Probe *> &&probes):
        probes_(std::move(probes))
    {
        for(auto &p : probes_)
            Probe::ProbeChangedIfaceAPI::set_iface(*p, *this);

        is_ready_ = update_state(probes_);
    }

    virtual ~Manager() {}

    void add_watcher(Watcher &&watcher, bool call_watchers)
    {
        watchers_.emplace_back(watcher);

        if(call_watchers)
            watcher(is_ready_);
    }

    bool is_ready() const { return is_ready_; }

    void notify_probe_state_changed() final override
    {
        const bool temp = update_state(probes_);

        if(is_ready_.exchange(temp) != temp)
            notify_watchers(watchers_, temp);
    }

    Probe *get_probe(const size_t idx) const
    {
        return idx < probes_.size() ? probes_[idx] : nullptr;
    }

  private:
    static bool update_state(const std::vector<Probe *> &probes)
    {
        for(auto &p : probes)
        {
            if(!p->is_ready())
                return false;
        }

        return true;
    }

    static void notify_watchers(const std::vector<Watcher> &watchers,
                                const bool state)
    {
        for(const auto &w : watchers)
            w(state);
    }
};

}

/*!@}*/

#endif /* !READY_HH */

/*
 * Copyright (C) 2017, 2019  T+A elektroakustik GmbH & Co. KG
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

#ifndef READYPROBE_HH
#define READYPROBE_HH

#include <atomic>

/*!
 * \addtogroup ready_state Daemon ready state.
 */
/*!@{*/

namespace Ready
{

class ProbeChangedIface
{
  protected:
    explicit ProbeChangedIface() {}

  public:
    ProbeChangedIface(const ProbeChangedIface &) = delete;
    ProbeChangedIface &operator=(const ProbeChangedIface &) = delete;

    virtual ~ProbeChangedIface() {}

    virtual void notify_probe_state_changed() = 0;
};

class Probe
{
  private:
    ProbeChangedIface *pciface_;

  protected:
    explicit Probe():
        pciface_(nullptr)
    {}

  public:
    Probe(const Probe &) = delete;
    Probe &operator=(const Probe &) = delete;

    virtual ~Probe() {}

    virtual bool is_ready() const = 0;

    class ProbeChangedIfaceAPI
    {
        static void set_iface(Probe &p, ProbeChangedIface &pciface)
        {
            p.pciface_ = &pciface;
        }

        friend class Manager;
    };

  protected:
    void notify_probe_state_changed() { pciface_->notify_probe_state_changed(); }
};

class SimpleProbe: public Probe
{
  private:
    std::atomic_bool is_ready_;

  public:
    SimpleProbe(const SimpleProbe &) = delete;
    SimpleProbe &operator=(const SimpleProbe &) = delete;

    explicit SimpleProbe():
        is_ready_(false)
    {}

    virtual ~SimpleProbe() {}

    bool is_ready() const final override { return is_ready_; }

    void set_ready()
    {
        if(!is_ready_.exchange(true))
            notify_probe_state_changed();
    }

    void set_unready()
    {
        if(is_ready_.exchange(false))
            notify_probe_state_changed();
    }
};

}

/*!@}*/

#endif /* !READYPROBE_HH */

/*
 * Copyright (C) 2019  T+A elektroakustik GmbH & Co. KG
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

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "periodic_rescan.hh"
#include "messages.h"

void UPnP::PeriodicRescan::enable()
{
    msg_info("Enable periodic UPnP rescanning, interval %u seconds", interval_seconds_);

    if(is_enabled_)
    {
        BUG("Already enabled");
        return;
    }

    is_enabled_ = true;
};

void UPnP::PeriodicRescan::disable()
{
    msg_info("Disable periodic UPnP rescanning");

    if(!is_enabled_)
        BUG("Already disabled");

    is_enabled_ = false;
}

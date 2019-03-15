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

#include <glib.h>

static gboolean rescan_now(gpointer user_data)
{
    msg_info("UPnP rescan");
    return G_SOURCE_CONTINUE;
}

void UPnP::PeriodicRescan::enable()
{
    msg_info("Enable periodic UPnP rescanning, interval %u seconds", interval_seconds_);

    if(timeout_id_ > 0)
    {
        BUG("Already enabled");
        return;
    }

    timeout_id_ = g_timeout_add_seconds(interval_seconds_, rescan_now, this);

    if(timeout_id_ == 0)
        msg_error(0, LOG_ERR,
                  "Failed registering timeout function for UPnP rescanning");
};

void UPnP::PeriodicRescan::disable()
{
    msg_info("Disable periodic UPnP rescanning");

    if(timeout_id_ == 0)
    {
        BUG("Already disabled");
        return;
    }

    g_source_remove(timeout_id_);
    timeout_id_ = 0;
}

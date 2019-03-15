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
#include "upnp_dleynaserver_dbus.h"
#include "dbus_upnp_iface_deep.h"
#include "dbus_common.h"

gboolean UPnP::PeriodicRescan::rescan_now_trampoline(gpointer scan)
{
    return static_cast<PeriodicRescan *>(scan)->rescan_now();
}

gboolean UPnP::PeriodicRescan::rescan_now()
{
    if(is_inhibited_)
    {
        msg_error(0, LOG_WARNING,
                  "Should perform UPnP rescan, but still waiting for completion of previous scan");
        return G_SOURCE_CONTINUE;
    }

    auto *iface = dbus_upnp_get_dleynaserver_manager_iface();

    if(iface == nullptr)
    {
        /* glitch, should never happen (TM) */
        BUG("Should perform UPnP rescan, but have no D-Bus connection to dLeyna");
        return G_SOURCE_REMOVE;
    }

    msg_info("UPnP rescan start");
    is_inhibited_ = true;
    tdbus_dleynaserver_manager_call_rescan(iface, nullptr, rescan_done, this);

    return G_SOURCE_CONTINUE;
}

void UPnP::PeriodicRescan::rescan_done(GObject *source_object,
                                       GAsyncResult *res, gpointer scan)
{
    msg_info("UPnP rescan finished");
    GError *gerror = NULL;
    tdbus_dleynaserver_manager_call_rescan_finish(
        TDBUS_DLEYNASERVER_MANAGER(source_object), res, &gerror);
    (void)dbus_common_handle_error(&gerror);
    static_cast<PeriodicRescan *>(scan)->is_inhibited_ = false;
}

void UPnP::PeriodicRescan::enable()
{
    msg_info("Enable periodic UPnP rescanning, interval %u seconds", interval_seconds_);

    if(timeout_id_ > 0)
    {
        BUG("Already enabled");
        return;
    }

    is_inhibited_ = false;
    timeout_id_ = g_timeout_add_seconds(interval_seconds_,
                                        rescan_now_trampoline, this);

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

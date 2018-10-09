/*
 * Copyright (C) 2015, 2016, 2018  T+A elektroakustik GmbH & Co. KG
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

#include <cstring>
#include <string>
#include <vector>

#include "dbus_upnp_handlers.hh"
#include "dbus_upnp_iface_deep.h"
#include "dbus_common.h"
#include "messages.h"

void dbussignal_dleynaserver_manager(GDBusProxy *proxy, const gchar *sender_name,
                                     const gchar *signal_name, GVariant *parameters,
                                     gpointer user_data)
{
    log_assert(user_data != NULL);

    DBusUPnPSignalData &data = *static_cast<DBusUPnPSignalData *>(user_data);

    static const char iface_name[] = "com.intel.dLeynaServer.Manager";

    msg_vinfo(MESSAGE_LEVEL_TRACE,
              "%s signal from '%s': %s", iface_name, sender_name, signal_name);

    if(strcmp(signal_name, "FoundServer") == 0)
    {
        GVariant *val = g_variant_get_child_value(parameters, 0);
        log_assert(val != NULL);

        const gchar *str = g_variant_get_string(val, NULL);
        msg_info("New server %s", str);

        /* for now, add each server synchronously inside this GLib signal
         * handler */
        std::vector<std::string> new_servers;
        new_servers.push_back(str);
        data.upnp_list_tree_.add_to_server_list(new_servers);

        g_variant_unref(val);
    }
    else if(strcmp(signal_name, "LostServer") == 0)
    {
        GVariant *val = g_variant_get_child_value(parameters, 0);
        log_assert(val != NULL);

        const gchar *str = g_variant_get_string(val, NULL);
        msg_info("Bye-bye server %s", str);

        /* for now, remove each server synchronously inside this GLib signal
         * handler */
        std::vector<std::string> lost_servers;
        lost_servers.push_back(str);
        data.upnp_list_tree_.remove_from_server_list(lost_servers);

        g_variant_unref(val);
    }
    else
        dbus_common_unknown_signal(iface_name, signal_name, sender_name);
}

void dbussignal_dleynaserver_vanished(struct DBusUPnPSignalData *data)
{
    data->upnp_list_tree_.clear();
}

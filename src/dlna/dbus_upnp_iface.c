/*
 * Copyright (C) 2015, 2016, 2018, 2019  T+A elektroakustik GmbH & Co. KG
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

#include <string.h>

#include "dbus_upnp_iface.h"
#include "dbus_upnp_iface_deep.h"
#include "dbus_common.h"
#include "dbus_upnp_handlers.h"
#include "messages.h"

struct DBusUPnPData
{
    const char *dbus_object_path;

    GDBusConnection *connection;
    guint dleyna_watcher;
    tdbusdleynaserverManager *dleynaserver_manager_iface;
    struct DBusUPnPSignalData *signal_data;
    bool is_connecting;
    void (*dleyna_status_watcher)(bool, void *);
    void *dleyna_status_watcher_data;
};

static void vanished(GDBusConnection *connection, const gchar *name,
                     gpointer user_data);

static void created_dleyna_proxy(GObject *source_object, GAsyncResult *res,
                                 gpointer user_data)
{
    struct DBusUPnPData *data = user_data;
    GError *error = NULL;

    data->is_connecting = false;
    data->dleynaserver_manager_iface =
        tdbus_dleynaserver_manager_proxy_new_finish(res, &error);

    if(dbus_common_handle_error(&error) == 0)
    {
        data->connection = g_dbus_proxy_get_connection(G_DBUS_PROXY(data->dleynaserver_manager_iface));
        g_signal_connect(data->dleynaserver_manager_iface, "g-signal",
                         G_CALLBACK(dbussignal_dleynaserver_manager),
                         data->signal_data);
        data->dleyna_status_watcher(true, data->dleyna_status_watcher_data);
    }
    else
        vanished(data->connection, NULL, user_data);
}

static void vanished(GDBusConnection *connection, const gchar *name,
                     gpointer user_data)
{
    struct DBusUPnPData *data = user_data;

    if(data->is_connecting)
        return;

    if(data->dleynaserver_manager_iface != NULL)
    {
        msg_error(0, LOG_NOTICE, "dLeyna has vanished, trying to reconnect");
        data->dleyna_status_watcher(false, data->dleyna_status_watcher_data);
        g_object_unref(data->dleynaserver_manager_iface);
        data->dleynaserver_manager_iface = NULL;
        dbussignal_dleynaserver_vanished(data->signal_data);
    }

    data->is_connecting = true;
    tdbus_dleynaserver_manager_proxy_new(connection, G_DBUS_PROXY_FLAGS_NONE,
                                         "com.intel.dleyna-server",
                                         "/com/intel/dLeynaServer", NULL,
                                         created_dleyna_proxy, user_data);
}

static void bus_acquired(GDBusConnection *connection, const gchar *name,
                         bool is_session_bus, gpointer user_data)
{
    struct DBusUPnPData *data = user_data;
    data->connection = connection;
    data->dleyna_watcher =
        g_bus_watch_name(is_session_bus ? G_BUS_TYPE_SESSION : G_BUS_TYPE_SYSTEM,
                         "com.intel.dleyna-server",
                         G_BUS_NAME_WATCHER_FLAGS_NONE,
                         NULL, vanished, user_data, NULL);
}

static struct DBusUPnPData dbus_upnp_data;

static void shutdown_dbus(bool is_session_bus, gpointer user_data)
{
    if(dbus_upnp_data.dleyna_watcher != 0)
        g_bus_unwatch_name(dbus_upnp_data.dleyna_watcher);

    if(dbus_upnp_data.dleynaserver_manager_iface != NULL)
        g_object_unref(dbus_upnp_data.dleynaserver_manager_iface);
}

/*!
 * Connect to dLeyna service.
 *
 * \todo
 *     Should call com.intel.dLeynaServer.Manager.SetProtocolInfo().
 */
void dbus_upnp_setup(bool connect_to_session_bus, const char *dbus_object_path,
                     struct DBusUPnPSignalData *signal_data,
                     void (*dleyna_status_watcher)(bool, void *),
                     void *dleyna_status_watcher_data)
{
    dbus_upnp_data.dbus_object_path = dbus_object_path;
    dbus_upnp_data.connection = NULL;
    dbus_upnp_data.dleyna_watcher = 0;
    dbus_upnp_data.dleynaserver_manager_iface = NULL;
    dbus_upnp_data.signal_data = signal_data;
    dbus_upnp_data.is_connecting = false;
    dbus_upnp_data.dleyna_status_watcher = dleyna_status_watcher;
    dbus_upnp_data.dleyna_status_watcher_data = dleyna_status_watcher_data;

    const struct dbus_register_submodule_t self =
    {
        .connect_to_session_bus = connect_to_session_bus,
        .bus_acquired = bus_acquired,
        .user_data = &dbus_upnp_data,
        .shutdown = shutdown_dbus,
    };

    dbus_common_register_submodule(&self);
}

tdbusdleynaserverManager *dbus_upnp_get_dleynaserver_manager_iface(void)
{
    return dbus_upnp_data.dleynaserver_manager_iface;
}

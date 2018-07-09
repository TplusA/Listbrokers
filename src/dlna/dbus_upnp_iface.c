/*
 * Copyright (C) 2015, 2016  T+A elektroakustik GmbH & Co. KG
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

#include <string.h>

#include "dbus_upnp_iface.h"
#include "dbus_upnp_iface_deep.h"
#include "dbus_common.h"
#include "dbus_upnp_handlers.h"
#include "upnp_dleynaserver_dbus.h"
#include "upnp_media_dbus.h"
#include "messages.h"

struct DBusUPnPData
{
    const char *dbus_object_path;

    tdbusdleynaserverManager *dleynaserver_manager_iface;
    struct DBusUPnPSignalData *signal_data;
};

static void connect_dbus_signals(GDBusConnection *connection,
                                 const gchar *name, bool is_session_bus,
                                 gpointer user_data)
{
    struct DBusUPnPData *data = user_data;

    GError *error = NULL;

    data->dleynaserver_manager_iface =
        tdbus_dleynaserver_manager_proxy_new_sync(connection,
                                                  G_DBUS_PROXY_FLAGS_NONE,
                                                  "com.intel.dleyna-server",
                                                  "/com/intel/dLeynaServer",
                                                  NULL, &error);
    (void)dbus_common_handle_error(&error);

    g_signal_connect(data->dleynaserver_manager_iface, "g-signal",
                     G_CALLBACK(dbussignal_dleynaserver_manager),
                     data->signal_data);
}

static struct DBusUPnPData dbus_upnp_data;

static void shutdown_dbus(bool is_session_bus, gpointer user_data)
{
    g_object_unref(dbus_upnp_data.dleynaserver_manager_iface);
}

/*!
 * Connect to dLeyna service.
 *
 * \todo
 *     Should call com.intel.dLeynaServer.Manager.SetProtocolInfo().
 */
void dbus_upnp_setup(bool connect_to_session_bus, const char *dbus_object_path,
                     struct DBusUPnPSignalData *signal_data)
{
    dbus_upnp_data.dbus_object_path = dbus_object_path;
    dbus_upnp_data.dleynaserver_manager_iface = NULL;
    dbus_upnp_data.signal_data = signal_data;

    const struct dbus_register_submodule_t self =
    {
        .connect_to_session_bus = connect_to_session_bus,
        .name_acquired = connect_dbus_signals,
        .user_data = &dbus_upnp_data,
        .shutdown = shutdown_dbus,
    };

    dbus_common_register_submodule(&self);
}

tdbusdleynaserverManager *dbus_upnp_get_dleynaserver_manager_iface(void)
{
    return dbus_upnp_data.dleynaserver_manager_iface;
}

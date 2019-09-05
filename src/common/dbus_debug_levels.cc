/*
 * Copyright (C) 2016, 2017, 2019  T+A elektroakustik GmbH & Co. KG
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

#include "dbus_debug_levels.hh"
#include "dbus_common.h"
#include "messages_dbus.h"

struct dbus_debug_levels_data_t
{
    const char *dbus_object_path;

    tdbusdebugLogging *debug_logging_iface;
    tdbusdebugLoggingConfig *debug_logging_config_proxy;
};

static void export_self(GDBusConnection *connection, const gchar *name,
                        bool is_session_bus, gpointer user_data)
{
    auto *const data = static_cast<dbus_debug_levels_data_t *>(user_data);

    data->debug_logging_iface = tdbus_debug_logging_skeleton_new();

    g_signal_connect(data->debug_logging_iface, "handle-debug-level",
                     G_CALLBACK(msg_dbus_handle_debug_level), NULL);

    dbus_common_try_export_iface(connection,
                                 G_DBUS_INTERFACE_SKELETON(data->debug_logging_iface),
                                 data->dbus_object_path);
}

static void created_debug_config_proxy(GObject *source_object, GAsyncResult *res,
                                       gpointer user_data)
{
    auto *const data = static_cast<dbus_debug_levels_data_t *>(user_data);
    GError *error = NULL;

    data->debug_logging_config_proxy =
        tdbus_debug_logging_config_proxy_new_finish(res, &error);

    if(dbus_common_handle_error(&error) == 0)
        g_signal_connect(data->debug_logging_config_proxy, "g-signal",
                         G_CALLBACK(msg_dbus_handle_global_debug_level_changed),
                         NULL);
}

static void connect_dbus_signals(GDBusConnection *connection,
                                 const gchar *name,
                                 bool is_session_bus, gpointer user_data)
{
    tdbus_debug_logging_config_proxy_new(connection,
                                         G_DBUS_PROXY_FLAGS_NONE,
                                         "de.tahifi.Dcpd", "/de/tahifi/Dcpd",
                                         NULL,
                                         created_debug_config_proxy,
                                         user_data);
}

static void shutdown_dbus(bool is_session_bus, gpointer user_data)
{
    auto *const data = static_cast<dbus_debug_levels_data_t *>(user_data);

    g_object_unref(data->debug_logging_iface);

    if(data->debug_logging_config_proxy != NULL)
        g_object_unref(data->debug_logging_config_proxy);
}

static dbus_debug_levels_data_t dbus_debug_levels_data;

void DBusDebugLevels::dbus_setup(bool connect_to_session_bus,
                                 const char *dbus_object_path)
{
    dbus_debug_levels_data.dbus_object_path = dbus_object_path;
    dbus_debug_levels_data.debug_logging_iface = NULL;
    dbus_debug_levels_data.debug_logging_config_proxy = NULL;

    const struct dbus_register_submodule_t self =
    {
        .connect_to_session_bus = connect_to_session_bus,
        .user_data = &dbus_debug_levels_data,
        .bus_acquired = export_self,
        .name_acquired = connect_dbus_signals,
        .destroy_notification = nullptr,
        .shutdown = shutdown_dbus,
    };

    dbus_common_register_submodule(&self);
}


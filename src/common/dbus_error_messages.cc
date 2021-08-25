/*
 * Copyright (C) 2021  T+A elektroakustik GmbH & Co. KG
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

#include "dbus_error_messages.hh"
#include "dbus_common.h"

struct dbus_error_messages_data_t
{
    const char *dbus_object_path;
    tdbusErrors *errors_iface;
};

static void export_self(GDBusConnection *connection, const gchar *name,
                        bool is_session_bus, gpointer user_data)
{
    auto *const data = static_cast<dbus_error_messages_data_t *>(user_data);

    data->errors_iface = tdbus_errors_skeleton_new();

    dbus_common_try_export_iface(connection,
                                 G_DBUS_INTERFACE_SKELETON(data->errors_iface),
                                 data->dbus_object_path);
}

static void shutdown_dbus(bool is_session_bus, gpointer user_data)
{
    auto *const data = static_cast<dbus_error_messages_data_t *>(user_data);
    g_object_unref(data->errors_iface);
}

static dbus_error_messages_data_t dbus_error_messages_data;

void DBusErrorMessages::dbus_setup(bool connect_to_session_bus,
                                   const char *dbus_object_path)
{
    dbus_error_messages_data.dbus_object_path = dbus_object_path;
    dbus_error_messages_data.errors_iface = nullptr;

    const struct dbus_register_submodule_t self =
    {
        connect_to_session_bus,
        &dbus_error_messages_data,
        export_self,
        nullptr,
        nullptr,
        shutdown_dbus,
    };

    dbus_common_register_submodule(&self);
}

tdbusErrors *DBusErrorMessages::get_iface()
{
    return dbus_error_messages_data.errors_iface;
}

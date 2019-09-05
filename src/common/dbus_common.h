/*
 * Copyright (C) 2015, 2016, 2019  T+A elektroakustik GmbH & Co. KG
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

#ifndef DBUS_COMMON_H
#define DBUS_COMMON_H

#include <gio/gio.h>
#include <stdbool.h>

/*!
 * \addtogroup dbus D-Bus handling
 */
/*!@{*/

#ifdef __cplusplus
extern "C" {
#endif

struct dbus_register_submodule_t
{
    const bool connect_to_session_bus;
    void *const user_data;

    void (*const bus_acquired)(GDBusConnection *connection,
                               const gchar *name, bool is_session_bus,
                               gpointer user_data);
    void (*const name_acquired)(GDBusConnection *connection,
                                const gchar *name, bool is_session_bus,
                                gpointer user_data);
    void (*const destroy_notification)(bool is_session_bus,
                                       gpointer user_data);
    void (*const shutdown)(bool is_session_bus, gpointer user_data);
};

void dbus_common_register_submodule(const struct dbus_register_submodule_t *submodule);
int dbus_common_setup(GMainLoop *loop, const char *bus_name);
void dbus_common_shutdown(GMainLoop *loop);

int dbus_common_try_export_iface(GDBusConnection *connection,
                                 GDBusInterfaceSkeleton *iface,
                                 const char *dbus_object_path);
void dbus_common_unknown_signal(const char *iface_name,
                                const char *signal_name,
                                const char *sender_name);

#ifdef __cplusplus
}
#endif

/*!@}*/

/*!
 * \addtogroup dbus_handlers D-Bus handlers for signals
 * \ingroup dbus
 */

#endif /* !DBUS_COMMON_H */

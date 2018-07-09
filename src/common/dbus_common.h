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

int dbus_common_handle_error(GError **error);
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

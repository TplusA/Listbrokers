/*
 * Copyright (C) 2017, 2019  T+A elektroakustik GmbH & Co. KG
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

#include "dbus_artcache_iface.hh"
#include "dbus_artcache_iface_deep.h"
#include "dbus_common.h"
#include "messages.h"

struct dbus_artcache_data_t
{
    tdbusartcacheWrite *write_proxy;
};

static void connect_to_artcache(GDBusConnection *connection,
                                const gchar *name,
                                bool is_session_bus, gpointer user_data)
{
    auto *const data = static_cast<dbus_artcache_data_t *>(user_data);

    GError *error = NULL;

    data->write_proxy =
        tdbus_artcache_write_proxy_new_sync(connection,
                                            G_DBUS_PROXY_FLAGS_NONE,
                                            "de.tahifi.TACAMan", "/de/tahifi/TACAMan",
                                            NULL, &error);
    (void)dbus_common_handle_error(&error);
}

static void shutdown_dbus(bool is_session_bus, gpointer user_data)
{
    auto *const data = static_cast<dbus_artcache_data_t *>(user_data);

    g_object_unref(data->write_proxy);
}

static struct dbus_artcache_data_t dbus_artcache_data;

void DBusArtCache::dbus_setup(bool connect_to_session_bus)
{
    dbus_artcache_data.write_proxy = NULL;

    const struct dbus_register_submodule_t self =
    {
        .connect_to_session_bus = connect_to_session_bus,
        .user_data = &dbus_artcache_data,
        .bus_acquired = nullptr,
        .name_acquired = connect_to_artcache,
        .destroy_notification = nullptr,
        .shutdown = shutdown_dbus,
    };

    dbus_common_register_submodule(&self);
}

tdbusartcacheWrite *dbus_artcache_get_write_iface(void)
{
    return dbus_artcache_data.write_proxy;
}

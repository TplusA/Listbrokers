/*
 * Copyright (C) 2015, 2016, 2017  T+A elektroakustik GmbH & Co. KG
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

#include "dbus_lists_iface.h"
#include "dbus_lists_iface_deep.h"
#include "dbus_common.h"
#include "lists_dbus.h"
#include "messages.h"

struct dbus_lists_data_t
{
    const char *dbus_object_path;

    tdbuslistsNavigation *navigation_iface;
    struct DBusNavlistsIfaceData *iface_data;
};

static void connect_dbus_lists_handlers(GDBusConnection *connection,
                                        const gchar *name, bool is_session_bus,
                                        gpointer user_data)
{
    struct dbus_lists_data_t *data = user_data;

    data->navigation_iface = tdbus_lists_navigation_skeleton_new();

    g_signal_connect(data->navigation_iface, "handle-get-list-contexts",
                     G_CALLBACK(dbusmethod_navlists_get_list_contexts), data->iface_data);
    g_signal_connect(data->navigation_iface, "handle-get-range",
                     G_CALLBACK(dbusmethod_navlists_get_range), data->iface_data);
    g_signal_connect(data->navigation_iface, "handle-get-range-with-meta-data",
                     G_CALLBACK(dbusmethod_navlists_get_range_with_meta_data), data->iface_data);
    g_signal_connect(data->navigation_iface, "handle-check-range",
                     G_CALLBACK(dbusmethod_navlists_check_range), data->iface_data);
    g_signal_connect(data->navigation_iface, "handle-get-list-id",
                     G_CALLBACK(dbusmethod_navlists_get_list_id), data->iface_data);
    g_signal_connect(data->navigation_iface, "handle-get-parameterized-list-id",
                     G_CALLBACK(dbusmethod_navlists_get_parameterized_list_id), data->iface_data);
    g_signal_connect(data->navigation_iface, "handle-get-parent-link",
                     G_CALLBACK(dbusmethod_navlists_get_parent_link), data->iface_data);
    g_signal_connect(data->navigation_iface, "handle-get-root-link-to-context",
                     G_CALLBACK(dbusmethod_navlists_get_root_link_to_context), data->iface_data);
    g_signal_connect(data->navigation_iface, "handle-get-uris",
                     G_CALLBACK(dbusmethod_navlists_get_uris), data->iface_data);
    g_signal_connect(data->navigation_iface, "handle-get-ranked-stream-links",
                     G_CALLBACK(dbusmethod_navlists_get_ranked_stream_links), data->iface_data);
    g_signal_connect(data->navigation_iface, "handle-discard-list",
                     G_CALLBACK(dbusmethod_navlists_discard_list), data->iface_data);
    g_signal_connect(data->navigation_iface, "handle-keep-alive",
                     G_CALLBACK(dbusmethod_navlists_keep_alive), data->iface_data);
    g_signal_connect(data->navigation_iface, "handle-force-in-cache",
                     G_CALLBACK(dbusmethod_navlists_force_in_cache), data->iface_data);
    g_signal_connect(data->navigation_iface, "handle-get-location-key",
                     G_CALLBACK(dbusmethod_navlists_get_location_key), data->iface_data);
    g_signal_connect(data->navigation_iface, "handle-get-location-trace",
                     G_CALLBACK(dbusmethod_navlists_get_location_trace), data->iface_data);
    g_signal_connect(data->navigation_iface, "handle-realize-location",
                     G_CALLBACK(dbusmethod_navlists_realize_location), data->iface_data);
    g_signal_connect(data->navigation_iface, "handle-abort-realize-location",
                     G_CALLBACK(dbusmethod_navlists_abort_realize_location), data->iface_data);

    dbus_common_try_export_iface(connection,
                                 G_DBUS_INTERFACE_SKELETON(data->navigation_iface),
                                 data->dbus_object_path);
}

static void shutdown_dbus(bool is_session_bus, gpointer user_data)
{
    struct dbus_lists_data_t *data = user_data;

    g_object_unref(data->navigation_iface);
}

static struct dbus_lists_data_t dbus_lists_data;

/*!
 * Prepare for serving \c de.tahifi.Lists.Navigation interface.
 */
void dbus_lists_setup(bool connect_to_session_bus,
                      const char *dbus_object_path,
                      struct DBusNavlistsIfaceData *iface_data)
{
    dbus_lists_data.dbus_object_path = dbus_object_path;
    dbus_lists_data.navigation_iface = NULL;
    dbus_lists_data.iface_data = iface_data;

    const struct dbus_register_submodule_t self =
    {
        .connect_to_session_bus = connect_to_session_bus,
        .user_data = &dbus_lists_data,
        .bus_acquired = connect_dbus_lists_handlers,
        .shutdown = shutdown_dbus,
    };

    dbus_common_register_submodule(&self);
}

tdbuslistsNavigation *dbus_lists_get_navigation_iface(void)
{
    return dbus_lists_data.navigation_iface;
}

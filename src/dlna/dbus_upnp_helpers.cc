/*
 * Copyright (C) 2015, 2018, 2019  T+A elektroakustik GmbH & Co. KG
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

#include "dbus_upnp_helpers.hh"
#include "dbus_upnp_iface_deep.h"
#include "gerrorwrapper.hh"

std::string UPnP::get_proxy_object_path(tdbusdleynaserverMediaDevice *proxy)
{
    log_assert(proxy != nullptr);

    return std::string(g_dbus_proxy_get_object_path(G_DBUS_PROXY(proxy)));
}

bool UPnP::proxy_object_path_equals(tdbusdleynaserverMediaDevice *proxy,
                                    const std::string &path)
{
    log_assert(proxy != nullptr);

    return path == g_dbus_proxy_get_object_path(G_DBUS_PROXY(proxy));
}

bool UPnP::create_media_device_proxy_for_object_path_begin(const std::string &path,
                                                           GCancellable *cancellable,
                                                           GAsyncReadyCallback callback,
                                                           void *callback_data)
{
    auto *proxy = G_DBUS_PROXY(dbus_upnp_get_dleynaserver_manager_iface());
    if(proxy == nullptr)
        return false;

    GDBusConnection *connection = g_dbus_proxy_get_connection(proxy);

    tdbus_dleynaserver_media_device_proxy_new(connection,
                                              G_DBUS_PROXY_FLAGS_NONE,
                                              "com.intel.dleyna-server",
                                              path.c_str(), cancellable,
                                              callback, callback_data);
    return true;
}

tdbusdleynaserverMediaDevice *
UPnP::create_media_device_proxy_for_object_path_end(const std::string &path,
                                                    GAsyncResult *res)
{
    GErrorWrapper error;
    tdbusdleynaserverMediaDevice *proxy =
        tdbus_dleynaserver_media_device_proxy_new_finish(res, error.await());

    if(!error.log_failure("Create dLeyna media device proxy"))
        return proxy;

    msg_error(0, LOG_NOTICE,
              "Failed obtaining D-Bus proxy for UPnP server %s", path.c_str());

    if(proxy != nullptr)
        g_object_unref(proxy);

    return nullptr;
}

bool UPnP::is_media_device_usable(tdbusdleynaserverMediaDevice *proxy)
{
    gchar **temp = g_dbus_proxy_get_cached_property_names(G_DBUS_PROXY(proxy));

    if(temp == nullptr)
        return false;
    else
    {
        g_strfreev(temp);
        return true;
    }
}

tdbusupnpMediaContainer2 *
UPnP::create_media_container_proxy_for_object_path(const char *path)
{
    auto *proxy = G_DBUS_PROXY(dbus_upnp_get_dleynaserver_manager_iface());
    if(proxy == nullptr)
        return nullptr;

    GDBusConnection *connection = g_dbus_proxy_get_connection(proxy);

    GErrorWrapper error;

    tdbusupnpMediaContainer2 *container_proxy =
        tdbus_upnp_media_container2_proxy_new_sync(connection,
                                                   G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
                                                   "com.intel.dleyna-server",
                                                   path, NULL, error.await());
    error.log_failure("Create UPnP media container proxy");

    return container_proxy;
}

tdbusupnpMediaItem2 *
UPnP::create_media_item_proxy_for_object_path(const char *path)
{
    auto *proxy = G_DBUS_PROXY(dbus_upnp_get_dleynaserver_manager_iface());
    if(proxy == nullptr)
        return nullptr;

    GDBusConnection *connection = g_dbus_proxy_get_connection(proxy);

    GErrorWrapper error;

    tdbusupnpMediaItem2 *item_proxy =
        tdbus_upnp_media_item2_proxy_new_sync(connection,
                                              G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
                                              "com.intel.dleyna-server",
                                              path, NULL, error.await());
    error.log_failure("Create UPnP media item proxy");

    return item_proxy;
}

uint32_t UPnP::get_size_of_container(const std::string &path)
{
    tdbusupnpMediaContainer2 *proxy =
        create_media_container_proxy_for_object_path(path.c_str());
    if(proxy == nullptr)
        return 0;

    guint retval = tdbus_upnp_media_container2_get_child_count(proxy);

    g_object_unref(proxy);

    return retval;
}

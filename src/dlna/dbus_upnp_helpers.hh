/*
 * Copyright (C) 2015, 2018  T+A elektroakustik GmbH & Co. KG
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

#ifndef DBUS_UPNP_HELPERS_HH
#define DBUS_UPNP_HELPERS_HH

#include <string>

#include "upnp_dleynaserver_dbus.h"
#include "upnp_media_dbus.h"

namespace UPnP
{

std::string get_proxy_object_path(tdbusdleynaserverMediaDevice *proxy);
bool proxy_object_path_equals(tdbusdleynaserverMediaDevice *proxy,
                              const std::string &path);
void create_media_device_proxy_for_object_path_begin(const std::string &path,
                                                     GCancellable *cancellable,
                                                     GAsyncReadyCallback callback,
                                                     void *callback_data);
tdbusdleynaserverMediaDevice *
create_media_device_proxy_for_object_path_end(const std::string &path, GAsyncResult *res);
bool is_media_device_usable(tdbusdleynaserverMediaDevice *proxy);
tdbusupnpMediaContainer2 *create_media_container_proxy_for_object_path(const char *path);
tdbusupnpMediaItem2 *create_media_item_proxy_for_object_path(const char *path);
uint32_t get_size_of_container(const std::string &path);

};

#endif /* !DBUS_UPNP_HELPERS_HH */

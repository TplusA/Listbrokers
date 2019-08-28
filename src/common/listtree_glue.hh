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

#ifndef LISTTREE_GLUE_HH
#define LISTTREE_GLUE_HH

#include <glib.h>

#include "listtree.hh"
#include "lists_base.hh"
#include "dbus_common.h"
#include "dbus_artcache_iface_deep.h"

GVariant *hash_to_variant(const ListItemKey &key);

template <typename T>
static void send_cover_art(const ListItem_<T> &item,
                           const ListItemKey &item_key, uint8_t priority)
{
    const Url::String album_art_url(item.get_specific_data().get_album_art_url());

    if(album_art_url.empty())
        return;

    GError *error = nullptr;
    tdbus_artcache_write_call_add_image_by_uri_sync(dbus_artcache_get_write_iface(),
                                                    hash_to_variant(item_key),
                                                    priority,
                                                    album_art_url.get_cleartext().c_str(),
                                                    NULL, &error);
    dbus_common_handle_error(&error);
}

#endif /* !LISTTREE_GLUE_HH */

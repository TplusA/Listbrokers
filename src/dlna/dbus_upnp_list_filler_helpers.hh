/*
 * Copyright (C) 2015  T+A elektroakustik GmbH & Co. KG
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

#ifndef DBUS_UPNP_LIST_FILLER_HELPERS_HH
#define DBUS_UPNP_LIST_FILLER_HELPERS_HH

#include "lru.hh"
#include "lists_base.hh"

namespace UPnP
{

template <typename T>
const TiledListFillerIface<T> &get_tiled_list_filler_for_root_directory();

void init_standard_dbus_fillers(const LRU::Cache &cache);

};

#endif /* !DBUS_UPNP_LIST_FILLER_HELPERS_HH */

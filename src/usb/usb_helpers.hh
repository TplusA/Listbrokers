/*
 * Copyright (C) 2015, 2019  T+A elektroakustik GmbH & Co. KG
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

#ifndef USB_HELPERS_HH
#define USB_HELPERS_HH

#include <string>
#include <memory>

#include "lru.hh"

namespace USB
{

class ListTree;
class DeviceList;
class DirList;

namespace Helpers
{

void init(ListTree &lt, const ::LRU::Cache &cache);

/*!
 * Look up cached list of USB devices.
 *
 * \see #USB::ListTree::get_list_of_usb_devices()
 */
std::shared_ptr<const DeviceList> get_list_of_usb_devices();

/*!
 * Construct absolute path in file system to given item in list.
 */
bool construct_fspath_to_item(const DirList &list, ID::Item item_id,
                              std::string &path, const char *prefix = nullptr);

}

}

#endif /* !USB_HELPERS_HH */

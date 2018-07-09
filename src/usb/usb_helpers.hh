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

/*
 * Copyright (C) 2015, 2017, 2019  T+A elektroakustik GmbH & Co. KG
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

#include <string>
#include <curlpp/cURLpp.hpp>

#include "usb_helpers.hh"
#include "usb_listtree.hh"
#include "messages.h"

static USB::ListTree *usb_helpers_list_tree_pointer;
static const LRU::Cache *usb_helpers_lru_pointer;

void USB::Helpers::init(USB::ListTree &lt, const ::LRU::Cache &cache)
{
    usb_helpers_list_tree_pointer = &lt;
    usb_helpers_lru_pointer = &cache;
    curlpp::initialize();
}

std::shared_ptr<const USB::DeviceList> USB::Helpers::get_list_of_usb_devices()
{
    log_assert(usb_helpers_list_tree_pointer != nullptr);

    auto id = usb_helpers_list_tree_pointer->get_root_list_id();
    log_assert(id.is_valid());

    return std::static_pointer_cast<const USB::DeviceList>(usb_helpers_lru_pointer->lookup(id));
}

bool USB::Helpers::construct_fspath_to_item(const DirList &list,
                                            ID::Item item_id,
                                            std::string &path,
                                            const char *prefix)
{
    log_assert(usb_helpers_list_tree_pointer != nullptr);
    log_assert(item_id.is_valid());

    const size_t list_depth = LRU::Entry::depth(list);
    log_assert(list_depth > 2);

    const DirList *dir_list_ptr = nullptr;
    std::shared_ptr<const LRU::Entry> lru_entry;
    std::vector<const std::string *> path_elements;

    for(size_t i = 2; i < list_depth; ++i)
    {
        dir_list_ptr = ((dir_list_ptr == nullptr)
                        ? &list
                        : static_cast<const DirList *>(lru_entry.get()));

        const auto &item = (*dir_list_ptr)[item_id];
        path_elements.push_back(&item.get_specific_data().get_name());

        if(!usb_helpers_list_tree_pointer->get_parent_link(dir_list_ptr->get_cache_id(),
                                                           item_id, lru_entry))
        {
            BUG("Item %u in list %u has no parent (but it must have)",
                item_id.get_raw_id(), dir_list_ptr->get_cache_id().get_raw_id());
            return false;
        }
    }

    const auto *volume_list = static_cast<const VolumeList *>(lru_entry.get());
    const auto &volume = (*volume_list)[item_id];

    /* prefix, if any, is assumed to be a protocol specification and needs not
     * be URL-encoded; the volume path needs no URL-encoding because it comes
     * from mounTA, which always uses simple non-fancy paths */
    path = (prefix == nullptr) ? "" : prefix;
    path += volume.get_specific_data().get_url();

    if(prefix == nullptr)
    {
        for(auto str = path_elements.rbegin(); str != path_elements.rend(); ++str)
        {
            path += '/';
            path += (*str)->c_str();
        }
    }
    else
    {
        /* URL-encode any path element that comes from the user */
        for(auto str = path_elements.rbegin(); str != path_elements.rend(); ++str)
        {
            path += '/';
            path += curlpp::escape(**str);
        }
    }

    return true;
}

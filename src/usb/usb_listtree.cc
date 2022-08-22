/*
 * Copyright (C) 2015--2020, 2022  T+A elektroakustik GmbH & Co. KG
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
#include <cstring>
#include <algorithm>

#include "usb_listtree.hh"
#include "strbo_url_usb.hh"
#include "strbo_url_helpers.hh"

constexpr const char USB::ListTree::CONTEXT_ID[];

void USB::ListTree::reinsert_volume_list(uint16_t device_id, uint32_t volume_number,
                                         size_t added_at_index)
{
    auto device_list_ptr = get_list_of_usb_devices();

    if(device_list_ptr == nullptr)
    {
        BUG("No device list, cannot reinsert volume list");
        return;
    }

    auto &device_list = *device_list_ptr;

    ID::Item device_index;
    const DeviceItemData *item_data =
        device_list.get_device_by_id(device_id, &device_index);

    if(item_data == nullptr)
    {
        BUG("No device data for device ID %u while reinserting volume list",
            device_id);
        return;
    }

    ListItem_<USB::DeviceItemData> &dev(device_list[device_index]);
    const auto volume_list = lt_manager_.lookup_list<USB::VolumeList>(dev.get_child_list());

    if(volume_list == nullptr)
        return;

    /*
     * So we have a volume list already. There are a few reasons as to why this
     * can happen:
     *
     * - The user is very fast and enters the device just when it appears on
     *   the display.
     * - The device is very slow and mounting takes ages.
     * - The device contains several partitions and mounting them all takes
     *   ages. See #741.
     *
     * Anyway, let's patch the volume list and GTFO.
     */
    ListItem_<VolumeItemData> vol;
    vol.get_specific_data() = VolumeItemData(device_id, volume_number);

    volume_list->insert_before(added_at_index, std::move(vol));

    dev.obliviate_child();
    auto volume_list_id = volume_list->get_cache_id();
    lt_manager_.reinsert_list(volume_list_id);
    dev.set_child_list(volume_list_id);
}

/*!
 * Check if the given ID refers to a #USB::VolumeItemData or is invalid.
 *
 * The purpose of this function is to distinguish between volume nodes and
 * directory item nodes, not to ensure validity of the given node. Odd as it
 * seems, this function returns \c true even if the ID passed in \p list_id is
 * invalid. The reason is simpler error handling on behalf of the caller.
 * Callers will pass the \p list_id on to the next function, and that function
 * will ultimately check the validity of the ID.
 */
static bool is_volume_list_or_invalid(const ListTreeManager &lt_manager,
                                      ID::List root_id, ID::List list_id)
{
    log_assert(root_id.is_valid());
    log_assert(list_id.is_valid());

    const ID::List parent_id = lt_manager.get_parent_list_id(list_id);

    return !parent_id.is_valid() || parent_id == root_id;
}

void USB::ListTree::pre_main_loop()
{
    lt_manager_.announce_root_list(devices_list_id_);

    auto devices = lt_manager_.lookup_list<USB::DeviceList>(devices_list_id_);
    log_assert(devices != nullptr);

    if(devices->init_from_mounta())
        reinsert_device_list();
}

I18n::String USB::ListTree::get_child_list_title(ID::List list_id,
                                                 ID::Item child_item_id)
{
    if(list_id == devices_list_id_)
        return ListTreeManager::get_dynamic_title<USB::DeviceList>(lt_manager_, list_id, child_item_id);
    else if(is_volume_list_or_invalid(lt_manager_, devices_list_id_, list_id))
        return ListTreeManager::get_dynamic_title<USB::VolumeList>(lt_manager_, list_id, child_item_id);
    else
        return ListTreeManager::get_dynamic_title<USB::DirList>(lt_manager_, list_id, child_item_id);
}

ID::List USB::ListTree::enter_child(ID::List list_id, ID::Item item_id, ListError &error)
{
    if(list_id == devices_list_id_)
        return lt_manager_.enter_child<USB::DeviceList, USB::VolumeItemData>(list_id, item_id, may_continue_fn_, error);
    else if(is_volume_list_or_invalid(lt_manager_, devices_list_id_, list_id))
        return lt_manager_.enter_child<USB::VolumeList, USB::ItemData>(list_id, item_id, may_continue_fn_, error);
    else
        return lt_manager_.enter_child<USB::DirList, USB::ItemData>(list_id, item_id, may_continue_fn_, error);
}

template <typename T>
static bool for_each_item_generic_apply_fn(ID::Item item_id, T &item,
                                           const ListTreeIface::ForEachGenericCallback &callback)
{
    ListTreeIface::ForEachItemDataGeneric data(item.get_kind());

    item.get_name(data.name_);

    return callback(data);
}

template <typename T>
static bool for_each_item_detailed_apply_fn(ID::Item item_id, T &item,
                                            const ListTreeIface::ForEachDetailedCallback &callback)
{
    std::string temp;
    item.get_name(temp);

    const ListTreeIface::ForEachItemDataDetailed data(temp, item.get_kind());

    return callback(data);
}

ListError USB::ListTree::for_each(ID::List list_id, ID::Item first, size_t count,
                                  const ListTreeIface::ForEachGenericCallback &callback) const
{
    if(list_id == devices_list_id_)
    {
        const std::function<bool(ID::Item, const ListItem_<USB::DeviceItemData> &)> fn =
            [&callback]
            (ID::Item item_id, const ListItem_<USB::DeviceItemData> &item)
            {
                return for_each_item_generic_apply_fn<const ListItem_<USB::DeviceItemData>>(item_id, item, callback);
            };

        return ::for_each_item(lt_manager_.lookup_list<const USB::DeviceList>(list_id),
                               first, count, fn);
    }
    else if(is_volume_list_or_invalid(lt_manager_, devices_list_id_, list_id))
    {
        const std::function<bool(ID::Item, const ListItem_<USB::VolumeItemData> &)> fn =
            [&callback]
            (ID::Item item_id, const ListItem_<USB::VolumeItemData> &item)
            {
                return for_each_item_generic_apply_fn<const ListItem_<USB::VolumeItemData>>(item_id, item, callback);
            };

        return ::for_each_item(lt_manager_.lookup_list<const USB::VolumeList>(list_id),
                               first, count, fn);
    }
    else
    {
        const std::function<bool(ID::Item, const ListItem_<USB::ItemData> &)> fn =
            [&callback]
            (ID::Item item_id, const ListItem_<USB::ItemData> &item)
            {
                return for_each_item_generic_apply_fn<const ListItem_<USB::ItemData>>(item_id, item, callback);
            };

        return ::for_each_item(lt_manager_.lookup_list<const USB::DirList>(list_id),
                               first, count, fn);
    }
}

ListError USB::ListTree::for_each(ID::List list_id, ID::Item first, size_t count,
                                  const ListTreeIface::ForEachDetailedCallback &callback) const
{
    if(list_id == devices_list_id_)
    {
        const std::function<bool(ID::Item, const ListItem_<USB::DeviceItemData> &)> fn =
            [&callback]
            (ID::Item item_id, const ListItem_<USB::DeviceItemData> &item)
            {
                return for_each_item_detailed_apply_fn<const ListItem_<USB::DeviceItemData>>(item_id, item, callback);
            };

        return ::for_each_item(lt_manager_.lookup_list<const USB::DeviceList>(list_id),
                               first, count, fn);
    }
    else if(is_volume_list_or_invalid(lt_manager_, devices_list_id_, list_id))
    {
        const std::function<bool(ID::Item, const ListItem_<USB::VolumeItemData> &)> fn =
            [&callback]
            (ID::Item item_id, const ListItem_<USB::VolumeItemData> &item)
            {
                return for_each_item_detailed_apply_fn<const ListItem_<USB::VolumeItemData>>(item_id, item, callback);
            };

        return ::for_each_item(lt_manager_.lookup_list<const USB::VolumeList>(list_id),
                               first, count, fn);
    }
    else
    {
        const std::function<bool(ID::Item, const ListItem_<USB::ItemData> &)> fn =
            [&callback]
            (ID::Item item_id, const ListItem_<USB::ItemData> &item)
            {
                return for_each_item_detailed_apply_fn<const ListItem_<USB::ItemData>>(item_id, item, callback);
            };

        return ::for_each_item(lt_manager_.lookup_list<const USB::DirList>(list_id),
                               first, count, fn);
    }
}

ssize_t USB::ListTree::size(ID::List list_id) const
{
    if(list_id == devices_list_id_)
    {
        const auto list = lt_manager_.lookup_list<const USB::DeviceList>(list_id);

        if(list != nullptr)
            return list->size();
    }
    else if(is_volume_list_or_invalid(lt_manager_, devices_list_id_, list_id))
    {
        const auto list = lt_manager_.lookup_list<const USB::VolumeList>(list_id);

        if(list != nullptr)
            return list->size();
    }
    else
    {
        const auto list = lt_manager_.lookup_list<const USB::DirList>(list_id);

        if(list != nullptr)
            return list->size();
    }

    return -1;
}

ID::List USB::ListTree::get_parent_link(ID::List list_id, ID::Item &parent_item_id) const
{
    std::shared_ptr<LRU::Entry const> parent_list;

    if(get_parent_link(list_id, parent_item_id, parent_list))
        return parent_list->get_cache_id();
    else
        return ID::List();
}

bool USB::ListTree::get_parent_link(ID::List list_id, ID::Item &parent_item_id,
                                    std::shared_ptr<const LRU::Entry> &parent_list) const
{
    const auto list = lt_manager_.lookup_list<const LRU::Entry>(list_id);

    if(list == nullptr)
        return false;

    parent_list = list->get_parent();

    if(parent_list == nullptr)
    {
        parent_list = list;
        return true;
    }

    const bool ok =
        (parent_list->get_cache_id() == devices_list_id_)
        ? std::static_pointer_cast<const USB::DeviceList>(parent_list)->lookup_item_id_by_child_id(list_id, parent_item_id)
        : (is_volume_list_or_invalid(lt_manager_, devices_list_id_, parent_list->get_cache_id())
           ? std::static_pointer_cast<const USB::VolumeList>(parent_list)->lookup_item_id_by_child_id(list_id, parent_item_id)
           : std::static_pointer_cast<const USB::DirList>(parent_list)->lookup_item_id_by_child_id(list_id, parent_item_id));

    if(!ok)
        BUG("Failed to find item in list %u linking to child list %u",
            parent_list->get_cache_id().get_raw_id(), list_id.get_raw_id());

    return ok;
}

ID::List USB::ListTree::get_link_to_context_root_impl(const char *context_id,
                                                      ID::Item &item_id,
                                                      bool &context_is_known,
                                                      bool &context_has_parent)
{
    context_is_known = (strcmp(context_id, CONTEXT_ID) == 0);
    return ID::List();
}

ListError USB::ListTree::get_uris_for_item(ID::List list_id, ID::Item item_id,
                                           std::vector<Url::String> &uris,
                                           ListItemKey &item_key) const
{
    uris.clear();

    if(list_id == devices_list_id_ ||
       is_volume_list_or_invalid(lt_manager_, devices_list_id_, list_id))
        return ListError(ListError::INVALID_ID);

    const auto list = lt_manager_.lookup_list<const USB::DirList>(list_id);

    if(list == nullptr)
        return ListError(ListError::INVALID_ID);

    if(item_id.get_raw_id() >= list->size())
        return ListError(ListError::INVALID_ID);

    const auto &item = (*list)[item_id];

    if(item.get_kind().is_directory())
        return ListError();

    std::string temp;
    USB::Helpers::construct_fspath_to_item(*list, item_id, temp, "file://");
    uris.emplace_back(Url::String(Url::Sensitivity::GENERIC, std::move(temp)));

    uris.back().compute_hash(item_key.get_for_setting());

    return ListError();
}

template <typename ListType>
static bool get_component_name(const USB::ListTree &lt,
                               const ListTreeManager &lt_manager,
                               std::shared_ptr<const LRU::Entry> &lru_entry,
                               ID::List dest_list_id, ID::Item &item_id,
                               ListError &error, const char *what,
                               std::function<bool(const ListType &, ID::Item)> process)
{
    const auto list = (lru_entry != nullptr)
        ? std::static_pointer_cast<const ListType>(lru_entry)
        : lt_manager.lookup_list<const ListType>(dest_list_id);

    if(list == nullptr || item_id.get_raw_id() >= list->size())
    {
        error = ListError::INVALID_ID;
        return false;
    }

    if(!process(*list, item_id))
    {
        BUG("Item %u in %s list %u has no name",
            item_id.get_raw_id(), what, list->get_cache_id().get_raw_id());
        error = ListError::INTERNAL;
        return false;
    }

    if(!lt.get_parent_link(list->get_cache_id(), item_id, lru_entry))
    {
        BUG("Item %u in %s list %u has no parent",
            item_id.get_raw_id(), what, list->get_cache_id().get_raw_id());
        error = ListError::INTERNAL;
        return false;
    }

    return true;
}

bool USB::ListTree::can_handle_strbo_url(const std::string &url) const
{
    return USB::LocationKeySimple::get_scheme().url_matches_scheme(url) ||
           USB::LocationKeyReference::get_scheme().url_matches_scheme(url) ||
           USB::LocationTrace::get_scheme().url_matches_scheme(url);
}

static ListError enter_volume(USB::ListTree &lt,
                              const std::string &device_name,
                              const std::string &volume_name,
                              ID::List &rootdir_list_id,
                              std::pair<ID::List, ID::Item> &parent_link_candidate,
                              std::pair<ID::List, ID::Item> &parent_link,
                              ListTreeIface::RealizeURLResult &result)
{
    if(device_name.empty())
        return ListError(ListError::INTERNAL);

    if(volume_name.empty())
        msg_vinfo(MESSAGE_LEVEL_DEBUG,
                  "Entering list of volumes on device \"%s\"",
                  device_name.c_str());
    else
        msg_vinfo(MESSAGE_LEVEL_DEBUG,
                  "Entering volume \"%s\" on device \"%s\"",
                  volume_name.c_str(), device_name.c_str());

    auto device_list = lt.get_list_of_usb_devices();
    ID::Item device_index;
    const auto *dev =
        device_list->get_device_by_name(device_name, &device_index);

    if(dev == nullptr)
    {
        msg_error(0, LOG_NOTICE,
                  "Device \"%s\" not found", device_name.c_str());
        return ListError(ListError::NOT_FOUND);
    }

    ListError error;

    const ID::List volumes_list_id =
        lt.enter_child(device_list->get_cache_id(), device_index, error);

    if(!volumes_list_id.is_valid())
        return error;

    parent_link_candidate =
        std::make_pair(device_list->get_cache_id(), ID::Item(device_index));

    size_t volume_index = 0;
    ListItemKind volume_kind(ListItemKind::LOGOUT_LINK);
    bool volume_found = false;

    if(!volume_name.empty())
        error = lt.for_each(volumes_list_id, ID::Item(), 0,
                    [&volume_name, &volume_found, &volume_index, &volume_kind]
                    (const ListTreeIface::ForEachItemDataGeneric &vol_data)
                    {
                        if(vol_data.name_ == volume_name)
                        {
                            volume_found = true;
                            volume_kind = vol_data.kind_;
                            return false;
                        }

                        ++volume_index;
                        return true;
                    });

    if(error.failed() || !volume_found)
    {
        result.set_item_data(device_list->get_cache_id(), device_index,
                             dev->get_kind());

        if(error.failed() || volume_name.empty())
            return error;

        msg_error(0, LOG_NOTICE, "Volume \"%s\" not found on device \"%s\"",
                  volume_name.c_str(), device_name.c_str());
        return ListError(ListError::NOT_FOUND);
    }

    rootdir_list_id = lt.enter_child(volumes_list_id, ID::Item(volume_index),
                                     error);

    if(rootdir_list_id.is_valid())
    {
        parent_link = parent_link_candidate;
        parent_link_candidate =
            std::make_pair(volumes_list_id, ID::Item(volume_index));
        result.set_item_data(volumes_list_id, ID::Item(volume_index),
                             volume_kind);
    }

    return error;
}

static ListError follow_path(USB::ListTree &lt, const std::string &path,
                             ID::List &dir_list_id,
                             std::pair<ID::List, ID::Item> &parent_link_candidate,
                             std::pair<ID::List, ID::Item> &parent_link,
                             std::pair<ID::Item, size_t> &&range,
                             bool auto_search_on_range_failure,
                             std::function<ListError(ID::List, ID::Item, ListItemKind)> found_item)
{
    if(!dir_list_id.is_valid())
        return ListError(path.empty() ? ListError::OK : ListError::INVALID_STRBO_URL);

    msg_vinfo(MESSAGE_LEVEL_DEBUG, "Following path \"%s\"", path.c_str());

    ListError error;
    auto path_iter = path.begin();

    auto component_start =
        std::find_if_not(path_iter, path.end(), [] (const char &ch)
                                                { return ch == '/'; });
    while(!error.failed() &&
          component_start != path.end() && path_iter != path.end())
    {
        const auto component_end =
            std::find_if(component_start + 1, path.end(),
                         [] (const char &ch) { return ch == '/'; });

        path_iter = component_end;

        const std::string component(component_end != path.end()
                                    ? path.substr(component_start - path.begin(),
                                                  component_end - component_start)
                                    : path.substr(component_start - path.begin()));

        size_t idx = range.first.get_raw_id();
        ListItemKind kind(ListItemKind::LOGOUT_LINK);
        bool found = false;

        for(int i = 0; i < 2; ++i)
        {
            error = lt.for_each(dir_list_id, range.first, range.second,
                        [&component, &found, &idx, &kind]
                        (const ListTreeIface::ForEachItemDataGeneric &item_data)
                        {
                            if(item_data.name_ == component)
                            {
                                found = true;
                                kind = item_data.kind_;
                                return false;
                            }

                            ++idx;
                            return true;
                        });

            if(found || i > 0)
                break;

            /* first round, nothing found */
            if(!auto_search_on_range_failure)
                break;

            if(range.first == ID::Item() && range.second == 0)
                break;

            msg_vinfo(MESSAGE_LEVEL_DEBUG,
                      "Lookup \"%s\" in range failed, searching entire list",
                      component.c_str());

            range.first = ID::Item();
            range.second = 0;
            idx = 0;
        }

        if(error.failed())
            break;

        if(!found)
        {
            msg_error(0, LOG_NOTICE,
                      "Path component \"%s\" not found", component.c_str());
            return ListError(ListError::NOT_FOUND);
        }

        component_start =
            std::find_if_not(path_iter, path.end(), [] (const char &ch)
                                                    { return ch == '/'; });
        parent_link = parent_link_candidate;

        if(kind.is_directory())
        {
            const auto next_id =
                lt.enter_child(dir_list_id, ID::Item(idx), error);

            if(!next_id.is_valid())
                break;

            error = found_item(dir_list_id, ID::Item(idx), kind);

            if(!error.failed())
                parent_link_candidate = std::make_pair(dir_list_id, ID::Item(idx));

            dir_list_id = next_id;
        }
        else
        {
            error = found_item(dir_list_id, ID::Item(idx), kind);

            const bool is_last_component = (component_start == path.end());
            if(!is_last_component && !error.failed())
            {
                /* this should have been the last component in the path */
                msg_error(0, LOG_NOTICE,
                          "Cannot follow path through non-directory component");
                error = ListError(ListError::NOT_FOUND);
            }
        }
    }

    return error;
}

static inline ListError follow_path(USB::ListTree &lt, const std::string &path,
                                    ID::List &dir_list_id,
                                    std::pair<ID::List, ID::Item> &parent_link_candidate,
                                    std::pair<ID::List, ID::Item> &parent_link,
                                    std::function<ListError(ID::List, ID::Item, ListItemKind)> found_item)
{
    return follow_path(lt, path, dir_list_id,
                       parent_link_candidate, parent_link,
                       std::make_pair(ID::Item(), size_t(0)), false,
                       found_item);
}

static void set_list_title(USB::ListTree &lt,
                           const std::pair<ID::List, ID::Item> &parent_link,
                           ListTreeIface::RealizeURLResult &result)
{
    if(!result.list_id.is_valid())
        return;

    result.list_title = parent_link.first.is_valid()
        ? lt.get_child_list_title(parent_link.first, parent_link.second)
        : lt.get_root_list_title();
}

static ListError realize(USB::ListTree &lt, const std::string &url,
                         const USB::LocationKeySimple &key,
                         ListTreeIface::RealizeURLResult &result)
{
    msg_vinfo(MESSAGE_LEVEL_DIAG,
              "Realize simple location key \"%s\"", url.c_str());

    const USB::LocationKeySimple::Components &d(key.unpack());

    ID::List dir_list_id;
    std::pair<ID::List, ID::Item> parent_link_candidate;
    std::pair<ID::List, ID::Item> parent_link;

    ListError error = enter_volume(lt, d.device_, d.partition_, dir_list_id,
                                   parent_link_candidate, parent_link, result);

    if(!error.failed())
        error = follow_path(lt, d.path_, dir_list_id,
                            parent_link_candidate, parent_link,
                            [&result]
                            (ID::List list_id, ID::Item item_id, ListItemKind item_kind)
                            {
                                result.set_item_data(list_id, item_id, item_kind);
                                return ListError();
                            });

    set_list_title(lt, parent_link, result);

    return error;
}

static ListError realize(USB::ListTree &lt, const std::string &url,
                         const USB::LocationKeyReference &key,
                         ListTreeIface::RealizeURLResult &result)
{
    msg_vinfo(MESSAGE_LEVEL_DIAG,
              "Realize reference location key \"%s\"", url.c_str());

    const USB::LocationKeyReference::Components &d(key.unpack());

    ID::List dir_list_id;
    std::pair<ID::List, ID::Item> parent_link_candidate;
    std::pair<ID::List, ID::Item> parent_link;

    ListError error = enter_volume(lt, d.device_, d.partition_, dir_list_id,
                                   parent_link_candidate, parent_link, result);

    if(!error.failed())
        error = follow_path(lt, d.reference_point_, dir_list_id,
                            parent_link_candidate, parent_link,
                            [&result]
                            (ID::List list_id, ID::Item item_id, ListItemKind item_kind)
                            {
                                if(item_kind.is_directory())
                                {
                                    result.set_item_data(list_id, item_id, item_kind);
                                    return ListError();
                                }
                                else
                                {
                                    msg_error(0, LOG_NOTICE,
                                              "Path to reference contains non-directory component");
                                    return ListError(ListError::NOT_FOUND);
                                }
                            });

    if(!error.failed())
        error = follow_path(lt, d.item_name_, dir_list_id,
                            parent_link_candidate, parent_link,
                            d.item_position_.is_valid()
                            ? std::make_pair(ID::Item(d.item_position_.get_raw_id() - 1), 1)
                            : std::make_pair(ID::Item(), 0),
                            true,
                            [&d, &result]
                            (ID::List list_id, ID::Item item_id, ListItemKind item_kind)
                            {
                                if(d.item_position_.is_valid() &&
                                   d.item_position_.get_raw_id() != item_id.get_raw_id() + 1)
                                {
                                    msg_vinfo(MESSAGE_LEVEL_DEBUG,
                                              "Referenced item found at position %u, expected at %u",
                                              item_id.get_raw_id() + 1, d.item_position_.get_raw_id());
                                }

                                result.set_item_data(list_id, item_id, item_kind);
                                return ListError();
                            });

    set_list_title(lt, parent_link, result);

    return error;
}

static ListError realize(USB::ListTree &lt, const std::string &url,
                         const USB::LocationTrace &trace,
                         ListTreeIface::RealizeURLResult &result)
{
    msg_vinfo(MESSAGE_LEVEL_DIAG,
              "Realize location trace \"%s\"", url.c_str());

    const USB::LocationTrace::Components &d(trace.unpack());

    result.trace_length = trace.get_trace_length();

    ID::List dir_list_id;
    std::pair<ID::List, ID::Item> parent_link_candidate;
    std::pair<ID::List, ID::Item> parent_link;

    ListError error = enter_volume(lt, d.device_, d.partition_, dir_list_id,
                                   parent_link_candidate, parent_link, result);

    if(!error.failed())
        error = follow_path(lt, d.reference_point_, dir_list_id,
                            parent_link_candidate, parent_link,
                            [&result]
                            (ID::List list_id, ID::Item item_id, ListItemKind item_kind)
                            {
                                if(item_kind.is_directory())
                                {
                                    result.set_item_data(list_id, item_id, item_kind);
                                    return ListError();
                                }
                                else
                                {
                                    msg_error(0, LOG_NOTICE,
                                              "Path to reference contains non-directory component");
                                    return ListError(ListError::NOT_FOUND);
                                }
                            });

    if(!error.failed())
    {
        result.ref_list_id = parent_link_candidate.first;
        result.ref_item_id = parent_link_candidate.second;

        error = follow_path(lt, d.item_name_, dir_list_id,
                            parent_link_candidate, parent_link,
                            [&result]
                            (ID::List list_id, ID::Item item_id, ListItemKind item_kind)
                            {
                                result.set_item_data(list_id, item_id, item_kind);
                                ++result.distance;
                                return ListError();
                            });
    }

    set_list_title(lt, parent_link, result);

    return error;
}

ListError USB::ListTree::realize_strbo_url(const std::string &url,
                                           ListTreeIface::RealizeURLResult &result)
{
    ListError error;

    if(!Url::try_set_url_and_apply<USB::LocationKeySimple>(url, error,
            [this, &url, &result] (const USB::LocationKeySimple &key)
            { return realize(*this, url, key, result); }) &&
       !Url::try_set_url_and_apply<USB::LocationKeyReference>(url, error,
            [this, &url, &result] (const USB::LocationKeyReference &key)
            { return realize(*this, url, key, result); }) &&
       !Url::try_set_url_and_apply<USB::LocationTrace>(url, error,
            [this, &url, &result] (const USB::LocationTrace &trace)
            { return realize(*this, url, trace, result); }))
    {
        if(!error.failed())
        {
            BUG("Failed handling URL, but no error is set");
            error = ListError(ListError::INTERNAL);
        }
    }

    if(error.failed())
        msg_error(0, LOG_NOTICE, "Failed to handle URL %s (%s)",
                  url.c_str(), error.to_string());

    return error;
}

std::unique_ptr<Url::Location>
USB::ListTree::get_location_key(const ID::List list_id, const ID::RefPos item_pos,
                                bool as_reference_key, ListError &error) const
{
    size_t list_depth = lt_manager_.get_list_depth(list_id);

    if(list_depth == 0)
    {
        error = ListError::INVALID_ID;
        return nullptr;
    }

    auto simple_key =
        std::unique_ptr<USB::LocationKeySimple>(as_reference_key
                                                ? nullptr
                                                : new USB::LocationKeySimple());
    auto reference_key =
        std::unique_ptr<USB::LocationKeyReference>(as_reference_key
                                                   ? new USB::LocationKeyReference()
                                                   : nullptr);

    if(simple_key == nullptr && reference_key == nullptr)
    {
        msg_out_of_memory("USB location key");
        error = ListError::INTERNAL;
        return nullptr;
    }

    std::shared_ptr<const LRU::Entry> lru_entry;
    ID::Item current_item_id(item_pos.get_raw_id() - 1);

    if(list_depth <= 2)
    {
        if(list_depth == 1)
        {
            if(simple_key != nullptr)
                simple_key->set_partition("");
            else
                reference_key->set_partition("");
        }

        if(simple_key != nullptr)
            simple_key->set_path("");
        else
        {
            reference_key->set_reference_point("");
            reference_key->set_item("", (list_depth == 1) ? ID::RefPos() : item_pos);
        }
    }
    else
    {
        /* directories */
        std::vector<const std::string *> path_elements;
        path_elements.reserve(list_depth - 2);

        while(list_depth > 2)
        {
            if(!get_component_name<DirList>(*this, lt_manager_, lru_entry,
                    list_id, current_item_id, error, "directory",
                    [&path_elements]
                    (const DirList &list, ID::Item item) -> bool
                    {
                        const auto &temp(list[item].get_specific_data().get_name());

                        if(temp.empty())
                            return false;

                        path_elements.push_back(&temp);

                        return true;
                    }))
                return nullptr;

            --list_depth;
        }

        if(simple_key != nullptr)
            for(auto it = path_elements.rbegin(); it != path_elements.rend(); ++it)
                simple_key->append_to_path(**it);
        else
        {
            if(path_elements.size() > 1)
                for(auto it = path_elements.rbegin(); it != path_elements.rend() - 1; ++it)
                    reference_key->append_to_reference_point(**it);
            else
                reference_key->set_reference_point("");

            reference_key->set_item(*path_elements.front(), item_pos);
        }
    }

    if(list_depth == 2)
    {
        /* volume */
        if(!get_component_name<VolumeList>(*this, lt_manager_, lru_entry,
                list_id, current_item_id, error, "volume",
                [&simple_key, &reference_key]
                (const VolumeList &list, ID::Item item) -> bool
                {
                    std::string temp;
                    list[item].get_specific_data().get_name(temp);

                    if(temp.empty())
                        return false;

                    if(simple_key != nullptr)
                        simple_key->set_partition(std::move(temp));
                    else
                        reference_key->set_partition(std::move(temp));

                    return true;
                }))
            return nullptr;

        --list_depth;
    }

    /* device */
    log_assert(list_depth == 1);

    if(!get_component_name<DeviceList>(*this, lt_manager_, lru_entry,
            list_id, current_item_id, error, "device",
            [&simple_key, &reference_key]
            (const DeviceList &list, ID::Item item) -> bool
            {
                std::string temp;
                list[item].get_specific_data().get_name(temp);

                if(temp.empty())
                    return false;

                if(simple_key != nullptr)
                    simple_key->set_device(std::move(temp));
                else
                    reference_key->set_device(std::move(temp));

                return true;
            }))
        return nullptr;

    error = ListError::OK;

    if(simple_key != nullptr)
        return simple_key;
    else
        return reference_key;
}

static bool handle_reference_point(ID::List list_id, ID::Item item_id,
                                   ID::List ref_list_id, ID::RefPos ref_item_pos,
                                   bool &found_reference_point,
                                   std::function<void()> action_if_found = nullptr)
{
    if(list_id != ref_list_id)
        return true;

    if(item_id.get_raw_id() + 1 != ref_item_pos.get_raw_id())
    {
        msg_error(0, LOG_NOTICE, "Reference point mismatch");
        return false;
    }

    found_reference_point = true;

    if(action_if_found != nullptr)
        action_if_found();

    return true;
}

std::unique_ptr<Url::Location>
USB::ListTree::get_location_trace(ID::List list_id, ID::RefPos item_pos,
                                  ID::List ref_list_id, ID::RefPos ref_item_pos,
                                  ListError &error) const
{
    size_t list_depth = lt_manager_.get_list_depth(list_id);

    if(list_depth == 0)
    {
        error = ListError::INVALID_ID;
        return nullptr;
    }

    auto trace = std::unique_ptr<USB::LocationTrace>(new USB::LocationTrace());

    if(trace == nullptr)
    {
        msg_out_of_memory("USB location key");
        error = ListError::INTERNAL;
        return nullptr;
    }

    std::shared_ptr<const LRU::Entry> lru_entry;
    ID::Item current_item_id(item_pos.get_raw_id() - 1);
    bool found_reference_point = !ref_list_id.is_valid();

    if(list_depth <= 2)
    {
        if(list_depth == 1)
            trace->set_partition("");

        trace->set_reference_point("");
        trace->set_item("", (list_depth == 1) ? ID::RefPos() : item_pos);
    }
    else
    {
        /* directories */
        std::vector<const std::string *> ref_elements;
        std::vector<const std::string *> item_elements;
        std::vector<const std::string *> *elements = &item_elements;

        while(list_depth > 2)
        {
            if(!get_component_name<DirList>(*this, lt_manager_, lru_entry,
                    list_id, current_item_id, error, "directory",
                    [&ref_elements, &elements,
                     ref_list_id, ref_item_pos, &found_reference_point]
                    (const DirList &list, ID::Item item) -> bool
                    {
                        const auto &temp(list[item].get_specific_data().get_name());

                        if(temp.empty())
                            return false;

                        if(!handle_reference_point(list.get_cache_id(), item,
                                                   ref_list_id, ref_item_pos,
                                                   found_reference_point,
                                                   [&elements, &ref_elements] ()
                                                   {
                                                       elements = &ref_elements;
                                                   }))
                            return false;

                        elements->push_back(&temp);

                        return true;
                    }))
                return nullptr;

            --list_depth;
        }

        if(!ref_elements.empty())
            for(auto it = ref_elements.rbegin(); it != ref_elements.rend(); ++it)
                trace->append_to_reference_point(**it);
        else
            trace->set_reference_point("");

        if(item_elements.size() > 1)
            for(auto it = item_elements.rbegin(); it != item_elements.rend() - 1; ++it)
                trace->append_to_item_path(**it);

        trace->append_item(*item_elements.front(), item_pos);
    }

    if(list_depth == 2)
    {
        /* volume */
        if(!get_component_name<VolumeList>(*this, lt_manager_, lru_entry,
                list_id, current_item_id, error, "volume",
                [&trace, ref_list_id, ref_item_pos, &found_reference_point]
                (const VolumeList &list, ID::Item item) -> bool
                {
                    std::string temp;
                    list[item].get_specific_data().get_name(temp);

                    if(temp.empty())
                        return false;

                    if(!handle_reference_point(list.get_cache_id(), item,
                                               ref_list_id, ref_item_pos,
                                               found_reference_point))
                        return false;

                    trace->set_partition(std::move(temp));

                    return true;
                }))
            return nullptr;

        --list_depth;
    }

    /* device */
    log_assert(list_depth == 1);

    if(!get_component_name<DeviceList>(*this, lt_manager_, lru_entry,
            list_id, current_item_id, error, "device",
            [&trace, ref_list_id, ref_item_pos, &found_reference_point]
            (const DeviceList &list, ID::Item item) -> bool
            {
                std::string temp;
                list[item].get_specific_data().get_name(temp);

                if(temp.empty())
                    return false;

                if(!handle_reference_point(list.get_cache_id(), item,
                                           ref_list_id, ref_item_pos,
                                           found_reference_point))
                    return false;

                trace->set_device(std::move(temp));

                return true;
            }))
        return nullptr;

    if(!found_reference_point)
    {
        msg_error(0, LOG_NOTICE,
                "Reference point does not exist on path to root");
        error = ListError::INVALID_ID;
        return nullptr;
    }

    error = ListError::OK;

    return trace;
}

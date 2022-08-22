/*
 * Copyright (C) 2015--2019, 2021, 2022  T+A elektroakustik GmbH & Co. KG
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

#include <algorithm>
#include <dirent.h>

#include "usb_list.hh"
#include "usb_helpers.hh"
#include "enterchild_template.hh"
#include "dbus_usb_iface_deep.h"
#include "gerrorwrapper.hh"

bool USB::DeviceItemData::add_volume(uint32_t vol_id,
                                     const char *display_name_utf8,
                                     const char *mountpoint_path,
                                     size_t &added_at_index, bool bug_if_dupe)
{
    auto vol = std::unique_ptr<VolumeInfo>(new VolumeInfo(vol_id, display_name_utf8, mountpoint_path));
    if(vol == nullptr)
    {
        msg_out_of_memory("volume information");
        added_at_index = SIZE_MAX;
        return false;
    }

    auto it =
        std::lower_bound(volumes_.begin(), volumes_.end(), vol,
                         [] (const std::unique_ptr<VolumeInfo> &a,
                             const std::unique_ptr<VolumeInfo> &b)
                         {
                             return a->number_ < b->number_;
                         });

    if(it == volumes_.end() || vol_id < (*it)->number_)
    {
        const auto pos(volumes_.insert(it, std::move(vol)));
        added_at_index = std::distance(volumes_.begin(), pos);
        return true;
    }
    else
    {
        if(bug_if_dupe)
            BUG("Tried to add existing volume %u \"%s\" to device %u",
                vol_id, display_name_utf8, dev_id_);

        added_at_index = SIZE_MAX;

        return false;
    }
}

void USB::DeviceItemData::fill_volume_list(USB::VolumeList &volumes) const
{
    for(const auto &it : volumes_)
    {
        ListItem_<VolumeItemData> vol;
        vol.get_specific_data() = VolumeItemData(dev_id_, it->number_);
        volumes.append_unsorted(std::move(vol));
    }
}

const USB::DeviceItemData::VolumeInfo &
USB::DeviceItemData::lookup_existing_volume_info(uint32_t volume_number) const
{
    const auto vol = std::unique_ptr<VolumeInfo>(new VolumeInfo(volume_number, "", ""));

    auto it =
        std::lower_bound(volumes_.begin(), volumes_.end(), vol,
                         [] (const std::unique_ptr<VolumeInfo> &a,
                             const std::unique_ptr<VolumeInfo> &b)
                         {
                             return a->number_ < b->number_;
                         });

    log_assert(it != volumes_.end());
    log_assert((*it) != nullptr);

    return *(*it);
}

const I18n::String USB::DeviceList::LIST_TITLE(true, "All USB devices");

void USB::DeviceList::enumerate_direct_sublists(const LRU::Cache &cache,
                                                std::vector<ID::List> &nodes) const
{
    BUG("%s(): function shall not be called", __PRETTY_FUNCTION__);
}

void USB::DeviceList::obliviate_child(ID::List child_id, const Entry *child)
{
    ID::Item idx;

    if(lookup_item_id_by_child_id(child_id, idx))
        (*this)[idx].obliviate_child();
    else
        BUG("Got obliviate notification for USB device %u, "
            "but could not find it in device list (ID %u)",
            child_id.get_raw_id(), get_cache_id().get_raw_id());
}

static bool fill_list_from_mounta_data(USB::DeviceList &dev_list,
                                       GVariant *devices_variant,
                                       GVariant *volumes_variant)
{
    bool list_changed = false;

    for(size_t i = 0; i < g_variant_n_children(devices_variant); ++i)
    {
        GVariant *tuple = g_variant_get_child_value(devices_variant, i);

        uint16_t id;
        const char *devname;
        const char *uuid;
        const char *rootpath;
        const char *usb_port;

        g_variant_get(tuple, "(q&s&s&s&s)", &id, &devname, &uuid, &rootpath, &usb_port);

        if(dev_list.add_to_list(id, devname, usb_port))
            list_changed = true;

        g_variant_unref(tuple);
    }

    uint16_t current_device_id = 0;
    USB::DeviceItemData *current_device = nullptr;

    for(size_t i = 0; i < g_variant_n_children(volumes_variant); ++i)
    {
        GVariant *tuple = g_variant_get_child_value(volumes_variant, i);

        uint32_t number;
        const char *label;
        const char *mountpoint;
        uint16_t device_id;
        const char *uuid;

        g_variant_get(tuple, "(u&s&sq&s)", &number, &label, &mountpoint, &device_id, &uuid);

        if(device_id == 0)
            BUG("Received zero device ID for volume %u \"%s\" "
                "from MounTA (skipping)", number, label);
        else
        {
            if(device_id != current_device_id)
            {
                current_device = dev_list.get_device_by_id(device_id);
                current_device_id = (current_device != nullptr) ? device_id : 0;

                if(current_device == nullptr)
                    BUG("Received volume %u \"%s\" on non-existent "
                        "device ID %u from MounTA", number, label, device_id);
            }

            if(current_device != nullptr)
            {
                size_t dummy;
                current_device->add_volume(number, label, mountpoint, dummy, true);
            }
        }

        g_variant_unref(tuple);
    }

    return list_changed;
}

bool USB::DeviceList::init_from_mounta()
{
    log_assert(size() == 0);

    GVariant *devices = nullptr;
    GVariant *volumes = nullptr;
    GErrorWrapper error;

    tdbus_moun_ta_call_get_all_sync(dbus_usb_get_mounta_iface(),
                                    &devices, &volumes, nullptr, error.await());
    const bool retval =
        !error.log_failure("Get MounTA info") &&
        fill_list_from_mounta_data(*this, devices, volumes);

    if(devices != nullptr)
        g_variant_unref(devices);

    if(volumes != nullptr)
        g_variant_unref(volumes);

    return retval;
}

bool USB::DeviceList::add_to_list(uint16_t id, const char *name,
                                  const char *usb_port)
{
    for(auto &it : *this)
    {
        if(it.get_specific_data().get_mounta_id() == id)
            return false;
    }

    ListItem_<DeviceItemData> new_device;
    new_device.get_specific_data() = DeviceItemData(id, name, usb_port);
    append_unsorted(std::move(new_device));

    return true;
}

bool USB::DeviceList::remove_from_list(uint16_t id, ID::List &volume_list_id)
{
    ID::Item item;
    auto *dev = get_device_by_id(id, &item);

    if(dev == nullptr)
    {
        BUG("Tried to remove non-existent USB device %u", id);
        return false;
    }

    volume_list_id = FIXME_remove(item);

    return true;
}

static const USB::DeviceItemData *get_device_by_x(
        const USB::DeviceList &list, ID::Item *item,
        const std::function<bool(const ListItem_<USB::DeviceItemData> &a)> &predicate)
{
    auto it = std::find_if(list.begin(), list.end(), predicate);

    if(it != list.end())
    {
        if(item != nullptr)
            *item = ID::Item(it - list.begin());

        return &it->get_specific_data();
    }
    else
        return nullptr;
}

const USB::DeviceItemData *USB::DeviceList::get_device_by_id(uint16_t id, ID::Item *item) const
{
    return get_device_by_x(*this, item,
                           [id](const ListItem_<DeviceItemData> &a) -> bool
                           {
                               return a.get_specific_data().get_mounta_id() == id;
                           });
}

const USB::DeviceItemData *USB::DeviceList::get_device_by_name(const std::string &name,
                                                               ID::Item *item) const
{
    return get_device_by_x(*this, item,
                           [&name](const ListItem_<DeviceItemData> &a) -> bool
                           {
                               return a.get_specific_data().get_name() == name;
                           });
}

void USB::VolumeItemData::get_name(std::string &name) const
{
    const auto list = USB::Helpers::get_list_of_usb_devices();
    log_assert(list != nullptr);

    const auto *dev = list->get_device_by_id(device_id_);
    log_assert(dev != nullptr);

    dev->get_volume_name(number_, name);
}

const std::string &USB::VolumeItemData::get_url() const
{
    const auto list = USB::Helpers::get_list_of_usb_devices();
    log_assert(list != nullptr);

    const auto *dev = list->get_device_by_id(device_id_);
    log_assert(dev != nullptr);

    return dev->get_volume_mountpoint(number_);
}

void USB::VolumeList::enumerate_direct_sublists(const LRU::Cache &cache,
                                                std::vector<ID::List> &nodes) const
{
    for(auto it = begin(); it != end(); ++it)
    {
        auto id = it->get_child_list();

        if(id.is_valid())
            nodes.push_back(id);
    }
}

void USB::VolumeList::obliviate_child(ID::List child_id, const Entry *child)
{
    ID::Item idx;

    if(lookup_item_id_by_child_id(child_id, idx))
        (*this)[idx].obliviate_child();
    else
        BUG("Got obliviate notification for USB volume %u, "
            "but could not find it in volume list (ID %u)",
            child_id.get_raw_id(), get_cache_id().get_raw_id());
}

void USB::DirList::enumerate_direct_sublists(const LRU::Cache &cache,
                                             std::vector<ID::List> &nodes) const
{
    for(auto it = begin(); it != end(); ++it)
    {
        auto id = it->get_child_list();

        if(id.is_valid())
            nodes.push_back(id);
    }
}

void USB::DirList::obliviate_child(ID::List child_id, const Entry *child)
{
    ID::Item idx;

    if(lookup_item_id_by_child_id(child_id, idx))
        (*this)[idx].obliviate_child();
    else
        BUG("Got obliviate notification for child %u, "
            "but could not find it in list (ID %u)",
            child_id.get_raw_id(), get_cache_id().get_raw_id());
}

class CollectNamesData
{
  private:
    std::string base_dir_;
    const size_t base_dir_end_offset_;

  public:
    std::vector<std::string> directories_;
    std::vector<std::string> files_;

    CollectNamesData(const CollectNamesData &) = delete;
    CollectNamesData &operator=(const CollectNamesData &) = delete;

    explicit CollectNamesData(const std::string &base_dir):
        base_dir_end_offset_(base_dir.length() + 1)
    {
        base_dir_.reserve(base_dir_end_offset_ + 128);
        base_dir_ = base_dir;
        base_dir_ += '/';
    }

    void set_file_name(const char *path)
    {
        base_dir_.resize(base_dir_end_offset_);
        base_dir_ += path;
    }

    const char *get_path() const
    {
        return base_dir_.c_str();
    }
};

static int collect_all_names(const char *path, unsigned char dtype,
                             void *user_data)
{
    auto &data = *static_cast<CollectNamesData *>(user_data);

    data.set_file_name(path);

    if(dtype == DT_DIR)
        data.directories_.push_back(path);
    else if(dtype == DT_REG)
        data.files_.push_back(path);
    else
    {
        /* just ignore anything else */
    }

    return 0;
}

bool USB::DirList::fill_from_file_system(const std::string &path)
{
    CollectNamesData directories_and_files(path);

    if(os_foreach_in_path(path.c_str(), collect_all_names, &directories_and_files) < 0)
        return false;

    std::sort(directories_and_files.directories_.begin(),
              directories_and_files.directories_.end());

    std::sort(directories_and_files.files_.begin(),
              directories_and_files.files_.end());

    for(const auto &dir : directories_and_files.directories_)
    {
        ListItem_<ItemData> new_item;
        new_item.get_specific_data() =
            ItemData(dir, ListItemKind(ListItemKind::DIRECTORY));
        append_unsorted(std::move(new_item));
    }

    for(const auto &file : directories_and_files.files_)
    {
        ListItem_<ItemData> new_item;
        new_item.get_specific_data() =
            ItemData(file, ListItemKind(ListItemKind::REGULAR_FILE));
        append_unsorted(std::move(new_item));
    }

    return true;
}

static ID::List attach_new_dirlist(LRU::Cache &cache, ID::List parent_list,
                                   const std::string &path, ListError &error)
{
    ID::List id =
        add_child_list_to_cache<USB::DirList>(cache, parent_list,
                                              LRU::CacheMode::CACHED,
                                              parent_list.get_context(),
                                              USB::DirList::estimate_size_in_bytes());

    if(!id.is_valid())
    {
        error = ListError::INTERNAL;
        return ID::List();
    }

    auto dir = std::static_pointer_cast<USB::DirList>(cache.lookup(id));
    log_assert(dir != nullptr);

    if(!dir->fill_from_file_system(path))
    {
        BUG("LEAKING LIST ID %u after failure to fill list from file system",
            id.get_raw_id());
        error = ListError::PHYSICAL_MEDIA_IO;
        return ID::List();
    }

    return id;
}

namespace USB
{

template<>
ID::List DeviceList::enter_child<VolumeItemData>(LRU::Cache &cache, LRU::CacheModeRequest cmr,
                                                 ID::Item item,
                                                 const std::function<bool()> &may_continue,
                                                 const EnterChild::CheckUseCached &use_cached,
                                                 const EnterChild::DoPurgeList &purge_list,
                                                 ListError &error)
{
    return EnterChild::enter_child_template<DeviceList::ListItemType>(
        this, cache, item, may_continue, use_cached, purge_list, error,
        [this, &cache, &error] (const DeviceList::ListItemType &child_entry)
        {
            const auto &device_data = child_entry.get_specific_data();

            {
                std::string name;
                device_data.get_name(name);
                msg_info("Enter USB device %s", name.c_str());
            }

            // false positive
            // cppcheck-suppress shadowVar
            const auto new_id =
                add_child_list_to_cache<VolumeList>(cache, get_cache_id(),
                        LRU::CacheMode::CACHED,
                        get_cache_id().get_context(),
                        VolumeList::estimate_size_in_bytes());

            if(new_id.is_valid())
            {
                auto volumes = std::static_pointer_cast<USB::VolumeList>(cache.lookup(new_id));
                log_assert(volumes != nullptr);
                device_data.fill_volume_list(*volumes);
            }
            else
                error = ListError::INTERNAL;

            return new_id;
        });
}

template<>
ID::List VolumeList::enter_child<ItemData>(LRU::Cache &cache, LRU::CacheModeRequest cmr,
                                           ID::Item item,
                                           const std::function<bool()> &may_continue,
                                           const EnterChild::CheckUseCached &use_cached,
                                           const EnterChild::DoPurgeList &purge_list,
                                           ListError &error)
{
    return EnterChild::enter_child_template<VolumeList::ListItemType>(
        this, cache, item, may_continue, use_cached, purge_list, error,
        [this, &cache, &error] (const VolumeList::ListItemType &child_entry)
        {
            const auto &volume_data = child_entry.get_specific_data();

            {
                std::string name;
                volume_data.get_name(name);
                msg_info("Enter USB root directory %s", name.c_str());
            }

            return attach_new_dirlist(cache, get_cache_id(),
                                      volume_data.get_url(), error);
        });
}

template<>
ID::List DirList::enter_child<ItemData>(LRU::Cache &cache, LRU::CacheModeRequest cmr,
                                        ID::Item item,
                                        const std::function<bool()> &may_continue,
                                        const EnterChild::CheckUseCached &use_cached,
                                        const EnterChild::DoPurgeList &purge_list,
                                        ListError &error)
{
    return EnterChild::enter_child_template<DirList::ListItemType>(
        this, cache, item, may_continue, use_cached, purge_list, error,
        [this, &cache, &item, &error] (const DirList::ListItemType &child_entry)
        {
            if(!child_entry.get_kind().is_directory())
            {
                error = ListError::INVALID_ID;
                return ID::List();
            }

            // false positive
            // cppcheck-suppress shadowVar
            std::string path;
            if(!USB::Helpers::construct_fspath_to_item(*this, item, path))
            {
                error = ListError::INTERNAL;
                return ID::List();
            }

            msg_info("Enter USB directory \"%s\"", path.c_str());

            return attach_new_dirlist(cache, get_cache_id(), path, error);
        });
}

}

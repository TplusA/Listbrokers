/*
 * Copyright (C) 2015, 2016, 2017, 2018  T+A elektroakustik GmbH & Co. KG
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

#ifndef USB_LIST_HH
#define USB_LIST_HH

#include <string>

#include "lists.hh"
#include "usb_helpers.hh"
#include "enterchild_glue.hh"
#include "i18nstring.hh"

namespace USB
{

class VolumeList;

/*!
 * Data about one USB device exposed over D-Bus by MounTA.
 *
 * \note
 *     This structure is meant to be embedded in a #ListItem_ template class.
 */
class DeviceItemData
{
  private:
    class VolumeInfo
    {
      public:
        uint32_t number_;
        const std::string display_name_utf8_;
        const std::string mountpoint_path_;

        VolumeInfo(const VolumeInfo &) = delete;
        VolumeInfo &operator=(const VolumeInfo &) = delete;

        explicit VolumeInfo(uint32_t number, const char *display_name_utf8,
                            const char *mountpoint_path):
            number_(number),
            display_name_utf8_(display_name_utf8),
            mountpoint_path_(mountpoint_path)
        {}
    };

    uint16_t dev_id_;
    std::string display_name_utf8_;
    std::string usb_port_;

    /* all volumes, sorted by ID */
    std::vector<std::unique_ptr<VolumeInfo>> volumes_;

  public:
    DeviceItemData(const DeviceItemData &) = delete;
    DeviceItemData &operator=(const DeviceItemData &) = delete;
    DeviceItemData(DeviceItemData &&) = default;
    DeviceItemData &operator=(DeviceItemData &&) = default;

    explicit DeviceItemData():
        dev_id_(0)
    {}

    explicit DeviceItemData(uint16_t dev_id,
                            const char *const display_name_utf8,
                            const char *const usb_port):
        dev_id_(dev_id),
        display_name_utf8_(display_name_utf8),
        usb_port_(usb_port)
    {}

    /*!
     * Add volume information to internal array of volumes.
     *
     * The usual lists used by D-Bus clients are generated from information
     * added here when they ask for them. We are doing it this way because we
     * get all device and volume information in a single go from MounTA.
     */
    bool add_volume(uint32_t vol_id, const char *display_name_utf8,
                    const char *mountpoint_path, size_t &added_at_index,
                    bool bug_if_dupe = false);

    const uint16_t &get_mounta_id() const
    {
        return dev_id_;
    }

    const std::string &get_name() const
    {
        return display_name_utf8_;
    }

    void get_name(std::string &name) const
    {
        name = display_name_utf8_;
    }

    // cppcheck-suppress functionStatic
    ListItemKind get_kind() const
    {
        return ListItemKind(ListItemKind::STORAGE_DEVICE);
    }

    /*!
     * Fill cached list of volumes from internal array.
     */
    void fill_volume_list(USB::VolumeList &volumes) const;

    /*!
     * \internal
     * Helper for #USB::VolumeItemData::get_name().
     */
    void get_volume_name(uint32_t volume_number, std::string &name) const
    {
        name = lookup_existing_volume_info(volume_number).display_name_utf8_;
    }

    /*!
     * \internal
     * Helper for #USB::VolumeItemData::get_url().
     */
    const std::string &get_volume_mountpoint(uint32_t volume_number) const
    {
        return lookup_existing_volume_info(volume_number).mountpoint_path_;
    }

  private:
    const VolumeInfo &lookup_existing_volume_info(uint32_t volume_number) const;
};

/*!
 * Data about one USB volume exposed over D-Bus by MounTA.
 *
 * \note
 *     This structure is meant to be embedded in a #ListItem_ template class.
 */
class VolumeItemData
{
  private:
    uint16_t device_id_;
    uint32_t number_;

  public:
    VolumeItemData(VolumeItemData &&) = default;
    VolumeItemData &operator=(VolumeItemData &&) = default;
    VolumeItemData(const VolumeItemData &) = delete;
    VolumeItemData &operator=(const VolumeItemData &) = delete;

    explicit VolumeItemData():
        device_id_(0),
        number_(0)
    {}

    explicit VolumeItemData(uint16_t device_id, uint32_t number):
        device_id_(device_id),
        number_(number)
    {}

    virtual ~VolumeItemData() {}

    void get_name(std::string &name) const;

    // cppcheck-suppress functionStatic
    ListItemKind get_kind() const
    {
        return ListItemKind(ListItemKind::STORAGE_DEVICE);
    }

    const std::string &get_url() const;
};

/*!
 * Data about one directory or file stored on a file system.
 *
 * \note
 *     This structure is meant to be embedded in a #ListItem_ template class.
 */
class ItemData
{
  private:
    std::string display_name_utf8_;
    ListItemKind kind_;

  public:
    ItemData(ItemData &&) = default;
    ItemData &operator=(ItemData &&) = default;
    ItemData(const ItemData &) = delete;
    ItemData &operator=(const ItemData &) = delete;

    explicit ItemData():
        kind_(ListItemKind::OPAQUE)
    {}

    explicit ItemData(const std::string &display_name_utf8, ListItemKind kind):
        display_name_utf8_(display_name_utf8),
        kind_(kind)
    {}

    virtual ~ItemData() {}

    void reset()
    {
        display_name_utf8_.clear();
        kind_ = ListItemKind(ListItemKind::OPAQUE);
    }

    void get_name(std::string &name) const
    {
        name = display_name_utf8_;
    }

    const std::string &get_name() const
    {
        return display_name_utf8_;
    }

    ListItemKind get_kind() const
    {
        return kind_;
    }
};

/*!
 * List of all USB devices connected to the system.
 *
 * This is a plain flat list, storing all the USB mass storage devices in RAM.
 * There is usually only one (or very few) USB device connected, so the
 * #TiledList would be complete overkill.
 */
class DeviceList: public FlatList<DeviceItemData>
{
  public:
    static const I18n::String LIST_TITLE;

    DeviceList(const DeviceList &) = delete;
    DeviceList &operator=(const DeviceList &) = delete;

    explicit DeviceList(std::shared_ptr<Entry> parent):
        FlatList<DeviceItemData>(parent)
    {}

    virtual ~DeviceList() {}

    void enumerate_direct_sublists(const LRU::Cache &cache,
                                   std::vector<ID::List> &nodes) const override;
    void obliviate_child(ID::List child_id, const Entry *child) override;

    template <typename T>
    ID::List enter_child(LRU::Cache &cache, LRU::CacheModeRequest cmr,
                         ID::Item item,
                         const std::function<bool()> &may_continue,
                         const EnterChild::CheckUseCached &use_cached,
                         const EnterChild::DoPurgeList &purge_list,
                         ListError &error);

    bool init_from_mounta();
    bool add_to_list(uint16_t id, const char *name, const char *usb_port);
    bool remove_from_list(uint16_t id, ID::List &volume_list_id);

    const DeviceItemData *get_device_by_id(uint16_t id,
                                           ID::Item *item = nullptr) const;

    DeviceItemData *get_device_by_id(uint16_t id, ID::Item *item = nullptr)
    {
        return const_cast<DeviceItemData *>(const_cast<const DeviceList *>(this)->get_device_by_id(id, item));
    }

    const DeviceItemData *get_device_by_name(const std::string &name,
                                             ID::Item *item = nullptr) const;
};

/*!
 * List of all volumes on a USB device.
 *
 * This is a plain flat list, storing all volumes (partitions) in RAM. There is
 * usually only one (or very few) volume connected, so the #TiledList would be
 * complete overkill.
 */
class VolumeList: public FlatList<VolumeItemData>
{
  public:
    VolumeList(const VolumeList &) = delete;
    VolumeList &operator=(const VolumeList &) = delete;

    explicit VolumeList(std::shared_ptr<Entry> parent):
        FlatList<VolumeItemData>(parent)
    {}

    virtual ~VolumeList() {}

    void enumerate_direct_sublists(const LRU::Cache &cache,
                                   std::vector<ID::List> &nodes) const override;
    void obliviate_child(ID::List child_id, const Entry *child) override;

    template <typename T>
    ID::List enter_child(LRU::Cache &cache, LRU::CacheModeRequest cmr,
                         ID::Item item,
                         const std::function<bool()> &may_continue,
                         const EnterChild::CheckUseCached &use_cached,
                         const EnterChild::DoPurgeList &purge_list,
                         ListError &error);

    /*!
     * Return estimated size of an empty #VolumeList object.
     */
    static constexpr size_t estimate_size_in_bytes() { return sizeof(VolumeList); }
};

/*!
 * Directories on a USB volume.
 *
 * Despite the risk of running into a large directory on a slow USB device
 * (think cheap 2 TiB HDD connected over some slow USB 2.0 bridge, packed with
 * a huge collection of relatively small MP3 files), we use a flat list here
 * and fill it completely in a single go. The reason is that we must sort the
 * directory listings, requiring us to know all entries anyway.
 *
 * Since the operating system is going to buffer directories anyway, we can get
 * away with storing only relatively few directories and purging the cache
 * frequently. Hot lists remain in RAM, sorted, new and rarely used lists are
 * fetched from the OS and sorted again when needed.
 */
class DirList: public FlatList<ItemData>
{
  public:
    DirList(const DirList &) = delete;
    DirList &operator=(const DirList &) = delete;

    explicit DirList(std::shared_ptr<Entry> parent):
        FlatList(parent)
    {}

    virtual ~DirList() {}

    void enumerate_direct_sublists(const LRU::Cache &cache,
                                   std::vector<ID::List> &nodes) const override;
    void obliviate_child(ID::List child_id, const Entry *child) override;

    template <typename T>
    ID::List enter_child(LRU::Cache &cache, LRU::CacheModeRequest cmr,
                         ID::Item item,
                         const std::function<bool()> &may_continue,
                         const EnterChild::CheckUseCached &use_cached,
                         const EnterChild::DoPurgeList &purge_list,
                         ListError &error);

    /*!
     * Return estimated size of an empty #DirList object.
     */
    static constexpr size_t estimate_size_in_bytes() { return sizeof(DirList); }

    bool fill_from_file_system(const std::string &path);
};

}

/*!
 * Traits to be used by \p for_each_item() for a #USB::DeviceList.
 */
template<>
struct ForEachItemTraits<const USB::DeviceList>
{
    using ListType = const USB::DeviceList;
    using ItemType = USB::DeviceItemData;
    using ParentListType = const FlatList<ItemType>;
    using ParentTraits = ForEachItemTraits<ParentListType>;
    using IterType = ParentTraits::IterType;

    static bool warm_up_cache(std::shared_ptr<ListType> list,
                              ID::Item first, ID::Item end)
    {
        return ParentTraits::warm_up_cache(list, first, end);
    }

    static IterType begin(std::shared_ptr<ListType> list, ID::Item first)
    {
        return ParentTraits::begin(list, first);
    }

    static const ListItem_<ItemType> &
    get_next_cached_element(std::shared_ptr<ListType> list, IterType &iter)
    {
        return ParentTraits::get_next_cached_element(list, iter);
    }
};

/*!
 * Traits to be used by \p for_each_item() for a #USB::VolumeList.
 */
template<>
struct ForEachItemTraits<const USB::VolumeList>
{
    using ListType = const USB::VolumeList;
    using ItemType = USB::VolumeItemData;
    using ParentListType = const FlatList<ItemType>;
    using ParentTraits = ForEachItemTraits<ParentListType>;
    using IterType = ParentTraits::IterType;

    static bool warm_up_cache(std::shared_ptr<ListType> list,
                              ID::Item first, ID::Item end)
    {
        return ParentTraits::warm_up_cache(list, first, end);
    }

    static IterType begin(std::shared_ptr<ListType> list, ID::Item first)
    {
        return ParentTraits::begin(list, first);
    }

    static const ListItem_<ItemType> &
    get_next_cached_element(std::shared_ptr<ListType> list, IterType &iter)
    {
        return ParentTraits::get_next_cached_element(list, iter);
    }
};

/*!
 * Traits to be used by \p for_each_item() for a #USB::DirList.
 */
template<>
struct ForEachItemTraits<const USB::DirList>
{
    using ListType = const USB::DirList;
    using ItemType = USB::ItemData;
    using ParentListType = const FlatList<ItemType>;
    using ParentTraits = ForEachItemTraits<ParentListType>;
    using IterType = ParentTraits::IterType;

    static bool warm_up_cache(std::shared_ptr<ListType> list,
                              ID::Item first, ID::Item end)
    {
        return ParentTraits::warm_up_cache(list, first, end);
    }

    static IterType begin(std::shared_ptr<ListType> list, ID::Item first)
    {
        return ParentTraits::begin(list, first);
    }

    static const ListItem_<ItemType> &
    get_next_cached_element(std::shared_ptr<ListType> list, IterType &iter)
    {
        return ParentTraits::get_next_cached_element(list, iter);
    }
};

#endif /* !USB_LIST_HH */

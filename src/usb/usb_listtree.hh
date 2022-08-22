/*
 * Copyright (C) 2015--2019, 2022  T+A elektroakustik GmbH & Co. KG
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

#ifndef USB_LISTTREE_HH
#define USB_LISTTREE_HH

#include <string>

#include "listtree.hh"
#include "listtree_manager.hh"
#include "usb_list.hh"
#include "i18nstring.hh"

namespace USB
{

/*!
 * Tree of lists of USB devices, USB volumes, and file system contents.
 */
class ListTree: public ListTreeIface
{
  private:
    ListTreeManager lt_manager_;

    /*!
     * ID of the list containing all USB mass storage devices known by MounTA.
     */
    ID::List devices_list_id_;

    static constexpr const char CONTEXT_ID[] = "usb";

  public:
    ListTree(const ListTree &) = delete;
    ListTree &operator=(const ListTree &) = delete;

    explicit ListTree(DBusAsync::WorkQueue &navlists_get_range,
                      DBusAsync::WorkQueue &navlists_get_list_id,
                      DBusAsync::WorkQueue &navlists_get_uris,
                      DBusAsync::WorkQueue &navlists_realize_location,
                      LRU::Cache &cache,
                      Cacheable::CheckNoOverrides &cache_check):
        ListTreeIface(navlists_get_range, navlists_get_list_id,
                      navlists_get_uris, navlists_realize_location),
        lt_manager_(cache, cache_check)
    {}

    ~ListTree()
    {
        shutdown_threads();
    }

    void init() override
    {
        devices_list_id_ =
            lt_manager_.allocate_blessed_list<USB::DeviceList>(nullptr, 0, 0, true);
        log_assert(devices_list_id_.is_valid());
    }

    void start_threads(unsigned int number_of_threads, bool synchronous_mode) const override {}
    void shutdown_threads() const override {}

    /*!
     * Called when a list was discarded from cache during garbage collection.
     */
    void list_discarded_from_cache(ID::List id)
    {
        lt_manager_.list_discarded_from_cache(id);
    }

    /*!
     * Reassign ID to device list, emit corresponding D-Bus signal.
     */
    void reinsert_device_list()
    {
        lt_manager_.reinsert_list(devices_list_id_);
    }

    /*!
     * Remove subtree of a USB device (thus, a #USB::VolumeList subtree).
     */
    void purge_device_subtree_and_reinsert_device_list(ID::List volume_list)
    {
        if(volume_list.is_valid())
        {
            log_assert(volume_list.get_raw_id() != devices_list_id_.get_raw_id());
            log_assert(lt_manager_.lookup_list<USB::VolumeList>(volume_list)->get_parent()->get_cache_id().get_raw_id() == devices_list_id_.get_raw_id());

            (void)lt_manager_.purge_subtree(volume_list, ID::List(), nullptr);
        }

        lt_manager_.reinsert_list(devices_list_id_);
    }

    /*!
     * Reassign ID to volume list, emit corresponding D-Bus signal.
     *
     * \param device_id
     *     ID of the USB device as assigned by MounTA.
     *
     * \param volume_number
     *     Number of the volume as reported by MounTA.
     *
     * \param added_at_index
     *     At which index the volume information has been sorted into the
     *     volume list of the #USB::DeviceItemData. This value gets relevant in
     *     case the externally visible #USB::VolumeList has been created
     *     already (UI has accessed it) when adding this new volume.
     */
    void reinsert_volume_list(uint16_t device_id, uint32_t volume_number,
                              size_t added_at_index);

    void pre_main_loop() override;

    bool use_list(ID::List list_id, bool pin_it) override
    {
        return lt_manager_.use_list(list_id, pin_it);
    }

    std::chrono::seconds force_list_into_cache(ID::List list_id, bool force) final override
    {
        return lt_manager_.force_list_into_cache(list_id, force);
    }

    ID::List get_root_list_id() override
    {
        return devices_list_id_;
    }

    I18n::String get_root_list_title() final override
    {
        return DeviceList::LIST_TITLE;
    }

    I18n::String get_child_list_title(ID::List list_id, ID::Item child_item_id) final override;

    ID::List enter_child(ID::List list_id, ID::Item item_id, ListError &error) override;

    ListError for_each(ID::List list_id, ID::Item first, size_t count,
                       const ForEachGenericCallback &callback)
        const override;

    ListError for_each(ID::List list_id, ID::Item first, size_t count,
                       const ForEachDetailedCallback &callback)
        const override;

    void for_each_context(const std::function<void(const char *, const char *, bool)> &callback)
        const override
    {
        callback(CONTEXT_ID, "USB devices", true);
    }

    ssize_t size(ID::List list_id) const override;

    ID::List get_parent_link(ID::List list_id, ID::Item &parent_item_id) const override;

    bool get_parent_link(ID::List list_id, ID::Item &parent_item_id,
                         std::shared_ptr<const LRU::Entry> &parent_list) const;

    ID::List get_link_to_context_root_impl(const char *context_id, ID::Item &item_id,
                                           bool &context_is_known,
                                           bool &context_has_parent) final override;

    ListError get_uris_for_item(ID::List list_id, ID::Item item_id,
                                        std::vector<Url::String> &uris,
                                        ListItemKey &item_key) const override;

    bool can_handle_strbo_url(const std::string &url) const final override;

    ListError realize_strbo_url(const std::string &url,
                                RealizeURLResult &result) final override;

    std::unique_ptr<Url::Location>
    get_location_key(ID::List list_id, ID::RefPos item_pos, bool as_reference_key,
                     ListError &error) const final override;

    std::unique_ptr<Url::Location>
    get_location_trace(ID::List list_id, ID::RefPos item_pos,
                       ID::List ref_list_id, ID::RefPos ref_item_pos,
                       ListError &error) const final override;

    /*!
     * List discard notification.
     *
     * If the list was pinned, then unpin it and pin the device list instead.
     */
    void discard_list_hint(ID::List list_id) override
    {
        if(list_id != devices_list_id_)
            lt_manager_.repin_if_first_is_deepest_pinned_list(list_id, devices_list_id_);
    }

    std::chrono::milliseconds get_gc_expiry_time() const override
    {
        return lt_manager_.get_gc_expiry_time();
    }

    /*!
     * Get pointer to list of USB devices (the root list).
     *
     * \see #USB::Helpers::get_list_of_usb_devices()
     */
    std::shared_ptr<USB::DeviceList> get_list_of_usb_devices()
    {
        return lt_manager_.lookup_list<USB::DeviceList>(get_root_list_id());
    }
};

}

#endif /* !USB_LISTTREE_HH */

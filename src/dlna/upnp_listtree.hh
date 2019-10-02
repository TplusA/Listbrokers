/*
 * Copyright (C) 2015--2019  T+A elektroakustik GmbH & Co. KG
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

#ifndef UPNP_LISTTREE_HH
#define UPNP_LISTTREE_HH

#include "listtree.hh"
#include "listtree_manager.hh"
#include "upnp_list.hh"

namespace UPnP
{

/*!
 * Tree of lists of UPnP servers, UPnP media containers, and media objects.
 */
class ListTree: public ListTreeIface
{
  private:
    ListTreeManager lt_manager_;

    /*!
     * ID of the root list of UPnP servers, always valid.
     */
    ID::List server_list_id_;

    static constexpr const char CONTEXT_ID[] = "upnp";

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
        server_list_id_ =
            lt_manager_.allocate_blessed_list<UPnP::ServerList>(nullptr, 0, 0, true);
        log_assert(server_list_id_.is_valid());
    }

    void pre_main_loop() override
    {
        lt_manager_.announce_root_list(server_list_id_);
    }

    void set_default_lru_cache_mode(LRU::CacheModeRequest req)
    {
        lt_manager_.set_default_lru_cache_mode(req);
    }

    /*!
     * Add UPnP servers in given list to server list.
     *
     * \param list
     *     List of D-Bus object names to add.
     */
    void add_to_server_list(const std::vector<std::string> &list);

    /*!
     * Remove UPnP servers in given list from server list.
     *
     * \param list
     *     List of D-Bus object names to remove.
     */
    void remove_from_server_list(const std::vector<std::string> &list);

    /*!
     * Remove all UPnP servers from the server list.
     */
    void clear();

    /*!
     * Called when a list was discarded from cache during garbage collection.
     */
    void list_discarded_from_cache(ID::List id)
    {
        log_assert(id != server_list_id_);
        lt_manager_.list_discarded_from_cache(id);
    }

    /*!
     * Dump list of all UPnP to log for debugging.
     */
    void dump_server_list();

    /*!
     * Read-only pointer to server list.
     */
    std::shared_ptr<const UPnP::ServerList> get_server_list()
    {
        return lt_manager_.lookup_list<UPnP::ServerList>(server_list_id_);
    }

    /*!
     * Reassign ID to server list, emit corresponding D-Bus signal.
     */
    void reinsert_server_list()
    {
        lt_manager_.reinsert_list(server_list_id_);
    }

    void start_threads(unsigned int number_of_threads,
                       bool synchronous_mode) const override
    {
        UPnP::MediaList::start_threads(number_of_threads, synchronous_mode);
    }

    void shutdown_threads() const override
    {
        UPnP::MediaList::shutdown_threads();
    }

    bool use_list(ID::List list_id, bool pin_it) override
    {
        return lt_manager_.use_list(list_id, pin_it);
    }

    std::chrono::seconds force_list_into_cache(ID::List list_id, bool force)
    {
        return lt_manager_.force_list_into_cache(list_id, force);
    }

    ID::List get_root_list_id() override
    {
        return server_list_id_;
    }

    I18n::String get_root_list_title() final override
    {
        return ServerList::LIST_TITLE;
    }

    I18n::String get_child_list_title(ID::List list_id, ID::Item child_item_id) final override
    {
        if(list_id == server_list_id_)
            return ListTreeManager::get_dynamic_title<UPnP::ServerList>(lt_manager_,
                                                                        list_id, child_item_id);
        else
            return ListTreeManager::get_dynamic_title<UPnP::MediaList>(lt_manager_,
                                                                       list_id, child_item_id);
    }

    ID::List enter_child(ID::List list_id, ID::Item item_id, ListError &error) override;

    ListError for_each(ID::List list, ID::Item first, size_t count,
                       const ForEachGenericCallback &callback)
        const override;

    ListError for_each(ID::List list, ID::Item first, size_t count,
                       const ForEachDetailedCallback &callback)
        const override;

    void for_each_context(const std::function<void(const char *, const char *, bool)> &callback)
        const override
    {
        callback(CONTEXT_ID, "UPnP A/V", true);
    }

    ssize_t size(ID::List list_id) const override;

    ID::List get_parent_link(ID::List list_id, ID::Item &parent_item_id) const override;

    ID::List get_link_to_context_root_impl(const char *context_id, ID::Item &item_id,
                                           bool &context_is_known,
                                           bool &context_has_parent) final override;

    const ListItem_<ServerItemData> *get_server_item(const MediaList &list) const;

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
     * If the list was pinned, then unpin it and pin the server list instead.
     */
    void discard_list_hint(ID::List list_id) override
    {
        if(list_id != server_list_id_)
            lt_manager_.repin_if_first_is_deepest_pinned_list(list_id, server_list_id_);
    }

    std::chrono::milliseconds get_gc_expiry_time() const override
    {
        return lt_manager_.get_gc_expiry_time();
    }
};

}

#endif /* !UPNP_LISTTREE_HH */

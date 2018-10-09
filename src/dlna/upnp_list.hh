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

#ifndef UPNP_LIST_HH
#define UPNP_LIST_HH

#include <string>

#include "lists.hh"
#include "enterchild_template.hh"
#include "upnp_dleynaserver_dbus.h"
#include "dbus_upnp_helpers.hh"
#include "dbus_upnp_list_filler_helpers.hh"
#include "servers_lost_and_found.hh"
#include "urlstring.hh"
#include "i18nstring.hh"

namespace UPnP
{

static constexpr uint16_t server_list_tile_size = 4;
static constexpr uint16_t media_list_tile_size = 8;

class ServerQuirks
{
  public:
    static constexpr uint32_t none                     = 0;
    static constexpr uint32_t album_art_url_not_usable = 1 << 0;

  private:
    uint32_t quirks_;

  public:
    ServerQuirks(const ServerQuirks &) = delete;
    ServerQuirks(ServerQuirks &&) = default;
    ServerQuirks &operator=(const ServerQuirks &) = delete;
    ServerQuirks &operator=(ServerQuirks &&) = default;

    constexpr explicit ServerQuirks(uint32_t quirks = none):
        quirks_(quirks)
    {}

    void add(uint32_t quirks) { quirks_ |= quirks; }

    bool check(const ServerQuirks &quirks) const
    {
        return (quirks_ & quirks.quirks_) != 0;
    }
};

/*!
 * Data about one UPnP media server exposed over D-Bus by dLeyna.
 *
 * \note
 *     This structure is meant to be embedded in a #ListItem_ template class.
 */
class ServerItemData
{
  private:
    /*!
     * D-Bus proxy to the UPnP server object as exposed by dLeyna.
     */
    tdbusdleynaserverMediaDevice *dbus_proxy_;

    /*!
     * Bugs everywhere.
     */
    ServerQuirks server_quirks_;

  public:
    ServerItemData(const ServerItemData &src) = delete;
    ServerItemData &operator=(const ServerItemData &src) = delete;

    ServerItemData(ServerItemData &&src)
    {
        dbus_proxy_ = src.dbus_proxy_;
        src.dbus_proxy_ = nullptr;

        server_quirks_ = std::move(src.server_quirks_);
    }

    ServerItemData &operator=(ServerItemData &&src)
    {
        if(dbus_proxy_ != nullptr)
            object_unref(dbus_proxy_);

        dbus_proxy_ = src.dbus_proxy_;
        src.dbus_proxy_ = nullptr;

        server_quirks_ = std::move(src.server_quirks_);

        return *this;
    }

    explicit ServerItemData():
        dbus_proxy_(nullptr)
    {}

    ~ServerItemData();

    /*!
     * Extra init function in addition to ctor.
     *
     * We cannot use a parametrized ctor because objects of this class are
     * embedded in templated classes. Their ctors do not know and must not know
     * about the specifics about this class, so type-specific code needs to
     * call this init function at some point.
     */
    void init(tdbusdleynaserverMediaDevice *dbus_proxy);

  public:
    void get_name(std::string &name) const;

    static ListItemKind get_kind()
    {
        return ListItemKind(ListItemKind::SERVER);
    }

    std::string get_dbus_path_copy() const
    {
        return UPnP::get_proxy_object_path(dbus_proxy_);
    }

    /*!\internal
     * For D-Bus/UPnP code only.
     */
    tdbusdleynaserverMediaDevice *get_dbus_proxy()
    {
        return dbus_proxy_;
    }

    /*!\internal
     * For D-Bus/UPnP code only.
     */
    void replace_dbus_proxy(tdbusdleynaserverMediaDevice *proxy)
    {
        UPnP::ServerItemData::object_ref(proxy);

        if(dbus_proxy_ != nullptr)
            UPnP::ServerItemData::object_unref(dbus_proxy_);

        dbus_proxy_ = proxy;
    }

    bool has_quirks(const ServerQuirks &quirks) const
    {
        return server_quirks_.check(quirks);
    }

    /*!\internal
     * Enable mocking away \c g_object_ref().
     */
    static gpointer (*object_ref)(gpointer);

    /*!\internal
     * Enable mocking away \c g_object_unref().
     */
    static void (*object_unref)(gpointer);
};

/*!
 * Data about one UPnP container or media object exposed over D-Bus by dLeyna.
 *
 * \note
 *     This structure is meant to be embedded in a #ListItem_ template class.
 */
class ItemData
{
  public:
    ItemData(ItemData &&) = default;
    ItemData &operator=(ItemData &&) = default;
    ItemData(const ItemData &) = delete;
    ItemData &operator=(const ItemData &) = delete;

    explicit ItemData():
        album_art_url_(Url::Sensitivity::GENERIC),
        kind_(ListItemKind::OPAQUE)
    {}

    explicit ItemData(std::string &&dbus_path,
                      std::string &&display_name_utf8,
                      Url::String &&album_art_url,
                      bool is_container):
        dbus_path_(dbus_path),
        display_name_utf8_(display_name_utf8),
        album_art_url_(album_art_url),
        kind_(is_container ? ListItemKind::DIRECTORY : ListItemKind::REGULAR_FILE)
    {}

  private:
    /*!
     * Name of this object on D-Bus.
     */
    std::string dbus_path_;

    /*!
     * Name of the object as to be represented to the user.
     */
    std::string display_name_utf8_;

    /*!
     * URL of album art, if any.
     */
    Url::String album_art_url_;

    /*! Item is either a container (directory) or an object (file/stream). */
    ListItemKind kind_;

  public:
    void reset()
    {
        dbus_path_.clear();
        display_name_utf8_.clear();
        album_art_url_.clear();
        kind_ = ListItemKind(ListItemKind::OPAQUE);
    }

    void get_name(std::string &name) const
    {
        name = display_name_utf8_;
    }

    ListItemKind get_kind() const
    {
        return kind_;
    }

    const std::string &get_dbus_path() const
    {
        return dbus_path_;
    }

    std::string get_dbus_path_copy() const
    {
        return dbus_path_;
    }

    const Url::String &get_album_art_url() const
    {
        return album_art_url_;
    }
};

/*!
 * List of media containers and items on a UPnP server.
 *
 * The directory hierarchy on a UPnP server is resembled using a hierarchy of
 * these lists. Since these lists may be huge and fetching their contents over
 * the network is usually (very) slow, we are using a tiled list.
 */
class MediaList: public TiledList<ItemData, media_list_tile_size>
{
  public:
    enum class Type
    {
        SUBDIRECTORY,
        AUDIO,
        MISC,
    };

    MediaList(const MediaList &) = delete;
    MediaList &operator=(const MediaList &) = delete;

    explicit MediaList(std::shared_ptr<Entry> parent,
                       size_t number_of_entries,
                       const TiledListFillerIface<ItemData> &filler):
        TiledList(parent, number_of_entries, filler)
    {}

    virtual ~MediaList() {}

    void enumerate_direct_sublists(const LRU::Cache &cache,
                                   std::vector<ID::List> &nodes) const override;
    void obliviate_child(ID::List child_id, const Entry *child) override;

    template <typename T>
    ID::List enter_child(LRU::Cache &cache, LRU::CacheModeRequest cmr,
                         ID::Item item,
                         const TiledListFillerIface<T> &filler,
                         const std::function<bool()> &may_continue,
                         const EnterChild::CheckUseCached &use_cached,
                         const EnterChild::DoPurgeList &purge_list,
                         ListError &error)
    {
        return EnterChild::enter_child_template<ListItemType>(
            this, cache, item, may_continue, use_cached, purge_list, error,
            [this, &cache, &cmr, &filler] (const ListItemType &child_entry)
            {
                const std::string &name = child_entry.get_specific_data().get_dbus_path();

                msg_vinfo(MESSAGE_LEVEL_DIAG,
                        "D-Bus path of new list is %s", name.c_str());

                return add_child_list_to_cache<UPnP::MediaList, T>(
                            cache, get_cache_id(), LRU::to_cache_mode(cmr),
                            get_cache_id().get_context(),
                            UPnP::get_size_of_container(name),
                            UPnP::MediaList::estimate_size_in_bytes(),
                            filler);
            });
    }

    template <typename T>
    ID::List enter_child(LRU::Cache &cache, LRU::CacheModeRequest cmr,
                         ID::Item item,
                         const std::function<bool()> &may_continue,
                         const EnterChild::CheckUseCached &use_cached,
                         const EnterChild::DoPurgeList &purge_list,
                         ListError &error)
    {
        return enter_child<T>(cache, cmr, item, filler_,
                              may_continue, use_cached, purge_list, error);
    }

    /*!
     * Return estimated size of an empty #MediaList object.
     */
    static constexpr size_t estimate_size_in_bytes() { return sizeof(MediaList); }

    std::string get_dbus_object_path() const;
};

/*!
 * List of all UPnP servers on the network.
 *
 * This is a plain flat list, storing all the UPnP servers in RAM. There are
 * usually only few (or only one) UPnP servers on a network, so very likely we
 * don't need a #TiledList here.
 */
class ServerList: public FlatList<ServerItemData>
{
  public:
    enum class RemoveFromListResult
    {
        REMOVED,
        NOT_ADDED_YET,
        NOT_FOUND,
    };

    static const I18n::String LIST_TITLE;

    ServersLostAndFound servers_lost_and_found_;

    ServerList(const ServerList &) = delete;
    ServerList &operator=(const ServerList &) = delete;

    explicit ServerList(std::shared_ptr<Entry> parent):
        FlatList<ServerItemData>(parent)
    {}

    virtual ~ServerList() {}

    void enumerate_tree_of_sublists(const LRU::Cache &cache,
                                    std::vector<ID::List> &nodes,
                                    bool append_to_nodes = false) const override;
    void enumerate_direct_sublists(const LRU::Cache &cache,
                                   std::vector<ID::List> &nodes) const override;
    void obliviate_child(ID::List child_id, const Entry *child) override;

    void add_to_list(const std::string &object_path,
                     std::function<void()> &&notify_server_added);
    RemoveFromListResult remove_from_list(const std::string &object_path,
                                          ID::List &child_list_id);

    template <typename T>
    ID::List enter_child(LRU::Cache &cache, LRU::CacheModeRequest cmr,
                         ID::Item item,
                         const TiledListFillerIface<T> &filler,
                         const std::function<bool()> &may_continue,
                         const EnterChild::CheckUseCached &use_cached,
                         const EnterChild::DoPurgeList &purge_list,
                         ListError &error)
    {
        return EnterChild::enter_child_template<ListItemType>(
            this, cache, item, may_continue, use_cached, purge_list, error,
            [this, &cache, &cmr, &filler] (const ListItemType &child_entry)
            {
                const std::string name(child_entry.get_specific_data().get_dbus_path_copy());

                return
                    add_child_list_to_cache<UPnP::MediaList, T>(
                            cache, get_cache_id(), LRU::to_cache_mode(cmr),
                            get_cache_id().get_context(),
                            UPnP::get_size_of_container(name),
                            UPnP::MediaList::estimate_size_in_bytes(),
                            filler);
            });
    }

    template <typename T>
    ID::List enter_child(LRU::Cache &cache, LRU::CacheModeRequest cmr,
                         ID::Item item,
                         const std::function<bool()> &may_continue,
                         const EnterChild::CheckUseCached &use_cached,
                         const EnterChild::DoPurgeList &purge_list,
                         ListError &error)
    {
        return enter_child(cache, cmr, item,
                           UPnP::get_tiled_list_filler_for_root_directory<T>(),
                           may_continue, use_cached, purge_list, error);
    }

  private:
    static void media_device_proxy_connected(GObject *source_object,
                                             GAsyncResult *res,
                                             gpointer user_data);
};

}

/*!
 * Traits to be used by \p for_each_item() for a #UPnP::MediaList.
 */
template<>
struct ForEachItemTraits<const UPnP::MediaList>
{
    using ListType = const UPnP::MediaList;
    using ItemType = UPnP::ItemData;
    using ParentListType = const TiledList<ItemType, UPnP::media_list_tile_size>;
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
 * Traits to be used by \p for_each_item() for a #UPnP::ServerList.
 */
template<>
struct ForEachItemTraits<const UPnP::ServerList>
{
    using ListType = const UPnP::ServerList;
    using ItemType = UPnP::ServerItemData;
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

#endif /* !UPNP_LIST_HH */

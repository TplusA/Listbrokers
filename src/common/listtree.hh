/*
 * Copyright (C) 2015--2017, 2019, 2020  T+A elektroakustik GmbH & Co. KG
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

#ifndef LISTTREE_HH
#define LISTTREE_HH

#include "idtypes.hh"
#include "dbus_async_workqueue.hh"
#include "ranked_stream_links.hh"
#include "strbo_url.hh"
#include "i18nstring.hh"
#include "md5.hh"
#include "de_tahifi_lists_errors.hh"
#include "de_tahifi_lists_item_kinds.hh"

#include <vector>
#include <atomic>

class ListItemKey
{
  private:
    MD5::Hash item_key_;
    bool is_valid_;

  public:
    ListItemKey(ListItemKey &&) = default;
    ListItemKey &operator=(ListItemKey &&) = default;

    explicit ListItemKey(): is_valid_(false) {}

    bool is_valid() const { return is_valid_; }
    const MD5::Hash &get() const { return item_key_; }

    MD5::Hash &get_for_setting()
    {
        is_valid_ = true;
        return item_key_;
    }
};

/*!
 * Interface for managing trees of lists.
 *
 * The defined interface is rather broad and high level. It serves as a glue
 * layer between generic list broker code and specific underlying data sources.
 */
class ListTreeIface
{
  public:
    class RealizeURLResult
    {
      public:
        ID::List list_id;
        ID::Item item_id;
        ListItemKind item_kind;
        ID::List ref_list_id;
        ID::Item ref_item_id;
        size_t distance;
        size_t trace_length;
        I18n::String list_title;

        RealizeURLResult(RealizeURLResult &&) = default;
        RealizeURLResult &operator=(RealizeURLResult &&) = default;

        explicit RealizeURLResult():
            item_id(UINT32_MAX),
            item_kind(ListItemKind::LOGOUT_LINK),
            distance(0),
            trace_length(0),
            list_title(false)
        {}

        void set_item_data(ID::List id, ID::Item idx, ListItemKind kind)
        {
            list_id = id;
            item_id = idx;
            item_kind = kind;
        }
    };

    /* GetRange(), GetRangeWithMetaData() */
    DBusAsync::WorkQueue &q_navlists_get_range_;

    /* GetListId(), GetParameterizedListId() */
    DBusAsync::WorkQueue &q_navlists_get_list_id_;

    /* GetURIs(), GetRankedStreamLinks() */
    DBusAsync::WorkQueue &q_navlists_get_uris_;

    /* GetLocationTrace(), RealizeLocation() */
    DBusAsync::WorkQueue &q_navlists_realize_location_;

  private:
    static const std::string empty_string;

  protected:
    std::atomic_uint cancel_blocking_operation_counter;
    const std::function<bool(void)> may_continue_fn_;

    explicit ListTreeIface(DBusAsync::WorkQueue &navlists_get_range,
                           DBusAsync::WorkQueue &navlists_get_list_id,
                           DBusAsync::WorkQueue &navlists_get_uris,
                           DBusAsync::WorkQueue &navlists_realize_location):
        q_navlists_get_range_(navlists_get_range),
        q_navlists_get_list_id_(navlists_get_list_id),
        q_navlists_get_uris_(navlists_get_uris),
        q_navlists_realize_location_(navlists_realize_location),
        cancel_blocking_operation_counter(0),
        may_continue_fn_([this] { return this->is_blocking_operation_allowed(); })
    {}

  public:
    ListTreeIface(const ListTreeIface &) = delete;
    ListTreeIface &operator=(const ListTreeIface &) = delete;

    /*!
     * Virtual destructor.
     *
     * \attention
     *     Classes that implement the #ListTreeIface must call
     *     #ListTreeIface::shutdown_threads() in their destructor. We cannot do
     *     it from here because that function is pure virtual.
     */
    virtual ~ListTreeIface() {}

    /*!
     * Initialize object further after basic construction.
     */
    virtual void init() = 0;

    /*!
     * Start networking threads.
     */
    virtual void start_threads(unsigned int number_of_threads,
                               bool synchronous_mode = false) const = 0;

    /*!
     * Stop networking threads.
     */
    virtual void shutdown_threads() const = 0;

    /*!
     * Called directly before entering the main loop.
     *
     * Last chance to get anything initialized before running the event-driven
     * main loop.
     */
    virtual void pre_main_loop() = 0;

    /*!
     * Use list so that its age drops to 0, possibly pin it.
     *
     * This is just a wrapper around #LRU::Cache::use(), but a list tree and
     * the #ListTreeManager usually need to keep track of object use.
     */
    virtual bool use_list(ID::List list_id, bool pin_it) = 0;

    /*!
     * Request given list ID to remain in cache.
     *
     * \returns
     *     The amount of time the normally non-cacheable list is kept in cache
     *     before reverting to normal behavior.
     */
    virtual std::chrono::seconds force_list_into_cache(ID::List list_id, bool force) = 0;

    /*!
     * Get cache ID of root list, if any.
     */
    virtual ID::List get_root_list_id() = 0;

    /*!
     * Get cache ID of root list, if any.
     */
    virtual I18n::String get_root_list_title() = 0;

    /*!
     * Figure out title of a list's child list.
     */
    virtual I18n::String get_child_list_title(ID::List list_id, ID::Item child_item_id) = 0;

    /*!
     * Materialize child list of given item in given list.
     */
    virtual ID::List enter_child(ID::List list_id, ID::Item item_id, ListError &error) = 0;

    /*!
     * Materialize parameterized child list.
     *
     * FIXME: This is an ad-hoc implementation that supports exactly one
     *        string-typed parameter. This is not going to be enough.
     */
    virtual ID::List enter_child_with_parameters(ID::List list_id,
                                                 ID::Item item_id,
                                                 const char *parameter,
                                                 ListError &error)
    {
        error = ListError::NOT_SUPPORTED;
        return ID::List();
    }

    /*!
     * Figure out list title given only the list ID.
     *
     * This function queries the parent list for title. If possible, it is
     * preferrable to call #ListTreeIface::get_child_list_title() on the parent
     * list so that the parent lookup can be omitted.
     */
    I18n::String get_list_title(ID::List list_id)
    {
        if(list_id == get_root_list_id())
            return get_root_list_title();

        ID::Item item_id;
        const ID::List parent_list_id(get_parent_link(list_id, item_id));

        return get_child_list_title(parent_list_id, item_id);
    }

    class ForEachItemData
    {
      public:
        ForEachItemData(const ForEachItemData &) = delete;
        ForEachItemData &operator=(const ForEachItemData &) = delete;

      protected:
        explicit ForEachItemData(ListItemKind kind):
            kind_(kind)
        {}

      public:
        virtual ~ForEachItemData() {}

        ListItemKind kind_;
    };

    /*!
     * For simple range queries.
     */
    class ForEachItemDataGeneric: public ForEachItemData
    {
      public:
        ForEachItemDataGeneric(const ForEachItemDataGeneric &) = delete;
        ForEachItemDataGeneric &operator=(const ForEachItemDataGeneric &) = delete;

        explicit ForEachItemDataGeneric(ListItemKind kind):
            ForEachItemData(kind)
        {}

        virtual ~ForEachItemDataGeneric() {}

        std::string name_;
    };

    /*!
     * For range queries including meta data.
     */
    class ForEachItemDataDetailed: public ForEachItemData
    {
      public:
        ForEachItemDataDetailed(const ForEachItemDataDetailed &) = delete;
        ForEachItemDataDetailed &operator=(const ForEachItemDataDetailed &) = delete;

        explicit ForEachItemDataDetailed(const std::string &title,
                                         ListItemKind kind):
            ForEachItemData(kind),
            artist_(::ListTreeIface::empty_string),
            album_(::ListTreeIface::empty_string),
            title_(title),
            primary_string_index_(2)
        {}

        explicit ForEachItemDataDetailed(const std::string &artist,
                                         const std::string &album,
                                         const std::string &title,
                                         uint8_t idx, ListItemKind kind):
            ForEachItemData(kind),
            artist_(artist),
            album_(album),
            title_(title),
            primary_string_index_(idx)
        {}

        virtual ~ForEachItemDataDetailed() {}

        const std::string &artist_;
        const std::string &album_;
        const std::string &title_;
        const uint8_t primary_string_index_;
    };

    using ForEachGenericCallback = std::function<bool(const ListTreeIface::ForEachItemDataGeneric &)>;
    using ForEachDetailedCallback = std::function<bool(const ListTreeIface::ForEachItemDataDetailed &)>;

    /*!
     * Iterate over a range of list items, generic version.
     */
    virtual ListError for_each(ID::List list_id, ID::Item first, size_t count,
                               const ForEachGenericCallback &callback)
        const = 0;

    /*!
     * Iterate over a range of list items, version with more details.
     */
    virtual ListError for_each(ID::List list_id, ID::Item first, size_t count,
                               const ForEachDetailedCallback &callback)
        const = 0;

    virtual void
    for_each_context(const std::function<void(const char *, const char *, bool)> &callback)
        const = 0;

    /*!
     * Get number of items in list.
     *
     * \returns
     *     Number of items, or -1 on error.
     */
    virtual ssize_t size(ID::List list_id) const = 0;

    /*!
     * Get item ID and parent list ID of given list.
     *
     * Basically, this function returns the coordinates of the item that links
     * to the list with ID \p list_id.
     *
     * \param list_id
     *     The list whose parent link should be retrieved.
     *
     * \param[out] parent_item_id
     *     The ID of the list item linking to \p list_id in the parent list is
     *     returned through this argument. This value remains untouched in case
     *     this function returns the invalid list ID or \p list_id itself.
     *
     * \returns
     *     The list ID of the parent list, if any. For the root list,
     *     \p list_id is returned. In case of any error, the invalid ID is
     *     returned.
     */
    virtual ID::List get_parent_link(ID::List list_id, ID::Item &parent_item_id) const = 0;

    /*!
     * Get item ID and parent list ID of list associated with given context.
     */
    ID::List get_link_to_context_root(const char *context_id, ID::Item &item_id,
                                      bool &context_is_known,
                                      bool &context_has_parent)
    {
        context_is_known = false;
        context_has_parent = false;

        if(context_id == nullptr || context_id[0] == '\0')
            return ID::List();

        return get_link_to_context_root_impl(context_id, item_id,
                                             context_is_known, context_has_parent);
    }

    /*!
     * Get list of stream URIs associated with the given item.
     */
    virtual ListError get_uris_for_item(ID::List list_id, ID::Item item_id,
                                        std::vector<Url::String> &uris,
                                        ListItemKey &item_key) const = 0;

    /*!
     * Get list of ranked stream links associated with the given item.
     */
    virtual ListError get_ranked_links_for_item(ID::List list_id, ID::Item item_id,
                                                std::vector<Url::RankedStreamLinks> &links,
                                                ListItemKey &item_key) const
    {
        links.clear();
        return ListError(ListError::NOT_SUPPORTED);
    }

    /*!
     * Check if given URL can be processed by looking at the URL scheme name.
     */
    virtual bool can_handle_strbo_url(const std::string &url) const = 0;

    /*!
     * Locate object specified by given URL.
     */
    virtual ListError realize_strbo_url(const std::string &url,
                                        RealizeURLResult &result) = 0;

    /*!
     * Generate location key URL from given position.
     */
    virtual std::unique_ptr<Url::Location>
    get_location_key(ID::List list_id, ID::RefPos item_pos,
                     bool as_reference_key, ListError &error) const = 0;

    /*!
     * Generate location trace URL to given coordinates.
     */
    virtual std::unique_ptr<Url::Location>
    get_location_trace(ID::List list_id, ID::RefPos item_pos,
                       ID::List ref_list_id, ID::RefPos ref_item_pos,
                       ListError &error) const = 0;

    /*!
     * Got notification over D-Bus that given list is not going to be used
     * anymore.
     *
     * This is just a hint. The list tree implementation may safely choose to
     * ignore it.
     */
    virtual void discard_list_hint(ID::List list_id) = 0;

    /*!
     * Garbage collection object expiry time to support proper use of
     * keep-alive.
     */
    virtual std::chrono::milliseconds get_gc_expiry_time() const = 0;

    void push_cancel_blocking_operation()
    {
        cancel_blocking_operation_counter.fetch_add(1);
    }

    void pop_cancel_blocking_operation()
    {
        cancel_blocking_operation_counter.fetch_sub(1);
    }

    /*!
     * Support cancelation of work items which download something.
     *
     * While downloading something via libcurl, the periodiclly called progress
     * callback calls this function to determine whether or not to continue
     * with an download.
     *
     * \see
     *     #ListTreeIface::push_cancel_blocking_operation(),
     *     #ListTreeIface::pop_cancel_blocking_operation()
     */
    bool is_blocking_operation_allowed() const
    {
        return cancel_blocking_operation_counter.load() == 0;
    }

  protected:
    /*!
     * Specialized implementation of #ListTreeIface::get_link_to_context_root().
     *
     * Contract: The \p context_id is guaranteed to be a non-null pointer to a
     *           non-empty string.
     * Contract: The \p item_id parameter is set to 0.
     * Contract: Both, \p context_is_known and \c context_has_parent, are
     *           preset to \c false value.
     */
    virtual ID::List get_link_to_context_root_impl(const char *context_id, ID::Item &item_id,
                                                   bool &context_is_known,
                                                   bool &context_has_parent) = 0;
};

#endif /* !LISTTREE_HH */

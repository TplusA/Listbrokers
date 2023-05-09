/*
 * Copyright (C) 2015--2020, 2023  T+A elektroakustik GmbH & Co. KG
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

#include <cppcutter.h>

#include "mock_messages.hh"
#include "mock_backtrace.hh"
#include "mock_dbus_upnp_helpers.hh"
#include "mock_dbus_lists_iface.hh"
#include "mock_listbrokers_dbus.hh"
#include "mock_upnp_dleynaserver_dbus.hh"
#include "mock_timebase.hh"
#include "fake_dbus.hh"

#include "upnp_listtree.hh"

/*!
 * \addtogroup upnp_list_tree_tests Unit tests
 * \ingroup upnp
 *
 * UPnP tree of cached lists unit tests.
 */
/*!@{*/

static MockTimebase mock_timebase;
Timebase *LRU::timebase = &mock_timebase;

const TiledListFillerIface<UPnP::ItemData> *current_tiled_list_filler;

constexpr const char *ListError::names_[];

const std::string ListTreeIface::empty_string;

namespace UPnP
{
    template <>
    const TiledListFillerIface<ItemData> &get_tiled_list_filler_for_root_directory()
    {
        cppcut_assert_not_null(current_tiled_list_filler);
        return *current_tiled_list_filler;
    }
};

namespace upnp_listtree_tests
{

static MockMessages *mock_messages;
static MockBacktrace *mock_backtrace;
static MockDBusUPnPHelpers *mock_dbus_upnp_helpers;
static MockDBusListsIface *mock_dbus_lists_iface;
static MockListbrokersDBus *mock_listbrokers_dbus;
static MockDleynaServerDBus *mock_dleynaserver_dbus;

static LRU::Cache *cache;
static constexpr size_t maximum_number_of_objects = 1000;

static UPnP::ListTree *list_tree;
static std::unique_ptr<Cacheable::CheckNoOverrides> cacheable_check;

static tdbuslistsNavigation *const dbus_lists_navigation_iface_dummy =
    reinterpret_cast<tdbuslistsNavigation *>(0x24681357);

static void add_servers(const std::vector<std::string> &servers, ID::List old_server_list_id)
{
    for(size_t i = 0; i < servers.size(); ++i)
    {
        /* these three are called by the UPnP::ServerList async callback */
        mock_dbus_upnp_helpers->expect_create_media_device_proxy_for_object_path_begin_callback(FakeDBus::create_proxy_begin);
        mock_dbus_upnp_helpers->expect_proxy_object_path_equals_callback(FakeDBus::compare_proxy_object_path, list_tree->get_server_list()->size() + i);
        mock_dbus_upnp_helpers->expect_is_media_device_usable(true,
            reinterpret_cast<tdbusdleynaserverMediaDevice *>(FakeDBus::next_proxy_id + i));

        /* these two are called by the UPnP::ListTree callback */
        mock_dbus_lists_iface->expect_dbus_lists_get_navigation_iface(dbus_lists_navigation_iface_dummy);
        mock_listbrokers_dbus->expect_tdbus_lists_navigation_emit_list_invalidate(dbus_lists_navigation_iface_dummy, old_server_list_id.get_raw_id() + i, old_server_list_id.get_raw_id() + 1 + i);

        /* this is again from the UPnP::ServerList async callback */
        mock_dleynaserver_dbus->expect_tdbus_dleynaserver_media_device_get_model_name(
            "",
            reinterpret_cast<tdbusdleynaserverMediaDevice *>(FakeDBus::next_proxy_id + i));
    }

    list_tree->add_to_server_list(servers);
}

void cut_setup()
{
    current_tiled_list_filler = nullptr;

    FakeDBus::next_proxy_id = 100;

    ::UPnP::ServerItemData::object_ref = FakeDBus::ref_proxy;
    ::UPnP::ServerItemData::object_unref = FakeDBus::unref_proxy;

    mock_messages = new MockMessages;
    cppcut_assert_not_null(mock_messages);
    mock_messages->init();
    mock_messages_singleton = mock_messages;

    mock_backtrace = new MockBacktrace;
    cppcut_assert_not_null(mock_backtrace);
    mock_backtrace->init();
    mock_backtrace_singleton = mock_backtrace;

    mock_dbus_upnp_helpers = new MockDBusUPnPHelpers;
    cppcut_assert_not_null(mock_dbus_upnp_helpers);
    mock_dbus_upnp_helpers->init();
    mock_dbus_upnp_helpers_singleton = mock_dbus_upnp_helpers;

    mock_dbus_lists_iface = new MockDBusListsIface;
    cppcut_assert_not_null(mock_dbus_lists_iface);
    mock_dbus_lists_iface->init();
    mock_dbus_lists_iface_singleton = mock_dbus_lists_iface;

    mock_listbrokers_dbus = new MockListbrokersDBus;
    cppcut_assert_not_null(mock_listbrokers_dbus);
    mock_listbrokers_dbus->init();
    mock_listbrokers_dbus_singleton = mock_listbrokers_dbus;

    mock_dleynaserver_dbus = new MockDleynaServerDBus;
    cppcut_assert_not_null(mock_dleynaserver_dbus);
    mock_dleynaserver_dbus->init();
    mock_dleynaserver_dbus_singleton = mock_dleynaserver_dbus;

    mock_messages->ignore_messages_with_level_or_above(MESSAGE_LEVEL_TRACE);

    mock_timebase.reset();

    cache = new LRU::Cache(100000, maximum_number_of_objects,
                           std::chrono::minutes(1));
    cppcut_assert_not_null(cache);
    cache->set_callbacks([]{}, []{}, [] (ID::List id) {}, []{});
    cppcut_assert_equal(size_t(0), cache->count());

    cacheable_check = std::make_unique<Cacheable::CheckNoOverrides>();
    cppcut_assert_not_null(cacheable_check.get());

    static DBusAsync::WorkQueue q(DBusAsync::WorkQueue::Mode::SYNCHRONOUS);
    list_tree = new UPnP::ListTree(q, q, q, q, *cache, std::move(cacheable_check));
    cppcut_assert_not_null(list_tree);
    list_tree->init();
    list_tree->start_threads(1, true);

    static const std::vector<std::string> servers = {std::string("/test/server/0")};
    add_servers(servers, ID::List(1));
    cppcut_assert_equal(size_t(1), list_tree->get_server_list()->size());
}

void cut_teardown()
{
    delete list_tree;
    list_tree = nullptr;

    cacheable_check = nullptr;

    delete cache;
    cache = nullptr;

    mock_dbus_upnp_helpers->check();
    mock_dbus_upnp_helpers_singleton = nullptr;
    delete mock_dbus_upnp_helpers;
    mock_dbus_upnp_helpers = nullptr;

    mock_backtrace->check();
    mock_backtrace_singleton = nullptr;
    delete mock_backtrace;
    mock_backtrace = nullptr;

    mock_messages->check();
    mock_messages_singleton = nullptr;
    delete mock_messages;
    mock_messages = nullptr;

    mock_dbus_lists_iface->check();
    mock_dbus_lists_iface_singleton = nullptr;
    delete mock_dbus_lists_iface;
    mock_dbus_lists_iface = nullptr;

    mock_listbrokers_dbus->check();
    mock_listbrokers_dbus_singleton = nullptr;
    delete mock_listbrokers_dbus;
    mock_listbrokers_dbus = nullptr;

    mock_dleynaserver_dbus->check();
    mock_dleynaserver_dbus_singleton = nullptr;
    delete mock_dleynaserver_dbus;
    mock_dleynaserver_dbus = nullptr;

    FakeDBus::all_proxies.clear();
    FakeDBus::proxy_extra_refs.clear();
}

static void check_server_list_id(ID::List expected_id, size_t expected_size)
{
    auto list = list_tree->get_server_list();
    cppcut_assert_not_null(list.get());
    cppcut_assert_equal(list_tree->get_root_list_id().get_raw_id(),
                        list->get_cache_id().get_raw_id());
    cppcut_assert_equal(expected_id.get_raw_id(), list->get_cache_id().get_raw_id());
    cppcut_assert_equal(expected_size, list_tree->get_server_list()->size());
}

/*!\test
 * Server list should be easy to guess after test setup.
 */
void test_expected_server_list_after_first_insertion()
{
    check_server_list_id(ID::List(2), 1);
}

/*!\test
 * Insertion of one more UPnP server into server list.
 */
void test_insert_second_server()
{
    static const std::vector<std::string> servers = {std::string("/test/server/10")};
    mock_dbus_upnp_helpers->expect_create_media_device_proxy_for_object_path_begin_callback(FakeDBus::create_proxy_begin);
    mock_dbus_upnp_helpers->expect_proxy_object_path_equals_callback(FakeDBus::compare_proxy_object_path, 1);
    mock_dbus_upnp_helpers->expect_is_media_device_usable(true,
        reinterpret_cast<tdbusdleynaserverMediaDevice *>(FakeDBus::next_proxy_id));
    mock_dleynaserver_dbus->expect_tdbus_dleynaserver_media_device_get_model_name(
        "Server number 10",
        reinterpret_cast<tdbusdleynaserverMediaDevice *>(FakeDBus::next_proxy_id));
    mock_dbus_lists_iface->expect_dbus_lists_get_navigation_iface(dbus_lists_navigation_iface_dummy);
    mock_listbrokers_dbus->expect_tdbus_lists_navigation_emit_list_invalidate(dbus_lists_navigation_iface_dummy, 2, 3);
    list_tree->add_to_server_list(servers);
    cppcut_assert_equal(size_t(2), list_tree->get_server_list()->size());

    check_server_list_id(ID::List(3), 2);
}

class UnusedFiller: public TiledListFillerIface<UPnP::ItemData>
{
  public:
    UnusedFiller(const UnusedFiller &) = delete;
    UnusedFiller &operator=(const UnusedFiller &) = delete;

    explicit UnusedFiller() {}

    ssize_t fill(ItemProvider<UPnP::ItemData> &item_provider, ID::List list_id,
                 ID::Item idx, size_t count, ListError &error,
                 const std::function<bool()> &may_continue) const override
    {
        cut_assert_true(may_continue != nullptr);
        cut_fail("%s(): unexpected call", __PRETTY_FUNCTION__);
        error = ListError::INTERNAL;
        return -1;
    }
};

/*!\test
 * Enter empty server child list.
 */
void test_enter_server_with_empty_root_directory()
{
    static UnusedFiller dummy;

    current_tiled_list_filler = &dummy;
    mock_dbus_upnp_helpers->expect_get_proxy_object_path_callback(FakeDBus::get_proxy_object_path);
    mock_dbus_upnp_helpers->expect_get_size_of_container(0, "/test/server/0");

    ListError error(ListError::INTERNAL);
    auto child_id = list_tree->enter_child(list_tree->get_root_list_id(), ID::Item(0), error);
    cut_assert_true(child_id.is_valid());
    cut_assert_false(error.failed());
    cppcut_assert_equal(size_t(0), std::static_pointer_cast<UPnP::MediaList>(cache->lookup(child_id))->size());

    /* doing it again does not trigger any D-Bus traffic */
    error = ListError::INTERNAL;
    auto again_id = list_tree->enter_child(list_tree->get_root_list_id(), ID::Item(0), error);
    cut_assert_false(error.failed());
    cppcut_assert_equal(child_id.get_raw_id(), again_id.get_raw_id());
}

class ItemGenerator: public TiledListFillerIface<UPnP::ItemData>
{
  private:
    const size_t number_of_items_;
    bool generate_directories_;

    size_t expected_number_of_filled_items_;
    size_t expected_number_of_requested_items_;
    mutable std::atomic<size_t> number_of_filled_items_;
    mutable std::atomic<size_t> number_of_requested_items_;

  public:
    ItemGenerator(const ItemGenerator &) = delete;
    ItemGenerator &operator=(const ItemGenerator &) = delete;

    /*!
     * Initialize an #ItemGenerator object.
     *
     * \param count
     *     Number of items generated in the directory.
     *
     * \param generate_directories
     *     Report items to be files if \c false, report them as directories if
     *     \c true.
     *
     * \param expected_number_of_filled_items, expected_number_of_requested_items
     *     Number of expected item fills and requests, respectively, in the
     *     test. These figures may be changed via
     *     #upnp_listtree_tests::ItemGenerator::expect().
     */
    explicit ItemGenerator(size_t count, bool generate_directories,
                           size_t expected_number_of_filled_items,
                           size_t expected_number_of_requested_items = 0):
        number_of_items_(count),
        generate_directories_(generate_directories),
        expected_number_of_filled_items_(expected_number_of_filled_items),
        expected_number_of_requested_items_(expected_number_of_requested_items),
        number_of_filled_items_(0),
        number_of_requested_items_(0)
    {
        if(expected_number_of_requested_items_ == 0)
            expected_number_of_requested_items_ = expected_number_of_filled_items_;
    }

    size_t size() const { return number_of_items_; }

    ssize_t fill(ItemProvider<UPnP::ItemData> &item_provider, ID::List list_id,
                 ID::Item idx, size_t count, ListError &error,
                 const std::function<bool()> &may_continue) const override
    {
        error = ListError::OK;

        number_of_requested_items_ += count;

        for(size_t i = 0; i < count; ++i)
        {
            if(idx.get_raw_id() >= number_of_items_)
            {
                number_of_filled_items_ += i;
                return i;
            }

            std::ostringstream os;
            os << "dbus-" << list_id.get_raw_id() << "-" << idx.get_raw_id();
            std::string temp = os.str();

            os.clear();
            os.str("");
            os << "Generated item " << idx.get_raw_id();

            *item_provider.next() =
                std::move(UPnP::ItemData(std::move(temp), std::move(os.str()),
                                         std::move(Url::String(Url::Sensitivity::GENERIC)),
                                         generate_directories_));

            idx = ID::Item(idx.get_raw_id() + 1);
        }

        number_of_filled_items_ += count;

        return count;
    }

    bool is_directory_generator() const
    {
        return generate_directories_;
    }

    void expect(size_t expected_number_of_filled_items,
                size_t expected_number_of_requested_items = 0)
    {
        expected_number_of_filled_items_ = expected_number_of_filled_items;
        expected_number_of_requested_items_ =
            expected_number_of_requested_items > 0
            ? expected_number_of_requested_items
            : expected_number_of_filled_items;
        number_of_filled_items_ = 0;
        number_of_requested_items_ = 0;
    }

    void check(size_t next_expected_number_of_filled_items = 0,
               size_t next_expected_number_of_requested_items = 0)
    {
        /*
         * We need to wait for all threads to finish to get the expected number
         * of requested and filled items right. Even after reading all list
         * items of interest (e.g., those requested in a for-loop), there may
         * still be a thread running for prefetching items that we didn't
         * request explicitly.
         */
        UPnP::MediaList::sync_threads();

        const size_t expected_number_of_filled_items = expected_number_of_filled_items_;
        const size_t expected_number_of_requested_items = expected_number_of_requested_items_;
        const size_t number_of_filled_items = number_of_filled_items_;
        const size_t number_of_requested_items = number_of_requested_items_;

        expected_number_of_filled_items_ = next_expected_number_of_filled_items;
        expected_number_of_requested_items_ = next_expected_number_of_requested_items;
        number_of_filled_items_ = 0;
        number_of_requested_items_ = 0;

        cppcut_assert_operator(expected_number_of_filled_items,
                               <=,
                               expected_number_of_requested_items);

        cppcut_assert_equal(expected_number_of_filled_items, number_of_filled_items);
        cppcut_assert_equal(expected_number_of_requested_items, number_of_requested_items);
    }
};

static ID::List prepare_enter_server_test(ItemGenerator &filler,
                                          size_t number_of_servers = 1)
{
    current_tiled_list_filler = &filler;

    ID::List first_child_id;

    for(size_t i = 0; i < number_of_servers; ++i)
    {
        mock_dbus_upnp_helpers->expect_get_proxy_object_path_callback(FakeDBus::get_proxy_object_path);

        std::ostringstream os;
        os << "/test/server/" << i;
        mock_dbus_upnp_helpers->expect_get_size_of_container(filler.size(), os.str());

        ListError error(ListError::INTERNAL);
        auto child_id = list_tree->enter_child(list_tree->get_root_list_id(), ID::Item(i), error);
        cut_assert_true(child_id.is_valid());
        cut_assert_false(error.failed());
        cppcut_assert_equal(filler.size(), std::static_pointer_cast<UPnP::MediaList>(cache->lookup(child_id))->size());

        mock_timebase.step(1000);

        if(!first_child_id.is_valid())
            first_child_id = child_id;

        /* doing it again does not trigger any D-Bus traffic */
        error = ListError::INTERNAL;
        auto again_id = list_tree->enter_child(list_tree->get_root_list_id(), ID::Item(i), error);
        cut_assert_false(error.failed());
        cppcut_assert_equal(child_id.get_raw_id(), again_id.get_raw_id());
    }

    cut_assert_true(first_child_id.is_valid());

    return first_child_id;
}

static void check_list_generated_by_item_generator(std::shared_ptr<const UPnP::MediaList> list,
                                                   size_t first, size_t end, bool directories)
{
    for(size_t i = first; i < end; ++i)
    {
        std::ostringstream os;
        os << "no need to prefetch index " << i << ", already in cache";
        mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG, os.str().c_str());

        const auto &item = (*list)[ID::Item(i)];

        cppcut_assert_equal(directories, item.get_specific_data().get_kind().is_directory());

        os.clear();
        os.str("");
        os << "Generated item " << i;
        std::string name;
        item.get_name(name);
        cppcut_assert_equal(os.str(), name);

        os.clear();
        os.str("");
        os << "dbus-" << list->get_cache_id().get_raw_id() << "-" << i;
        cppcut_assert_equal(os.str(), item.get_specific_data().get_dbus_path());

        cut_assert_false(item.get_kind().is_directory());
    }
}

static void check_list_generated_by_item_generator(std::shared_ptr<const UPnP::MediaList> list,
                                                   const ItemGenerator &filler)
{
    check_list_generated_by_item_generator(list, 0, filler.size(),
                                           filler.is_directory_generator());
}

/*!\test
 * Enter server child list with a single item.
 */
void test_enter_server_with_single_item_in_root_directory()
{
    ItemGenerator filler(1, false, 1, UPnP::media_list_tile_size);
    ID::List child_id = prepare_enter_server_test(filler);
    auto child_list = std::static_pointer_cast<const UPnP::MediaList>(cache->lookup(child_id));

    /* accessing first element of child list materializes its tile */
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "prefetch 1 items, starting at index 0");
    (void)(*child_list)[ID::Item(0)];

    check_list_generated_by_item_generator(child_list, filler);
    filler.check();
}

/*!\test
 * Enter server child list with the number of items matching the list's tile
 * size.
 *
 * This test is supposed to show that the tile sliding mechanism does not kick
 * in for such a small list.
 */
void test_enter_server_with_tile_size_items_in_root_directory()
{
    ItemGenerator filler(UPnP::media_list_tile_size, false, UPnP::media_list_tile_size);
    ID::List child_id = prepare_enter_server_test(filler);
    auto child_list = std::static_pointer_cast<const UPnP::MediaList>(cache->lookup(child_id));

    /* accessing first element of child list materializes its tile */
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "prefetch 1 items, starting at index 0");
    (void)(*child_list)[ID::Item(0)];

    check_list_generated_by_item_generator(child_list, filler);
    filler.check();
}

/*!\test
 * Enter server child list with one item more than the list's tile size.
 *
 * This test is supposed to show that the tile sliding mechanism is really
 * triggered for lists of at least this size. It is also a corner case in which
 * one of the internal tiles remains empty all the time.
 */
void test_enter_server_with_tile_size_plus_1_items_in_root_directory()
{
    ItemGenerator filler(UPnP::media_list_tile_size + 1, false,
                         UPnP::media_list_tile_size + 1, 2 * UPnP::media_list_tile_size);
    ID::List child_id = prepare_enter_server_test(filler);
    auto child_list = std::static_pointer_cast<const UPnP::MediaList>(cache->lookup(child_id));

    /* accessing first element of child list materializes its tile and the one
     * below */
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "prefetch 1 items, starting at index 0");
    (void)(*child_list)[ID::Item(0)];

    /* accessing first element in next tile does not materialize anything */
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "slide down to index 8");
    (void)(*child_list)[ID::Item(UPnP::media_list_tile_size)];

    /* go back and forth between the tiles, still no more materialization */
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "slide up to index 7");
    (void)(*child_list)[ID::Item(UPnP::media_list_tile_size - 1)];

    check_list_generated_by_item_generator(child_list, 0, UPnP::media_list_tile_size, false);

    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "slide down to index 8");
    (void)(*child_list)[ID::Item(UPnP::media_list_tile_size)];

    check_list_generated_by_item_generator(child_list, UPnP::media_list_tile_size,
                                           filler.size(), false);

    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "slide up to index 7");
    (void)(*child_list)[ID::Item(UPnP::media_list_tile_size - 1)];
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "no need to prefetch index 0, already in cache");
    (void)(*child_list)[ID::Item(0)];

    filler.check();
}

/*!\test
 * Enter server child list with the number of items matching three times the
 * list's tile size.
 *
 * This test is supposed to show that the tile sliding mechanism works for this
 * corner case in which all tiles are completely filled at first access, but
 * are only shuffled around afterwards.
 */
void test_enter_server_with_3_times_tile_size_items_in_root_directory()
{
    ItemGenerator filler(3 * UPnP::media_list_tile_size, false, 3 * UPnP::media_list_tile_size);
    ID::List child_id = prepare_enter_server_test(filler);
    auto child_list = std::static_pointer_cast<const UPnP::MediaList>(cache->lookup(child_id));

    /* accessing first element of child list materializes its tile and the
     * other two surrounding it */
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "prefetch 1 items, starting at index 0");
    (void)(*child_list)[ID::Item(0)];

    /* accessing first element in next tile does not materialize anything,
     * neither does any other access */
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "slide down to index 8");
    (void)(*child_list)[ID::Item(UPnP::media_list_tile_size)];

    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "slide up to index 7");
    (void)(*child_list)[ID::Item(UPnP::media_list_tile_size - 1)];
    check_list_generated_by_item_generator(child_list,
                                           0,
                                           UPnP::media_list_tile_size,
                                           false);

    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "slide down to index 8");
    (void)(*child_list)[ID::Item(UPnP::media_list_tile_size)];
    check_list_generated_by_item_generator(child_list,
                                           UPnP::media_list_tile_size,
                                           2 * UPnP::media_list_tile_size,
                                           false);

    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "slide down to index 16");
    (void)(*child_list)[ID::Item(2 * UPnP::media_list_tile_size)];
    check_list_generated_by_item_generator(child_list,
                                           2 * UPnP::media_list_tile_size,
                                           filler.size(), false);

    /* wrap around, notion of "up" and "down" is a bit twisted in this case */
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "slide down to index 0");
    (void)(*child_list)[ID::Item(0)];
    check_list_generated_by_item_generator(child_list,
                                           0,
                                           UPnP::media_list_tile_size,
                                           false);

    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "slide down to index 8");
    (void)(*child_list)[ID::Item(UPnP::media_list_tile_size)];
    check_list_generated_by_item_generator(child_list,
                                           UPnP::media_list_tile_size,
                                           2 * UPnP::media_list_tile_size,
                                           false);

    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "slide up to index 0");
    (void)(*child_list)[ID::Item(0)];
    check_list_generated_by_item_generator(child_list,
                                           0,
                                           UPnP::media_list_tile_size,
                                           false);

    /* wrap around again */
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "slide up to index 16");
    (void)(*child_list)[ID::Item(2 * UPnP::media_list_tile_size)];
    check_list_generated_by_item_generator(child_list,
                                           2 * UPnP::media_list_tile_size,
                                           filler.size(), false);

    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "slide up to index 8");
    (void)(*child_list)[ID::Item(UPnP::media_list_tile_size)];
    check_list_generated_by_item_generator(child_list,
                                           UPnP::media_list_tile_size,
                                           2 * UPnP::media_list_tile_size,
                                           false);

    filler.check();
}

/*!\test
 * Access items in a long server list in ascending order.
 *
 * The list has a prime numbered number of items exceeding the combined
 * capacity of the list's tiles. The last tile is not completely filled unless
 * the tile size matches our prime number.
 */
void test_walk_down_long_server_list()
{
    cppcut_assert_operator(3U * UPnP::media_list_tile_size, <, 83U);

    ItemGenerator filler(83, false,
                         5 * UPnP::media_list_tile_size + 83 % UPnP::media_list_tile_size,
                         6 * UPnP::media_list_tile_size);
    ID::List child_id = prepare_enter_server_test(filler);
    auto child_list = std::static_pointer_cast<const UPnP::MediaList>(cache->lookup(child_id));

    /* accessing first element of child list materializes its tile and the
     * other two surrounding it */
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "prefetch 1 items, starting at index 0");
    (void)(*child_list)[ID::Item(0)];
    check_list_generated_by_item_generator(child_list,
                                           0 * UPnP::media_list_tile_size,
                                           1 * UPnP::media_list_tile_size,
                                           false);

    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "slide down to index 8");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "materialize adjacent tile around index 16");
    (void)(*child_list)[ID::Item(UPnP::media_list_tile_size)];
    check_list_generated_by_item_generator(child_list,
                                           1 * UPnP::media_list_tile_size,
                                           2 * UPnP::media_list_tile_size,
                                           false);

    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "slide down to index 16");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "materialize adjacent tile around index 24");
    (void)(*child_list)[ID::Item(2 * UPnP::media_list_tile_size)];
    check_list_generated_by_item_generator(child_list,
                                           2 * UPnP::media_list_tile_size,
                                           3 * UPnP::media_list_tile_size,
                                           false);

    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "slide down to index 24");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "materialize adjacent tile around index 32");
    (void)(*child_list)[ID::Item(3 * UPnP::media_list_tile_size)];
    check_list_generated_by_item_generator(child_list,
                                           3 * UPnP::media_list_tile_size,
                                           4 * UPnP::media_list_tile_size,
                                           false);

    filler.check();
}

/*!\test
 * Access items in a long server list in descending order.
 *
 * The list has a prime numbered number of items exceeding the combined
 * capacity of the list's tiles. The last tile is not completely filled unless
 * the tile size matches our prime number.
 */
void test_walk_up_long_server_list()
{
    cppcut_assert_operator(3U * UPnP::media_list_tile_size, <, 83U);

    ItemGenerator filler(83, false,
                         5 * UPnP::media_list_tile_size + 83 % UPnP::media_list_tile_size,
                         6 * UPnP::media_list_tile_size);
    ID::List child_id = prepare_enter_server_test(filler);
    auto child_list = std::static_pointer_cast<const UPnP::MediaList>(cache->lookup(child_id));

    /* accessing first element of child list materializes its tile and the
     * other two surrounding it */
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "prefetch 1 items, starting at index 0");
    (void)(*child_list)[ID::Item(0)];
    check_list_generated_by_item_generator(child_list,
                                           0 * UPnP::media_list_tile_size,
                                           1 * UPnP::media_list_tile_size,
                                           false);

    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "slide up to index 82");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "materialize adjacent tile around index 74");
    (void)(*child_list)[ID::Item(filler.size() - 1)];
    check_list_generated_by_item_generator(child_list,
                                           10 * UPnP::media_list_tile_size,
                                           filler.size(),
                                           false);

    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "slide up to index 72");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "materialize adjacent tile around index 64");
    (void)(*child_list)[ID::Item(9 * UPnP::media_list_tile_size)];
    check_list_generated_by_item_generator(child_list,
                                           9 * UPnP::media_list_tile_size,
                                           10 * UPnP::media_list_tile_size,
                                           false);

    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "slide up to index 64");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "materialize adjacent tile around index 56");
    (void)(*child_list)[ID::Item(8 * UPnP::media_list_tile_size)];
    check_list_generated_by_item_generator(child_list,
                                           8 * UPnP::media_list_tile_size,
                                           9 * UPnP::media_list_tile_size,
                                           false);

    filler.check();
}

/*!\test
 * Randomly access items in a long server list.
 *
 * This is the worst case for our tiled list. It always has to fill all tiles
 * (and it does so synchronously). It never benefits from the cached tiles.
 *
 * The list has a prime numbered number of items exceeding the combined
 * capacity of the list's tiles. The last tile is not completely filled unless
 * the tile size matches our prime number.
 */
void test_random_access_in_long_server_list()
{
    cppcut_assert_operator(3U * UPnP::media_list_tile_size, <, 83U);

    ItemGenerator filler(83, false,
                         3 * 3 * UPnP::media_list_tile_size +
                         2 * UPnP::media_list_tile_size + (83 % UPnP::media_list_tile_size),
                         4 * 3 * UPnP::media_list_tile_size);
    ID::List child_id = prepare_enter_server_test(filler);
    auto child_list = std::static_pointer_cast<const UPnP::MediaList>(cache->lookup(child_id));

    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "prefetch 1 items, starting at index 15");
    (void)(*child_list)[ID::Item(15)];
    check_list_generated_by_item_generator(child_list,
                                           1 * UPnP::media_list_tile_size,
                                           2 * UPnP::media_list_tile_size,
                                           false);

    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "prefetch 1 items, starting at index 67");
    (void)(*child_list)[ID::Item(67)];
    check_list_generated_by_item_generator(child_list,
                                           8 * UPnP::media_list_tile_size,
                                           9 * UPnP::media_list_tile_size,
                                           false);

    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "prefetch 1 items, starting at index 50");
    (void)(*child_list)[ID::Item(50)];
    check_list_generated_by_item_generator(child_list,
                                           6 * UPnP::media_list_tile_size,
                                           7 * UPnP::media_list_tile_size,
                                           false);

    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "prefetch 1 items, starting at index 5");
    (void)(*child_list)[ID::Item(5)];
    check_list_generated_by_item_generator(child_list,
                                           0 * UPnP::media_list_tile_size,
                                           1 * UPnP::media_list_tile_size,
                                           false);

    filler.check();
}

/*!\test
 * Directly accessing elements of an empty directory throws an exception and
 * triggers emission of a "BUG" message.
 */
void test_enter_empty_content_directory_throws_exception_and_emits_bug_message()
{
    ItemGenerator filler(0, true, 0, 0);
    ID::List child_id = prepare_enter_server_test(filler);
    auto child_list = std::static_pointer_cast<const UPnP::MediaList>(cache->lookup(child_id));

    cppcut_assert_equal(size_t(0), child_list->size());

    /* accessing first element of child list yields bug report */
    mock_messages->expect_msg_error_formatted(0, LOG_CRIT,
        "BUG: requested tile list materialization around 0, but have only 0 items");

    const void *pointer = nullptr;

    try
    {
        pointer = &(*child_list)[ID::Item(0)];
        cut_fail("Missing expected exception");
    }
    catch(const ListIterException &e)
    {
        cppcut_assert_equal("Tile materialization failed", e.what());
    }

    cppcut_assert_null(pointer);

    check_list_generated_by_item_generator(child_list, filler);
    filler.check();
}

/*!\test
 * Iterating over elements of an empty directory has no effect.
 */
void test_iterate_over_empty_content_directory_does_not_invoke_callback()
{
    ItemGenerator filler(0, true, 0, 0);
    ID::List child_id = prepare_enter_server_test(filler);
    auto child_list = std::static_pointer_cast<const UPnP::MediaList>(cache->lookup(child_id));

    cppcut_assert_equal(size_t(0), child_list->size());

    auto callback = [] (const ListTreeIface::ForEachItemDataGeneric &data) -> bool
    {
        cut_fail("Unexpected call of callback for empty directory");
        return false;
    };

    mock_messages->expect_msg_error_formatted(0, LOG_WARNING,
        "WARNING: Client requested 3 items starting at index 0, but list size is 0");

    cut_assert_false(list_tree->for_each(child_list->get_cache_id(),
                                         ID::Item(0), 3, callback).failed());

    check_list_generated_by_item_generator(child_list, filler);
    filler.check();
}

static ID::List generate_hierarchy(ID::List &last_list_id, size_t expected_extra_tiles = 0)
{
    static ItemGenerator filler(5, true, 0);
    filler.expect((3 + expected_extra_tiles) * 5,
                  (3 + expected_extra_tiles) * UPnP::media_list_tile_size);

    ID::List child_id = prepare_enter_server_test(filler);

    cppcut_assert_equal(size_t(1), list_tree->get_server_list()->size());
    cppcut_assert_equal(2U, list_tree->get_root_list_id().get_raw_id());
    cppcut_assert_equal(3U, child_id.get_raw_id());

    ListError error;

    /* first level */
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "prefetch 1 items, starting at index 0");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DIAG,
                                              "D-Bus path of new list is dbus-3-0");
    mock_dbus_upnp_helpers->expect_get_size_of_container(2, "dbus-3-0");
    list_tree->enter_child(child_id, ID::Item(0), error);
    cut_assert_false(error.failed());
    mock_timebase.step(1000);

    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "no need to prefetch index 1, already in cache");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DIAG,
                                              "D-Bus path of new list is dbus-3-1");
    mock_dbus_upnp_helpers->expect_get_size_of_container(1, "dbus-3-1");
    list_tree->enter_child(child_id, ID::Item(1), error);
    cut_assert_false(error.failed());
    mock_timebase.step(1000);

    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "no need to prefetch index 2, already in cache");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DIAG,
                                              "D-Bus path of new list is dbus-3-2");
    mock_dbus_upnp_helpers->expect_get_size_of_container(5, "dbus-3-2");
    ID::List here = list_tree->enter_child(child_id, ID::Item(2), error);
    cut_assert_false(error.failed());
    mock_timebase.step(1000);

    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "no need to prefetch index 3, already in cache");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DIAG,
                                              "D-Bus path of new list is dbus-3-3");
    mock_dbus_upnp_helpers->expect_get_size_of_container(1, "dbus-3-3");
    list_tree->enter_child(child_id, ID::Item(3), error);
    cut_assert_false(error.failed());
    mock_timebase.step(1000);

    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "no need to prefetch index 4, already in cache");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DIAG,
                                              "D-Bus path of new list is dbus-3-4");
    mock_dbus_upnp_helpers->expect_get_size_of_container(3, "dbus-3-4");
    ID::List there = list_tree->enter_child(child_id, ID::Item(4), error);
    cut_assert_false(error.failed());
    mock_timebase.step(1000);

    /* first subtree on second level */
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "prefetch 1 items, starting at index 0");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DIAG,
                                              "D-Bus path of new list is dbus-6-0");
    mock_dbus_upnp_helpers->expect_get_size_of_container(5, "dbus-6-0");
    list_tree->enter_child(here, ID::Item(0), error);
    cut_assert_false(error.failed());
    mock_timebase.step(1000);

    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "no need to prefetch index 1, already in cache");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DIAG,
                                              "D-Bus path of new list is dbus-6-1");
    mock_dbus_upnp_helpers->expect_get_size_of_container(0, "dbus-6-1");
    list_tree->enter_child(here, ID::Item(1), error);
    cut_assert_false(error.failed());
    mock_timebase.step(1000);

    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "no need to prefetch index 2, already in cache");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DIAG,
                                              "D-Bus path of new list is dbus-6-2");
    mock_dbus_upnp_helpers->expect_get_size_of_container(1, "dbus-6-2");
    list_tree->enter_child(here, ID::Item(2), error);
    cut_assert_false(error.failed());
    mock_timebase.step(1000);

    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "no need to prefetch index 3, already in cache");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DIAG,
                                              "D-Bus path of new list is dbus-6-3");
    mock_dbus_upnp_helpers->expect_get_size_of_container(4, "dbus-6-3");
    list_tree->enter_child(here, ID::Item(3), error);
    cut_assert_false(error.failed());
    mock_timebase.step(1000);

    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "no need to prefetch index 4, already in cache");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DIAG,
                                              "D-Bus path of new list is dbus-6-4");
    mock_dbus_upnp_helpers->expect_get_size_of_container(2, "dbus-6-4");
    list_tree->enter_child(here, ID::Item(4), error);
    cut_assert_false(error.failed());
    mock_timebase.step(1000);

    /* second subtree on second level */
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "prefetch 1 items, starting at index 0");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DIAG,
                                              "D-Bus path of new list is dbus-8-0");
    mock_dbus_upnp_helpers->expect_get_size_of_container(4, "dbus-8-0");
    list_tree->enter_child(there, ID::Item(0), error);
    cut_assert_false(error.failed());
    mock_timebase.step(1000);

    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "no need to prefetch index 1, already in cache");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DIAG,
                                              "D-Bus path of new list is dbus-8-1");
    mock_dbus_upnp_helpers->expect_get_size_of_container(2, "dbus-8-1");
    list_tree->enter_child(there, ID::Item(1), error);
    cut_assert_false(error.failed());
    mock_timebase.step(1000);

    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "no need to prefetch index 2, already in cache");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DIAG,
                                              "D-Bus path of new list is dbus-8-2");
    mock_dbus_upnp_helpers->expect_get_size_of_container(3, "dbus-8-2");
    last_list_id = list_tree->enter_child(there, ID::Item(2), error);
    cut_assert_false(error.failed());
    mock_timebase.step(1000);

    filler.check();

    return child_id;
}

/*!\test
 * Garbage collection of part of the directory hierarchy keeps the lists in
 * consistent state.
 */
void test_partial_garbage_collection_in_directory_hierarchy()
{
    ID::List last_list_id;
    ID::List root = generate_hierarchy(last_list_id);

    auto next_gc = std::chrono::milliseconds(cache->gc());
    mock_timebase.step(next_gc.count());

    /* the expected message comes from #UPnP::MediaList::obliviate_child()
     * which uses the index operator iff the child list discarded here is known
     * to be present in the parent list */
    mock_messages->expect_msg_vinfo(MESSAGE_LEVEL_DEBUG,
                                    "no need to prefetch index %u, already in cache");
    next_gc = std::chrono::milliseconds(cache->gc());

    mock_timebase.step(5000);

    mock_messages->expect_msg_vinfo(MESSAGE_LEVEL_DEBUG,
                                    "no need to prefetch index %u, already in cache");
    mock_messages->expect_msg_vinfo(MESSAGE_LEVEL_DEBUG,
                                    "no need to prefetch index %u, already in cache");
    mock_messages->expect_msg_vinfo(MESSAGE_LEVEL_DEBUG,
                                    "no need to prefetch index %u, already in cache");
    next_gc = std::chrono::milliseconds(cache->gc());

    mock_timebase.step(1000);

    /* now enter the child again that was discarded during first gc() run */
    mock_messages->expect_msg_vinfo(MESSAGE_LEVEL_DEBUG,
                                    "no need to prefetch index %u, already in cache");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DIAG,
                                              "D-Bus path of new list is dbus-3-0");
    mock_dbus_upnp_helpers->expect_get_size_of_container(2, "dbus-3-0");
    ListError error;
    ID::List materialized_again = list_tree->enter_child(root, ID::Item(0), error);
    cut_assert_false(error.failed());

    cppcut_assert_equal(last_list_id.get_raw_id() + 1, materialized_again.get_raw_id());
}

/*!\test
 * Iterating over invalid or non-existent list fails and has no effect.
 */
void test_iterate_over_stored_names_and_types_in_invalid_list()
{
    bool called = false;
    ListError error = list_tree->for_each(ID::List(), ID::Item(0), 0,
                                          [&called]
                                          (const ListTreeIface::ForEachItemDataGeneric &data)
                                          {
                                              called = true;
                                              return true;
                                          });

    cppcut_assert_equal(ListError(ListError::INVALID_ID), error);
    cut_assert_false(called);

    error = list_tree->for_each(ID::List(42), ID::Item(0), 0,
                                [&called]
                                (const ListTreeIface::ForEachItemDataGeneric &data)
                                {
                                    called = true;
                                    return true;
                                });

    cppcut_assert_equal(ListError(ListError::INVALID_ID), error);
    cut_assert_false(called);
}

static void set_dleynaserver_expectations_for_foreach(size_t count,
                                                      const FakeDBus::ProxyID expected_first_proxy_id)
{
    for(size_t i = 0; i < count; ++i)
    {
        std::ostringstream os;
        os << "Friendly name for '/test/server/" << i << "'";
        mock_dleynaserver_dbus->expect_tdbus_dleynaserver_media_device_get_friendly_name(
            os.str().c_str(),
            reinterpret_cast<tdbusdleynaserverMediaDevice *>(expected_first_proxy_id + i));

        os.str("");
        os << "Model description for '/test/server/" << i << "'";
        mock_dleynaserver_dbus->expect_tdbus_dleynaserver_media_device_get_model_description(
            os.str().c_str(),
            reinterpret_cast<tdbusdleynaserverMediaDevice *>(expected_first_proxy_id + i));

        os.str("");
        os << "Model name for '/test/server/" << i << "'";
        mock_dleynaserver_dbus->expect_tdbus_dleynaserver_media_device_get_model_name(
            os.str().c_str(),
            reinterpret_cast<tdbusdleynaserverMediaDevice *>(expected_first_proxy_id + i));

        os.str("");
        os << "Model number for '/test/server/" << i << "'";
        mock_dleynaserver_dbus->expect_tdbus_dleynaserver_media_device_get_model_number(
            os.str().c_str(),
            reinterpret_cast<tdbusdleynaserverMediaDevice *>(expected_first_proxy_id + i));
    }
}

/*!\test
 * Iterating over server list with only one server succeeds.
 */
void test_iterate_over_server_list_with_single_entry()
{
    bool called = false;

    auto callback = [&called] (const ListTreeIface::ForEachItemDataGeneric &data) -> bool
    {
        cut_assert_false(called);

        cppcut_assert_equal("Friendly name for '/test/server/0' "
                            "(Model description for '/test/server/0' "
                            "Model name for '/test/server/0' "
                            "Model number for '/test/server/0')",
                            data.name_.c_str());
        cut_assert_true(data.kind_.is_directory());

        called = true;

        return true;
    };

    set_dleynaserver_expectations_for_foreach(1, FakeDBus::next_proxy_id - 1);

    cut_assert_false(list_tree->for_each(list_tree->get_root_list_id(),
                                         ID::Item(0), 0, callback).failed());

    cut_assert_true(called);
}

/*!\test
 * Iterating over server list with several servers succeeds.
 */
void test_iterate_over_server_list_with_multiple_entries()
{
    /*
     * This is "-1" because the fixture setup code has already created the
     * server list with one entry, so the first server's expected proxy ID is
     * one off from what is now stored in #FakeDBus::next_proxy_id.
     */
    const FakeDBus::ProxyID expected_first_server_dbus_proxy_id = FakeDBus::next_proxy_id - 1;

    static const std::vector<std::string> servers =
    {
        std::string("/test/server/1"),
        std::string("/test/server/2"),
        std::string("/test/server/3"),
        std::string("/test/server/4"),
    };

    add_servers(servers, ID::List(2));

    size_t count = 0;

    auto callback = [&count] (const ListTreeIface::ForEachItemDataGeneric &data)
    {
        std::ostringstream os;
        os << "Friendly name for '/test/server/" << count << "' ("
           << "Model description for '/test/server/" << count << "' "
           << "Model name for '/test/server/" << count << "' "
           << "Model number for '/test/server/" << count << "')";

        cppcut_assert_equal(os.str(), data.name_);
        cut_assert_true(data.kind_.is_directory());

        ++count;

        return true;
    };

    set_dleynaserver_expectations_for_foreach(servers.size() + 1,
                                              expected_first_server_dbus_proxy_id);

    cut_assert_false(list_tree->for_each(list_tree->get_root_list_id(),
                                         ID::Item(0), 0, callback).failed());

    cppcut_assert_equal(size_t(1) + servers.size(), count);
    cppcut_assert_equal(list_tree->get_server_list()->size(), count);
    cppcut_assert_equal(list_tree->size(list_tree->get_root_list_id()), ssize_t(count));
}

/*!\test
 * Iterating over empty directory succeeds, but has no effect.
 */
void test_iterate_over_stored_names_and_types_in_empty_directory()
{
    static UnusedFiller dummy;

    current_tiled_list_filler = &dummy;
    mock_dbus_upnp_helpers->expect_get_proxy_object_path_callback(FakeDBus::get_proxy_object_path);
    mock_dbus_upnp_helpers->expect_get_size_of_container(0, "/test/server/0");

    ListError error(ListError::INTERNAL);
    auto child_id = list_tree->enter_child(list_tree->get_root_list_id(), ID::Item(0), error);
    cut_assert_false(error.failed());
    cut_assert_true(child_id.is_valid());
    cppcut_assert_equal(size_t(0), std::static_pointer_cast<UPnP::MediaList>(cache->lookup(child_id))->size());

    bool called = false;

    cut_assert_false(list_tree->for_each(child_id, ID::Item(0), 0,
                                         [&called]
                                         (const ListTreeIface::ForEachItemDataGeneric &data)
                                         {
                                             called = true;
                                             return true;
                                         }).failed());

    cut_assert_false(called);
}

/*!\test
 * Iterating over directory with multiple entries succeeds.
 */
void test_iterate_over_stored_names_and_types_in_directory()
{
    ID::List last_list_id;
    ID::List root = generate_hierarchy(last_list_id);

    size_t count = 0;

    auto callback = [&count] (const ListTreeIface::ForEachItemDataGeneric &data)
    {
        std::ostringstream os;
        os << "Generated item " << count;

        cppcut_assert_equal(os.str(), data.name_);
        cut_assert_true(data.kind_.is_directory());

        ++count;

        return true;
    };

    /* root directory of the only server in the hierarchy */
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "no need to prefetch index 0, already in cache");

    mock_dbus_upnp_helpers->expect_get_proxy_object_path(
        "Server root",
        reinterpret_cast<tdbusdleynaserverMediaDevice *>(FakeDBus::next_proxy_id - 1));
    cppcut_assert_equal("Server root", std::static_pointer_cast<UPnP::MediaList>(cache->lookup(root))->get_dbus_object_path().c_str());

    cut_assert_false(list_tree->for_each(root, ID::Item(0), 0, callback).failed());

    cppcut_assert_equal(size_t(5), count);
    cppcut_assert_equal(list_tree->size(root), ssize_t(count));

    /* somewhere in the middle, currently leaf object in the list hierarchy */
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "prefetch 3 items, starting at index 0");

    count = 0;
    cppcut_assert_equal("dbus-8-2", std::static_pointer_cast<UPnP::MediaList>(cache->lookup(last_list_id))->get_dbus_object_path().c_str());

    cut_assert_false(list_tree->for_each(last_list_id, ID::Item(0), 0, callback).failed());

    cppcut_assert_equal(size_t(3), count);
    cppcut_assert_equal(list_tree->size(last_list_id), ssize_t(count));
}

/*!\test
 * Iterating over directory stops at and of directory even if more items were
 * requested.
 *
 * This means, no reads beyond buffer boundaries happend and no error is
 * indicated.
 */
void test_iterate_over_stored_names_and_types_in_directory_with_too_many_items_requested()
{
    ID::List last_list_id;
    ID::List root = generate_hierarchy(last_list_id, 0);

    size_t count = 0;

    auto callback = [&count] (const ListTreeIface::ForEachItemDataGeneric &data)
    {
        std::ostringstream os;
        os << "Generated item " << count;

        cppcut_assert_equal(os.str(), data.name_);
        cut_assert_true(data.kind_.is_directory());

        ++count;

        return true;
    };

    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "no need to prefetch index 0, already in cache");

    cut_assert_false(list_tree->for_each(root, ID::Item(0), 500, callback).failed());

    cppcut_assert_equal(size_t(5), count);
    cppcut_assert_equal(list_tree->size(root), ssize_t(count));
}

/*!\test
 * Iterating over directory may start in the middle of the list and with a
 * restriction on the number of items.
 */
void test_restricted_iterate_over_stored_names_and_types_in_directory()
{
    ID::List last_list_id;
    ID::List root = generate_hierarchy(last_list_id);

    size_t count = 0;
    static constexpr size_t offset = 1;

    auto callback = [&count] (const ListTreeIface::ForEachItemDataGeneric &data)
    {
        std::ostringstream os;
        os << "Generated item " << count + offset;

        cppcut_assert_equal(os.str(), data.name_);
        cut_assert_true(data.kind_.is_directory());

        ++count;

        return true;
    };

    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "no need to prefetch index 1, already in cache");

    cut_assert_false(list_tree->for_each(root, ID::Item(offset), 3, callback).failed());

    cppcut_assert_equal(size_t(3), count);
    cppcut_assert_equal(list_tree->size(root) - 2, ssize_t(count));
}

/*!\test
 * Iterating over directory from the middle of the list stops at and of
 * directory even if more items were requested.
 *
 * This means, no reads beyond buffer boundaries happend and no error is
 * indicated.
 */
void test_restricted_iterate_over_stored_names_and_types_in_directory_with_too_many_items_requested()
{
    ID::List last_list_id;
    ID::List root = generate_hierarchy(last_list_id);

    size_t count = 0;
    static constexpr size_t offset = 2;

    auto callback = [&count] (const ListTreeIface::ForEachItemDataGeneric &data)
    {
        std::ostringstream os;
        os << "Generated item " << count + offset;

        cppcut_assert_equal(os.str(), data.name_);
        cut_assert_true(data.kind_.is_directory());

        ++count;

        return true;
    };

    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "no need to prefetch index 2, already in cache");

    cut_assert_false(list_tree->for_each(root, ID::Item(offset), 50, callback).failed());

    cppcut_assert_equal(size_t(3), count);
    cppcut_assert_equal(list_tree->size(root) - 2, ssize_t(count));
}

/*!\test
 * Enter a directory which is located below another directory (not server),
 * with an ID that references an item beyond the down tile.
 *
 * This test was added for hunting down bug #1 and its follow-up bug #2. It
 * boils down being a test for the #ListTiles_::const_iterator implementation.
 */
void test_id_of_nested_media_list_is_contained_in_parent_media_list()
{
    static constexpr size_t list_size =
        UPnP::media_list_tile_size * 2 + 3;
    ItemGenerator filler(list_size, true,
                         list_size, 3 * UPnP::media_list_tile_size);

    ID::List child_id = prepare_enter_server_test(filler, 1);

    cppcut_assert_equal(size_t(1), list_tree->get_server_list()->size());
    cppcut_assert_equal(2U, list_tree->get_root_list_id().get_raw_id());
    cppcut_assert_equal(3U, child_id.get_raw_id());
    cppcut_assert_equal(ssize_t(list_size), list_tree->size(child_id));

    /* enumerating all items in the server's root directory loads the whole
     * list into the tile cache, with the first items in the list ending up in
     * the up tile */
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "prefetch 19 items, starting at index 0");
    cut_assert_false(list_tree->for_each(child_id, ID::Item(0), 0,
                                         [] (const ListTreeIface::ForEachItemDataGeneric &) { return true; })
                     .failed());

    /* entering an item at the end of the list (cached in the down tile) causes
     * the down tile to be the new center tile; the tile that contains the
     * first list items ends up being the new down tile */
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "slide down to index 17");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DIAG,
                                              "D-Bus path of new list is dbus-3-17");
    mock_dbus_upnp_helpers->expect_get_size_of_container(list_size, "dbus-3-17");
    ListError error;
    ID::List second_level_id = list_tree->enter_child(child_id, ID::Item(17), error);
    cut_assert_true(second_level_id.is_valid());
    cut_assert_false(error.failed());

    /* the parent list of the second-level list must contain the ID of the
     * second-level list */
    auto second_level = cache->lookup(second_level_id);
    cppcut_assert_not_null(second_level.get());

    auto first_level = std::static_pointer_cast<UPnP::MediaList>(second_level->get_parent());
    cppcut_assert_not_null(first_level.get());

    auto child_item = first_level->lookup_child_by_id(second_level_id);
    cppcut_assert_not_null(child_item);

    filler.check();
}

/*!\test
 * Demonstrate that the tile cache is effective.
 *
 * Iterating over a bunch of items in the middle of a list should cause the
 * tile cache to materialize these items and to keep them stored. As long as
 * these items are covered by adjacent tiles, iterating over the same items
 * should not cause any further materialization; expensive UPnP traffic should
 * be avoided by this.
 *
 * This test stresses the tile cache by starting exactly on a tile boundary at
 * the beginning of a tile and ending exactly on a tile boundary at the end of
 * the last tile. All tiles should be completely filled by this.
 *
 * \note
 *     The test \e assumes that there are up to 3 tiles in the tile cache. As a
 *     result of good encapsulation, there is no good way to find out this
 *     parameter at runtime.
 */
void test_iterating_twice_over_same_area_fetches_content_only_once()
{
    ItemGenerator filler(500, false, 3 * UPnP::media_list_tile_size);
    ID::List list_id = prepare_enter_server_test(filler);

    size_t count = 0;
    static constexpr size_t offset = 10 * UPnP::media_list_tile_size;

    auto callback = [&count] (const ListTreeIface::ForEachItemDataGeneric &data)
    {
        std::ostringstream os;
        os << "Generated item " << count + offset;

        cppcut_assert_equal(os.str(), data.name_);
        cut_assert_false(data.kind_.is_directory());

        ++count;

        return true;
    };

    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "prefetch 24 items, starting at index 80");

    cut_assert_false(list_tree->for_each(list_id, ID::Item(offset),
                                         3 * UPnP::media_list_tile_size,
                                         callback).failed());

    mock_messages->check();

    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "no need to prefetch index 80, already in cache");
    count = 0;
    cut_assert_false(list_tree->for_each(list_id, ID::Item(offset),
                                         3 * UPnP::media_list_tile_size,
                                         callback).failed());

    filler.check();
}

/*!\test
 * Iterating over a big range uses less efficient algorithm.
 *
 * Instead of a cache-friendly version, a cache-thrashing version of our
 * #for_each_item() function is used in case the items do not fit into cache.
 */
void test_iterating_over_big_range_causes_cache_thrashing()
{
    ItemGenerator filler(500, false, 6 * UPnP::media_list_tile_size);
    ID::List list_id = prepare_enter_server_test(filler);

    size_t count = 0;
    static constexpr size_t offset = 10 * UPnP::media_list_tile_size;

    auto callback = [&count] (const ListTreeIface::ForEachItemDataGeneric &data)
    {
        std::ostringstream os;
        os << "Generated item " << count + offset;

        cppcut_assert_equal(os.str(), data.name_);
        cut_assert_false(data.kind_.is_directory());

        ++count;

        return true;
    };

    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "prefetch 1 items, starting at index 80");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "no need to prefetch index 81, already in cache");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "no need to prefetch index 82, already in cache");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "no need to prefetch index 83, already in cache");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "no need to prefetch index 84, already in cache");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "no need to prefetch index 85, already in cache");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "no need to prefetch index 86, already in cache");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "no need to prefetch index 87, already in cache");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "slide down to index 88");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "materialize adjacent tile around index 96");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "no need to prefetch index 89, already in cache");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "no need to prefetch index 90, already in cache");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "no need to prefetch index 91, already in cache");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "no need to prefetch index 92, already in cache");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "no need to prefetch index 93, already in cache");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "no need to prefetch index 94, already in cache");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "no need to prefetch index 95, already in cache");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "slide down to index 96");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "materialize adjacent tile around index 104");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "no need to prefetch index 97, already in cache");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "no need to prefetch index 98, already in cache");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "no need to prefetch index 99, already in cache");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "no need to prefetch index 100, already in cache");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "no need to prefetch index 101, already in cache");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "no need to prefetch index 102, already in cache");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "no need to prefetch index 103, already in cache");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "slide down to index 104");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "materialize adjacent tile around index 112");

    cut_assert_false(list_tree->for_each(list_id, ID::Item(offset),
                                         3 * UPnP::media_list_tile_size + 1,
                                         callback).failed());

    filler.check();
}

/*!\test
 * Iterating over tile boundaries causes use of less efficient algorithm.
 *
 * Instead of a cache-friendly version, a cache-thrashing version of our
 * #for_each_item() function is used in case the requested items are unluckily
 * distributed over more than 3 tiles.
 */
void test_iterating_over_too_many_tiles_causes_cache_thrashing()
{
    ItemGenerator filler(500, false, 6 * UPnP::media_list_tile_size);
    ID::List list_id = prepare_enter_server_test(filler);

    size_t count = 0;
    static constexpr size_t offset = 10 * UPnP::media_list_tile_size - 1;

    auto callback = [&count] (const ListTreeIface::ForEachItemDataGeneric &data)
    {
        std::ostringstream os;
        os << "Generated item " << count + offset;

        cppcut_assert_equal(os.str(), data.name_);
        cut_assert_false(data.kind_.is_directory());

        ++count;

        return true;
    };

    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "prefetch 1 items, starting at index 79");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "slide down to index 80");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "materialize adjacent tile around index 88");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "no need to prefetch index 81, already in cache");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "no need to prefetch index 82, already in cache");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "no need to prefetch index 83, already in cache");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "no need to prefetch index 84, already in cache");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "no need to prefetch index 85, already in cache");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "no need to prefetch index 86, already in cache");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "no need to prefetch index 87, already in cache");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "slide down to index 88");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "materialize adjacent tile around index 96");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "no need to prefetch index 89, already in cache");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "no need to prefetch index 90, already in cache");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "no need to prefetch index 91, already in cache");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "no need to prefetch index 92, already in cache");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "no need to prefetch index 93, already in cache");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "no need to prefetch index 94, already in cache");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "no need to prefetch index 95, already in cache");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "slide down to index 96");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "materialize adjacent tile around index 104");

    cut_assert_false(list_tree->for_each(list_id, ID::Item(offset),
                                         2 * UPnP::media_list_tile_size + 2,
                                         callback).failed());

    filler.check();
}

/*!\test
 * Iterating over a range that starts in a cached range and ends just below the
 * down tile fetches only what's necessary.
 */
void test_iterating_over_big_overlapping_ranges_causes_minimal_upnp_traffic()
{
    ItemGenerator filler(500, false, 4 * UPnP::media_list_tile_size);
    ID::List list_id = prepare_enter_server_test(filler);

    static constexpr size_t base_offset = 10 * UPnP::media_list_tile_size;

    size_t count;
    size_t offset;

    auto callback = [&count, &offset] (const ListTreeIface::ForEachItemDataGeneric &data)
    {
        std::ostringstream os;
        os << "Generated item " << count + offset;

        cppcut_assert_equal(os.str(), data.name_);
        cut_assert_false(data.kind_.is_directory());

        ++count;

        return true;
    };

    /* fill three tiles */
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "prefetch 16 items, starting at index 81");
    count = 0;
    offset = base_offset + 1;
    cut_assert_false(list_tree->for_each(list_id, ID::Item(offset),
                                         2 * UPnP::media_list_tile_size,
                                         callback).failed());
    mock_messages->check();

    /* iterate over last few items in down tile */
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "no need to prefetch index 100, already in cache");
    count = 0;
    offset = base_offset + 2 * UPnP::media_list_tile_size + 4;
    cut_assert_false(list_tree->for_each(list_id, ID::Item(offset),
                                         4,
                                         callback).failed());
    mock_messages->check();

    /* iterate over just one more to trigger sliding */
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "slide down to index 100");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "materialize adjacent tile around index 108");
    count = 0;
    offset = base_offset + 2 * UPnP::media_list_tile_size + 4;
    cut_assert_false(list_tree->for_each(list_id, ID::Item(offset),
                                         5,
                                         callback).failed());
    mock_messages->check();

    /* iterate over all items that should be in cache now */
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "no need to prefetch index 88, already in cache");
    count = 0;
    offset = base_offset + UPnP::media_list_tile_size;
    cut_assert_false(list_tree->for_each(list_id, ID::Item(offset),
                                         3 * UPnP::media_list_tile_size,
                                         callback).failed());

    filler.check();
}

/*!\test
 * Iterating over a range that starts in a cached range and end two tiles below
 * the down tile fetches only what's necessary.
 */
void test_iterating_over_small_overlapping_ranges_causes_minimal_upnp_traffic()
{
    ItemGenerator filler(500, false, 5 * UPnP::media_list_tile_size);
    ID::List list_id = prepare_enter_server_test(filler);

    static constexpr size_t base_offset = 10 * UPnP::media_list_tile_size;

    size_t count;
    size_t offset;

    auto callback = [&count, &offset] (const ListTreeIface::ForEachItemDataGeneric &data)
    {
        std::ostringstream os;
        os << "Generated item " << count + offset;

        cppcut_assert_equal(os.str(), data.name_);
        cut_assert_false(data.kind_.is_directory());

        ++count;

        return true;
    };

    /* fill three tiles */
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "prefetch 24 items, starting at index 80");
    count = 0;
    offset = base_offset;
    cut_assert_false(list_tree->for_each(list_id, ID::Item(offset),
                                         3 * UPnP::media_list_tile_size,
                                         callback).failed());
    mock_messages->check();

    /* iterate over ten items, nine of them beyond the down tile */
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "slide down to index 103");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "materialize adjacent tile around index 111");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "materialize adjacent tile around index 119");
    count = 0;
    offset = base_offset + 3 * UPnP::media_list_tile_size - 1;
    cut_assert_false(list_tree->for_each(list_id, ID::Item(offset),
                                         10,
                                         callback).failed());
    mock_messages->check();

    /* iterate over all items that should be in cache now */
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "no need to prefetch index 96, already in cache");
    count = 0;
    offset = base_offset + 2 * UPnP::media_list_tile_size;
    cut_assert_false(list_tree->for_each(list_id, ID::Item(offset),
                                         3 * UPnP::media_list_tile_size,
                                         callback).failed());

    filler.check();
}

/*!\test
 * When iterating over a range whose last elements are already the two upper
 * tiles in cache, then the cached items are not fetched again.
 */
void test_iterate_from_top_into_big_overlapping_range_causes_minimal_upnp_traffic()
{
    ItemGenerator filler(500, false, 4 * UPnP::media_list_tile_size);
    ID::List list_id = prepare_enter_server_test(filler);

    static constexpr size_t base_offset = 20 * UPnP::media_list_tile_size;

    size_t count;
    size_t offset;

    auto callback = [&count, &offset] (const ListTreeIface::ForEachItemDataGeneric &data)
    {
        std::ostringstream os;
        os << "Generated item " << count + offset;

        cppcut_assert_equal(os.str(), data.name_);
        cut_assert_false(data.kind_.is_directory());

        ++count;

        return true;
    };

    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "prefetch 24 items, starting at index 160");
    count = 0;
    offset = base_offset;
    cut_assert_false(list_tree->for_each(list_id, ID::Item(offset),
                                         3 * UPnP::media_list_tile_size,
                                         callback).failed());
    mock_messages->check();

    /* iterate over two items with one of them just above the up tile */
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "slide up to index 159");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "materialize adjacent tile around index 159");
    count = 0;
    offset = base_offset - 1;
    cut_assert_false(list_tree->for_each(list_id, ID::Item(offset),
                                         2,
                                         callback).failed());
    mock_messages->check();

    /* iterate over supposedly cached items */
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "no need to prefetch index 152, already in cache");
    count = 0;
    offset = base_offset - UPnP::media_list_tile_size;
    cut_assert_false(list_tree->for_each(list_id, ID::Item(offset),
                                         3 * UPnP::media_list_tile_size,
                                         callback).failed());

    filler.check();
}

/*!\test
 * When iterating over a range whose last elements are already in the up tile,
 * then the cached items are not fetched again.
 */
void test_iterate_from_top_into_small_overlapping_range_causes_minimal_upnp_traffic()
{
    ItemGenerator filler(500, false, 5 * UPnP::media_list_tile_size);
    ID::List list_id = prepare_enter_server_test(filler);

    static constexpr size_t base_offset = 20 * UPnP::media_list_tile_size;

    size_t count;
    size_t offset;

    auto callback = [&count, &offset] (const ListTreeIface::ForEachItemDataGeneric &data)
    {
        std::ostringstream os;
        os << "Generated item " << count + offset;

        cppcut_assert_equal(os.str(), data.name_);
        cut_assert_false(data.kind_.is_directory());

        ++count;

        return true;
    };

    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "prefetch 24 items, starting at index 160");
    count = 0;
    offset = base_offset;
    cut_assert_false(list_tree->for_each(list_id, ID::Item(offset),
                                         3 * UPnP::media_list_tile_size,
                                         callback).failed());
    mock_messages->check();

    /* iterate over 10 items with nine of them just above the up tile */
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "slide up to index 151");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "materialize adjacent tile around index 159");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "materialize adjacent tile around index 151");
    count = 0;
    offset = base_offset - UPnP::media_list_tile_size - 1;
    cut_assert_false(list_tree->for_each(list_id, ID::Item(offset),
                                         UPnP::media_list_tile_size + 2,
                                         callback).failed());
    mock_messages->check();

    /* iterate over supposedly cached items */
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "no need to prefetch index 144, already in cache");
    count = 0;
    offset = base_offset - 2 * UPnP::media_list_tile_size;
    cut_assert_false(list_tree->for_each(list_id, ID::Item(offset),
                                         3 * UPnP::media_list_tile_size,
                                         callback).failed());

    filler.check();
}

/*!\test
 * Materialize level 2 in tree hierarchy, then use list on level 1.
 *
 * This is a very simple and straightforward test. It was made in an attempt to
 * simulate a situation that causes an assertion to fail in LRU code.
 *
 * This test, however, did not fail the assertion because of unspecified
 * evaluation order of operator==. If the comparison would have been written
 * the other way around or if the compiler settings would have been changed,
 * then this test would have failed (I actually checked this).
 */
void test_get_list_id_of_internal_node()
{
    mock_timebase.set_auto_increment();

    static const std::vector<std::string> servers =
    {
        std::string("/test/server/1"),
        std::string("/test/server/2"),
    };

    add_servers(servers, list_tree->get_root_list_id());

    ItemGenerator filler(5, true, UPnP::media_list_tile_size);
    filler.expect(1 * 5, 1 * UPnP::media_list_tile_size);

    prepare_enter_server_test(filler, 3);

    ListError error;

    /* query root node */
    const ID::List root_id = list_tree->get_root_list_id();
    cut_assert_true(root_id.is_valid());
    cut_assert_true(list_tree->use_list(root_id, false));

    mock_timebase.step();

    /* get ID of first entry in root node */
    cut_assert_true(list_tree->use_list(root_id, false));
    const ID::List child_1_id = list_tree->enter_child(root_id, ID::Item(0), error);
    cut_assert_true(child_1_id.is_valid());
    cut_assert_false(error.failed());

    mock_timebase.step();

    /* get ID of first entry in first child node */
    cut_assert_true(list_tree->use_list(child_1_id, false));
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "prefetch 1 items, starting at index 0");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DIAG,
                                              "D-Bus path of new list is dbus-5-0");
    mock_dbus_upnp_helpers->expect_get_size_of_container(3, "dbus-5-0");
    const ID::List child_2_id = list_tree->enter_child(child_1_id, ID::Item(0), error);
    cut_assert_true(child_2_id.is_valid());
    cut_assert_false(error.failed());

    mock_timebase.step();

    /* get ID of first entry in root node again */
    cut_assert_true(list_tree->use_list(root_id, false));
    const ID::List child_1_id_again = list_tree->enter_child(root_id, ID::Item(0), error);
    cppcut_assert_equal(child_1_id.get_raw_id(), child_1_id_again.get_raw_id());
    cut_assert_false(error.failed());

    filler.check();
}

/*!\test
 * Entering server entry marked as cacheable never reloads server's root list.
 */
void test_enter_cacheable_server_list_never_reloads_server_root_list()
{
    static constexpr const size_t SERVER_ROOT_SIZE = 7;

    /* query root node */
    const ID::List root_id = list_tree->get_root_list_id();
    cut_assert_true(root_id.is_valid());
    cut_assert_false(root_id.get_nocache_bit());

    ItemGenerator filler(SERVER_ROOT_SIZE, false, 0);
    current_tiled_list_filler = &filler;

    ListError error;

    /* get ID of first entry in root node */
    mock_dbus_upnp_helpers->expect_get_proxy_object_path_callback(FakeDBus::get_proxy_object_path);
    mock_dbus_upnp_helpers->expect_get_size_of_container(filler.size(), "/test/server/0");

    const ID::List child_id_first = list_tree->enter_child(root_id, ID::Item(0), error);
    cut_assert_false(error.failed());
    cppcut_assert_equal(filler.size(),
                        std::static_pointer_cast<UPnP::MediaList>(cache->lookup(child_id_first))->size());
    cut_assert_true(child_id_first.is_valid());
    cut_assert_false(child_id_first.get_nocache_bit());
    filler.check();

    filler.expect(SERVER_ROOT_SIZE, UPnP::media_list_tile_size);
    const auto child_list_first = std::static_pointer_cast<const UPnP::MediaList>(cache->lookup(child_id_first));
    cppcut_assert_not_null(child_list_first.get());
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "prefetch 1 items, starting at index 0");
    (void)(*child_list_first)[ID::Item(0)];
    filler.check();

    /* get ID of same entry again, same ID because it is served from cache */
    const ID::List child_id_again = list_tree->enter_child(root_id, ID::Item(0), error);
    cut_assert_false(error.failed());
    cut_assert_true(child_id_again.is_valid());
    cut_assert_false(child_id_again.get_nocache_bit());
    filler.check();

    cppcut_assert_equal(child_id_again.get_raw_id(), child_id_first.get_raw_id());

    const auto child_list_again = std::static_pointer_cast<const UPnP::MediaList>(cache->lookup(child_id_again));
    cppcut_assert_not_null(child_list_again.get());
    cut_assert_true(child_list_first == child_list_again);
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "no need to prefetch index 0, already in cache");
    (void)(*child_list_again)[ID::Item(0)];
    filler.check();
}

/*!\test
 * Entering server entry marked as non-cacheable always reloads server's root
 * list.
 */
void test_enter_noncacheable_server_list_always_reloads_server_root_list()
{
    static constexpr const size_t SERVER_ROOT_SIZE = 7;

    mock_timebase.set_auto_increment();
    list_tree->set_default_lru_cache_mode(LRU::CacheModeRequest::UNCACHED);

    /* query root node */
    const ID::List root_id = list_tree->get_root_list_id();
    cut_assert_true(root_id.is_valid());
    cut_assert_false(root_id.get_nocache_bit());
    cut_assert_true(list_tree->use_list(root_id, false));

    ItemGenerator filler(SERVER_ROOT_SIZE, false, 0);
    current_tiled_list_filler = &filler;

    ListError error;

    /* get ID of first entry in root node */
    mock_dbus_upnp_helpers->expect_get_proxy_object_path_callback(FakeDBus::get_proxy_object_path);
    mock_dbus_upnp_helpers->expect_get_size_of_container(filler.size(), "/test/server/0");

    const ID::List child_id_first = list_tree->enter_child(root_id, ID::Item(0), error);
    cut_assert_false(error.failed());
    cppcut_assert_equal(filler.size(),
                        std::static_pointer_cast<UPnP::MediaList>(cache->lookup(child_id_first))->size());
    cut_assert_true(child_id_first.is_valid());
    cut_assert_true(child_id_first.get_nocache_bit());
    filler.check();

    filler.expect(SERVER_ROOT_SIZE, UPnP::media_list_tile_size);
    const auto child_list_first = std::static_pointer_cast<const UPnP::MediaList>(cache->lookup(child_id_first));
    cut_assert_true(list_tree->use_list(child_id_first, false));
    cppcut_assert_not_null(child_list_first.get());
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "prefetch 1 items, starting at index 0");
    (void)(*child_list_first)[ID::Item(0)];
    filler.check();

    /* get ID of same entry again, different ID because non-cacheable */
    mock_dbus_lists_iface->expect_dbus_lists_get_navigation_iface(dbus_lists_navigation_iface_dummy);
    mock_listbrokers_dbus->expect_tdbus_lists_navigation_emit_list_invalidate(
            dbus_lists_navigation_iface_dummy,
            child_id_first.get_raw_id(), child_id_first.get_raw_id() + 1);
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "Purge entry %u", child_id_first.get_raw_id());
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_IMPORTANT, buffer);

    mock_dbus_upnp_helpers->expect_get_proxy_object_path_callback(FakeDBus::get_proxy_object_path);
    mock_dbus_upnp_helpers->expect_get_size_of_container(filler.size(), "/test/server/0");

    const ID::List child_id_again = list_tree->enter_child(root_id, ID::Item(0), error);
    cut_assert_false(error.failed());
    cut_assert_true(child_id_again.is_valid());
    cut_assert_true(child_id_again.get_nocache_bit());
    filler.check();

    cppcut_assert_not_equal(child_id_again.get_raw_id(), child_id_first.get_raw_id());
    cppcut_assert_equal(child_id_again.get_raw_id(), child_id_first.get_raw_id() + 1);

    filler.expect(SERVER_ROOT_SIZE, UPnP::media_list_tile_size);
    const auto child_list_again = std::static_pointer_cast<const UPnP::MediaList>(cache->lookup(child_id_again));
    cut_assert_true(list_tree->use_list(child_id_again, false));
    cppcut_assert_not_null(child_list_again.get());
    cut_assert_false(child_list_first == child_list_again);
    cppcut_assert_null(cache->lookup(child_id_first).get());
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "prefetch 1 items, starting at index 0");
    (void)(*child_list_again)[ID::Item(0)];
    filler.check();
}

};

/*!@}*/

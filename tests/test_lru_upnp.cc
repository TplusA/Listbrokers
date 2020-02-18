/*
 * Copyright (C) 2015--2020  T+A elektroakustik GmbH & Co. KG
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
#include <tuple>
#include <stack>

#include "mock_messages.hh"
#include "mock_backtrace.hh"
#include "mock_dbus_upnp_helpers.hh"
#include "mock_upnp_dleynaserver_dbus.hh"
#include "mock_timebase.hh"
#include "fake_dbus.hh"

#include "upnp_list.hh"

/*!
 * \addtogroup upnp_lru_tests Unit tests
 * \ingroup upnp
 *
 * Unit tests for cached lists of UPnP media.
 */
/*!@{*/

/* for use with enter_child() */
static bool always_continue() { return true; }

/* for use with enter_child() */
static bool always_use_cached(ID::List id) { return id.is_valid(); }

/* for use with enter_child() */
static ID::List purge_dummy(ID::List old_id, ID::List new_id,
                            const EnterChild::SetNewRoot &set_root)
{
    cut_assert_true(set_root != nullptr);
    set_root(old_id, new_id);
    return new_id;
}

static MockTimebase mock_timebase;
Timebase *LRU::timebase = &mock_timebase;

class ListTreeIface { static const std::string empty_string; };
const std::string ListTreeIface::empty_string;

namespace upnp_lists_lru_tests
{

static MockMessages *mock_messages;
static MockBacktrace *mock_backtrace;
static MockDBusUPnPHelpers *mock_dbus_upnp_helpers;
static MockDleynaServerDBus *mock_dleynaserver_dbus;

static LRU::Cache *cache;
static constexpr size_t maximum_number_of_objects = 100;

void cut_setup()
{
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

    mock_dleynaserver_dbus = new MockDleynaServerDBus;
    cppcut_assert_not_null(mock_dleynaserver_dbus);
    mock_dleynaserver_dbus->init();
    mock_dleynaserver_dbus_singleton = mock_dleynaserver_dbus;

    mock_messages->ignore_messages_with_level_or_above(MESSAGE_LEVEL_TRACE);

    mock_timebase.reset();

    cache = new LRU::Cache(200000, maximum_number_of_objects,
                           std::chrono::minutes(1));
    cppcut_assert_not_null(cache);
    cache->set_callbacks([]{}, []{}, [] (ID::List id) {}, []{});
    cppcut_assert_equal(size_t(0), cache->count());

    UPnP::MediaList::start_threads(1, true);
}

void cut_teardown()
{
    UPnP::MediaList::shutdown_threads();

    delete cache;
    cache = nullptr;

    mock_dbus_upnp_helpers->check();
    mock_dbus_upnp_helpers_singleton = nullptr;
    delete mock_dbus_upnp_helpers;
    mock_dbus_upnp_helpers = nullptr;

    mock_dleynaserver_dbus->check();
    mock_dleynaserver_dbus_singleton = nullptr;
    delete mock_dleynaserver_dbus;
    mock_dleynaserver_dbus = nullptr;

    mock_backtrace->check();
    mock_backtrace_singleton = nullptr;
    delete mock_backtrace;
    mock_backtrace = nullptr;

    mock_messages->check();
    mock_messages_singleton = nullptr;
    delete mock_messages;
    mock_messages = nullptr;

    FakeDBus::all_proxies.clear();
    FakeDBus::proxy_extra_refs.clear();
}

/*!
 * Helper function template for inserting a new server into the list.
 *
 * \param root
 *     Pointer to the root list (list of UPnP servers) into which the new
 *     server is going to be inserted.
 *
 * \param path
 *     Name of the UPnP on D-Bus.
 *
 * \param expect_new_entry
 *     Expected outcome of #UPnP::ServerList::add_to_list().
 *
 * \param model_name
 *     Model name of the UPnP device. Used for quirks detection.
 */
static void insert_upnp_server(std::shared_ptr<UPnP::ServerList> root,
                               const char *path,
                               bool expect_new_entry = true,
                               const char *model_name = nullptr)
{
    mock_dbus_upnp_helpers->expect_create_media_device_proxy_for_object_path_begin_callback(FakeDBus::create_proxy_begin);

    const size_t old_root_size = root->size();

    if(old_root_size > 0)
        mock_dbus_upnp_helpers->expect_proxy_object_path_equals_callback(FakeDBus::compare_proxy_object_path, root->size());

    if(expect_new_entry)
    {
        mock_dbus_upnp_helpers->expect_is_media_device_usable(true,
            reinterpret_cast<tdbusdleynaserverMediaDevice *>(FakeDBus::next_proxy_id));
        mock_dleynaserver_dbus->expect_tdbus_dleynaserver_media_device_get_model_name(
            model_name != nullptr ? model_name : "",
            reinterpret_cast<tdbusdleynaserverMediaDevice *>(FakeDBus::next_proxy_id));
    }
    else
    {
        char buffer[512];
        snprintf(buffer, sizeof(buffer), "Updating already known UPnP server %s", path);
        mock_messages->expect_msg_info_formatted(buffer);
    }

    root->add_to_list(path, nullptr);

    if(expect_new_entry)
        cppcut_assert_equal(old_root_size + 1, root->size());
    else
        cppcut_assert_equal(old_root_size, root->size());
}

/*!\test
 * Insertion of several UPnP servers into the server list.
 */
void test_insert_upnp_servers_into_list()
{
    auto root = std::make_shared<UPnP::ServerList>(nullptr);
    cppcut_assert_not_null(root.get());

    static const char *paths[] =
    {
        "/com/intel/dLeynaServer/server/0",
        "/com/intel/dLeynaServer/server/1",
        "/com/intel/dLeynaServer/server/2",
        "/com/intel/dLeynaServer/server/3",
    };

    insert_upnp_server(root, paths[0]);
    insert_upnp_server(root, paths[1], true, "This one has a model name");
    insert_upnp_server(root, paths[2]);
    insert_upnp_server(root, paths[3]);
    cppcut_assert_equal(4LU, FakeDBus::all_proxies.size());
    cut_assert_true(FakeDBus::proxy_extra_refs.empty());
}

/*!\test
 * UPnP servers with same path cannot exist, but the D-Bus proxy to the server
 * may have to be updated.
 */
void test_insert_same_upnp_server_into_list_twice_updates_dbus_proxy()
{
    auto root = std::make_shared<UPnP::ServerList>(nullptr);
    cppcut_assert_not_null(root.get());

    static const char *paths[] =
    {
        "/com/intel/dLeynaServer/server/0",
        "/com/intel/dLeynaServer/server/1",
    };

    insert_upnp_server(root, paths[0]);
    insert_upnp_server(root, paths[1]);
    insert_upnp_server(root, paths[1], false);
    cppcut_assert_equal(2LU, FakeDBus::all_proxies.size());
    cut_assert_true(FakeDBus::proxy_extra_refs.empty());
}

static std::shared_ptr<UPnP::ServerList> make_list_for_remove_tests()
{
    auto root = std::make_shared<UPnP::ServerList>(nullptr);
    cppcut_assert_not_null(root.get());

    static const char *paths[] =
    {
        "/com/intel/dLeynaServer/server/0",
        "/com/intel/dLeynaServer/server/1",
        "/com/intel/dLeynaServer/server/2",
    };

    insert_upnp_server(root, paths[0]);
    insert_upnp_server(root, paths[1]);
    insert_upnp_server(root, paths[2]);
    cppcut_assert_equal(3LU, FakeDBus::all_proxies.size());
    cut_assert_true(FakeDBus::proxy_extra_refs.empty());

    return root;
}

/*!\test
 * Removal of an existing UPnP server from the server list.
 */
void test_remove_upnp_server_from_list()
{
    auto root = make_list_for_remove_tests();
    const auto expected_number_of_servers = root->size() - 1;

    auto it = FakeDBus::all_proxies.find(100);
    cppcut_assert_equal(1L, it->second.use_count());
    it = FakeDBus::all_proxies.find(101);
    cppcut_assert_equal(1L, it->second.use_count());
    it = FakeDBus::all_proxies.find(102);
    cppcut_assert_equal(1L, it->second.use_count());

    mock_dbus_upnp_helpers->expect_proxy_object_path_equals_callback(FakeDBus::compare_proxy_object_path, 2);
    ID::List id;
    cut_assert(root->remove_from_list("/com/intel/dLeynaServer/server/1", id) == UPnP::ServerList::RemoveFromListResult::REMOVED);
    cut_assert_false(id.is_valid());
    cppcut_assert_equal(expected_number_of_servers, root->size());

    cut_assert_true(FakeDBus::proxy_extra_refs.empty());
    it = FakeDBus::all_proxies.find(100);
    cppcut_assert_equal(1L, it->second.use_count());
    it = FakeDBus::all_proxies.find(101);
    cut_assert(it == FakeDBus::all_proxies.end());
    it = FakeDBus::all_proxies.find(102);
    cppcut_assert_equal(1L, it->second.use_count());
}

/*!\test
 * Removal of a UPnP server not in the server list fails.
 */
void test_remove_nonexistent_upnp_server_from_list_fails()
{
    auto root = make_list_for_remove_tests();
    const auto expected_number_of_servers = root->size();

    static const char *nonexistent_paths[] =
    {
        "/com/intel/dLeynaServer/server",
        "/com/intel/dLeynaServer/server/",
        "/com/intel/dLeynaServer/server/10",
        "/com/intel/dLeynaServer/server/0 ",
        " /com/intel/dLeynaServer/server/0",
        "com/intel/dLeynaServer/server/0",
        "/intel/dLeynaServer/server/0",
    };

    for(auto p : nonexistent_paths)
    {
        mock_dbus_upnp_helpers->expect_proxy_object_path_equals_callback(FakeDBus::compare_proxy_object_path, root->size());
        ID::List id;
        cut_assert(root->remove_from_list(p, id) == UPnP::ServerList::RemoveFromListResult::NOT_FOUND);
        cut_assert_false(id.is_valid());
        cppcut_assert_equal(expected_number_of_servers, root->size());
    }

    cppcut_assert_equal(3LU, FakeDBus::all_proxies.size());
    cut_assert_true(FakeDBus::proxy_extra_refs.empty());
}

class DummyFiller: public TiledListFillerIface<UPnP::ItemData>
{
  public:
    DummyFiller(const DummyFiller &) = delete;
    DummyFiller &operator=(const DummyFiller &) = delete;

    explicit DummyFiller() {}
    virtual ~DummyFiller() {}

    ssize_t fill(ItemProvider<UPnP::ItemData> &item_provider, ID::List list_id,
                 ID::Item idx, size_t count, ListError &error,
                 const std::function<bool()> &may_continue) const override
    {
        error = ListError::OK;
        return count;
    }
};

/*!\test
 * No memory is leaking during garbage collection.
 *
 * This test must run under Valgrind to be meaningful.
 *
 * \todo
 *     Test triggers three bug reports, and rightly so because the test is kind
 *     of buggy. It does fails to inform the list implementations about the
 *     hierarchy built up in here, so the list objects are complaining when
 *     their children are garbage collected. Neither will the test crash nor
 *     will the tested code do anything wrong here, but still the test should
 *     be fixed to use the proper interfaces for building list hierarchies.
 */
void test_no_memory_leaks()
{
    DummyFiller filler;

    auto root               = std::make_shared<UPnP::ServerList>(nullptr);
    cppcut_assert_not_null(root.get());
    auto first_level        = std::make_shared<UPnP::MediaList>(root, 10, filler);
    cppcut_assert_not_null(first_level.get());
    auto second_level_left  = std::make_shared<UPnP::MediaList>(first_level, 20, filler);
    cppcut_assert_not_null(second_level_left.get());
    auto second_level_right = std::make_shared<UPnP::MediaList>(first_level, 20, filler);
    cppcut_assert_not_null(second_level_right.get());

    cppcut_assert_equal(2L, root.use_count());
    cppcut_assert_equal(3L, first_level.use_count());
    cppcut_assert_equal(1L, second_level_left.use_count());
    cppcut_assert_equal(1L, second_level_right.use_count());

    insert_upnp_server(root, "/com/intel/dLeynaServer/server/0");

    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "prefetch 1 items, starting at index 5");
    (void)(*first_level)[ID::Item(5)];

    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "prefetch 1 items, starting at index 7");
    (void)(*second_level_left)[ID::Item(7)];

    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "prefetch 1 items, starting at index 11");
    (void)(*second_level_right)[ID::Item(11)];

    ID::List id;

    id = cache->insert(root, LRU::CacheMode::CACHED, 0, 100);
    cut_assert_true(id.is_valid());
    id = cache->insert(first_level, LRU::CacheMode::CACHED, 0, 150);
    cut_assert_true(id.is_valid());
    id = cache->insert(second_level_left, LRU::CacheMode::CACHED, 0, 200);
    cut_assert_true(id.is_valid());
    id = cache->insert(second_level_right, LRU::CacheMode::CACHED, 0, 250);
    cut_assert_true(id.is_valid());

    cppcut_assert_equal(3L, root.use_count());
    cppcut_assert_equal(4L, first_level.use_count());
    cppcut_assert_equal(2L, second_level_left.use_count());
    cppcut_assert_equal(2L, second_level_right.use_count());

    std::chrono::seconds duration = cache->gc();
    mock_timebase.step(std::chrono::milliseconds(duration).count());

    /* we get a bunch of bug reports here because we are not really associating
     * the lists with each other by content, only by cache hierarchy; to get
     * rid of them, #ServerList::enter_child() and #MediaList::enter_child()
     * would have to be used */
    mock_messages->expect_msg_error_formatted(
        0, LOG_CRIT,
        "BUG: Got obliviate notification for child 3, but could not find it in list with ID 2");
    mock_backtrace->expect_backtrace_log();
    mock_messages->expect_msg_error_formatted(
        0, LOG_CRIT,
        "BUG: Got obliviate notification for child 4, but could not find it in list with ID 2");
    mock_backtrace->expect_backtrace_log();
    mock_messages->expect_msg_error_formatted(
        0, LOG_CRIT,
        "BUG: Got obliviate notification for server root 2, but could not find it in server list (ID 1)");
    mock_backtrace->expect_backtrace_log();
    cache->gc();

    cppcut_assert_equal(2L, root.use_count());
    cppcut_assert_equal(3L, first_level.use_count());
    cppcut_assert_equal(1L, second_level_left.use_count());
    cppcut_assert_equal(1L, second_level_right.use_count());
}

/*!\test
 * Enumerating empty server list yields list containing the ID of the empty
 * server list.
 */
void test_enumerate_empty_server_list_replace()
{
    auto root = std::make_shared<UPnP::ServerList>(nullptr);
    cppcut_assert_not_null(root.get());
    cache->insert(root, LRU::CacheMode::CACHED, 0, 10);

    std::vector<ID::List> nodes;

    nodes.push_back(ID::List(10));
    nodes.push_back(ID::List(20));

    root->enumerate_tree_of_sublists(*cache, nodes);

    cppcut_assert_equal(size_t(1), nodes.size());
    cppcut_assert_equal(root->get_cache_id().get_raw_id(), nodes[0].get_raw_id());
}

/*!\test
 * Enumerating empty server list yields list containing the ID of the empty
 * server list.
 */
void test_enumerate_empty_server_list_append()
{
    auto root = std::make_shared<UPnP::ServerList>(nullptr);
    cppcut_assert_not_null(root.get());
    cache->insert(root, LRU::CacheMode::CACHED, 0, 10);

    std::vector<ID::List> nodes;

    nodes.push_back(ID::List(10));
    nodes.push_back(ID::List(20));

    root->enumerate_tree_of_sublists(*cache, nodes, true);

    cppcut_assert_equal(size_t(3), nodes.size());
    cppcut_assert_equal(10U, nodes[0].get_raw_id());
    cppcut_assert_equal(20U, nodes[1].get_raw_id());
    cppcut_assert_equal(root->get_cache_id().get_raw_id(), nodes[2].get_raw_id());
}

/*!\test
 * Enumerating single server yields list containing the IDs of the server list
 * and the server's root directory list.
 */
void test_enumerate_server_list_with_single_entry()
{
    auto root = std::make_shared<UPnP::ServerList>(nullptr);
    cppcut_assert_not_null(root.get());
    insert_upnp_server(root, "server");
    cache->insert(root, LRU::CacheMode::CACHED, 0, 10);

    DummyFiller filler;
    ListError error;

    mock_dbus_upnp_helpers->expect_get_proxy_object_path(
        "server",
        reinterpret_cast<tdbusdleynaserverMediaDevice *>(100));
    mock_dbus_upnp_helpers->expect_get_size_of_container(5, "server");
    root->enter_child(*cache, LRU::CacheModeRequest::AUTO,
                      ID::Item(0), filler,
                      always_continue, always_use_cached, purge_dummy, error);

    std::vector<ID::List> nodes;
    root->enumerate_tree_of_sublists(*cache, nodes);

    cppcut_assert_equal(size_t(2), nodes.size());
    cppcut_assert_equal(root->get_cache_id().get_raw_id(), nodes[0].get_raw_id());
    cppcut_assert_equal(2U, nodes[1].get_raw_id());
}

/*!\test
 * Enumerating server list yields list containing the IDs of the server list
 * and of the servers' complete directory hierarchies.
 */
void test_enumerate_server_list()
{
    auto root = std::make_shared<UPnP::ServerList>(nullptr);
    cppcut_assert_not_null(root.get());

    static const char *paths[] =
    {
        "server 0",
        "server 1",
        "server 2",
        "server 3",
    };

    for(const auto p : paths)
        insert_upnp_server(root, p);

    cache->insert(root, LRU::CacheMode::CACHED, 0, 40);

    DummyFiller filler;
    ListError error;

    for(size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i)
    {
        mock_dbus_upnp_helpers->expect_get_proxy_object_path(
            paths[i],
            reinterpret_cast<tdbusdleynaserverMediaDevice *>(100 + i));
        mock_dbus_upnp_helpers->expect_get_size_of_container(5, paths[i]);
        root->enter_child(*cache, LRU::CacheModeRequest::AUTO,
                          ID::Item(i), filler,
                          always_continue, always_use_cached, purge_dummy, error);
    }

    std::vector<ID::List> nodes;
    root->enumerate_tree_of_sublists(*cache, nodes);

    cppcut_assert_equal(sizeof(paths) / sizeof(paths[0]) + 1, nodes.size());
    cppcut_assert_equal(root->get_cache_id().get_raw_id(), nodes[0].get_raw_id());

    for(size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i)
        cppcut_assert_equal(uint32_t(i + 2), nodes[i + 1].get_raw_id());
}

struct RawItem
{
    constexpr RawItem(const RawItem &) = delete;
    constexpr RawItem(RawItem &&) = default;
    constexpr RawItem &operator=(const RawItem &) = delete;
    constexpr RawItem &operator=(RawItem &&) = delete;

    const char *const name_;
    const char *const dbus_name_;
    const char *const album_art_url_;
    const UPnP::MediaList::Type type_;
    const size_t child_item_;

    constexpr explicit RawItem(const char *const name,
                               const char *const dbus_name,
                               const UPnP::MediaList::Type type,
                               const size_t child_item,
                               const char *const album_art_url = nullptr):
        name_(name),
        dbus_name_(dbus_name),
        album_art_url_(album_art_url),
        type_(type),
        child_item_(child_item)
    {}
};

class EnumerateMediaListFiller: public TiledListFillerIface<UPnP::ItemData>
{
  private:
    const RawItem *const items_;
    const size_t count_;

  public:
    EnumerateMediaListFiller(EnumerateMediaListFiller &&) = default;
    EnumerateMediaListFiller &operator=(EnumerateMediaListFiller &&) = default;
    EnumerateMediaListFiller(const EnumerateMediaListFiller &) = default;
    EnumerateMediaListFiller &operator=(const EnumerateMediaListFiller &) = delete;

    explicit EnumerateMediaListFiller(const RawItem *items, size_t count):
        items_(items),
        count_(count)
    {}

    template <size_t N>
    explicit EnumerateMediaListFiller(const std::array<const RawItem, N> &items):
        items_(items.data()),
        count_(N)
    {}

    virtual ~EnumerateMediaListFiller() {}

    ssize_t fill(ItemProvider<UPnP::ItemData> &item_provider, ID::List list_id,
                 ID::Item idx, size_t count, ListError &error,
                 const std::function<bool()> &may_continue) const override
    {
        error = ListError::OK;

        for(size_t i = 0; i < count; ++i)
        {
            if(idx.get_raw_id() >= count_)
                return i;

            const RawItem &it = items_[idx.get_raw_id()];

            *item_provider.next() =
                std::move(UPnP::ItemData(std::move(std::string("/de/tahifi/unittests/23/") + it.dbus_name_),
                                         it.name_,
                                         std::move(it.album_art_url_ != nullptr
                                                   ? Url::String(Url::Sensitivity::GENERIC,
                                                                 it.album_art_url_)
                                                   : Url::String(Url::Sensitivity::GENERIC)),
                                         it.type_ == UPnP::MediaList::Type::SUBDIRECTORY));

            idx = ID::Item(idx.get_raw_id() + 1);
        }

        return count;
    }
};

/*!\test
 * Enumeration of empty media list yields list containing only the list itself.
 */
void test_enumerate_empty_media_list()
{
    EnumerateMediaListFiller empty_filler(nullptr, 0);

    auto list = std::make_shared<UPnP::MediaList>(nullptr, 0, empty_filler);
    cut_assert_true(cache->insert(list, LRU::CacheMode::CACHED, 0, 10).is_valid());

    std::vector<ID::List> nodes;
    list->enumerate_tree_of_sublists(*cache, nodes);

    cppcut_assert_equal(size_t(1), nodes.size());
    cut_assert_true(list->get_cache_id() == nodes[0]);
}

static constexpr std::array<const RawItem, 3> media_list_items_child =
{
    /* 0 */
    RawItem("1st below root",      "0001",     UPnP::MediaList::Type::SUBDIRECTORY, 1),
    RawItem("2nd below root",      "0002",     UPnP::MediaList::Type::SUBDIRECTORY, 2),
    RawItem("3rd below root",      "0003",     UPnP::MediaList::Type::SUBDIRECTORY, 3),
};

static constexpr std::array<const RawItem, 8> media_list_items_child_1 =
{
    /* 1 */
    RawItem("Dummy 1.0",           "0101.mp3", UPnP::MediaList::Type::AUDIO,        0,
            "http://coverart.de/dummy1_0"),
    RawItem("Dummy 1.1",           "0102.mp3", UPnP::MediaList::Type::AUDIO,        0,
            "http://coverart.de/dummy1_1"),
    RawItem("1st below child/1",   "0103",     UPnP::MediaList::Type::SUBDIRECTORY, 0),
    RawItem("Dummy 1.3",           "0104.txt", UPnP::MediaList::Type::MISC,         0),
    RawItem("Dummy 1.4",           "0105.mp3", UPnP::MediaList::Type::AUDIO,        0),
    RawItem("2nd below child/1",   "0106",     UPnP::MediaList::Type::SUBDIRECTORY, 0),
    RawItem("3rd below child/1",   "0107",     UPnP::MediaList::Type::SUBDIRECTORY, 0),
    RawItem("Dummy 1.7",           "0108.txt", UPnP::MediaList::Type::MISC,         0),
};

static constexpr std::array<const RawItem, 5> media_list_items_child_2 =
{
    /* 2 */
    RawItem("1st below child/2",   "0201",     UPnP::MediaList::Type::SUBDIRECTORY, 0),
    RawItem("Dummy 2.1",           "0202.mp3", UPnP::MediaList::Type::AUDIO,        0),
    RawItem("2nd below child/2",   "0203",     UPnP::MediaList::Type::SUBDIRECTORY, 4),
    RawItem("Dummy 2.3",           "0204.mp3", UPnP::MediaList::Type::AUDIO,        0,
            "http://coverart.de/dummy2_3"),
    RawItem("3rd below child/2",   "0205",     UPnP::MediaList::Type::SUBDIRECTORY, 0),
};

static constexpr std::array<const RawItem, 5> media_list_items_child_3 =
{
    /* 3 */
    RawItem("1st below child/3",   "0301",     UPnP::MediaList::Type::SUBDIRECTORY, 0),
    RawItem("2nd below child/3",   "0302",     UPnP::MediaList::Type::SUBDIRECTORY, 0),
    RawItem("Dummy 3.2",           "0303.txt", UPnP::MediaList::Type::MISC,         0),
    RawItem("Dummy 3.3",           "0304.mp3", UPnP::MediaList::Type::AUDIO,        0),
    RawItem("3rd below child/3",   "0305",     UPnP::MediaList::Type::SUBDIRECTORY, 0),
};

static constexpr std::array<const RawItem, 3> media_list_items_child_2_1 =
{
    /* 4 */
    RawItem("1st below child/2/2", "2301",     UPnP::MediaList::Type::SUBDIRECTORY, 0),
    RawItem("2nd below child/2/2", "2302",     UPnP::MediaList::Type::SUBDIRECTORY, 0),
    RawItem("3rd below child/2/2", "2303",     UPnP::MediaList::Type::SUBDIRECTORY, 0),
};

static const std::array<const std::tuple<const RawItem *const, const size_t, const EnumerateMediaListFiller>, 5> media_list_items_tree =
{
#define MK_ENTRY(V) \
    std::move(std::make_tuple(V.data(), V.size(), std::move(EnumerateMediaListFiller(V))))

    MK_ENTRY(media_list_items_child),
    MK_ENTRY(media_list_items_child_1),
    MK_ENTRY(media_list_items_child_2),
    MK_ENTRY(media_list_items_child_3),
    MK_ENTRY(media_list_items_child_2_1),

#undef MK_ENTRY
};

/*!\test
 * Generic enumeration of a root-level media list.
 *
 * \todo
 *     This test looks scary. Its actually simple, but still should be
 *     refactored so that all those expectations are better sorted out.
 */
void test_enumerate_root_media_list()
{
    /* we always need a server list at the root, so create one */
    auto server_list = std::make_shared<UPnP::ServerList>(nullptr);
    cppcut_assert_not_null(server_list.get());

    insert_upnp_server(server_list, "/com/intel/dLeynaServer/server/5");
    (void)cache->insert(server_list, LRU::CacheMode::CACHED, 0, 50);

    /* we have one server at index 0, now we are entering its root directory */
    mock_dbus_upnp_helpers->expect_get_proxy_object_path(
        "/com/intel/dLeynaServer/server/5",
        reinterpret_cast<tdbusdleynaserverMediaDevice *>(100));
    mock_dbus_upnp_helpers->expect_get_size_of_container(
        std::get<1>(media_list_items_tree[0]),
        "/com/intel/dLeynaServer/server/5");

    ListError error(ListError::INTERNAL);
    auto root_directory_id =
        server_list->enter_child(*cache, LRU::CacheModeRequest::AUTO,
                                 ID::Item(0), std::get<2>(media_list_items_tree[0]),
                                 always_continue, always_use_cached, purge_dummy, error);
    cut_assert_false(error.failed());
    cut_assert_true(root_directory_id.is_valid());

    /* have a root directory with some entries */
    auto root_directory = std::static_pointer_cast<UPnP::MediaList>(cache->lookup(root_directory_id));
    cppcut_assert_equal(std::get<1>(media_list_items_tree[0]), root_directory->size());

    /* OK, so there is a list now, but no materialized sublists yet */
    std::vector <ID::List> nodes;
    root_directory->enumerate_direct_sublists(*cache, nodes);
    cppcut_assert_equal(size_t(0), nodes.size());

    cache->lookup(root_directory_id)->enumerate_tree_of_sublists(*cache, nodes);
    cppcut_assert_equal(size_t(1), nodes.size());

    /* walk over root directory, enter all children using our test fillers */
    for(size_t i = 0; i < root_directory->size(); ++i)
    {
        const std::string expected_dbus_path =
            std::string("/de/tahifi/unittests/23/") +
            std::get<0>(media_list_items_tree[0])[i].dbus_name_;
        const std::string expected_dbus_path_message =
            "D-Bus path of new list is " + expected_dbus_path;

        if(i == 0)
            mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                                      "prefetch 1 items, starting at index 0");
        else
            mock_messages->expect_msg_vinfo(MESSAGE_LEVEL_DEBUG,
                                            "no need to prefetch index %u, already in cache");

        mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DIAG,
                                                  expected_dbus_path_message.c_str());
        mock_dbus_upnp_helpers->expect_get_size_of_container(
            std::get<1>(media_list_items_tree[i + 1]),
            expected_dbus_path);

        error = ListError::INTERNAL;
        auto child_id =
            root_directory->enter_child(*cache, LRU::CacheModeRequest::AUTO,
                                        ID::Item(i), std::get<2>(media_list_items_tree[i + 1]),
                                        always_continue, always_use_cached, purge_dummy,
                                        error);
        cut_assert_false(error.failed());
        cut_assert_true(child_id.is_valid());

        /* child list was created, but is not materialized yet */
        auto child = std::static_pointer_cast<UPnP::MediaList>(cache->lookup(child_id));
        cppcut_assert_not_null(child.get());
        cppcut_assert_equal(std::get<1>(media_list_items_tree[i + 1]), child->size());

        nodes.clear();
        child->enumerate_direct_sublists(*cache, nodes);
        cppcut_assert_equal(size_t(0), nodes.size());
    }

    /* root list has 3 populated child lists now */
    nodes.clear();
    root_directory->enumerate_direct_sublists(*cache, nodes);
    cppcut_assert_equal(std::get<1>(media_list_items_tree[0]), nodes.size());

    /* add another nested list */
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "no need to prefetch index 1, already in cache");
    auto dir =
        std::static_pointer_cast<UPnP::MediaList>(cache->lookup((*root_directory)[ID::Item(1)].get_child_list()));
    const std::string expected_dbus_path =
        std::string("/de/tahifi/unittests/23/") +
        std::get<0>(media_list_items_tree[2])[2].dbus_name_;
    const std::string expected_dbus_path_message =
            "D-Bus path of new list is " + expected_dbus_path;
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG,
                                              "prefetch 1 items, starting at index 2");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DIAG,
                                              expected_dbus_path_message.c_str());
    mock_dbus_upnp_helpers->expect_get_size_of_container(
        std::get<1>(media_list_items_tree[4]),
        expected_dbus_path);
    error = ListError::INTERNAL;
    dir->enter_child(*cache, LRU::CacheModeRequest::AUTO,
                     ID::Item(2), std::get<2>(media_list_items_tree[4]),
                     always_continue, always_use_cached, purge_dummy, error);
    cut_assert_false(error.failed());


    /* now we are complete; check some additional things */
    cache->lookup(root_directory_id)->enumerate_tree_of_sublists(*cache, nodes);
    cppcut_assert_equal(media_list_items_tree.size() + 1, cache->count());
    cppcut_assert_equal(media_list_items_tree.size(), nodes.size());
    cut_assert_true(nodes[0] == root_directory_id);
}

/*!\test
 * Get D-Bus path of subdirectories, given only the subdirectory.
 *
 * Doing this requires looking up the parent list for each subdirectory and
 * finding the subdirectory entry in the parent list.
 *
 * \see Issue #67.
 */
void test_get_dbus_object_paths_of_sublists()
{
    static constexpr std::array<const RawItem, 50> big_media_list=
    {
        RawItem("item-01", "path-01", UPnP::MediaList::Type::SUBDIRECTORY, 0),
        RawItem("item-02", "path-02", UPnP::MediaList::Type::SUBDIRECTORY, 0),
        RawItem("item-03", "path-03", UPnP::MediaList::Type::SUBDIRECTORY, 0),
        RawItem("item-04", "path-04", UPnP::MediaList::Type::SUBDIRECTORY, 0),
        RawItem("item-05", "path-05", UPnP::MediaList::Type::SUBDIRECTORY, 0),
        RawItem("item-06", "path-06", UPnP::MediaList::Type::SUBDIRECTORY, 0),
        RawItem("item-07", "path-07", UPnP::MediaList::Type::SUBDIRECTORY, 0),
        RawItem("item-08", "path-08", UPnP::MediaList::Type::SUBDIRECTORY, 0),
        RawItem("item-09", "path-09", UPnP::MediaList::Type::SUBDIRECTORY, 0),
        RawItem("item-10", "path-10", UPnP::MediaList::Type::SUBDIRECTORY, 0),
        RawItem("item-11", "path-11", UPnP::MediaList::Type::SUBDIRECTORY, 0),
        RawItem("item-12", "path-12", UPnP::MediaList::Type::SUBDIRECTORY, 0),
        RawItem("item-13", "path-13", UPnP::MediaList::Type::SUBDIRECTORY, 0),
        RawItem("item-14", "path-14", UPnP::MediaList::Type::SUBDIRECTORY, 0),
        RawItem("item-15", "path-15", UPnP::MediaList::Type::SUBDIRECTORY, 0),
        RawItem("item-16", "path-16", UPnP::MediaList::Type::SUBDIRECTORY, 0),
        RawItem("item-17", "path-17", UPnP::MediaList::Type::SUBDIRECTORY, 0),
        RawItem("item-18", "path-18", UPnP::MediaList::Type::SUBDIRECTORY, 0),
        RawItem("item-19", "path-19", UPnP::MediaList::Type::SUBDIRECTORY, 0),
        RawItem("item-20", "path-20", UPnP::MediaList::Type::SUBDIRECTORY, 0),
        RawItem("item-21", "path-21", UPnP::MediaList::Type::SUBDIRECTORY, 0),
        RawItem("item-22", "path-22", UPnP::MediaList::Type::SUBDIRECTORY, 0),
        RawItem("item-23", "path-23", UPnP::MediaList::Type::SUBDIRECTORY, 0),
        RawItem("item-24", "path-24", UPnP::MediaList::Type::SUBDIRECTORY, 0),
        RawItem("item-25", "path-25", UPnP::MediaList::Type::SUBDIRECTORY, 0),
        RawItem("item-26", "path-26", UPnP::MediaList::Type::SUBDIRECTORY, 0),
        RawItem("item-27", "path-27", UPnP::MediaList::Type::SUBDIRECTORY, 0),
        RawItem("item-28", "path-28", UPnP::MediaList::Type::SUBDIRECTORY, 0),
        RawItem("item-29", "path-29", UPnP::MediaList::Type::SUBDIRECTORY, 0),
        RawItem("item-30", "path-30", UPnP::MediaList::Type::SUBDIRECTORY, 0),
        RawItem("item-31", "path-31", UPnP::MediaList::Type::SUBDIRECTORY, 0),
        RawItem("item-32", "path-32", UPnP::MediaList::Type::SUBDIRECTORY, 0),
        RawItem("item-33", "path-33", UPnP::MediaList::Type::SUBDIRECTORY, 0),
        RawItem("item-34", "path-34", UPnP::MediaList::Type::SUBDIRECTORY, 0),
        RawItem("item-35", "path-35", UPnP::MediaList::Type::SUBDIRECTORY, 0),
        RawItem("item-36", "path-36", UPnP::MediaList::Type::SUBDIRECTORY, 0),
        RawItem("item-37", "path-37", UPnP::MediaList::Type::SUBDIRECTORY, 0),
        RawItem("item-38", "path-38", UPnP::MediaList::Type::SUBDIRECTORY, 0),
        RawItem("item-39", "path-39", UPnP::MediaList::Type::SUBDIRECTORY, 0),
        RawItem("item-40", "path-40", UPnP::MediaList::Type::SUBDIRECTORY, 0),
        RawItem("item-41", "path-41", UPnP::MediaList::Type::SUBDIRECTORY, 0),
        RawItem("item-42", "path-42", UPnP::MediaList::Type::SUBDIRECTORY, 0),
        RawItem("item-43", "path-43", UPnP::MediaList::Type::SUBDIRECTORY, 0),
        RawItem("item-44", "path-44", UPnP::MediaList::Type::SUBDIRECTORY, 0),
        RawItem("item-45", "path-45", UPnP::MediaList::Type::SUBDIRECTORY, 0),
        RawItem("item-46", "path-46", UPnP::MediaList::Type::SUBDIRECTORY, 0),
        RawItem("item-47", "path-47", UPnP::MediaList::Type::SUBDIRECTORY, 0),
        RawItem("item-48", "path-48", UPnP::MediaList::Type::SUBDIRECTORY, 0),
        RawItem("item-49", "path-49", UPnP::MediaList::Type::SUBDIRECTORY, 0),
        RawItem("item-50", "path-50", UPnP::MediaList::Type::SUBDIRECTORY, 0),
    };

    static constexpr std::array<const RawItem, 1> small_sublist =
    {
        RawItem("subitem-1", "subitem-1-path", UPnP::MediaList::Type::SUBDIRECTORY, 0),
    };

    static const EnumerateMediaListFiller big_media_list_filler(big_media_list);
    static const EnumerateMediaListFiller small_sublist_filler(small_sublist);

    auto server_list = std::make_shared<UPnP::ServerList>(nullptr);
    cppcut_assert_not_null(server_list.get());

    insert_upnp_server(server_list, "/com/intel/dLeynaServer/server/68");
    (void)cache->insert(server_list, LRU::CacheMode::CACHED, 0, 123);

    /* enter server's root directory */
    mock_dbus_upnp_helpers->expect_get_proxy_object_path(
        "/com/intel/dLeynaServer/server/68",
        reinterpret_cast<tdbusdleynaserverMediaDevice *>(100));
    mock_dbus_upnp_helpers->expect_get_size_of_container(
        big_media_list.size(), "/com/intel/dLeynaServer/server/68");

    ListError error(ListError::INTERNAL);
    auto root_directory_id =
        server_list->enter_child(*cache, LRU::CacheModeRequest::AUTO,
                                 ID::Item(0), big_media_list_filler,
                                 always_continue, always_use_cached, purge_dummy, error);
    cut_assert_false(error.failed());
    cut_assert_true(root_directory_id.is_valid());

    auto root_directory = std::static_pointer_cast<UPnP::MediaList>(cache->lookup(root_directory_id));
    cppcut_assert_equal(big_media_list.size(), root_directory->size());

    /* enter sublist at each item in root directory, get D-Bus paths of each
     * sublist */
    for(size_t i = 0; i < big_media_list.size(); ++i)
    {
        char buffer[256];

        if((i % UPnP::media_list_tile_size) == 0)
        {
            if(i > 0)
            {
                snprintf(buffer, sizeof(buffer), "slide down to index %zu", i);
                mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG, buffer);

                size_t idx = i + UPnP::media_list_tile_size;
                if(idx >= big_media_list.size())
                    idx = 0;

                snprintf(buffer, sizeof(buffer),
                         "materialize adjacent tile around index %zu", idx);
            }
            else
                snprintf(buffer, sizeof(buffer),
                         "prefetch 1 items, starting at index %zu", i);
        }
        else
            snprintf(buffer, sizeof(buffer),
                     "no need to prefetch index %zu, already in cache", i);

        mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DEBUG, buffer);

        snprintf(buffer, sizeof(buffer),
                 "/de/tahifi/unittests/23/path-%02zu", i + 1);

        const std::string expected_path(buffer);

        snprintf(buffer, sizeof(buffer),
                 "D-Bus path of new list is %s", expected_path.c_str());
        mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_DIAG, buffer);

        mock_dbus_upnp_helpers->expect_get_size_of_container(small_sublist.size(),
                                                             expected_path);

        error = ListError::INTERNAL;
        auto subdir_id =
            root_directory->enter_child(*cache, LRU::CacheModeRequest::AUTO,
                                        ID::Item(i), small_sublist_filler,
                                        always_continue, always_use_cached, purge_dummy, error);
        cut_assert_false(error.failed());
        cut_assert_true(subdir_id.is_valid());

        auto subdir = std::static_pointer_cast<UPnP::MediaList>(cache->lookup(subdir_id));
        cppcut_assert_equal(small_sublist.size(), subdir->size());

        /* get D-Bus path of sublist */
        const std::string path = subdir->get_dbus_object_path();
        cppcut_assert_equal(expected_path, path);
    }
}

};

/*!@}*/

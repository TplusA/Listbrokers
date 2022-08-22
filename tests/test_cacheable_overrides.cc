/*
 * Copyright (C) 2017--2020, 2022  T+A elektroakustik GmbH & Co. KG
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
#include <algorithm>

#include "mock_messages.hh"
#include "mock_backtrace.hh"
#include "mock_timebase.hh"

#include "cacheable.hh"

class FakeTimer
{
  public:
    enum FireResult
    {
        ARMED,
        FIRED,
        ALREADY_EXPIRED,
    };

    const ID::List list_id_;
    const uint32_t id_;
    const uint64_t start_time_;
    const uint64_t trigger_time_;

  private:
    int (*const callback_)(void *user_data);
    void *const callback_data_;

    bool is_expired_;
    bool keep_around_on_removal_;

  public:
    FakeTimer(const FakeTimer &) = delete;
    FakeTimer(FakeTimer &&) = default;
    FakeTimer &operator=(const FakeTimer &) = delete;

    explicit FakeTimer(ID::List list_id,
                       uint32_t id, uint64_t start_time, unsigned int seconds,
                       int (*callback)(void *), void *user_data):
        list_id_(list_id),
        id_(id),
        start_time_(start_time),
        trigger_time_(start_time + std::chrono::microseconds(std::chrono::seconds(seconds)).count()),
        callback_(callback),
        callback_data_(user_data),
        is_expired_(false),
        keep_around_on_removal_(false)
    {}

    FireResult try_fire(uint64_t now)
    {
        if(is_expired_)
            return FireResult::ALREADY_EXPIRED;

        if(now < trigger_time_)
            return FireResult::ARMED;

        is_expired_ = true;
        callback_(callback_data_);

        return FireResult::FIRED;
    }

    void keep_around_on_removal()
    {
        cut_assert_false(is_expired_);
        cut_assert_false(keep_around_on_removal_);
        keep_around_on_removal_ = true;
    }

    bool try_expire_internally()
    {
        if(!keep_around_on_removal_)
            return false;

        cut_assert_false(is_expired_);
        is_expired_ = true;

        return true;
    }
};

class ActiveTimers
{
  public:
    using MapType = std::map<uint32_t, FakeTimer>;

  private:
    uint32_t next_free_timer_id_;
    MapType timers_;

    ID::List pending_add_override_id_;

  public:
    ActiveTimers(const ActiveTimers &) = delete;
    ActiveTimers &operator=(const ActiveTimers &) = delete;

    explicit ActiveTimers():
        next_free_timer_id_(1)
    {}

    void expect_add_override(ID::List id)
    {
        check_no_pending_add_override();
        cut_assert_true(id.is_valid());
        pending_add_override_id_ = id;
    }

    void check_no_pending_add_override()
    {
        cut_assert_false(pending_add_override_id_.is_valid());
    }

    uint32_t add(uint64_t start_time, unsigned int seconds,
                 int (*callback)(void *), void *user_data)
    {
        cut_assert_true(pending_add_override_id_.is_valid());

        const auto id = next_free_timer_id_++;

        timers_.emplace(id, std::move(FakeTimer(pending_add_override_id_,
                                                id, start_time, seconds,
                                                callback, user_data)));
        pending_add_override_id_ = ID::List();

        return id;
    }

    void remove_or_expire_internally(uint32_t id)
    {
        cut_assert_true(exists(id));

        if(!timers_.find(id)->second.try_expire_internally())
            do_remove(id);
    }

    void do_remove(uint32_t id)
    {
        cut_assert_true(exists(id));
        cppcut_assert_equal(size_t(1), timers_.erase(id));
    }

    bool exists(uint32_t id) const
    {
        return timers_.find(id) != timers_.end();
    }

    MapType::iterator begin() { return timers_.begin(); }
    MapType::iterator end() { return timers_.end(); }
    MapType::const_iterator begin() const { return timers_.begin(); }
    MapType::const_iterator end() const { return timers_.end(); }
};

class GLibWrapperMock: public Cacheable::GLibWrapperIface
{
  public:
    class UIntWrapper
    {
      private:
        unsigned int value_;

      protected:
        explicit UIntWrapper(unsigned int value):
            value_(value)
        {}

      public:
        virtual ~UIntWrapper() {}

        UIntWrapper &operator++()
        {
            value_++;
            return *this;
        }

        bool operator==(const UIntWrapper &other) const
        {
            return value_ == other.value_;
        }

        bool operator==(const unsigned int &other) const
        {
            return value_ == other;
        }

        friend std::ostream &operator<<(std::ostream &os, const UIntWrapper &ui);
    };

    struct ArmedCount: public UIntWrapper { explicit ArmedCount(unsigned int value): UIntWrapper(value) {} };
    struct FiredCount: public UIntWrapper { explicit FiredCount(unsigned int value): UIntWrapper(value) {} };
    struct ExpiredCount: public UIntWrapper { explicit ExpiredCount(unsigned int value): UIntWrapper(value) {} };

    struct TimerStatistics
    {
        ArmedCount armed_;
        FiredCount fired_;
        ExpiredCount expired_;

        TimerStatistics(const TimerStatistics &) = delete;
        TimerStatistics(TimerStatistics &&) = default;
        TimerStatistics &operator=(const TimerStatistics &) = delete;
        TimerStatistics &operator=(TimerStatistics &&) = default;

        explicit TimerStatistics():
            armed_(0),
            fired_(0),
            expired_(0)
        {}

        void expect(const ArmedCount expected_armed,
                    const FiredCount expected_fired,
                    const ExpiredCount expected_expired)
        {
            cppcut_assert_equal(expected_armed, armed_);
            cppcut_assert_equal(expected_fired, fired_);
            cppcut_assert_equal(expected_expired, expired_);
        }

        void expect(const ArmedCount expected_armed)
        {
            expect(expected_armed, FiredCount(0), ExpiredCount(0));
        }

        void expect(const FiredCount expected_fired)
        {
            expect(ArmedCount(0), expected_fired, ExpiredCount(0));
        }

        void expect(const ExpiredCount expected_expired)
        {
            expect(ArmedCount(0), FiredCount(0), expected_expired);
        }

        void expect_nothing()
        {
            expect(ArmedCount(0));
        }
    };

  private:
    const MockTimebase &timebase_;
    mutable ActiveTimers timers_;

  public:
    GLibWrapperMock(const GLibWrapperMock &) = delete;
    GLibWrapperMock(GLibWrapperMock &&) = default;
    GLibWrapperMock &operator=(const GLibWrapperMock &) = delete;

    explicit GLibWrapperMock(const MockTimebase &timebase):
        timebase_(timebase)
    {}

    void ref_main_loop(struct _GMainLoop *loop) const final override
    {
        cppcut_assert_null(loop);
    }

    void unref_main_loop(struct _GMainLoop *loop) const final override
    {
        cut_fail("unexpected call");
    }

    void create_timeout(int64_t &start_time, uint32_t &active_timer_id,
                        int (*trampoline)(void *user_data),
                        Cacheable::Override *origin_object) const final override
    {
        cppcut_assert_not_null(reinterpret_cast<void *>(trampoline));
        cppcut_assert_not_null(origin_object);

        using us = std::chrono::microseconds;
        start_time = std::chrono::duration_cast<us>(timebase_.now().time_since_epoch()).count();
        active_timer_id =
            timers_.add(start_time, Cacheable::Override::EXPIRY_TIME.count(),
                        trampoline, origin_object);
    }

    void remove_timeout(uint32_t active_timer_id) const final override
    {
        timers_.remove_or_expire_internally(active_timer_id);
    }

    bool has_t_exceeded_expiry_time(int64_t t) const final override
    {
        using us = std::chrono::microseconds;
        const auto now =
            std::chrono::duration_cast<us>(timebase_.now().time_since_epoch()).count();

        return now >= t + std::chrono::microseconds(Cacheable::Override::EXPIRY_TIME).count();
    }

    void expect_add_override(ID::List id) { timers_.expect_add_override(id); }
    void check_no_pending_add_override() { timers_.check_no_pending_add_override(); }

    void assume_timer_fire_event_is_pending_in_glib_main(ID::List id)
    {
        const auto it =
            std::find_if(timers_.begin(), timers_.end(),
                         [&id] (const ActiveTimers::MapType::value_type &ft)
                         {
                             return ft.second.list_id_ == id;
                         });

        cut_assert_true(it != timers_.end());
        cppcut_assert_equal(id.get_raw_id(), it->second.list_id_.get_raw_id());

        it->second.keep_around_on_removal();
    }

    size_t get_number_of_timers() const
    {
        return std::distance(timers_.begin(), timers_.end());
    }

    TimerStatistics do_timers()
    {
        using us = std::chrono::microseconds;
        const auto now =
            std::chrono::duration_cast<us>(timebase_.now().time_since_epoch()).count();

        TimerStatistics counts;

        for(auto it = timers_.begin(); it != timers_.end(); /* nothing */)
        {
            /* we must advance to the next timer and use a copy of our current
             * one because the timer callback may erroneously erase the timer,
             * invalidating our iterator */
            auto temp = it++;
            const auto timer_id = temp->first;

            switch(temp->second.try_fire(now))
            {
              case FakeTimer::FireResult::ARMED:
                ++counts.armed_;
                break;

              case FakeTimer::FireResult::FIRED:
                /* the timer callback must not have erased the timer---we are
                 * doing this */
                ++counts.fired_;
                timers_.do_remove(timer_id);
                break;

              case FakeTimer::FireResult::ALREADY_EXPIRED:
                /* clean up */
                ++counts.expired_;
                timers_.do_remove(timer_id);
                break;
            }
        }

        return counts;
    }
};

std::ostream &operator<<(std::ostream &os, const GLibWrapperMock::UIntWrapper &ui)
{
    os << ui.value_;
    return os;
}

class Object: public LRU::Entry
{
  public:
    const unsigned int dummy_data_;
    std::vector<ID::List> children_;

    Object(const Object &) = delete;
    Object &operator=(const Object &) = delete;

    explicit Object(const std::shared_ptr<Entry> &parent, unsigned int data = 0):
        LRU::Entry(parent),
        dummy_data_(data)
    {}

    virtual ~Object() {}

    void enumerate_tree_of_sublists(const LRU::Cache &cache,
                                    std::vector<ID::List> &nodes,
                                    bool append_to_nodes) const override
    {
        if(!append_to_nodes)
            nodes.clear();

        nodes.push_back(get_cache_id());

        for(const auto &child_id : children_)
        {
            cut_assert_true(child_id.is_valid());
            cache.lookup(child_id)->enumerate_tree_of_sublists(cache, nodes, true);
        }
    }

    void enumerate_direct_sublists(const LRU::Cache &cache,
                                   std::vector<ID::List> &nodes) const override
    {
        cut_fail("Unexpected child list enumeration");
    }

    void obliviate_child(ID::List child_id, const LRU::Entry *child) override
    {
        auto it = std::find(children_.begin(), children_.end(), child_id);
        cut_assert_true(it != children_.end());
        cppcut_assert_not_null(child);
        cppcut_assert_equal(it->get_raw_id(), child->get_cache_id().get_raw_id());
        children_.erase(it);
        cppcut_assert_equal(get_number_of_children(), children_.size());
    }
};

static MockMessages *mock_messages;
static MockBacktrace *mock_backtrace;
static MockTimebase mock_timebase;
Timebase *LRU::timebase = &mock_timebase;

namespace cacheable_lowlevel_tests
{

static LRU::Cache *cache;
static Cacheable::CheckWithOverrides *overrides;
static GLibWrapperMock *glib_wrapper_mock;

static constexpr size_t maximum_number_of_objects = 200;

void cut_setup()
{
    mock_timebase.reset();

    mock_messages = new MockMessages;
    cppcut_assert_not_null(mock_messages);
    mock_messages->init();
    mock_messages_singleton = mock_messages;

    mock_backtrace = new MockBacktrace;
    cppcut_assert_not_null(mock_backtrace);
    mock_backtrace->init();
    mock_backtrace_singleton = mock_backtrace;

    cache = new LRU::Cache(500000, maximum_number_of_objects,
                           std::chrono::minutes(1));
    cppcut_assert_not_null(cache);
    cache->set_callbacks([]{}, []{}, [] (ID::List id) {}, []{});
    cppcut_assert_equal(size_t(0), cache->count());

    glib_wrapper_mock = new GLibWrapperMock(mock_timebase);
    cppcut_assert_not_null(glib_wrapper_mock);

    overrides = new Cacheable::CheckWithOverrides(*glib_wrapper_mock, *cache, nullptr);
    cppcut_assert_not_null(overrides);

    mock_messages->ignore_messages_with_level_or_above(MESSAGE_LEVEL_TRACE);
}

void cut_teardown()
{
    delete overrides;
    overrides = nullptr;

    delete glib_wrapper_mock;
    glib_wrapper_mock = nullptr;

    delete cache;
    cache = nullptr;

    mock_backtrace->check();
    mock_backtrace_singleton = nullptr;
    delete mock_backtrace;
    mock_backtrace = nullptr;

    mock_messages->check();
    mock_messages_singleton = nullptr;
    delete mock_messages;
    mock_messages = nullptr;
}

static ID::List put_override_for_id(ID::List id)
{
    glib_wrapper_mock->expect_add_override(id);

    cppcut_assert_equal(std::chrono::microseconds(Cacheable::Override::EXPIRY_TIME).count(),
                        std::chrono::microseconds(overrides->put_override(id)).count());

    glib_wrapper_mock->check_no_pending_add_override();

    cut_assert_true(overrides->is_cacheable(id));

    return id;
}

/*!\test
 * Invalid list ID.
 */
void test_invalid_list_id_is_defined_noncacheble()
{
    cut_assert_false(overrides->is_cacheable(ID::List()));
}

/*!\test
 * Valid list ID, but not in our cache.
 */
void test_nonexistent_list_ids_are_defined_noncacheble()
{
    mock_messages->expect_msg_error_formatted(0, LOG_CRIT, "BUG: No list in cache for ID 23");
    cut_assert_false(overrides->is_cacheable(ID::List(23)));

    mock_messages->expect_msg_error_formatted(0, LOG_CRIT, "BUG: No list in cache for ID 134217751");
    cut_assert_false(overrides->is_cacheable(ID::List(23U | ID::List::NOCACHE_BIT)));
}

/*!\test
 * Root list is uncacheable.
 */
void test_noncacheable_root()
{
    auto root = std::make_shared<Object>(nullptr);
    cppcut_assert_not_null(root.get());

    const auto root_id = cache->insert(root, LRU::CacheMode::UNCACHED, 0, 90);
    cppcut_assert_not_equal(0U, root_id.get_raw_id());
    cut_assert_true(root_id.get_nocache_bit());

    cut_assert_false(overrides->is_cacheable(root_id));
}

/*!\test
 * Root list is cacheable.
 */
void test_cacheable_root()
{
    auto root = std::make_shared<Object>(nullptr);
    cppcut_assert_not_null(root.get());

    const auto root_id = cache->insert(root, LRU::CacheMode::CACHED, 0, 90);
    cppcut_assert_not_equal(0U, root_id.get_raw_id());
    cut_assert_false(root_id.get_nocache_bit());

    cut_assert_true(overrides->is_cacheable(root_id));
}

/*!\test
 * Uncacheable root list is temporarily marked cacheable, then put back to
 * normal.
 */
void test_noncacheable_root_override()
{
    auto root = std::make_shared<Object>(nullptr);
    cppcut_assert_not_null(root.get());

    const auto root_id = cache->insert(root, LRU::CacheMode::UNCACHED, 0, 90);

    cut_assert_false(overrides->is_cacheable(root_id));
    put_override_for_id(root_id);
    cut_assert_true(overrides->remove_override(root_id));
    cut_assert_false(overrides->is_cacheable(root_id));
}

static std::shared_ptr<LRU::Entry>
add_cached_or_uncached_object(const std::shared_ptr<LRU::Entry> &parent,
                              LRU::CacheMode cmode, unsigned int data = 0,
                              ID::List *object_id = nullptr,
                              size_t object_size = 10)
{
    std::shared_ptr<LRU::Entry> obj = std::make_shared<Object>(parent, data);
    cppcut_assert_not_null(obj.get());

    const size_t previous_count = cache->count();
    const ID::List id = cache->insert(std::shared_ptr<LRU::Entry>(obj), cmode, 0, object_size);
    cppcut_assert_operator(0U, <, id.get_raw_id());
    cppcut_assert_equal(id.get_raw_id(), obj->get_cache_id().get_raw_id());
    cppcut_assert_equal(previous_count + 1U, cache->count());

    if(object_id)
        *object_id = id;

    return obj;
}

static std::shared_ptr<LRU::Entry>
add_object(std::shared_ptr<LRU::Entry> parent, unsigned int data,
           const std::array<const bool, 10> &cacheable,
           std::array<ID::List, 10> &list_ids, size_t &i)
{
    cppcut_assert_operator(cacheable.size(), >, i);

    auto obj =
        add_cached_or_uncached_object(parent,
                                      cacheable[i] ? LRU::CacheMode::CACHED : LRU::CacheMode::UNCACHED,
                                      data, &list_ids[i], 10);

    ++i;

    return obj;
}

/*
 * This function constructs the hierarchy shown below.
 *
 *             123
 *            _/ \_
 *           /     \
 *          256    712
 *         _/ \_
 *        /     \
 *      309     631
 *       |       |
 *      379     665
 *     _/ \_
 *    /     \
 *  446     579
 *           |
 *          551
 */
static void construct_list_hierarchy(const std::array<const bool, 10> &cacheable,
                                     std::array<ID::List, 10> &list_ids)
{
    size_t i = 0;

    std::shared_ptr<LRU::Entry> root       = add_object(nullptr,    123, cacheable, list_ids, i);
    std::shared_ptr<LRU::Entry> fork_upper = add_object(root,       256, cacheable, list_ids, i);
    std::shared_ptr<LRU::Entry> temp       = add_object(fork_upper, 309, cacheable, list_ids, i);
    std::shared_ptr<LRU::Entry> fork_lower = add_object(temp,       379, cacheable, list_ids, i);
    temp                                   = add_object(fork_lower, 446, cacheable, list_ids, i);
    temp                                   = add_object(fork_lower, 579, cacheable, list_ids, i);
    temp                                   = add_object(temp,       551, cacheable, list_ids, i);
    temp                                   = add_object(fork_upper, 631, cacheable, list_ids, i);
    temp                                   = add_object(temp,       665, cacheable, list_ids, i);
    temp                                   = add_object(root,       712, cacheable, list_ids, i);

    cppcut_assert_equal(cacheable.size(), cache->count());

    for(i = 0; i < cacheable.size(); ++i)
        cppcut_assert_not_equal(cacheable[i], list_ids[i].get_nocache_bit());
}

static void assert_cacheable_defaults(const std::array<ID::List, 10> &list_ids)
{
    for(const auto &id : list_ids)
        cppcut_assert_not_equal(id.get_nocache_bit(), overrides->is_cacheable(id));
}

static ID::List prepare_override_test(const std::array<const bool, 10> &cacheable,
                                      std::array<ID::List, 10> &list_ids,
                                      size_t auto_override_index = 10)
{
    cppcut_assert_operator(list_ids.size(), >=, auto_override_index);

    construct_list_hierarchy(cacheable, list_ids);
    assert_cacheable_defaults(list_ids);

    return(auto_override_index < list_ids.size())
        ? put_override_for_id(list_ids[auto_override_index])
        : ID::List();
}

/*!\test
 * Completely cacheable hierarchy.
 */
void test_cacheable_nodes()
{
    std::array<ID::List, 10> list_ids;
    prepare_override_test({ true, true, true, true, true,
                            true, true, true, true, true, },
                          list_ids);
    assert_cacheable_defaults(list_ids);
}

/*!\test
 * The complete path to a noncacheable node which is temporarily marked as
 * cacheable is checked as cacheable, but noncacheable subtrees branching from
 * that path are still noncacheable.
 */
void test_noncacheable_leaf_override()
{
    std::array<ID::List, 10> list_ids;

    const ID::List override_id =
        prepare_override_test({ true, true, true, true, false,
                                true, true, true, true, true, },
                              list_ids, 4);

    cut_assert_true(overrides->is_cacheable(list_ids[4]));
    cut_assert_true(overrides->is_cacheable(list_ids[3]));
    cut_assert_true(overrides->is_cacheable(list_ids[2]));
    cut_assert_true(overrides->is_cacheable(list_ids[1]));
    cut_assert_true(overrides->is_cacheable(list_ids[0]));
    cut_assert_true(overrides->is_cacheable(list_ids[5]));
    cut_assert_true(overrides->is_cacheable(list_ids[6]));
    cut_assert_true(overrides->is_cacheable(list_ids[7]));
    cut_assert_true(overrides->is_cacheable(list_ids[8]));
    cut_assert_true(overrides->is_cacheable(list_ids[9]));

    cut_assert_true(overrides->remove_override(override_id));

    assert_cacheable_defaults(list_ids);
}

/*!\test
 * The complete path to a subtree of a noncacheable node which is temporarily
 * marked as cacheable and the complete subtree itself are checked as
 * cacheable, but noncacheable subtrees branching from the path are still
 * noncacheable.
 */
void test_noncacheable_subtree_node_override_by_transitivity()
{
    std::array<ID::List, 10> list_ids;

    ID::List override_id =
        prepare_override_test({ true, false, false, false, false,
                                false, false, false, false, true, },
                              list_ids, 2);

    cut_assert_true(overrides->is_cacheable(list_ids[2]));
    cut_assert_true(overrides->is_cacheable(list_ids[1]));
    cut_assert_true(overrides->is_cacheable(list_ids[0]));
    cut_assert_true(overrides->is_cacheable(list_ids[3]));
    cut_assert_true(overrides->is_cacheable(list_ids[4]));
    cut_assert_true(overrides->is_cacheable(list_ids[5]));
    cut_assert_true(overrides->is_cacheable(list_ids[6]));
    cut_assert_false(overrides->is_cacheable(list_ids[7]));
    cut_assert_false(overrides->is_cacheable(list_ids[8]));
    cut_assert_true(overrides->is_cacheable(list_ids[9]));

    cut_assert_true(overrides->remove_override(override_id));

    assert_cacheable_defaults(list_ids);

    /* test another node */
    override_id = put_override_for_id(list_ids[7]);

    cut_assert_true(overrides->is_cacheable(list_ids[7]));
    cut_assert_true(overrides->is_cacheable(list_ids[1]));
    cut_assert_true(overrides->is_cacheable(list_ids[0]));
    cut_assert_true(overrides->is_cacheable(list_ids[8]));
    cut_assert_false(overrides->is_cacheable(list_ids[2]));
    cut_assert_false(overrides->is_cacheable(list_ids[3]));
    cut_assert_false(overrides->is_cacheable(list_ids[4]));
    cut_assert_false(overrides->is_cacheable(list_ids[5]));
    cut_assert_false(overrides->is_cacheable(list_ids[6]));
    cut_assert_true(overrides->is_cacheable(list_ids[9]));

    cut_assert_true(overrides->remove_override(override_id));

    assert_cacheable_defaults(list_ids);
}

/*!\test
 * Same as
 * #cacheable_lowlevel_tests::test_noncacheable_subtree_node_override_by_transitivity(),
 * but the whole list hierarchy is marked noncacheable by default.
 */
void test_noncacheable_tree_override_by_transitivity()
{
    std::array<ID::List, 10> list_ids;

    ID::List override_id =
        prepare_override_test({ false, false, false, false, false,
                                false, false, false, false, false, },
                              list_ids, 2);

    cut_assert_true(overrides->is_cacheable(list_ids[2]));
    cut_assert_true(overrides->is_cacheable(list_ids[1]));
    cut_assert_true(overrides->is_cacheable(list_ids[0]));
    cut_assert_true(overrides->is_cacheable(list_ids[3]));
    cut_assert_true(overrides->is_cacheable(list_ids[4]));
    cut_assert_true(overrides->is_cacheable(list_ids[5]));
    cut_assert_true(overrides->is_cacheable(list_ids[6]));
    cut_assert_false(overrides->is_cacheable(list_ids[7]));
    cut_assert_false(overrides->is_cacheable(list_ids[8]));
    cut_assert_false(overrides->is_cacheable(list_ids[9]));

    cut_assert_true(overrides->remove_override(override_id));

    assert_cacheable_defaults(list_ids);

    /* caching the root node caches everything */
    override_id = put_override_for_id(list_ids[0]);

    cut_assert_true(overrides->is_cacheable(list_ids[0]));
    cut_assert_true(overrides->is_cacheable(list_ids[1]));
    cut_assert_true(overrides->is_cacheable(list_ids[2]));
    cut_assert_true(overrides->is_cacheable(list_ids[3]));
    cut_assert_true(overrides->is_cacheable(list_ids[4]));
    cut_assert_true(overrides->is_cacheable(list_ids[5]));
    cut_assert_true(overrides->is_cacheable(list_ids[6]));
    cut_assert_true(overrides->is_cacheable(list_ids[7]));
    cut_assert_true(overrides->is_cacheable(list_ids[8]));
    cut_assert_true(overrides->is_cacheable(list_ids[9]));

    cut_assert_true(overrides->remove_override(override_id));

    assert_cacheable_defaults(list_ids);
}

/*!\test
 * Setting an override on a cached node works and marks any noncacheable
 * subtree as cacheable.
 *
 * This is not only a convenience feature, but also meant to simplify use in
 * client code so that a player only needs to mark its playback root as
 * cacheable, not each and every subtree it enters. It also helps with
 * resources because likely, only a single override is ever going to be needed
 * (fingers crossed).
 */
void test_cacheable_node_overrides_noncacheable_subtree_by_transitivity()
{
    std::array<ID::List, 10> list_ids;

    ID::List override_id =
        prepare_override_test({ true, true, true, false, false,
                                false, false, true, true, true, },
                              list_ids, 1);

    cut_assert_true(overrides->is_cacheable(list_ids[0]));
    cut_assert_true(overrides->is_cacheable(list_ids[1]));
    cut_assert_true(overrides->is_cacheable(list_ids[2]));
    cut_assert_true(overrides->is_cacheable(list_ids[3]));
    cut_assert_true(overrides->is_cacheable(list_ids[4]));
    cut_assert_true(overrides->is_cacheable(list_ids[5]));
    cut_assert_true(overrides->is_cacheable(list_ids[6]));
    cut_assert_true(overrides->is_cacheable(list_ids[7]));
    cut_assert_true(overrides->is_cacheable(list_ids[8]));
    cut_assert_true(overrides->is_cacheable(list_ids[9]));

    cut_assert_true(overrides->remove_override(override_id));

    assert_cacheable_defaults(list_ids);
}

/*!\test
 * Multiple active overrides are supported in partially cached hierarchies.
 */
void test_multiple_overrides_in_mixed_hierarchy()
{
    std::array<ID::List, 10> list_ids;

    const ID::List override_A =
        prepare_override_test({ true, true, false, false, false,
                                false, false, false, false, false, },
                              list_ids, 4);
    const ID::List override_B = put_override_for_id(list_ids[5]);
    const ID::List override_C = put_override_for_id(list_ids[7]);

    cut_assert_true(overrides->is_cacheable(list_ids[0]));
    cut_assert_true(overrides->is_cacheable(list_ids[1]));
    cut_assert_true(overrides->is_cacheable(list_ids[2]));
    cut_assert_true(overrides->is_cacheable(list_ids[3]));
    cut_assert_true(overrides->is_cacheable(list_ids[4]));
    cut_assert_true(overrides->is_cacheable(list_ids[5]));
    cut_assert_true(overrides->is_cacheable(list_ids[6]));
    cut_assert_true(overrides->is_cacheable(list_ids[7]));
    cut_assert_true(overrides->is_cacheable(list_ids[8]));
    cut_assert_false(overrides->is_cacheable(list_ids[9]));

    cut_assert_true(overrides->remove_override(override_A));

    cut_assert_true(overrides->is_cacheable(list_ids[0]));
    cut_assert_true(overrides->is_cacheable(list_ids[1]));
    cut_assert_true(overrides->is_cacheable(list_ids[2]));
    cut_assert_true(overrides->is_cacheable(list_ids[3]));
    cut_assert_false(overrides->is_cacheable(list_ids[4]));
    cut_assert_true(overrides->is_cacheable(list_ids[5]));
    cut_assert_true(overrides->is_cacheable(list_ids[6]));
    cut_assert_true(overrides->is_cacheable(list_ids[7]));
    cut_assert_true(overrides->is_cacheable(list_ids[8]));
    cut_assert_false(overrides->is_cacheable(list_ids[9]));

    cut_assert_true(overrides->remove_override(override_B));

    cut_assert_true(overrides->is_cacheable(list_ids[0]));
    cut_assert_true(overrides->is_cacheable(list_ids[1]));
    cut_assert_false(overrides->is_cacheable(list_ids[2]));
    cut_assert_false(overrides->is_cacheable(list_ids[3]));
    cut_assert_false(overrides->is_cacheable(list_ids[4]));
    cut_assert_false(overrides->is_cacheable(list_ids[5]));
    cut_assert_false(overrides->is_cacheable(list_ids[6]));
    cut_assert_true(overrides->is_cacheable(list_ids[7]));
    cut_assert_true(overrides->is_cacheable(list_ids[8]));
    cut_assert_false(overrides->is_cacheable(list_ids[9]));

    cut_assert_true(overrides->remove_override(override_C));

    assert_cacheable_defaults(list_ids);
}

/*!\test
 * Multiple active overrides are supported in noncached hierarchies.
 */
void test_multiple_overrides_in_noncached_hierarchy()
{
    std::array<ID::List, 10> list_ids;

    const ID::List override_A =
        prepare_override_test({ false, false, false, false, false,
                                false, false, false, false, false, },
                              list_ids, 4);
    const ID::List override_B = put_override_for_id(list_ids[5]);
    const ID::List override_C = put_override_for_id(list_ids[7]);

    cut_assert_true(overrides->is_cacheable(list_ids[0]));
    cut_assert_true(overrides->is_cacheable(list_ids[1]));
    cut_assert_true(overrides->is_cacheable(list_ids[2]));
    cut_assert_true(overrides->is_cacheable(list_ids[3]));
    cut_assert_true(overrides->is_cacheable(list_ids[4]));
    cut_assert_true(overrides->is_cacheable(list_ids[5]));
    cut_assert_true(overrides->is_cacheable(list_ids[6]));
    cut_assert_true(overrides->is_cacheable(list_ids[7]));
    cut_assert_true(overrides->is_cacheable(list_ids[8]));
    cut_assert_false(overrides->is_cacheable(list_ids[9]));

    cut_assert_true(overrides->remove_override(override_A));

    cut_assert_true(overrides->is_cacheable(list_ids[0]));
    cut_assert_true(overrides->is_cacheable(list_ids[1]));
    cut_assert_true(overrides->is_cacheable(list_ids[2]));
    cut_assert_true(overrides->is_cacheable(list_ids[3]));
    cut_assert_false(overrides->is_cacheable(list_ids[4]));
    cut_assert_true(overrides->is_cacheable(list_ids[5]));
    cut_assert_true(overrides->is_cacheable(list_ids[6]));
    cut_assert_true(overrides->is_cacheable(list_ids[7]));
    cut_assert_true(overrides->is_cacheable(list_ids[8]));
    cut_assert_false(overrides->is_cacheable(list_ids[9]));

    cut_assert_true(overrides->remove_override(override_B));

    cut_assert_true(overrides->is_cacheable(list_ids[0]));
    cut_assert_true(overrides->is_cacheable(list_ids[1]));
    cut_assert_false(overrides->is_cacheable(list_ids[2]));
    cut_assert_false(overrides->is_cacheable(list_ids[3]));
    cut_assert_false(overrides->is_cacheable(list_ids[4]));
    cut_assert_false(overrides->is_cacheable(list_ids[5]));
    cut_assert_false(overrides->is_cacheable(list_ids[6]));
    cut_assert_true(overrides->is_cacheable(list_ids[7]));
    cut_assert_true(overrides->is_cacheable(list_ids[8]));
    cut_assert_false(overrides->is_cacheable(list_ids[9]));

    cut_assert_true(overrides->has_overrides());
    cut_assert_true(overrides->remove_override(override_C));
    cut_assert_false(overrides->has_overrides());

    assert_cacheable_defaults(list_ids);
}

/*!\test
 * In case an override isn't refreshed for some time, it expires and will be
 * removed automatically.
 */
void test_single_override_timeout_expired()
{
    std::array<ID::List, 10> list_ids;

    ID::List override_id =
        prepare_override_test({ true, true, true, false, false,
                                false, false, true, true, true, },
                              list_ids, 3);

    cut_assert_true(overrides->is_cacheable(list_ids[0]));
    cut_assert_true(overrides->is_cacheable(list_ids[1]));
    cut_assert_true(overrides->is_cacheable(list_ids[2]));
    cut_assert_true(overrides->is_cacheable(list_ids[3]));
    cut_assert_true(overrides->is_cacheable(list_ids[4]));
    cut_assert_true(overrides->is_cacheable(list_ids[5]));
    cut_assert_true(overrides->is_cacheable(list_ids[6]));
    cut_assert_true(overrides->is_cacheable(list_ids[7]));
    cut_assert_true(overrides->is_cacheable(list_ids[8]));
    cut_assert_true(overrides->is_cacheable(list_ids[9]));

    /* timer has not expired yet... */
    mock_timebase.step(std::chrono::milliseconds(Cacheable::Override::EXPIRY_TIME).count() - 1);

    auto counts = glib_wrapper_mock->do_timers();
    counts.expect(GLibWrapperMock::ArmedCount(1));

    cppcut_assert_equal(size_t(1), glib_wrapper_mock->get_number_of_timers());
    cut_assert_true(overrides->is_cacheable(override_id));
    cut_assert_true(overrides->has_overrides());

    /* ...but now it has */
    mock_timebase.step();

    counts = glib_wrapper_mock->do_timers();
    counts.expect(GLibWrapperMock::FiredCount(1));

    cppcut_assert_equal(size_t(0), glib_wrapper_mock->get_number_of_timers());
    cut_assert_false(overrides->is_cacheable(override_id));

    assert_cacheable_defaults(list_ids);
    cut_assert_false(overrides->remove_override(override_id));
    cut_assert_false(overrides->has_overrides());
}

static void single_override_does_not_get_removed_if_refreshed(bool assume_timer_fired_pending)
{
    std::array<ID::List, 10> list_ids;

    const ID::List override_id =
        prepare_override_test({ true, true, true, false, false,
                                false, false, true, true, true, },
                              list_ids, 3);

    cut_assert_true(overrides->is_cacheable(list_ids[0]));
    cut_assert_true(overrides->is_cacheable(list_ids[1]));
    cut_assert_true(overrides->is_cacheable(list_ids[2]));
    cut_assert_true(overrides->is_cacheable(list_ids[3]));
    cut_assert_true(overrides->is_cacheable(list_ids[4]));
    cut_assert_true(overrides->is_cacheable(list_ids[5]));
    cut_assert_true(overrides->is_cacheable(list_ids[6]));
    cut_assert_true(overrides->is_cacheable(list_ids[7]));
    cut_assert_true(overrides->is_cacheable(list_ids[8]));
    cut_assert_true(overrides->is_cacheable(list_ids[9]));

    /* timer has not expired yet... */
    mock_timebase.step(std::chrono::milliseconds(Cacheable::Override::EXPIRY_TIME).count() - 1);

    auto counts = glib_wrapper_mock->do_timers();
    counts.expect(GLibWrapperMock::ArmedCount(1));

    cppcut_assert_equal(size_t(1), glib_wrapper_mock->get_number_of_timers());
    cut_assert_true(overrides->is_cacheable(override_id));
    cut_assert_true(overrides->has_overrides());

    /* refresh, just in time, inserting a new timer which replaces the old one;
     * the old timer object will still be around (because GLib does it this way
     * because GLib is a ridiculous pile of junk...) and it will expire in a
     * millisecond; it should not invoke the timer callback, but be removed
     * internally */
    if(assume_timer_fired_pending)
        glib_wrapper_mock->assume_timer_fire_event_is_pending_in_glib_main(override_id);

    put_override_for_id(override_id);

    if(assume_timer_fired_pending)
        cppcut_assert_equal(size_t(2), glib_wrapper_mock->get_number_of_timers());
    else
        cppcut_assert_equal(size_t(1), glib_wrapper_mock->get_number_of_timers());

    /* ...but now it has */
    mock_timebase.step();

    /* there will be no timer callback invocation for the expired timer;
     * instead, the timer object is erased */
    counts = glib_wrapper_mock->do_timers();

    if(assume_timer_fired_pending)
        counts.expect(GLibWrapperMock::ArmedCount(1),
                      GLibWrapperMock::FiredCount(0),
                      GLibWrapperMock::ExpiredCount(1));
    else
        counts.expect(GLibWrapperMock::ArmedCount(1));

    cppcut_assert_equal(size_t(1), glib_wrapper_mock->get_number_of_timers());
    cut_assert_true(overrides->is_cacheable(override_id));

    /* let it expire now, again checking in two steps; subtract 2 ms instead
     * only 1 because the timer was created 1 ms ago (mind the step() above) */
    mock_timebase.step(std::chrono::milliseconds(Cacheable::Override::EXPIRY_TIME).count() - 2);

    counts = glib_wrapper_mock->do_timers();
    counts.expect(GLibWrapperMock::ArmedCount(1));
    cut_assert_true(overrides->is_cacheable(override_id));

    mock_timebase.step();

    counts = glib_wrapper_mock->do_timers();
    counts.expect(GLibWrapperMock::FiredCount(1));

    assert_cacheable_defaults(list_ids);
    cut_assert_false(overrides->remove_override(override_id));
    cut_assert_false(overrides->has_overrides());
}

/*!\test
 * In case an override isn't refreshed for some time, it expires and will be
 * removed automatically (assume that timer hasn't fired internally yet).
 */
void test_single_override_does_not_get_removed_if_refreshed__not_fired_yet()
{
    single_override_does_not_get_removed_if_refreshed(false);
}

/*!\test
 * In case an override isn't refreshed for some time, it expires and will be
 * removed automatically (assume that timer has fired internally already).
 */
void test_single_override_does_not_get_removed_if_refreshed__fired_event_pending()
{
    single_override_does_not_get_removed_if_refreshed(true);
}

static void removing_single_override_causes_no_timer_callback(bool assume_timer_fired_pending)
{
    std::array<ID::List, 10> list_ids;

    const ID::List override_id =
        prepare_override_test({ true, true, true, false, false,
                                false, false, true, true, true, },
                              list_ids, 3);

    cut_assert_true(overrides->is_cacheable(list_ids[0]));
    cut_assert_true(overrides->is_cacheable(list_ids[1]));
    cut_assert_true(overrides->is_cacheable(list_ids[2]));
    cut_assert_true(overrides->is_cacheable(list_ids[3]));
    cut_assert_true(overrides->is_cacheable(list_ids[4]));
    cut_assert_true(overrides->is_cacheable(list_ids[5]));
    cut_assert_true(overrides->is_cacheable(list_ids[6]));
    cut_assert_true(overrides->is_cacheable(list_ids[7]));
    cut_assert_true(overrides->is_cacheable(list_ids[8]));
    cut_assert_true(overrides->is_cacheable(list_ids[9]));

    auto counts = glib_wrapper_mock->do_timers();
    counts.expect(GLibWrapperMock::ArmedCount(1));

    mock_timebase.step(std::chrono::milliseconds(Cacheable::Override::EXPIRY_TIME).count() / 2);

    counts = glib_wrapper_mock->do_timers();
    counts.expect(GLibWrapperMock::ArmedCount(1));

    if(assume_timer_fired_pending)
        glib_wrapper_mock->assume_timer_fire_event_is_pending_in_glib_main(override_id);

    cppcut_assert_equal(size_t(1), glib_wrapper_mock->get_number_of_timers());

    cut_assert_true(overrides->remove_override(override_id));
    cut_assert_false(overrides->has_overrides());

    if(assume_timer_fired_pending)
        cppcut_assert_equal(size_t(1), glib_wrapper_mock->get_number_of_timers());
    else
        cppcut_assert_equal(size_t(0), glib_wrapper_mock->get_number_of_timers());

    counts = glib_wrapper_mock->do_timers();

    if(assume_timer_fired_pending)
        counts.expect(GLibWrapperMock::ExpiredCount(1));
    else
        counts.expect_nothing();

    cppcut_assert_equal(size_t(0), glib_wrapper_mock->get_number_of_timers());

    mock_timebase.step(std::chrono::milliseconds(Cacheable::Override::EXPIRY_TIME).count() / 2 + 500);

    counts = glib_wrapper_mock->do_timers();
    counts.expect_nothing();

    cppcut_assert_equal(size_t(0), glib_wrapper_mock->get_number_of_timers());
}

/*!\test
 * Removing an override ensures that no timer callback is invoked (assume that
 * timer hasn't fired internally yet).
 */
void test_removing_single_override_causes_no_timer_callback__fired_event_pending()
{
    removing_single_override_causes_no_timer_callback(false);
}

/*!\test
 * Removing an override ensures that no timer callback is invoked (assume that
 * timer has fired internally already).
 */
void test_removing_single_override_causes_no_timer_callback__not_fired_yet()
{
    removing_single_override_causes_no_timer_callback(true);
}

static ID::List replace_list(std::array<ID::List, 10> &list_ids, size_t list_idx)
{
    std::shared_ptr<LRU::Entry> list(cache->lookup(list_ids[list_idx]));
    ID::List new_id = cache->insert_again(std::move(list));
    cppcut_assert_not_equal(list_ids[list_idx].get_raw_id(), new_id.get_raw_id());

    overrides->list_invalidate(list_ids[list_idx], new_id);
    list_ids[list_idx] = new_id;

    return new_id;
}

/*!\test
 * Replacement of some node does have any effect as long as there are no
 * overrides defined.
 */
void test_list_invalidation__replace_internal_node_with_no_overrides()
{
    std::array<ID::List, 10> list_ids;

    prepare_override_test({ true, true, true, false, false,
                            false, false, true, true, true, },
                          list_ids);

    replace_list(list_ids, 1);
    assert_cacheable_defaults(list_ids);

    replace_list(list_ids, 0);
    assert_cacheable_defaults(list_ids);

    replace_list(list_ids, 4);
    assert_cacheable_defaults(list_ids);
}

/*!\test
 * Replacement of nothingness by something (root list creation) is ignored.
 */
void test_list_invalidation_announce_new_root()
{
    std::array<ID::List, 10> list_ids;

    prepare_override_test({ true, true, true, false, false,
                            false, false, true, true, true, },
                          list_ids);

    overrides->list_invalidate(ID::List(), ID::List(4321));
    assert_cacheable_defaults(list_ids);
}

/*!\test
 * Replacement of root node causes override at root node to be recomputed
 * (completely noncacheable hierarchy).
 */
void test_list_invalidation__replace_root_node__completely_noncacheable()
{
    std::array<ID::List, 10> list_ids;

    prepare_override_test({ false, false, false, false, false,
                            false, false, false, false, false, },
                          list_ids, 0);

    cut_assert_true(overrides->is_cacheable(list_ids[0]));
    cut_assert_true(overrides->is_cacheable(list_ids[1]));
    cut_assert_true(overrides->is_cacheable(list_ids[2]));
    cut_assert_true(overrides->is_cacheable(list_ids[3]));
    cut_assert_true(overrides->is_cacheable(list_ids[4]));
    cut_assert_true(overrides->is_cacheable(list_ids[5]));
    cut_assert_true(overrides->is_cacheable(list_ids[6]));
    cut_assert_true(overrides->is_cacheable(list_ids[7]));
    cut_assert_true(overrides->is_cacheable(list_ids[8]));
    cut_assert_true(overrides->is_cacheable(list_ids[9]));
    cppcut_assert_equal(size_t(1), glib_wrapper_mock->get_number_of_timers());

    replace_list(list_ids, 0);

    cut_assert_true(overrides->is_cacheable(list_ids[0]));
    cut_assert_true(overrides->is_cacheable(list_ids[1]));
    cut_assert_true(overrides->is_cacheable(list_ids[2]));
    cut_assert_true(overrides->is_cacheable(list_ids[3]));
    cut_assert_true(overrides->is_cacheable(list_ids[4]));
    cut_assert_true(overrides->is_cacheable(list_ids[5]));
    cut_assert_true(overrides->is_cacheable(list_ids[6]));
    cut_assert_true(overrides->is_cacheable(list_ids[7]));
    cut_assert_true(overrides->is_cacheable(list_ids[8]));
    cut_assert_true(overrides->is_cacheable(list_ids[9]));
    cppcut_assert_equal(size_t(1), glib_wrapper_mock->get_number_of_timers());
}

/*!\test
 * Replacement of root node causes overrides at internal node to be recomputed
 * (completely noncacheable hierarchy).
 */
void test_list_invalidation__replace_internal_node__completely_noncacheable()
{
    std::array<ID::List, 10> list_ids;

    prepare_override_test({ false, false, false, false, false,
                            false, false, false, false, false, },
                          list_ids, 3);
    put_override_for_id(list_ids[9]);

    cut_assert_true(overrides->is_cacheable(list_ids[0]));
    cut_assert_true(overrides->is_cacheable(list_ids[1]));
    cut_assert_true(overrides->is_cacheable(list_ids[2]));
    cut_assert_true(overrides->is_cacheable(list_ids[3]));
    cut_assert_true(overrides->is_cacheable(list_ids[4]));
    cut_assert_true(overrides->is_cacheable(list_ids[5]));
    cut_assert_true(overrides->is_cacheable(list_ids[6]));
    cut_assert_false(overrides->is_cacheable(list_ids[7]));
    cut_assert_false(overrides->is_cacheable(list_ids[8]));
    cut_assert_true(overrides->is_cacheable(list_ids[9]));
    cppcut_assert_equal(size_t(2), glib_wrapper_mock->get_number_of_timers());

    replace_list(list_ids, 0);

    cut_assert_true(overrides->is_cacheable(list_ids[0]));
    cut_assert_true(overrides->is_cacheable(list_ids[1]));
    cut_assert_true(overrides->is_cacheable(list_ids[2]));
    cut_assert_true(overrides->is_cacheable(list_ids[3]));
    cut_assert_true(overrides->is_cacheable(list_ids[4]));
    cut_assert_true(overrides->is_cacheable(list_ids[5]));
    cut_assert_true(overrides->is_cacheable(list_ids[6]));
    cut_assert_false(overrides->is_cacheable(list_ids[7]));
    cut_assert_false(overrides->is_cacheable(list_ids[8]));
    cut_assert_true(overrides->is_cacheable(list_ids[9]));
    cppcut_assert_equal(size_t(2), glib_wrapper_mock->get_number_of_timers());

    cut_assert_true(overrides->remove_override(list_ids[3]));

    cut_assert_true(overrides->is_cacheable(list_ids[0]));
    cut_assert_false(overrides->is_cacheable(list_ids[1]));
    cut_assert_false(overrides->is_cacheable(list_ids[2]));
    cut_assert_false(overrides->is_cacheable(list_ids[3]));
    cut_assert_false(overrides->is_cacheable(list_ids[4]));
    cut_assert_false(overrides->is_cacheable(list_ids[5]));
    cut_assert_false(overrides->is_cacheable(list_ids[6]));
    cut_assert_false(overrides->is_cacheable(list_ids[7]));
    cut_assert_false(overrides->is_cacheable(list_ids[8]));
    cut_assert_true(overrides->is_cacheable(list_ids[9]));
    cppcut_assert_equal(size_t(1), glib_wrapper_mock->get_number_of_timers());
}

/*!\test
 * Replacement of nodes causes overrides to be recomputed.
 */
void test_list_invalidation__replace_internal_node()
{
    std::array<ID::List, 10> list_ids;

    auto override_id =
        prepare_override_test({ true, true, true, false, false,
                                false, false, true, true, true, },
                              list_ids, 5);

    cut_assert_true(overrides->is_cacheable(list_ids[0]));
    cut_assert_true(overrides->is_cacheable(list_ids[1]));
    cut_assert_true(overrides->is_cacheable(list_ids[2]));
    cut_assert_true(overrides->is_cacheable(list_ids[3]));
    cut_assert_false(overrides->is_cacheable(list_ids[4]));
    cut_assert_true(overrides->is_cacheable(list_ids[5]));
    cut_assert_true(overrides->is_cacheable(list_ids[6]));
    cut_assert_true(overrides->is_cacheable(list_ids[7]));
    cut_assert_true(overrides->is_cacheable(list_ids[8]));
    cut_assert_true(overrides->is_cacheable(list_ids[9]));
    cppcut_assert_equal(size_t(1), glib_wrapper_mock->get_number_of_timers());

    replace_list(list_ids, 4);

    cut_assert_true(overrides->is_cacheable(list_ids[0]));
    cut_assert_true(overrides->is_cacheable(list_ids[1]));
    cut_assert_true(overrides->is_cacheable(list_ids[2]));
    cut_assert_true(overrides->is_cacheable(list_ids[3]));
    cut_assert_false(overrides->is_cacheable(list_ids[4]));
    cut_assert_true(overrides->is_cacheable(list_ids[5]));
    cut_assert_true(overrides->is_cacheable(list_ids[6]));
    cut_assert_true(overrides->is_cacheable(list_ids[7]));
    cut_assert_true(overrides->is_cacheable(list_ids[8]));
    cut_assert_true(overrides->is_cacheable(list_ids[9]));
    cppcut_assert_equal(size_t(1), glib_wrapper_mock->get_number_of_timers());

    replace_list(list_ids, 3);

    cut_assert_true(overrides->is_cacheable(list_ids[0]));
    cut_assert_true(overrides->is_cacheable(list_ids[1]));
    cut_assert_true(overrides->is_cacheable(list_ids[2]));
    cut_assert_true(overrides->is_cacheable(list_ids[3]));
    cut_assert_false(overrides->is_cacheable(list_ids[4]));
    cut_assert_true(overrides->is_cacheable(list_ids[5]));
    cut_assert_true(overrides->is_cacheable(list_ids[6]));
    cut_assert_true(overrides->is_cacheable(list_ids[7]));
    cut_assert_true(overrides->is_cacheable(list_ids[8]));
    cut_assert_true(overrides->is_cacheable(list_ids[9]));
    cppcut_assert_equal(size_t(1), glib_wrapper_mock->get_number_of_timers());

    replace_list(list_ids, 2);

    cut_assert_true(overrides->is_cacheable(list_ids[0]));
    cut_assert_true(overrides->is_cacheable(list_ids[1]));
    cut_assert_true(overrides->is_cacheable(list_ids[2]));
    cut_assert_true(overrides->is_cacheable(list_ids[3]));
    cut_assert_false(overrides->is_cacheable(list_ids[4]));
    cut_assert_true(overrides->is_cacheable(list_ids[5]));
    cut_assert_true(overrides->is_cacheable(list_ids[6]));
    cut_assert_true(overrides->is_cacheable(list_ids[7]));
    cut_assert_true(overrides->is_cacheable(list_ids[8]));
    cut_assert_true(overrides->is_cacheable(list_ids[9]));
    cppcut_assert_equal(size_t(1), glib_wrapper_mock->get_number_of_timers());

    replace_list(list_ids, 6);

    cut_assert_true(overrides->is_cacheable(list_ids[0]));
    cut_assert_true(overrides->is_cacheable(list_ids[1]));
    cut_assert_true(overrides->is_cacheable(list_ids[2]));
    cut_assert_true(overrides->is_cacheable(list_ids[3]));
    cut_assert_false(overrides->is_cacheable(list_ids[4]));
    cut_assert_true(overrides->is_cacheable(list_ids[5]));
    cut_assert_true(overrides->is_cacheable(list_ids[6]));
    cut_assert_true(overrides->is_cacheable(list_ids[7]));
    cut_assert_true(overrides->is_cacheable(list_ids[8]));
    cut_assert_true(overrides->is_cacheable(list_ids[9]));
    cppcut_assert_equal(size_t(1), glib_wrapper_mock->get_number_of_timers());

    override_id = replace_list(list_ids, 5);

    cut_assert_true(overrides->is_cacheable(list_ids[0]));
    cut_assert_true(overrides->is_cacheable(list_ids[1]));
    cut_assert_true(overrides->is_cacheable(list_ids[2]));
    cut_assert_true(overrides->is_cacheable(list_ids[3]));
    cut_assert_false(overrides->is_cacheable(list_ids[4]));
    cut_assert_true(overrides->is_cacheable(list_ids[5]));
    cut_assert_true(overrides->is_cacheable(list_ids[6]));
    cut_assert_true(overrides->is_cacheable(list_ids[7]));
    cut_assert_true(overrides->is_cacheable(list_ids[8]));
    cut_assert_true(overrides->is_cacheable(list_ids[9]));
    cppcut_assert_equal(size_t(1), glib_wrapper_mock->get_number_of_timers());

    replace_list(list_ids, 0);

    cut_assert_true(overrides->is_cacheable(list_ids[0]));
    cut_assert_true(overrides->is_cacheable(list_ids[1]));
    cut_assert_true(overrides->is_cacheable(list_ids[2]));
    cut_assert_true(overrides->is_cacheable(list_ids[3]));
    cut_assert_false(overrides->is_cacheable(list_ids[4]));
    cut_assert_true(overrides->is_cacheable(list_ids[5]));
    cut_assert_true(overrides->is_cacheable(list_ids[6]));
    cut_assert_true(overrides->is_cacheable(list_ids[7]));
    cut_assert_true(overrides->is_cacheable(list_ids[8]));
    cut_assert_true(overrides->is_cacheable(list_ids[9]));
    cppcut_assert_equal(size_t(1), glib_wrapper_mock->get_number_of_timers());

    replace_list(list_ids, 9);

    cut_assert_true(overrides->is_cacheable(list_ids[0]));
    cut_assert_true(overrides->is_cacheable(list_ids[1]));
    cut_assert_true(overrides->is_cacheable(list_ids[2]));
    cut_assert_true(overrides->is_cacheable(list_ids[3]));
    cut_assert_false(overrides->is_cacheable(list_ids[4]));
    cut_assert_true(overrides->is_cacheable(list_ids[5]));
    cut_assert_true(overrides->is_cacheable(list_ids[6]));
    cut_assert_true(overrides->is_cacheable(list_ids[7]));
    cut_assert_true(overrides->is_cacheable(list_ids[8]));
    cut_assert_true(overrides->is_cacheable(list_ids[9]));
    cppcut_assert_equal(size_t(1), glib_wrapper_mock->get_number_of_timers());

    replace_list(list_ids, 1);

    cut_assert_true(overrides->is_cacheable(list_ids[0]));
    cut_assert_true(overrides->is_cacheable(list_ids[1]));
    cut_assert_true(overrides->is_cacheable(list_ids[2]));
    cut_assert_true(overrides->is_cacheable(list_ids[3]));
    cut_assert_false(overrides->is_cacheable(list_ids[4]));
    cut_assert_true(overrides->is_cacheable(list_ids[5]));
    cut_assert_true(overrides->is_cacheable(list_ids[6]));
    cut_assert_true(overrides->is_cacheable(list_ids[7]));
    cut_assert_true(overrides->is_cacheable(list_ids[8]));
    cut_assert_true(overrides->is_cacheable(list_ids[9]));
    cppcut_assert_equal(size_t(1), glib_wrapper_mock->get_number_of_timers());
}

static void removed_object(std::array<ID::List, 10> &list_ids,
                           ID::List removed_id)
{
    overrides->list_invalidate(removed_id, ID::List());

    for(auto &id : list_ids)
    {
        if(id == removed_id)
            id = ID::List();
    }
}

static void make_traversable(std::array<ID::List, 10> list_ids)
{
    for(const auto &id : list_ids)
    {
        auto e = cache->lookup(id);
        cppcut_assert_not_null(e.get());

        auto p = std::static_pointer_cast<Object>(e->get_parent());

        if(p != nullptr)
            p->children_.push_back(e->get_cache_id());
    }

    for(const auto &id : list_ids)
    {
        auto e = std::static_pointer_cast<Object>(cache->lookup(id));
        cppcut_assert_not_null(e.get());
        cppcut_assert_equal(e->get_number_of_children(), e->children_.size());
    }
}

/*!\test
 * Removal of a subtree with overrides causes removal of attached overrides.
 */
void test_list_invalidation_remove_subtree_with_overrides()
{
    std::array<ID::List, 10> list_ids;

    cache->set_callbacks([] {}, [] {},
                         [&list_ids] (ID::List id) { removed_object(list_ids, id); },
                         [] {});

    prepare_override_test({ true, true, true, true, false,
                            false, false, true, true, false, },
                          list_ids, 5);
    put_override_for_id(list_ids[9]);

    make_traversable(list_ids);

    cut_assert_true(overrides->is_cacheable(list_ids[0]));
    cut_assert_true(overrides->is_cacheable(list_ids[1]));
    cut_assert_true(overrides->is_cacheable(list_ids[2]));
    cut_assert_true(overrides->is_cacheable(list_ids[3]));
    cut_assert_false(overrides->is_cacheable(list_ids[4]));
    cut_assert_true(overrides->is_cacheable(list_ids[5]));
    cut_assert_true(overrides->is_cacheable(list_ids[6]));
    cut_assert_true(overrides->is_cacheable(list_ids[7]));
    cut_assert_true(overrides->is_cacheable(list_ids[8]));
    cut_assert_true(overrides->is_cacheable(list_ids[9]));
    cppcut_assert_equal(size_t(2), glib_wrapper_mock->get_number_of_timers());

    std::vector<ID::List> kill_list;
    cache->lookup(list_ids[2])->enumerate_tree_of_sublists(*cache, kill_list);
    cache->toposort_for_purge(kill_list.begin(), kill_list.end());
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_IMPORTANT, "Purge entry 134217735");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_IMPORTANT, "Purge entry 134217733");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_IMPORTANT, "Purge entry 134217734");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_IMPORTANT, "Purge entry 4");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_IMPORTANT, "Purge entry 3");
    cache->purge_entries(kill_list.begin(), kill_list.end());

    cut_assert_true(overrides->is_cacheable(list_ids[0]));
    cut_assert_true(overrides->is_cacheable(list_ids[1]));
    cut_assert_false(overrides->is_cacheable(list_ids[2]));
    cut_assert_false(overrides->is_cacheable(list_ids[3]));
    cut_assert_false(overrides->is_cacheable(list_ids[4]));
    cut_assert_false(overrides->is_cacheable(list_ids[5]));
    cut_assert_false(overrides->is_cacheable(list_ids[6]));
    cut_assert_true(overrides->is_cacheable(list_ids[7]));
    cut_assert_true(overrides->is_cacheable(list_ids[8]));
    cut_assert_true(overrides->is_cacheable(list_ids[9]));
    cppcut_assert_equal(size_t(1), glib_wrapper_mock->get_number_of_timers());
    cppcut_assert_equal(size_t(5), cache->count());

    cache->lookup(list_ids[9])->enumerate_tree_of_sublists(*cache, kill_list);
    cache->toposort_for_purge(kill_list.begin(), kill_list.end());
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_IMPORTANT, "Purge entry 134217738");
    cache->purge_entries(kill_list.begin(), kill_list.end());

    cut_assert_true(overrides->is_cacheable(list_ids[0]));
    cut_assert_true(overrides->is_cacheable(list_ids[1]));
    cut_assert_false(overrides->is_cacheable(list_ids[2]));
    cut_assert_false(overrides->is_cacheable(list_ids[3]));
    cut_assert_false(overrides->is_cacheable(list_ids[4]));
    cut_assert_false(overrides->is_cacheable(list_ids[5]));
    cut_assert_false(overrides->is_cacheable(list_ids[6]));
    cut_assert_true(overrides->is_cacheable(list_ids[7]));
    cut_assert_true(overrides->is_cacheable(list_ids[8]));
    cut_assert_false(overrides->is_cacheable(list_ids[9]));
    cppcut_assert_equal(size_t(0), glib_wrapper_mock->get_number_of_timers());
    cppcut_assert_equal(size_t(4), cache->count());
}

}

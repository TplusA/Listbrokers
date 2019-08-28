/*
 * Copyright (C) 2015, 2016, 2018, 2019  T+A elektroakustik GmbH & Co. KG
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
#include <array>

#include "mock_messages.hh"
#include "mock_timebase.hh"

#include "lru.hh"

constexpr const uint32_t LRU::CacheIdGenerator::ID_MAX;

/*!
 * \addtogroup lru_cache_tests Unit tests
 * \ingroup lru_cache
 *
 * LRU cache unit tests.
 */
/*!@{*/

static MockTimebase mock_timebase;
Timebase *LRU::timebase = &mock_timebase;

class ObliviateExpectations
{
  private:
    class Expectation
    {
      public:
        const ID::List parent_id_;
        const ID::List child_id_;
        const LRU::Entry *const child_;
        const bool expecting_child_;

        Expectation(const Expectation &) = delete;
        Expectation &operator=(const Expectation &) = delete;

        explicit Expectation(ID::List parent_id, ID::List child_id,
                             const LRU::Entry *child):
            parent_id_(parent_id),
            child_id_(child_id),
            child_(child),
            expecting_child_(child != nullptr)
        {}

        explicit Expectation(ID::List parent_id, ID::List child_id,
                             bool expecting_child):
            parent_id_(parent_id),
            child_id_(child_id),
            child_(nullptr),
            expecting_child_(expecting_child)
        {}

        Expectation(Expectation &&) = default;
    };

    typedef MockExpectationsTemplate<Expectation> MockExpectations;
    MockExpectations *expectations_;

  public:
    ObliviateExpectations(const ObliviateExpectations &) = delete;
    ObliviateExpectations &operator=(const ObliviateExpectations &) = delete;

    explicit ObliviateExpectations()
    {
        expectations_ = new MockExpectations();
    }

    ~ObliviateExpectations()
    {
        delete expectations_;
    }

    void init()
    {
        cppcut_assert_not_null(expectations_);
        expectations_->init();
    }

    void check()
    {
        cppcut_assert_not_null(expectations_);
        expectations_->check();
    }

    void expect_obliviate_child(ID::List parent_id, ID::List child_id,
                                const LRU::Entry *child)
    {
        expectations_->add(Expectation(parent_id, child_id, child));
    }

    void expect_obliviate_child(ID::List parent_id, ID::List child_id,
                                bool expecting_child = true)
    {
        expectations_->add(Expectation(parent_id, child_id, expecting_child));
    }

    void obliviate_child(ID::List parent_id, ID::List child_id, const LRU::Entry *child)
    {
        const auto &expect(expectations_->get_next_expectation(__func__));

        cppcut_assert_equal(expect.parent_id_.get_raw_id(), parent_id.get_raw_id());
        cppcut_assert_equal(expect.child_id_.get_raw_id(), child_id.get_raw_id());

        if(expect.expecting_child_)
        {
            cppcut_assert_not_null(child);

            if(expect.child_ != nullptr)
                cppcut_assert_equal(expect.child_, child);
        }
        else
        {
            cppcut_assert_null(child);
            cppcut_assert_null(expect.child_);
        }
    }
};

static ObliviateExpectations *obliviate_expectations;

class Object: public LRU::Entry
{
  public:
    const unsigned int dummy_data_;

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
        cut_fail("Unexpected subtree enumeration");
    }

    void enumerate_direct_sublists(const LRU::Cache &cache,
                                   std::vector<ID::List> &nodes) const override
    {
        cut_fail("Unexpected child list enumeration");
    }

    void obliviate_child(ID::List child_id, const LRU::Entry *child) override
    {
        obliviate_expectations->obliviate_child(get_cache_id(),
                                                child_id, child);
    }
};

template <typename T>
static void assert_equal_times(const T &expected, const T &value)
{
    cppcut_assert_equal(expected.count(), value.count());
}

namespace lru_cacheentry_tests
{

static MockMessages *mock_messages;

static LRU::Cache *cache;
static constexpr size_t maximum_number_of_objects = 10;
static std::shared_ptr<LRU::Entry> root;

void cut_setup(void)
{
    obliviate_expectations = new ObliviateExpectations;
    cppcut_assert_not_null(obliviate_expectations);
    obliviate_expectations->init();

    mock_messages = new MockMessages;
    cppcut_assert_not_null(mock_messages);
    mock_messages->init();
    mock_messages_singleton = mock_messages;

    mock_messages->ignore_messages_with_level_or_above(MESSAGE_LEVEL_TRACE);

    mock_timebase.reset();

    cache = new LRU::Cache(500, maximum_number_of_objects,
                           std::chrono::minutes(1));
    cppcut_assert_not_null(cache);
    cache->set_callbacks([]{}, []{}, [] (ID::List id) {}, []{});
    cppcut_assert_equal(size_t(0), cache->count());

    root = std::make_shared<Object>(nullptr);
    cppcut_assert_not_null(root.get());

    cppcut_assert_not_equal(0U, cache->insert(std::move(root),
                                              LRU::CacheMode::CACHED, 0, 90).get_raw_id());
    cppcut_assert_equal(size_t(1), cache->count());
}

void cut_teardown(void)
{
    cache->self_check();

    delete cache;

    root = nullptr;
    cache = nullptr;

    obliviate_expectations->check();
    delete obliviate_expectations;
    obliviate_expectations = nullptr;

    mock_messages->check();
    mock_messages_singleton = nullptr;
    delete mock_messages;
    mock_messages = nullptr;
}

/*!\test
 * The single cached object has a depth of 1.
 */
void test_depth_of_root_object_is_one(void)
{
    cppcut_assert_equal(size_t(1), LRU::Entry::depth(*root));
}

/*!\test
 * Explicit use of a cached object.
 */
void test_use_object(void)
{
    assert_equal_times(std::chrono::milliseconds(0), root->get_age());

    cache->use(root);
    assert_equal_times(std::chrono::milliseconds(0), root->get_age());

    mock_timebase.step();
    assert_equal_times(std::chrono::milliseconds(1), root->get_age());

    cache->use(root);
    assert_equal_times(std::chrono::milliseconds(0), root->get_age());

    mock_timebase.step(999UL);
    assert_equal_times(std::chrono::milliseconds(999), root->get_age());

    mock_timebase.step(1UL);
    assert_equal_times(std::chrono::milliseconds(1000), root->get_age());

    cache->use(root);
    assert_equal_times(std::chrono::milliseconds(0), root->get_age());
}

/*!\test
 * Setting size of object with nonexistent object ID returns an error.
 */
void test_change_size_of_non_existent_object_fails(void)
{
    cut_assert_false(cache->set_object_size(ID::List(2), 10));
}


/*!\test
 * Changing the size of an object is considered a use of that object.
 */
void test_change_size_of_object_is_use_of_object(void)
{
    cut_assert_true(cache->set_object_size(root->get_cache_id(), 10));
    mock_timebase.step(500);
    assert_equal_times(std::chrono::milliseconds(500), root->get_age());
    cut_assert_true(cache->set_object_size(root->get_cache_id(), 150));
    assert_equal_times(std::chrono::milliseconds(0), root->get_age());
}

/*!\test
 * Setting the size of an object to its same size is considered a use of that
 * object.
 */
void test_set_same_size_of_object_is_use_of_object(void)
{
    cut_assert_true(cache->set_object_size(root->get_cache_id(), 10));
    mock_timebase.step(500);
    assert_equal_times(std::chrono::milliseconds(500), root->get_age());
    cut_assert_true(cache->set_object_size(root->get_cache_id(), 10));
    assert_equal_times(std::chrono::milliseconds(0), root->get_age());
}

/*!\test
 * Garbage collection is triggered when exceeding memory limits.
 *
 * The hot object is discarded when exceeding the hard memory limit for the
 * sake of overall system stability, even if this means running into
 * user-visible side-effects.
 */
void test_exceeding_memory_limits_by_setting_size_of_object_triggers_gc(void)
{
    cut_assert_true(cache->set_object_size(root->get_cache_id(), 450));
    mock_messages->check();

    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_IMPORTANT,
                                              "Soft memory limit exceeded by new size 451 of object 1, attempting to collect garbage");
    cut_assert_true(cache->set_object_size(root->get_cache_id(), 451));

    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_IMPORTANT,
                                              "Soft memory limit exceeded by new size 500 of object 1, attempting to collect garbage");
    cut_assert_true(cache->set_object_size(root->get_cache_id(), 500));

    cppcut_assert_equal(size_t(1), cache->count());
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_IMPORTANT,
                                              "Hard memory limit exceeded by new size 501 of object 1, attempting to collect garbage");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_IMPORTANT,
                                              "Discarding hot object 1 (size exceeded, count not exceeded)");
    cut_assert_true(cache->set_object_size(root->get_cache_id(), 501));
    cppcut_assert_equal(size_t(0), cache->count());
}

};


namespace lru_cache_tests
{

static MockMessages *mock_messages;

static LRU::Cache *cache;
static constexpr size_t maximum_number_of_objects = 500;
static constexpr std::chrono::minutes maximum_object_age_minutes(5);

void cut_setup(void)
{
    obliviate_expectations = new ObliviateExpectations;
    cppcut_assert_not_null(obliviate_expectations);
    obliviate_expectations->init();

    mock_messages = new MockMessages;
    cppcut_assert_not_null(mock_messages);
    mock_messages->init();
    mock_messages_singleton = mock_messages;

    mock_messages->ignore_messages_with_level_or_above(MESSAGE_LEVEL_TRACE);

    mock_timebase.reset();

    cache = new LRU::Cache(1024UL * 1024UL, maximum_number_of_objects,
                           std::chrono::minutes(maximum_object_age_minutes));
    cppcut_assert_not_null(cache);
    cache->set_callbacks([]{}, []{}, [] (ID::List id) {}, []{});
    cppcut_assert_equal(size_t(0), cache->count());
}

void cut_teardown(void)
{
    cache->self_check();

    delete cache;

    cache = nullptr;

    obliviate_expectations->check();
    delete obliviate_expectations;
    obliviate_expectations = nullptr;

    mock_messages->check();
    mock_messages_singleton = nullptr;
    delete mock_messages;
    mock_messages = nullptr;
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
    const ID::List id = cache->insert(std::shared_ptr<LRU::Entry>(obj),
                                      cmode, 0, object_size);
    cppcut_assert_operator(0U, <, id.get_raw_id());
    cppcut_assert_equal(id.get_raw_id(), obj->get_cache_id().get_raw_id());
    cppcut_assert_equal(previous_count + 1U, cache->count());

    if(object_id)
        *object_id = id;

    return obj;
}

static std::shared_ptr<LRU::Entry>
add_object(const std::shared_ptr<LRU::Entry> &parent, unsigned int data = 0,
           ID::List *object_id = nullptr, size_t object_size = 10)
{
    return add_cached_or_uncached_object(parent, LRU::CacheMode::CACHED,
                                         data, object_id, object_size);
}

static std::shared_ptr<LRU::Entry>
add_uncached_object(const std::shared_ptr<LRU::Entry> &parent, unsigned int data = 0,
                    ID::List *object_id = nullptr, size_t object_size = 12)
{
    return add_cached_or_uncached_object(parent, LRU::CacheMode::UNCACHED,
                                         data, object_id, object_size);
}

static void check_aging_list(const unsigned int *const expected_data,
                             size_t expected_number_of_objects)
{
    size_t i = 0;
    auto previous_age = std::chrono::milliseconds::max();

    for(auto it = cache->begin(); it != cache->end(); ++it)
    {
        auto obj = static_cast<const Object *>(&*it);
        cppcut_assert_operator(expected_number_of_objects, >, i);
        cppcut_assert_equal(expected_data[i++], obj->dummy_data_);
        cppcut_assert_operator(obj->get_age().count(), <=, previous_age.count());
        previous_age = obj->get_age();
    }

    cppcut_assert_equal(expected_number_of_objects, i);

    i = 0;
    previous_age = std::chrono::milliseconds::min();

    for(auto it = cache->rbegin(); it != cache->rend(); ++it)
    {
        auto obj = static_cast<const Object *>((&*it));
        cppcut_assert_operator(expected_number_of_objects, >, i);
        cppcut_assert_equal(expected_data[expected_number_of_objects - i - 1], obj->dummy_data_);
        i++;
        cppcut_assert_operator(obj->get_age().count(), >=, previous_age.count());
        previous_age = obj->get_age();
    }

    cppcut_assert_equal(expected_number_of_objects, i);
}

template <size_t N>
static void check_aging_list(const std::array<unsigned int, N> &expected_data)
{
    check_aging_list(expected_data.begin(), N);
}

/*!\test
 * Look up objects in empty cache returns nullptr.
 */
void test_lookup_in_empty_cache(void)
{
    cppcut_assert_equal(size_t(0), cache->count());

    auto obj = cache->lookup(ID::List(1));
    cppcut_assert_null(obj.get());

    obj = cache->lookup(ID::List(2));
    cppcut_assert_null(obj.get());

    obj = cache->lookup(ID::List(maximum_number_of_objects - 1));
    cppcut_assert_null(obj.get());

    obj = cache->lookup(ID::List(maximum_number_of_objects));
    cppcut_assert_null(obj.get());

    obj = cache->lookup(ID::List(maximum_number_of_objects + 1));
    cppcut_assert_null(obj.get());

    obj = cache->lookup(ID::List(INT32_MAX));
    cppcut_assert_null(obj.get());

    obj = cache->lookup(ID::List(UINT32_MAX));
    cppcut_assert_null(obj.get());
}

/*!\test
 * Setting size of object in empty cache fails.
 */
void test_set_size_in_empty_cache(void)
{
    cppcut_assert_equal(size_t(0), cache->count());

    cut_assert_false(cache->set_object_size(ID::List(1), 10));
    cut_assert_false(cache->set_object_size(ID::List(2), 10));
    cut_assert_false(cache->set_object_size(ID::List(maximum_number_of_objects - 1), 10));
    cut_assert_false(cache->set_object_size(ID::List(maximum_number_of_objects), 10));
    cut_assert_false(cache->set_object_size(ID::List(maximum_number_of_objects + 1), 10));
    cut_assert_false(cache->set_object_size(ID::List(INT32_MAX), 10));
    cut_assert_false(cache->set_object_size(ID::List(UINT32_MAX), 10));
}

/*!\test
 * Attempting to garbage collect empty cache returns infinity.
 */
void test_gc_on_empty_cache_returns_max_seconds(void)
{
    cppcut_assert_equal(size_t(0), cache->count());

    std::chrono::seconds duration = cache->gc();
    assert_equal_times(std::chrono::seconds::max(), duration);
}

/*!\test
 * Insertion of first element.
 */
void test_insert_and_lookup_one_element(void)
{
    ID::List id;
    cut_assert_false(id.is_valid());
    std::shared_ptr<LRU::Entry> obj = add_object(nullptr, 123, &id, 321);

    cut_assert_true(id.is_valid());

    auto found = cache->lookup(id);
    cppcut_assert_equal(obj.get(), found.get());

    auto dynamic_cast_obj = dynamic_cast<const Object *>(found.get());
    cppcut_assert_not_null(dynamic_cast_obj);

    auto found_obj = static_cast<const Object *>(found.get());

    cppcut_assert_equal(static_cast<LRU::Entry *>(nullptr),
                        found_obj->get_parent().get());
    cppcut_assert_equal(123U, found_obj->dummy_data_);
    cppcut_assert_equal(id.get_raw_id(), found_obj->get_cache_id().get_raw_id());
}

/*!\test
 * Insertion of three elements.
 */
void test_insert_and_lookup_multiple_elements(void)
{
    static const std::array<unsigned int, 3> data_array = {123, 456, 789};
    std::array<ID::List, data_array.size()> ids;
    std::array<std::shared_ptr<LRU::Entry>, data_array.size()> objs;

    objs[0] = add_object(nullptr, data_array[0], &ids[0], 321);
    objs[1] = add_object(objs[0], data_array[1], &ids[1], 654);
    objs[2] = add_object(objs[1], data_array[2], &ids[2], 987);

    for(size_t i = 0; i < ids.size(); ++i)
    {
        auto found = cache->lookup(ids[i]);
        cppcut_assert_equal(objs[i].get(), found.get());

        auto dynamic_cast_obj = dynamic_cast<const Object *>(found.get());
        cppcut_assert_not_null(dynamic_cast_obj);

        auto found_obj = static_cast<const Object *>(found.get());

        if(i == 0)
            cppcut_assert_equal(static_cast<LRU::Entry *>(nullptr),
                                found_obj->get_parent().get());
        else
            cppcut_assert_equal(objs[i - 1].get(),
                                found_obj->get_parent().get());

        cppcut_assert_equal(data_array[i], found_obj->dummy_data_);
        cppcut_assert_equal(ids[i].get_raw_id(), found_obj->get_cache_id().get_raw_id());
    }

    check_aging_list(std::array<unsigned int, data_array.size()>({789, 456, 123}));
}

/*!\test
 * Run garbage collection on cache with one fresh (non-gc'ed) entry.
 */
void test_gc_one_fresh_element(void)
{
    ID::List id;
    (void)add_object(nullptr, 123, &id, 20);

    auto duration = cache->gc();
    assert_equal_times(std::chrono::seconds(maximum_object_age_minutes), duration);
    cppcut_assert_equal(size_t(1), cache->count());
}

/*!\test
 * Run garbage collection on cache with one expired entry.
 */
void test_gc_one_expired_element(void)
{
    /* some arbitrary offset just to make sure this works as well */
    mock_timebase.step(std::chrono::milliseconds(std::chrono::seconds(5)).count());

    ID::List id;
    (void)add_object(nullptr, 123, &id, 20);

    auto duration = cache->gc();
    cppcut_assert_equal(size_t(1), cache->count());

    /* one millisecond too early */
    mock_timebase.step(std::chrono::milliseconds(duration).count() - 1);
    duration = cache->gc();
    cppcut_assert_equal(size_t(1), cache->count());

    /* right on time */
    mock_timebase.step();
    duration = cache->gc();
    assert_equal_times(std::chrono::seconds::max(), duration);
    cppcut_assert_equal(size_t(0), cache->count());
}

/*!\test
 * Age of used object is honored during garbage collection.
 */
void test_gc_one_used_and_later_expired_element(void)
{
    ID::List id;
    (void)add_object(nullptr, 123, &id, 20);

    auto duration = cache->gc();
    cppcut_assert_equal(size_t(1), cache->count());

    /* use after 0.1 s */
    mock_timebase.step(100);
    cache->use(id);

    /* one hundred milliseconds too early (because this is what gc() has
     * returned the last it was called) */
    mock_timebase.step(std::chrono::milliseconds(duration).count() - 100);
    duration = cache->gc();
    assert_equal_times(std::chrono::seconds(1), duration);
    cppcut_assert_equal(size_t(1), cache->count());

    /* one millisecond too early to show that the precision is maintained */
    mock_timebase.step(99);

    /* right on time */
    mock_timebase.step();
    duration = cache->gc();
    assert_equal_times(std::chrono::seconds::max(), duration);
    cppcut_assert_equal(size_t(0), cache->count());
}

/*!\test
 * Run garbage collection on cache with multiple expired entries, all at the
 * same time.
 */
void test_gc_multiple_expired_elements_with_equal_ages(void)
{
    ID::List id;
    (void)add_object(nullptr, 123, &id, 321);
    (void)add_object(cache->lookup(id), 456, &id, 654);
    (void)add_object(cache->lookup(id), 789, &id, 987);
    (void)add_object(cache->lookup(id), 246, &id, 642);

    cppcut_assert_equal(size_t(4), cache->count());
    auto duration = cache->gc();
    cppcut_assert_equal(size_t(4), cache->count());
    obliviate_expectations->check();

    mock_timebase.step(std::chrono::milliseconds(duration).count());
    obliviate_expectations->expect_obliviate_child(ID::List(3), ID::List(4));
    obliviate_expectations->expect_obliviate_child(ID::List(2), ID::List(3));
    obliviate_expectations->expect_obliviate_child(ID::List(1), ID::List(2));
    duration = cache->gc();
    assert_equal_times(std::chrono::seconds::max(), duration);
    cppcut_assert_equal(size_t(0), cache->count());
}

/*!\test
 * Run garbage collection on cache with multiple expired entries, all at
 * different times.
 */
void test_gc_multiple_expired_elements_with_unequal_ages(void)
{
    ID::List id;
    std::shared_ptr<LRU::Entry> root = add_object(nullptr, 123, &id, 321);

    /* t = 0 seconds, oldest object */
    (void)add_object(root, 456, &id, 654);

    /* t = 2 seconds */
    mock_timebase.step(2000);
    (void)add_object(root, 789, &id, 987);
    (void)add_object(root, 246, &id, 642);

    /* t = 2.2 seconds */
    mock_timebase.step(200);
    (void)add_object(root, 135, &id, 531);

    cppcut_assert_equal(size_t(5), cache->count());
    auto duration = cache->gc();
    cppcut_assert_equal(size_t(5), cache->count());

    check_aging_list(std::array<unsigned int, 5>({456, 789, 246, 135, 123}));

    /* oldest object's age is 2.2 seconds now, so #LRU::Cache::gc() tells us to
     * check back when that object's age is 0.2 seconds overdue (because of
     * rounding to seconds scale) */
    assert_equal_times(std::chrono::seconds(maximum_object_age_minutes) - std::chrono::seconds(2),
                       duration);

    /* t = 2.999 seconds */
    mock_timebase.step(799);
    duration = cache->gc();

    /* still 2 seconds, the object will be 999 ms overdue then */
    assert_equal_times(std::chrono::seconds(maximum_object_age_minutes) - std::chrono::seconds(2),
                       duration);

    /* t = 3 seconds */
    mock_timebase.step();
    duration = cache->gc();
    assert_equal_times(std::chrono::seconds(maximum_object_age_minutes) - std::chrono::seconds(3),
                       duration);
    obliviate_expectations->check();

    /* t = #maximum_object_age_minutes */
    mock_timebase.step(std::chrono::milliseconds(duration).count());
    obliviate_expectations->expect_obliviate_child(ID::List(1), ID::List(2));
    duration = cache->gc();

    /* objects inserted as third and fourth one were added 2 seconds after the
     * one just discarded was inserted, so they need to be discarded in 2
     * seconds */
    assert_equal_times(std::chrono::seconds(2), duration);
    cppcut_assert_equal(size_t(4), cache->count());

    check_aging_list(std::array<unsigned int, 4>({789, 246, 135, 123}));

    /* just do as #LRU::Cache::gc() tells us */
    mock_timebase.step(std::chrono::milliseconds(duration).count());
    obliviate_expectations->expect_obliviate_child(ID::List(1), ID::List(3));
    obliviate_expectations->expect_obliviate_child(ID::List(1), ID::List(4));
    duration = cache->gc();
    assert_equal_times(std::chrono::seconds(1), duration);
    cppcut_assert_equal(size_t(2), cache->count());

    check_aging_list(std::array<unsigned int, 2>({135, 123}));

    /* just do as #LRU::Cache::gc() tells us */
    mock_timebase.step(std::chrono::milliseconds(duration).count());
    obliviate_expectations->expect_obliviate_child(ID::List(1), ID::List(5));
    duration = cache->gc();
    assert_equal_times(std::chrono::seconds::max(), duration);
    cppcut_assert_equal(size_t(0), cache->count());
}

/*!\test
 * Given an object A that was created before object B, attempting to insert A
 * into the cache after B has been inserted fails.
 */
void test_objects_must_be_inserted_in_order_of_creation_or_use()
{
    std::shared_ptr<LRU::Entry> a = std::make_shared<Object>(nullptr, 5);
    cppcut_assert_not_null(a.get());

    mock_timebase.step();

    std::shared_ptr<LRU::Entry> b = std::make_shared<Object>(nullptr, 6);
    cppcut_assert_not_null(b.get());

    cppcut_assert_equal(size_t(0), cache->count());
    cppcut_assert_equal(1U, cache->insert(std::move(b), LRU::CacheMode::CACHED, 0, 30).get_raw_id());
    cppcut_assert_equal(size_t(1), cache->count());

    mock_messages->expect_msg_error(0, LOG_CRIT,
                                    "BUG: Attempted to insert outdated object into cache");

    cppcut_assert_equal(0U, cache->insert(std::move(a), LRU::CacheMode::CACHED, 0, 30).get_raw_id());
    cppcut_assert_equal(size_t(1), cache->count());
}

/*!\test
 * Given an objects A and B, where A is the parent of B, attempting to insert B
 * into the cache before A has been inserted fails.
 *
 * It succeeds if A is inserted first.
 */
void test_can_only_insert_objects_if_parent_is_also_inserted()
{
    std::shared_ptr<LRU::Entry> a = std::make_shared<Object>(nullptr, 0);
    cppcut_assert_not_null(a.get());

    mock_timebase.step();

    std::shared_ptr<LRU::Entry> b = std::make_shared<Object>(a, 1);
    cppcut_assert_not_null(b.get());

    mock_messages->expect_msg_error(0, LOG_CRIT,
                                    "BUG: Attempted to insert object into cache with unknown parent");

    cppcut_assert_equal(size_t(0), cache->count());
    cppcut_assert_equal(0U, cache->insert(std::shared_ptr<LRU::Entry>(b),
                                          LRU::CacheMode::CACHED, 0, 30).get_raw_id());
    cppcut_assert_equal(size_t(0), cache->count());

    cppcut_assert_equal(1U, cache->insert(std::move(a), LRU::CacheMode::CACHED, 0, 30).get_raw_id());
    cppcut_assert_equal(size_t(1), cache->count());
    cppcut_assert_equal(2U, cache->insert(std::move(b), LRU::CacheMode::CACHED, 0, 30).get_raw_id());
    cppcut_assert_equal(size_t(2), cache->count());
}

/*!\test
 * The trail of oldest to youngest objects must remain correct while inserting
 * objects into the cache.
 */
void test_aging_list_consistency(void)
{
    /* Single object with size 1000:
     * 0 [1000] */
    std::shared_ptr<LRU::Entry> root = add_object(nullptr, 1000);
    check_aging_list(std::array<unsigned int, 1>({1000U}));

    /* 0 [1000]
     * |
     * 0 [500] */
    std::shared_ptr<LRU::Entry> child_left = add_object(root, 500);
    check_aging_list(std::array<unsigned int, 2>({500U, 1000U}));

    /*    1 [1000]
     *   / \___
     *  /      \
     * 0 [500]  1 [300] */
    mock_timebase.step();
    add_object(root, 300);
    check_aging_list(std::array<unsigned int, 3>({500U, 300U, 1000U}));

    /*          2 [1000]
     *   ______/|\______
     *  /       |       \
     * 0 [500]  1 [300]  2 [400] */
    mock_timebase.step();
    std::shared_ptr<LRU::Entry> child_right = add_object(root, 400);
    check_aging_list(std::array<unsigned int, 4>({500U, 300U, 400U, 1000U}));

    /*          3 [1000]
     *   ______/|\______
     *  /       |       \
     * 3 [500]  1 [300]  2 [400]
     * |
     * 3 [800]                   */
    mock_timebase.step();
    add_object(child_left, 800);
    check_aging_list(std::array<unsigned int, 5>({300U, 400U, 800U, 500U, 1000U}));

    /*          4 [1000]
     *   ______/|\______
     *  /       |       \
     * 3 [500]  1 [300]  4 [400]
     * |                 |
     * 3 [800]           4 [600] */
    mock_timebase.step();
    add_object(child_right, 600);
    check_aging_list(std::array<unsigned int, 6>({300U, 800U, 500U, 600U, 400U, 1000U}));

    /*               5 [1000]
     *        ______/|\______
     *       /       |       \
     *      5 [500]  1 [300]  4 [400]
     *   __/\__               |
     *  /      \              |
     * 3 [800]  5 [100]       4 [600] */
    mock_timebase.step();
    std::shared_ptr<LRU::Entry> deeper_child = add_object(child_left, 100);
    check_aging_list(std::array<unsigned int, 7>({300U, 800U, 600U, 400U, 100U, 500U, 1000U}));

    /*               6 [1000]
     *        ______/|\______
     *       /       |       \
     *      6 [500]  1 [300]  4 [400]
     *   __/\__               |
     *  /      \              |
     * 3 [800]  6 [100]       4 [600]
     *          |
     *          6 [200]               */
    mock_timebase.step();
    add_object(deeper_child, 200);
    check_aging_list(std::array<unsigned int, 8>({300U, 800U, 600U, 400U, 200U, 100U, 500U, 1000U}));

    /*               7 [1000]
     *        ______/|\______
     *       /       |       \
     *      6 [500]  7 [300]  4 [400]
     *   __/\__               |
     *  /      \              |
     * 3 [800]  6 [100]       4 [600]
     *          |
     *          6 [200]               */
    mock_timebase.step();
    cache->use(ID::List(3));
    check_aging_list(std::array<unsigned int, 8>({800U, 600U, 400U, 200U, 100U, 500U, 300U, 1000U}));

    /*               8 [1000]
     *        ______/|\______
     *       /       |       \
     *      8 [500]  7 [300]  4 [400]
     *   __/\__               |
     *  /      \              |
     * 3 [800]  8 [100]       4 [600]
     *          |
     *          6 [200]               */
    mock_timebase.step();
    cache->use(ID::List(7));
    check_aging_list(std::array<unsigned int, 8>({800U, 600U, 400U, 200U, 300U, 100U, 500U, 1000U}));

    /*               8 [1000]
     *        ______/|\______
     *       /       |       \
     *      8 [500]  7 [300]  8 [400]
     *   __/\__               |
     *  /      \              |
     * 3 [800]  8 [100]       8 [600]
     *          |
     *          6 [200]               */
    cache->use(ID::List(6));
    check_aging_list(std::array<unsigned int, 8>({800U, 200U, 300U, 100U, 500U, 600U, 400U, 1000U}));

    /*
     * No change in structure (only aging list):
     *
     *               8 [1000]
     *        ______/|\______
     *       /       |       \
     *      8 [500]  7 [300]  8 [400]
     *   __/\__               |
     *  /      \              |
     * 3 [800]  8 [100]       8 [600]
     *          |
     *          6 [200]               */
    cache->use(ID::List(2));
    check_aging_list(std::array<unsigned int, 8>({800U, 200U, 300U, 100U, 600U, 400U, 500U, 1000U}));

    /*               9 [1000]
     *        ______/|\______
     *       /       |       \
     *      8 [500]  7 [300]  9 [400]
     *   __/\__               |
     *  /      \              |
     * 3 [800]  8 [100]       9 [600]
     *          |
     *          6 [200]               */
    mock_timebase.step();
    cache->use(ID::List(6));
    check_aging_list(std::array<unsigned int, 8>({800U, 200U, 300U, 100U, 500U, 600U, 400U, 1000U}));
}

/*!\test
 * Long path with no forks.
 *
 * This is a typical use case where the user knows exactly where he wants to go
 * in a deeply nested directory hierarchy.
 */
void test_browse_deep(void)
{
    ID::List id;
    (void)add_object(nullptr, 10, &id, 500);
    mock_timebase.step(300);
    (void)add_object(cache->lookup(id), 20, &id, 400);
    mock_timebase.step(300);
    (void)add_object(cache->lookup(id), 30, &id, 300);
    mock_timebase.step(300);
    (void)add_object(cache->lookup(id), 40, &id, 200);
    mock_timebase.step(300);
    (void)add_object(cache->lookup(id), 50, &id, 100);

    check_aging_list(std::array<unsigned int, 5>({50U, 40U, 30U, 20U, 10U}));
}

static void check_reinsert_expectations(const std::array<ID::List, 3> &expected_ids)
{
    static const std::array<unsigned int, 3> expected_aging_list_data({42, 5, 23});
    static const std::array<long, 3> expected_ages({ 2, 1, 1});

    check_aging_list(expected_aging_list_data);

    auto ids_iter = expected_ids.begin();
    auto age_iter = expected_ages.begin();

    for(auto it = cache->begin(); it != cache->end(); ++it)
    {
        auto obj = static_cast<const Object *>(&*it);

        cppcut_assert_equal(ids_iter->get_raw_id(), obj->get_cache_id().get_raw_id());
        cut_assert_true(*ids_iter++ == obj->get_cache_id());
        cppcut_assert_equal(*age_iter++, obj->get_age().count());
    }

}

static void reinsert_prepare(std::array<ID::List, 3> &all_ids,
                             std::array<std::shared_ptr<LRU::Entry>, 3> &all_lists)
{
    all_lists[2] = add_object(nullptr, 23, &all_ids[2]);
    mock_timebase.step();
    all_lists[0] = add_object(all_lists[2], 42, &all_ids[0]);
    mock_timebase.step();
    all_lists[1] = add_object(all_lists[2], 5, &all_ids[1]);
    mock_timebase.step();

    check_reinsert_expectations(all_ids);
}

/*!\test
 * Insert root list into cache once again to obtain new ID.
 */
void test_reinsert_root_list()
{
    std::array<ID::List, 3> all_ids;
    std::array<std::shared_ptr<LRU::Entry>, 3> all_lists;

    reinsert_prepare(all_ids, all_lists);

    /* just make sure that we really have the root list */
    cppcut_assert_null(all_lists[2]->get_parent().get());

    ID::List new_id = cache->insert_again(std::shared_ptr<LRU::Entry>(all_lists[2]));

    cut_assert_true(new_id.is_valid());
    cut_assert_true(new_id != all_ids[0]);
    cut_assert_true(new_id != all_ids[1]);
    cut_assert_true(new_id != all_ids[2]);

    cppcut_assert_equal(all_lists[2], cache->lookup(new_id));
    cppcut_assert_equal(all_lists[0], cache->lookup(all_ids[0]));
    cppcut_assert_equal(all_lists[1], cache->lookup(all_ids[1]));
    cppcut_assert_equal(std::shared_ptr<LRU::Entry>(nullptr), cache->lookup(all_ids[2]));

    all_ids[2] = new_id;

    check_reinsert_expectations(all_ids);
}

/*!\test
 * Insert inner node list into cache once again to obtain new ID.
 */
void test_reinsert_inner_list()
{
    std::array<ID::List, 3> all_ids;
    std::array<std::shared_ptr<LRU::Entry>, 3> all_lists;

    reinsert_prepare(all_ids, all_lists);

    /* just make sure that we do not have the root list */
    cppcut_assert_not_null(all_lists[0]->get_parent().get());

    ID::List new_id = cache->insert_again(std::shared_ptr<LRU::Entry>(all_lists[0]));

    cut_assert_true(new_id.is_valid());
    cut_assert_true(new_id != all_ids[0]);
    cut_assert_true(new_id != all_ids[1]);
    cut_assert_true(new_id != all_ids[2]);

    cppcut_assert_equal(all_lists[2], cache->lookup(all_ids[2]));
    cppcut_assert_equal(all_lists[0], cache->lookup(new_id));
    cppcut_assert_equal(all_lists[1], cache->lookup(all_ids[1]));
    cppcut_assert_equal(std::shared_ptr<LRU::Entry>(nullptr), cache->lookup(all_ids[0]));

    all_ids[0] = new_id;

    check_reinsert_expectations(all_ids);
}

/*!\test
 * Attempting to re-insert a nullptr list doesn't work.
 */
void test_reinsert_null_list_returns_invalid_id()
{
    (void)add_object(nullptr);

    ID::List id = cache->insert_again(nullptr);
    cut_assert_false(id.is_valid());
}

/*!\test
 * Attempting to re-insert a nullptr list into an empty cache doesn't work.
 */
void test_reinsert_null_list_into_empty_cache_returns_invalid_id()
{
    ID::List id = cache->insert_again(nullptr);
    cut_assert_false(id.is_valid());
}

/*!\test
 * Attempting to re-insert uncached list into cache doesn't work.
 */
void test_reinsert_uncached_list_returns_invalid_id()
{
    (void)add_object(nullptr);

    auto list = std::make_shared<Object>(nullptr);
    cppcut_assert_not_null(list.get());

    ID::List id = cache->insert_again(list);
    cut_assert_false(id.is_valid());
}

/*!\test
 * Attempting to re-insert uncached list into empty cache doesn't work.
 */
void test_reinsert_uncached_list_into_empty_cache_returns_invalid_id()
{
    auto list = std::make_shared<Object>(nullptr);
    cppcut_assert_not_null(list.get());

    ID::List id = cache->insert_again(list);
    cut_assert_false(id.is_valid());
}

/*!\test
 * One "hot" path from some object to the root object may be pinned to prevent
 * it from being garbage collected.
 */
void test_pinned_path_is_protected_from_gc()
{
    std::shared_ptr<LRU::Entry> root = add_object(nullptr, 123);
    std::shared_ptr<LRU::Entry> child = root;

    child = add_object(child, 456);
    child = add_object(child, 789);
    child = add_object(child, 246);

    const ID::List pinned_child_id = child->get_cache_id();
    cache->pin(pinned_child_id);

    child = add_object(child, 579);
    const ID::List upmost_killed_child_id = child->get_cache_id();
    child = add_object(child, 951);
    const ID::List leaf_killed_child_id = child->get_cache_id();

    cppcut_assert_equal(size_t(6), cache->count());
    cppcut_assert_not_null(cache->lookup(pinned_child_id).get());
    cppcut_assert_not_null(cache->lookup(upmost_killed_child_id).get());
    cppcut_assert_not_null(cache->lookup(leaf_killed_child_id).get());

    /* let all objects expire */
    mock_timebase.step(std::chrono::milliseconds(maximum_object_age_minutes).count());

    obliviate_expectations->expect_obliviate_child(ID::List(5), ID::List(6));
    obliviate_expectations->expect_obliviate_child(ID::List(4), ID::List(5));

    cache->gc();

    cppcut_assert_equal(size_t(4), cache->count());
    cppcut_assert_not_null(cache->lookup(pinned_child_id).get());
    cppcut_assert_null(cache->lookup(upmost_killed_child_id).get());
    cppcut_assert_null(cache->lookup(leaf_killed_child_id).get());
}

/*
 * This function constructs the hierarchy shown below.
 *
 *         9
 *         |
 *         9
 *       _/|\_
 *      /  |  \
 *     7   4   9
 *     |   |   |
 *     1  *2   5
 */
static ID::List construct_list_hierarchy_for_pin_tests(ID::List *leftmost_leaf_id = nullptr)
{
    /* t = 1 second */
    mock_timebase.step(1000);
    std::shared_ptr<LRU::Entry> root       = add_object(nullptr, 123);
    std::shared_ptr<LRU::Entry> fork       = add_object(root, 156);
    std::shared_ptr<LRU::Entry> below_fork = add_object(fork, 389);
    std::shared_ptr<LRU::Entry> leaf       = add_object(below_fork, 346);

    std::shared_ptr<LRU::Entry> left_below_fork = below_fork;

    if(leftmost_leaf_id != nullptr)
        *leftmost_leaf_id = leaf->get_cache_id();

    /* t = 2 seconds */
    mock_timebase.step(1000);
    below_fork = add_object(fork, 579);
    leaf       = add_object(below_fork, 551);

    const ID::List retval = leaf->get_cache_id();

    cache->pin(retval);

    /* t = 4 seconds */
    mock_timebase.step(2000);
    cache->use(below_fork);

    /* t = 5 seconds */
    mock_timebase.step(1000);
    below_fork = add_object(fork, 731);
    leaf       = add_object(below_fork, 765);

    /* t = 7 seconds */
    mock_timebase.step(2000);
    cache->use(left_below_fork);

    /* t = 9 seconds */
    mock_timebase.step(2000);
    cache->use(below_fork);

    cppcut_assert_equal(size_t(8), cache->count());

    check_aging_list(std::array<unsigned int, 8>({346, 551, 579, 765, 389, 731, 156, 123}));

    return retval;
}

/*!\test
 * The pinned path may refer to very old objects is still not garbage
 * collected.
 */
void test_pinned_path_may_be_older_than_more_recently_used_paths()
{
    ID::List pinned_id = construct_list_hierarchy_for_pin_tests();

    /* let all objects expire */
    mock_timebase.step(std::chrono::milliseconds(maximum_object_age_minutes).count());

    obliviate_expectations->expect_obliviate_child(ID::List(3), ID::List(4));
    obliviate_expectations->expect_obliviate_child(ID::List(7), ID::List(8));
    obliviate_expectations->expect_obliviate_child(ID::List(2), ID::List(3));
    obliviate_expectations->expect_obliviate_child(ID::List(2), ID::List(7));

    cache->gc();

    cppcut_assert_equal(size_t(4), cache->count());
    cppcut_assert_not_null(cache->lookup(pinned_id).get());
}

/*!\test
 * Same test as #test_pinned_path_may_be_older_than_more_recently_used_paths(),
 * but with garbage collection driven by LRU::Cache::gc() return values.
 */
void test_pinned_path_may_be_older_than_more_recently_used_paths_one_by_one()
{
    ID::List pinned_id = construct_list_hierarchy_for_pin_tests();

    auto next_timeout = cache->gc();
    cppcut_assert_not_equal(std::chrono::seconds::max().count(), next_timeout.count());

    mock_timebase.step(std::chrono::milliseconds(next_timeout).count());
    obliviate_expectations->expect_obliviate_child(ID::List(3), ID::List(4));
    next_timeout = cache->gc();
    cppcut_assert_not_equal(std::chrono::seconds::max().count(), next_timeout.count());
    cppcut_assert_equal(size_t(7), cache->count());

    mock_timebase.step(std::chrono::milliseconds(next_timeout).count());
    obliviate_expectations->expect_obliviate_child(ID::List(7), ID::List(8));
    next_timeout = cache->gc();
    cppcut_assert_not_equal(std::chrono::seconds::max().count(), next_timeout.count());
    cppcut_assert_equal(size_t(6), cache->count());

    mock_timebase.step(std::chrono::milliseconds(next_timeout).count());
    obliviate_expectations->expect_obliviate_child(ID::List(2), ID::List(3));
    next_timeout = cache->gc();
    cppcut_assert_not_equal(std::chrono::seconds::max().count(), next_timeout.count());
    cppcut_assert_equal(size_t(5), cache->count());

    mock_timebase.step(std::chrono::milliseconds(next_timeout).count());
    obliviate_expectations->expect_obliviate_child(ID::List(2), ID::List(7));
    next_timeout = cache->gc();
    cppcut_assert_equal(std::chrono::seconds::max().count(), next_timeout.count());
    cppcut_assert_equal(size_t(4), cache->count());

    cppcut_assert_not_null(cache->lookup(pinned_id).get());
}

/*!\test
 * When unpinning a pinned path, the garbage collection kicks in if the path
 * contains expired objects.
 */
void test_unpin_old_path_triggers_garbage_collection()
{
    std::shared_ptr<LRU::Entry> root = add_object(nullptr, 123);
    std::shared_ptr<LRU::Entry> child = root;

    child = add_object(child, 456);
    cache->pin(child->get_cache_id());

    child = add_object(child, 789);
    child = add_object(child, 579);

    cppcut_assert_equal(size_t(4), cache->count());

    /* let all objects expire */
    mock_timebase.step(std::chrono::milliseconds(maximum_object_age_minutes).count());

    obliviate_expectations->expect_obliviate_child(ID::List(3), ID::List(4));
    obliviate_expectations->expect_obliviate_child(ID::List(2), ID::List(3));

    cache->gc();

    cppcut_assert_equal(size_t(2), cache->count());

    obliviate_expectations->expect_obliviate_child(ID::List(1), ID::List(2));

    cache->pin(ID::List());

    cppcut_assert_equal(size_t(0), cache->count());
}

/*!\test
 * Changing the pinned path from one expired object to another garbage collects
 * the previously pinned path, but keeps the newly pinned path.
 *
 * This is an important property that avoids certain potential, very rare race
 * conditions.
 */
void test_change_pinned_path_is_atomic()
{
    ID::List leftmost_leaf_id;
    ID::List pinned_id = construct_list_hierarchy_for_pin_tests(&leftmost_leaf_id);

    cppcut_assert_not_null(cache->lookup(pinned_id).get());
    cppcut_assert_not_null(cache->lookup(leftmost_leaf_id).get());

    /* let all objects expire */
    mock_timebase.step(std::chrono::milliseconds(maximum_object_age_minutes).count());

    obliviate_expectations->expect_obliviate_child(ID::List(5), ID::List(6));
    obliviate_expectations->expect_obliviate_child(ID::List(2), ID::List(5));
    obliviate_expectations->expect_obliviate_child(ID::List(7), ID::List(8));
    obliviate_expectations->expect_obliviate_child(ID::List(2), ID::List(7));

    cache->pin(leftmost_leaf_id);

    cppcut_assert_equal(size_t(4), cache->count());
    cppcut_assert_null(cache->lookup(pinned_id).get());
    cppcut_assert_not_null(cache->lookup(leftmost_leaf_id).get());

    obliviate_expectations->expect_obliviate_child(ID::List(3), ID::List(4));
    obliviate_expectations->expect_obliviate_child(ID::List(2), ID::List(3));
    obliviate_expectations->expect_obliviate_child(ID::List(1), ID::List(2));

    cache->pin(ID::List());

    cppcut_assert_equal(size_t(0), cache->count());
    cppcut_assert_null(cache->lookup(pinned_id).get());
    cppcut_assert_null(cache->lookup(leftmost_leaf_id).get());
}

/*!\test
 * Reinsertion of leaf cache object does not break pinning.
 */
void test_reinsert_pinned_leaf_object_keeps_pinned_property()
{
    std::shared_ptr<LRU::Entry> root = add_object(nullptr, 123);
    std::shared_ptr<LRU::Entry> child = root;

    child = add_object(child, 456);
    child = add_object(child, 789);
    child = add_object(child, 579);

    const ID::List previous_id = child->get_cache_id();
    cut_assert_true(previous_id.is_valid());

    cache->pin(previous_id);

    cppcut_assert_equal(previous_id.get_raw_id(),
                        cache->get_pinned_object().get_raw_id());
    cppcut_assert_equal(size_t(4), cache->count());

    const ID::List new_id = cache->insert_again(std::move(child));

    cppcut_assert_not_null(cache->lookup(new_id).get());
    cppcut_assert_null(cache->lookup(previous_id).get());

    /* let all objects expire */
    mock_timebase.step(std::chrono::milliseconds(maximum_object_age_minutes).count());

    /* nothing happens */
    auto next_timeout = cache->gc();

    cppcut_assert_equal(std::chrono::seconds::max().count(), next_timeout.count());
    cppcut_assert_equal(size_t(4), cache->count());

    /* unpin should work */
    obliviate_expectations->expect_obliviate_child(ID::List(3), new_id);
    obliviate_expectations->expect_obliviate_child(ID::List(2), ID::List(3));
    obliviate_expectations->expect_obliviate_child(ID::List(1), ID::List(2));

    cache->pin(ID::List());

    cut_assert_false(cache->get_pinned_object().is_valid());
    cppcut_assert_equal(size_t(0), cache->count());
}

/*!\test
 * Reinsertion of inner cache object does not break pinning.
 */
void test_reinsert_pinned_internal_node_object_keeps_pinned_property()
{
    std::shared_ptr<LRU::Entry> root = add_object(nullptr, 123);
    std::shared_ptr<LRU::Entry> child = root;

    child = add_object(child, 456);

    std::shared_ptr<LRU::Entry> internal_list = child;

    child = add_object(child, 789);
    child = add_object(child, 579);

    cache->pin(child->get_cache_id());

    cppcut_assert_equal(size_t(4), cache->count());

    const ID::List previous_id = internal_list->get_cache_id();
    cut_assert_true(previous_id.is_valid());

    const ID::List new_id = cache->insert_again(std::move(internal_list));

    cppcut_assert_not_null(cache->lookup(new_id).get());
    cppcut_assert_null(cache->lookup(previous_id).get());

    /* let all objects expire */
    mock_timebase.step(std::chrono::milliseconds(maximum_object_age_minutes).count());

    /* nothing happens */
    auto next_timeout = cache->gc();

    cppcut_assert_equal(std::chrono::seconds::max().count(), next_timeout.count());
    cppcut_assert_equal(size_t(4), cache->count());

    /* unpin should work */
    obliviate_expectations->expect_obliviate_child(ID::List(3), ID::List(4));
    obliviate_expectations->expect_obliviate_child(new_id,      ID::List(3));
    obliviate_expectations->expect_obliviate_child(ID::List(1), new_id);

    cache->pin(ID::List());

    cppcut_assert_equal(size_t(0), cache->count());
}

/*!\test
 * Purge single entry from cache.
 */
void test_purge_single_entry()
{
    std::shared_ptr<LRU::Entry> root = add_object(nullptr, 123);

    cppcut_assert_equal(size_t(1), cache->count());

    std::vector<ID::List> kill_list;
    kill_list.push_back(root->get_cache_id());

    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_IMPORTANT, "Purge entry 1");

    cut_assert_true(cache->toposort_for_purge(kill_list.begin(), kill_list.end()));
    cache->purge_entries(kill_list.begin(), kill_list.end());

    cppcut_assert_equal(size_t(0), cache->count());
    cut_assert_false(cache->begin() != cache->end());
}

/*!\test
 * Purge multiple entries from cache.
 */
void test_purge_multiple_entries()
{
    std::shared_ptr<LRU::Entry> root   = add_object(nullptr, 123);
    std::shared_ptr<LRU::Entry> leaf_a = add_object(root,    321);
    std::shared_ptr<LRU::Entry> inner  = add_object(root,    456);
    std::shared_ptr<LRU::Entry> leaf_b = add_object(inner,   654);

    cppcut_assert_equal(size_t(4), cache->count());

    std::vector<ID::List> kill_list;
    kill_list.push_back(leaf_a->get_cache_id());
    kill_list.push_back(inner->get_cache_id());
    kill_list.push_back(root->get_cache_id());
    kill_list.push_back(leaf_b->get_cache_id());

    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_IMPORTANT, "Purge entry 2");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_IMPORTANT, "Purge entry 4");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_IMPORTANT, "Purge entry 3");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_IMPORTANT, "Purge entry 1");

    /* there is no obliviation notification for the root node */
    obliviate_expectations->expect_obliviate_child(ID::List(1), ID::List(2));
    obliviate_expectations->expect_obliviate_child(ID::List(3), ID::List(4));
    obliviate_expectations->expect_obliviate_child(ID::List(1), ID::List(3));

    cut_assert_true(cache->toposort_for_purge(kill_list.begin(), kill_list.end()));
    cache->purge_entries(kill_list.begin(), kill_list.end());

    cppcut_assert_equal(size_t(0), cache->count());
    cut_assert_false(cache->begin() != cache->end());
}

/*!\test
 * Purge single subtree from cache.
 */
void test_purge_single_subtree()
{
    mock_timebase.step();
    std::shared_ptr<LRU::Entry> root   = add_object(nullptr, 123);
    mock_timebase.step();
    std::shared_ptr<LRU::Entry> leaf_a = add_object(root,    321);
    mock_timebase.step();
    std::shared_ptr<LRU::Entry> inner  = add_object(root,    456);
    mock_timebase.step();
    std::shared_ptr<LRU::Entry> leaf_d = add_object(root,    987);
    mock_timebase.step();
    std::shared_ptr<LRU::Entry> leaf_b = add_object(inner,   654);
    mock_timebase.step();
    std::shared_ptr<LRU::Entry> leaf_c = add_object(inner,   789);
    mock_timebase.step();

    cppcut_assert_equal(size_t(6), cache->count());
    check_aging_list(std::array<unsigned int, 6>({321, 987, 654, 789, 456, 123}));

    std::vector<ID::List> kill_list;
    kill_list.push_back(inner->get_cache_id());
    kill_list.push_back(leaf_c->get_cache_id());
    kill_list.push_back(leaf_b->get_cache_id());

    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_IMPORTANT, "Purge entry 5");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_IMPORTANT, "Purge entry 6");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_IMPORTANT, "Purge entry 3");

    obliviate_expectations->expect_obliviate_child(ID::List(3), ID::List(5));
    obliviate_expectations->expect_obliviate_child(ID::List(3), ID::List(6));
    obliviate_expectations->expect_obliviate_child(ID::List(1), ID::List(3));

    cut_assert_true(cache->toposort_for_purge(kill_list.begin(), kill_list.end()));
    cache->purge_entries(kill_list.begin(), kill_list.end());

    cppcut_assert_equal(size_t(3), cache->count());
    check_aging_list(std::array<unsigned int, 3>({321, 987, 123}));
}

/*!\test
 * Purge multiple subtrees from cache.
 */
void test_purge_multiple_subtrees()
{
    mock_timebase.step();
    std::shared_ptr<LRU::Entry> root     = add_object(nullptr,  1);
    mock_timebase.step();
    std::shared_ptr<LRU::Entry> leaf_a   = add_object(root,     2);
    mock_timebase.step();
    std::shared_ptr<LRU::Entry> inner1   = add_object(root,     3);
    mock_timebase.step();
    std::shared_ptr<LRU::Entry> leaf_b   = add_object(root,     4);
    mock_timebase.step();
    std::shared_ptr<LRU::Entry> inner2   = add_object(root,     5);
    mock_timebase.step();
    std::shared_ptr<LRU::Entry> leaf_c   = add_object(root,     6);
    mock_timebase.step();
    std::shared_ptr<LRU::Entry> inner3   = add_object(root,     7);
    mock_timebase.step();
    std::shared_ptr<LRU::Entry> leaf_1a  = add_object(inner1,  10);
    mock_timebase.step();
    std::shared_ptr<LRU::Entry> leaf_1b  = add_object(inner1,  11);
    mock_timebase.step();
    std::shared_ptr<LRU::Entry> leaf_1c  = add_object(inner1,  12);
    mock_timebase.step();
    std::shared_ptr<LRU::Entry> leaf_2a  = add_object(inner2,  20);
    mock_timebase.step();
    std::shared_ptr<LRU::Entry> leaf_3a  = add_object(inner3,  30);
    mock_timebase.step();
    std::shared_ptr<LRU::Entry> inner3a  = add_object(inner3,  31);
    mock_timebase.step();
    std::shared_ptr<LRU::Entry> leaf_3aa = add_object(inner3a, 32);
    mock_timebase.step();
    std::shared_ptr<LRU::Entry> leaf_3ab = add_object(inner3a, 33);
    mock_timebase.step();
    std::shared_ptr<LRU::Entry> leaf_d   = add_object(root,     8);
    mock_timebase.step();

    cppcut_assert_equal(size_t(16), cache->count());
    check_aging_list(std::array<unsigned int, 16>({2, 4, 6, 10, 11, 12, 3, 20, 5, 30, 32, 33, 31, 7, 8, 1}));

    std::vector<ID::List> kill_list;
    kill_list.push_back(inner1->get_cache_id());
    kill_list.push_back(leaf_1a->get_cache_id());
    kill_list.push_back(leaf_1b->get_cache_id());
    kill_list.push_back(leaf_1c->get_cache_id());
    kill_list.push_back(leaf_c->get_cache_id());
    kill_list.push_back(inner2->get_cache_id());
    kill_list.push_back(leaf_2a->get_cache_id());

    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_IMPORTANT, "Purge entry 11");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_IMPORTANT, "Purge entry 8");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_IMPORTANT, "Purge entry 9");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_IMPORTANT, "Purge entry 10");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_IMPORTANT, "Purge entry 6");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_IMPORTANT, "Purge entry 3");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_IMPORTANT, "Purge entry 5");

    obliviate_expectations->expect_obliviate_child(ID::List(5), ID::List(11));
    obliviate_expectations->expect_obliviate_child(ID::List(3), ID::List(8));
    obliviate_expectations->expect_obliviate_child(ID::List(3), ID::List(9));
    obliviate_expectations->expect_obliviate_child(ID::List(3), ID::List(10));
    obliviate_expectations->expect_obliviate_child(ID::List(1), ID::List(6));
    obliviate_expectations->expect_obliviate_child(ID::List(1), ID::List(3));
    obliviate_expectations->expect_obliviate_child(ID::List(1), ID::List(5));

    cut_assert_true(cache->toposort_for_purge(kill_list.begin(), kill_list.end()));
    cache->purge_entries(kill_list.begin(), kill_list.end());

    cppcut_assert_equal(size_t(9), cache->count());
    check_aging_list(std::array<unsigned int, 9>({2, 4, 30, 32, 33, 31, 7, 8, 1}));
}

/*!\test
 * Purge pinned leaf from cache.
 */
void test_purge_pinned_entry()
{
    mock_timebase.step();
    std::shared_ptr<LRU::Entry> root   = add_object(nullptr,  1);
    mock_timebase.step();
    std::shared_ptr<LRU::Entry> inner  = add_object(root,     2);
    mock_timebase.step();
    std::shared_ptr<LRU::Entry> leaf_a = add_object(inner,    3);
    mock_timebase.step();
    std::shared_ptr<LRU::Entry> leaf_b = add_object(root,     4);
    mock_timebase.step();

    cache->pin(leaf_a->get_cache_id());
    cut_assert_true(cache->get_pinned_object().is_valid());

    check_aging_list(std::array<unsigned int, 4>({3, 2, 4, 1}));

    std::vector<ID::List> kill_list;
    kill_list.push_back(inner->get_cache_id());
    kill_list.push_back(leaf_a->get_cache_id());

    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_IMPORTANT, "Purge entry 3");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_IMPORTANT, "Purge entry 2");

    obliviate_expectations->expect_obliviate_child(ID::List(2), ID::List(3));
    obliviate_expectations->expect_obliviate_child(ID::List(1), ID::List(2));

    cut_assert_true(cache->toposort_for_purge(kill_list.begin(), kill_list.end()));
    cache->purge_entries(kill_list.begin(), kill_list.end());

    check_aging_list(std::array<unsigned int, 2>({4, 1}));
    cut_assert_false(cache->get_pinned_object().is_valid());
}

/*!\test
 * Purge pinned inner node from cache.
 */
void test_purge_pinned_inner_node()
{
    mock_timebase.step();
    std::shared_ptr<LRU::Entry> root   = add_object(nullptr,  1);
    mock_timebase.step();
    std::shared_ptr<LRU::Entry> inner  = add_object(root,     2);
    mock_timebase.step();
    std::shared_ptr<LRU::Entry> leaf_a = add_object(inner,    3);
    mock_timebase.step();
    std::shared_ptr<LRU::Entry> leaf_b = add_object(root,     4);
    mock_timebase.step();

    cache->pin(inner->get_cache_id());
    cut_assert_true(cache->get_pinned_object().is_valid());

    check_aging_list(std::array<unsigned int, 4>({3, 2, 4, 1}));

    std::vector<ID::List> kill_list;
    kill_list.push_back(inner->get_cache_id());
    kill_list.push_back(leaf_a->get_cache_id());

    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_IMPORTANT, "Purge entry 3");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_IMPORTANT, "Purge entry 2");

    obliviate_expectations->expect_obliviate_child(ID::List(2), ID::List(3));
    obliviate_expectations->expect_obliviate_child(ID::List(1), ID::List(2));

    cut_assert_true(cache->toposort_for_purge(kill_list.begin(), kill_list.end()));
    cache->purge_entries(kill_list.begin(), kill_list.end());

    check_aging_list(std::array<unsigned int, 2>({4, 1}));
    cut_assert_false(cache->get_pinned_object().is_valid());
}

/*!\test
 * Purge subtree with kill list given in most inconvenient order.
 */
void test_purge_subtree_with_unsorted_kill_list()
{
    std::shared_ptr<LRU::Entry> root     = add_object(nullptr,  1);
    std::shared_ptr<LRU::Entry> inner_a  = add_object(root,    10);
    std::shared_ptr<LRU::Entry> inner_b  = add_object(root,    20);
    mock_timebase.step();
    std::shared_ptr<LRU::Entry> leaf_a_a = add_object(inner_a, 11);
    mock_timebase.step();
    std::shared_ptr<LRU::Entry> leaf_a_b = add_object(inner_a, 12);
    mock_timebase.step();
    std::shared_ptr<LRU::Entry> leaf_a_c = add_object(inner_a, 13);
    mock_timebase.step();
    std::shared_ptr<LRU::Entry> leaf_b_a = add_object(inner_b, 21);
    mock_timebase.step();
    std::shared_ptr<LRU::Entry> leaf_b_b = add_object(inner_b, 22);
    mock_timebase.step();
    std::shared_ptr<LRU::Entry> leaf_b_c = add_object(inner_b, 23);
    mock_timebase.step();

    check_aging_list(std::array<unsigned int, 9>({11, 12, 13, 10, 21, 22, 23, 20, 1}));

    std::vector<ID::List> kill_list;
    kill_list.push_back(inner_b->get_cache_id());
    kill_list.push_back(leaf_b_a->get_cache_id());
    kill_list.push_back(leaf_b_c->get_cache_id());
    kill_list.push_back(leaf_b_b->get_cache_id());
    kill_list.push_back(leaf_a_b->get_cache_id());

    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_IMPORTANT, "Purge entry 5");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_IMPORTANT, "Purge entry 7");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_IMPORTANT, "Purge entry 9");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_IMPORTANT, "Purge entry 8");
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_IMPORTANT, "Purge entry 3");

    obliviate_expectations->expect_obliviate_child(ID::List(2), ID::List(5));
    obliviate_expectations->expect_obliviate_child(ID::List(3), ID::List(7));
    obliviate_expectations->expect_obliviate_child(ID::List(3), ID::List(9));
    obliviate_expectations->expect_obliviate_child(ID::List(3), ID::List(8));
    obliviate_expectations->expect_obliviate_child(ID::List(1), ID::List(3));

    cut_assert_true(cache->toposort_for_purge(kill_list.begin(), kill_list.end()));
    cache->purge_entries(kill_list.begin(), kill_list.end());

    check_aging_list(std::array<unsigned int, 4>({11, 13, 10, 1}));
}

/*!\test
 * The depth of objects are correctly computed for bigger structures.
 */
void test_depth_of_leaf_objects()
{
    ID::List leftmost_leaf_id;
    ID::List pinned_id = construct_list_hierarchy_for_pin_tests(&leftmost_leaf_id);

    cppcut_assert_not_equal(leftmost_leaf_id.get_raw_id(), pinned_id.get_raw_id());
    cppcut_assert_equal(size_t(4), LRU::Entry::depth(*cache->lookup(leftmost_leaf_id)));
    cppcut_assert_equal(size_t(4), LRU::Entry::depth(*cache->lookup(pinned_id)));
    cppcut_assert_equal(size_t(3), LRU::Entry::depth(*cache->lookup(pinned_id)->get_parent()));
}

/*
 * This function constructs the hierarchy shown below.
 *
 *         3
 *         |
 *         3
 *       _/|\_
 *      /  |  \
 *     1   2   3
 *
 * A pointer to the inner node with three children is returned in the \p fork
 * parameter, the center child leaf is returned in the \p leaf_b parameter.
 */
static void construct_hierarchy_for_uncached_tests(std::shared_ptr<LRU::Entry> &fork,
                                                   std::shared_ptr<LRU::Entry> &leaf_b)
{
    /* t = 1 second */
    mock_timebase.step(1000);
    std::shared_ptr<LRU::Entry> root   = add_object(nullptr, 12);
    fork                               = add_object(root, 34);
    std::shared_ptr<LRU::Entry> leaf_a = add_object(fork, 56);

    /* t = 2 seconds */
    mock_timebase.step(1000);
    leaf_b                             = add_object(fork, 78);

    /* t = 3 seconds */
    mock_timebase.step(1000);
    std::shared_ptr<LRU::Entry> leaf_c = add_object(fork, 90);

    cppcut_assert_equal(size_t(5), cache->count());
    check_aging_list(std::array<unsigned int, 5>({56, 78, 90, 34, 12}));
}

/*!\test
 * Uncached objects are just like regular ones, but they can be identified.
 */
void test_uncached_objects_in_hierarchy()
{
    std::shared_ptr<LRU::Entry> fork;
    std::shared_ptr<LRU::Entry> leaf_b;
    construct_hierarchy_for_uncached_tests(fork, leaf_b);

    /* t = 5 seconds */
    mock_timebase.step(2000);
    std::shared_ptr<LRU::Entry> uncached_a      = add_uncached_object(fork, 200);
    std::shared_ptr<LRU::Entry> uncached_leaf_a = add_uncached_object(uncached_a, 201);

    /*
     * We have this hierarchy now ('#' means "not cached"):
     *
     *         5
     *         |
     *         5______
     *       _/|\_    \
     *      /  |  \    |
     *     1   2   3   5#
     *                 |
     *                 5#
     */
    cppcut_assert_equal(size_t(7), cache->count());
    check_aging_list(std::array<unsigned int, 7>({56, 78, 90, 201, 200, 34, 12}));

    /* t = 6 seconds */
    mock_timebase.step(1000);
    cache->use(leaf_b);

    /*
     * We have this hierarchy now ('#' means "not cached"):
     *
     *         6
     *         |
     *         6______
     *       _/|\_    \
     *      /  |  \    |
     *     1   6   3   5#
     *                 |
     *                 5#
     */
    check_aging_list(std::array<unsigned int, 7>({56, 90, 201, 200, 78, 34, 12}));

    /* t = 7 seconds */
    std::shared_ptr<LRU::Entry> uncached_b      = add_uncached_object(fork, 300);
    std::shared_ptr<LRU::Entry> uncached_leaf_b = add_uncached_object(uncached_b, 301);

    /*
     * We have this hierarchy now ('#' means "not cached"):
     *
     *         7
     *         |
     *         7======--__
     *       _/|\_    \   \
     *      /  |  \    |   |
     *     1   6   3   5#  7#
     *                 |   |
     *                 5#  7#
     */
    cppcut_assert_equal(size_t(9), cache->count());
    check_aging_list(std::array<unsigned int, 9>({56, 90, 201, 200, 78, 301, 300, 34, 12}));

    std::vector<ID::List> kill_list;

    for(const auto &it : *cache)
    {
        if(it.get_cache_id().get_nocache_bit())
            kill_list.push_back(it.get_cache_id());
    }

    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_IMPORTANT, "Purge entry 134217735");  // 7
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_IMPORTANT, "Purge entry 134217737");  // 9
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_IMPORTANT, "Purge entry 134217734");  // 6
    mock_messages->expect_msg_vinfo_formatted(MESSAGE_LEVEL_IMPORTANT, "Purge entry 134217736");  // 8

    obliviate_expectations->expect_obliviate_child(ID::List(0x08000006), ID::List(0x08000007));
    obliviate_expectations->expect_obliviate_child(ID::List(0x08000008), ID::List(0x08000009));
    obliviate_expectations->expect_obliviate_child(ID::List(0x00000002), ID::List(0x08000006));
    obliviate_expectations->expect_obliviate_child(ID::List(0x00000002), ID::List(0x08000008));

    cut_assert_true(cache->toposort_for_purge(kill_list.begin(), kill_list.end()));
    cache->purge_entries(kill_list.begin(), kill_list.end());

    /*
     * We have this hierarchy now:
     *
     *         7
     *         |
     *         7
     *       _/|\_
     *      /  |  \
     *     1   6   3
     */
    cppcut_assert_equal(size_t(5), cache->count());
    check_aging_list(std::array<unsigned int, 5>({56, 90, 78, 34, 12}));

    auto duration = cache->gc();
    assert_equal_times(std::chrono::seconds(maximum_object_age_minutes) - std::chrono::seconds(5),
                       duration);
}

/*!\test
 * An arbitrary mix of cached and uncached objects is allowed.
 */
void test_cached_object_can_be_child_of_uncached_object()
{
    std::shared_ptr<LRU::Entry> root     = add_object(nullptr, 300);
    std::shared_ptr<LRU::Entry> uncached = add_uncached_object(root, 400);
    std::shared_ptr<LRU::Entry> cached   = std::make_shared<Object>(uncached, 500);
    cppcut_assert_not_null(cached.get());

    const size_t previous_count = cache->count();
    const ID::List id = cache->insert(std::move(cached), LRU::CacheMode::CACHED, 0, 15);
    cut_assert_true(id.is_valid());
    cppcut_assert_equal(previous_count + 1U, cache->count());
}

};


namespace lru_cache_id_generator_tests
{

static LRU::CacheIdGenerator *gen;

void cut_setup(void)
{
    obliviate_expectations = new ObliviateExpectations;
    cppcut_assert_not_null(obliviate_expectations);
    obliviate_expectations->init();

    gen = nullptr;
}

void cut_teardown(void)
{
    delete gen;
    gen = nullptr;

    obliviate_expectations->check();
    delete obliviate_expectations;
    obliviate_expectations = nullptr;
}

static bool return_always_ok(ID::List id)
{
    return true;
}

static bool return_never_ok(ID::List id)
{
    return false;
}

static bool reject_all_smaller_than_2000(ID::List id)
{
    return id.get_cooked_id() >= 2000;
}

/*!\test
 * Cached IDs are assigned in ascending order.
 */
void test_counts_up_cached()
{
    gen = new LRU::CacheIdGenerator(1, LRU::CacheIdGenerator::ID_MAX, return_always_ok);
    cppcut_assert_not_null(gen);

    cppcut_assert_equal(uint32_t(0xe0000001), gen->next(LRU::CacheMode::CACHED, 14).get_raw_id());
    cppcut_assert_equal(uint32_t(0xe0000002), gen->next(LRU::CacheMode::CACHED, 14).get_raw_id());
    cppcut_assert_equal(uint32_t(0xe0000003), gen->next(LRU::CacheMode::CACHED, 14).get_raw_id());
    cppcut_assert_equal(uint32_t(0xe0000004), gen->next(LRU::CacheMode::CACHED, 14).get_raw_id());
}

/*!\test
 * Uncached IDs are assigned in ascending order.
 */
void test_counts_up_uncached()
{
    gen = new LRU::CacheIdGenerator(1, LRU::CacheIdGenerator::ID_MAX, return_always_ok);
    cppcut_assert_not_null(gen);

    cppcut_assert_equal(uint32_t(0x58000001), gen->next(LRU::CacheMode::UNCACHED, 5).get_raw_id());
    cppcut_assert_equal(uint32_t(0x58000002), gen->next(LRU::CacheMode::UNCACHED, 5).get_raw_id());
    cppcut_assert_equal(uint32_t(0x58000003), gen->next(LRU::CacheMode::UNCACHED, 5).get_raw_id());
    cppcut_assert_equal(uint32_t(0x58000004), gen->next(LRU::CacheMode::UNCACHED, 5).get_raw_id());
}

/*!\test
 * IDs are assigned in ascending order, based on a single counter for cached
 * and uncached IDs.
 */
void test_counts_up_with_single_counter_for_cached_and_uncached()
{
    gen = new LRU::CacheIdGenerator(1, LRU::CacheIdGenerator::ID_MAX, return_always_ok);
    cppcut_assert_not_null(gen);

    cppcut_assert_equal(uint32_t(0xf0000001), gen->next(LRU::CacheMode::CACHED,   15).get_raw_id());
    cppcut_assert_equal(uint32_t(0xf0000002), gen->next(LRU::CacheMode::CACHED,   15).get_raw_id());
    cppcut_assert_equal(uint32_t(0xf8000003), gen->next(LRU::CacheMode::UNCACHED, 15).get_raw_id());
    cppcut_assert_equal(uint32_t(0xf8000004), gen->next(LRU::CacheMode::UNCACHED, 15).get_raw_id());
    cppcut_assert_equal(uint32_t(0xf0000005), gen->next(LRU::CacheMode::CACHED,   15).get_raw_id());
    cppcut_assert_equal(uint32_t(0xf8000006), gen->next(LRU::CacheMode::UNCACHED, 15).get_raw_id());
}

/*!\test
 * IDs are assigned in ascending order with per-context counters.
 */
void test_counts_up_with_context()
{
    cppcut_assert_equal(15U, DBUS_LISTS_CONTEXT_ID_MAX);

    gen = new LRU::CacheIdGenerator(1, LRU::CacheIdGenerator::ID_MAX, return_always_ok);
    cppcut_assert_not_null(gen);

    cppcut_assert_equal(uint32_t(0x00000001), gen->next(LRU::CacheMode::CACHED, 0).get_raw_id());
    const ID::List ctx_0_id(gen->next(LRU::CacheMode::CACHED, 0).get_raw_id());
    cppcut_assert_equal(uint32_t(0x00000002), ctx_0_id.get_raw_id());
    cppcut_assert_equal(0U, static_cast<unsigned int>(ctx_0_id.get_context()));
    cppcut_assert_equal(2U, ctx_0_id.get_cooked_id());

    cppcut_assert_equal(uint32_t(0x10000001), gen->next(LRU::CacheMode::CACHED, 1).get_raw_id());
    cppcut_assert_equal(uint32_t(0x10000002), gen->next(LRU::CacheMode::CACHED, 1).get_raw_id());
    cppcut_assert_equal(uint32_t(0x10000003), gen->next(LRU::CacheMode::CACHED, 1).get_raw_id());
    const ID::List ctx_1_id(gen->next(LRU::CacheMode::CACHED, 1).get_raw_id());
    cppcut_assert_equal(uint32_t(0x10000004), ctx_1_id.get_raw_id());
    cppcut_assert_equal(1U, static_cast<unsigned int>(ctx_1_id.get_context()));
    cppcut_assert_equal(4U, ctx_1_id.get_cooked_id());

    cppcut_assert_equal(uint32_t(0xf0000001),
                        gen->next(LRU::CacheMode::CACHED, DBUS_LISTS_CONTEXT_ID_MAX).get_raw_id());
    cppcut_assert_equal(uint32_t(0xf0000002),
                        gen->next(LRU::CacheMode::CACHED, DBUS_LISTS_CONTEXT_ID_MAX).get_raw_id());
    const ID::List ctx_15_id(gen->next(LRU::CacheMode::CACHED, 15).get_raw_id());
    cppcut_assert_equal(uint32_t(0xf0000003), ctx_15_id.get_raw_id());
    cppcut_assert_equal(15U, static_cast<unsigned int>(ctx_15_id.get_context()));
    cppcut_assert_equal(3U, ctx_15_id.get_cooked_id());
}

/*!\test
 * Validity of IDs is determined without context bits.
 */
void test_invalid_id_with_context()
{
    static const std::array<ID::List, 16> valid_ids =
    {
        ID::List(0x00000001),
        ID::List(0x10000001),
        ID::List(0xe0000001),
        ID::List(0xf0000001),
        ID::List(0x04000000),
        ID::List(0x14000000),
        ID::List(0xe4000000),
        ID::List(0xf4000000),
        ID::List(0x07ffffff),
        ID::List(0x17ffffff),
        ID::List(0xe7ffffff),
        ID::List(0xf7ffffff),
        ID::List(0x0fffffff),
        ID::List(0x1fffffff),
        ID::List(0xefffffff),
        ID::List(0xffffffff),
    };

    static const std::array<ID::List, 8> invalid_ids =
    {
        ID::List(0x00000000),
        ID::List(0x10000000),
        ID::List(0xe0000000),
        ID::List(0xf0000000),
        ID::List(0x08000000),
        ID::List(0x18000000),
        ID::List(0xe8000000),
        ID::List(0xf8000000),
    };

    for(const auto &id : valid_ids)
        cut_assert_true(id.is_valid());

    for(const auto &id : invalid_ids)
        cut_assert_false(id.is_valid());
}

/*!\test
 * When reaching the maximum ID, is returned. Next ID starts at minimum.
 */
void test_wraps_around()
{
    gen = new LRU::CacheIdGenerator(10, 12, return_always_ok);
    cppcut_assert_not_null(gen);

    cppcut_assert_equal(uint32_t(0x3000000a), gen->next(LRU::CacheMode::CACHED, 3).get_raw_id());
    cppcut_assert_equal(uint32_t(0x3000000b), gen->next(LRU::CacheMode::CACHED, 3).get_raw_id());
    cppcut_assert_equal(uint32_t(0x3000000c), gen->next(LRU::CacheMode::CACHED, 3).get_raw_id());
    cppcut_assert_equal(uint32_t(0x3000000a), gen->next(LRU::CacheMode::CACHED, 3).get_raw_id());
    cppcut_assert_equal(uint32_t(0x3000000b), gen->next(LRU::CacheMode::CACHED, 3).get_raw_id());
    cppcut_assert_equal(uint32_t(0x3000000c), gen->next(LRU::CacheMode::CACHED, 3).get_raw_id());
    cppcut_assert_equal(uint32_t(0x3000000a), gen->next(LRU::CacheMode::CACHED, 3).get_raw_id());
}

/*!\test
 * Wrap-around also works if the maximum valid ID is configured to be equal to
 * the maximal value the underlying integer can represent.
 */
void test_wraps_around_at_integer_maximum()
{
    gen = new LRU::CacheIdGenerator(LRU::CacheIdGenerator::ID_MAX - 4,
                                    LRU::CacheIdGenerator::ID_MAX,
                                    return_always_ok);
    cppcut_assert_not_null(gen);

    cppcut_assert_equal(uint32_t(0x80000000 | (LRU::CacheIdGenerator::ID_MAX - 4U)),
                        gen->next(LRU::CacheMode::CACHED, 8).get_raw_id());
    cppcut_assert_equal(uint32_t(0x80000000 | (LRU::CacheIdGenerator::ID_MAX - 3U)),
                        gen->next(LRU::CacheMode::CACHED, 8).get_raw_id());
    cppcut_assert_equal(uint32_t(0x80000000 | (LRU::CacheIdGenerator::ID_MAX - 2U)),
                        gen->next(LRU::CacheMode::CACHED, 8).get_raw_id());
    cppcut_assert_equal(uint32_t(0x80000000 | (LRU::CacheIdGenerator::ID_MAX - 1U)),
                        gen->next(LRU::CacheMode::CACHED, 8).get_raw_id());
    cppcut_assert_equal(uint32_t(0x80000000 | (LRU::CacheIdGenerator::ID_MAX)),
                        gen->next(LRU::CacheMode::CACHED, 8).get_raw_id());
    cppcut_assert_equal(uint32_t(0x80000000 | (LRU::CacheIdGenerator::ID_MAX - 4U)),
                        gen->next(LRU::CacheMode::CACHED, 8).get_raw_id());
    cppcut_assert_equal(uint32_t(0x80000000 | (LRU::CacheIdGenerator::ID_MAX - 3U)),
                        gen->next(LRU::CacheMode::CACHED, 8).get_raw_id());
}

/*!\test
 * When searching for a free ID, the search terminates if there are no free IDs
 * left.
 */
void test_no_infinite_loop_if_no_free_ids()
{
    gen = new LRU::CacheIdGenerator(100, 110, return_never_ok);
    cppcut_assert_not_null(gen);

    auto id1 = gen->next(LRU::CacheMode::CACHED, 0);

    cut_assert_false(id1.is_valid());
    cppcut_assert_equal(0U, id1.get_raw_id());

    auto id2 = gen->next(LRU::CacheMode::CACHED, 2);

    cut_assert_false(id2.is_valid());
    cppcut_assert_equal(0U, id2.get_raw_id());

    auto id3 = gen->next(LRU::CacheMode::UNCACHED, 0);

    cut_assert_false(id3.is_valid());
    cppcut_assert_equal(0U, id3.get_raw_id());

    auto id4 = gen->next(LRU::CacheMode::UNCACHED, 2);

    cut_assert_false(id4.is_valid());
    cppcut_assert_equal(0U, id4.get_raw_id());
}

/*!\test
 * Allocated cached IDs are never returned.
 */
void test_skips_non_free_cached_ids()
{
    gen = new LRU::CacheIdGenerator(1000, 3000, reject_all_smaller_than_2000);
    cppcut_assert_not_null(gen);

    cppcut_assert_equal(uint32_t(0xb0000000 | 2000U), gen->next(LRU::CacheMode::CACHED, 11).get_raw_id());
}

/*!\test
 * Allocated uncached IDs are never returned.
 */
void test_skips_non_free_uncached_ids()
{
    gen = new LRU::CacheIdGenerator(1000, 3000, reject_all_smaller_than_2000);
    cppcut_assert_not_null(gen);

    cppcut_assert_equal(uint32_t(0x68000000 | 2000U), gen->next(LRU::CacheMode::UNCACHED, 6).get_raw_id());
}

};

/*!@}*/

/*
 * Copyright (C) 2017, 2019  T+A elektroakustik GmbH & Co. KG
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

#ifndef CACHEABLE_HH
#define CACHEABLE_HH

#include <map>
#include <chrono>
#include <functional>

#include "lru.hh"

struct _GMainLoop;

namespace Cacheable
{

class Override;

class GLibWrapperIface
{
  protected:
    explicit GLibWrapperIface() {}

  public:
    GLibWrapperIface(const GLibWrapperIface &) = delete;
    GLibWrapperIface(GLibWrapperIface &&) = default;
    GLibWrapperIface &operator=(const GLibWrapperIface &) = delete;

    virtual ~GLibWrapperIface() {}

    virtual void ref_main_loop(struct _GMainLoop *loop) const = 0;
    virtual void unref_main_loop(struct _GMainLoop *loop) const = 0;
    virtual void create_timeout(int64_t &start_time, uint32_t &active_timer_id,
                                int (*trampoline)(void *user_data),
                                Override *origin_object) const = 0;
    virtual void remove_timeout(uint32_t active_timer_id) const = 0;
    virtual bool has_t_exceeded_expiry_time(int64_t t) const = 0;
};

class CheckIface
{
  protected:
    explicit CheckIface() {}

  public:
    CheckIface(const CheckIface &) = delete;
    CheckIface &operator=(const CheckIface &) = delete;

    virtual ~CheckIface() {}

    virtual bool is_cacheable(ID::List list_id) const = 0;
    virtual std::chrono::seconds put_override(ID::List list_id) = 0;
    virtual bool remove_override(ID::List list_id) = 0;
    virtual bool has_overrides() const = 0;

    virtual void list_invalidate(ID::List list_id, ID::List replacement_id) = 0;
};

class CheckNoOverrides: public CheckIface
{
  public:
    CheckNoOverrides(const CheckNoOverrides &) = delete;
    CheckNoOverrides &operator=(const CheckNoOverrides &) = delete;

    explicit CheckNoOverrides() {}
    virtual ~CheckNoOverrides() {}

    bool is_cacheable(ID::List list_id) const final override
    {
        return list_id.is_valid() && !list_id.get_nocache_bit();
    }

    std::chrono::seconds put_override(ID::List list_id) final override
    {
        return std::chrono::seconds(-1);
    }

    bool remove_override(ID::List list_id) final override { return false; }

    bool has_overrides() const final override { return false; }

    void list_invalidate(ID::List list_id, ID::List replacement_id) final override {}
};

class Override
{
  public:
    static constexpr const std::chrono::seconds EXPIRY_TIME = std::chrono::minutes(3);

    using ExpiredFn = std::function<void()>;
    const ExpiredFn expired_fn_;

  private:
    const GLibWrapperIface &glib_wrapper_;

    std::map<const ID::List, const bool> nodes_on_overridden_path_to_root_;

    int64_t start_time_;
    uint32_t active_timer_id_;

  public:
    Override(const Override &) = delete;
    Override(Override &&) = default;
    Override &operator=(const Override &) = delete;

    explicit Override(const GLibWrapperIface &glib_wrapper,
                      std::map<const ID::List, const bool> &&overridden_nodes,
                      ExpiredFn &&expired_fn):
        expired_fn_(std::move(expired_fn)),
        glib_wrapper_(glib_wrapper),
        nodes_on_overridden_path_to_root_(std::move(overridden_nodes)),
        start_time_(INT64_MIN),
        active_timer_id_(0)
    {}

    ~Override()
    {
        start_time_ = INT64_MIN;
        active_timer_id_ = 0;
    }

    std::chrono::seconds keep_alive();

    bool is_on_path_to_override(ID::List list_id) const;

    void invalidate() { do_invalidate(true); }
    bool is_invalidated() const { return start_time_ == INT64_MIN; }

    bool is_timeout_exceeded() const;

    void list_invalidate(ID::List list_id, ID::List replacement_id);

  private:
    void do_invalidate(bool may_call_expiry_callback);
};

class CheckWithOverrides: public CheckIface
{
  private:
    const GLibWrapperIface &glib_wrapper_;

    LRU::Cache &cache_;
    struct _GMainLoop *const loop_;

    std::map<ID::List, Override> overrides_;

  public:
    CheckWithOverrides(const CheckWithOverrides &) = delete;
    CheckWithOverrides &operator=(const CheckWithOverrides &) = delete;

    explicit CheckWithOverrides(const GLibWrapperIface &glib_wrapper,
                                LRU::Cache &cache, struct _GMainLoop *loop):
        glib_wrapper_(glib_wrapper),
        cache_(cache),
        loop_(loop)
    {
        glib_wrapper.ref_main_loop(loop);
    }

    virtual ~CheckWithOverrides()
    {
        if(loop_ != nullptr)
            glib_wrapper_.unref_main_loop(loop_);
    }

    bool is_cacheable(ID::List list_id) const final override;

    std::chrono::seconds put_override(ID::List list_id) final override;
    bool remove_override(ID::List list_id) final override;
    bool has_overrides() const final override { return !overrides_.empty(); }

    void list_invalidate(ID::List list_id, ID::List replacement_id) final override;

  private:
    void expired(ID::List list_id);
    bool is_cacheable_by_override(ID::List list_id) const;
};

}

#endif /* !CACHEABLE_HH */

/*
 * Copyright (C) 2015, 2018, 2019  T+A elektroakustik GmbH & Co. KG
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

#ifndef FAKE_DBUS_HH
#define FAKE_DBUS_HH

#include <string>
#include <map>
#include <vector>
#include <memory>
#include <algorithm>

namespace FakeDBus
{
    static gpointer ref_proxy(gpointer object);
};

tdbusdleynaserverMediaDevice *
UPnP::create_media_device_proxy_for_object_path_end(const std::string &path,
                                                    GAsyncResult *res)
{
    cut_assert_false(path.empty());
    cppcut_assert_not_null(res);

    return reinterpret_cast<tdbusdleynaserverMediaDevice *>(res);
}

namespace FakeDBus
{

class ObjectProxy
{
  public:
    ObjectProxy(const ObjectProxy &) = delete;
    ObjectProxy &operator=(const ObjectProxy &) = delete;

    explicit ObjectProxy(const char *name):
        name_(name)
    {}

    explicit ObjectProxy(const std::string &name):
        name_(name)
    {}

    const std::string name_;
};

template <size_t S> struct uint_pointer_mapping;
template <> struct uint_pointer_mapping<4U> { typedef uint32_t uint_type; };
template <> struct uint_pointer_mapping<8U> { typedef uint64_t uint_type; };

typedef uint_pointer_mapping<sizeof(void *)>::uint_type ProxyID;

static ProxyID next_proxy_id;
static std::map<ProxyID, std::shared_ptr<ObjectProxy>> all_proxies;
static std::vector<std::shared_ptr<ObjectProxy>> proxy_extra_refs;

static bool create_proxy_begin(const std::string &path,
                               GAsyncReadyCallback ready_callback,
                               void *ready_callback_data)
{
    cut_assert_false(path.empty());
    cppcut_assert_not_null(reinterpret_cast<void *>(ready_callback));
    cppcut_assert_not_null(ready_callback_data);

    auto result = all_proxies.insert(decltype(all_proxies)::value_type(
        next_proxy_id,
        std::make_shared<FakeDBus::ObjectProxy>(path)));

    cut_assert_true(result.second);

    ready_callback(nullptr, reinterpret_cast<GAsyncResult *>(next_proxy_id++),
                   ready_callback_data);

    return true;
}

static std::string get_proxy_object_path(tdbusdleynaserverMediaDevice *proxy)
{
    ProxyID proxy_id = reinterpret_cast<ProxyID>(proxy);
    cppcut_assert_operator(ProxyID(100), <=, proxy_id);
    cppcut_assert_operator(next_proxy_id, >, proxy_id);

    const auto it = all_proxies.find(proxy_id);
    cut_assert_true(it != all_proxies.end());

    return it->second->name_;
}

static bool compare_proxy_object_path(tdbusdleynaserverMediaDevice *proxy,
                                      const std::string &path)
{
    return path == get_proxy_object_path(proxy);
}

static gpointer ref_proxy(gpointer object)
{
    ProxyID proxy_id = reinterpret_cast<ProxyID>(object);
    cppcut_assert_operator(ProxyID(100), <=, proxy_id);
    cppcut_assert_operator(next_proxy_id, >, proxy_id);

    const auto it = all_proxies.find(proxy_id);
    cut_assert_true(it != all_proxies.end());

    proxy_extra_refs.push_back(it->second);

    return object;
}

static void unref_proxy(gpointer object)
{
    ProxyID proxy_id = reinterpret_cast<ProxyID>(object);
    cppcut_assert_operator(ProxyID(100), <=, proxy_id);
    cppcut_assert_operator(next_proxy_id, >, proxy_id);

    const auto it = all_proxies.find(proxy_id);
    cut_assert_true(it != all_proxies.end());

    auto extra_it =
        std::find(proxy_extra_refs.begin(), proxy_extra_refs.end(),
                  it->second);

    if(extra_it != proxy_extra_refs.end())
        proxy_extra_refs.erase(extra_it);
    else
        all_proxies.erase(it);
}

};

#endif /* !FAKE_DBUS_HH */

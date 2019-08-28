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

#ifndef MOCK_DBUS_UPNP_HELPERS_HH
#define MOCK_DBUS_UPNP_HELPERS_HH

#include "dbus_upnp_helpers.hh"
#include "mock_expectation.hh"

class MockDBusUPnPHelpers
{
  public:
    MockDBusUPnPHelpers(const MockDBusUPnPHelpers &) = delete;
    MockDBusUPnPHelpers &operator=(const MockDBusUPnPHelpers &) = delete;

    class Expectation;
    typedef MockExpectationsTemplate<Expectation> MockExpectations;
    MockExpectations *expectations_;

    explicit MockDBusUPnPHelpers();
    ~MockDBusUPnPHelpers();

    void init();
    void check() const;

    typedef std::string (*get_path_callback_t)(tdbusdleynaserverMediaDevice *proxy);
    void expect_get_proxy_object_path(const char *retval,
                                      tdbusdleynaserverMediaDevice *proxy);
    void expect_get_proxy_object_path_callback(get_path_callback_t fn);

    typedef bool (*path_equals_callback_t)(tdbusdleynaserverMediaDevice *proxy, const std::string &path);
    void expect_proxy_object_path_equals(bool retval, tdbusdleynaserverMediaDevice *proxy,
                                         const std::string &path);
    void expect_proxy_object_path_equals_callback(path_equals_callback_t fn, size_t count = 1);

    typedef bool (*create_proxy_callback_t)(const std::string &path,
                                            GAsyncReadyCallback ready_callback,
                                            void *ready_callback_data);
    void expect_create_media_device_proxy_for_object_path_begin_callback(create_proxy_callback_t fn);

    void expect_is_media_device_usable(bool retval, tdbusdleynaserverMediaDevice *proxy);

    void expect_get_size_of_container(uint32_t retval, const std::string &path);
};

extern MockDBusUPnPHelpers *mock_dbus_upnp_helpers_singleton;

#endif /* !MOCK_DBUS_UPNP_HELPERS_HH */

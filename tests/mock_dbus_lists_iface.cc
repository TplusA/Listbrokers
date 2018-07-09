/*
 * Copyright (C) 2015  T+A elektroakustik GmbH & Co. KG
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

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <cppcutter.h>

#include "mock_dbus_lists_iface.hh"

enum class DBusListsFn
{
    setup,
    lists_get_navigation_iface,

    first_valid_dbus_lists_fn_id = setup,
    last_valid_dbus_lists_fn_id = lists_get_navigation_iface,
};

static std::ostream &operator<<(std::ostream &os, const DBusListsFn id)
{
    if(id < DBusListsFn::first_valid_dbus_lists_fn_id ||
       id > DBusListsFn::last_valid_dbus_lists_fn_id)
    {
        os << "INVALID";
        return os;
    }

    switch(id)
    {
      case DBusListsFn::setup:
        os << "setup";
        break;

      case DBusListsFn::lists_get_navigation_iface:
        os << "lists_get_navigation_iface";
        break;
    }

    os << "()";

    return os;
}

class MockDBusListsIface::Expectation
{
  public:
    const DBusListsFn function_id_;

    tdbuslistsNavigation *const ret_dbus_object_;
    const std::string arg_dbus_object_path_;
    const bool arg_connect_to_session_bus_;
    struct DBusNavlistsIfaceData *const arg_navlists_iface_data_;

    Expectation(const Expectation &) = delete;
    Expectation &operator=(const Expectation &) = delete;

    explicit Expectation(bool connect_to_session_bus,
                         const char *dbus_object_path,
                         struct DBusNavlistsIfaceData *iface_data):
        function_id_(DBusListsFn::setup),
        ret_dbus_object_(nullptr),
        arg_dbus_object_path_(dbus_object_path),
        arg_connect_to_session_bus_(connect_to_session_bus),
        arg_navlists_iface_data_(iface_data)
    {}

    explicit Expectation(tdbuslistsNavigation *ret_object):
        function_id_(DBusListsFn::lists_get_navigation_iface),
        ret_dbus_object_(ret_object),
        arg_connect_to_session_bus_(false),
        arg_navlists_iface_data_(nullptr)
    {}

    Expectation(Expectation &&) = default;
};

MockDBusListsIface::MockDBusListsIface():
    ignore_all_(false)
{
    expectations_ = new MockExpectations();
}

MockDBusListsIface::~MockDBusListsIface()
{
    delete expectations_;
}

void MockDBusListsIface::init()
{
    cppcut_assert_not_null(expectations_);
    expectations_->init();
}

void MockDBusListsIface::check() const
{
    cppcut_assert_not_null(expectations_);
    expectations_->check();
}


void MockDBusListsIface::expect_dbus_lists_setup(bool connect_to_session_bus,
                                                 const char *dbus_object_path,
                                                 struct DBusNavlistsIfaceData *iface_data)
{
    expectations_->add(Expectation(connect_to_session_bus, dbus_object_path, iface_data));
}

void MockDBusListsIface::expect_dbus_lists_get_navigation_iface(tdbuslistsNavigation *ret)
{
    expectations_->add(Expectation(ret));
}


MockDBusListsIface *mock_dbus_lists_iface_singleton = nullptr;

void dbus_lists_setup(bool connect_to_session_bus,
                      const char *dbus_object_path,
                      struct DBusNavlistsIfaceData *iface_data)
{
    const auto &expect(mock_dbus_lists_iface_singleton->expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.function_id_, DBusListsFn::setup);
    cppcut_assert_equal(expect.arg_connect_to_session_bus_, connect_to_session_bus);
    cppcut_assert_equal(expect.arg_dbus_object_path_.c_str(), dbus_object_path);
    cppcut_assert_equal(expect.arg_navlists_iface_data_, iface_data);
}

tdbuslistsNavigation *dbus_lists_get_navigation_iface(void)
{
    const auto &expect(mock_dbus_lists_iface_singleton->expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.function_id_, DBusListsFn::lists_get_navigation_iface);
    return expect.ret_dbus_object_;
}

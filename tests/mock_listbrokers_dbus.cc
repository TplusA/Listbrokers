/*
 * Copyright (C) 2015, 2019  T+A elektroakustik GmbH & Co. KG
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

#include "mock_listbrokers_dbus.hh"

enum class DBusFn
{
    lists_navigation_emit_list_invalidate,

    first_valid_dbus_fn_id = lists_navigation_emit_list_invalidate,
    last_valid_dbus_fn_id = lists_navigation_emit_list_invalidate,
};

static std::ostream &operator<<(std::ostream &os, const DBusFn id)
{
    if(id < DBusFn::first_valid_dbus_fn_id ||
       id > DBusFn::last_valid_dbus_fn_id)
    {
        os << "INVALID";
        return os;
    }

    switch(id)
    {
      case DBusFn::lists_navigation_emit_list_invalidate:
        os << "lists_navigation_emit_list_invalidate";
        break;
    }

    os << "()";

    return os;
}

class MockListbrokersDBus::Expectation
{
  public:
    const DBusFn function_id_;

    const tdbuslistsNavigation *const dbus_object_;
    const guint arg_list_id_;
    const guint arg_new_list_id_;

    Expectation(const Expectation &) = delete;
    Expectation &operator=(const Expectation &) = delete;

    explicit Expectation(DBusFn id, tdbuslistsNavigation *dbus_object,
                         guint list_id, guint new_list_id):
        function_id_(id),
        dbus_object_(dbus_object),
        arg_list_id_(list_id),
        arg_new_list_id_(new_list_id)
    {}

    Expectation(Expectation &&) = default;
};


MockListbrokersDBus::MockListbrokersDBus():
    ignore_all_(false)
{
    expectations_ = new MockExpectations();
}

MockListbrokersDBus::~MockListbrokersDBus()
{
    delete expectations_;
}

void MockListbrokersDBus::init()
{
    cppcut_assert_not_null(expectations_);
    expectations_->init();
}

void MockListbrokersDBus::check() const
{
    cppcut_assert_not_null(expectations_);
    expectations_->check();
}

void MockListbrokersDBus::expect_tdbus_lists_navigation_emit_list_invalidate(tdbuslistsNavigation *object, guint arg_list_id, guint arg_new_list_id)
{
    expectations_->add(Expectation(DBusFn::lists_navigation_emit_list_invalidate, object, arg_list_id, arg_new_list_id));
}


MockListbrokersDBus *mock_listbrokers_dbus_singleton = nullptr;

void tdbus_lists_navigation_emit_list_invalidate(tdbuslistsNavigation *object, guint arg_list_id, guint arg_new_list_id)
{
    const auto &expect(mock_listbrokers_dbus_singleton->expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.function_id_, DBusFn::lists_navigation_emit_list_invalidate);
    cppcut_assert_equal(expect.dbus_object_, object);
    cppcut_assert_equal(expect.arg_list_id_, arg_list_id);
    cppcut_assert_equal(expect.arg_new_list_id_, arg_new_list_id);
}

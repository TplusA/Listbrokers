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

#include "mock_upnp_dleynaserver_dbus.hh"

enum class DBusFn
{
    get_friendly_name,
    get_model_description,
    get_model_name,
    get_model_number,

    first_valid_dbus_fn_id = get_friendly_name,
    last_valid_dbus_fn_id = get_model_number,
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
      case DBusFn::get_friendly_name:
        os << "dleynaserver_media_device_get_friendly_name";
        break;

      case DBusFn::get_model_description:
        os << "dleynaserver_media_device_get_model_description";
        break;

      case DBusFn::get_model_name:
        os << "dleynaserver_media_device_get_model_name";
        break;

      case DBusFn::get_model_number:
        os << "dleynaserver_media_device_get_model_number";
        break;
    }

    os << "()";

    return os;
}

class MockDleynaServerDBus::Expectation
{
  public:
    const DBusFn function_id_;

    const tdbusdleynaserverMediaDevice *const dbus_object_;
    const std::string ret_string_;

    Expectation(const Expectation &) = delete;
    Expectation &operator=(const Expectation &) = delete;
    Expectation(Expectation &&) = default;

    explicit Expectation(DBusFn fn_id, const char *retval,
                         tdbusdleynaserverMediaDevice *dbus_object):
        function_id_(fn_id),
        dbus_object_(dbus_object),
        ret_string_(retval)
    {}
};


MockDleynaServerDBus::MockDleynaServerDBus()
{
    expectations_ = new MockExpectations();
}

MockDleynaServerDBus::~MockDleynaServerDBus()
{
    delete expectations_;
}

void MockDleynaServerDBus::init()
{
    cppcut_assert_not_null(expectations_);
    expectations_->init();
}

void MockDleynaServerDBus::check() const
{
    cppcut_assert_not_null(expectations_);
    expectations_->check();
}

void MockDleynaServerDBus::expect_tdbus_dleynaserver_media_device_get_friendly_name(const gchar *retval, tdbusdleynaserverMediaDevice *object)
{
    expectations_->add(Expectation(DBusFn::get_friendly_name, retval, object));
}

void MockDleynaServerDBus::expect_tdbus_dleynaserver_media_device_get_model_description(const gchar *retval, tdbusdleynaserverMediaDevice *object)
{
    expectations_->add(Expectation(DBusFn::get_model_description, retval, object));
}

void MockDleynaServerDBus::expect_tdbus_dleynaserver_media_device_get_model_name(const gchar *retval, tdbusdleynaserverMediaDevice *object)
{
    expectations_->add(Expectation(DBusFn::get_model_name, retval, object));
}

void MockDleynaServerDBus::expect_tdbus_dleynaserver_media_device_get_model_number(const gchar *retval, tdbusdleynaserverMediaDevice *object)
{
    expectations_->add(Expectation(DBusFn::get_model_number, retval, object));
}


MockDleynaServerDBus *mock_dleynaserver_dbus_singleton = nullptr;

const gchar *tdbus_dleynaserver_media_device_get_friendly_name(tdbusdleynaserverMediaDevice *object)
{
    const auto &expect(mock_dleynaserver_dbus_singleton->expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.function_id_, DBusFn::get_friendly_name);
    cppcut_assert_equal(expect.dbus_object_, object);

    return expect.ret_string_.c_str();
}

const gchar *tdbus_dleynaserver_media_device_get_model_description(tdbusdleynaserverMediaDevice *object)
{
    const auto &expect(mock_dleynaserver_dbus_singleton->expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.function_id_, DBusFn::get_model_description);
    cppcut_assert_equal(expect.dbus_object_, object);

    return expect.ret_string_.c_str();
}

const gchar *tdbus_dleynaserver_media_device_get_model_name(tdbusdleynaserverMediaDevice *object)
{
    const auto &expect(mock_dleynaserver_dbus_singleton->expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.function_id_, DBusFn::get_model_name);
    cppcut_assert_equal(expect.dbus_object_, object);

    return expect.ret_string_.c_str();
}

const gchar *tdbus_dleynaserver_media_device_get_model_number(tdbusdleynaserverMediaDevice *object)
{
    const auto &expect(mock_dleynaserver_dbus_singleton->expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.function_id_, DBusFn::get_model_number);
    cppcut_assert_equal(expect.dbus_object_, object);

    return expect.ret_string_.c_str();
}

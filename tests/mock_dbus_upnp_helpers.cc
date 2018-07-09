/*
 * Copyright (C) 2015, 2018  T+A elektroakustik GmbH & Co. KG
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

#include "mock_dbus_upnp_helpers.hh"

enum class DBusUPnPFn
{
    get_proxy_object_path,
    proxy_object_path_equals,
    create_media_device_proxy_for_object_path_begin,
    is_media_device_usable,
    get_size_of_container,

    first_valid_dbus_upnp_fn_id = get_proxy_object_path,
    last_valid_dbus_upnp_fn_id = get_size_of_container,
};

static std::ostream &operator<<(std::ostream &os, const DBusUPnPFn id)
{
    if(id < DBusUPnPFn::first_valid_dbus_upnp_fn_id ||
       id > DBusUPnPFn::last_valid_dbus_upnp_fn_id)
    {
        os << "INVALID";
        return os;
    }

    os << "UPnP::";

    switch(id)
    {
      case DBusUPnPFn::get_proxy_object_path:
        os << "get_proxy_object_path";
        break;

      case DBusUPnPFn::proxy_object_path_equals:
        os << "proxy_object_path_equals";
        break;

      case DBusUPnPFn::create_media_device_proxy_for_object_path_begin:
        os << "create_media_device_proxy_for_object_path_begin";
        break;

      case DBusUPnPFn::is_media_device_usable:
        os << "is_media_device_usable";
        break;

      case DBusUPnPFn::get_size_of_container:
        os << "get_size_of_container";
        break;
    }

    os << "()";

    return os;
}

class MockDBusUPnPHelpers::Expectation
{
  public:
    struct Data
    {
        const DBusUPnPFn function_id_;
        bool ret_bool_;
        uint32_t ret_uint32_;
        std::string ret_string_;
        tdbusdleynaserverMediaDevice *proxy_;
        std::string string_;
        get_path_callback_t get_path_fn_;
        create_proxy_callback_t create_proxy_fn_;
        path_equals_callback_t path_equals_fn_;

        explicit Data(DBusUPnPFn fn):
            function_id_(fn),
            ret_bool_(false),
            ret_uint32_(UINT32_MAX),
            proxy_(nullptr),
            get_path_fn_(nullptr),
            create_proxy_fn_(nullptr),
            path_equals_fn_(nullptr)
        {}
    };

    const Data d;

  private:
    /* writable reference for simple ctor code */
    Data &data_ = *const_cast<Data *>(&d);

  public:
    Expectation(const Expectation &) = delete;
    Expectation &operator=(const Expectation &) = delete;
    Expectation(Expectation &&) = default;

    explicit Expectation(const char *retval,
                         tdbusdleynaserverMediaDevice *proxy):
        d(DBusUPnPFn::get_proxy_object_path)
    {
        data_.ret_string_ = retval;
        data_.proxy_ = proxy;
    }

    explicit Expectation(get_path_callback_t fn):
        d(DBusUPnPFn::get_proxy_object_path)
    {
        data_.get_path_fn_ = fn;
    }

    explicit Expectation(create_proxy_callback_t fn):
        d(DBusUPnPFn::create_media_device_proxy_for_object_path_begin)
    {
        data_.create_proxy_fn_ = fn;
    }

    explicit Expectation(bool retval, tdbusdleynaserverMediaDevice *proxy):
        d(DBusUPnPFn::is_media_device_usable)
    {
        data_.ret_bool_ = retval;
        data_.proxy_ = proxy;
    }

    explicit Expectation(bool retval, tdbusdleynaserverMediaDevice *proxy,
                         const std::string &path):
        d(DBusUPnPFn::proxy_object_path_equals)
    {
        data_.ret_bool_ = retval;
        data_.proxy_ = proxy;
        data_.string_ = path;
    }

    explicit Expectation(path_equals_callback_t fn):
        d(DBusUPnPFn::proxy_object_path_equals)
    {
        data_.path_equals_fn_ = fn;
    }

    explicit Expectation(uint32_t retval, const std::string &path):
        d(DBusUPnPFn::get_size_of_container)
    {
        data_.ret_uint32_ = retval;
        data_.string_ = path;
    }
};

MockDBusUPnPHelpers::MockDBusUPnPHelpers()
{
    expectations_ = new MockExpectations();
}

MockDBusUPnPHelpers::~MockDBusUPnPHelpers()
{
    delete expectations_;
}

void MockDBusUPnPHelpers::init()
{
    cppcut_assert_not_null(expectations_);
    expectations_->init();
}

void MockDBusUPnPHelpers::check() const
{
    cppcut_assert_not_null(expectations_);
    expectations_->check();
}

void MockDBusUPnPHelpers::expect_get_proxy_object_path(const char *retval,
                                                       tdbusdleynaserverMediaDevice *proxy)
{
    expectations_->add(Expectation(retval, proxy));
}

void MockDBusUPnPHelpers::expect_get_proxy_object_path_callback(get_path_callback_t fn)
{
    expectations_->add(Expectation(fn));
}

void MockDBusUPnPHelpers::expect_proxy_object_path_equals(bool retval, tdbusdleynaserverMediaDevice *proxy, const std::string &path)
{
    expectations_->add(Expectation(retval, proxy, path));
}

void MockDBusUPnPHelpers::expect_proxy_object_path_equals_callback(path_equals_callback_t fn, size_t count)
{
    for(size_t i = 0; i < count; ++i)
        expectations_->add(Expectation(fn));
}

void MockDBusUPnPHelpers::expect_create_media_device_proxy_for_object_path_begin_callback(MockDBusUPnPHelpers::create_proxy_callback_t create_proxy_fn)
{
    expectations_->add(Expectation(create_proxy_fn));
}

void MockDBusUPnPHelpers::expect_is_media_device_usable(bool retval, tdbusdleynaserverMediaDevice *proxy)
{
    expectations_->add(Expectation(retval, proxy));
}

void MockDBusUPnPHelpers::expect_get_size_of_container(uint32_t retval,
                                                       const std::string &path)
{
    expectations_->add(Expectation(retval, path));
}


MockDBusUPnPHelpers *mock_dbus_upnp_helpers_singleton = nullptr;

std::string UPnP::get_proxy_object_path(tdbusdleynaserverMediaDevice *proxy)
{
    const auto &expect(mock_dbus_upnp_helpers_singleton->expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.d.function_id_, DBusUPnPFn::get_proxy_object_path);;

    if(expect.d.get_path_fn_ != nullptr)
        return expect.d.get_path_fn_(proxy);

    cppcut_assert_equal(expect.d.proxy_, proxy);

    return expect.d.ret_string_;
}

bool UPnP::proxy_object_path_equals(tdbusdleynaserverMediaDevice *proxy,
                                    const std::string &path)
{
    const auto &expect(mock_dbus_upnp_helpers_singleton->expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.d.function_id_, DBusUPnPFn::proxy_object_path_equals);

    if(expect.d.path_equals_fn_ != nullptr)
        return expect.d.path_equals_fn_(proxy, path);

    cppcut_assert_equal(expect.d.proxy_, proxy);
    cppcut_assert_equal(expect.d.string_, path);

    return expect.d.ret_bool_;
}

void UPnP::create_media_device_proxy_for_object_path_begin(const std::string &path,
                                                           GCancellable *cancellable,
                                                           GAsyncReadyCallback callback,
                                                           void *callback_data)
{
    const auto &expect(mock_dbus_upnp_helpers_singleton->expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.d.function_id_, DBusUPnPFn::create_media_device_proxy_for_object_path_begin);

    if(expect.d.create_proxy_fn_ != nullptr)
        return expect.d.create_proxy_fn_(path, callback, callback_data);

    cppcut_assert_equal(expect.d.string_, std::string(path));
}

bool UPnP::is_media_device_usable(tdbusdleynaserverMediaDevice *proxy)
{
    const auto &expect(mock_dbus_upnp_helpers_singleton->expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.d.function_id_, DBusUPnPFn::is_media_device_usable);
    cppcut_assert_equal(expect.d.proxy_, proxy);

    return expect.d.ret_bool_;
}

uint32_t UPnP::get_size_of_container(const std::string &path)
{
    const auto &expect(mock_dbus_upnp_helpers_singleton->expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.d.function_id_, DBusUPnPFn::get_size_of_container);
    cppcut_assert_equal(expect.d.string_, path);

    return expect.d.ret_uint32_;
}

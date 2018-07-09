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

#ifndef MOCK_DBUS_LISTS_IFACE_HH
#define MOCK_DBUS_LISTS_IFACE_HH

#include "dbus_lists_iface_deep.h"
#include "mock_expectation.hh"

class MockDBusListsIface
{
  public:
    MockDBusListsIface(const MockDBusListsIface &) = delete;
    MockDBusListsIface &operator=(const MockDBusListsIface &) = delete;

    class Expectation;
    typedef MockExpectationsTemplate<Expectation> MockExpectations;
    MockExpectations *expectations_;

    bool ignore_all_;

    explicit MockDBusListsIface();
    ~MockDBusListsIface();

    void init();
    void check() const;

    void expect_dbus_lists_setup(bool connect_to_session_bus,
                                 const char *dbus_object_path,
                                 struct DBusNavlistsIfaceData *iface_data);
    void expect_dbus_lists_get_navigation_iface(tdbuslistsNavigation *);
};

extern MockDBusListsIface *mock_dbus_lists_iface_singleton;

#endif /* !MOCK_DBUS_LISTS_IFACE_HH */

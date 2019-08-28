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

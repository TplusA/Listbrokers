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

#ifndef MOCK_LISTBROKERS_DBUS_HH
#define MOCK_LISTBROKERS_DBUS_HH

#include "de_tahifi_lists.h"
#include "mock_expectation.hh"

class MockListbrokersDBus
{
  public:
    MockListbrokersDBus(const MockListbrokersDBus &) = delete;
    MockListbrokersDBus &operator=(const MockListbrokersDBus &) = delete;

    class Expectation;
    typedef MockExpectationsTemplate<Expectation> MockExpectations;
    MockExpectations *expectations_;

    bool ignore_all_;

    explicit MockListbrokersDBus();
    ~MockListbrokersDBus();

    void init();
    void check() const;

    void expect_tdbus_lists_navigation_emit_list_invalidate(tdbuslistsNavigation *object, guint arg_list_id, guint arg_new_list_id);
};

extern MockListbrokersDBus *mock_listbrokers_dbus_singleton;

#endif /* !MOCK_LISTBROKERS_DBUS_HH */

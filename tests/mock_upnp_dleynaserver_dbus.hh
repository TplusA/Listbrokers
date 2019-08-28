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

#ifndef MOCK_UPNP_DLEYNASERVER_DBUS_HH
#define MOCK_UPNP_DLEYNASERVER_DBUS_HH

#include "upnp_dleynaserver_dbus.h"
#include "mock_expectation.hh"

class MockDleynaServerDBus
{
  public:
    MockDleynaServerDBus(const MockDleynaServerDBus &) = delete;
    MockDleynaServerDBus &operator=(const MockDleynaServerDBus &) = delete;

    class Expectation;
    typedef MockExpectationsTemplate<Expectation> MockExpectations;
    MockExpectations *expectations_;

    bool ignore_all_;

    explicit MockDleynaServerDBus();
    ~MockDleynaServerDBus();

    void init();
    void check() const;

    void expect_tdbus_dleynaserver_media_device_get_friendly_name(const gchar *retval, tdbusdleynaserverMediaDevice *object);
    void expect_tdbus_dleynaserver_media_device_get_model_description(const gchar *retval, tdbusdleynaserverMediaDevice *object);
    void expect_tdbus_dleynaserver_media_device_get_model_name(const gchar *retval, tdbusdleynaserverMediaDevice *object);
    void expect_tdbus_dleynaserver_media_device_get_model_number(const gchar *retval, tdbusdleynaserverMediaDevice *object);
};

extern MockDleynaServerDBus *mock_dleynaserver_dbus_singleton;

#endif /* !MOCK_UPNP_DLEYNASERVER_DBUS_HH */

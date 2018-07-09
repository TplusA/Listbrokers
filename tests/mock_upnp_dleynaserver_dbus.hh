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

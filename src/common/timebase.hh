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

#ifndef TIMEBASE_HH
#define TIMEBASE_HH

#include <chrono>

/*!
 * Interface with default implementation for obtaining time stamps.
 *
 * Functions may be overloaded by unit test code so that full, precise control
 * over time becomes possible during tests.
 */
class Timebase
{
  private:
    typedef std::chrono::steady_clock clock;

  public:
    typedef clock::time_point time_point;

    Timebase(const Timebase &) = delete;
    Timebase &operator=(const Timebase &) = delete;

    explicit Timebase() {}
    virtual ~Timebase() {}

    virtual time_point now() const { return clock::now(); }
};

#endif /* !TIMEBASE_HH */

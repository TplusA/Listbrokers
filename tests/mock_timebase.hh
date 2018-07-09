/*
 * Copyright (C) 2015, 2016  T+A elektroakustik GmbH & Co. KG
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

#ifndef MOCK_TIMEBASE_HH
#define MOCK_TIMEBASE_HH

#include "timebase.hh"

class MockTimebase: public Timebase
{
  private:
    time_point now_;
    std::chrono::milliseconds auto_increment_;

  public:
    MockTimebase(const MockTimebase &) = delete;
    MockTimebase &operator=(const MockTimebase &) = delete;

    explicit MockTimebase() {}
    virtual ~MockTimebase() {}

    time_point now() const override
    {
        const_cast<MockTimebase *>(this)->now_ += auto_increment_;
        return now_;
    }

    void reset()
    {
        now_ = time_point(std::chrono::milliseconds::zero());
        auto_increment_ = std::chrono::milliseconds::zero();
    }

    void step(unsigned int ms = 1)
    {
        now_ += std::chrono::milliseconds(ms);
    }

    void set_auto_increment(unsigned int ms = 1)
    {
        auto_increment_ = std::chrono::milliseconds(ms);
    }
};

#endif /* !MOCK_TIMEBASE_HH */

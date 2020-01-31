/*
 * Copyright (C) 2017, 2019, 2020  T+A elektroakustik GmbH & Co. KG
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

#include "ready.hh"

/*!
 * \addtogroup ready_state_tests Unit tests
 * \ingroup ready_state
 *
 * Ready state unit tests.
 */
/*!@{*/

namespace ready_probes_tests
{

void test_simple_probe_is_unready_by_default()
{
    Ready::SimpleProbe p;
    cut_assert_false(p.is_ready());
}

}

class WatcherExpectations
{
  private:
    std::vector<bool> expectations_;
    size_t next_expectation_;

  public:
    WatcherExpectations(const WatcherExpectations &) = delete;
    WatcherExpectations &operator=(const WatcherExpectations &) = delete;

    explicit WatcherExpectations():
        next_expectation_(0)
    {}

    void expect_false() { expectations_.push_back(false); }
    void expect_true()  { expectations_.push_back(true); }

    void set_state(bool state)
    {
        cppcut_assert_operator(expectations_.size(), >, next_expectation_);
        cppcut_assert_equal(bool(expectations_[next_expectation_]), state);
        ++next_expectation_;
    }

    void check()
    {
        cppcut_assert_equal(expectations_.size(), next_expectation_);
        expectations_.clear();
        next_expectation_ = 0;
    }
};

static void watcher(bool ready_state, WatcherExpectations *we)
{
    cppcut_assert_not_null(we);
    we->set_state(ready_state);
}

namespace ready_manager_tests
{

static Ready::Manager *ready;
static Ready::SimpleProbe *probe_1;
static Ready::SimpleProbe *probe_2;
static WatcherExpectations *watcher_expectations;

void cut_setup()
{
    watcher_expectations = new WatcherExpectations;
    probe_1 = new Ready::SimpleProbe;
    probe_2 = new Ready::SimpleProbe;
    ready = new Ready::Manager({ static_cast<Ready::Probe *>(probe_1), static_cast<Ready::Probe *>(probe_2) });

    ready->add_watcher([] (bool ready_state) { watcher(ready_state, watcher_expectations); }, false);
    watcher_expectations->check();
}

void cut_teardown()
{
    delete ready;
    delete probe_1;
    delete probe_2;
    ready = nullptr;
    probe_1 = nullptr;
    probe_2 = nullptr;

    watcher_expectations->check();
    delete watcher_expectations;
    watcher_expectations = nullptr;
}

void test_manager_reports_unready_on_watcher_registration_if_requested()
{
    delete ready;

    ready = new Ready::Manager({ static_cast<Ready::Probe *>(probe_1), static_cast<Ready::Probe *>(probe_2) });

    watcher_expectations->expect_false();
    ready->add_watcher([] (bool ready_state) { watcher(ready_state, watcher_expectations); }, true);
}

void test_manager_reports_ready_on_watcher_registration_if_requested()
{
    watcher_expectations->expect_true();
    probe_1->set_ready();
    probe_2->set_ready();
    watcher_expectations->check();

    delete ready;

    ready = new Ready::Manager({ static_cast<Ready::Probe *>(probe_1), static_cast<Ready::Probe *>(probe_2) });

    watcher_expectations->expect_true();
    ready->add_watcher([] (bool ready_state) { watcher(ready_state, watcher_expectations); }, true);
}

void test_manager_reports_unready_by_default()
{
    cut_assert_false(ready->is_ready());
}

void test_manager_unready_if_only_single_probe_is_ready()
{
    probe_1->set_ready();
    cut_assert_false(ready->is_ready());

    probe_1->set_unready();
    probe_2->set_ready();
    cut_assert_false(ready->is_ready());
}

void test_manager_ready_if_all_probes_are_ready()
{
    probe_1->set_ready();
    cut_assert_false(ready->is_ready());

    watcher_expectations->expect_true();

    probe_2->set_ready();
    cut_assert_true(ready->is_ready());
}

void test_manager_watchers_are_called_only_on_actual_probe_changes()
{
    probe_1->set_ready();
    cut_assert_false(ready->is_ready());

    watcher_expectations->expect_true();

    probe_2->set_ready();
    cut_assert_true(ready->is_ready());

    watcher_expectations->check();

    /* watchers are not called */
    probe_1->set_ready();
    probe_2->set_ready();
    probe_1->set_ready();
    probe_2->set_ready();
    cut_assert_true(ready->is_ready());

    /* drop to unready state */
    watcher_expectations->expect_false();

    probe_1->set_unready();
    cut_assert_false(ready->is_ready());

    watcher_expectations->check();

    /* watchers are not called */
    probe_1->set_unready();
    probe_1->set_unready();
    probe_2->set_unready();
    probe_2->set_unready();
    probe_2->set_ready();
    cut_assert_false(ready->is_ready());

    /* back to ready state */
    watcher_expectations->expect_true();

    probe_1->set_ready();
    cut_assert_true(ready->is_ready());
}

void test_probes_can_be_extracted_from_manager()
{
    cppcut_assert_equal(static_cast<Ready::Probe *>(probe_1), ready->get_probe(0));
    cppcut_assert_equal(static_cast<Ready::Probe *>(probe_2), ready->get_probe(1));
    cppcut_assert_null(ready->get_probe(2));
}

}

/*!@}*/

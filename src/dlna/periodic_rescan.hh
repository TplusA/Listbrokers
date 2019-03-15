/*
 * Copyright (C) 2019  T+A elektroakustik GmbH & Co. KG
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

#ifndef PERIODIC_RESCAN_HH
#define PERIODIC_RESCAN_HH

struct _GObject;
struct _GAsyncResult;

namespace UPnP
{

/*!
 * The permanent rescan button clicker...
 */
class PeriodicRescan
{
  private:
    const unsigned int interval_seconds_;
    bool is_inhibited_;
    unsigned int timeout_id_;

  public:
    PeriodicRescan(const PeriodicRescan &) = delete;
    PeriodicRescan(PeriodicRescan &&) = default;
    PeriodicRescan &operator=(const PeriodicRescan &) = delete;
    PeriodicRescan &operator=(PeriodicRescan &&) = default;

    explicit PeriodicRescan(unsigned int interval_seconds):
        interval_seconds_(interval_seconds),
        is_inhibited_(false),
        timeout_id_(0)
    {}

    void enable();
    void disable();

  private:
    static int rescan_now_trampoline(void *scan);
    int rescan_now();
    static void rescan_done(struct _GObject *source_object,
                            struct _GAsyncResult *res, void *scan);
};

}

#endif /* !PERIODIC_RESCAN_HH */

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

#ifndef DBUS_UPNP_LIST_FILLER_HH
#define DBUS_UPNP_LIST_FILLER_HH

#include "upnp_list.hh"
#include "lru.hh"

namespace UPnP
{

/*!
 * Fill list items from UPnP sources using dLeyna over D-Bus.
 */
class DBusUPnPFiller: public TiledListFillerIface<ItemData>
{
  private:
    const LRU::Cache *cache_;
    bool request_alphabetically_sorted_;

  public:
    DBusUPnPFiller(const DBusUPnPFiller &) = delete;
    DBusUPnPFiller &operator=(const DBusUPnPFiller &) = delete;

    explicit DBusUPnPFiller():
        cache_(nullptr),
        request_alphabetically_sorted_(false)
    {}

    /*!
     * Init object at runtime after static initialization.
     *
     * \todo This is annoying. Static initialization should be possible.
     */
    void init(const LRU::Cache &cache)
    {
        cache_ = &cache;
    }

    ssize_t fill(ItemProvider<UPnP::ItemData> &item_provider, ID::List list_id,
                 ID::Item idx, size_t count, ListError &error,
                 const std::function<bool()> &may_continue) const override;
};

};

#endif /* !DBUS_UPNP_LIST_FILLER_HH */

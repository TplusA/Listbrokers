#ifndef LRU_KILLED_LISTS_HH
#define LRU_KILLED_LISTS_HH

/*
 * Copyright (C) 2022  T+A elektroakustik GmbH & Co. KG
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

#include "idtypes.hh"
#include "logged_lock.hh"

#include <set>

namespace LRU
{

/*!
 * Record lists which have been removed without notifying the cache.
 *
 * Inside the various list implementations based on #TiledList, it is possible
 * for cached child lists to be removed directly from the list structure
 * without notifying the LRU cache which manages list lifecycles. This happens
 * when a hot tile gets invalidated in order to be replaced by a new tile
 * representing another part of the original list. Commonly, this is caused by
 * quick navigation.
 *
 * Ideally, the lists should notify their LRU cache about the self-authorized
 * removal of a list. However, since a #TiledList doesn't know anything about
 * the #LRU::Cache which is managing it and because there are various threads
 * involved in #TiledList operation, it would get more complicated than
 * necessary to implement this. The cache structure is not thread-safe and is
 * not protected by locks anywhere, so we'd have to add locking all over the
 * place.
 *
 * Thus, we take a different approach. Since the cache is scrubbed regularly
 * via garbage collection driven by #LRU::CacheControl (based on timing and
 * cache size), there will be no leaks anyway. On garbage collection, it is
 * possible that a list is about to be removed which doesn't exist anymore as a
 * result of how #TiledList is working. A bug message was triggered in previous
 * versions, but now we try to remove such a list from the #LRU::KilledLists
 * structure to suppress false bug messages. The bug messages still appear for
 * non-existent lists which have \e not been removed by #TiledList.
 *
 * The #LRU::KilledLists class itself is thread-safe. Note, however, that we
 * only have a single instance of this class. Strictly speaking, this
 * constricts our original design as we are now constraint to use a single
 * #LRU::Cache object in the whole program. There should be one
 * #LRU::KilledLists object per #LRU::Cache, but since we really only ever have
 * to deal with a single #LRU::Cache instance, we are going with a singleton to
 * keep it simple.
 */
class KilledLists
{
  private:
    mutable LoggedLock::Mutex lock_;
    std::set<ID::List> killed_;

  public:
    KilledLists(const KilledLists &) = delete;
    explicit KilledLists() = default;

    void killed(ID::List list_id);
    bool erase(ID::List list_id);
    bool reset();
    void dump(const char *fn, int line) const;

    static KilledLists &get_singleton();
};

}

#endif /* !LRU_KILLED_LISTS_HH */

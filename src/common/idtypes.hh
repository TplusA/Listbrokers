/*
 * Copyright (C) 2015, 2016, 2019, 2023  T+A elektroakustik GmbH & Co. KG
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

#ifndef IDTYPES_HH
#define IDTYPES_HH

#include <inttypes.h>

#include "de_tahifi_lists_context.h"

namespace ID
{

/*!
 * \internal
 * Class template for type-safe IDs.
 *
 * \tparam Traits
 *     Structure that defines an \c is_valid() function. The function shall
 *     return a \c bool that is true if and only if the ID given to the
 *     function is within the range valid for the specific ID type. It does
 *     \e not check if the ID is valid in the context of its use (e.g., if the
 *     ID is a valid ID for a cached object in some cache).
 */
template <typename Traits>
class IDType_
{
  private:
    uint32_t id_;

  public:
    constexpr explicit IDType_(uint32_t id = 0) throw():
        id_(id)
    {}

    uint32_t get_raw_id() const noexcept
    {
        return id_;
    }

    bool is_valid() const noexcept
    {
        return Traits::is_valid(id_);
    }

    bool operator<(const IDType_<Traits> &other) const noexcept
    {
        return id_ < other.id_;
    }

    bool operator!=(const IDType_<Traits> &other) const noexcept
    {
        return id_ != other.id_;
    }

    bool operator==(const IDType_<Traits> &other) const noexcept
    {
        return id_ == other.id_;
    }
};

/*!
 * \internal
 * Traits class for list IDs.
 */
struct ListIDTraits_
{
    static inline bool is_valid(uint32_t id) { return id > 0; }
};

/*!
 * \internal
 * Traits class for list item IDs.
 */
struct ItemIDTraits_
{
    static inline bool is_valid(uint32_t id) { return true; }
};

/*!
 * \internal
 * Traits class for list item positions.
 */
struct RefPosIDTraits_
{
    static inline bool is_valid(uint32_t id) { return id > 0; }
};

/*!
 * Type to use for list IDs.
 *
 * List IDs identify whole lists. Their contents are identified by ascending
 * item IDs (see #ID::Item).
 */
class List
{
  public:
    using context_t = uint8_t;

    static constexpr const uint32_t NOCACHE_BIT =
        ((DBUS_LISTS_CONTEXT_ID_MASK >> 1) & ~DBUS_LISTS_CONTEXT_ID_MASK);

    static constexpr const uint32_t VALUE_MASK =
        ~(DBUS_LISTS_CONTEXT_ID_MASK | NOCACHE_BIT);

  private:
    using ID = IDType_<ListIDTraits_>;

    ID id_;

  public:
    constexpr explicit List(uint32_t id = 0) throw(): id_(id) {}
    uint32_t get_raw_id() const noexcept { return id_.get_raw_id(); }
    bool operator<(const List &other) const noexcept { return id_ < other.id_; }
    bool operator!=(const List &other) const noexcept { return id_ != other.id_; }
    bool operator==(const List &other) const noexcept { return id_ == other.id_; }

    bool is_valid() const noexcept
    {
        return ListIDTraits_::is_valid(id_.get_raw_id() & VALUE_MASK);
    }

    uint32_t get_cooked_id() const noexcept
    {
        return id_.get_raw_id() & VALUE_MASK;
    }

    context_t get_context() const noexcept
    {
        return DBUS_LISTS_CONTEXT_GET(id_.get_raw_id());
    }

    bool get_nocache_bit() const noexcept
    {
        return (id_.get_raw_id() & NOCACHE_BIT) != 0;
    }
};

/*!
 * Type to use to identify list items.
 *
 * Item IDs are unique within a list. They are basically just numbers starting
 * at zero and refer directly to a list index.
 *
 * The main purpose of this type is enhanced type-safety compared to a plain
 * integer type. It teaches the compiler how to figure out and to tell us when
 * we confuse a list ID with an item ID.
 */
typedef IDType_<ItemIDTraits_> Item;

}

#endif /* !IDTYPES_HH */

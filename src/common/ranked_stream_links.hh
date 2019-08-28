/*
 * Copyright (C) 2017, 2019  T+A elektroakustik GmbH & Co. KG
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

#ifndef RANKED_STREAM_LINKS_HH
#define RANKED_STREAM_LINKS_HH

#include "urlstring.hh"

namespace Url
{

class RankedStreamLinks
{
  private:
    uint32_t rank_;
    uint32_t bitrate_bits_per_second_;
    StreamLink link_;

  public:
    RankedStreamLinks(const RankedStreamLinks &) = delete;
    RankedStreamLinks(RankedStreamLinks &&) = default;
    RankedStreamLinks &operator=(const RankedStreamLinks &) = delete;

    explicit RankedStreamLinks(uint32_t rank, uint32_t rate,
                               const std::string &uri):
        rank_(rank),
        bitrate_bits_per_second_(rate),
        link_(uri)
    {}

    explicit RankedStreamLinks(uint32_t rank, uint32_t rate,
                               const char *const uri):
        rank_(rank),
        bitrate_bits_per_second_(rate),
        link_(uri)
    {}

    uint32_t get_rank() const { return rank_; }
    uint32_t get_bitrate() const { return bitrate_bits_per_second_; }
    const StreamLink &get_stream_link() const { return link_; }
};

}

#endif /* !RANKED_STREAM_LINKS_HH */

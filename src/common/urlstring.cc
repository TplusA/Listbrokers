/*
 * Copyright (C) 2016, 2017  T+A elektroakustik GmbH & Co. KG
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

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "urlstring.hh"

void Url::String::compute_hash(MD5::Hash &hash) const
{
    MD5::Context ctx;
    MD5::init(ctx);
    MD5::update(ctx,
                static_cast<const uint8_t *>(static_cast<const void *>(url_.c_str())),
                url_.size());
    MD5::finish(ctx, hash);
}

static inline char rot(const char ch, const char base)
{
    return base + (((ch - base) + 13) % 26);
}

void Url::String::copy_concealed(std::string &dest, const std::string &url)
{
    dest.clear();
    dest.reserve(url.length());

    for(const auto &ch : url)
    {
        if(ch >= 'a' && ch <= 'z')
            dest.push_back(rot(ch, 'a'));
        else if(ch >= 'A' && ch <= 'z')
            dest.push_back(rot(ch, 'A'));
        else
            dest.push_back(ch);
    }
}

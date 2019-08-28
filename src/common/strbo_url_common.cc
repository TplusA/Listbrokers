/*
 * Copyright (C) 2017, 2019  T+A elektroakustik GmbH & Co. KG
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

#include <string>
#include <limits>

#include "strbo_url.hh"
#include "messages.h"

const std::string Url::Location::valid_characters =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789$-_.~+!*'(),;/?:@=&%";

const std::string Url::Location::safe_characters =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789$-_.~";

void Url::for_each_url_encoded(const std::string &src,
                               const std::function<void(const char *, size_t)> &apply)
{
    for(const char &ch : src)
    {
        if(Url::Location::safe_characters.find(ch) != std::string::npos)
            apply(&ch, 1);
        else
        {
            char buffer[4];
            const int len = snprintf(buffer, sizeof(buffer), "%%%02X", ch);

            apply(buffer, len);
        }
    }
}

static bool decode(const char ch1, const char ch2, uint8_t &out)
{
    if(ch1 >= '0' && ch1 <= '9')
        out = static_cast<uint8_t>(ch1 - '0') << 4;
    else if(ch1 >= 'A' && ch1 <= 'F')
        out = static_cast<uint8_t>(ch1 - 'A' + 10) << 4;
    else
        return false;

    if(ch2 >= '0' && ch2 <= '9')
        out |= static_cast<uint8_t>(ch2 - '0');
    else if(ch2 >= 'A' && ch2 <= 'F')
        out |= static_cast<uint8_t>(ch2 - 'A' + 10);
    else
        return false;

    return true;
}

void Url::for_each_url_decoded(const std::string &src,
                               const std::function<void(char)> &apply)
{
    for(size_t i = 0; i < src.length(); ++i)
    {
        const char ch = src[i];

        if(ch != '%')
        {
            apply(ch);
            continue;
        }

        if(i + 3 <= src.length())
        {
            uint8_t out;

            if(decode(src[i + 1], src[i + 2], out))
            {
                i += 2;
                apply(out);
                continue;
            }

            msg_error(0, LOG_NOTICE,
                      "Invalid URL-encoding \"%c%c%c\" in URL \"%s\"",
                      src[i + 0], src[i + 1], src[i + 2], src.c_str());
        }
        else
            msg_error(0, LOG_NOTICE,
                      "URL too short for last code: \"%s\"", src.c_str());

        break;
    }
}

bool Url::Parse::extract_field(const std::string &url, size_t offset,
                               const char separator, FieldPolicy policy,
                               size_t &end_of_field, const char *error_prefix,
                               const char *component_name)
{
    end_of_field = url.find(separator, offset);

    if(end_of_field == std::string::npos)
    {
        switch(policy)
        {
          case FieldPolicy::FIELD_OPTIONAL:
            return true;

          case FieldPolicy::MAY_BE_EMPTY:
          case FieldPolicy::MUST_NOT_BE_EMPTY:
            msg_error(0, LOG_NOTICE, "%sNo '%c' found", error_prefix, separator);
            break;
        }

        return false;
    }

    switch(policy)
    {
      case FieldPolicy::FIELD_OPTIONAL:
      case FieldPolicy::MAY_BE_EMPTY:
        break;

      case FieldPolicy::MUST_NOT_BE_EMPTY:
        if(end_of_field <= offset)
        {
            msg_error(0, LOG_NOTICE,
                      "%s%s component empty", error_prefix, component_name);
            return false;
        }

        break;
    }

    return true;
}

static bool parse_item_position(const std::string &url, size_t offset,
                                size_t expected_end,
                                bool expecting_zero_terminator,
                                ID::RefPos &parsed_position,
                                const char *error_prefix,
                                const char *component_name)
{
    if(offset >= expected_end)
    {
        msg_error(0, LOG_NOTICE,
                  "%s%s component empty", error_prefix, component_name);
        return false;
    }

    char *endptr = nullptr;
    unsigned long long temp = strtoull(&url[offset], &endptr, 10);

    if(*endptr != '\0' && expecting_zero_terminator)
    {
        msg_error(0, LOG_NOTICE, "%s%s component with trailing junk",
                  error_prefix, component_name);
        return false;
    }

    if((temp == std::numeric_limits<unsigned long long>::max() && errno == ERANGE) ||
       temp > std::numeric_limits<uint32_t>::max())
    {
        msg_error(0, LOG_NOTICE, "%s%s component out of range",
                  error_prefix, component_name);
        return false;
    }

    parsed_position = ID::RefPos(temp);

    return true;
}

bool Url::Parse::item_position(const std::string &url, size_t offset,
                               size_t expected_end, ID::RefPos &parsed_pos,
                               const char *error_prefix,
                               const char *component_name)
{
    return parse_item_position(url, offset, expected_end, false, parsed_pos,
                               error_prefix, component_name);
}

bool Url::Parse::item_position(const std::string &url, size_t offset,
                               ID::RefPos &parsed_pos,
                               const char *error_prefix,
                               const char *component_name)
{
    return parse_item_position(url, offset, url.length(), true, parsed_pos,
                               error_prefix, component_name);
}

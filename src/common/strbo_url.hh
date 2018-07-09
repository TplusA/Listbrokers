/*
 * Copyright (C) 2017  T+A elektroakustik GmbH & Co. KG
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

#ifndef STRBO_URL_HH
#define STRBO_URL_HH

#include <string>
#include <functional>

#include "strbo_url_schemes.hh"
#include "idtypes.hh"

namespace Url
{

enum class Encoding
{
    IS_URL_ENCODED,
    IS_PLAIN_TEXT,
};

void for_each_url_encoded(const std::string &src,
                          const std::function<void(const char *, size_t)> &apply);

void for_each_url_decoded(const std::string &src,
                          const std::function<void(char)> &apply);

static inline void copy_encoded(const std::string &src, Encoding src_enc,
                                std::string &dest)
{
    switch(src_enc)
    {
      case Url::Encoding::IS_URL_ENCODED:
        dest = src;
        break;

      case Url::Encoding::IS_PLAIN_TEXT:
        dest.clear();
        for_each_url_encoded(src,
            [&dest] (const char *enc, size_t len) { dest.append(enc, len); });
        break;
    }
}

namespace Parse
{

enum class FieldPolicy
{
    FIELD_OPTIONAL,
    MAY_BE_EMPTY,
    MUST_NOT_BE_EMPTY,
};

bool extract_field(const std::string &url, size_t offset, const char separator,
                   FieldPolicy policy, size_t &end_of_field,
                   const char *error_prefix, const char *component_name);
bool item_position(const std::string &url, size_t offset, size_t expected_end,
                   ID::RefPos &parsed_pos, const char *error_prefix,
                   const char *component_name);
bool item_position(const std::string &url, size_t offset,
                   ID::RefPos &parsed_pos, const char *error_prefix,
                   const char *component_name);
}

/*!
 * Base class for Streaming Board location URLs.
 *
 * A location URL can be a location key (following a resource locator scheme)
 * or a location trace (folowing a trace locator scheme).
 */
class Location
{
  public:
    enum class SetURLResult
    {
        OK,
        WRONG_SCHEME,
        INVALID_CHARACTERS,
        PARSING_ERROR,
    };

    static const std::string valid_characters;
    static const std::string safe_characters;

  protected:
    const Schema::StrBoLocator &scheme_;

    explicit Location(const Schema::StrBoLocator &scheme):
        scheme_(scheme)
    {}

  public:
    Location(const Location &) = delete;
    Location &operator=(const Location &) = delete;

    virtual ~Location() {}

    virtual void clear() = 0;
    virtual bool is_valid() const = 0;

    std::string str() const
    {
        if(is_valid())
            return str_impl();
        else
            return "";
    }

    SetURLResult set_url(const std::string &url)
    {
        if(!scheme_.url_matches_scheme(url))
            return SetURLResult::WRONG_SCHEME;

        if(url.find_first_not_of(valid_characters) != std::string::npos)
            return SetURLResult::INVALID_CHARACTERS;

        return set_url_impl(url, scheme_.get_scheme_name().length() + 3)
            ? SetURLResult::OK
            : SetURLResult::PARSING_ERROR;
    }

  protected:
    /*!
     * Return string representation of the location.
     *
     * This function is supposed to return a URL following its configured
     * scheme.
     *
     * Contract: This function is called only if a preceding call of
     *     #Url::Location::is_valid() returned \c true. Therefore, this
     *     function needs to perform no further checks and is required to
     *     return a valid, non-empty URL.
     */
    virtual std::string str_impl() const = 0;

    /*!
     * Set URL object by string.
     *
     * This function is supposed to parse the URL and initialize the object
     * using the URL components.
     *
     * It shall not simply copy the URL. The #Url::Location::str() function
     * member shall always generate URL strings from the object state, not from
     * some copied string.
     *
     * Contract: The URL scheme is guaranteed to match the configurated scheme.
     *     Implementations should not check the scheme prefix again. The offset
     *     parameter points at the first character after the scheme definition.
     */
    virtual bool set_url_impl(const std::string &url, size_t offset) = 0;
};

}

#endif /* !STRBO_URL_HH */

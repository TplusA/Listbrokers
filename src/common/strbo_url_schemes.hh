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

#ifndef STRBO_URL_SCHEMES_HH
#define STRBO_URL_SCHEMES_HH

#include <string>

namespace Url
{

namespace Schema
{

/*!
 * Base class for Streaming Board locator schemes.
 *
 * This class is concerned with the representation of schemes, not with URLs
 * which are supposed to follow a scheme. It does not do much more than turning
 * a scheme name into a C++ type, thus providing type-safety, and defining a
 * function for checking whether or not a given URL matches the scheme.
 */
class StrBoLocator
{
  private:
    const std::string scheme_name_;

  protected:
    explicit StrBoLocator(std::string &&scheme_name):
        scheme_name_(std::move(scheme_name))
    {}

  public:
    StrBoLocator(const StrBoLocator &) = delete;
    StrBoLocator &operator=(const StrBoLocator &) = delete;

    virtual ~StrBoLocator() {}

    const std::string &get_scheme_name() const { return scheme_name_; }

    bool url_matches_scheme(const std::string &url) const
    {
        /* URL must be no shorter than the scheme name plus "://" */
        if(url.length() < scheme_name_.length() + 3)
            return false;

        /* prefix must match the scheme name */
        if(url.compare(0, scheme_name_.length(), scheme_name_) != 0)
            return false;

        /* scheme name must be followed by "://" separator */
        if(url.compare(scheme_name_.length(), 3, "://") != 0)
            return false;

        return true;
    }
};

/*!
 * Base class for simple resource locator schemes.
 */
class ResourceLocatorSimple: public StrBoLocator
{
  protected:
    explicit ResourceLocatorSimple(std::string &&scheme_name):
        StrBoLocator(std::move(scheme_name))
    {}

  public:
    ResourceLocatorSimple(const ResourceLocatorSimple &) = delete;
    ResourceLocatorSimple &operator=(const ResourceLocatorSimple &) = delete;

    virtual ~ResourceLocatorSimple() {}
};

/*!
 * Base class for reference resource locator schemes.
 */
class ResourceLocatorReference: public StrBoLocator
{
  protected:
    explicit ResourceLocatorReference(std::string &&scheme_name):
        StrBoLocator(std::move(scheme_name))
    {}

  public:
    ResourceLocatorReference(const ResourceLocatorReference &) = delete;
    ResourceLocatorReference &operator=(const ResourceLocatorReference &) = delete;

    virtual ~ResourceLocatorReference() {}
};

/*!
 * Base class for trace locator schemes.
 */
class TraceLocator: public StrBoLocator
{
  protected:
    explicit TraceLocator(std::string &&scheme_name):
        StrBoLocator(std::move(scheme_name))
    {}

  public:
    TraceLocator(const TraceLocator &) = delete;
    TraceLocator &operator=(const TraceLocator &) = delete;

    virtual ~TraceLocator() {}
};

}

}

#endif /* !STRBO_URL_SCHEMES_HH */

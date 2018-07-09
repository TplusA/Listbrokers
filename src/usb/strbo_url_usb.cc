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

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <string>
#include <sstream>

#include "strbo_url_usb.hh"
#include "messages.h"

std::string USB::LocationKeySimple::str_impl() const
{
    std::string result = scheme_.get_scheme_name() + "://";

    Url::for_each_url_encoded(c_.device_,
        [&result] (const char *enc, size_t len) { result.append(enc, len); });

    result += ':';

    Url::for_each_url_encoded(c_.partition_,
        [&result] (const char *enc, size_t len) { result.append(enc, len); });

    result += '/';

    Url::for_each_url_encoded(c_.path_,
        [&result] (const char *enc, size_t len) { result.append(enc, len); });

    return result;
}

static bool parse_device_and_partition(const std::string &url, size_t offset,
                                       const char *error_prefix,
                                       size_t &end_of_device,
                                       size_t &end_of_partition)
{
    if(!Url::Parse::extract_field(url, offset, ':',
                                  Url::Parse::FieldPolicy::MUST_NOT_BE_EMPTY,
                                  end_of_device, error_prefix, "Device"))
        return false;

    if(!Url::Parse::extract_field(url, offset, '/',
                                  Url::Parse::FieldPolicy::MUST_NOT_BE_EMPTY,
                                  end_of_partition, error_prefix, "Partition"))
        return false;

    if(end_of_partition <= end_of_device)
    {
        msg_error(0, LOG_NOTICE,
                  "%sFailed parsing device and partition", error_prefix);
        return false;
    }

    return true;
}

bool USB::LocationKeySimple::set_url_impl(const std::string &url, size_t offset)
{
    static const char error_prefix[] = "Simple USB location key malformed: ";

    size_t end_of_device;
    size_t end_of_partition;

    if(!parse_device_and_partition(url, offset, error_prefix,
                                   end_of_device, end_of_partition))
        return false;

    c_.device_.clear();
    c_.partition_.clear();
    c_.path_.clear();

    Url::for_each_url_decoded(url.substr(offset, end_of_device - offset),
                              [this] (const char ch) { c_.device_ += ch; } );
    Url::for_each_url_decoded(url.substr(end_of_device + 1, end_of_partition - end_of_device - 1),
                              [this] (const char ch) { c_.partition_ += ch; } );
    Url::for_each_url_decoded(url.substr(end_of_partition + 1),
                              [this] (const char ch) { c_.path_ += ch; } );

    is_partition_set_ = true;
    is_path_set_ = true;

    return true;
}

std::string USB::LocationKeyReference::str_impl() const
{
    std::ostringstream os;
    os << scheme_.get_scheme_name() << "://";

    Url::for_each_url_encoded(c_.device_,
        [&os] (const char *enc, size_t len) { os.write(enc, len); });

    os << ':';

    Url::for_each_url_encoded(c_.partition_,
        [&os] (const char *enc, size_t len) { os.write(enc, len); });

    os << '/';

    Url::for_each_url_encoded(c_.reference_point_,
        [&os] (const char *enc, size_t len) { os.write(enc, len); });

    os << '/';

    Url::for_each_url_encoded(c_.item_name_,
        [&os] (const char *enc, size_t len) { os.write(enc, len); });

    os << ':' << c_.item_position_.get_raw_id();

    return os.str();
}

bool USB::LocationKeyReference::set_url_impl(const std::string &url, size_t offset)
{
    static const char error_prefix[] = "Reference USB location key malformed: ";

    size_t end_of_device;
    size_t end_of_partition;

    if(!parse_device_and_partition(url, offset, error_prefix,
                                   end_of_device, end_of_partition))
        return false;

    size_t end_of_reference;

    if(!Url::Parse::extract_field(url, end_of_partition + 1, '/',
                                  Url::Parse::FieldPolicy::MAY_BE_EMPTY,
                                  end_of_reference, error_prefix,
                                  "Reference point"))
        return false;

    const bool is_reference_empty = end_of_reference == end_of_partition + 1;

    size_t end_of_item;

    if(!Url::Parse::extract_field(url, end_of_reference + 1, ':',
                                  is_reference_empty
                                  ? Url::Parse::FieldPolicy::MAY_BE_EMPTY
                                  : Url::Parse::FieldPolicy::MUST_NOT_BE_EMPTY,
                                  end_of_item, error_prefix, "Item name"))
        return false;

    ID::RefPos item_position;

    if(!Url::Parse::item_position(url, end_of_item + 1, item_position,
                                  error_prefix, "Item position"))
        return false;

    std::string temp;
    bool is_path = false;

    Url::for_each_url_decoded(url.substr(end_of_reference + 1, end_of_item - end_of_reference - 1),
                              [&temp, &is_path] (const char ch)
                              {
                                  temp += ch;

                                  if(ch == '/')
                                      is_path = true;
                              } );

    if(is_path)
    {
        msg_error(0, LOG_NOTICE, "%sItem component is a path", error_prefix);
        return false;
    }

    c_.device_.clear();
    c_.partition_.clear();
    c_.reference_point_.clear();
    c_.item_name_ = std::move(temp);
    c_.item_position_ = item_position;

    Url::for_each_url_decoded(url.substr(offset, end_of_device - offset),
                              [this] (const char ch) { c_.device_ += ch; } );
    Url::for_each_url_decoded(url.substr(end_of_device + 1, end_of_partition - end_of_device - 1),
                              [this] (const char ch) { c_.partition_ += ch; } );
    Url::for_each_url_decoded(url.substr(end_of_partition + 1, end_of_reference - end_of_partition - 1),
                              [this] (const char ch) { c_.reference_point_ += ch; } );

    is_partition_set_ = true;
    is_reference_point_set_ = true;
    is_item_set_ = true;

    return true;
}

size_t USB::LocationTrace::get_trace_length() const
{

    if(c_.item_name_.empty())
        return 0;

    size_t result = 1;

    for(const char &ch : c_.item_name_)
        if(ch == '/')
            ++result;

    return result;
}

std::string USB::LocationTrace::str_impl() const
{
    std::ostringstream os;
    os << scheme_.get_scheme_name() << "://";

    Url::for_each_url_encoded(c_.device_,
        [&os] (const char *enc, size_t len) { os.write(enc, len); });

    os << ':';

    Url::for_each_url_encoded(c_.partition_,
        [&os] (const char *enc, size_t len) { os.write(enc, len); });

    os << '/';

    if(!c_.reference_point_.empty())
    {
        Url::for_each_url_encoded(c_.reference_point_,
            [&os] (const char *enc, size_t len) { os.write(enc, len); });

        os << '/';
    }

    Url::for_each_url_encoded(c_.item_name_,
        [&os] (const char *enc, size_t len) { os.write(enc, len); });

    os << ':' << c_.item_position_.get_raw_id();

    return os.str();
}

bool USB::LocationTrace::set_url_impl(const std::string &url, size_t offset)
{
    static const char error_prefix[] = "USB location trace malformed: ";

    size_t end_of_device;
    size_t end_of_partition;

    if(!parse_device_and_partition(url, offset, error_prefix,
                                   end_of_device, end_of_partition))
        return false;

    size_t end_of_reference;

    if(url.find('/', end_of_partition + 1) != std::string::npos)
    {
        if(!Url::Parse::extract_field(url, end_of_partition + 1, '/',
                                      Url::Parse::FieldPolicy::MAY_BE_EMPTY,
                                      end_of_reference, error_prefix,
                                      "Reference point"))
            return false;
    }
    else
        end_of_reference = end_of_partition;

    const bool is_reference_empty = end_of_reference == end_of_partition;

    size_t end_of_item;

    if(!Url::Parse::extract_field(url, end_of_reference + 1, ':',
                                  is_reference_empty
                                  ? Url::Parse::FieldPolicy::MAY_BE_EMPTY
                                  : Url::Parse::FieldPolicy::MUST_NOT_BE_EMPTY,
                                  end_of_item, error_prefix, "Item name"))
        return false;

    ID::RefPos item_position;

    if(!Url::Parse::item_position(url, end_of_item + 1, item_position,
                                  error_prefix, "Item position"))
        return false;

    c_.device_.clear();
    c_.partition_.clear();
    c_.reference_point_.clear();
    c_.item_name_.clear();
    c_.item_position_ = item_position;

    Url::for_each_url_decoded(url.substr(offset, end_of_device - offset),
                              [this] (const char ch) { c_.device_ += ch; } );
    Url::for_each_url_decoded(url.substr(end_of_device + 1, end_of_partition - end_of_device - 1),
                              [this] (const char ch) { c_.partition_ += ch; } );

    if(end_of_partition < end_of_reference)
        Url::for_each_url_decoded(url.substr(end_of_partition + 1,
                                             end_of_reference - end_of_partition - 1),
                                  [this] (const char ch) { c_.reference_point_ += ch; } );

    Url::for_each_url_decoded(url.substr(end_of_reference + 1, end_of_item - end_of_reference - 1),
                              [this] (const char ch) { c_.item_name_ += ch; } );

    if(c_.reference_point_ == "/")
    {
        msg_error(0, LOG_WARNING,
                  "USB location trace contains unneeded explicit reference to root");
        c_.reference_point_.clear();
    }

    is_partition_set_ = true;
    is_item_set_ = true;

    return true;
}

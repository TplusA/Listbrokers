/*
 * Copyright (C) 2017, 2019, 2020  T+A elektroakustik GmbH & Co. KG
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

#ifndef STRBO_URL_USB_HH
#define STRBO_URL_USB_HH

#include <string>

#include "strbo_url_schemes.hh"
#include "strbo_url.hh"

namespace USB
{

/*!
 * The \c strbo-usb scheme.
 */
class ResourceLocatorSimple: public ::Url::Schema::ResourceLocatorSimple
{
  public:
    explicit ResourceLocatorSimple():
        Url::Schema::ResourceLocatorSimple("strbo-usb")
    {}
};

/*!
 * The \c strbo-ref-usb scheme.
 */
class ResourceLocatorReference: public ::Url::Schema::ResourceLocatorReference
{
  public:
    explicit ResourceLocatorReference():
        Url::Schema::ResourceLocatorReference("strbo-ref-usb")
    {}
};

/*!
 * The \c strbo-trace-usb scheme.
 */
class TraceLocator: public ::Url::Schema::TraceLocator
{
  public:
    explicit TraceLocator():
        Url::Schema::TraceLocator("strbo-trace-usb")
    {}
};

/*!
 * Representation of a USB simple location key.
 */
class LocationKeySimple: public ::Url::Location
{
  public:
    struct Components
    {
        std::string device_;
        std::string partition_;
        std::string path_;

        Components() {}

        explicit Components(const char *device, const char *partition,
                            const char *path):
            device_(device),
            partition_(partition),
            path_(path)
        {}
    };

  private:
    Components c_;
    bool is_partition_set_;
    bool is_path_set_;

  public:
    LocationKeySimple(const LocationKeySimple &) = delete;
    LocationKeySimple &operator=(const LocationKeySimple &) = delete;

    explicit LocationKeySimple():
        ::Url::Location(get_scheme()),
        is_partition_set_(false),
        is_path_set_(false)
    {}

    void clear() final override
    {
        c_.device_.clear();
        c_.partition_.clear();
        c_.path_.clear();
        is_partition_set_ = false,
        is_path_set_ = false;
    }

    bool is_valid() const final override
    {
        return is_partition_set_ && is_path_set_ && !c_.device_.empty();
    }

    void set_device(const std::string &device)
    {
        c_.device_ = device;
    }

    void set_device(std::string &&device)
    {
        c_.device_ = std::move(device);
    }

    void set_partition(const std::string &partition)
    {
        c_.partition_ = partition;
        is_partition_set_ = true;
    }

    void set_partition(std::string &&partition)
    {
        c_.partition_ = std::move(partition);
        is_partition_set_ = true;
    }

    void set_path(const std::string &path)
    {
        c_.path_ = path;
        is_path_set_ = true;
    }

    void set_path(std::string &&path)
    {
        c_.path_ = std::move(path);
        is_path_set_ = true;
    }

    void append_to_path(const std::string &path)
    {
        if(c_.path_.empty())
            set_path(path);
        else
        {
            c_.path_ += '/';
            c_.path_ += path;
        }
    }

    void append_to_path(const char *path)
    {
        if(c_.path_.empty())
            set_path(path);
        else
        {
            c_.path_ += '/';
            c_.path_ += path;
        }
    }

    const Components &unpack() const { return c_; }

  protected:
    std::string str_impl() const final override;
    bool set_url_impl(const std::string &url, size_t offset) final override;

  public:
    static const ::Url::Schema::StrBoLocator &get_scheme()
    {
        static const ::USB::ResourceLocatorSimple scheme;
        return scheme;
    }
};

/*!
 * Representation of a USB reference location key.
 */
class LocationKeyReference: public ::Url::Location
{
  public:
    struct Components
    {
        std::string device_;
        std::string partition_;
        std::string reference_point_;
        std::string item_name_;
        ID::RefPos item_position_;

        Components() {}

        explicit Components(const char *device, const char *partition,
                            const char *reference_point, const char *item_name,
                            ID::RefPos item_position):
            device_(device),
            partition_(partition),
            reference_point_(reference_point),
            item_name_(item_name),
            item_position_(item_position)
        {}
    };

  private:
    Components c_;
    bool is_partition_set_;
    bool is_reference_point_set_;
    bool is_item_set_;

  public:
    LocationKeyReference(const LocationKeyReference &) = delete;
    LocationKeyReference &operator=(const LocationKeyReference &) = delete;

    explicit LocationKeyReference():
        ::Url::Location(get_scheme()),
        is_partition_set_(false),
        is_reference_point_set_(false),
        is_item_set_(false)
    {}

    void clear() final override
    {
        c_.device_.clear();
        c_.partition_.clear();
        c_.reference_point_.clear();
        c_.item_name_.clear();
        c_.item_position_ = ID::RefPos();
        is_partition_set_ = false;
        is_reference_point_set_ = false;
        is_item_set_ = false;
    }

    bool is_valid() const final override
    {
        return is_partition_set_ && is_reference_point_set_ && is_item_set_ &&
               !c_.device_.empty() &&
               c_.item_name_.find('/') == std::string::npos;
    }

    void set_device(const std::string &device)
    {
        c_.device_ = device;
    }

    void set_device(std::string &&device)
    {
        c_.device_ = std::move(device);
    }

    void set_partition(const std::string &partition)
    {
        c_.partition_ = partition;
        is_partition_set_ = true;
    }

    void set_partition(std::string &&partition)
    {
        c_.partition_ = std::move(partition);
        is_partition_set_ = true;
    }

    void set_reference_point(const std::string &reference_point)
    {
        c_.reference_point_ = reference_point;
        is_reference_point_set_ = true;
    }

    void set_reference_point(std::string &&reference_point)
    {
        c_.reference_point_ = std::move(reference_point);
        is_reference_point_set_ = true;
    }

    void append_to_reference_point(const std::string &path)
    {
        if(c_.reference_point_.empty())
            set_reference_point(path);
        else
        {
            c_.reference_point_ += '/';
            c_.reference_point_ += path;
        }
    }

    void append_to_reference_point(const char *path)
    {
        if(c_.reference_point_.empty())
            set_reference_point(path);
        else
        {
            c_.reference_point_ += '/';
            c_.reference_point_ += path;
        }
    }

    void set_item(const std::string &item_name, ID::RefPos item_pos)
    {
        c_.item_name_ = item_name;
        c_.item_position_ = item_pos;
        is_item_set_ = true;
    }

    const Components &unpack() const { return c_; }

  protected:
    std::string str_impl() const final override;
    bool set_url_impl(const std::string &url, size_t offset) final override;

  public:
    static const ::Url::Schema::StrBoLocator &get_scheme()
    {
        static const ::USB::ResourceLocatorReference scheme;
        return scheme;
    }
};

/*!
 * Representation of a USB location trace.
 */
class LocationTrace: public ::Url::Location
{
  public:
    struct Components
    {
        std::string device_;
        std::string partition_;
        std::string reference_point_;
        std::string item_name_;
        ID::RefPos item_position_;

        Components() {}

        explicit Components(const char *device, const char *partition,
                            const char *reference_point, const char *item_name,
                            ID::RefPos item_position):
            device_(device),
            partition_(partition),
            reference_point_(reference_point),
            item_name_(item_name),
            item_position_(item_position)
        {}
    };

  private:
    Components c_;
    bool is_partition_set_;
    bool is_item_set_;

  public:
    LocationTrace(const LocationTrace &) = delete;
    LocationTrace &operator=(const LocationTrace &) = delete;

    explicit LocationTrace():
        ::Url::Location(get_scheme()),
        is_partition_set_(false),
        is_item_set_(false)
    {}

    void clear() final override
    {
        c_.device_.clear();
        c_.partition_.clear();
        c_.reference_point_.clear();
        c_.item_name_.clear();
        c_.item_position_ = ID::RefPos();
        is_partition_set_ = false;
        is_item_set_ = false;
    }

    bool is_valid() const final override
    {
        return is_partition_set_ && is_item_set_ && !c_.device_.empty();
    }

    size_t get_trace_length() const;

    void set_device(const std::string &device)
    {
        c_.device_ = device;
    }

    void set_device(std::string &&device)
    {
        c_.device_ = std::move(device);
    }

    void set_partition(const std::string &partition)
    {
        c_.partition_ = partition;
        is_partition_set_ = true;
    }

    void set_partition(std::string &&partition)
    {
        c_.partition_ = std::move(partition);
        is_partition_set_ = true;
    }

    void set_reference_point(const std::string &reference_point)
    {
        if(reference_point != "/")
            c_.reference_point_ = reference_point;
        else
            c_.reference_point_.clear();
    }

    void set_reference_point(std::string &&reference_point)
    {
        if(reference_point != "/")
            c_.reference_point_ = std::move(reference_point);
        else
            c_.reference_point_.clear();
    }

    void append_to_reference_point(const std::string &path)
    {
        if(c_.reference_point_.empty())
            set_reference_point(path);
        else
        {
            c_.reference_point_ += '/';
            c_.reference_point_ += path;
        }
    }

    void append_to_reference_point(const char *path)
    {
        if(c_.reference_point_.empty())
            set_reference_point(path);
        else
        {
            c_.reference_point_ += '/';
            c_.reference_point_ += path;
        }
    }

    void set_item(const std::string &item_name, ID::RefPos item_pos)
    {
        c_.item_name_ = item_name;
        c_.item_position_ = item_pos;
        is_item_set_ = true;
    }

    void append_item(const std::string &item_name, ID::RefPos item_pos)
    {
        if(is_item_set_)
            return;

        if(!c_.item_name_.empty())
            c_.item_name_ += '/';

        c_.item_name_ += item_name;
        c_.item_position_ = item_pos;
        is_item_set_ = true;
    }

    void append_to_item_path(const std::string &path)
    {
        if(is_item_set_)
            return;

        if(c_.item_name_.empty())
            c_.item_name_ = path;
        else
        {
            c_.item_name_ += '/';
            c_.item_name_ += path;
        }
    }

    void append_to_item_path(const char *path)
    {
        if(is_item_set_)
            return;

        if(c_.item_name_.empty())
            c_.item_name_ = path;
        else
        {
            c_.item_name_ += '/';
            c_.item_name_ += path;
        }
    }

    const Components &unpack() const { return c_; }

  protected:
    std::string str_impl() const final override;
    bool set_url_impl(const std::string &url, size_t offset) final override;

  public:
    static const ::Url::Schema::StrBoLocator &get_scheme()
    {
        static const ::USB::TraceLocator scheme;
        return scheme;
    }
};

}

#endif /* !STRBO_URL_USB_HH */

/*
 * Copyright (C) 2016, 2017, 2019  T+A elektroakustik GmbH & Co. KG
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

#ifndef URLSTRING_HH
#define URLSTRING_HH

#include <string>
#include <sstream>

#include "md5.hh"

namespace Url
{

enum class Sensitivity
{
    GENERIC,
    CONTAINS_SENSITIVE_DATA,
};

class String
{
  private:
    std::string url_;
    Sensitivity sensitivity_;

  public:
    String(const String &) = default;
    String(String &&) = default;
    String &operator=(const String &) = default;
    String &operator=(String &&) = default;

    explicit String(Sensitivity sensitivity):
        sensitivity_(sensitivity)
    {}

    explicit String(Sensitivity sensitivity, const std::string &url):
        url_(url),
        sensitivity_(sensitivity)
    {}

    explicit String(Sensitivity sensitivity, const char *const url):
        url_(url),
        sensitivity_(sensitivity)
    {}

    const std::string &get_cleartext() const { return url_; }

    /*!
     * Get URL for the purpose of putting it into the log.
     *
     * The outcome of this function depends on the URL sensitivity. For
     * #Url::Sensitivity::GENERIC, the URL is copied verbatim to \p dest. For
     * #Url::Sensitivity::CONTAINS_SENSITIVE_DATA, a concealed version of the
     * URL is written to \p dest. The concealed version will have the exact
     * same length as the original version.
     *
     * Note that the interface of this function has deliberately been designed
     * to be not easy to use. It could return a new \c std::string object for
     * ease of use, but then it would be too similar to
     * #Url::String::get_cleartext(). By design, code that calls this function
     * tends to be a bit noisy and stick out a bit, which is exactly how it
     * should be.
     */
    void get_for_logging(std::string &dest) const
    {
        switch(sensitivity_)
        {
          case Sensitivity::GENERIC:
            dest = url_;
            return;

          case Sensitivity::CONTAINS_SENSITIVE_DATA:
            copy_concealed(dest, url_);
            return;
        }

        /* should not be reached */
        dest.clear();
    }

    Sensitivity get_sensitivity() const { return sensitivity_; }

    void compute_hash(MD5::Hash &hash) const;

    void clear() { url_.clear(); }
    bool empty() const { return url_.empty(); }

    String &operator=(const std::string &src)
    {
        url_ = src;
        return *this;
    }

    template <typename T>
    String &operator+=(const T &src)
    {
        url_ += src;
        return *this;
    }

  private:
    static void copy_concealed(std::string &dest, const std::string &url);
};

class StreamLink
{
  public:
    const String url_;

    StreamLink(const StreamLink &) = delete;
    StreamLink(StreamLink &&) = default;
    StreamLink &operator=(const StreamLink &) = delete;

    explicit StreamLink(const std::string &url):
        url_(Sensitivity::GENERIC, url)
    {}

    explicit StreamLink(const char *const url):
        url_(Sensitivity::GENERIC, url)
    {}
};

class OStream
{
  private:
    std::ostringstream ostr_;
    Sensitivity sensitivity_;

  public:
    OStream(const OStream &) = delete;
    OStream &operator=(const OStream &) = delete;

    explicit OStream(Sensitivity sensitivity):
        sensitivity_(sensitivity)
    {}

    OStream &operator<<(const ::Url::String &src)
    {
        ostr_ << src.get_cleartext();

        switch(src.get_sensitivity())
        {
          case Sensitivity::GENERIC:
            break;

          case Sensitivity::CONTAINS_SENSITIVE_DATA:
            sensitivity_ = Sensitivity::CONTAINS_SENSITIVE_DATA;
            break;
        }

        return *this;
    }

    template <typename T>
    OStream &operator<<(const T &src)
    {
        ostr_ << src;
        return *this;
    }

    String str() const { return String(sensitivity_, ostr_.str()); }
};

}

#endif /* !URLSTRING_HH */

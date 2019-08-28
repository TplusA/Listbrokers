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

#ifndef I18NSTRING_HH
#define I18NSTRING_HH

#include <string>

namespace I18n
{

class String
{
  private:
    std::string string_;
    bool is_subject_to_translation_;

  public:
    String(const String &) = default;
    String(String &&) = default;
    String &operator=(const String &) = default;
    String &operator=(String &&) = default;

    explicit String(bool is_subject_to_translation):
        is_subject_to_translation_(is_subject_to_translation)
    {}

    explicit String(bool is_subject_to_translation, const std::string &str):
        string_(str),
        is_subject_to_translation_(is_subject_to_translation)
    {}

    explicit String(bool is_subject_to_translation, const char *const str):
        string_(str),
        is_subject_to_translation_(is_subject_to_translation)
    {}

    const std::string &get_text() const { return string_; }
    bool is_translatable() const { return is_subject_to_translation_; }

    String &operator=(const std::string &src)
    {
        string_ = src;
        return *this;
    }

    template <typename T>
    String &operator+=(const T &src)
    {
        string_ += src;
        return *this;
    }
};

}

#endif /* !I18NSTRING_HH */

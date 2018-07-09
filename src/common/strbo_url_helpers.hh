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

#ifndef STRBO_URL_HELPERS_HH
#define STRBO_URL_HELPERS_HH

#include <string>

#include "strbo_url.hh"
#include "de_tahifi_lists_errors.hh"

namespace Url
{

/*!
 * Try to handle URL, call function on success.
 *
 * \returns
 *     \c True if handled, \c false otherwise. A \c true return value does not
 *     imply success. Check the error code to distinguish success from failure.
 */
template <typename LocType>
static bool try_set_url_and_apply(const std::string &url, ListError &error,
                                  const std::function<ListError(const LocType &)> &apply)
{
    LocType loc;

    switch(loc.set_url(url))
    {
      case Url::Location::SetURLResult::OK:
        error = apply(loc);
        break;

      case Url::Location::SetURLResult::WRONG_SCHEME:
        error = ListError(ListError::NOT_SUPPORTED);
        return false;;

      case Url::Location::SetURLResult::INVALID_CHARACTERS:
      case Url::Location::SetURLResult::PARSING_ERROR:
        error = ListError(ListError::INVALID_STRBO_URL);
        break;
    }

    return true;
}

}

#endif /* !STRBO_URL_HELPERS_HH */

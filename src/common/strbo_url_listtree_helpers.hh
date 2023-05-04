/*
 * Copyright (C) 2023  T+A elektroakustik GmbH & Co. KG
 *
 * This file is part of T+A StrBo-URL.
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

#ifndef STRBO_URL_LISTTREE_HELPERS_HH
#define STRBO_URL_LISTTREE_HELPERS_HH

#include "de_tahifi_lists_errors.hh"
#include "strbo_url.hh"
#include "messages.h"

#include <string>
#include <functional>

namespace StrBoUrl
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

    try
    {
        loc.set_url(url);
        error = apply(loc);
    }
    catch(const StrBoUrl::Location::WrongSchemeError &e)
    {
        msg_error(0, LOG_NOTICE, "%s", e.what());
        error = ListError(ListError::NOT_SUPPORTED);
        return false;
    }
    catch(const StrBoUrl::Location::InvalidCharactersError &e)
    {
        msg_error(0, LOG_NOTICE, "%s", e.what());
        error = ListError(ListError::INVALID_STRBO_URL);
    }
    catch(const StrBoUrl::Location::ParsingError &e)
    {
        msg_error(0, LOG_NOTICE, "%s", e.what());
        error = ListError(ListError::INVALID_STRBO_URL);
    }

    return true;
}

}

#endif /* !STRBO_URL_LISTTREE_HELPERS_HH */

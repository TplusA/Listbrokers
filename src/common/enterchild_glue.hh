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

#ifndef ENTERCHILD_GLUE_HH
#define ENTERCHILD_GLUE_HH

#include <functional>

#include "idtypes.hh"

namespace EnterChild
{

using CheckUseCached = std::function<bool(ID::List id)>;
using SetNewRoot = std::function<void(ID::List old_id, ID::List new_id)>;
using DoPurgeList =
    std::function<ID::List(ID::List old_id, ID::List new_id, const SetNewRoot &set_root)>;

}

#endif /* !ENTERCHILD_GLUE_HH */

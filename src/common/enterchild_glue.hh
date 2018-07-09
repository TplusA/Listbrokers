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

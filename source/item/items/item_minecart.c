/*
    Copyright (c) 2023 ByteBit/xtreme8000

    This file is part of CavEX.

    CavEX is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    CavEX is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with CavEX.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "../../network/server_local.h"
#include "../../network/client_interface.h"
#include "../../network/server_interface.h"

static bool onItemPlace(struct server_local* s, struct item_data* it,
                        struct block_info* where, struct block_info* on,
                        enum side on_side)
{
    // only place on rails
    if (!on || on->block->type != BLOCK_RAIL) return false;

    // compute spawn position: center of rail
    vec3 pos = {
        on->x + 0.5f,
        on->y + 0.1f,
        on->z + 0.5f
    };

    // server side: spawn a minecart entity
    server_local_spawn_minecart(pos, s);

    return true;
}

struct item item_minecart = {
    .name         = "Minecart",
    .has_damage   = false,
    .max_stack    = 64, // should be 1
    .fuel         = 0,
    .renderItem   = render_item_flat,
    .onItemPlace  = onItemPlace,
    .armor.is_armor = false,
    .tool.type      = TOOL_TYPE_ANY,
    .render_data = {
        .item = {
            .texture_x = 7,
            .texture_y = 8,
        },
    },
};

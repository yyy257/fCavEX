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

static bool onItemPlace(struct server_local* s, struct item_data* it,
                        struct block_info* where, struct block_info* on,
                        enum side on_side) {
    if (!on || !on->block) return false;
	if (on_side != SIDE_TOP) return false;

    int tx = on->x;
    int ty = on->y + 1;
    int tz = on->z;
	
/*	struct block_data below;
	if (!server_world_get_block(&s->world, where->x, where->y - 1, where->z, &below))
		return false;
	if (!blocks[below.type] || blocks[below.type]->can_see_through)
		return false;
*/
    server_world_set_block(s, tx, ty, tz, (struct block_data) {
        .type = BLOCK_REDSTONE_WIRE,
        .metadata = 0,
        .sky_light = 0,
        .torch_light = 0,
    });

    return true;
}

struct item item_redstone = {
	.name = "Redstone",
	.has_damage = false,
	.max_stack = 64,
	.fuel = 0,
	.renderItem = render_item_flat,
	.onItemPlace = onItemPlace,
	.armor.is_armor = false,
	.tool.type = TOOL_TYPE_ANY,
	.render_data = {
		.item = {
			.texture_x = 8,
			.texture_y = 3,
		},
	},
};

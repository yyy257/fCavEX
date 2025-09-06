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
#include "../../block/blocks.h"

static bool onItemPlace(struct server_local* s, struct item_data* it,
						struct block_info* where, struct block_info* on,
						enum side on_side) {
	if (!on || !on->block) return false;

	// Check if the clicked block is flammable
	if (!blocks[on->block->type] || !blocks[on->block->type]->flammable)
		return false;

	int tx = on->x;
	int ty = on->y;
	int tz = on->z;

	// Determine the target block based on click side
	switch (on_side) {
		case SIDE_TOP:    ty += 1; break;
		case SIDE_BOTTOM: ty -= 1; break;
		case SIDE_FRONT:  tz += 1; break;
		case SIDE_BACK:   tz -= 1; break;
		case SIDE_LEFT:   tx -= 1; break;
		case SIDE_RIGHT:  tx += 1; break;
		default: break;
	}

	struct block_data blk;
	if (!server_world_get_block(&s->world, tx, ty, tz, &blk))
		return false;

	// Only if the target block is air
	if (blk.type != BLOCK_AIR)
		return false;

	// Place fire
	server_world_set_block(s, tx, ty, tz, (struct block_data) {
		.type = BLOCK_FIRE,
		.metadata = 0,
		.sky_light = 0,
		.torch_light = 15,
	});

	return false;
}

struct item item_flint_steel = {
	.name = "Flint and Steel",
	.has_damage = false,  // Disable damage (it won't be used up anymore)
	.max_damage = 0,      // Set max_damage to 0, so it doesn't get used up
	.max_stack = 1,
	.fuel = 0,
	.renderItem = render_item_flat,
	.onItemPlace = onItemPlace,
	.armor.is_armor = false,
	.tool.type = TOOL_TYPE_ANY,
	.render_data = {
		.item = {
			.texture_x = 5,
			.texture_y = 0,
		},
	},
};

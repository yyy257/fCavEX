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
	struct block_data blk;
	if(!server_world_get_block(&s->world, where->x, where->y - 1, where->z, &blk))
		return false;
	if(!server_world_get_block(&s->world, where->x, where->y + 1, where->z, &blk))
		return false;
	if(blk.type != 0) return false;
	if(!blocks[on->block->type]) return false;

	int metadata = 6;
	switch(on_side) {
		case SIDE_LEFT: metadata = 2; break;
		case SIDE_RIGHT: metadata = 0; break;
		case SIDE_BACK: metadata = 4; break;
		default: break;
	}

	struct block_data blk2 = (struct block_data) {
		.type = BLOCK_DOOR_IRON,
		.metadata = metadata,
		.sky_light = 0,
		.torch_light = 0,
	};

	struct block_info blk2_info = *where;
	blk2_info.block = &blk2;

	if(entity_local_player_block_collide(
		   (vec3) {s->player.x, s->player.y, s->player.z}, &blk2_info))
		return false;

	if(entity_local_player_block_collide(
		   (vec3) {s->player.x, s->player.y + 1, s->player.z}, &blk2_info))
		return false;

	server_world_set_block(s, where->x, where->y, where->z, blk2);

	blk2.metadata |= 8;
	server_world_set_block(s, where->x, where->y + 1, where->z, blk2);
	return true;
}

struct item item_door_iron = {
	.name = "Iron Door",
	.has_damage = false,
	.max_stack = 1,
	.fuel = 0,
	.renderItem = render_item_flat,
	.onItemPlace = onItemPlace,
	.armor.is_armor = false,
	.tool.type = TOOL_TYPE_ANY,
	.render_data = {
		.item = {
			.texture_x = 12,
			.texture_y = 2,
		},
	},
};

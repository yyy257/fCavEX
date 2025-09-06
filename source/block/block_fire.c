/*
	Copyright (c) 2022 ByteBit/xtreme8000

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

#include "blocks.h"
#include "../network/server_local.h"
#include "../network/server_world.h"


static enum block_material getMaterial(struct block_info* this) {
	return MATERIAL_STONE;
}

static size_t getBoundingBox(struct block_info* this, bool entity,
							 struct AABB* x) {
	return 0;
}

static struct face_occlusion*
getSideMask(struct block_info* this, enum side side, struct block_info* it) {
	return face_occlusion_empty();
}

static uint8_t getTextureIndex(struct block_info* this, enum side side) {
	return TEXTURE_INDEX(3, 0);
}

static size_t getDroppedItem(struct block_info* this, struct item_data* it,
							 struct random_gen* g, struct server_local* s) {
	return 0;
}


static void onRandomTick(struct server_local* s, struct block_info* blk) {
	// Check if fire should extinguish
	if (rand_gen(&s->rand_src) % 10 == 0) {
		// 10% chance to extinguish naturally
		server_world_set_block(s, blk->x, blk->y, blk->z,
							   (struct block_data){ .type = BLOCK_AIR });
		return;
	}

	// Define the 6 directions around the fire block
	const int dx[] = { 1, -1, 0, 0, 0, 0 };
	const int dy[] = { 0, 0, 1, -1, 0, 0 };
	const int dz[] = { 0, 0, 0, 0, 1, -1 };

	for (int i = 0; i < 6; i++) {
		int nx = blk->x + dx[i];
		int ny = blk->y + dy[i];
		int nz = blk->z + dz[i];

		struct block_data neighbor;
		if (server_world_get_block(&s->world, nx, ny, nz, &neighbor)) {
			// Check if the neighbor block is flammable
			if (blocks[neighbor.type] && blocks[neighbor.type]->flammable) {
				// Replace it with fire
				server_world_set_block(s, nx, ny, nz,
					(struct block_data){
						.type = BLOCK_FIRE,
						.metadata = 0,
						.sky_light = 0,
						.torch_light = 15,
					});

				// Optionally remove the original block (burn it away)
				server_world_set_block(s, nx, ny, nz,
					(struct block_data){
						.type = BLOCK_FIRE,
						.metadata = 0,
						.sky_light = 0,
						.torch_light = 15,
					});
			}
		}
	}
}


struct block block_fire = {
	.name = "Fire",
	.getSideMask = getSideMask,
	.getBoundingBox = getBoundingBox,
	.getMaterial = getMaterial,
	.getTextureIndex = getTextureIndex,
	.getDroppedItem = getDroppedItem,
	.onRandomTick = onRandomTick,
	.onRightClick = NULL,
	.transparent = true,
	.renderBlock = render_block_fire,
	.renderBlockAlways = NULL,
	.luminance = 15,
	.double_sided = false,
	.can_see_through = true,
	.opacity = 0,
	.ignore_lighting = false,
	.flammable = false,
	.place_ignore = true,
	.digging.hardness = 50,
	.digging.tool = TOOL_TYPE_ANY,
	.digging.min = TOOL_TIER_ANY,
	.digging.best = TOOL_TIER_ANY,
	.block_item = {
		.has_damage = false,
		.max_stack = 64,
		.renderItem = render_item_flat,
		.onItemPlace = block_place_default,
		.fuel = 0,
		.armor.is_armor = false,
		.tool.type = TOOL_TYPE_ANY,
	},
};

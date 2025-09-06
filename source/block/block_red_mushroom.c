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

#include "../network/server_local.h"
#include "blocks.h"

static enum block_material getMaterial(struct block_info* this) {
	return MATERIAL_ORGANIC;
}

static size_t getBoundingBox(struct block_info* this, bool entity,
							 struct AABB* x) {
	if(x)
		aabb_setsize(x, 0.375F, 0.375F, 0.375F);
	return entity ? 0 : 1;
}

static struct face_occlusion*
getSideMask(struct block_info* this, enum side side, struct block_info* it) {
	return face_occlusion_empty();
}

static uint8_t getTextureIndex(struct block_info* this, enum side side) {
	return tex_atlas_lookup(TEXAT_MUSHROOM_RED);
}

static bool onItemPlace(struct server_local* s, struct item_data* it,
						struct block_info* where, struct block_info* on,
						enum side on_side) {
	struct block_data blk;
	if (!server_world_get_block(&s->world, where->x, where->y - 1, where->z, &blk))
		return false;

	// Check if the block is suitable for planting mushrooms (grass or dirt)
	if (blk.type != BLOCK_GRASS && blk.type != BLOCK_DIRT)
		return false;

	// Allow placing the mushroom
	return block_place_default(s, it, where, on, on_side);
}

static void onRandomTick(struct server_local* s, struct block_info* blk) {
	if (blk->block->sky_light >= 13 || blk->block->torch_light >= 13)
		return;

	// Count surrounding mushrooms
	int count = 0;
	for (int dx = -4; dx <= 4; dx++) {
		for (int dy = -1; dy <= 1; dy++) {
			for (int dz = -4; dz <= 4; dz++) {
				struct block_data nearby;
				if (server_world_get_block(&s->world, blk->x + dx, blk->y + dy, blk->z + dz, &nearby)) {
					if (nearby.type == blk->block->type)
						count++;
				}
			}
		}
	}
	if (count > 5) return;

	// Random chance for red vs brown mushroom spawn (brown mushroom has 8x more chance)
	int mushroomType = (rand() % 9); // 0-8, 0 means red mushroom, 1-8 means brown mushroom

	// Choose random neighboring block to spread
	int ox = blk->x + (rand() % 3) - 1;
	int oy = blk->y + (rand() % 2) - rand() % 2;
	int oz = blk->z + (rand() % 3) - 1;

	// Check if the chosen block is air
	struct block_data dest;
	if (!server_world_get_block(&s->world, ox, oy, oz, &dest)) return;
	if (dest.type != BLOCK_AIR) return;

	// Check the block below to see if it is dirt, grass
	struct block_data below;
	if (server_world_get_block(&s->world, ox, oy - 1, oz, &below)) {
		// Ensure the block below is suitable for mushrooms to grow on (dirt, grass)
		if (below.type == BLOCK_DIRT || below.type == BLOCK_GRASS) {
			// Ensure the block above is not transparent
			if (!blocks[below.type] || blocks[below.type]->can_see_through) {
				// Don't allow mushroom growth if the block is transparent (like glass or leaves)
				return;
			}

			if (mushroomType == 0) {
				// 1 in 9 chance on red mushroom
				server_world_set_block(s, ox, oy, oz, (struct block_data){
					.type = BLOCK_RED_MUSHROOM,
					.metadata = 0
				});
			} else {
				// 8 in 9 kans on brown mushroom
				server_world_set_block(s, ox, oy, oz, (struct block_data){
					.type = BLOCK_BROWM_MUSHROOM,
					.metadata = 0
				});
			}

		}
	}
}



struct block block_red_mushroom = {
	.name = "Red mushroom",
	.getSideMask = getSideMask,
	.getBoundingBox = getBoundingBox,
	.getMaterial = getMaterial,
	.getTextureIndex = getTextureIndex,
	.getDroppedItem = block_drop_default,
	.onRandomTick = onRandomTick,
	.onRightClick = NULL,
	.transparent = false,
	.renderBlock = render_block_cross,
	.renderBlockAlways = NULL,
	.luminance = 0,
	.double_sided = true,
	.can_see_through = true,
	.opacity = 0,
	.render_block_data.cross_random_displacement = false,
	.ignore_lighting = false,
	.flammable = false,
	.place_ignore = false,
	.digging.hardness = 50,
	.digging.tool = TOOL_TYPE_ANY,
	.digging.min = TOOL_TIER_ANY,
	.digging.best = TOOL_TIER_ANY,
	.block_item = {
		.has_damage = false,
		.max_stack = 64,
		.renderItem = render_item_flat,
		.onItemPlace = onItemPlace,
		.fuel = 0,
		.armor.is_armor = false,
		.tool.type = TOOL_TYPE_ANY,
	},
};

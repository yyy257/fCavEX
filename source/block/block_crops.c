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
	return MATERIAL_ORGANIC;
}

static size_t getBoundingBox(struct block_info* this, bool entity,
							 struct AABB* x) {
	if(x)
		aabb_setsize(x, 1.0F, 0.25F, 1.0F);
	return entity ? 0 : 1;
}

static struct face_occlusion*
getSideMask(struct block_info* this, enum side side, struct block_info* it) {
	return face_occlusion_empty();
}

static uint8_t getTextureIndex(struct block_info* this, enum side side) {
	switch(this->block->metadata) {
		case 0: return tex_atlas_lookup(TEXAT_CROPS_0);
		case 1: return tex_atlas_lookup(TEXAT_CROPS_1);
		case 2: return tex_atlas_lookup(TEXAT_CROPS_2);
		case 3: return tex_atlas_lookup(TEXAT_CROPS_3);
		case 4: return tex_atlas_lookup(TEXAT_CROPS_4);
		case 5: return tex_atlas_lookup(TEXAT_CROPS_5);
		case 6: return tex_atlas_lookup(TEXAT_CROPS_6);
		default:
		case 7: return tex_atlas_lookup(TEXAT_CROPS_7);
	}
}

static size_t getDroppedItem(struct block_info* this, struct item_data* it,
							 struct random_gen* g, struct server_local* s) {
	bool has_seeds = this->block->metadata >= 5;
	bool has_wheat = this->block->metadata == 7;

	if(it && has_seeds) {
		it[0].id = ITEM_SEED;
		it[0].durability = 0;
		it[0].count = rand_gen_range(g, 1, 3);

		if(has_wheat) {
			it[1].id = ITEM_WHEAT;
			it[1].durability = 0;
			it[1].count = 1;
		}
	}

	return (has_seeds ? (has_wheat ? 2 : 1) : 0);
}

static void onRandomTick(struct server_local* s, struct block_info* blk) {
    // check if there's enough light to grow.
    if (blk->block->sky_light < 9 && blk->block->torch_light < 9)
        return;

    // get the current crop growt stage
    uint8_t stage = blk->block->metadata;

    // check if it's grown or not
    if (stage < 7) {
        // add one as long as it's not fully grown (7)
        blk->block->metadata = stage + 1;
        
        // update the block
        server_world_set_block(s, blk->x, blk->y, blk->z, *blk->block);
    }
}

struct block block_crops = {
	.name = "Crops",
	.getSideMask = getSideMask,
	.getBoundingBox = getBoundingBox,
	.getMaterial = getMaterial,
	.getTextureIndex = getTextureIndex,
	.getDroppedItem = getDroppedItem,
	.onRandomTick = onRandomTick,
	.onRightClick = NULL,
	.transparent = false,
	.renderBlock = render_block_crops,
	.renderBlockAlways = NULL,
	.luminance = 0,
	.double_sided = true,
	.can_see_through = true,
	.opacity = 0,
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
		.onItemPlace = block_place_default,
		.fuel = 0,
		.armor.is_armor = false,
		.tool.type = TOOL_TYPE_ANY,
	},
};

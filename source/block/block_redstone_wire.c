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
#include "../particle.h"
#include "../game/game_state.h"


static enum block_material getMaterial(struct block_info* this) {
	return MATERIAL_STONE;
}

static size_t getBoundingBox(struct block_info* this, bool entity,
                             struct AABB* x) {
    if (x) {
        aabb_setsize(x,
                     1.0F,
                     0.125F,
                     1.0F);
    }
    return entity ? 0 : 1;
}

static struct face_occlusion*
getSideMask(struct block_info* this, enum side side, struct block_info* it) {
	return face_occlusion_empty();
}

static uint8_t getTextureIndex(struct block_info* this, enum side side) {
	uint8_t lvl = this->block->metadata & 0x0F;
	if (lvl == 0) {
		return tex_atlas_lookup(TEXAT_REDSTONE_WIRE_OFF);
	}
	return tex_atlas_lookup(TEXAT_REDSTONE_WIRE_L1 + (lvl - 1));
}

static void onWorldTick(struct server_local* s, struct block_info* blk) {
    if (!blk->neighbours) return;
    struct block_data cur = *blk->block;
    if (cur.type != BLOCK_REDSTONE_WIRE) return;

    uint8_t strong = 0;
    for (int side = 0; side < SIDE_MAX; ++side) {
        if (!blk->neighbours) continue;
        uint8_t ntype = blk->neighbours[side].type;
        uint8_t nmeta = blk->neighbours[side].metadata & 0x0F;

        // redstone torch always powers at level 15
        if (ntype == BLOCK_REDSTONE_TORCH ||
            ntype == BLOCK_REDSTONE_TORCH_LIT ){
            strong = 15;
            break;
        }
        // stone or wooden pressure plate, pressed when metadata == 1
        if ((ntype == BLOCK_STONE_PRESSURE_PLATE ||
             ntype == BLOCK_WOOD_PRESSURE_PLATE) &&
             nmeta == 1) {
            strong = 15;
            break;
        }
    }

    const enum side horiz[4] = {
            SIDE_RIGHT,  // +X
            SIDE_LEFT,   // -X
            SIDE_FRONT,  // +Z
            SIDE_BACK    // -Z
    };
    uint8_t maxWire = 0;
    for (int i = 0; i < 4; ++i) {
        if (blk->neighbours
            && blk->neighbours[horiz[i]].type == 55)
        {
            uint8_t p = blk->neighbours[horiz[i]].metadata & 0x0F;
            if (p > maxWire) maxWire = p;
        }
    }

    uint8_t desired = strong
                     ? 15
                     : (maxWire ? maxWire - 1 : 0);

    vec3 c = { blk->x + 0.5f, blk->y, blk->z + 0.5f };
    particle_generate_redstone_wire(c, desired);

    if ((cur.metadata & 0x0F) != desired) {
        server_world_set_block(s,
                               blk->x, blk->y, blk->z,
                               (struct block_data){
                                   .type     = BLOCK_REDSTONE_WIRE,
                                   .metadata = desired
                               });
        notifyNeighbours(s, blk->x, blk->y, blk->z);

    }
}

static size_t getDroppedItem(struct block_info* this,
                             struct item_data* it,
                             struct random_gen* g,
                             struct server_local* s)
{
    if (it) {
        it->id = ITEM_REDSTONE;
        it->durability = 0;
        it->count = 1;
    }
    return 1;
}

struct block block_redstone_wire = {
	.name = "Redstone wire",
	.getSideMask = getSideMask,
	.getBoundingBox = getBoundingBox,
	.getMaterial = getMaterial,
	.getTextureIndex = getTextureIndex,
	.getDroppedItem = getDroppedItem,
	.onRandomTick = NULL,
	.onRightClick = NULL,
	.onWorldTick = onWorldTick,
	.transparent = false,
	.renderBlock = render_block_redstone_wire,
	.renderBlockAlways = NULL,
	.luminance = 0,
	.double_sided = true,
	.can_see_through = true,
	.opacity = 0,
	.render_block_data.rail_curved_possible = true,
	.ignore_lighting = false,
	.flammable = false,
	.place_ignore = false,
	.digging.hardness = 1050,
	.digging.tool = TOOL_TYPE_ANY,
	.digging.min = TOOL_TIER_ANY,
	.digging.best = TOOL_TIER_ANY,
	.block_item = {
		.has_damage = false,
		.max_stack = 64,
		.renderItem = render_item_flat,
		.onItemPlace = block_place_default,
		.armor.is_armor = false,
		.tool.type = TOOL_TYPE_ANY,
	},
};

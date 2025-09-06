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

//todo: fix trapdoor logic, as it currently isn't opening nicely..
// this is caused by dual opening function: open manually or toggle open/close status by redstone.
// best would be to add something that looks at the neighbour state change
//todo; fix rendering of the door in various metadata positions

#include "../network/server_local.h"
#include "blocks.h"

static enum block_material getMaterial(struct block_info* this) {
	return MATERIAL_WOOD;
}

static size_t getBoundingBox(struct block_info* this, bool entity,
							 struct AABB* x) {
	if(x) {
		if(this->block->metadata & 0x04) {
			switch(this->block->metadata & 0x03) {
				case 0:
					aabb_setsize(x, 1.0F, 1.0F, 0.1875F);
					aabb_translate(x, 0, 0, 0.40625F);
					break;
				case 1:
					aabb_setsize(x, 1.0F, 1.0F, 0.1875F);
					aabb_translate(x, 0, 0, -0.40625F);
					break;
				case 2:
					aabb_setsize(x, 0.1875F, 1.0F, 1.0F);
					aabb_translate(x, 0.40625F, 0, 0);
					break;
				case 3:
					aabb_setsize(x, 0.1875F, 1.0F, 1.0F);
					aabb_translate(x, -0.40625F, 0, 0);
					break;
			}
		} else {
			aabb_setsize(x, 1.0F, 0.1875F, 1.0F);
		}
	}

	return 1;
}

static struct face_occlusion*
getSideMask(struct block_info* this, enum side side, struct block_info* it) {
	if(this->block->metadata & 0x04) {
		// could be improved
		return face_occlusion_empty();
	} else {
		switch(side) {
			case SIDE_TOP: return face_occlusion_empty();
			case SIDE_BOTTOM:
				return (it->block->type == this->block->type) ?
					face_occlusion_full() :
					face_occlusion_empty();
			default: return face_occlusion_rect(3);
		}
	}
}

static uint8_t getTextureIndex(struct block_info* this, enum side side) {
	return tex_atlas_lookup(TEXAT_TRAPDOOR);
}

static bool onItemPlace(struct server_local* s, struct item_data* it,
						struct block_info* where, struct block_info* on,
						enum side on_side) {
	if(!blocks[on->block->type])
		return false;

	int metadata = 0;
	switch(on_side) {
		case SIDE_LEFT: metadata = 2; break;
		case SIDE_RIGHT: metadata = 3; break;
		case SIDE_BACK: metadata = 1; break;
		default: break;
	}

	struct block_data blk = (struct block_data) {
		.type = it->id,
		.metadata = metadata,
		.sky_light = 0,
		.torch_light = 0,
	};

	struct block_info blk_info = *where;
	blk_info.block = &blk;

	if(entity_local_player_block_collide(
		   (vec3) {s->player.x, s->player.y, s->player.z}, &blk_info))
		return false;

	server_world_set_block(s, where->x, where->y, where->z, blk);
	return true;
}


static void onNeighbourBlockChange(struct server_local* s, struct block_info* info) {
    struct block_data cur = *info->block;
    if (!info->neighbours) return;

    bool powered = false;
    for (int side = 0; side < SIDE_MAX; ++side) {
        struct block_data nb = info->neighbours[side];
        uint8_t m = nb.metadata & 0x0F;
        if ((nb.type == BLOCK_REDSTONE_WIRE && m > 0) ||
            nb.type == BLOCK_REDSTONE_TORCH_LIT ||
           ((nb.type == BLOCK_STONE_PRESSURE_PLATE ||
             nb.type == BLOCK_WOOD_PRESSURE_PLATE) &&
            (m & 0x01))) {
            powered = true;
            break;
        }
    }

    uint8_t facing = cur.metadata & 0x03;
    uint8_t newMeta = facing | (powered ? 0x04 : 0x00);

    if (newMeta != cur.metadata) {
        cur.metadata = newMeta;
        server_world_set_block(s,
                               info->x, info->y, info->z,
                               cur);
        // <-- trigger alleen bij echte state-change
        notifyNeighbours(s, info->x, info->y, info->z);
    }
}


static void onRightClick(struct server_local* s, struct item_data* it,
                         struct block_info* where, struct block_info* on,
                         enum side on_side) {
    struct block_data cur = *on->block;
    cur.metadata ^= 0x04;
    server_world_set_block(s,
                           on->x, on->y, on->z,
                           cur);
}

struct block block_trapdoor = {
	.name = "Trapdoor",
	.getSideMask = getSideMask,
	.getBoundingBox = getBoundingBox,
	.getMaterial = getMaterial,
	.getTextureIndex = getTextureIndex,
	.getDroppedItem = block_drop_default,
	.onNeighbourBlockChange = onNeighbourBlockChange,
	.onRandomTick = NULL,
	.onRightClick = onRightClick,
	.transparent = false,
	.renderBlock = render_block_trapdoor,
	.renderBlockAlways = NULL,
	.luminance = 0,
	.double_sided = false,
	.can_see_through = true,
	.opacity = 1,
	.ignore_lighting = false,
	.flammable = false,
	.place_ignore = false,
	.digging.hardness = 4500, // TODO: might not be correct
	.digging.tool = TOOL_TYPE_ANY,
	.digging.min = TOOL_TIER_ANY,
	.digging.best = TOOL_TIER_ANY,
	.block_item = {
		.has_damage = false,
		.max_stack = 64,
		.renderItem = render_item_block,
		.onItemPlace = onItemPlace,
		.fuel = 1,
		.render_data.block.has_default = false,
		.armor.is_armor = false,
		.tool.type = TOOL_TYPE_ANY,

	},
};

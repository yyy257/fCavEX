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

#include "../network/client_interface.h"
#include "../network/inventory_logic.h"
#include "../network/server_local.h"
#include "blocks.h"

static enum block_material getMaterial(struct block_info* this) {
	return MATERIAL_WOOD;
}

static size_t getBoundingBox(struct block_info* this, bool entity,
							 struct AABB* x) {
	if(x)
		aabb_setsize(x, 1.0F, 1.0F, 1.0F);
	return 1;
}

static struct face_occlusion*
getSideMask(struct block_info* this, enum side side, struct block_info* it) {
	return face_occlusion_full();
}

static uint8_t getTextureIndex(struct block_info* this, enum side side) {
	uint8_t tex[SIDE_MAX] = {
		[SIDE_TOP] = tex_atlas_lookup(TEXAT_CAST_BLOCK_IRON),
		[SIDE_BOTTOM] = tex_atlas_lookup(TEXAT_CAST_BLOCK_IRON),
		[SIDE_BACK] = tex_atlas_lookup(TEXAT_CHEST_SIDE),
		[SIDE_FRONT] = tex_atlas_lookup(TEXAT_CHEST_SIDE),
		[SIDE_LEFT] = tex_atlas_lookup(TEXAT_CHEST_SIDE),
		[SIDE_RIGHT] = tex_atlas_lookup(TEXAT_CHEST_SIDE),
	};

	switch(this->block->metadata) {
		case 0: tex[SIDE_FRONT] = tex_atlas_lookup(TEXAT_CHEST_FRONT_SINGLE); break;
		case 1: tex[SIDE_RIGHT] = tex_atlas_lookup(TEXAT_CHEST_FRONT_SINGLE); break;
		case 2: tex[SIDE_BACK] = tex_atlas_lookup(TEXAT_CHEST_FRONT_SINGLE); break;
		case 3: tex[SIDE_LEFT] = tex_atlas_lookup(TEXAT_CHEST_FRONT_SINGLE); break;
	}

 	return tex[side];
}

static void onRightClick(struct server_local* s, struct item_data* it,
						 struct block_info* where, struct block_info* on,
						 enum side on_side) {
	if(s->player.active_inventory == &s->player.inventory) {
		clin_rpc_send(&(struct client_rpc) {
			.type = CRPC_OPEN_WINDOW,
			.payload.window_open.window = WINDOWC_IRON_CHEST,
			.payload.window_open.type = WINDOW_TYPE_IRON_CHEST,
			.payload.window_open.slot_count = IRON_CHEST_SIZE,
		});

		struct inventory* inv = malloc(sizeof(struct inventory));
		inventory_create(inv, &inventory_logic_iron_chest, s, IRON_CHEST_SIZE, on->x, on->y, on->z);
		s->player.active_inventory = inv;
	}
}

static bool onItemPlace(struct server_local* s, struct item_data* it,
						struct block_info* where, struct block_info* on,
						enum side on_side) {
	struct block_data blk = (struct block_data) {
		.type = it->id,
		.metadata = it->durability,
		.sky_light = 0,
		.torch_light = 0,
	};

	struct block_info blk_info = *where;
	blk_info.block = &blk;

	if(entity_local_player_block_collide(
		   (vec3) {s->player.x, s->player.y, s->player.z}, &blk_info))
		return false;

	for (int i=0; i<MAX_CHESTS; i++) {
		if (s->chest_pos[i].y < 0) {
			s->chest_pos[i].x = where->x;
			s->chest_pos[i].y = where->y;
			s->chest_pos[i].z = where->z;
			memset(&s->chest_items[i], 0, 54*sizeof(struct item_data));
			server_world_set_block(s, where->x, where->y, where->z, blk);
			return true;
		}
	}

	puts("Too many chests");
	return false;
}

static size_t getDroppedItem(struct block_info* this, struct item_data* it,
							 struct random_gen* g, struct server_local* s) {
	if(it) {
		it->id = this->block->type;
		it->durability = this->block->metadata;
		it->count = 1;
	} else { //only free chest on first run of getDroppedItem
		for(int i=0; i<MAX_CHESTS; i++) {
			if(s->chest_pos[i].x == this->x && s->chest_pos[i].y == this->y && s->chest_pos[i].z == this->z) {
				for(int j=0; j<MAX_CHEST_SLOTS; j++) {
					if(s->chest_items[i][j].id) {
						server_local_spawn_item((vec3) {this->x + 0.5F,
									this->y + 0.5F,
									this->z + 0.5F},
									&s->chest_items[i][j], false, s);
					}
				}
				s->chest_pos[i].y = -1;
				break;
			}
		}
	}

	return 1;
}

struct block block_iron_chest = {
	.name = "Iron Chest",
	.getSideMask = getSideMask,
	.getBoundingBox = getBoundingBox,
	.getMaterial = getMaterial,
	.getTextureIndex = getTextureIndex,
	.getDroppedItem = getDroppedItem,
	.onRandomTick = NULL,
	.onRightClick = onRightClick,
	.transparent = false,
	.renderBlock = render_block_full,
	.renderBlockAlways = NULL,
	.luminance = 0,
	.double_sided = false,
	.can_see_through = false,
	.ignore_lighting = false,
	.flammable = false,
	.place_ignore = false,
	.digging.hardness = 3750,
	.digging.tool = TOOL_TYPE_PICKAXE,
	.digging.min = TOOL_TIER_WOOD,
	.digging.best = TOOL_TIER_MAX,
	.block_item = {
		.has_damage = false,
		.max_stack = 64,
		.renderItem = render_item_block,
		.onItemPlace = onItemPlace,
		.fuel = 0,
		.render_data.block.has_default = true,
		.render_data.block.default_metadata = 0,
		.render_data.block.default_rotation = 2,
		.armor.is_armor = false,
		.tool.type = TOOL_TYPE_ANY,
	},
};


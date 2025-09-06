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

//todo: fix door logic, as it currently isn't opening nicely..
// this is caused by dual opening function: open manually or toggle open/close status by redstone.
// best would be to add something that looks at the neighbour state change
//todo; fix rendering of the door in various metadata positions


#include "../network/server_local.h"
#include "blocks.h"

static enum block_material getMaterial1(struct block_info* this) {
	return MATERIAL_WOOD;
}

static enum block_material getMaterial2(struct block_info* this) {
	return MATERIAL_STONE;
}

static size_t getBoundingBox(struct block_info* this, bool entity,
							 struct AABB* x) {
	if(x) {
		uint8_t state = ((this->block->metadata & 0x03)
						 + ((this->block->metadata & 0x04) ? 1 : 0))
			% 4;
		switch(state) {
			case 0:
				aabb_setsize(x, 0.1875F, 1.0F, 1.0F);
				aabb_translate(x, -0.40625F, 0, 0);
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
				aabb_setsize(x, 1.0F, 1.0F, 0.1875F);
				aabb_translate(x, 0, 0, 0.40625F);
				break;
		}
	}

	return 1;
}

static struct face_occlusion*
getSideMask(struct block_info* this, enum side side, struct block_info* it) {
	// could be improved
	return face_occlusion_empty();
}

static uint8_t getTextureIndex1(struct block_info* this, enum side side) {
	return (this->block->metadata & 0x08) ?
		tex_atlas_lookup(TEXAT_DOOR_WOOD_TOP) :
		tex_atlas_lookup(TEXAT_DOOR_WOOD_BOTTOM);
}

static uint8_t getTextureIndex2(struct block_info* this, enum side side) {
	return (this->block->metadata & 0x08) ?
		tex_atlas_lookup(TEXAT_DOOR_IRON_TOP) :
		tex_atlas_lookup(TEXAT_DOOR_IRON_BOTTOM);
}

static size_t getDroppedItem(struct block_info* this, struct item_data* it,
							 struct random_gen* g, struct server_local* s) {
	if(it) {
		it->id = ITEM_DOOR_WOOD;
		it->durability = 0;
		it->count = 1;
	}

	//only drop item from bottom half
	if(this->block->metadata < 8) return 1;
	return 0;
}

static size_t getDroppedItem2(struct block_info* this, struct item_data* it,
							 struct random_gen* g, struct server_local* s) {
	if(it) {
		it->id = ITEM_DOOR_IRON;
		it->durability = 0;
		it->count = 1;
	}

	//only drop item from bottom half
	if(this->block->metadata < 8) return 1;
	return 0;
}

static void toggleDoor(struct server_local* s,
                       w_coord_t x, w_coord_t y, w_coord_t z,
                       uint8_t doorType)
{
    struct block_data bd;
    if (!server_world_get_block(&s->world, x, y, z, &bd)) return;

    // flip the manual-open bit, preserve redstone bit
    uint8_t manual = bd.metadata & 0x01;
    uint8_t newMeta = (bd.metadata & ~0x01) | (manual ^ 0x01);
    newMeta |= bd.metadata & 0x04;

    // ——— NUDGE PLAYER IF THE DOOR SWINGS INTO THEM ———
    // handle both opening (0→1) and closing (1→0)
    if ((bd.metadata & 0x01) != (newMeta & 0x01)) {
        struct block_data test = { .type = doorType, .metadata = newMeta };
        struct block_info di = {
            .x          = x,
            .y          = y,
            .z          = z,
            .block      = &test,
            .neighbours = NULL
        };
        if (entity_local_player_block_collide(
                (vec3){s->player.x, s->player.y, s->player.z}, &di))
        {
            double dx = s->player.x - (x + 0.5);
            double dz = s->player.z - (z + 0.5);
            if (fabs(dx) > fabs(dz)) {
                // push out along X
                double push = (dx > 0.0) ? +0.6 : -0.6;
                s->player.x = x + 0.5 + push;
            } else {
                // push out along Z
                double push = (dz > 0.0) ? +0.6 : -0.6;
                s->player.z = z + 0.5 + push;
            }
        }
    }
    // ————————————————————————————————————————————

    // now actually flip the door blocks
    server_world_set_block(s, x,   y,   z,
        (struct block_data){ .type = doorType, .metadata = newMeta });
    server_world_set_block(s, x, y+1, z,
        (struct block_data){ .type = doorType, .metadata = newMeta | 0x08 });
}

static void onRightClick(struct server_local* s,
                              struct item_data* it,
                              struct block_info* where,
                              struct block_info* on,
                              enum side on_side)
{
    struct block_data cur = *on->block;
    bool topHalf = (cur.metadata & 0x08) != 0;
    w_coord_t bx = on->x;
    w_coord_t by = on->y - (topHalf ? 1 : 0);
    w_coord_t bz = on->z;

    toggleDoor(s, bx, by, bz, cur.type);
}


static void onNeighbourBlockChange(struct server_local* s,
                                   struct block_info* info)
{
    struct block_data cur = *info->block;
    // alleen bottom half
    if (cur.metadata & 0x08) return;

    // bepaal of we nu power hebben
    const int dx[6] = {  1, -1,  0,  0,  0,  0 };
    const int dy[6] = {  0,  0,  0,  0,  1, -1 };
    const int dz[6] = {  0,  0,  1, -1,  0,  0 };
    bool powered = false;

    for (int i = 0; i < 6; i++) {
        w_coord_t nx = info->x + dx[i];
        w_coord_t ny = info->y + dy[i];
        w_coord_t nz = info->z + dz[i];

        struct block_data nb;
        if (!server_world_get_block(&s->world, nx, ny, nz, &nb))
            continue;

        uint8_t m = nb.metadata & 0x0F;
        if ((nb.type == BLOCK_REDSTONE_WIRE && m > 0) ||
            nb.type == BLOCK_REDSTONE_TORCH_LIT            ||
           ((nb.type == BLOCK_STONE_PRESSURE_PLATE         ||
             nb.type == BLOCK_WOOD_PRESSURE_PLATE)         &&
            (m & 0x01)))
        {
            powered = true;
            break;
        }
    }

    // extraheren van de hand-bit (bit 0)
    uint8_t handBit = cur.metadata & 0x01;
    // nieuw meta = handBit plus (powered ? redstone-bit : 0)
    uint8_t newMeta = handBit | (powered ? 0x04 : 0x00);

    // wijzig alleen als het verschil is
    if ((cur.metadata & 0x05) != newMeta) {
        // onderste helft
        server_world_set_block(s, info->x, info->y,   info->z,
            (struct block_data){ .type = cur.type, .metadata = newMeta });
        // bovenste helft: zet top-flag (0x08) wél altijd als het oorspronkelijk top-flag had
        server_world_set_block(s, info->x, info->y+1, info->z,
            (struct block_data){ .type = cur.type, .metadata = newMeta | 0x08 });
    }
}



struct block block_wooden_door = {
	.name = "Wooden Door",
	.getSideMask = getSideMask,
	.getBoundingBox = getBoundingBox,
	.getMaterial = getMaterial1,
	.getTextureIndex = getTextureIndex1,
	.getDroppedItem = getDroppedItem,
	.onRandomTick = NULL,
	.onWorldTick = NULL,
    .onNeighbourBlockChange  = onNeighbourBlockChange,
	.onRightClick = onRightClick,
	.transparent = false,
	.renderBlock = render_block_door,
	.renderBlockAlways = NULL,
	.luminance = 0,
	.double_sided = false,
	.can_see_through = true,
	.opacity = 1,
	.ignore_lighting = false,
	.flammable = false,
	.place_ignore = false,
	.digging.hardness = 4500,
	.digging.tool = TOOL_TYPE_ANY,
	.digging.min = TOOL_TIER_ANY,
	.digging.best = TOOL_TIER_ANY,
	.block_item = {
		.has_damage = false,
		.max_stack = 1,
		.renderItem = render_item_flat,
		.onItemPlace = block_place_default,
		.fuel = 0,
		.armor.is_armor = false,
		.tool.type = TOOL_TYPE_ANY,
	},
};

struct block block_iron_door = {
	.name = "Iron Door",
	.getSideMask = getSideMask,
	.getBoundingBox = getBoundingBox,
	.getMaterial = getMaterial2,
	.getTextureIndex = getTextureIndex2,
	.getDroppedItem = getDroppedItem2,
	.onRandomTick = NULL,
    .onNeighbourBlockChange  = onNeighbourBlockChange,
	.onRightClick = onRightClick,
	.transparent = false,
	.renderBlock = render_block_door,
	.renderBlockAlways = NULL,
	.luminance = 0,
	.double_sided = false,
	.can_see_through = true,
	.opacity = 1,
	.ignore_lighting = false,
	.flammable = false,
	.place_ignore = false,
	.digging.hardness = 7500,
	.digging.tool = TOOL_TYPE_PICKAXE,
	.digging.min = TOOL_TIER_WOOD,
	.digging.best = TOOL_TIER_WOOD,
	.block_item = {
		.has_damage = false,
		.max_stack = 1,
		.renderItem = render_item_flat,
		.onItemPlace = block_place_default,
		.fuel = 0,
		.armor.is_armor = false,
		.tool.type = TOOL_TYPE_ANY,
	},
};

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
	return MATERIAL_STONE;
}

static size_t getBoundingBox(struct block_info* this, bool entity,
							 struct AABB* x) {
	if(x)
		aabb_setsize(x, 1.0F,
					 ((this->block->metadata & 0x7) > 1
					  && (this->block->metadata & 0x7) < 6) ?
						 0.625F :
						 0.125F,
					 1.0F);
	return entity ? 0 : 1;
}

static inline bool isRail(uint8_t type) {
    return type == BLOCK_RAIL
        || type == BLOCK_POWERED_RAIL
        || type == BLOCK_DETECTOR_RAIL;
}

static void updateRailToSlope(struct server_local* s, w_coord_t x, w_coord_t y, w_coord_t z, uint8_t newMeta) {
    struct block_data bd;
    if (!server_world_get_block(&s->world, x, y, z, &bd) || !isRail(bd.type)) {
        return;
    }
    uint8_t oldMeta = bd.metadata & 0xF;
    if (oldMeta >= 6 && oldMeta <= 9) {
        return;
    }
    bd.metadata = (bd.metadata & ~0xF) | (newMeta & 0xF);
    server_world_set_block(s, x, y, z, bd);
}



// compute a single metadata value 0–9:
//   2–5 = ascending slopes,
//   0–1 = straight NS/EW,
//   6–9 = curves,
//   2–5 also reused for descending slopes (mirrored)
static uint8_t calcRailMeta(struct server_local* s,
                            w_coord_t x, w_coord_t y, w_coord_t z) {

	 struct block_data bd;

	// curve when this rail sits atop a diagonal slope
	bool lower_e = server_world_get_block(&s->world, x+1, y-1, z, &bd) && isRail(bd.type);
	bool lower_w = server_world_get_block(&s->world, x-1, y-1, z, &bd) && isRail(bd.type);
	bool lower_n = server_world_get_block(&s->world, x,   y-1, z-1, &bd) && isRail(bd.type);
	bool lower_s = server_world_get_block(&s->world, x,   y-1, z+1, &bd) && isRail(bd.type);

	// same‐level horizontal neighbours
	bool n = server_world_get_block(&s->world, x,   y, z-1, &bd) && isRail(bd.type);
	bool s_ =server_world_get_block(&s->world, x,   y, z+1, &bd) && isRail(bd.type);
	bool w = server_world_get_block(&s->world, x-1, y, z,   &bd) && isRail(bd.type);
	bool e = server_world_get_block(&s->world, x+1, y, z,   &bd) && isRail(bd.type);

	// if exactly one diagonal‐below slope + exactly one H‐neighbour on the perpendicular axis => curve
	if (lower_e && (n ^ s_) && !w && !e) return n ? 9 : 6;  // NE or SE
	if (lower_w && (n ^ s_) && !w && !e) return n ? 8 : 7;  // NW or SW
	if (lower_n && (w ^ e) && !n && !s_) return e ? 9 : 8;  // NE or NW
	if (lower_s && (w ^ e) && !n && !s_) return e ? 6 : 7;  // SE or SW

    // 1) slopes
    if (server_world_get_block(&s->world, x+1,y+1,z, &bd) && isRail(bd.type)) return 2;
    if (server_world_get_block(&s->world, x-1,y+1,z, &bd) && isRail(bd.type)) return 3;
    if (server_world_get_block(&s->world, x,y+1,z-1, &bd) && isRail(bd.type)) return 4;
    if (server_world_get_block(&s->world, x,y+1,z+1, &bd) && isRail(bd.type)) return 5;


    // 2) straight & curves
    n  = server_world_get_block(&s->world, x,   y, z-1, &bd) && isRail(bd.type);
    s_ = server_world_get_block(&s->world, x,   y, z+1, &bd) && isRail(bd.type);
    w  = server_world_get_block(&s->world, x-1, y, z,   &bd) && isRail(bd.type);
    e  = server_world_get_block(&s->world, x+1, y, z,   &bd) && isRail(bd.type);

    if ((n||s_) && !(w||e))      return 0; // NS
    if ((w||e) && !(n||s_))      return 1; // EW
    if (s_ && e)                 return 6; // SE
    if (s_ && w)                 return 7; // SW
    if (n   && w)                return 8; // NW
    if (n   && e)                return 9; // NE

    return 0;
}

static struct face_occlusion*
getSideMask(struct block_info* this, enum side side, struct block_info* it) {
	return face_occlusion_empty();
}

static uint8_t getTextureIndex1(struct block_info* this, enum side side) {
	return (this->block->metadata < 6) ? tex_atlas_lookup(TEXAT_RAIL) :
										 tex_atlas_lookup(TEXAT_RAIL_CURVED);
}

static uint8_t getTextureIndex2(struct block_info* this, enum side side) {
	return (this->block->metadata & 0x8) ?
		tex_atlas_lookup(TEXAT_RAIL_POWERED_ON) :
		tex_atlas_lookup(TEXAT_RAIL_POWERED_OFF);
}

static uint8_t getTextureIndex3(struct block_info* this, enum side side) {
	return tex_atlas_lookup(TEXAT_RAIL_DETECTOR);
}

static bool onItemPlace(struct server_local* s, struct item_data* it,
                        struct block_info* where, struct block_info* on,
                        enum side on_side) {
    struct block_data below_blk;
    if (!server_world_get_block(&s->world, where->x, where->y - 1, where->z, &below_blk)) {
        return false;
    }
    if (!blocks[below_blk.type] || blocks[below_blk.type]->can_see_through) {
        return false;
    }

    uint8_t meta = calcRailMeta(s, where->x, where->y, where->z);
    bool lower_e = false, lower_w = false, lower_n = false, lower_s = false;
    struct block_data neighbor;
    if (server_world_get_block(&s->world, where->x + 1, where->y - 1, where->z, &neighbor) && isRail(neighbor.type)) {
        lower_e = true;
    }
    if (server_world_get_block(&s->world, where->x - 1, where->y - 1, where->z, &neighbor) && isRail(neighbor.type)) {
        lower_w = true;
    }
    if (server_world_get_block(&s->world, where->x, where->y - 1, where->z + 1, &neighbor) && isRail(neighbor.type)) {
        lower_s = true;
    }
    if (server_world_get_block(&s->world, where->x, where->y - 1, where->z - 1, &neighbor) && isRail(neighbor.type)) {
        lower_n = true;
    }

    if (meta < 2) {
        if ((lower_e || lower_w) && !(lower_n || lower_s)) {
            meta = 1;
        } else if ((lower_n || lower_s) && !(lower_e || lower_w)) {
            meta = 0;
        }
    }

    if (lower_e) {
        updateRailToSlope(s, where->x + 1, where->y - 1, where->z, 3);
    }
    if (lower_w) {
        updateRailToSlope(s, where->x - 1, where->y - 1, where->z, 2);
    }
    if (lower_s) {
        updateRailToSlope(s, where->x, where->y - 1, where->z + 1, 4);
    }
    if (lower_n) {
        updateRailToSlope(s, where->x, where->y - 1, where->z - 1, 5);
    }

    struct block_data new_blk = {
        .type = it->id,
        .metadata = meta,
        .sky_light = 0,
        .torch_light = 0
    };
    server_world_set_block(s, where->x, where->y, where->z, new_blk);
    return true;
}

static void onNeighbourBlockChange(struct server_local* s,
                                   struct block_info* info) {
    struct block_data cur;
    server_world_get_block(&s->world,
                           info->x, info->y, info->z,
                           &cur);

    uint8_t oldMeta = cur.metadata & 0xF;
    uint8_t newMeta = calcRailMeta(s,
                                   info->x,
                                   info->y,
                                   info->z) & 0xF;
    if (newMeta == oldMeta) return;

    cur.metadata = (cur.metadata & ~0xF) | newMeta;
    server_world_set_block(s,
                           info->x, info->y, info->z,
                           cur);
}


struct block block_rail = {
	.name = "Rail",
	.getSideMask = getSideMask,
	.getBoundingBox = getBoundingBox,
	.getMaterial = getMaterial,
	.getTextureIndex = getTextureIndex1,
	.getDroppedItem = block_drop_default,
	.onNeighbourBlockChange = onNeighbourBlockChange,
	.onRandomTick = NULL,
	.onRightClick = NULL,
	.transparent = false,
	.renderBlock = render_block_rail,
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
		.onItemPlace = onItemPlace,
		.armor.is_armor = false,
		.tool.type = TOOL_TYPE_ANY,
	},
};

struct block block_powered_rail = {
	.name = "Powered rail",
	.getSideMask = getSideMask,
	.getBoundingBox = getBoundingBox,
	.getMaterial = getMaterial,
	.getTextureIndex = getTextureIndex2,
	.getDroppedItem = block_drop_default,
	.onNeighbourBlockChange = onNeighbourBlockChange,
	.onRandomTick = NULL,
	.onRightClick = NULL,
	.transparent = false,
	.renderBlock = render_block_rail,
	.renderBlockAlways = NULL,
	.luminance = 0,
	.double_sided = true,
	.can_see_through = true,
	.opacity = 0,
	.render_block_data.rail_curved_possible = false,
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
		.onItemPlace = onItemPlace,
		.armor.is_armor = false,
		.tool.type = TOOL_TYPE_ANY,
	},
};

struct block block_detector_rail = {
	.name = "Detector rail",
	.getSideMask = getSideMask,
	.getBoundingBox = getBoundingBox,
	.getMaterial = getMaterial,
	.getTextureIndex = getTextureIndex3,
	.getDroppedItem = block_drop_default,
	.onNeighbourBlockChange = onNeighbourBlockChange,
	.onRandomTick = NULL,
	.onRightClick = NULL,
	.transparent = false,
	.renderBlock = render_block_rail,
	.renderBlockAlways = NULL,
	.luminance = 0,
	.double_sided = true,
	.can_see_through = true,
	.opacity = 0,
	.render_block_data.rail_curved_possible = false,
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
		.onItemPlace = onItemPlace,
		.fuel = 0,
		.armor.is_armor = false,
		.tool.type = TOOL_TYPE_ANY,
	},
};

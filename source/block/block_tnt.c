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

#include "../cglm/types.h"
#include "../graphics/render_block.h"
#include "../graphics/render_item.h"
#include "../graphics/texture_atlas.h"
#include "../item/tool.h"
#include "../network/server_local.h"
#include "../network/server_world.h"
#include "../particle.h"
#include "aabb.h"
#include "blocks.h"
#include "blocks_data.h"
#include "face_occlusion.h"
#include <math.h>
#include "../game/game_state.h"


//todo: cleanup
//todo: move explosion logic to a helper function outside of this?

#define TNT_POWER 4.0f	  // blast radius 4 looks to be right, but crashes the game
#define TNT_FUSE_TICKS 15 // amount of ticks before TNT explodes
#define NUM_RAYS 1000	// amount of rays cast for an explosion 1000 is pretty heavy.. 300 maybe too weak.
#define STEP_SIZE 0.25f
#define HARDNESS_SCALE 0.0005f // how much the digging.hardness influences the resistance to explosion. Higher = materials don't break.
#define MAX_BROKEN 1024
struct broken_coord { w_coord_t x, y, z; };

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void random_unit_vector(vec3 out) {
    float z = 2.0f * ((rand() / (float)RAND_MAX) - 0.5f);
    float t = 2.0f * M_PI * (rand() / (float)RAND_MAX);
    float r = sqrtf(1.0f - z * z);
    out[0] = r * cosf(t);
    out[1] = r * sinf(t);
    out[2] = z;
}


// helper to perform the ray-cast block destruction
// 1) Ray-cast to collect blocks to destroy
// 2) Batch-remove them and spawn drops


// simplified explode: only TNT removal, particle effects, and delegate destruction
void tnt_explode(struct server_local* s,
                 w_coord_t x, w_coord_t y, w_coord_t z,
                 float power)
{
    if (power > TNT_POWER) power = TNT_POWER;

    // remove TNT block
    server_world_set_block(s,
                           x, y, z,
                           (struct block_data){ 0 });


    vec3 center = { x+0.5f, y+0.5f, z+0.5f };

    // initial explosion flash
    particle_generate_explosion_flash(center, power);

    // destroy blocks
    server_world_explode(s, center,	 TNT_POWER);


    // final explosion puffs
    particle_generate_explosion_smoke(center, power);

}

static void onWorldTick(struct server_local* s, struct block_info* info) {
    uint8_t fuse = info->block->metadata;
    if (fuse == 0) {
        bool powered = false;
        for (int side = 0; side < SIDE_MAX; ++side) {
            if (!info->neighbours) break;
            struct block_data nb = info->neighbours[side];
            uint8_t m = nb.metadata & 0x0F;
            if ((nb.type == BLOCK_REDSTONE_WIRE && m > 0) ||
                nb.type == BLOCK_REDSTONE_TORCH_LIT ||
                ((nb.type == BLOCK_STONE_PRESSURE_PLATE ||
                  nb.type == BLOCK_WOOD_PRESSURE_PLATE) &&
                 (m & 0x01)))
            {
                powered = true;
                break;
            }
        }
        if (powered) {
            server_world_set_block(s, info->x, info->y, info->z,
                (struct block_data){ .type = BLOCK_TNT, .metadata = TNT_FUSE_TICKS });
        }
        return;
    }
    if (fuse > 1) {
        info->block->metadata--;
        server_world_set_block(s, info->x, info->y, info->z, *info->block);
        vec3 center = { info->x + 0.5f, info->y + 0.5f, info->z + 0.5f };
        particle_generate_smoke(center, 1.0f);
    } else {
        server_world_set_block(s, info->x, info->y, info->z,
            (struct block_data){ .type = BLOCK_AIR, .metadata = 0, .sky_light = 0, .torch_light = 15 });
        tnt_explode(s, info->x, info->y, info->z, TNT_POWER);
    }
}



static enum block_material getMaterial(struct block_info* this) {
	return MATERIAL_ORGANIC;
}

static size_t getBoundingBox(struct block_info* this, bool entity, struct AABB* x) {
	if (x) aabb_setsize(x, 1.0F, 1.0F, 1.0F);
	return 1;
}

static struct face_occlusion* getSideMask(struct block_info* this, enum side side, struct block_info* it) {
	return face_occlusion_full();
}

static uint8_t getTextureIndex(struct block_info* this, enum side side) {
	if (this->block->metadata > 0 && (this->block->metadata % 4 < 2)) {
		return tex_atlas_lookup(TEXAT_SNOW); // flashing
	}

	switch (side) {
		case SIDE_TOP:
		case SIDE_BOTTOM:
			return tex_atlas_lookup(TEXAT_TNT_TOP);
		default:
			return tex_atlas_lookup(TEXAT_TNT_SIDE);
	}
}

static void onRightClick(struct server_local* s, struct item_data* it,
						 struct block_info* where, struct block_info* on,
						 enum side on_side) {
	if (on->block->type == BLOCK_TNT && on->block->metadata == 0) {
		server_world_set_block(s, on->x, on->y, on->z,
			(struct block_data){ .type = BLOCK_TNT, .metadata = TNT_FUSE_TICKS });
	}
}

static bool onItemPlace(struct server_local* s, struct item_data* it,
							struct block_info* where, struct block_info* on,
							enum side on_side) {
	struct block_data blk = (struct block_data) {
		.type = it->id,
		.metadata = 0,
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

struct block block_tnt = {
	.name = "TNT",
	.getSideMask = getSideMask,
	.getBoundingBox = getBoundingBox,
	.getMaterial = getMaterial,
	.getTextureIndex = getTextureIndex,
	.getDroppedItem = block_drop_default,
	.onRandomTick = NULL,
	.onWorldTick = onWorldTick,
	.onRightClick = onRightClick,
	.transparent = false,
	.renderBlock = render_block_full,
	.renderBlockAlways = NULL,
	.luminance = 0,
	.double_sided = false,
	.can_see_through = false,
	.ignore_lighting = false,
	.flammable = true,
	.place_ignore = false,
	.digging.hardness = 50,
	.digging.tool = TOOL_TYPE_ANY,
	.digging.min = TOOL_TIER_ANY,
	.digging.best = TOOL_TIER_ANY,
	.block_item = {
		.has_damage = false,
		.max_stack = 64,
		.renderItem = render_item_block,
		.onItemPlace = onItemPlace,
		.fuel = 0,
		.render_data.block.has_default = false,
		.armor.is_armor = false,
		.tool.type = TOOL_TYPE_ANY,
	},
};

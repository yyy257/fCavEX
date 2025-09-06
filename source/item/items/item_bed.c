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

#include "../../block/blocks_data.h"
#include "../../cglm/types.h"
#include "../../entity/entity.h"
#include "../../graphics/render_item.h"
#include "../../network/server_local.h"
#include "../../network/server_world.h"
#include "../items.h"
#include "../tool.h"

static bool onItemPlace(struct server_local* s, struct item_data* it,
                        struct block_info* where, struct block_info* on,
                        enum side on_side) {
    // Check directions: +Z, -Z, +X, -X
    const int dir[4][3] = {
        {0, 0, 1},   // +Z (metadata 0)
        {0, 0, -1},  // -Z (metadata 2)
        {1, 0, 0},   // +X (metadata 4)
        {-1, 0, 0},  // -X (metadata 6)
    };

    struct block_data blk;
    int metadata = -1;
    int dx = 0, dz = 0;

    for (int i = 0; i < 4; i++) {
        int tx = where->x + dir[i][0];
        int ty = where->y + dir[i][1];
        int tz = where->z + dir[i][2];
        if (!server_world_get_block(&s->world, tx, ty, tz, &blk)) continue;
        if (blk.type == 0) {
            metadata = i * 2;
            dx = dir[i][0];
            dz = dir[i][2];
            break;
        }
    }

    if (metadata == -1) return false; // No space for bed head

    struct block_data foot = {
        .type = BLOCK_BED,
        .metadata = metadata,
        .sky_light = 0,
        .torch_light = 0,
    };
    struct block_data head = {
        .type = BLOCK_BED,
        .metadata = metadata | 8, // Bit 3 = head flag
        .sky_light = 0,
        .torch_light = 0,
    };

    // Collision check with foot
    struct block_info blk2_info = *where;
    blk2_info.block = &foot;
    if (entity_local_player_block_collide((vec3) {s->player.x, s->player.y, s->player.z}, &blk2_info))
        return false;

    // Place foot
    server_world_set_block(s, where->x, where->y, where->z, foot);
    // Place head
    server_world_set_block(s, where->x + dx, where->y, where->z + dz, head);

    return true;
}

struct item item_bed = {
    .name = "Bed",
    .has_damage = false,
    .max_stack = 1,
    .fuel = 0,
    .renderItem = render_item_flat,
    .onItemPlace = onItemPlace,
    .armor.is_armor = false,
    .tool.type = TOOL_TYPE_ANY,
    .render_data = {
        .item = {
            .texture_x = 13,
            .texture_y = 2,
        },
    },
};

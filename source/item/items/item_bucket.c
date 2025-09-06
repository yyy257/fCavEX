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

#include "../../network/server_local.h"
#include "../../network/client_interface.h"
#include "../../network/server_interface.h"
#include "../../block/blocks.h"

#include "../../item/items.h"           // ITEM_BUCKET, ITEM_BUCKET_WATER, ITEM_BUCKET_LAVA
#include "../../item/inventory.h"
#include "../../item/window_container.h"// WINDOWC_INVENTORY
#include "../../network/server_local.h" // server_local_send_inv_changes
#include "../../network/inventory_logic.h" // set_inv_slot_*

// Use-on-block: if target cell (or clicked cell) is still water/lava, scoop it.
static bool onItemPlace(struct server_local* s, struct item_data* it,
                        struct block_info* where, struct block_info* on,
                        enum side on_side)
{
    if (!s || !it || !on || !on->block) return false;

    // Compute the placement cell adjacent to the clicked face
    int tx = on->x, ty = on->y, tz = on->z;
    switch (on_side) {
        case SIDE_TOP:    ty += 1; break;
        case SIDE_BOTTOM: ty -= 1; break;
        case SIDE_FRONT:  tz += 1; break;
        case SIDE_BACK:   tz -= 1; break;
        case SIDE_LEFT:   tx -= 1; break;
        case SIDE_RIGHT:  tx += 1; break;
        default: break;
    }

    struct block_data bd;
    uint16_t filled_id = 0;
    int sx=0, sy=0, sz=0; // liquid source coords

    // Prefer the placement cell (most engines hit liquid here)
    if (server_world_get_block(&s->world, tx, ty, tz, &bd)) {
        if (bd.type == BLOCK_WATER_STILL)      { filled_id = ITEM_BUCKET_WATER; sx=tx; sy=ty; sz=tz; }
        else if (bd.type == BLOCK_LAVA_STILL)  { filled_id = ITEM_BUCKET_LAVA;  sx=tx; sy=ty; sz=tz; }
    }
    // Fallbacks: clicked block itself, or "where"
    if (!filled_id) {
        if (on->block) {
            uint16_t bt = on->block->type;
            if (bt == BLOCK_WATER_STILL)      { filled_id = ITEM_BUCKET_WATER; sx=on->x; sy=on->y; sz=on->z; }
            else if (bt == BLOCK_LAVA_STILL)  { filled_id = ITEM_BUCKET_LAVA;  sx=on->x; sy=on->y; sz=on->z; }
        }
        if (!filled_id && where && where->block) {
            if (server_world_get_block(&s->world, where->x, where->y, where->z, &bd)) {
                if (bd.type == BLOCK_WATER_STILL)      { filled_id = ITEM_BUCKET_WATER; sx=where->x; sy=where->y; sz=where->z; }
                else if (bd.type == BLOCK_LAVA_STILL)  { filled_id = ITEM_BUCKET_LAVA;  sx=where->x; sy=where->y; sz=where->z; }
            }
        }
    }

    if (!filled_id) {
        // Not pointing at liquid -> do nothing (and don't consume).
        return false;
    }

    // Remove the liquid source (keep lights cheap/default)
    server_world_set_block(s, sx, sy, sz, (struct block_data){
        .type = BLOCK_AIR, .metadata = 0, .sky_light = 0, .torch_light = 0,
    });

    // Replace the actually-held hotbar slot with the filled bucket
    struct inventory* inv = &s->player.inventory;
    const size_t hotbar_rel = inventory_get_hotbar(inv);             // 0..8
    const size_t slot_abs   = INVENTORY_SLOT_HOTBAR + hotbar_rel;    // absolute index

    struct item_data filled = { .id = filled_id, .count = 1, .durability = 0 };
    inventory_set_slot(inv, slot_abs, filled);

    // Notify client (use your helper that emits CRPC_INVENTORY_SLOT)
    set_inv_slot_t changes; set_inv_slot_init(changes);
    set_inv_slot_push(changes, slot_abs);
    server_local_send_inv_changes(changes, inv, WINDOWC_INVENTORY);
    set_inv_slot_clear(changes);

    // Mirror local copy as well
    *it = filled;

    // CRITICAL: return false so server_local.c does NOT inventory_consume(...)
    return false;
}


struct item item_bucket = {
	.name = "Bucket",
	.has_damage = false,  // Disable damage (it won't be used up anymore)
	.max_damage = 0,      // Set max_damage to 0, so it doesn't get used up
	.max_stack = 1,
	.fuel = 0,
	.renderItem = render_item_flat,
	.onItemPlace = onItemPlace,
	.armor.is_armor = false,
	.tool.type = TOOL_TYPE_ANY,
	.render_data = {
		.item = {
			.texture_x = 10,
			.texture_y = 4,
		},
	},
};

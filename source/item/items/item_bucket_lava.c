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

static bool onItemPlace(struct server_local* s, struct item_data* it,
                        struct block_info* where, struct block_info* on,
                        enum side on_side)
{
    if (!s || !it || !on || !on->block) return false;

    // Compute placement cell adjacent to the clicked face
    int x = on->x, y = on->y, z = on->z;
    switch (on_side) {
        case SIDE_TOP:    y += 1; break;
        case SIDE_BOTTOM: y -= 1; break;
        case SIDE_FRONT:  z += 1; break;
        case SIDE_BACK:   z -= 1; break;
        case SIDE_LEFT:   x -= 1; break;
        case SIDE_RIGHT:  x += 1; break;
        default: break;
    }

    // Only place into AIR to keep logic simple and cheap
    struct block_data bd;
    if (!server_world_get_block(&s->world, x, y, z, &bd)) return false;
    if (bd.type != BLOCK_AIR) return false;

    // Place still water (reuse current light values from the AIR cell)
    server_world_set_block(s, x, y, z, (struct block_data){
        .type        = BLOCK_LAVA_STILL,
        .metadata    = 0,
        .sky_light   = bd.sky_light,
        .torch_light = bd.torch_light,
    });

    // Swap the actually-held hotbar slot back to an empty bucket
    struct inventory* inv = &s->player.inventory;
    const size_t hotbar_rel = inventory_get_hotbar(inv);           // 0..8
    const size_t slot_abs   = INVENTORY_SLOT_HOTBAR + hotbar_rel;  // absolute index (e.g. 36..44)

    const struct item_data new_it = { .id = ITEM_BUCKET, .count = 1, .durability = 0 };
    inventory_set_slot(inv, slot_abs, new_it);

    // Notify client for this slot
    set_inv_slot_t changes; set_inv_slot_init(changes);
    set_inv_slot_push(changes, slot_abs);
    server_local_send_inv_changes(changes, inv, WINDOWC_INVENTORY);
    set_inv_slot_clear(changes);

    // Mirror local copy
    *it = new_it;

    // IMPORTANT: return false so the engine does NOT consume the item again
    return false;
}

struct item item_bucket_lava = {
	.name = "Lava Bucket",
	.has_damage = false,  // Disable damage (it won't be used up anymore)
	.max_damage = 0,      // Set max_damage to 0, so it doesn't get used up
	.max_stack = 1,
	.fuel = 0,
	.renderItem = render_item_flat,
	.onItemPlace = NULL, //onItemPlace,
	.armor.is_armor = false,
	.tool.type = TOOL_TYPE_ANY,
	.render_data = {
		.item = {
			.texture_x = 12,
			.texture_y = 4,
		},
	},
};

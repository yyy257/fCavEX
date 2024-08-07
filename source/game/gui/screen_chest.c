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

#include "../../graphics/gfx_util.h"
#include "../../graphics/gfx_settings.h"
#include "../../graphics/gui_util.h"
#include "../../graphics/render_model.h"
#include "../../network/server_interface.h"
#include "../../platform/gfx.h"
#include "../../platform/input.h"
#include "../../platform/time.h"
#include "../game_state.h"
#include "screen.h"

#define GUI_WIDTH 176
#define GUI_HEIGHT 167

struct inv_slot {
	int x, y;
	size_t slot;
};

static bool pointer_has_item;
static bool pointer_available;
static float pointer_x, pointer_y, pointer_angle;
static struct inv_slot slots[CHEST_SIZE];
static size_t slots_index;
static size_t selected_slot;
static uint8_t chest_container;

void screen_chest_set_windowc(uint8_t container) {
	chest_container = container;
}

static void screen_chest_reset(struct screen* s, int width, int height) {
	input_pointer_enable(true);

	if(gstate.local_player)
		gstate.local_player->data.local_player.capture_input = false;

	s->render3D = screen_ingame.render3D;

	pointer_available = false;
	pointer_has_item = false;

	slots_index = 0;

	for(int k = 0; k < INVENTORY_SIZE_MAIN; k++) {
		slots[slots_index++] = (struct inv_slot) {
			.x = (8 + (k % INVENTORY_SIZE_HOTBAR) * 18) * GFX_GUI_SCALE,
			.y = (84 + (k / INVENTORY_SIZE_HOTBAR) * 18) * GFX_GUI_SCALE,
			.slot = k + CHEST_SLOT_MAIN,
		};
	}

	for(int k = 0; k < INVENTORY_SIZE_HOTBAR; k++) {
		if(k
		   == (int)inventory_get_hotbar(
			   windowc_get_latest(gstate.windows[WINDOWC_INVENTORY])))
			selected_slot = slots_index;

		slots[slots_index++] = (struct inv_slot) {
			.x = (8 + k * 18) * GFX_GUI_SCALE,
			.y = (84 + 3 * 18 + 4) * GFX_GUI_SCALE,
			.slot = k + CHEST_SLOT_HOTBAR,
		};
	}

	for(int k = 0; k < CHEST_SIZE_STORAGE; k++) {
		slots[slots_index++] = (struct inv_slot) {
			.x = (8 + (k % INVENTORY_SIZE_HOTBAR) * 18) * GFX_GUI_SCALE,
			.y = (16 + (k / INVENTORY_SIZE_HOTBAR) * 18) * GFX_GUI_SCALE,
			.slot = k + CHEST_SLOT_STORAGE,
		};
	}
}

static void screen_chest_update(struct screen* s, float dt) {
	if(input_pressed(IB_INVENTORY)) {
		svin_rpc_send(&(struct server_rpc) {
			.type = SRPC_WINDOW_CLOSE,
			.payload.window_close.window = chest_container,
		});

		screen_set(&screen_ingame);
	}

	if(input_pressed(IB_GUI_CLICK)) {
		uint16_t action_id;
		if(windowc_new_action(gstate.windows[chest_container], &action_id,
							  false, slots[selected_slot].slot)) {
			svin_rpc_send(&(struct server_rpc) {
				.type = SRPC_WINDOW_CLICK,
				.payload.window_click.window = chest_container,
				.payload.window_click.action_id = action_id,
				.payload.window_click.right_click = false,
				.payload.window_click.slot = slots[selected_slot].slot,
			});
		}
	} else if(input_pressed(IB_GUI_CLICK_ALT)) {
		uint16_t action_id;
		if(windowc_new_action(gstate.windows[chest_container], &action_id,
							  true, slots[selected_slot].slot)) {
			svin_rpc_send(&(struct server_rpc) {
				.type = SRPC_WINDOW_CLICK,
				.payload.window_click.window = chest_container,
				.payload.window_click.action_id = action_id,
				.payload.window_click.right_click = true,
				.payload.window_click.slot = slots[selected_slot].slot,
			});
		}
	}

	pointer_available = input_pointer(&pointer_x, &pointer_y, &pointer_angle);

	size_t slot_nearest[4]
		= {selected_slot, selected_slot, selected_slot, selected_slot};
	int slot_dist[4] = {INT_MAX, INT_MAX, INT_MAX, INT_MAX};
	int pointer_slot = -1;

	int off_x = (gfx_width() - GUI_WIDTH * GFX_GUI_SCALE) / 2;
	int off_y = (gfx_height() - GUI_HEIGHT * GFX_GUI_SCALE) / 2;

	for(size_t k = 0; k < slots_index; k++) {
		int dx = slots[k].x - slots[selected_slot].x;
		int dy = slots[k].y - slots[selected_slot].y;

		if(pointer_x >= off_x + slots[k].x
		   && pointer_x < off_x + slots[k].x + 16 * GFX_GUI_SCALE 
		   && pointer_y >= off_y + slots[k].y
		   && pointer_y < off_y + slots[k].y + 16 * GFX_GUI_SCALE)
			pointer_slot = k;

		int distx = dx * dx + dy * dy * 8;
		int disty = dx * dx * 8 + dy * dy;

		if(dx < 0 && distx < slot_dist[0]) {
			slot_nearest[0] = k;
			slot_dist[0] = distx;
		}

		if(dx > 0 && distx < slot_dist[1]) {
			slot_nearest[1] = k;
			slot_dist[1] = distx;
		}

		if(dy < 0 && disty < slot_dist[2]) {
			slot_nearest[2] = k;
			slot_dist[2] = disty;
		}

		if(dy > 0 && disty < slot_dist[3]) {
			slot_nearest[3] = k;
			slot_dist[3] = disty;
		}
	}

	if(pointer_available && pointer_slot >= 0) {
		selected_slot = pointer_slot;
		pointer_has_item = true;
	} else {
		if(input_pressed(IB_GUI_LEFT)) {
			selected_slot = slot_nearest[0];
			pointer_has_item = false;
		}

		if(input_pressed(IB_GUI_RIGHT)) {
			selected_slot = slot_nearest[1];
			pointer_has_item = false;
		}

		if(input_pressed(IB_GUI_UP)) {
			selected_slot = slot_nearest[2];
			pointer_has_item = false;
		}

		if(input_pressed(IB_GUI_DOWN)) {
			selected_slot = slot_nearest[3];
			pointer_has_item = false;
		}
	}
}

static void screen_chest_render2D(struct screen* s, int width, int height) {
	struct inventory* inv
		= windowc_get_latest(gstate.windows[chest_container]);

	// darken background
	gfx_texture(false);
	gutil_texquad_col(0, 0, 0, 0, 0, 0, width, height, 0, 0, 0, 180);
	gfx_texture(true);

	int off_x = (width - GUI_WIDTH * GFX_GUI_SCALE) / 2;
	int off_y = (height - GUI_HEIGHT * GFX_GUI_SCALE) / 2;

	// draw inventory
	gfx_bind_texture(&texture_gui_chest);
	gutil_texquad(off_x, off_y, 0, 0, GUI_WIDTH, GUI_HEIGHT, GUI_WIDTH * GFX_GUI_SCALE,
				  GUI_HEIGHT * GFX_GUI_SCALE);
	gutil_text(off_x + 28 * GFX_GUI_SCALE, off_y + 6 * GFX_GUI_SCALE, "\2478Chest", 8 * GFX_GUI_SCALE, false);

	struct inv_slot* selection = slots + selected_slot;

	// draw items
	for(size_t k = 0; k < slots_index; k++) {
		struct item_data item;
		if((selected_slot != k || !inventory_get_picked_item(inv, NULL)
			|| (pointer_available && pointer_has_item))
		   && inventory_get_slot(inv, slots[k].slot, &item))
			gutil_draw_item(&item, off_x + slots[k].x, off_y + slots[k].y, 1);
	}

	gfx_bind_texture(&texture_gui2);

	gutil_texquad(off_x + selection->x - 4 * GFX_GUI_SCALE, off_y + selection->y - 4 * GFX_GUI_SCALE, 208, 0,
				  24, 24, 24 * GFX_GUI_SCALE, 24 * GFX_GUI_SCALE);

	int icon_offset = 16 * GFX_GUI_SCALE;
	icon_offset += gutil_control_icon(icon_offset, IB_GUI_UP, "Move");
	if(inventory_get_picked_item(inv, NULL)) {
		icon_offset
			+= gutil_control_icon(icon_offset, IB_GUI_CLICK, "Swap item");
	} else if(inventory_get_slot(inv, selection->slot, NULL)) {
		icon_offset
			+= gutil_control_icon(icon_offset, IB_GUI_CLICK, "Pickup item");
		icon_offset
			+= gutil_control_icon(icon_offset, IB_GUI_CLICK_ALT, "Split stack");
	}

	icon_offset += gutil_control_icon(icon_offset, IB_INVENTORY, "Leave");

	struct item_data item;
	if(inventory_get_picked_item(inv, &item)) {
		if(pointer_available && pointer_has_item) {
			gutil_draw_item(&item, pointer_x - 8 * GFX_GUI_SCALE, pointer_y - 8 * GFX_GUI_SCALE, 0);
		} else {
			gutil_draw_item(&item, off_x + selection->x, off_y + selection->y,
							0);
		}
	} else if(inventory_get_slot(inv, selection->slot, &item)) {
		char* tmp = item_get(&item) ? item_get(&item)->name : "Unknown";
		gfx_blending(MODE_BLEND);
		gfx_texture(false);
		gutil_texquad_col(off_x + selection->x - 2 * GFX_GUI_SCALE + 8 * GFX_GUI_SCALE
							  - gutil_font_width(tmp, 8 * GFX_GUI_SCALE) / 2,
						  off_y + selection->y - 2 * GFX_GUI_SCALE + 23 * GFX_GUI_SCALE, 0, 0, 0, 0,
						  gutil_font_width(tmp, 8 * GFX_GUI_SCALE) + 7, 12 * GFX_GUI_SCALE, 0, 0, 0, 180);
		gfx_texture(true);
		gfx_blending(MODE_OFF);

		gutil_text(off_x + selection->x + 8 * GFX_GUI_SCALE - gutil_font_width(tmp, 8 * GFX_GUI_SCALE) / 2,
				   off_y + selection->y + 23 * GFX_GUI_SCALE, tmp, 8 * GFX_GUI_SCALE, false);
	}

	if(pointer_available) {
		gfx_bind_texture(&texture_pointer);
		gutil_texquad_rt_any(pointer_x, pointer_y, glm_rad(pointer_angle), 0, 0,
							 256, 256, 48 * GFX_GUI_SCALE, 48 * GFX_GUI_SCALE);
	}
}

struct screen screen_chest = {
	.reset = screen_chest_reset,
	.update = screen_chest_update,
	.render2D = screen_chest_render2D,
	.render3D = NULL,
	.render_world = true,
};

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

#include "../../graphics/gui_util.h"
#include "../../graphics/gfx_settings.h"
#include "../../network/server_local.h"
#include "../../platform/gfx.h"
#include "../../platform/input.h"
#include "../game_state.h"

static void screen_lworld_reset(struct screen* s, int width, int height) {
	input_pointer_enable(false);

	if(gstate.local_player)
		gstate.local_player->data.local_player.capture_input = false;
}

static void screen_lworld_update(struct screen* s, float dt) {
	if(gstate.world_loaded)
		screen_set(&screen_ingame);
}

static void screen_lworld_render2D(struct screen* s, int width, int height) {
	gutil_bg();

	gutil_text((width - gutil_font_width("Generating level", 8 * GFX_GUI_SCALE)) / 2,
			   height / 2 - 20 * GFX_GUI_SCALE, "Generating level", 8 * GFX_GUI_SCALE, true);

	gutil_text((width - gutil_font_width("Building terrain", 8 * GFX_GUI_SCALE)) / 2,
			   height / 2 + 4 * GFX_GUI_SCALE, "Building terrain", 8 * GFX_GUI_SCALE, true);

	gutil_text(2 * GFX_GUI_SCALE, height - 2 * GFX_GUI_SCALE - (9 * GFX_GUI_SCALE) * 2, "Licensed under GPLv3", 8 * GFX_GUI_SCALE, true);
	gutil_text(2 * GFX_GUI_SCALE, height - 2 * GFX_GUI_SCALE - (9 * GFX_GUI_SCALE) * 1, "Copyright (c) 2023 ByteBit/xtreme8000",
			   8 * GFX_GUI_SCALE, true);

	// just a rough estimate
	float progress
		= fminf((float)world_loaded_chunks(&gstate.world) / MAX_CHUNKS, 1.0F);

	gfx_texture(false);
	gutil_texquad_col((width - 100 * GFX_GUI_SCALE) / 2, height / 2 + 16 * GFX_GUI_SCALE, 0, 0, 0, 0, 100 * GFX_GUI_SCALE, 2 * GFX_GUI_SCALE,
					  128, 128, 128, 255);
	gutil_texquad_col((width - 100 * GFX_GUI_SCALE) / 2, height / 2 + 16 * GFX_GUI_SCALE, 0, 0, 0, 0,
					  100 * GFX_GUI_SCALE * progress, 2 * GFX_GUI_SCALE, 128, 255, 128, 255);
	gfx_texture(true);
}

struct screen screen_load_world = {
	.reset = screen_lworld_reset,
	.update = screen_lworld_update,
	.render2D = screen_lworld_render2D,
	.render3D = NULL,
	.render_world = false,
};

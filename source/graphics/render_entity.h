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

#ifndef RENDER_MINECART_H
#define RENDER_MINECART_H

#include "../item/items.h"
#include "../platform/gfx.h"
#include "../cglm/types.h"

typedef struct {
    uint8_t u, v, w, h;
} UVRect;

void render_entity_minecart_init(void);
void render_entity_minecart(mat4 view);
void render_entity_update_light(uint8_t light);
void render_entity_init(void);
void render_entity_creeper(mat4 view, float headYawDeg, float bodyYawDeg, int frame);
void render_entity_pig(mat4 view, float headYawDeg);

#endif

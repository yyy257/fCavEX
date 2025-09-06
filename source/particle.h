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

#ifndef PARTICLE_H
#define PARTICLE_H

#include "cglm/cglm.h"
#include "platform/time.h"
#include "world.h"


typedef enum {
  TEXTURE_ATLAS_TERRAIN,
  TEXTURE_ATLAS_PARTICLES
} particle_atlas_t;


struct particle {
    vec3             pos;        // current position
    vec3             pos_old;    // previous position for interpolation
    vec3             vel;        // velocity
    vec2             tex_uv;     // only for terrain‐atlas random offset
    float            size;       // half‐quad size
    int              age;        // remaining life in ticks
    int              lifetime;   // initial life in ticks, for animating smoke
    uint8_t          tex;        // base tile index
    bool             gravity;    // apply gravity?
    uint8_t     	 r, g, b;	 // rgb colorisation of a particle
    bool             ignore_light;    // apply gravity?
    particle_atlas_t atlas;      // which atlas to sample
};


void particle_add(vec3 pos,
                  vec3 vel,
                  uint8_t tex,
                  float size,
                  float lifetime,
                  bool gravity,
                  uint8_t r,
                  uint8_t g,
                  uint8_t b,
				  bool ignore_light,
				  particle_atlas_t atlas);

void particle_init(void);
void particle_set_camera(vec3 p);
void particle_generate_block(struct block_info* info);
void particle_generate_side(struct block_info* info, enum side s);
void particle_update(void);
void particle_render(mat4 view, vec3 camera, float delta);
void particle_generate_explosion_flash(vec3 center, float intensity);
void particle_generate_explosion_smoke(vec3 center, float intensity);
void particle_generate_smoke(vec3 center, float intensity);
void particle_generate_torch(vec3 pos);
void particle_generate_redstone_torch(vec3 center);
void particle_generate_redstone_wire(vec3 center, uint8_t power);
#endif

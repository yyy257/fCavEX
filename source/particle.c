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

#include <assert.h>

#include "game/game_state.h"
#include "graphics/render_block.h"
#include "particle.h"
#include "platform/gfx.h"
#include "graphics/texture_atlas.h"
#include "platform/texture.h"


#define PARTICLES_AREA 8
#define PARTICLES_VOLUME 64

ARRAY_DEF(array_particle, struct particle, M_POD_OPLIST)

array_particle_t particles;

static vec3 s_cameraPos;
static const float SPAWN_CULL_RADIUS = 64.0f;
static const float SPAWN_CULL_RADIUS2 = SPAWN_CULL_RADIUS * SPAWN_CULL_RADIUS;

static const uint8_t redstone_colors[16][3] = {
    {111,   0,  0},  // 0
    {120,   3,  0},  // 1
    {130,   7,  0},  // 2
    {139,  10,  0},  // 3
    {149,  13,  0},  // 4
    {158,  16,  0},  // 5
    {167,  20,  0},  // 6
    {177,  23,  0},  // 7
    {186,  26,  0},  // 8
    {196,  29,  0},  // 9
    {205,  33,  0},  // 10
    {214,  36,  0},  // 11
    {224,  39,  0},  // 12
    {233,  42,  0},  // 13
    {243,  46,  0},  // 14
    {252,  49,  0}   // 15
};


void particle_init() {
	array_particle_init(particles);
}

void particle_set_camera(vec3 p) {
	glm_vec3_copy(p, s_cameraPos);
}

static float rnd(void) {
    return rand_gen_flt(&gstate.rand_src);
}

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
                  particle_atlas_t atlas)
{
	// skip any new particle > spawn radius from camera
    if (glm_vec3_distance2(pos, s_cameraPos) > SPAWN_CULL_RADIUS2) {
        return;
    }

    struct particle *p = array_particle_push_new(particles);
    if (!p) return;

    glm_vec3_copy(pos,    p->pos);
    glm_vec3_copy(pos,    p->pos_old);
    glm_vec3_copy(vel,    p->vel);

    p->tex      = tex;
    p->size     = size;
    p->age      = (int)lifetime;
    p->lifetime = (int)lifetime;
    p->gravity  = gravity;
    p->r = r;  p->g = g;  p->b = b;
    p->ignore_light = ignore_light;
    p->atlas    = atlas;

    if (atlas == TEXTURE_ATLAS_TERRAIN) {
        float fx = (TEX_OFFSET(TEXTURE_X(tex)) + rnd() * 12.0f) / 256.0f;
        float fy = (TEX_OFFSET(TEXTURE_Y(tex)) + rnd() * 12.0f) / 256.0f;
        p->tex_uv[0] = fx;
        p->tex_uv[1] = fy;
    } else {
        p->tex_uv[0] = p->tex_uv[1] = 0.0f;
    }
}

void particle_generate_block(struct block_info* info) {
    assert(info && info->block && info->neighbours);
    if (!blocks[info->block->type]) return;

    size_t count = blocks[info->block->type]->getBoundingBox(info, false, NULL);
    if (!count) return;

    struct AABB aabb[count];
    blocks[info->block->type]->getBoundingBox(info, false, aabb);

    float volume = (aabb->x2 - aabb->x1)
                 * (aabb->y2 - aabb->y1)
                 * (aabb->z2 - aabb->z1);
    uint8_t tex = blocks[info->block->type]
                  ->getTextureIndex(info, SIDE_FRONT);

    for (int k = 0; k < volume * PARTICLES_VOLUME; k++) {
        float x = rand_gen_flt(&gstate.rand_src) * (aabb->x2 - aabb->x1) + aabb->x1;
        float y = rand_gen_flt(&gstate.rand_src) * (aabb->y2 - aabb->y1) + aabb->y1;
        float z = rand_gen_flt(&gstate.rand_src) * (aabb->z2 - aabb->z1) + aabb->z1;

        vec3 vel = {
            rand_gen_flt(&gstate.rand_src) - 0.5F,
            rand_gen_flt(&gstate.rand_src) - 0.5F,
            rand_gen_flt(&gstate.rand_src) - 0.5F
        };
        glm_vec3_normalize(vel);
        glm_vec3_scale(
            vel,
            (2.0F * rand_gen_flt(&gstate.rand_src) + 0.5F) * 0.05F,
            vel
        );

        vec3 pos = { info->x + x, info->y + y, info->z + z };
        float size = (rand_gen_flt(&gstate.rand_src) + 1.0F) * 0.03125F;
        float age  = 4.0F / (rand_gen_flt(&gstate.rand_src) * 0.9F + 0.1F);

        particle_add(pos, vel, tex, size, age, true, 255,255,255, false, TEXTURE_ATLAS_TERRAIN);
    }
}

void particle_generate_side(struct block_info* info, enum side s) {
    assert(info && info->block && info->neighbours);
    if (!blocks[info->block->type]) return;

    size_t count = blocks[info->block->type]->getBoundingBox(info, false, NULL);
    if (!count) return;

    struct AABB aabb[count];
    blocks[info->block->type]->getBoundingBox(info, false, aabb);

    float area;
    switch (s) {
        case SIDE_RIGHT:
        case SIDE_LEFT:
            area = (aabb->y2 - aabb->y1) * (aabb->z2 - aabb->z1);
            break;
        case SIDE_BOTTOM:
        case SIDE_TOP:
            area = (aabb->x2 - aabb->x1) * (aabb->z2 - aabb->z1);
            break;
        case SIDE_FRONT:
        case SIDE_BACK:
            area = (aabb->x2 - aabb->x1) * (aabb->y2 - aabb->y1);
            break;
        default:
            return;
    }

    uint8_t tex = blocks[info->block->type]
                  ->getTextureIndex(info, s);
    float offset = 0.0625F;

    for (int k = 0; k < area * PARTICLES_AREA; k++) {
        float x = rand_gen_flt(&gstate.rand_src) * (aabb->x2 - aabb->x1) + aabb->x1;
        float y = rand_gen_flt(&gstate.rand_src) * (aabb->y2 - aabb->y1) + aabb->y1;
        float z = rand_gen_flt(&gstate.rand_src) * (aabb->z2 - aabb->z1) + aabb->z1;

        switch (s) {
            case SIDE_LEFT:   x = aabb->x1 - offset; break;
            case SIDE_RIGHT:  x = aabb->x2 + offset; break;
            case SIDE_BOTTOM: y = aabb->y1 - offset; break;
            case SIDE_TOP:    y = aabb->y2 + offset; break;
            case SIDE_FRONT:  z = aabb->z1 - offset; break;
            case SIDE_BACK:   z = aabb->z2 + offset; break;
            default: return;
        }

        vec3 vel = {
            rand_gen_flt(&gstate.rand_src) - 0.5F,
            rand_gen_flt(&gstate.rand_src) - 0.5F,
            rand_gen_flt(&gstate.rand_src) - 0.5F
        };
        glm_vec3_normalize(vel);
        glm_vec3_scale(
            vel,
            (2.0F * rand_gen_flt(&gstate.rand_src) + 0.5F) * 0.05F,
            vel
        );

        vec3 pos = { info->x + x, info->y + y, info->z + z };
        float size = (rand_gen_flt(&gstate.rand_src) + 1.0F) * 0.03125F;
        float age  = 4.0F / (rand_gen_flt(&gstate.rand_src) * 0.9F + 0.1F);

        particle_add(pos, vel, tex, size, age, true, 255,255,255, false, TEXTURE_ATLAS_TERRAIN);
        }
}

void particle_generate_explosion_flash(vec3 center, float intensity) {
    uint8_t tex_smoke_base = tex_atlas_lookup_particle(TEXAT_PARTICLE_EMBER);
    // roughly 12 smoke puffs per unit intensity
    int count = (int)ceilf(intensity * 12.0f);

    for (int i = 0; i < count; i++) {
        vec3 vel = {
            rnd()*2.0f - 1.0f,
            rnd()*2.0f - 1.0f,
            rnd()*2.0f - 1.0f
        };
        glm_vec3_normalize(vel);
        // give them a bit of speed outward
        glm_vec3_scale(vel, (0.3f + rnd()*0.2f) * intensity, vel);

        vec3 pos = {
            center[0] + (rnd()-0.5f)*0.2f,
            center[1] + (rnd()-0.5f)*0.2f,
            center[2] + (rnd()-0.5f)*0.2f
        };

        float size = 0.2f * intensity;

        float life = (8.0f + rnd()*8.0f) * intensity * 0.5f;

        uint8_t brightness = (uint8_t)(rnd() * 150.0f + 50.0f);

        //    no gravity => no damping, so they drift until age runs out
        particle_add(
            pos,
            vel,
            tex_smoke_base,            // start frame for smoke animation
            size,                      // particle size
            life,                      // lifetime in ticks
            false,                     // no gravity (no damping)
            brightness, brightness, brightness, // random gray brightness
			true,
            TEXTURE_ATLAS_PARTICLES    // use particle atlas
        );
    }
}

void particle_generate_redstone_torch(vec3 center) {
    const int count = 3;  // particles per tick
    const uint8_t baseR = 252, baseG = 49, baseB = 0;

    for (int i = 0; i < count; ++i) {
        uint8_t frame     = (uint8_t)(rnd() * 8.0f);
        uint8_t tex_smoke = tex_atlas_lookup_particle(TEXAT_PARTICLE_SMOKE_0 + frame);

        float factor = 0.5f + rnd() * 0.5f;
        uint8_t r = (uint8_t)(baseR * factor);
        uint8_t g = (uint8_t)(baseG * factor);
        uint8_t b = (uint8_t)(baseB * factor);

        vec3 pos = {
            center[0] + (rnd() - 0.5f) * 0.1f,
            center[1],
            center[2] + (rnd() - 0.5f) * 0.1f
        };

        vec3 vel = {
            (rnd() - 0.5f) * 0.01f,
            0.02f           + rnd() * 0.01f,
            (rnd() - 0.5f) * 0.01f
        };

        float size = 0.15f + rnd() * 0.05f;
        float life = 10.0f  + rnd() * 10.0f;

        particle_add(
            pos, vel,
            tex_smoke,
            size, life,
            false,       // no gravity
            r, g, b,     // tinted by brightness factor
			false,		// light
            TEXTURE_ATLAS_PARTICLES
        );
    }
}

void particle_generate_redstone_wire(vec3 center, uint8_t power) {
    if (power == 0) return;               // no puffs at zero

    float strength = (float)power / 15.0f;

    int count = 1 + (int)(3.0f * strength);
    for (int i = 0; i < count; ++i) {
        uint8_t frame     = (uint8_t)(rnd() * 8.0f);
        uint8_t tex_smoke = tex_atlas_lookup_particle(
            TEXAT_PARTICLE_SMOKE_0 + frame
        );

        const uint8_t *baseCol = redstone_colors[power];
        float brightFactor     = 0.6f + rnd() * 0.4f;  // 60–100%
        uint8_t r = (uint8_t)(baseCol[0] * brightFactor);
        uint8_t g = (uint8_t)(baseCol[1] * brightFactor);
        uint8_t b = (uint8_t)(baseCol[2] * brightFactor);

        vec3 pos = {
            center[0] + (rnd() - 0.5f) * 0.12f,
            center[1] + 0.125f + rnd() * 0.03f,
            center[2] + (rnd() - 0.5f) * 0.12f
        };

        float baseVel = 0.008f + 0.012f * strength;
        vec3 vel = {
            (rnd() - 0.5f) * baseVel,
            (0.015f + rnd() * 0.01f) * strength,
            (rnd() - 0.5f) * baseVel
        };

        // size & lifetime: small at low power, bigger at high power
        float size = (0.05f + rnd() * 0.04f) * (0.5f + 0.5f * strength);
        float life = (5.0f  + rnd() * 5.0f)  * (0.5f + 0.5f * strength);

        particle_add(
            pos, vel,
            tex_smoke,
            size, life,
            false,    // no gravity
            r, g, b,  // tint by power
			false,
            TEXTURE_ATLAS_PARTICLES
        );
    }
}

void particle_generate_explosion_smoke(vec3 center, float intensity) {
    // number of smoke puffs
    int count = (int)ceilf(intensity * 12.0f);

    for (int i = 0; i < count; i++) {
        // 1) Choose a random smoke frame (0…7) for variety
        uint8_t frame = (uint8_t)(rnd() * 8.0f);
        uint8_t tex_smoke = tex_atlas_lookup_particle(TEXAT_PARTICLE_SMOKE_0 + frame);

        // 2) Random brightness between 100 (dark gray) and 255 (light gray)
        uint8_t brightness = (uint8_t)(rnd() * 155.0f + 100.0f);

        // 3) Random direction in a sphere
        vec3 vel = {
            rnd()*2.0f - 1.0f,
            rnd()*2.0f - 1.0f,
            rnd()*2.0f - 1.0f
        };
        glm_vec3_normalize(vel);
        // scale velocity so puffs stay reasonably close
        glm_vec3_scale(vel, (0.2f + rnd()*0.1f) * intensity, vel);

        // 4) Spawn very close to center, with small jitter
        vec3 pos = {
            center[0] + (rnd()-0.5f)*0.15f,
            center[1] + (rnd()-0.5f)*0.15f,
            center[2] + (rnd()-0.5f)*0.15f
        };

        // 5) Randomize lifetime between 10–20 ticks × intensity
        float life = (10.0f + rnd()*10.0f) * intensity;

        // 6) Add particle with no gravity (so no damping), random gray shade
        particle_add(
            pos,
            vel,
            tex_smoke,             // random smoke frame
            0.15f * intensity,     // size
            life,                  // lifetime
            false,                 // no gravity => no damping
            brightness,brightness,brightness,// random gray brightness
			false,
            TEXTURE_ATLAS_PARTICLES
        );
    }
}

// render single takes the brightness of a particle into account and is also used for animation of smoke particles
static void render_single(struct particle* p, vec3 camera, float delta) {
    // Cull any particles beyond 32 blocks
    if (glm_vec3_distance2(p->pos, camera) > 32.0f * 32.0f)
        return;

    // Interpolate position for smooth motion
    vec3 pos_lerp;
    glm_vec3_lerp(p->pos_old, p->pos, delta, pos_lerp);

    // Build billboarding axes
    vec3 view_dir, axis_s, axis_t;
    glm_vec3_sub(pos_lerp, camera, view_dir);
    glm_vec3_crossn(view_dir, (vec3){0,1,0}, axis_s);
    glm_vec3_crossn(view_dir, axis_s, axis_t);
    glm_vec3_scale(axis_s, p->size, axis_s);
    glm_vec3_scale(axis_t, p->size, axis_t);

    // Determine the correct texture tile (for animated smoke)
    uint8_t tile = p->tex;
    if (p->atlas == TEXTURE_ATLAS_PARTICLES
     && tile >= tex_atlas_lookup_particle(TEXAT_PARTICLE_SMOKE_0)
     && tile <= tex_atlas_lookup_particle(TEXAT_PARTICLE_SMOKE_7))
    {
        float t      = (float)p->age / (float)p->lifetime;  // 1.0→0.0
        int   frame  = (int)(t * 7.0f + 0.5f);
        tile         = tex_atlas_lookup_particle(TEXAT_PARTICLE_SMOKE_0 + frame);
    }

    // Compute UV coordinates
    float u0, v0, u1, v1;
    if (p->atlas == TEXTURE_ATLAS_TERRAIN) {
        u0 = p->tex_uv[0];
        v0 = p->tex_uv[1];
        u1 = u0 + (4.0f  / 256.0f);
        v1 = v0 + (4.0f  / 256.0f);
    } else {
        u0 = (TEX_OFFSET(TEXTURE_X(tile))) / 256.0f;
        v0 = (TEX_OFFSET(TEXTURE_Y(tile))) / 256.0f;
        u1 = u0 + (16.0f / 256.0f);
        v1 = v0 + (16.0f / 256.0f);
    }

    // Lookup world light at the particle's block position
    uint8_t light;
    if (p->ignore_light) {
        light = 255;  // full brightness
    } else {
        struct block_data in_block = world_get_block(
            &gstate.world,
            floorf(pos_lerp[0]),
            floorf(pos_lerp[1]),
            floorf(pos_lerp[2])
        );
        light = roundf(
            gfx_lookup_light((in_block.torch_light<<4)|in_block.sky_light)
            *255.0F*0.8F
        );
    }


    // Modulate our RGBA color by that light
    uint8_t cols[16];
    for (int i = 0; i < 4; ++i) {
        cols[i*4 + 0] = (p->r * light) >> 8;
        cols[i*4 + 1] = (p->g * light) >> 8;
        cols[i*4 + 2] = (p->b * light) >> 8;
        cols[i*4 + 3] = 255;
    }

    // Finally draw the quad
    gfx_draw_quads_flt(
        4,
        (float[]){
            -axis_s[0] - axis_t[0] + pos_lerp[0],
            -axis_s[1] - axis_t[1] + pos_lerp[1],
            -axis_s[2] - axis_t[2] + pos_lerp[2],

             axis_s[0] - axis_t[0] + pos_lerp[0],
             axis_s[1] - axis_t[1] + pos_lerp[1],
             axis_s[2] - axis_t[2] + pos_lerp[2],

             axis_s[0] + axis_t[0] + pos_lerp[0],
             axis_s[1] + axis_t[1] + pos_lerp[1],
             axis_s[2] + axis_t[2] + pos_lerp[2],

            -axis_s[0] + axis_t[0] + pos_lerp[0],
            -axis_s[1] + axis_t[1] + pos_lerp[1],
            -axis_s[2] + axis_t[2] + pos_lerp[2]
        },
        cols,
        (float[]){
            u0, v0,
            u1, v0,
            u1, v1,
            u0, v1
        }
    );
}


void particle_update() {
	array_particle_it_t it;
	array_particle_it(it, particles);

	while(!array_particle_end_p(it)) {
		struct particle* p = array_particle_ref(it);

		glm_vec3_copy(p->pos, p->pos_old);

		vec3 new_pos;
		glm_vec3_add(p->pos, p->vel, new_pos);

		w_coord_t bx = floorf(new_pos[0]);
		w_coord_t by = floorf(new_pos[1]);
		w_coord_t bz = floorf(new_pos[2]);
		struct block_data in_block = world_get_block(&gstate.world, bx, by, bz);

		bool intersect = false;
		if(blocks[in_block.type]) {
			struct block_info blk = (struct block_info) {
				.block = &in_block,
				.neighbours = NULL,
				.x = bx,
				.y = by,
				.z = bz,
			};

			size_t count
				= blocks[in_block.type]->getBoundingBox(&blk, true, NULL);
			if(count > 0) {
				struct AABB aabb[count];
				blocks[in_block.type]->getBoundingBox(&blk, true, aabb);

				for(size_t k = 0; k < count; k++) {
					aabb_translate(aabb + k, bx, by, bz);
					intersect = aabb_intersection_point(aabb + k, new_pos[0],
														new_pos[1], new_pos[2]);
					if(intersect)
						break;
				}
			}
		}

		if(!intersect) {
			glm_vec3_copy(new_pos, p->pos);
		} else {
			glm_vec3_zero(p->vel);
		}

        if (p->gravity) {
            p->vel[1] -= 0.04F;
            glm_vec3_scale(p->vel, 0.98F, p->vel);
        }
		p->age--;

		if(p->age > 0) {
			array_particle_next(it);
		} else {
			array_particle_remove(particles, it);
		}
	}
}

void particle_generate_smoke(vec3 center, float intensity) {
    int count = (int)ceilf(intensity * 4.0f);
    // always start at the largest smoke-frame
    uint8_t tex_smoke_base = tex_atlas_lookup_particle(TEXAT_PARTICLE_SMOKE_7);

    for (int i = 0; i < count; i++) {
        // Position jitter around center
        vec3 pos = {
            center[0] + (rnd() - 0.5f) * 0.3f,
            center[1] + 0.7f + rnd() * 0.2f,
            center[2] + (rnd() - 0.5f) * 0.3f
        };

        // Upward drift + slight horizontal drift
        vec3 vel = {
            (rnd() - 0.5f) * 0.01f,
            0.1f + rnd() * 0.02f,
            (rnd() - 0.5f) * 0.01f
        };

        // Shorter, randomized lifetime (10–20 ticks) × intensity
        float life = (10.0f + rnd() * 10.0f) * intensity;

        // Random gray shade: brightness between 50 (dark) and 200 (light)
        uint8_t brightness = (uint8_t)(rnd() * 150.0f + 50.0f);

        // Add particle without gravity (no damping), now with full RGBA:
        particle_add(
            pos,
            vel,
            tex_smoke_base,           // enum index for frame 7
            0.15f * intensity,        // size
            life,                     // lifetime in ticks
            false,                    // no gravity => no damping
            brightness,               // r
            brightness,               // g
            brightness,               // b
			false,
            TEXTURE_ATLAS_PARTICLES   // atlas
        );
    }
}

void particle_generate_torch(vec3 pos) {
    uint8_t tex_flame      = tex_atlas_lookup_particle(TEXAT_PARTICLE_FLAME);
    uint8_t tex_smoke_base = tex_atlas_lookup_particle(TEXAT_PARTICLE_SMOKE_7);

    // spawn 2 sparks per call
    for (int i = 0; i < 2; i++) {
        // flickering flame spark
        vec3 vel_f = {
            (rnd() - 0.5f) * 0.015f,
            0.015f + rnd() * 0.01f,
            (rnd() - 0.5f) * 0.015f
        };
        float size_f = 0.04f + rnd() * 0.04f;   // size 0.04–0.08
        float life_f = 6.0f  + rnd() * 2.0f;    // lifetime 6–8 ticks

        particle_add(
            pos, vel_f,
            tex_flame,
            size_f, life_f,
            false,    // no gravity (no damping)
			255, 255, 255,     // full brightness for flame
			true,
            TEXTURE_ATLAS_PARTICLES
        );

        // occasional smoke: about 1 in 10 sparks
        if (rnd() < 0.1f) {
            // a) smaller smoke puff
            vec3 vel_s = {
                (rnd() - 0.5f) * 0.006f,
                0.008f + rnd() * 0.005f,
                (rnd() - 0.5f) * 0.006f
            };
            vec3 spawn = {
                pos[0] + (rnd() - 0.5f) * 0.03f,
                pos[1] + (rnd() - 0.5f) * 0.03f,
                pos[2] + (rnd() - 0.5f) * 0.03f
            };
            float size_s = 0.06f + rnd() * 0.04f;   // now 0.06–0.10
            float life_s = 10.0f + rnd() * 10.0f;
            uint8_t brightness_s = (uint8_t)(rnd() * 100.0f + 50.0f);

            particle_add(
                spawn, vel_s,
                tex_smoke_base,
                size_s, life_s,
                false,          // no gravity => no damping
	            brightness_s,               // r
	            brightness_s,               // g
	            brightness_s,               // b,   // varied gray brightness
				false,
                TEXTURE_ATLAS_PARTICLES
            );
        }
    }
}

void particle_render(mat4 view, vec3 camera, float delta) {
    gfx_matrix_modelview(view);
    gfx_lighting(false);

    // Terrain/block particles
    gfx_bind_texture(&texture_terrain);
    array_particle_it_t it;
    array_particle_it(it, particles);
    while(!array_particle_end_p(it)) {
        struct particle* p = array_particle_ref(it);
        if(p->atlas == TEXTURE_ATLAS_TERRAIN)
            render_single(p, camera, delta);
        array_particle_next(it);
    }

    // Particle-atlas particles (sparks, smoke, etc)
    gfx_bind_texture(&texture_particles);
    array_particle_it(it, particles);
    while(!array_particle_end_p(it)) {
        struct particle* p = array_particle_ref(it);
        if(p->atlas == TEXTURE_ATLAS_PARTICLES)
        	render_single(p, camera, delta);
        array_particle_next(it);
    }

    gfx_lighting(true);
}

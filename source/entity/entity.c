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

#include "entity.h"
#include "../network/server_world.h"
#include "../network/server_local.h"

#include "../platform/gfx.h"
#include "../world.h"

void entity_default_init(struct entity* e, bool server, void* world) {
	e->on_server = server;
	e->world = world;
	e->on_ground = true;
	e->delay_destroy = -1;

	glm_vec3_zero(e->pos);
	glm_vec3_zero(e->pos_old);
	glm_vec3_zero(e->network_pos);
	glm_vec3_zero(e->vel);
	glm_vec2_zero(e->orient);
	glm_vec2_zero(e->orient_old);
}

void entity_default_teleport(struct entity* e, vec3 pos) {
	e->on_ground = false;

	glm_vec3_copy(pos, e->pos);
	glm_vec3_copy(pos, e->pos_old);
	glm_vec3_copy(pos, e->network_pos);
}

bool entity_default_client_tick(struct entity* e) {
	assert(e);

	glm_vec3_copy(e->pos, e->pos_old);
	glm_vec3_copy(e->network_pos, e->pos);
	glm_vec2_copy(e->orient, e->orient_old);
	return false;
}

bool entity_get_block(struct entity* e, w_coord_t x, w_coord_t y, w_coord_t z,
					  struct block_data* blk) {
	assert(e && blk);

	if(e->on_server) {
		return server_world_get_block(e->world, x, y, z, blk);
	} else {
		*blk = world_get_block(e->world, x, y, z);
		return true;
	}
}

void entity_shadow(struct entity* e, struct AABB* a, mat4 view) {
	assert(e && a && view);

	w_coord_t min_x = floorf(a->x1);
	w_coord_t min_y = floorf(a->y1);
	w_coord_t min_z = floorf(a->z1);

	w_coord_t max_x = ceilf(a->x2) + 1;
	w_coord_t max_y = ceilf(a->y2) + 1;
	w_coord_t max_z = ceilf(a->z2) + 1;

	gfx_matrix_modelview(view);
	gfx_blending(MODE_BLEND);
	gfx_alpha_test(false);
	gfx_bind_texture(&texture_shadow);
	gfx_lighting(false);

	float offset = 0.01F;
	float du = 1.0F / (a->x2 - a->x1);
	float dv = 1.0F / (a->z2 - a->z1);

	for(w_coord_t x = min_x; x < max_x; x++) {
		for(w_coord_t z = min_z; z < max_z; z++) {
			for(w_coord_t y = min_y; y < max_y; y++) {
				struct block_data blk;

				if(entity_get_block(e, x, y, z, &blk) && blocks[blk.type]) {
					struct block_info blk_info = (struct block_info) {
						.block = &blk,
						.neighbours = NULL,
						.x = x,
						.y = y,
						.z = z,
					};

					size_t count = blocks[blk.type]->getBoundingBox(&blk_info,
																	true, NULL);
					if(count > 0) {
						struct AABB bbox[count];
						blocks[blk.type]->getBoundingBox(&blk_info, true, bbox);

						for(size_t k = 0; k < count; k++) {
							aabb_translate(bbox + k, x, y, z);

							if(a->y2 > bbox[k].y2
							   && aabb_intersection(a, bbox + k)) {
								float u1 = (bbox[k].x1 - a->x1) * du;
								float u2 = (bbox[k].x2 - a->x1) * du;
								float v1 = (bbox[k].z1 - a->z1) * dv;
								float v2 = (bbox[k].z2 - a->z1) * dv;

								gfx_draw_quads_flt(
									4,
									(float[]) {bbox[k].x1, bbox[k].y2 + offset,
											   bbox[k].z1, bbox[k].x2,
											   bbox[k].y2 + offset, bbox[k].z1,
											   bbox[k].x2, bbox[k].y2 + offset,
											   bbox[k].z2, bbox[k].x1,
											   bbox[k].y2 + offset, bbox[k].z2},
									(uint8_t[]) {0xFF, 0xFF, 0xFF, 0x60, 0xFF,
												 0xFF, 0xFF, 0x60, 0xFF, 0xFF,
												 0xFF, 0x60, 0xFF, 0xFF, 0xFF,
												 0x60},
									(float[]) {u1, v1, u2, v1, u2, v2, u1, v2});
							}
						}
					}
				}
			}
		}
	}

	gfx_blending(MODE_OFF);
	gfx_alpha_test(true);
	gfx_lighting(true);
}

#define max(a, b) (((a) > (b)) ? (a) : (b))
#define min(a, b) (((a) < (b)) ? (a) : (b))

bool entity_block_aabb_test(struct AABB* entity, struct block_info* blk_info) {
	assert(entity && blk_info);

	struct block* b = blocks[blk_info->block->type];
	size_t count = b->getBoundingBox(blk_info, true, NULL);

	if(count > 0) {
		struct AABB bbox[count];
		b->getBoundingBox(blk_info, true, bbox);

		for(size_t k = 0; k < count; k++) {
			aabb_translate(bbox + k, blk_info->x, blk_info->y, blk_info->z);

			if(aabb_intersection(entity, bbox + k))
				return true;
		}
	}

	return false;
}

bool entity_intersection(struct entity* e, struct AABB* a,
						 bool (*test)(struct AABB* entity,
									  struct block_info* blk_info)) {
	assert(e && a && test);

	w_coord_t min_x = floorf(a->x1);
	// need to look one further, otherwise fence block breaks
	w_coord_t min_y = max((w_coord_t)(floorf(a->y1) - 1), 0);
	w_coord_t min_z = floorf(a->z1);

	w_coord_t max_x = ceilf(a->x2) + 1;
	w_coord_t max_y = min((w_coord_t)(ceilf(a->y2) + 1), WORLD_HEIGHT - 1);
	w_coord_t max_z = ceilf(a->z2) + 1;

	for(w_coord_t x = min_x; x < max_x; x++) {
		for(w_coord_t z = min_z; z < max_z; z++) {
			for(w_coord_t y = min_y; y < max_y; y++) {
				struct block_data blk;

				if(entity_get_block(e, x, y, z, &blk) && blocks[blk.type]
				   && test(a,
						   &(struct block_info) {.block = &blk,
												 .neighbours = NULL,
												 .x = x,
												 .y = y,
												 .z = z}))
					return true;
			}
		}
	}

	return false;
}

bool entity_aabb_intersection(struct entity* e, struct AABB* a) {
	return entity_intersection(e, a, entity_block_aabb_test);
}

bool entity_intersection_threshold(struct entity* e, struct AABB* aabb,
								   vec3 old_pos, vec3 new_pos,
								   float* threshold) {
	assert(e && aabb && old_pos && new_pos && threshold);

	struct AABB tmp = *aabb;
	aabb_translate(&tmp, old_pos[0], old_pos[1], old_pos[2]);
	bool a = entity_aabb_intersection(e, &tmp);

	tmp = *aabb;
	aabb_translate(&tmp, new_pos[0], new_pos[1], new_pos[2]);
	bool b = entity_aabb_intersection(e, &tmp);

	if(!a && b) {
		float range_min = 0.0F;
		float range_max = 1.0F;

		while(1) {
			vec3 pos_min, pos_max;
			glm_vec3_lerp(old_pos, new_pos, range_min, pos_min);
			glm_vec3_lerp(old_pos, new_pos, range_max, pos_max);

			if(glm_vec3_distance2(pos_min, pos_max) < glm_pow2(0.01F))
				break;

			float mid = (range_max + range_min) / 2.0F;

			vec3 pos_mid;
			glm_vec3_lerp(old_pos, new_pos, mid, pos_mid);

			struct AABB dest = *aabb;
			aabb_translate(&dest, pos_mid[0], pos_mid[1], pos_mid[2]);

			if(entity_aabb_intersection(e, &dest)) {
				range_max = mid;
			} else {
				range_min = mid;
			}
		}

		*threshold = range_min;
		return true;
	} else if(a) {
		*threshold = 0.0F;
		return true;
	} else {
		*threshold = 1.0F;
		return false;
	}
}

void entity_try_move(struct entity* e, vec3 pos, vec3 vel, struct AABB* bbox,
					 size_t coord, bool* collision_xz, bool* on_ground) {
	assert(e && pos && vel && bbox && collision_xz && on_ground);

	vec3 tmp;
	glm_vec3_copy(pos, tmp);
	tmp[coord] += vel[coord];

	float threshold;
	if(entity_intersection_threshold(e, bbox, pos, tmp, &threshold)) {
		if(coord == 1 && vel[1] < 0.0F)
			*on_ground = true;
		if(coord == 0 || coord == 2)
			*collision_xz = true;

		vel[coord] = 0.0F;
	} else if(coord == 1) {
		*on_ground = false;
	}
	pos[coord] = pos[coord] * (1.0F - threshold) + tmp[coord] * threshold;
}

uint32_t entity_gen_id(dict_entity_t dict) {
	assert(dict);

	dict_entity_it_t it;
	dict_entity_it(it, dict);

	// id = 0 is reserved for local player

	uint32_t id = 0;

	while(!dict_entity_end_p(it)) {
		uint32_t key = dict_entity_ref(it)->key;

		if(key > id)
			id = key;

		dict_entity_next(it);
	}

	return id + 1;
}

void entities_client_tick(dict_entity_t dict) {
    dict_entity_it_t it;
    dict_entity_it(it, dict);

    while (!dict_entity_end_p(it)) {
        uint32_t key = dict_entity_ref(it)->key;
        struct entity *e = dict_entity_ref(it)->value;

        if (e->tick_client) {

            bool remove = (e->delay_destroy == 0) || e->tick_client(e);

            dict_entity_next(it);

            if (remove) {
                free(e);
                dict_entity_erase(dict, key);
            }
        } else {
            dict_entity_next(it);
        }
    }
}

void entities_client_render(dict_entity_t dict, struct camera* c,
							float tick_delta) {
	dict_entity_it_t it;
	dict_entity_it(it, dict);

	while(!dict_entity_end_p(it)) {
		struct entity* e = dict_entity_ref(it)->value;
		if(e->render
		   && glm_vec3_distance2(e->pos, (vec3) {c->x, c->y, c->z})
			   < glm_pow2(32.0F))
			e->render(e, c->view, tick_delta);
		dict_entity_next(it);
	}
}

bool entity_aabb_intersect_ray(const vec3 origin,
                               const vec3 dir,
                               const struct entity *e,
                               float *out_t)
{
    assert(origin && dir && e && out_t);
    assert(e->getBoundingBox != NULL);

    // 1) Vraag de AABB van de entity op via de functiepointer
    struct AABB box;
    size_t count = e->getBoundingBox(e, &box);
    if (count == 0) {
        return false;
    }

    // 2) Voer de slab‐methode uit op box.x1..box.z2
    float invDx = 1.0f / (fabsf(dir[0]) > GLM_FLT_EPSILON
                          ? dir[0]
                          : copysignf(GLM_FLT_EPSILON, dir[0]));
    float invDy = 1.0f / (fabsf(dir[1]) > GLM_FLT_EPSILON
                          ? dir[1]
                          : copysignf(GLM_FLT_EPSILON, dir[1]));
    float invDz = 1.0f / (fabsf(dir[2]) > GLM_FLT_EPSILON
                          ? dir[2]
                          : copysignf(GLM_FLT_EPSILON, dir[2]));

    float t1 = (box.x1 - origin[0]) * invDx;
    float t2 = (box.x2 - origin[0]) * invDx;
    float t3 = (box.y1 - origin[1]) * invDy;
    float t4 = (box.y2 - origin[1]) * invDy;
    float t5 = (box.z1 - origin[2]) * invDz;
    float t6 = (box.z2 - origin[2]) * invDz;

    float tmin = fmaxf(fmaxf(fminf(t1, t2), fminf(t3, t4)), fminf(t5, t6));
    float tmax = fminf(fminf(fmaxf(t1, t2), fmaxf(t3, t4)), fmaxf(t5, t6));

    if (tmax < 0.0f || tmin > tmax) {
        return false;
    }
    *out_t = (tmin >= 0.0f ? tmin : tmax);
    return true;
}



//-----------------------------------------------------------------------------
// Raycasts against all entities stored in dict_entity_t. Returns the closest
// hit entity within maxDist; *out_tNear is set to that hit distance.
// Returns NULL if no entity is hit.
//-----------------------------------------------------------------------------
struct entity *
raycast_entity(dict_entity_t *entities,
               const vec3 origin,
               const vec3 dir,
               float maxDist,
               float *out_tNear)
{
    struct entity *closest = NULL;
    float closestT = maxDist + 1.0f;

    // Use the dict_entity iterator API to loop through all entities
    dict_entity_it_t it;
    dict_entity_it(it, *entities);

    while (!dict_entity_end_p(it)) {
        struct entity *e = dict_entity_ref(it)->value;

        if (e->id == 0) {
            dict_entity_next(it);
            continue;
        }

        float tHit;
        if (entity_aabb_intersect_ray(origin, dir, e, &tHit)) {
            if (tHit >= 0.0f && tHit < closestT && tHit <= maxDist) {
                closestT = tHit;
                closest  = e;
            }
        }

        dict_entity_next(it);
    }

    if (closest && out_tNear) {
        *out_tNear = closestT;
    }
    return closest;
}


void entity_move_in_direction(struct entity* e, float accel, vec2 dir) {
    e->vel[0] += accel * dir[0];
    e->vel[2] += accel * dir[1];
}

void entity_clamp_speed(struct entity* e, float max_speed) {
    float vx = e->vel[0], vz = e->vel[2];
    float mag = sqrtf(vx * vx + vz * vz);
    if (mag > max_speed) {
        float scale = max_speed / mag;
        e->vel[0] *= scale;
        e->vel[2] *= scale;
    }
}

void entity_apply_gravity(struct entity* e, float gravity) {
    e->vel[1] -= gravity;
    e->on_ground = false;
}

void entity_apply_friction(struct entity* e, float slip) {
    e->vel[0] *= slip * 0.8f;
    e->vel[2] *= slip * 0.8f;
    e->vel[1] *= 0.9f;
}

void entity_apply_gravity_and_friction(struct entity* e, float gravity, float slip_ground, float slip_air) {
    e->vel[1] -= gravity;
    float slip = e->on_ground ? slip_ground : slip_air;
    e->vel[0] *= slip * 0.8f;
    e->vel[2] *= slip * 0.8f;
    e->vel[1] *= 0.9f;
    e->on_ground = false;
}

bool entity_try_auto_jump(struct entity* e, float jump_force, float threshold) {
    float dx = e->vel[0];
    float dz = e->vel[2];
    float mag = sqrtf(dx * dx + dz * dz);
    if (mag < threshold) return false;

    dx /= mag;
    dz /= mag;

    int tx = (int)floorf(e->pos[0] + dx);
    int ty = (int)floorf(e->pos[1]);
    int tz = (int)floorf(e->pos[2] + dz);

    struct block_data blk1, blk2;
    bool has_front = entity_get_block(e, tx, ty,     tz, &blk1);
    bool has_top   = entity_get_block(e, tx, ty + 1, tz, &blk2);

    bool blocked_ahead = has_front && !blocks[blk1.type]->can_see_through;
    bool space_above   = !has_top || blocks[blk2.type]->can_see_through;

    if (blocked_ahead && space_above) {
        e->vel[1] = jump_force;
        return true;
    }
    return false;
}


void entity_try_unstuck(struct entity* e, void (*make_bbox)(struct AABB*)) {
    struct AABB b0, b1;
    make_bbox(&b0);
    aabb_translate(&b0, e->pos[0], e->pos[1], e->pos[2]);
    b1 = b0;
    aabb_translate(&b1, 0.0f, 0.01f, 0.0f);

    if (entity_aabb_intersection(e, &b0) && !entity_aabb_intersection(e, &b1)) {
        e->pos[1] += 0.01f;
    }
}

void entity_try_move_axis(struct entity* e, int axis, struct AABB (*make_bbox)(void), bool* collision_flag, bool* on_ground_flag) {
    struct AABB bb = make_bbox();
    entity_try_move(e, e->pos, e->vel, &bb, axis, collision_flag, on_ground_flag);
    if (axis == 1 && *on_ground_flag) {
        e->vel[1] = 0.0f;
    }
}

void entity_damp_velocity(struct entity* e, float threshold) {
    if (fabsf(e->vel[0]) < threshold) e->vel[0] = 0.0f;
    if (fabsf(e->vel[2]) < threshold) e->vel[2] = 0.0f;
// don't touch Y unless we really need it
    }

void entity_get_delta(struct entity* e, vec3 out_delta) {
    glm_vec3_sub(e->pos, e->pos_old, out_delta);
}

float entity_get_horizontal_speed(vec3 delta) {
    return sqrtf(delta[0]*delta[0] + delta[2]*delta[2]);
}


void entity_blend_body_to_head(float* body_yaw, float head_yaw, float factor) {
    float diff = head_yaw - *body_yaw;
    while (diff >  M_PI) diff -= 2.0f * M_PI;
    while (diff < -M_PI) diff += 2.0f * M_PI;
    *body_yaw += diff * factor;
}

void entity_tick_animation(struct entity* e, float walk_speed, int max_frame) {
    if (walk_speed > 0.001f) {
        e->data.monster.frame_time_left--;
        if (e->data.monster.frame_time_left <= 0) {
            e->data.monster.frame = (e->data.monster.frame + 1) % max_frame;
            e->data.monster.frame_time_left = 1;
        }
    }
}

void entity_choose_random_direction(struct entity* e, vec2 out_dir) {
    struct server_local* s = e->world;
    float ang = rand_gen_flt(&s->rand_src) * 2.0f * M_PI;
    out_dir[0] = cosf(ang);
    out_dir[1] = sinf(ang);
}

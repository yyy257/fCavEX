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
    along with CavEX. If not, see <http://www.gnu.org/licenses/>.
*/

#include <assert.h>
#include <math.h>
#include "entity.h"
#include "entity_monster.h"
#include "../block/blocks_data.h"
#include "../network/server_local.h"
#include "../network/client_interface.h"
#include "../network/server_interface.h"
#include "../world.h"
#include "../graphics/render_entity.h"
#include "../platform/gfx.h"
#include "../util.h"
#include "../particle.h"
#include "../game/game_state.h"
#include "../network/server_world.h"




// Creeper movement constants
#define CREEPER_ACCEL            0.25f    // acceleration per tick
#define CREEPER_MAX_SPEED        1.25f    // max horizontal speed per tick
#define CREEPER_WANDER_INTERVAL  20      // ticks between direction changes
#define GRAVITY                  2.0f   // gravity per tick
#define UNSTUCK_MOVE             0.01f   // slight upward nudge if stuck
#define YAW_SMOOTH_FACTOR        0.2f    // smoothing factor [0..1]

// Local bounding box for collision (centered, half-height offset)
void make_creeper_bbox(struct AABB* out) {
    const float sx = 0.6f, sy = 1.7f, sz = 0.6f;
    aabb_setsize_centered_offset(out, sx, sy, sz, 0.0f, sy * 0.5f, 0.0f);
}

static struct AABB make_creeper_bbox_ret(void) {
    struct AABB b;
    make_creeper_bbox(&b);
    return b;
}

// Client-side tick: interpolate and smooth orientation
static bool client_tick_creeper(struct entity* e) {
    assert(e);

    glm_vec3_copy(e->pos, e->pos_old);
    glm_vec3_lerp(e->pos, e->network_pos, 0.3f, e->pos);

    vec3 delta;
    entity_get_delta(e, delta);
    float speed = sqrtf(delta[0]*delta[0] + delta[2]*delta[2]);
    if (speed > 0.001f) {
        e->data.monster.head_yaw = atan2f(delta[0], delta[2]);
    }
    entity_blend_body_to_head(&e->data.monster.body_yaw,
                              e->data.monster.head_yaw,
                              YAW_SMOOTH_FACTOR);

    e->orient[0] = 0.0f;
    e->orient[1] = e->data.monster.body_yaw + e->data.monster.head_yaw;

    entity_tick_animation(e, speed, 60);
    if (e->health <= 0 && e->data.monster.fuse > 0) {
        vec3 interp;
        glm_vec3_lerp(e->pos_old, e->pos, 1.0f, interp);
        particle_generate_explosion_smoke(interp, 1.0f);

    }

    return false;
}

void server_explode(struct server_local *s, vec3 center, float radius) {
    int r = ceilf(radius);
    for(int dx = -r; dx <= r; dx++) {
      for(int dy = -r; dy <= r; dy++) {
        for(int dz = -r; dz <= r; dz++) {
          float dist = sqrtf(dx*dx + dy*dy + dz*dz);
          if (dist > radius) continue;

          int bx = (int)floorf(center[0]) + dx;
          int by = (int)floorf(center[1]) + dy;
          int bz = (int)floorf(center[2]) + dz;

          // *** BOUNDARY CHECK ***
          if (by < 0 || by >= WORLD_HEIGHT) continue;
          // eventueel ook x/z check binnen chunk-limieten,
          // maar server_world_get_block doet dat vaak al intern.

          if (server_world_get_block(&s->world, bx, by, bz, NULL)) {
            server_world_set_block(s, bx, by, bz,
              (struct block_data){ .type = BLOCK_AIR, .metadata = 0 });
          }
        }
      }
    }
}


// Server-side creeper logic
static bool server_tick_creeper(struct entity* e, struct server_local* s) {
    assert(e && s);

    glm_vec3_copy(e->pos,    e->pos_old);
    glm_vec2_copy(e->orient, e->orient_old);

    entity_damp_velocity(e, 0.005f);

    if (e->health <= 0 && e->data.monster.fuse < 0) {
            e->data.monster.fuse = 8; // was 30 but that's wayyyy too long
            e->ai_state = AI_FUSE;
        }

    vec3 center = { e->pos[0], e->pos[1], e->pos[2] };

    if (e->data.monster.fuse >= 0) {
        printf("[DEBUG] Creeper %u fuse remaining: %d ticks\n",
               (unsigned)e->id,
               e->data.monster.fuse);
        particle_generate_explosion_smoke(center, 2.0f);


        if (--e->data.monster.fuse == 0) {
            // explosie
            particle_generate_explosion_flash(center, 3.0f);
            particle_generate_explosion_smoke(center, 3.0f);
            server_world_explode(s, center, 3.0f);
            e->delay_destroy = 0;
            server_local_spawn_item(center,
                    &e->drop_item,
                    true,
                    s
                );
        }
        return false;  // sla rest van AI over zolang we fuseren
    }

    if (--e->data.monster.direction_time <= 0) {
        float ang = rand_gen_flt(&s->rand_src) * 2.0f * M_PI;
        e->data.monster.direction[0] = cosf(ang);
        e->data.monster.direction[1] = sinf(ang);
        e->data.monster.direction_time = 30 + rand_gen_int(&s->rand_src, 30);
    }

    entity_move_in_direction(e, CREEPER_ACCEL, e->data.monster.direction);
    entity_clamp_speed(e, CREEPER_MAX_SPEED);

    entity_try_unstuck(e, make_creeper_bbox);

    bool collision_xz = false;
    entity_try_move_axis(e, 1, make_creeper_bbox_ret, &collision_xz, &e->on_ground);

    if (e->on_ground) {
        struct AABB bb_check = make_creeper_bbox_ret();
        bb_check.y1 -= 0.01f;
        bb_check.y2 += 0.01f;
        aabb_translate(&bb_check, e->pos[0], e->pos[1], e->pos[2]);

        if (collision_xz || entity_aabb_intersection(e, &bb_check)) {
            if (entity_try_auto_jump(e, 4.0f, 0.01f)) {
                float ang = rand_gen_flt(&s->rand_src) * 2.0f * M_PI;
                e->data.monster.direction[0] = cosf(ang);
                e->data.monster.direction[1] = sinf(ang);
                e->data.monster.direction_time = 30 + rand_gen_int(&s->rand_src, 30);
            }
        }
    }

    entity_try_move_axis(e, 0, make_creeper_bbox_ret, &collision_xz, &e->on_ground);
    entity_try_move_axis(e, 2, make_creeper_bbox_ret, &collision_xz, &e->on_ground);

    entity_apply_gravity(e, GRAVITY);
    entity_apply_friction(e, e->on_ground ? 0.6f : 1.0f);

    clin_rpc_send(&(struct client_rpc){
        .type = CRPC_ENTITY_MOVE,
        .payload.entity_move.entity_id = e->id,
        .payload.entity_move.pos = {e->pos[0], e->pos[1], e->pos[2]}
    });

    return false;
}

static bool entity_server_tick(struct entity* e, struct server_local* s) {
    return (e->data.monster.id == MONSTER_CREEPER)
        ? server_tick_creeper(e, s)
        : false;
}

static bool entity_client_tick(struct entity* e) {
    if (e->delay_destroy == 0) {
        return true;
    }

    if (e->type == ENTITY_MONSTER) {
        switch (e->data.monster.id) {
            case MONSTER_CREEPER:
                return client_tick_creeper(e);
            // todo: other monsters
          // case MONSTER_ZOMBIE: return client_tick_zombie(e);
            default:
                return false;
        }
    }
    return false;
}

// Render creeper
static void entity_render(struct entity* e, mat4 view, float td) {
    assert(e);

    vec3 p;
    glm_vec3_lerp(e->pos_old, e->pos, td, p);

    vec3 delta;
    glm_vec3_sub(e->pos, e->pos_old, delta);
    float bodyYaw = e->data.monster.body_yaw;
    float headYaw = bodyYaw + e->data.monster.head_yaw;

    // update light
    struct block_data blk;
    entity_get_block(e, floorf(p[0]), floorf(p[1]), floorf(p[2]), &blk);
    render_entity_update_light((blk.torch_light << 4) | blk.sky_light);

    mat4 model, mv;
    glm_translate_make(model, p);
    glm_mat4_mul(view, model, mv);

    render_entity_creeper(mv,
                          glm_deg(bodyYaw),
                          glm_deg(headYaw),
                          e->data.monster.frame);

    // shadow, doesn't seem to work currently
    struct AABB shadow_bb;
    aabb_setsize_centered(&shadow_bb, 0.25f, 0.25f, 0.25f);
    aabb_translate(&shadow_bb, p[0], p[1] - 0.04f, p[2]);
    entity_shadow(e, &shadow_bb, view);
}



// Raycast & interaction bounding box (world-translated)
static size_t getBoundingBox_creeper(const struct entity* e, struct AABB* out) {
    assert(e && out);
    make_creeper_bbox(out);
    aabb_translate(out, e->pos[0], e->pos[1], e->pos[2]);
    return 1;
}

bool onLeftClick(struct entity *e) {

    svin_rpc_send(&(struct server_rpc){
        .type = SRPC_ENTITY_ATTACK,
        .payload.entity_attack.entity_id = e->id,
    });

    return true;
}

// Factory
void entity_monster(uint32_t id,
                    struct entity* e,
                    bool server,
                    void* world,
                    int monster_id) {
    assert(e && world);
    entity_default_init(e, server, world);
    e->id = id;
    e->type = ENTITY_MONSTER;
    e->drop_item = (struct item_data){
        .id         = ITEM_GUNPOWDER,
        .durability = 0,
        .count      = 1
    };
    e->name = "Monster";
    e->data.monster.id = monster_id;
    e->data.monster.head_yaw = 0.0f;
    e->health     = 20;
    e->data.monster.body_yaw = 0.0f;
    e->data.monster.direction[0] = 0.0f;
    e->data.monster.direction[1] = 0.0f;
    e->data.monster.direction_time= 0;
    e->data.monster.frame = 0;
    e->data.monster.fuse = -1;
    e->data.monster.frame_time_left = 1;
    glm_vec3_copy(e->pos, e->network_pos);
    e->tick_server = entity_server_tick;
    e->tick_client = entity_client_tick;
    e->render      = entity_render;
    e->teleport    = entity_default_teleport;
    e->getBoundingBox = getBoundingBox_creeper;
    e->onLeftClick = onLeftClick;
    e->leftClickText = "Attack";
    e->onRightClick = NULL;
    e->rightClickText = NULL;
    e->delay_destroy = -1;
    // spawn slightly higher, to prevent getting stuck
    if (server) {
        e->pos[1] += 0.1f;
        e->pos_old[1] = e->pos[1];
    }

    e->ai_timer     = CREEPER_WANDER_INTERVAL;
}

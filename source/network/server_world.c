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

#include "../lighting.h"
#include "../util.h"
#include "client_interface.h"
#include "server_local.h"
#include "server_world.h"
#include "../daytime.h"

#define EXPLOSION_MAX_RAYS 300
#define EXPLOSION_STEP     0.5f
#define HARDNESS_SCALE     0.0005f

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

#define CHUNK_DIST2(x1, x2, z1, z2)                                            \
	(((x1) - (x2)) * ((x1) - (x2)) + ((z1) - (z2)) * ((z1) - (z2)))

#define S_CHUNK_IDX(x, y, z)                                                   \
	((y) + (W2C_COORD(z) + W2C_COORD(x) * CHUNK_SIZE) * WORLD_HEIGHT)

static void random_unit_vector(vec3 out) {
    float z = 2.0f * ((rand()/(float)RAND_MAX) - 0.5f);
    float t = 2.0f * M_PI * (rand()/(float)RAND_MAX);
    float r = sqrtf(1.0f - z*z);
    out[0] = r * cosf(t);
    out[1] = r * sinf(t);
    out[2] = z;
}



void server_world_chunk_destroy(struct server_chunk* sc) {
	assert(sc);

	free(sc->ids);
	free(sc->metadata);
	free(sc->lighting_sky);
	free(sc->lighting_torch);
	free(sc->heightmap);
}

void server_world_create(struct server_world* w, string_t level_name,
						 enum world_dim dimension) {
	assert(w && dimension >= -1 && dimension <= 0);

	dict_server_chunks_init(w->chunks);
	ilist_regions_init(w->loaded_regions_lru);
	string_init_set(w->level_name, level_name);
	w->dimension = dimension;
	w->loaded_regions_length = 0;
}

void server_world_destroy(struct server_world* w) {
	assert(w);

	dict_server_chunks_it_t it;
	dict_server_chunks_it(it, w->chunks);

	while(!dict_server_chunks_end_p(it)) {
		struct server_chunk* sc = &dict_server_chunks_ref(it)->value;
		int64_t id = dict_server_chunks_ref(it)->key;
		server_world_save_chunk_obj(w, false, S_CHUNK_X(id), S_CHUNK_Z(id), sc);
		server_world_chunk_destroy(sc);

		dict_server_chunks_next(it);
	}

	dict_server_chunks_clear(w->chunks);
	string_clear(w->level_name);
}

static bool server_chunk_get_block(void* user, c_coord_t x, w_coord_t y,
								   c_coord_t z, struct block_data* blk) {
	assert(user && blk);
	struct server_chunk* sc = user;

	if(y < 0 || y >= WORLD_HEIGHT)
		return false;

	size_t idx = S_CHUNK_IDX(x, y, z);

	*blk = (struct block_data) {
		.type = sc->ids[idx],
		.metadata = nibble_read(sc->metadata, idx),
		.sky_light = nibble_read(sc->lighting_sky, idx),
		.torch_light = nibble_read(sc->lighting_torch, idx),
	};

	return true;
}

static bool server_world_light_get_block(void* user, w_coord_t x, w_coord_t y,
										 w_coord_t z, struct block_data* blk,
										 uint8_t* height) {
	assert(user);
	struct server_world* w = user;

	if(y < 0 || y >= WORLD_HEIGHT)
		return false;

	struct server_chunk* sc = dict_server_chunks_get(
		w->chunks, S_CHUNK_ID(WCOORD_CHUNK_OFFSET(x), WCOORD_CHUNK_OFFSET(z)));

	if(!sc)
		return false;

	if(blk)
		server_chunk_get_block(sc, W2C_COORD(x), y, W2C_COORD(z), blk);

	if(height)
		*height = sc->heightmap[W2C_COORD(x) + W2C_COORD(z) * CHUNK_SIZE];

	return true;
}

static void server_world_light_set_light(void* user, w_coord_t x, w_coord_t y,
										 w_coord_t z, uint8_t light) {
	assert(user);
	struct server_world* w = user;
	struct server_chunk* sc = dict_server_chunks_get(
		w->chunks, S_CHUNK_ID(WCOORD_CHUNK_OFFSET(x), WCOORD_CHUNK_OFFSET(z)));
	assert(sc);

	size_t idx = S_CHUNK_IDX(x, y, z);
	nibble_write(sc->lighting_sky, idx, light & 0xF);
	nibble_write(sc->lighting_torch, idx, light >> 4);
	sc->modified = true;
}

bool server_world_get_block(struct server_world* w, w_coord_t x, w_coord_t y,
							w_coord_t z, struct block_data* blk) {
	assert(w && blk);

	if(y < 0 || y >= WORLD_HEIGHT)
		return false;

	struct server_chunk* sc = dict_server_chunks_get(
		w->chunks, S_CHUNK_ID(WCOORD_CHUNK_OFFSET(x), WCOORD_CHUNK_OFFSET(z)));

	if(!sc)
		return false;

	size_t idx = S_CHUNK_IDX(W2C_COORD(x), y, W2C_COORD(z));

	*blk = (struct block_data) {
		.type = sc->ids[idx],
		.metadata = nibble_read(sc->metadata, idx),
		.sky_light = nibble_read(sc->lighting_sky, idx),
		.torch_light = nibble_read(sc->lighting_torch, idx),
	};

	return true;
}

bool server_world_set_block(struct server_local* s, w_coord_t x, w_coord_t y, w_coord_t z, struct block_data blk) {
    struct server_world* w = &s->world;
	assert(w);
	if(y < 0 || y >= WORLD_HEIGHT)
		return false;

	struct server_chunk* sc = dict_server_chunks_get(
		w->chunks, S_CHUNK_ID(WCOORD_CHUNK_OFFSET(x), WCOORD_CHUNK_OFFSET(z)));

	if(sc) {
		size_t idx = S_CHUNK_IDX(x, y, z);
		sc->modified = true;
		sc->ids[idx] = blk.type;
		nibble_write(sc->metadata, idx, blk.metadata);

		if(w->dimension != WORLD_DIM_NETHER)
			lighting_heightmap_update(sc->heightmap, W2C_COORD(x), y,
									  W2C_COORD(z), blk.type,
									  server_chunk_get_block, sc);

		lighting_update_at_block(
			(struct world_modification_entry) {
				.x = x,
				.y = y,
				.z = z,
				.blk = blk,
			},
			w->dimension == WORLD_DIM_NETHER, server_world_light_get_block,
			server_world_light_set_light, w);

		clin_rpc_send(&(struct client_rpc) {
			.type = CRPC_SET_BLOCK,
			.payload.set_block.x = x,
			.payload.set_block.y = y,
			.payload.set_block.z = z,
			.payload.set_block.block = blk,
		});
	}

    static const int dx[6] = {  1, -1,  0,  0,  0,  0 };
    static const int dy[6] = {  0,  0,  0,  0,  1, -1 };
    static const int dz[6] = {  0,  0,  1, -1,  0,  0 };

    for (int i = 0; i < 6; i++) {
        w_coord_t nx = x + dx[i];
        w_coord_t ny = y + dy[i];
        w_coord_t nz = z + dz[i];

        struct block_data nb;
        if (!server_world_get_block(w, nx, ny, nz, &nb))
            continue;

        const struct block* b = blocks[nb.type];
        if (b && b->onNeighbourBlockChange) {
            struct block_info info = {
                .block      = &nb,
                .neighbours = NULL,
                .x          = nx,
                .y          = ny,
                .z          = nz
            };
            b->onNeighbourBlockChange(s, &info);
        }
    }

	return sc;
}

bool server_world_furthest_chunk(struct server_world* w, w_coord_t dist,
								 w_coord_t px, w_coord_t pz, w_coord_t* x,
								 w_coord_t* z) {
	assert(w && x && z);

	dict_server_chunks_it_t it;
	dict_server_chunks_it(it, w->chunks);

	w_coord_t furthest_dist2 = -1;
	bool found = false;
	while(!dict_server_chunks_end_p(it)) {
		int64_t id = dict_server_chunks_ref(it)->key;
		w_coord_t d = CHUNK_DIST2(px, S_CHUNK_X(id), pz, S_CHUNK_Z(id));
		dict_server_chunks_next(it);

		if((abs(px - S_CHUNK_X(id)) > dist || abs(pz - S_CHUNK_Z(id)) > dist)
		   && d > furthest_dist2) {
			*x = S_CHUNK_X(id);
			*z = S_CHUNK_Z(id);
			furthest_dist2 = d;
			found = true;
		}
	}

	return found;
}

bool server_world_is_chunk_loaded(struct server_world* w, w_coord_t x,
								  w_coord_t z) {
	assert(w);
	return dict_server_chunks_get(w->chunks, S_CHUNK_ID(x, z)) != NULL;
}

bool server_world_load_chunk(struct server_world* w, w_coord_t x, w_coord_t z,
							 struct server_chunk** sc) {
	assert(w && sc);

	if(server_world_is_chunk_loaded(w, x, z))
		return false;

	ilist_regions_it_t it;
	ilist_regions_it(it, w->loaded_regions_lru);

	while(!ilist_regions_end_p(it)) {
		struct region_archive* ra = ilist_regions_ref(it);

		struct server_chunk tmp = (struct server_chunk) {.modified = false};

		bool chunk_exists;
		if(region_archive_contains(ra, x, z, &chunk_exists)) {
			if(chunk_exists && region_archive_get_blocks(ra, x, z, &tmp)) {
				dict_server_chunks_set_at(w->chunks, S_CHUNK_ID(x, z), tmp);
				*sc = dict_server_chunks_get(w->chunks, S_CHUNK_ID(x, z));
				return true;
			} else {
				return false;
			}
		}

		ilist_regions_next(it);
	}

	return false;
}

void server_world_save_chunk(struct server_world* w, bool erase, w_coord_t x,
							 w_coord_t z) {
	assert(w);
	struct server_chunk* c
		= dict_server_chunks_get(w->chunks, S_CHUNK_ID(x, z));
	if(c)
		server_world_save_chunk_obj(w, erase, x, z, c);
}

void server_world_save_chunk_obj(struct server_world* w, bool erase,
								 w_coord_t x, w_coord_t z,
								 struct server_chunk* c) {
	assert(w && c);

	if(c->modified) {
		//  load region archive into cache
		struct region_archive tmp;
		struct region_archive* ra = server_world_chunk_region(w, x, z);

		if(!ra) {
			if(!region_archive_create_new(&tmp, w->level_name,
										  CHUNK_REGION_COORD(x),
										  CHUNK_REGION_COORD(z), w->dimension))
				return;
			ra = &tmp;
		}

		region_archive_set_blocks(ra, x, z, c);
		c->modified = false;
	}

	if(erase) {
		server_world_chunk_destroy(c);
		dict_server_chunks_erase(w->chunks, S_CHUNK_ID(x, z));
	}
}

bool server_world_disk_has_chunk(struct server_world* w, w_coord_t x,
								 w_coord_t z) {
	struct region_archive* ra = server_world_chunk_region(w, x, z);
	bool chunk_exists;
	return ra ?
		(region_archive_contains(ra, x, z, &chunk_exists) && chunk_exists) :
		false;
}

struct region_archive* server_world_chunk_region(struct server_world* w,
												 w_coord_t x, w_coord_t z) {
	assert(w);

	for(size_t k = 0; k < w->loaded_regions_length; k++) {
		bool chunk_exists;
		if(region_archive_contains(w->loaded_regions + k, x, z,
								   &chunk_exists)) {
			ilist_regions_unlink(w->loaded_regions + k);
			ilist_regions_push_back(w->loaded_regions_lru,
									w->loaded_regions + k);
			return w->loaded_regions + k;
		}
	}

	struct region_archive ra;
	if(!region_archive_create(&ra, w->level_name, CHUNK_REGION_COORD(x),
							  CHUNK_REGION_COORD(z), w->dimension))
		return NULL;

	struct region_archive* lru;
	if(ilist_regions_size(w->loaded_regions_lru) < MAX_REGIONS) {
		assert(w->loaded_regions_length < MAX_REGIONS);
		lru = w->loaded_regions + (w->loaded_regions_length++);
	} else {
		lru = ilist_regions_pop_front(w->loaded_regions_lru);
		region_archive_destroy(lru);
	}

	*lru = ra;
	ilist_regions_push_back(w->loaded_regions_lru, lru);

	return lru;
}


void server_world_tick(struct server_world* w, struct server_local* s) {
    dict_server_chunks_it_t it;
    dict_server_chunks_it(it, w->chunks);

    while (!dict_server_chunks_end_p(it)) {
        struct server_chunk* sc   = &dict_server_chunks_ref(it)->value;
        w_coord_t        baseX    = S_CHUNK_X(dict_server_chunks_ref(it)->key) * CHUNK_SIZE;
        w_coord_t        baseZ    = S_CHUNK_Z(dict_server_chunks_ref(it)->key) * CHUNK_SIZE;

        for (int cx = 0; cx < CHUNK_SIZE; cx++) {
            for (int cz = 0; cz < CHUNK_SIZE; cz++) {
                for (int y = 0; y < WORLD_HEIGHT; y++) {
                    struct block_data blk;
                    if (!server_chunk_get_block(sc, cx, y, cz, &blk))
                        continue;

                    const struct block* b = blocks[blk.type];
                    if (!b || !b->onWorldTick)
                        continue;


                    // determine if we need any neigbour info, only is needed for these types
                    bool needNeighbours =
                        (blk.type == BLOCK_REDSTONE_WIRE) ||
                        (blk.type == BLOCK_REDSTONE_TORCH) ||
						(blk.type == BLOCK_TNT) ||
						(blk.type == BLOCK_WOOD_PRESSURE_PLATE) ||
						(blk.type == BLOCK_STONE_PRESSURE_PLATE)||
						(blk.type == BLOCK_DOOR_WOOD)||
						(blk.type == BLOCK_DOOR_IRON) ||
						(blk.type == BLOCK_RAIL) ||
						(blk.type == BLOCK_POWERED_RAIL) ||
						(blk.type == BLOCK_DETECTOR_RAIL)
						;

                    struct block_data neighbour_data[SIDE_MAX];
                    struct block_data* neigh_ptr = NULL;
                    if (needNeighbours) {

						for (int side = 0; side < SIDE_MAX; ++side) {
							int ox, oy, oz;
							blocks_side_offset((enum side)side, &ox, &oy, &oz);

							w_coord_t nx = baseX + cx + ox;
							w_coord_t ny = y        + oy;
							w_coord_t nz = baseZ + cz + oz;

                            if (!server_world_get_block(&s->world,
                                                        nx, ny, nz,
                                                        &neighbour_data[side]))
							{
                                neighbour_data[side].type        = BLOCK_AIR;
                                neighbour_data[side].metadata    = 0;
                                neighbour_data[side].sky_light   = 0;
                                neighbour_data[side].torch_light = 0;
							}
						}

                        neigh_ptr = neighbour_data;

                    }
                    struct block_info info = {
                        .block      = &blk,
                        .neighbours = neigh_ptr,
                        .x          = baseX + cx,
                        .y          = y,
                        .z          = baseZ + cz
                    };

                    b->onWorldTick(s, &info);
						
					float time = fmodf(daytime_get_time(), 24000.0f);
					if (b->onDay && time >= 0.0f && time < 13000.0f)
						b->onDay(s, &info);

					if (b->onNight && time >= 13000.0f && time < 24000.0f)
						b->onNight(s, &info);	
                    }
                }
            }

        dict_server_chunks_next(it);
    }
}

void server_world_random_tick(struct server_world* w, struct random_gen* g,
							  struct server_local* s, w_coord_t px,
							  w_coord_t pz, w_coord_t dist) {
	assert(w && g && s);

	dict_server_chunks_it_t it;
	dict_server_chunks_it(it, w->chunks);

	while(!dict_server_chunks_end_p(it)) {
		struct server_chunk* sc = &dict_server_chunks_ref(it)->value;
		int64_t id = dict_server_chunks_ref(it)->key;

		if(abs(S_CHUNK_X(id) - px) <= dist && abs(S_CHUNK_Z(id) - pz) <= dist) {
			// 80 random ticks each chunk
			for(int k = 0; k < 80; k++) {
				c_coord_t cx = rand_gen_range(g, 0, CHUNK_SIZE);
				c_coord_t cz = rand_gen_range(g, 0, CHUNK_SIZE);

				w_coord_t x = S_CHUNK_X(id) * CHUNK_SIZE + cx;
				w_coord_t y = rand_gen_range(g, 0, WORLD_HEIGHT);
				w_coord_t z = S_CHUNK_Z(id) * CHUNK_SIZE + cz;

				struct block_data blk;
				if(server_chunk_get_block(sc, cx, y, cz, &blk)
				   && blocks[blk.type] && blocks[blk.type]->onRandomTick) {
					blocks[blk.type]->onRandomTick(
						s,
						&(struct block_info) {.block = &blk,
											  .neighbours = NULL,
											  .x = x,
											  .y = y,
											  .z = z});
				}
			}
		}

		dict_server_chunks_next(it);
	}
}


void server_world_explode(struct server_local *s, vec3 center, float power) {
    struct broken_coord { int x,y,z; };
    struct broken_coord broken[512];
    int bc = 0;

    for (int i = 0; i < EXPLOSION_MAX_RAYS; i++) {
        vec3 dir;
        random_unit_vector(dir);

        vec3 pos = { center[0], center[1], center[2] };
        float rem = power;

        while (rem > 0.0f) {
            pos[0] += dir[0] * EXPLOSION_STEP;
            pos[1] += dir[1] * EXPLOSION_STEP;
            pos[2] += dir[2] * EXPLOSION_STEP;

            int bx = (int)floorf(pos[0]);
            int by = (int)floorf(pos[1]);
            int bz = (int)floorf(pos[2]);

            struct block_data blk;
            if (!server_world_get_block(&s->world, bx, by, bz, &blk))
                break;

            if (blk.type == 0 || blk.type == BLOCK_BEDROCK) {
                rem -= EXPLOSION_STEP;
                continue;
            }

            float hardness = blocks[blk.type]->digging.hardness;
            if ((rem / power) > (rand()/(float)RAND_MAX)) {
                bool seen = false;
                for (int k = 0; k < bc; k++) {
                    if (broken[k].x == bx
                     && broken[k].y == by
                     && broken[k].z == bz) {
                        seen = true;
                        break;
                    }
                }
                if (!seen && bc < 512) {
                    broken[bc].x = bx;
                    broken[bc].y = by;
                    broken[bc].z = bz;
                    bc++;
                }
            }
            rem -= EXPLOSION_STEP + hardness * HARDNESS_SCALE;
        }
    }

    for (int i = 0; i < bc; i++) {
        int bx = broken[i].x, by = broken[i].y, bz = broken[i].z;
        struct block_data old;
        server_world_get_block(&s->world, bx, by, bz, &old);
        server_world_set_block(s, bx, by, bz, (struct block_data){0});
        if (old.type != BLOCK_TNT
            && rand()/(float)RAND_MAX < 0.33f) {
            server_local_spawn_block_drops(
                s,
                &(struct block_info){ .x=bx,.y=by,.z=bz,.block=&old }
            );
        }
    }
}


bool server_world_find_empty_spot_nearby(const float pos[3], const struct server_world *world, float out_pos[3]){
	const float offs[][3] = {
        { 1.0f, 0.0f,  0.0f }, { -1.0f, 0.0f,  0.0f },
        { 0.0f, 0.0f,  1.0f }, {  0.0f, 0.0f, -1.0f },
        { 1.0f, 0.0f,  1.0f }, { -1.0f, 0.0f, -1.0f },
        { 0.0f, 1.0f,  0.0f }, // bovenop als laatste
    };
    int num_offsets = sizeof(offs) / sizeof(offs[0]);
    for (int i = 0; i < num_offsets; ++i) {
        float nx = pos[0] + offs[i][0];
        float ny = pos[1] + offs[i][1];
        float nz = pos[2] + offs[i][2];
        int tx = (int)floorf(nx);
        int ty = (int)floorf(ny);
        int tz = (int)floorf(nz);
        struct block_data bd;
        server_world_get_block(world, tx, ty, tz, &bd);
        if (!blocks[bd.type] || !blocks[bd.type]->getBoundingBox) {
            out_pos[0] = nx; out_pos[1] = ny; out_pos[2] = nz;
            return true;
        }
    }
    return false;
}


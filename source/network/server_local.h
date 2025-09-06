/*
	Copyright (c) 2022 ByteBit/xtreme8000

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

#ifndef SERVER_LOCAL
#define SERVER_LOCAL

#include <m-lib/m-string.h>
#include <stdbool.h>
#include <stddef.h>

#include "../item/inventory.h"
#include "../world.h"
#include "level_archive.h"
#include "server_world.h"
#include "../entity/entity.h"

#define MAX_REGIONS 4
#define MAX_VIEW_DISTANCE 5 // in chunks
#define MAX_HIGH_DETAIL_VIEW_DISTANCE 2
#define MAX_CHUNKS ((MAX_VIEW_DISTANCE * 2 + 2) * (MAX_VIEW_DISTANCE * 2 + 2))
#define MAX_HIGH_DETAIL_CHUNKS ((MAX_HIGH_DETAIL_VIEW_DISTANCE * 2 + 2) * (MAX_HIGH_DETAIL_VIEW_DISTANCE * 2 + 2))
#define MAX_CHESTS 256
#define MAX_CHEST_SLOTS 54
#define MAX_SIGNS 256

#define MAX_OXYGEN 351
#define OXYGEN_THRESHOLD 0

struct complex_block_pos {
	int x, y, z;
};

struct server_local {
	struct random_gen rand_src;
	struct {
		double x, y, z;
		float rx, ry;
		enum world_dim dimension;
		bool has_pos;
		bool finished_loading;
		struct inventory inventory;
		struct inventory* active_inventory;
		short health;
		int oxygen;
		int spawn_x, spawn_y, spawn_z;
		float vel_y, old_vel_y;
		int fall_y;
	} player;
	struct server_world world;
	dict_entity_t entities;
	struct complex_block_pos chest_pos[MAX_CHESTS];
	struct item_data chest_items[MAX_CHESTS][MAX_CHEST_SLOTS];
	struct complex_block_pos sign_pos[MAX_SIGNS];
	char sign_texts[MAX_SIGNS][SIGN_SIZE];
	uint64_t world_time;
	string_t level_name;
	struct level_archive level;
	bool paused;
	ptime_t last_tick;
};

void server_local_create(struct server_local* s);
struct entity* server_local_spawn_minecart(vec3 pos, struct server_local* s);
struct entity* server_local_spawn_item(vec3 pos, struct item_data* it,
									   bool throw, struct server_local* s);
struct entity* server_local_spawn_monster(vec3 pos, int monster_id,
									   struct server_local* s);
void server_local_spawn_block_drops(struct server_local* s,
									struct block_info* blk_info);
void server_local_send_inv_changes(set_inv_slot_t changes,
								   struct inventory* inv, uint8_t window);
void server_local_set_player_health(struct server_local* s, short new_health);
#endif

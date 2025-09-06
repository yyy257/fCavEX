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

#ifndef SERVER_INTERFACE_H
#define SERVER_INTERFACE_H

#include "../world.h"
#include <m-lib/m-string.h>
#include <stdbool.h>

#define MAX_PLAYER_HEALTH 160
#define HEALTH_PER_HEART 16

enum server_rpc_type {
	SRPC_PLAYER_POS,
	SRPC_LOAD_WORLD,
	SRPC_ENTITY_ATTACK,
	SRPC_UNLOAD_WORLD,
	SRPC_HOTBAR_SLOT,
	SRPC_BLOCK_PLACE,
	SRPC_BLOCK_DIG,
	SRPC_WINDOW_CLICK,
	SRPC_WINDOW_CLOSE,
	SRPC_TOGGLE_PAUSE,
};

struct server_rpc {
	enum server_rpc_type type;
	union {
		struct {
			double x, y, z;
			float rx, ry;
			float vel_y;
		} player_pos;
		struct {
			string_t name;
		} load_world;
		struct {
			w_coord_t x, y, z;
			enum side side;
		} block_place;
		struct {
			bool finished;
			w_coord_t x, y, z;
			enum side side;
		} block_dig;
		struct {
			size_t slot;
		} hotbar_slot;
		struct {
			uint8_t window;
			uint8_t slot;
			bool right_click;
			uint16_t action_id;
		} window_click;
		struct {
			uint8_t window;
		} window_close;
		struct {
			uint32_t entity_id;
		} entity_attack;
	} payload;
};

void svin_init(void);
void svin_process_messages(void (*process)(struct server_rpc*, void*),
						   void* user, bool block);
void svin_rpc_send(struct server_rpc* call);

#endif

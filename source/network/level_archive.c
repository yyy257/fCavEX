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

#include <assert.h>

#include "level_archive.h"

bool level_archive_create(struct level_archive* la, string_t filename) {
	assert(la && filename);

	la->modified = false;

	string_init_printf(la->file_name, "%s/level.dat",
					   string_get_cstr(filename));

	la->data = nbt_parse_path(string_get_cstr(la->file_name));

	return la->data;
}

static bool level_archive_read_internal(nbt_node* root,
										struct level_archive_tag tag,
										void* result, size_t length) {
	assert(root && result);
	assert((tag.type == TAG_STRING && length > 0)
		   || (tag.type != TAG_STRING && length == 0));

	nbt_node* node = nbt_find_by_path(root, tag.name);

	if(!node || node->type != tag.type)
		return false;

	switch(tag.type) {
		case TAG_COMPOUND:
		case TAG_LIST: *((nbt_node**)result) = node; return true;
		case TAG_STRING:
			if(length > 0) {
				strncpy(result, node->payload.tag_string, length - 1);
				((char*)result)[length - 1] = 0;
			}
			return true;
		case TAG_LONG:
			*((int64_t*)result) = node->payload.tag_long;
			return true;
		case TAG_INT: *((int32_t*)result) = node->payload.tag_int; return true;
		case TAG_SHORT:
			*((int16_t*)result) = node->payload.tag_short;
			return true;
		case TAG_BYTE: *((int8_t*)result) = node->payload.tag_byte; return true;
		default: return false;
	}
}

bool level_archive_read(struct level_archive* la, struct level_archive_tag tag,
						void* result, size_t length) {
	assert(la && result);
	assert(la->data);
	return level_archive_read_internal(la->data, tag, result, length);
}

static bool level_archive_write_internal(nbt_node* root,
										 struct level_archive_tag tag,
										 void* data) {
	assert(root && data);

	nbt_node* node = nbt_find_by_path(root, tag.name);

	if(!node || node->type != tag.type)
		return false;

	switch(tag.type) {
		case TAG_LONG: node->payload.tag_long = *(int64_t*)data; return true;
		case TAG_INT: node->payload.tag_int = *(int32_t*)data; return true;
		case TAG_SHORT: node->payload.tag_short = *(int16_t*)data; return true;
		case TAG_BYTE: node->payload.tag_byte = *(int8_t*)data; return true;
		default: return false;
	}
}

bool level_archive_write(struct level_archive* la, struct level_archive_tag tag,
						 void* data) {
	assert(la && data);
	assert(la->data);

	if(level_archive_write_internal(la->data, tag, data)) {
		la->modified = true;
		return true;
	}

	return false;
}

bool level_archive_read_inventory(struct level_archive* la,
								  struct inventory* inventory) {
	assert(la && inventory);
	assert(la->data);

	inventory_clear(inventory);

	nbt_node* inv;
	if(!level_archive_read(la, LEVEL_PLAYER_INVENTORY, &inv, 0))
		return false;

	struct list_head* current;
	list_for_each(current, &inv->payload.tag_list->entry) {
		nbt_node* obj = list_entry(current, struct nbt_list, entry)->data;
		if(obj && obj->type == TAG_COMPOUND) {
			uint8_t slot;
			if(!level_archive_read_internal(obj, LEVEL_PLAYER_ITEM_SLOT, &slot,
											0))
				return false;

			if(slot >= 100 && slot < 104) {
				// armor slots
				slot = (3 - (slot - 100)) + INVENTORY_SLOT_ARMOR;
			} else if(slot < INVENTORY_SIZE_HOTBAR) {
				// hotbar slots
				slot += INVENTORY_SLOT_HOTBAR;
			} else if(slot < INVENTORY_SIZE_MAIN + INVENTORY_SIZE_HOTBAR) {
				// main slots
				slot += INVENTORY_SLOT_MAIN - INVENTORY_SIZE_HOTBAR;
			} else {
				// invalid slot number
				return false;
			}

			if(!level_archive_read_internal(obj, LEVEL_PLAYER_ITEM_ID,
											&inventory->items[slot].id, 0))
				return false;

			uint16_t durability16;
			if(!level_archive_read_internal(obj, LEVEL_PLAYER_ITEM_DURABILITY,
											&durability16,
											0))
				return false;
											inventory->items[slot].durability = (uint8_t)durability16;

			if(!level_archive_read_internal(obj, LEVEL_PLAYER_ITEM_COUNT,
											&inventory->items[slot].count, 0))
				return false;
		} else {
			return false;
		}
	}

	return true;
}

bool level_archive_write_inventory(struct level_archive* la,
								   struct inventory* inventory) {
	assert(la && inventory);
	assert(la->data);

	nbt_node* inv;
	if(!level_archive_read(la, LEVEL_PLAYER_INVENTORY, &inv, 0))
		return false;

	nbt_free_list(inv->payload.tag_list);

	inv->payload.tag_list = malloc(sizeof(struct nbt_list));

	if(!inv->payload.tag_list)
		return false;

	inv->payload.tag_list->data = malloc(sizeof(nbt_node));

	if(!inv->payload.tag_list->data) {
		free(inv->payload.tag_list);
		return false;
	}

	inv->payload.tag_list->data->type = TAG_COMPOUND;

	INIT_LIST_HEAD(&inv->payload.tag_list->entry);

	la->modified = true;

	for(int i = 0; i < (int)inventory->capacity; i++) {
		if(inventory->items[i].id == 0)
			continue;

		int slot_translated;

		if(i >= INVENTORY_SLOT_ARMOR
		   && i < INVENTORY_SLOT_ARMOR + INVENTORY_SIZE_ARMOR) {
			slot_translated = (3 - (i - INVENTORY_SLOT_ARMOR)) + 100;
		} else if(i >= INVENTORY_SLOT_HOTBAR
				  && i < INVENTORY_SLOT_HOTBAR + INVENTORY_SIZE_HOTBAR) {
			slot_translated = i - INVENTORY_SLOT_HOTBAR;
		} else if(i >= INVENTORY_SLOT_MAIN
				  && i < INVENTORY_SLOT_MAIN + INVENTORY_SIZE_MAIN) {
			slot_translated = i - INVENTORY_SLOT_MAIN + INVENTORY_SIZE_HOTBAR;
		} else {
			continue;
		}

		nbt_node item_list_nodes[] = {
			{
				.type = TAG_BYTE,
				.name = "Slot",
				.payload.tag_byte = slot_translated,
			},
			{
				.type = TAG_SHORT,
				.name = "id",
				.payload.tag_short = inventory->items[i].id,
			},
			{
				.type = TAG_BYTE,
				.name = "Count",
				.payload.tag_byte = inventory->items[i].count,
			},
			{
				.type = TAG_SHORT,
				.name = "Damage",
				.payload.tag_short = (uint16_t)inventory->items[i].durability,
			},
		};

		struct nbt_list item_list_sentinel = (struct nbt_list) {
			.data = NULL,
		};

		INIT_LIST_HEAD(&item_list_sentinel.entry);

		struct nbt_list
			item_list[sizeof(item_list_nodes) / sizeof(*item_list_nodes)];

		for(size_t k = 0; k < sizeof(item_list) / sizeof(*item_list); k++) {
			item_list[k].data = item_list_nodes + k;
			list_add_tail(&item_list[k].entry, &item_list_sentinel.entry);
		}

		struct nbt_list* node = malloc(sizeof(struct nbt_list));

		if(!node)
			return false;

		nbt_node* item = nbt_clone(&(nbt_node) {
			.type = TAG_COMPOUND,
			.name = NULL,
			.payload.tag_compound = &item_list_sentinel,
		});

		if(!item) {
			free(node);
			return false;
		}

		node->data = item;

		list_add_tail(&node->entry, &inv->payload.tag_list->entry);
	}

	return true;
}

// could use float* instead of vec3, but not sure of possible alignment for vec3
// (right now this means we can read three values at most)
static bool read_vector(nbt_node* node, nbt_type type, vec3 result,
						size_t amount) {
	assert(node && node->type == TAG_LIST && result && amount <= 3);

	size_t k = 0;
	struct list_head* current;
	list_for_each(current, &node->payload.tag_list->entry) {
		nbt_node* obj = list_entry(current, struct nbt_list, entry)->data;
		if(obj && obj->type == type && k < amount) {
			switch(type) {
				case TAG_DOUBLE: result[k++] = obj->payload.tag_double; break;
				case TAG_FLOAT: result[k++] = obj->payload.tag_float; break;
				default: return false;
			}
		} else {
			return false;
		}
	}

	return k == amount;
}

static bool write_vector(nbt_node* node, nbt_type type, vec3 data,
						 size_t amount) {
	assert(node && node->type == TAG_LIST && data && amount <= 3);

	size_t k = 0;
	struct list_head* current;
	list_for_each(current, &node->payload.tag_list->entry) {
		nbt_node* obj = list_entry(current, struct nbt_list, entry)->data;
		if(obj && obj->type == type && k < amount) {
			switch(type) {
				case TAG_DOUBLE: obj->payload.tag_double = data[k++]; break;
				case TAG_FLOAT: obj->payload.tag_float = data[k++]; break;
				default: return false;
			}
		} else {
			return false;
		}
	}

	return k == amount;
}

bool level_archive_write_player(struct level_archive* la, vec3 position,
								vec2 rotation, vec3 velocity,
								enum world_dim dimension) {
	assert(la && la->data);

	nbt_node* pos;
	if(!level_archive_read(la, LEVEL_PLAYER_POSITION, &pos, 0))
		return false;

	nbt_node* vel;
	if(!level_archive_read(la, LEVEL_PLAYER_VELOCITY, &vel, 0))
		return false;

	nbt_node* rot;
	if(!level_archive_read(la, LEVEL_PLAYER_ROTATION, &rot, 0))
		return false;

	int32_t dim = dimension;
	if(!level_archive_write(la, LEVEL_PLAYER_DIMENSION, &dim))
		return false;

	if(position && !write_vector(pos, TAG_DOUBLE, position, 3))
		return false;

	if(velocity && !write_vector(vel, TAG_DOUBLE, velocity, 3))
		return false;

	if(rotation
	   && !write_vector(rot, TAG_FLOAT, (vec3) {rotation[0], rotation[1], 0.0F},
						2))
		return false;

	la->modified = true;

	return true;
}

bool level_archive_read_player(struct level_archive* la, vec3 position,
							   vec2 rotation, vec3 velocity,
							   enum world_dim* dimension) {
	assert(la && la->data);

	nbt_node* pos;
	if(!level_archive_read(la, LEVEL_PLAYER_POSITION, &pos, 0))
		return false;

	nbt_node* vel;
	if(!level_archive_read(la, LEVEL_PLAYER_VELOCITY, &vel, 0))
		return false;

	nbt_node* rot;
	if(!level_archive_read(la, LEVEL_PLAYER_ROTATION, &rot, 0))
		return false;

	int32_t dim;
	if(dimension && !level_archive_read(la, LEVEL_PLAYER_DIMENSION, &dim, 0))
		return false;

	if(position && !read_vector(pos, TAG_DOUBLE, position, 3))
		return false;

	if(velocity && !read_vector(vel, TAG_DOUBLE, velocity, 3))
		return false;

	if(rotation) {
		vec3 tmp;
		if(!read_vector(rot, TAG_FLOAT, tmp, 2))
			return false;

		rotation[0] = tmp[0];
		rotation[1] = tmp[1];
	}

	// ensures output is valid dimension
	if(dimension)
		*dimension = (dim == 0) ? WORLD_DIM_OVERWORLD : WORLD_DIM_NETHER;

	return true;
}

void level_archive_destroy(struct level_archive* la) {
	assert(la && la->data);

	if(la->modified) {
		FILE* f = fopen(string_get_cstr(la->file_name), "wb");

		if(f) {
			nbt_dump_file(la->data, f, STRAT_GZIP);
			fclose(f);
		}
	}

	string_clear(la->file_name);
	nbt_free(la->data);
}

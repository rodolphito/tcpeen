#ifndef HB_MAP_H
#define HB_MAP_H


#include <stdint.h>


typedef struct hb_map_s {
	void *priv;
	uint64_t capacity;
} hb_map_t;


int hb_map_setup(hb_map_t *map, uint64_t capacity);
void hb_map_cleanup(hb_map_t *map);
int hb_map_get(hb_map_t *map, void *key, void **out_value);
int hb_map_set(hb_map_t *map, void *key, void *value);
int hb_map_remove(hb_map_t *map, void *key);
void hb_map_clear(hb_map_t *map);


#endif
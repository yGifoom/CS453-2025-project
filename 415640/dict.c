#include "dict.h"
#include "stdio.h"
#include "macros.h"

static inline uint32_t hash_pointer(void *ptr) {
	//printf("key is %p\n", ptr);fflush(stdout);
	// Shift out alignment bits to get meaningful variation
	uintptr_t val = (uintptr_t)ptr >> 3;
	// Mix the bits to distribute values better in smaller ranges
	val ^= val >> 16;
	val *= 0x85ebca6b;
	val ^= val >> 13;
	val *= 0xc2b2ae35;
	val ^= val >> 16;
	//printf("key %p hashed to value %u\n", ptr, (uint32_t)val);fflush(stdout);
	return (uint32_t)val;
}

struct keynode *keynode_new(void *k) {
	struct keynode *node = malloc(sizeof(struct keynode));
	node->key = k;
	node->next = 0;
	node->value = NULL;
	return node;
}

void keynode_delete(struct keynode *node) {
	if (node->next) keynode_delete(node->next);
	free(node);
}

struct dictionary* dic_new(int initial_size) {
	struct dictionary* dic = malloc(sizeof(struct dictionary));
	if (initial_size == 0) initial_size = 1024;
	dic->length = initial_size;
	dic->count = 0;
	dic->table = calloc(sizeof(struct keynode*), initial_size);
	dic->growth_treshold = 2.0;
	dic->growth_factor = 10;
	return dic;
}

void dic_delete(struct dictionary* dic) {
	for (int i = 0; i < dic->length; i++) {
		if (dic->table[i])
			keynode_delete(dic->table[i]);
	}
	free(dic->table);
	dic->table = 0;
	free(dic);
}

void dic_reinsert_when_resizing(struct dictionary* dic, struct keynode *k2) {
	int n = hash_pointer(k2->key) % dic->length;
	if (dic->table[n] == 0) {
		dic->table[n] = k2;
		dic->value = &dic->table[n]->value;
		return;
	}
	struct keynode *k = dic->table[n];
	k2->next = k;
	dic->table[n] = k2;
	dic->value = &k2->value;
}

void dic_resize(struct dictionary* dic, int newsize) {
	int o = dic->length;
	struct keynode **old = dic->table;
	dic->table = calloc(sizeof(struct keynode*), newsize);
	dic->length = newsize;
	for (int i = 0; i < o; i++) {
		struct keynode *k = old[i];
		while (k) {
			struct keynode *next = k->next;
			k->next = 0;
			dic_reinsert_when_resizing(dic, k);
			k = next;
		}
	}
	free(old);
}

int dic_add(struct dictionary* dic, void *key, int unused(keyn)) {
	int n = hash_pointer(key) % dic->length;
	if (dic->table[n] == 0) {
		double f = (double)dic->count / (double)dic->length;
		if (f > dic->growth_treshold) {
			dic_resize(dic, dic->length * dic->growth_factor);
			return dic_add(dic, key, keyn);
		}
		dic->table[n] = keynode_new(key);
		dic->value = &dic->table[n]->value;
		dic->count++;
		return 0;
	}
	struct keynode *k = dic->table[n];
	while (k) {
		if (k->key == key) {
			dic->value = &k->value;
			return 1;
		}
		k = k->next;
	}
	dic->count++;
	struct keynode *k2 = keynode_new(key);
	k2->next = dic->table[n];
	dic->table[n] = k2;
	dic->value = &k2->value;
	return 0;
}

int dic_find(struct dictionary* dic, void *key, int unused(keyn)) {
	int n = hash_pointer(key) % dic->length;
    #if defined(__MINGW32__) || defined(__MINGW64__)
	__builtin_prefetch(gc->table[n]);
    #endif
    
    #if defined(_WIN32) || defined(_WIN64)
    _mm_prefetch((char*)gc->table[n], _MM_HINT_T0);
    #endif
	struct keynode *k = dic->table[n];
	if (!k) return 0;
	while (k) {
		if (k->key == key) {
			dic->value = &k->value;
			return 1;
		}
		k = k->next;
	}
	return 0;
}

void dic_forEach(struct dictionary* dic, enumFunc f, void *user) {
	for (int i = 0; i < dic->length; i++) {
		if (dic->table[i] != 0) {
			struct keynode *k = dic->table[i];
			while (k) {
				if (!f(k->key, k->len, &k->value, user)) return;
				k = k->next;
			}
		}
	}
}
#undef hash_func
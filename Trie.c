#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "Trie.h"
#include "lib.h"

trie_node_t *trie_create_node(void) {
	/* Alloc memory for the new node. */
	trie_node_t *new_node = calloc(1, sizeof(trie_node_t));

	return new_node;
}

trie_t *trie_create(void) {
	/* Alloc memory for the trie. */
	trie_t *trie = calloc(1, sizeof(trie_t));

	/* Initialize data. */
	trie->root = trie_create_node();

	return trie;
}

void trie_insert(trie_t *trie, uint32_t ip_prefix, struct route_table_entry info) {
	int i;
	trie_node_t *curr_node = trie->root;
	/* Build the path for our key in the trie and at final instert the new_elem. */
	for (i = 0; i < 32 && info.mask; i++) {
		if (ip_prefix & (1 << (31 - i))) {
			if (curr_node->right == NULL) {
				curr_node->right = trie_create_node();
			}
			curr_node = curr_node->right;
		} else {
			if (curr_node->left == NULL) {
				curr_node->left = trie_create_node();
			}
			curr_node = curr_node->left;
		}
		info.mask = info.mask << 1;
	}
	curr_node->end_of_ip = 1;
	curr_node->info = malloc(sizeof(struct route_table_entry));
	curr_node->info->interface = info.interface;
	curr_node->info->next_hop = info.next_hop;
}

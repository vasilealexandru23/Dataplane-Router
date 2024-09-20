#ifndef TRIE_H_
#define TRIE_H_

#include <errno.h>
#include <arpa/inet.h>

typedef struct trie_node_t trie_node_t;
struct trie_node_t {
	struct route_table_entry *info;

	int end_of_ip;

	trie_node_t *left, *right;
};

typedef struct trie_t trie_t;
struct trie_t {
	trie_node_t *root;
};

/* Function that allocates memory for a node in the trie. */
trie_node_t *trie_create_node(void);

/* Function that allocates memory for a trie. */
trie_t *trie_create(void);

/* Function that inserts in a trie a pair <key, value>. */
void trie_insert(trie_t *trie, uint32_t ip_prefix, struct route_table_entry info);

#endif /* TRIE_H_ */
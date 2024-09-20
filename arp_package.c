#include "arp_package.h"
#include "lib.h"
#include "Trie.h"

struct route_table_entry *get_best_route(uint32_t ip, trie_t *trie) {
	trie_node_t *curr_node = trie->root;
	/* Search for the key in the trie. */
	struct route_table_entry *candidate = NULL;
	for (int i = 0; i < 32; i++) {
		if (curr_node->end_of_ip == 1) {
			candidate = curr_node->info;
		}
		if (ip & (1 << (31 - i))) {
			if (!curr_node->right) {
				break;	
			}
			curr_node = curr_node->right;
		} else {
			if (!curr_node->left) {
				break;
			}
			curr_node = curr_node->left;
		}
	}

	return candidate;
}

struct arp_table_entry *get_mac_entry_list(uint32_t given_ip, list arp_cache_table) {
	list curr_node = arp_cache_table;
	while (curr_node != NULL) {
		struct arp_table_entry *curr_entry = (struct arp_table_entry *)curr_node->element;
		if (curr_entry->ip == given_ip) {
			return curr_entry;
		}
		curr_node = curr_node->next;
	}

	return NULL;
}

void send_reply_arp(char *packet, size_t packet_len, trie_t *rtable) {
	ether_header *eth_hdr = extract_ether_hdr(packet);
	arp_header *arp_hdr = extract_arp_hdr(packet);

	struct route_table_entry *next_node = get_best_route(ntohl(arp_hdr->tpa), rtable);
	if (next_node == NULL) {
		printf("no found\n");
		return;
	}

	/* Update the MAC source address. */
	get_interface_mac(next_node->interface, eth_hdr->ether_shost);

	/* Copy to dest mac. */
	memcpy(eth_hdr->ether_dhost, arp_hdr->tha, MAC_ADDR_LEN);

	/* Send the package to the next interface. */ 
	send_to_link(next_node->interface, packet, packet_len);
}

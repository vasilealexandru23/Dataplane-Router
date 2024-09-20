#ifndef ARP_PACKAGE_H_
#define ARP_PACKAGE_H_

#include "list.h"
#include "utils.h"
#include "string.h"
#include "Trie.h"

struct route_table_entry *get_best_route(uint32_t ip_dest, trie_t *rtable); 

struct arp_table_entry *get_mac_entry_list(uint32_t given_ip, list arp_cache_table);

void send_reply_arp(char *packet, size_t packet_len, trie_t *rtable);

#endif /* ARP_PACKAGE_H_ */

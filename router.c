#include "include/lib.h"
#include "include/list.h"
#include "include/queue.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "include/utils.h"
#include "arp_package.h"

#define ETHERTYPE_IP		0x0800  /* IP protocol */
#define ETHERTYPE_ARP		0x0806  /* ARP protocol */
#define MAX_ENTRIES 		100000  /* Max number of entries */
#define MAC_ADDR_LEN        6 		/* Mac address length. */

queue packets_queue = NULL;

struct queue_elem {
	char *packet;
	size_t packet_len;
};

void send_forward(char *packet, size_t packet_len, list arp_cache_table,
			trie_t *rtable) {
	ether_header *eth_hdr = extract_ether_hdr(packet);
	struct iphdr *ip_hdr = extract_ip_hdr(packet);
	struct route_table_entry *next_node = get_best_route(ntohl(ip_hdr->daddr), rtable);

	/* Check if there is a next node to go. */
	if (next_node == NULL) {
		struct icmphdr *icmp_hdr = extract_icmp_hdr(packet);

		size_t packet_structure = sizeof(struct ether_header) + sizeof(struct iphdr) + sizeof(struct icmphdr);
		char ICMP_TTL[packet_structure + sizeof(struct iphdr) + 8];

		/* Copy old ip header and first 64 bits from payload. */
		memcpy(ICMP_TTL + packet_structure, (packet + sizeof(struct ether_header)), sizeof(struct iphdr) + 8);
			
		ip_hdr->protocol = 1;
		ip_hdr->tot_len = htons(packet_structure + sizeof(struct iphdr) + 8 - sizeof(struct ether_header));
			
		/* Swap MAC address. */
		swap_mac_addr(eth_hdr->ether_dhost, eth_hdr->ether_shost);

		/* Swap IP address. */
		swap_ip_addr(&ip_hdr->saddr, &ip_hdr->daddr);

		/* Set type and code. */
		icmp_hdr->type = 3;
		icmp_hdr->code = 0;

		/* Recalculate the icmp checksum. */
		icmp_hdr->checksum = 0;
		icmp_hdr->checksum = htons(checksum((uint16_t *)icmp_hdr, sizeof(struct icmphdr) + sizeof(struct iphdr) + 8));

		memcpy(ICMP_TTL, packet, packet_structure);

		/* Send back package. */
		send_forward(ICMP_TTL, sizeof(ICMP_TTL), arp_cache_table, rtable);
		return;
	}

	/* Update the MAC destination address. */
	struct arp_table_entry *next_mac = get_mac_entry_list(next_node->next_hop, arp_cache_table);

	/* Check if there is the next interface. */
	if (next_mac == NULL) {
		/* Send an ARP to request MAC from next_hop. */
		size_t size_arp = sizeof(struct ether_header) + sizeof(struct arp_header);
		char *send_arp_request = malloc(size_arp * sizeof(char));

		/* Set ETHER header. */
		struct ether_header *ether_hdr = (struct ether_header *)send_arp_request;
		get_interface_mac(next_node->interface, ether_hdr->ether_shost);

		make_mac_broadcast(ether_hdr->ether_dhost);

		ether_hdr->ether_type = htons(ETHERTYPE_ARP);

		/* Set ARP header. */
		struct arp_header *arp_hdr = extract_arp_hdr(send_arp_request);
		arp_hdr->htype = htons(1);
		arp_hdr->ptype = htons(ETHERTYPE_IP);
		arp_hdr->hlen = 6;
		arp_hdr->plen = 4;
		arp_hdr->op = htons(1);
		get_interface_mac(next_node->interface, arp_hdr->sha);
		arp_hdr->spa = inet_addr(get_interface_ip(next_node->interface));
		memset(arp_hdr->tha, 0x0, MAC_ADDR_LEN);

		arp_hdr->tpa = next_node->next_hop;

		struct queue_elem *new_elem = malloc(sizeof(struct queue_elem));
		new_elem->packet = malloc(packet_len);
		memcpy(new_elem->packet, packet, packet_len);

		new_elem->packet_len = packet_len;
		queue_enq(packets_queue, new_elem);

		/* Trimit mai departe broadcastul. */
		send_to_link(next_node->interface, send_arp_request, size_arp);
		return;
	}
	/* Decrement time to live and update checksum. */
	(ip_hdr->ttl)--;

	ip_hdr->check = htons(checksum((uint16_t *) ip_hdr, sizeof(struct iphdr)));

	/* Update the MAC source address. */
	get_interface_mac(next_node->interface, eth_hdr->ether_shost);

	memcpy(eth_hdr->ether_dhost, next_mac->mac, MAC_ADDR_LEN);

	/* Send the package to the next interface. */ 
	send_to_link(next_node->interface, packet, packet_len);
}

/* ICMP echo. */
void send_icmp_reply(char *packet, size_t packet_len, list arp_cache_table,
			trie_t *rtable) {
	ether_header *eth_hdr = extract_ether_hdr(packet);
	iphdr *ip_hdr = extract_ip_hdr(packet);
	icmphdr *icmp_hdr = extract_icmp_hdr(packet);

	/* Swap MAC address. */
	swap_mac_addr(eth_hdr->ether_dhost, eth_hdr->ether_shost);

	/* Swap IP address. */
	swap_ip_addr(&ip_hdr->daddr, &ip_hdr->saddr);

	/* Set type and code. */
	icmp_hdr->type = 0;
	icmp_hdr->code = 0;

	/* Recalculate the icmp checksum. */
	icmp_hdr->checksum = 0;
	icmp_hdr->checksum = htons(checksum((uint16_t *)icmp_hdr, sizeof(icmphdr)));

	/* Send back package. */
	send_forward(packet, packet_len, arp_cache_table, rtable);
}

void read_route_table(const char *path, trie_t *rtable) {
	FILE *fp = fopen(path, "r");
	int i;
	char *p, line[64];
	struct route_table_entry r_entry;

	while (fgets(line, sizeof(line), fp) != NULL) {
		p = strtok(line, " .");
		i = 0;
		while (p != NULL) {
			if (i < 4)
				*(((unsigned char *)&(r_entry.prefix))  + i % 4) = (unsigned char)atoi(p);

			if (i >= 4 && i < 8)
				*(((unsigned char *)&(r_entry.next_hop))  + i % 4) = atoi(p);

			if (i >= 8 && i < 12)
				*(((unsigned char *)&(r_entry.mask))  + i % 4) = atoi(p);

			if (i == 12) {
				r_entry.interface = atoi(p);

				r_entry.prefix = ntohl(r_entry.prefix);
				r_entry.mask = ntohl(r_entry.mask);
				trie_insert(rtable, r_entry.prefix, r_entry);
			}
			p = strtok(NULL, " .");
			i++;
		}
	}
}

int main(int argc, char *argv[]) {
	char packet[MAX_PACKET_LEN];
	size_t packet_len;

	/* Routing table */
	trie_t *rtable = trie_create();
	read_route_table(argv[1], rtable);

	packets_queue = queue_create();

	/* Mac table */
	list arp_cache_table = NULL;

	// Do not modify this line
	init(argc - 2, argv + 2);

	while (1) {
		int interface;

		interface = recv_from_any_link(packet, &packet_len);
		DIE(interface < 0, "recv_from_any_links");

		struct ether_header *eth_hdr = (struct ether_header *) packet;

		/* Check if the length of the packet is too small. */
		if (packet_len < sizeof(struct ether_header)) {
			continue;
		}

		/* Check the ether type for IPv4 and ARP. */
		if (eth_hdr->ether_type != htons(ETHERTYPE_IP) &&
				eth_hdr->ether_type != htons(ETHERTYPE_ARP)) {
			printf("Only IPv4 and ARP packets accepted.\n");
			continue;
		}

		/* Check for ARP package. */
		if (eth_hdr->ether_type == htons(ETHERTYPE_ARP)) {
			struct arp_header *arp_hdr = (struct arp_header *)(packet + sizeof(struct ether_header));
			/* Need to reply with my mac address. */
			if (arp_hdr->op == htons(1)) {
				/* Change code to reply identifier. */
				arp_hdr->op = htons(2);

				/* Change MAC addresses. */
				memcpy(arp_hdr->tha, arp_hdr->sha, MAC_ADDR_LEN * sizeof(uint8_t));
				get_interface_mac(interface, arp_hdr->sha);

				/* Change IP addresses. */
				uint32_t cpy_ip_d = arp_hdr->tpa;
				arp_hdr->tpa = arp_hdr->spa;
				arp_hdr->spa = cpy_ip_d;

				send_reply_arp(packet, packet_len, rtable);
				continue;
			} else if (arp_hdr->op == htons(2)) {
				/* I have an ARP reply from someone. Check in queue. */
				if (queue_empty(packets_queue)) {
					continue;
				}
				struct queue_elem *my_elem = (struct queue_elem *)queue_deq(packets_queue);

				/* Add in my list of arps-ips. */	
				struct arp_table_entry *new_entry = malloc(sizeof(struct arp_table_entry));
				new_entry->ip = arp_hdr->spa;
				memcpy(new_entry->mac, arp_hdr->sha, MAC_ADDR_LEN);
				arp_cache_table = cons(new_entry, arp_cache_table);

				/* Resend the package. */
				send_forward(my_elem->packet, my_elem->packet_len, arp_cache_table, rtable);
				continue;
			}
		}

		struct iphdr *ip_hdr = (struct iphdr *)(packet + sizeof(struct ether_header));
		uint16_t old_sum = ip_hdr->check;
		ip_hdr->check = 0;

		/* Check if checksum is correct. */
		if (checksum((uint16_t *) ip_hdr, sizeof(struct iphdr)) != ntohs(old_sum)) {
			printf("Check-sum failed.\n");
			continue;
		}

		/* Check the time to live. */
		if (ip_hdr->ttl <= 1) {
			struct icmphdr *icmp_hdr = (struct icmphdr *)(packet
					+ sizeof(struct ether_header) + sizeof(struct iphdr));

			size_t packet_structure = sizeof(struct ether_header) + sizeof(struct iphdr) + sizeof(struct icmphdr);
			char ICMP_TTL[packet_structure + sizeof(struct iphdr) + 8];

			/* Copy old ip header and first 64 bits from payload. */
			memcpy(ICMP_TTL + packet_structure, (packet + sizeof(struct ether_header)), sizeof(struct iphdr) + 8);

			ip_hdr->ttl = 64;
			ip_hdr->protocol = 1;
			ip_hdr->tot_len = htons(packet_structure + sizeof(struct iphdr) + 8 - sizeof(struct ether_header));

			/* Swap MAC address. */
			swap_mac_addr(eth_hdr->ether_dhost, eth_hdr->ether_shost);

			/* Swap IP address. */
			swap_ip_addr(&ip_hdr->daddr, &ip_hdr->saddr);

			/* Set type and code. */
			icmp_hdr->type = 11;
			icmp_hdr->code = 0;

			/* Recalculate the icmp checksum. */
			icmp_hdr->checksum = 0;
			icmp_hdr->checksum = htons(checksum((uint16_t *)icmp_hdr, sizeof(struct icmphdr) + sizeof(struct iphdr) + 8));

			memcpy(ICMP_TTL, packet, packet_structure);

			/* Send back package. */
			send_forward(ICMP_TTL, sizeof(ICMP_TTL), arp_cache_table, rtable);
			continue;
		}

		if (ip_hdr->protocol == 1) {
			/* Extract icmp header. */
			struct icmphdr *icmp_hdr = extract_icmp_hdr(packet);
			if (icmp_hdr->type == 8 && icmp_hdr->code == 0) {
				if (inet_addr(get_interface_ip(interface)) == ip_hdr->daddr) {
					send_icmp_reply(packet, packet_len, arp_cache_table, rtable);
					continue;
				}
			}
		}

		send_forward(packet, packet_len, arp_cache_table, rtable);
	}

	return 0;
}

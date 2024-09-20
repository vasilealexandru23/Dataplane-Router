#ifndef UTILS_H_
#define UTILS_H_

#include <arpa/inet.h>
#include "protocols.h"

#define MAC_ADDR_LEN 6

void swap_ip_addr(uint32_t *ip_addr1, uint32_t *ip_addr2);

void swap_mac_addr(uint8_t *mac_addr1, uint8_t *mac_addr2);

ether_header *extract_ether_hdr(char *packet);

iphdr *extract_ip_hdr(char *packet);

icmphdr *extract_icmp_hdr(char *packet);

arp_header *extract_arp_hdr(char *packet);

int check_broadcast(uint8_t *mac_addr);

void make_mac_broadcast(uint8_t *mac_address);

#endif /* UTILS_H_ */

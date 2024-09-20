#include "include/utils.h"
#include <string.h>

void swap_ip_addr(uint32_t *ip_addr1, uint32_t *ip_addr2) {
    uint32_t ip_temp = *ip_addr1;
    *ip_addr1 = *ip_addr2;
    *ip_addr2 = ip_temp;
}

void swap_mac_addr(uint8_t *mac_addr1, uint8_t *mac_addr2) {
    uint8_t mac_temp[MAC_ADDR_LEN];
	memcpy(mac_temp, mac_addr1, MAC_ADDR_LEN);
	memcpy(mac_addr1, mac_addr2, MAC_ADDR_LEN);
	memcpy(mac_addr2, mac_temp, MAC_ADDR_LEN);
}

ether_header *extract_ether_hdr(char *packet) {
    return (ether_header *)packet;
}

iphdr *extract_ip_hdr(char *packet) {
    return (iphdr *)(packet + sizeof(ether_header));
}

icmphdr *extract_icmp_hdr(char *packet) {
    return (icmphdr *)(packet + sizeof(ether_header) + sizeof(iphdr));
}

arp_header *extract_arp_hdr(char *packet) {
    return (arp_header *)(packet + sizeof(ether_header));
}

int check_broadcast(uint8_t *mac_addr) {
	for (int i = 0; i < MAC_ADDR_LEN; ++i) {
		if (mac_addr[i] != 0xFF) {
			return 0;
		}
	}

	return 1;
}

void make_mac_broadcast(uint8_t *mac_address) {
	memset(mac_address, 0xff, MAC_ADDR_LEN);
}

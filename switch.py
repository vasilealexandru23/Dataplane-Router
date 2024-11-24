#!/usr/bin/python3
import sys
import struct
import wrapper
import threading
import time
from wrapper import recv_from_any_link, send_to_link, get_switch_mac, get_interface_name
import binascii

own_bridge_id = 0
root_bridge_id = 0
root_path_cost = 0
root_port = 0
ports_type = {}
ports_state = {}
interfaces = []

def parse_ethernet_header(data):
    # Unpack the header fields from the byte array
    #dest_mac, src_mac, ethertype = struct.unpack('!6s6sH', data[:14])
    dest_mac = data[0:6]
    src_mac = data[6:12]
    
    # Extract ethertype. Under 802.1Q, this may be the bytes from the VLAN TAG
    ether_type = (data[12] << 8) + data[13]

    vlan_id = -1
    # Check for VLAN tag (0x8100 in network byte order is b'\x81\x00')
    if ether_type == 0x8200:
        vlan_tci = int.from_bytes(data[14:16], byteorder='big')
        vlan_id = vlan_tci & 0x0FFF  # extract the 12-bit VLAN ID
        ether_type = (data[16] << 8) + data[17]

    return dest_mac, src_mac, ether_type, vlan_id

def create_vlan_tag(vlan_id):
    # 0x8100 for the Ethertype for 802.1Q
    # vlan_id & 0x0FFF ensures that only the last 12 bits are used
    return struct.pack('!H', 0x8200) + struct.pack('!H', vlan_id & 0x0FFF)

def init_switch(switch_id):
    global ports_state, ports_type, own_bridge_id, root_bridge_id, root_path_cost

    # Open config file and save data
    with open('configs/switch' + switch_id + '.cfg') as file:
        # Extract priority
        own_bridge_id = int(file.readline().strip())

        # Store the type of ports
        for line in file:
            parts = line.strip().split()
            ports_type[parts[0]] = parts[1]
            if parts[1] == "T":
                ports_state[parts[0]] = "BLOCKED"

    root_bridge_id = own_bridge_id
    root_path_cost = 0

    if own_bridge_id == root_bridge_id:
        for i in interfaces:
            ports_state[get_interface_name(i)] = "DESIGNATED"

def foward_packet(send_port, vlan_id, recv_port, vlan_tagged_length,
           vlan_tagged_packet, vlan_untagged_length, vlan_untagged_packet):
    if ports_type[get_interface_name(send_port)] == 'T' and ports_state[get_interface_name(send_port)] != "BLOCKED":
        # Send to another switch with VLAN tag (802.1Q)
        send_to_link(send_port, vlan_tagged_length, vlan_tagged_packet)
    else:
        # Send to access point without VLAN tag (802.1Q)
        if (vlan_id == -1 and ports_type[get_interface_name(send_port)] == str(recv_port)) or \
            (vlan_id != -1 and ports_type[get_interface_name(send_port)] == str(vlan_id)):
            send_to_link(send_port, vlan_untagged_length, vlan_untagged_packet)

def create_bpdu():
    dest_mac =  binascii.unhexlify("01:80:c2:00:00:00".replace(':', ''))
    macs = dest_mac + get_switch_mac()

    # Add DSAP = 0x42, SSAP = 0x42, Control = 0x03
    llc_length_header = struct.pack("!H3b", 38, 0x42, 0x42, 0x03)

    # Add my switch data (! - big endian, Q - 64 bits, I -32 bits) + padding to 35
    bpdu = struct.pack("!QIQ", root_bridge_id, root_path_cost, own_bridge_id) + bytes(15)

    data = macs + llc_length_header + bpdu

    return data

def send_bdpu_every_sec():
    while root_bridge_id == own_bridge_id:
        bpdu = create_bpdu()

        # Send BDPU on all trunk ports
        for interface in interfaces:
            if ports_type[get_interface_name(interface)] == 'T':
                send_to_link(interface, len(bpdu), bpdu)

        time.sleep(1)

def recv_bpdu(data, interface):
    global own_bridge_id, root_bridge_id, root_path_cost, root_port, ports_state
    port = get_interface_name(interface)
    bpdu_root_bridge, bpdu_root_cost, bpdu_bridge = struct.unpack("!QIQ", data[17:37])

    if bpdu_root_bridge < root_bridge_id:
        root_path_cost = bpdu_root_cost + 10
        root_port = port
        ports_state[root_port] = "ROOT"

        if own_bridge_id == root_bridge_id:
            for i in ports_type.keys():
                if ports_type[i] == "T" and i != root_port:
                    ports_state[i] = "BLOCKED"

        # Update root bridge ID
        root_bridge_id = bpdu_root_bridge

        bpdu_packet = create_bpdu()

        for i in interfaces:
            i_name = get_interface_name(i)
            if ports_type[i_name] == 'T' and ports_state[i_name] != "BLOCKED" and i_name != root_port:
                send_to_link(i, len(bpdu_packet), bpdu_packet)

    elif bpdu_root_bridge == root_bridge_id:
        if port == root_port and bpdu_root_cost + 10 < root_path_cost:
            root_path_cost = bpdu_root_cost + 10
        
        elif port != root_port and bpdu_root_cost > root_path_cost:
            ports_state[port] = "DESIGNATED"

    elif bpdu_bridge == own_bridge_id:
        ports_state[port] = "BLOCKED"

    if own_bridge_id == root_bridge_id:
        for i in ports_state.keys():
            ports_state[i] = "DESIGNATED"

def main():
    global interfaces
    switch_id = sys.argv[1]

    num_interfaces = wrapper.init(sys.argv[2:])
    interfaces = range(0, num_interfaces)

    # Extract data and init switch
    init_switch(switch_id)

    # Create and start a new thread that deals with sending BDPU
    t = threading.Thread(target=send_bdpu_every_sec)
    t.start()

    # Cam table
    CAM = {}

    while True:
        interface, data, length = recv_from_any_link()

        dest_mac, src_mac, ethertype, vlan_id = parse_ethernet_header(data)

        dest_mac = ':'.join(f'{b:02x}' for b in dest_mac)
        src_mac = ':'.join(f'{b:02x}' for b in src_mac)

        # Check of BDPU packet
        if dest_mac == "01:80:c2:00:00:00":
            recv_bpdu(data, interface)
            continue

        # Add to Cam Table the entry
        CAM[src_mac] = interface

        vlan_tagged_packet = data
        vlan_untagged_packet = data
        vlan_tagged_length = length
        vlan_untagged_length = length
        
        if vlan_id == -1:
            # Add VLAN tag to frame
            vlan_tagged_packet = data[0:12] + create_vlan_tag(int(ports_type[get_interface_name(interface)])) + data[12:]
            vlan_tagged_length = length + 4
        else:
            # Remove VLAN tag from frame
            vlan_untagged_packet = data[0:12] + data[16:]
            vlan_untagged_length = length - 4

        if dest_mac in CAM:
            next_port = CAM[dest_mac]
            foward_packet(next_port, vlan_id, interface,
                          vlan_tagged_length, vlan_tagged_packet,
                          vlan_untagged_length, vlan_untagged_packet)
        else:
            for i in interfaces:
                if i != interface:
                    foward_packet(i, vlan_id, interface,
                                  vlan_tagged_length, vlan_tagged_packet,
                                  vlan_untagged_length, vlan_untagged_packet)

if __name__ == "__main__":
    main()


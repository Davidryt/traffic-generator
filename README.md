### Build:

`gcc -g -o frame_generator frame_generator.c raw_socket.c`

### Usage:

`frame_generator [options]`

#### Options:

- `-i <iface_name>` : **Interface name**. Example: `eth0`.
- `-e <ethertype>` : **Ethertype** in hexadecimal (default = `0x80F1`). List of examples [here](https://www.iana.org/assignments/ieee-802-numbers/ieee-802-numbers.xhtml).
- `-t` : Add a **VLAN IEEE 802.1Q tag** to the generated frames.
- `-v <vlan_id>` : **VLAN ID** (default = `0` = no VLAN). Must be an integer between 0 and 4094 included. No effect without `-t`.
- `-p <vlan_pcp>` : **VLAN PCP** (default = `0`). Must be an integer between 0 and 7 included. No effect without `-t`.
- `-s <frame_size>` : **Frame size** in bytes, from destination MAC address to FCS included (default = `64`). Must be an integer between 64 and 1518 (1522 if `-t`) included.
- `-r <burst_size>` : **Number of frames per burst** (default = `1` = no burst). Must be a positive integer.
- `-n <n_bursts>` : **Number of bursts** (frames if `-r 1`) to send and stop (default = `-1` = continuous transmission). Must be a positive integer or -1.
- `-c <cycle_time>` : **Cycle time**, i.e. period of the transmission, in nanoseconds (default = `100000000` = 100 ms). Must be a positive integer lower than 2147483647 (≈ 2.15s).
- `-h` : Show the **command help**.

### Structure of the generated frames:

- The frame structure is the following: **MAC destination** (6 bytes), **MAC source** (6 bytes), **802.1Q tag** if `-t` (4 bytes), **Ethertype** (2 bytes), **Payload** (42-1500 bytes), **FCS/CRC** (4 bytes). The frame size is the sum of its components' sizes. For example, with `-s 64`, the payload will be 42-bytes long if `-t`, 46-bytes long else.
- The destination MAC address is **D2:C2:0E:5C:A9:97**. It cannot be changed, but it is possible to set the address of the destination interface using the command `ifconfig <interface> hw ether d2:c2:0e:5c:a9:97`.
- The first 4 bytes of the payload are the **burst number**. It is set to 0 at the beginning of the transmission, and it is incremented at each new burst (frame if `-r 1`).
- The following 4 bytes are the **frame number**. It is set to 0 at the beginning of a new burst, then it is incremented at each new frame in this burst. It is always equal to 0 if `-r 1`.
- The rest of the payload is set to 0.

### Examples:

- Display the command help: `./frame_generator -h`
- Send a 100B-frame every 1ms (through eth0): `sudo ./frame_generator -i eth0 -s 100 -c 1000000`
- Send 50 bursts of 15 frames each, with a period of 1s (through eth1): `sudo ./frame_generator -i eth1 -r 15 -n 50 -c 1000000000`
- Send 20 frames with a VLAN tag, VID 10, PCP 7 (through enp5s0): `sudo ./frame_generator -i enp5s0 -t -v 10 -p 7 -n 20`
- Send 100 bursts of 10 frames each, every 100ms; each frame being 1000-bytes long, having an ethertype of 80F2 and being VLAN-tagged with VID = 1 and PCP = 5 (through eth0): `sudo ./frame_generator -i eth0 -e 0x80F2 -t -v 1 -p 5 -s 1000 -r 10 -n 100 -c 100000000`

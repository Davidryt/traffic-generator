#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> //getopt
#include <string.h>
#include <netinet/ether.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/syscall.h>

#include "raw_socket.h"

/* Print usage */
static void
print_usage()
{
        printf("Frame generator application.\n\n");
        printf("Usage: frame_generator [options]\n");
        printf("Options:\n");
        printf("  -i <iface_name> Interface name.\n");
        printf("  -e <ethertype> Ethertype in hexadecimal (default = 0x80F1).\n");
        printf("  -t : Add a VLAN IEEE 802.1Q tag to the frames sent.\n");
        printf("  -v <vlan_id> VLAN ID (default = 0 = no VLAN).\n");
        printf("  -p <vlan_pcp> VLAN PCP (default = 0).\n");
        printf("  -s <frame_size> Frame size in bytes, from destination MAC address to FCS included (default = 64 B).\n");
        printf("  -r <burst_size> Number of frames per burst (default = 1 = no burst).\n");
        printf("  -n <n_bursts> Number of bursts (or frames if r = 1) to send and stop (default = continuous transmission).\n");
        printf("  -c <cycle_time> Cycle time in nanoseconds (default = 100000000 ns).\n");
        printf("  -h : Show this help.\n");
        printf("\nExample: sudo ./frame_generator -i eth0 -e 0x80F1 -t -v 1 -p 5 -s 1000 -r 10 -n 100 -c 100000000\n");
}

int main(int argc, char *argv[])
{
        int c, ret, sockfd;
        long res = 0;
        user_arguments_t user_arguments;
        iface_info_t iface_info;
        //unsigned char dest_addr[ETH_ALEN] = {0x70, 0xf8, 0xe7, 0xd0, 0x24, 0x67};
        unsigned char dest_addr[ETH_ALEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
        int header_size;
        uint32_t *seq_id_ptr;
        uint32_t seq_id = 0;
        uint32_t *frame_id_ptr;
        uint32_t frame_id = 0;
        struct ether_addr tmp_dst_ether_addr;
        period_info_t pinfo;

        /* Initialize user arguments */
        user_arguments.iface_name = NULL;
        user_arguments.ethertype = DEFAULT_ETHERTYPE;
        user_arguments.vlan_tagged = false;
        user_arguments.vlan_id = DEFAULT_VLAN_ID;
        user_arguments.vlan_pcp = DEFAULT_VLAN_PCP;
        user_arguments.frame_size = DEFAULT_FRAME_SIZE;
        user_arguments.burst_size = DEFAULT_BURST_SIZE;
        user_arguments.n_bursts = -1;
        user_arguments.cycle_time = DEFAULT_CYCLE_TIME;

        /* Parse command line */
        while ((c = getopt(argc, argv, "hi:e:tv:p:s:r:n:c:")) != -1)
        {
                switch (c)
                {
                case 'i': /* interface name */
                        user_arguments.iface_name = strdup(optarg);
                        break;

                case 'e': /* ethertype */
                        res = strtol(optarg, NULL, 16);
                        if (res < 0 || res > 0xFFFE)
                        {
                                printf("Invalid Ethertype\n");
                                return -1;
                        }
                        user_arguments.ethertype = (uint32_t)res;
                        break;
                        
                case 't': /* vlan tag */
                	 user_arguments.vlan_tagged = true;
                	 break;

                case 'v': /* vlan id */
                        res = strtol(optarg, NULL, 10);
                        if (res < 0 || res > 0x4094)
                        {
                                printf("Invalid VLAN ID\n");
                                return -1;
                        }
                        user_arguments.vlan_id = (uint16_t)res;
                        break;

                case 'p': /* vlan pcp */
                        res = strtol(optarg, NULL, 10);
                        if (res < 0 || res > 7)
                        {
                                printf("Invalid VLAN PCP\n");
                                return -1;
                        }
                        user_arguments.vlan_pcp = (uint8_t)res;
                        break;

                case 's': /* frame size */
                        res = strtol(optarg, NULL, 10);
                        user_arguments.frame_size = (uint32_t)res;
                        break;

                case 'r': /* burst size */
                	 res = strtol(optarg, NULL, 10);
                        if (res < 1)
                        {
                                printf("Invalid burst size\n");
                                return -1;
                        }
                        user_arguments.burst_size = (int)res;
                        break;
                        
                case 'n': /* number of bursts */
                        user_arguments.n_bursts = (int)strtol(optarg, NULL, 10);
                        break;

                case 'c': /* cycle time */
                        user_arguments.cycle_time = (uint32_t)strtol(optarg, NULL, 10);
                        break;

                case 'h':
                case '?':
                case ':':
                default:
                        print_usage();
                        return 1;
                }
        }

        if (!user_arguments.iface_name)
        {
                printf("Invalid Interface name\n");
                return -1;
        }
        
        if (user_arguments.frame_size < ETH_ZLEN+ETH_FCS_LEN || user_arguments.frame_size > ETH_FRAME_LEN+ETH_FCS_LEN+ETH_TAG_LEN*user_arguments.vlan_tagged)	// 64 <= frame size <= 1518 (1522 for tagged frames)
	{
		printf("Invalid frame size\n");
		return -1;
	}
        
        /* Create raw socket */
        ret = create_raw_socket(user_arguments, &iface_info, &sockfd);
        if (ret < 0)
        {
                printf("Error creating Raw Socket\n");
                return -1;
        }

        printf("Frame generator started (pid = %ld)\n\n", syscall(__NR_gettid));
        printf("\tInterface: %s\n", user_arguments.iface_name);
        printf("\tDst MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", dest_addr[0], dest_addr[1], dest_addr[2], dest_addr[3], dest_addr[4], dest_addr[5]);
        printf("\tSrc MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", iface_info.mac.ether_addr_octet[0], iface_info.mac.ether_addr_octet[1], iface_info.mac.ether_addr_octet[2], iface_info.mac.ether_addr_octet[3], iface_info.mac.ether_addr_octet[4], iface_info.mac.ether_addr_octet[5]);
        printf("\tEthertype: 0x%X\n", user_arguments.ethertype);
        if (user_arguments.vlan_tagged) 
        {
        	printf("\tVID = %u, PCP = %u\n", user_arguments.vlan_id, user_arguments.vlan_pcp);
        }
        printf("\tFrame size: %u\n", user_arguments.frame_size);
        if (user_arguments.burst_size == 1) 
        {
        	if (user_arguments.n_bursts > 0)
        	{
        		printf("\tNumber of frames: %u\n", user_arguments.n_bursts);
        	}
        } else 
        {
		printf("\tFrames per burst: %u\n", user_arguments.burst_size);
		if (user_arguments.n_bursts > 0)
        	{
			printf("\tNumber of bursts: %u\n", user_arguments.n_bursts);
		}
	}
	if (user_arguments.n_bursts != 1) 
        {
        printf("\tCycle time: %u\n", user_arguments.cycle_time);
        }
        printf("\n");

        /* Construct packet */
        user_arguments.frame_size = user_arguments.frame_size - ETH_FCS_LEN;	/* take FCS into account */
        char packet_buffer[user_arguments.frame_size];
        memset(packet_buffer, 0, user_arguments.frame_size);

        /* Ethernet header */
        if (user_arguments.vlan_tagged) 
        {
		ethernet_header_t *ethernet_header = (ethernet_header_t *)packet_buffer;
		memcpy(&(ethernet_header->l2_header.ether_dhost), dest_addr, ETH_ALEN);
		memcpy(&(ethernet_header->l2_header.ether_shost), iface_info.mac.ether_addr_octet, ETH_ALEN);
        	ethernet_header->l2_header.ether_type = htons(ETHERTYPE_VLAN);		// VLAN tag TPID
        	ethernet_header->vlan = htons((uint16_t)(((uint8_t)user_arguments.vlan_pcp << 13) & 0xE000) | ((uint16_t)user_arguments.vlan_id & 0x0FFF)); 	// VLAN tag PCP, DEI, VID
        	ethernet_header->ethertype = htons(user_arguments.ethertype);
        	header_size = sizeof(ethernet_header_t);
        } else 
        {
        	struct ether_header *ethernet_header = (struct ether_header *)packet_buffer;
		memcpy(&(ethernet_header->ether_dhost), dest_addr, ETH_ALEN);
		memcpy(&(ethernet_header->ether_shost), iface_info.mac.ether_addr_octet, ETH_ALEN);
        	ethernet_header->ether_type = htons(user_arguments.ethertype);
        	header_size = sizeof(struct ether_header);
        }

        /* Sequence ID (4 bytes) just after ethernet header */
        seq_id_ptr = (uint32_t *)(packet_buffer + header_size);
        *seq_id_ptr = 0;

        /* Frame ID (4 bytes) just after the sequence ID */
        frame_id_ptr = (uint32_t *)(packet_buffer + header_size + 4);
        *frame_id_ptr = 0;
        
        pinfo.period_ns = user_arguments.cycle_time;
        wait_for_first_cycle(&pinfo); 

        while (1)
        {
        	for (int i = 0; i < user_arguments.burst_size; i++)
        	{
                	send_frame(sockfd, packet_buffer, user_arguments.frame_size);
                	
		        /* Increase frame ID */
		        frame_id++;
		        *frame_id_ptr = htonl(frame_id);
              	}
              	
	        frame_id = 0;
	        *frame_id_ptr = htonl(frame_id);

                //printf("Burst with seqId=%d sent at %ld s, %ld ns\n", seq_id, pinfo.next_period.tv_sec, pinfo.next_period.tv_nsec);

                /* Increase sequence ID */
                seq_id++;
                *seq_id_ptr = htonl(seq_id);

                if (seq_id == user_arguments.n_bursts)
                        break;

                wait_for_next_cycle(&pinfo);
        }

        close_socket(sockfd);

        return 0;
}

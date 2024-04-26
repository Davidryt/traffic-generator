#include <stdint.h>
#include <stdbool.h>
#include <net/ethernet.h>

#define DEFAULT_ETHERTYPE 0x80F1
#define DEFAULT_VLAN_ID 0
#define DEFAULT_VLAN_PCP 0
#define DEFAULT_FRAME_SIZE 64
#define DEFAULT_BURST_SIZE 1
#define DEFAULT_CYCLE_TIME 100000000

#define ETH_TAG_LEN 4

// Structure to hold information about a network interface
typedef struct
{
        char name[20];
        struct ether_addr mac;
        int index;
        int mtu;
} iface_info_t;

typedef struct {
        struct ether_header l2_header;
	uint16_t vlan;
        uint16_t ethertype;
} ethernet_header_t;

typedef struct {

        char *iface_name;
        uint16_t ethertype;
        bool vlan_tagged;
        uint16_t vlan_id;
        uint8_t vlan_pcp;
        uint32_t frame_size;
        int burst_size;
        int n_bursts;
        uint32_t cycle_time;
        int cpu;

} user_arguments_t;

typedef struct {
        struct timespec next_period;
        long period_ns;
} period_info_t;

void close_socket(int sockfd);
int create_raw_socket(user_arguments_t user_arguments, iface_info_t *iface_info, int *sockfd);
void send_frame(int sockfd, char *packet_buffer, int frame_size);
void wait_for_first_cycle(period_info_t *pinfo);
void wait_for_next_cycle(period_info_t *pinfo);

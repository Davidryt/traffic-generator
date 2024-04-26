#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <netinet/ether.h>
#include <net/ethernet.h>
#include <linux/if_packet.h>
#include <time.h>
#include <unistd.h>

#include "raw_socket.h"

#define NANOSECONDS_PER_SECOND (1000000000ULL)
#define TIMESPEC_TO_NSEC(ts) (((uint64_t)ts.tv_sec * (uint64_t)NANOSECONDS_PER_SECOND) + (uint64_t)ts.tv_nsec)

// Get information about an interface
int get_iface_info(const char *ifname, iface_info_t *info)
{
        if (!ifname || !info)
        {
                printf("Checking interface; invalid arguments\n");
                return 0;
        }

        // zap the result struct
        memset(info, 0, sizeof(iface_info_t));

        strncpy(info->name, ifname, 20);

        // open a throw-away socket - used for our ioctls
        int sk = socket(AF_INET, SOCK_STREAM, 0);
        if (sk == -1)
        {
                printf("Checking interface; socket open failed\n");
                return 0;
        }

        // set the name of the interface in the ioctl request struct
        struct ifreq ifr;
        memset(&ifr, 0, sizeof(struct ifreq));
        strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name) - 1);

        // First check if the interface is up
        //  (also proves that the interface exists!)
        int r = ioctl(sk, SIOCGIFFLAGS, &ifr);
        if (r != 0)
        {
                printf("Checking interface; ioctl(SIOCGIFFLAGS) failed\n");
                close(sk);
                return 0;
        }

        if (!(ifr.ifr_flags & IFF_UP))
        {
                printf("Checking interface; interface is not up: %s\n", ifname);
                close(sk);
                return 0;
        }

        // get index for interface
        r = ioctl(sk, SIOCGIFINDEX, &ifr);
        if (r != 0)
        {
                printf("Checking interface; ioctl(SIOCGIFINDEX) failed\n");
                close(sk);
                return 0;
        }
        info->index = ifr.ifr_ifindex;

        // get the MAC address for the interface
        r = ioctl(sk, SIOCGIFHWADDR, &ifr);
        if (r != 0)
        {
                printf("Checking interface; ioctl(SIOCGIFHWADDR) failed\n");
                close(sk);
                return 0;
        }
        memcpy(&info->mac.ether_addr_octet, &ifr.ifr_hwaddr.sa_data, ETH_ALEN);

        // get the MTU for the interface
        r = ioctl(sk, SIOCGIFMTU, &ifr);
        if (r != 0)
        {
                printf("Checking interface; ioctl(SIOCGIFMTU) failed\n");
                close(sk);
                return 0;
        }
        info->mtu = ifr.ifr_mtu;

        // close the temporary socket
        close(sk);
        return 1;
}

/* Print information about the interface */
void print_iface_info(iface_info_t info)
{
        printf("Interface name: %s\n", info.name);
        printf("MAC address: %s\n", ether_ntoa(&info.mac));
        printf("Index: %d\n", info.index);
        printf("MTU: %d\n\n", info.mtu);
}

void close_socket(int sockfd)
{
    if (sockfd != -1)
    {
            close(sockfd);
    }
}

int create_raw_socket(user_arguments_t user_arguments, iface_info_t *iface_info, int *sockfd)
{
    int tmp;
    struct sockaddr_ll socket_address;

    // Get info about the network device
    if (!get_iface_info(user_arguments.iface_name, iface_info))
    {
            printf("Creating rawsock: bad interface name: %s\n", user_arguments.iface_name);
            return -1;
    }

    // Print interface information
    //print_iface_info(iface_info);

    // Create socket
    *sockfd = socket(AF_PACKET, SOCK_RAW, (int)user_arguments.ethertype);
    if (*sockfd == -1)
    {
            printf("Creating rawsock; opening socket error.\n");
            return -1;
    }

    // Allow address reuse
    tmp = 1;
    if (setsockopt(*sockfd, SOL_SOCKET, SO_REUSEADDR, &tmp, sizeof(int)) < 0)
    {
            printf("Creating rawsock; failed to set reuseaddr\n");
            close_socket(*sockfd);
            return -1;
    }

    // set the class'es priority on the TX socket
    // (required by Telechips platform for FQTSS Credit Based Shaper to work)
    // TODO: check this value
    if (setsockopt(*sockfd, SOL_SOCKET, SO_PRIORITY, (char *)&user_arguments.vlan_pcp, sizeof(uint32_t)) < 0) {
        printf("stcRawsockTxSetHdr; SO_PRIORITY setsockopt failed \n");
        return 0;
    }

    // Bind to interface
    memset(&socket_address, 0, sizeof(socket_address));
    socket_address.sll_family = AF_PACKET;
    socket_address.sll_protocol = (int)user_arguments.ethertype;
    socket_address.sll_ifindex = iface_info->index;

    if (bind(*sockfd, (struct sockaddr *)&socket_address, sizeof(socket_address)) == -1)
    {
            printf("Creating rawsock; bind socket error\n");
            close_socket(*sockfd);
            return -1;
    }

    return 0;
}

void send_frame(int sockfd, char *packet_buffer, int frame_size)
{
    send(sockfd, packet_buffer, frame_size, MSG_DONTWAIT);
}

void wait_for_first_cycle(period_info_t *pinfo)
{
    uint32_t current_cycle_number;
    uint64_t cycle_start_time;

    clock_gettime(CLOCK_MONOTONIC, &(pinfo->next_period));

    current_cycle_number = TIMESPEC_TO_NSEC(pinfo->next_period) / pinfo->period_ns;
    cycle_start_time = (current_cycle_number + 2) * pinfo->period_ns;
    pinfo->next_period.tv_sec = cycle_start_time / 1000000000;
    pinfo->next_period.tv_nsec = cycle_start_time % 1000000000;

    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &pinfo->next_period, NULL);
}

void wait_for_next_cycle(period_info_t *pinfo)
{
    pinfo->next_period.tv_nsec += pinfo->period_ns;

    while (pinfo->next_period.tv_nsec >= 1000000000)
    {
        /* timespec nsec overflow */
        pinfo->next_period.tv_sec++;
        pinfo->next_period.tv_nsec -= 1000000000;
    }

    /* for simplicity, ignoring possibilities of signal wakes */
    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &pinfo->next_period, NULL);
}
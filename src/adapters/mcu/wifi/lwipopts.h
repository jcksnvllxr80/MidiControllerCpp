#ifndef MC_LWIPOPTS_H
#define MC_LWIPOPTS_H
// Minimal lwIP config for the Pico 2 W: NO_SYS (bare metal, poll mode), TCP for
// the protocol server, UDP+IGMP+mDNS for discovery. Modeled on the pico-examples
// picow lwipopts.

#define NO_SYS 1
#define LWIP_SOCKET 0
#define LWIP_NETCONN 0

#define MEM_LIBC_MALLOC 0
#define MEM_ALIGNMENT 4
// Headroom for streaming a large (~30 KB) get_pedal reply: replies are chunked
// across many tcp_write/sent cycles (WifiManager::trySend), so this only needs to
// cover in-flight segments + pbufs, not the whole payload. Bumped from 16000/24 —
// the RP2350 has plenty of RAM, and tight pools were a stall risk under load.
#define MEM_SIZE 32000
#define MEMP_NUM_TCP_SEG 32
#define MEMP_NUM_ARP_QUEUE 10
#define PBUF_POOL_SIZE 32

#define LWIP_ARP 1
#define LWIP_ETHERNET 1
#define LWIP_ICMP 1
#define LWIP_RAW 1
#define LWIP_IPV4 1
#define LWIP_TCP 1
#define LWIP_UDP 1
#define LWIP_DNS 0

#define TCP_MSS 1460
#define TCP_WND (8 * TCP_MSS)
#define TCP_SND_BUF (8 * TCP_MSS)
#define TCP_SND_QUEUELEN ((4 * (TCP_SND_BUF) + (TCP_MSS - 1)) / (TCP_MSS))

#define LWIP_NETIF_STATUS_CALLBACK 1
#define LWIP_NETIF_LINK_CALLBACK 1
#define LWIP_NETIF_HOSTNAME 1
#define LWIP_NETIF_TX_SINGLE_PBUF 1
#define DHCP_DOES_ARP_CHECK 0
#define LWIP_DHCP_DOES_ACD_CHECK 0

#define LWIP_DHCP 1

// mDNS discovery
#define LWIP_IGMP 1
#define LWIP_MDNS_RESPONDER 1
#define LWIP_NUM_NETIF_CLIENT_DATA 1
#define MDNS_MAX_SERVICES 1
#define MEMP_NUM_UDP_PCB 6

#define LWIP_CHKSUM_ALGORITHM 3

// stats/debug off for size
#define MEM_STATS 0
#define SYS_STATS 0
#define MEMP_STATS 0
#define LINK_STATS 0
#define LWIP_STATS 0
#define LWIP_DEBUG 0

#endif  // MC_LWIPOPTS_H

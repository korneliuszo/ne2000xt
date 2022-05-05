#ifndef ETH_H_
#define ETH_H_

#include <stdint.h>
#include <stdbool.h>
#include "inlines.h"

void eth_detect();
void eth_initialize();

static inline void eth_barrier(void);
#ifdef FAST_SYS_SLOW_CARD
#pragma aux barrier = \
	"jmp L1" \
	"L1:" \
	"push ax" \
	"in al,61h" \
	"pop ax" \
	modify exact [] nomemory;
#else
void eth_barrier(void){};
#endif

extern uint16_t io_base;
extern uint16_t io_dma;

static inline void eth_outb(uint16_t port, uint8_t val)
{
	eth_barrier();
	outb(io_base+port,val);
}

static inline uint8_t eth_inb(uint16_t port)
{
	eth_barrier();
	return inb(io_base+port);
}


static inline void eth_outdma(uint8_t val)
{
	eth_barrier();
	outb(io_dma,val);
}

static inline uint8_t eth_indma()
{
	eth_barrier();
	return inb(io_dma);
}

extern uint8_t mac_address[6]; //big_endian!!

extern uint32_t local_ip; // big_endian!!!

typedef struct
{
	uint8_t remote_mac[6];// big_endian!!!
	uint32_t remote_ip;// big_endian!!!
	uint16_t remote_port;
	uint16_t local_port;
} udp_conn;

void start_send_udp(udp_conn *conn, uint16_t len);
void fin_send_udp(uint16_t len);
void send_dhcp_discover();
extern uint8_t rx_pkt[1522];
extern uint16_t rx_len;
bool start_recv();
uint8_t* decode_udp(uint16_t local_port, uint16_t *len);
void fill_udp_conn(udp_conn * conn);
bool dhcp_poll();

#endif /* ETH_H_ */

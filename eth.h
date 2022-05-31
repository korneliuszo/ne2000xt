#ifndef ETH_H_
#define ETH_H_

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
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


#define prepare_dma() uint16_t io_dma_local = io_dma
#define eth_outdma(val) do { \
	eth_barrier(); \
	outb(io_dma_local,val);} while(0)

uint8_t eth_indma_a(uint16_t ptr);
#pragma aux eth_indma_a = \
		"in	al,dx" \
		parm [dx] \
		modify exact [al] nomemory \
		value [al];

uint16_t eth_indma_a_16be(uint16_t ptr);
#pragma aux eth_indma_a_16be = \
		"in	al,dx" \
		"mov ah,al" \
		"in	al,dx" \
		parm [dx] \
		modify exact [ax] nomemory \
		value [ax];

#define eth_indma() eth_indma_a(io_dma_local)
#define eth_indma_16be() eth_indma_a_16be(io_dma_local)

//#define eth_indma()
//	(uint8_t)inb(io_dma_local)

extern uint16_t rx_buff_len, rx_buff2_len;
extern uint16_t rx_off;
extern uint16_t rx_pkt_off;
extern uint8_t next_pkt;


extern uint8_t mac_address[6]; //big_endian!!

extern uint8_t local_ip[4]; // big_endian!!!

typedef struct
{
	uint8_t remote_mac[6];// big_endian!!!
	uint8_t remote_ip[4];// big_endian!!!
	uint16_t remote_port;
	uint16_t local_port;
} udp_conn;

void start_send_udp(const udp_conn *conn, uint16_t len);
void fin_send_udp(uint16_t len);
void send_dhcp_discover();
bool start_recv();
void recv_end();
uint16_t process_eth(udp_conn * conn);
bool decode_ip_udp(uint16_t local_port, uint16_t *len, udp_conn * conn);
void paste_restpacket(uint8_t far *buff,size_t len);
void paste_restpacket_flipped(uint8_t far *buff,size_t len);
bool dhcp_poll();
void process_arp(udp_conn * conn);

#endif /* ETH_H_ */

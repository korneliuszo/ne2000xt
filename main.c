#include <i86.h>
#include <stdint.h>
#include "biosint.h"
#include "delay.h"
#include "eth.h"
#include <string.h>

typedef struct {
	uint16_t es;
	uint16_t ds;
	uint16_t di;
	uint16_t si;
	uint16_t bp;
	uint16_t bx;
	uint16_t dx;
	uint16_t cx;
	uint16_t ax;
	uint16_t f;
	uint16_t ph1;
	uint16_t ph2;
	uint16_t ip;
	uint16_t cs;
	uint16_t rf;
} IRQ_DATA;

int __cdecl start(uint16_t irq, IRQ_DATA far * params);

void __cdecl install13h();

int start(uint16_t irq, IRQ_DATA far * params)
{
	static udp_conn conn;

	bool still_resend = true;
	if (irq == 0)
	{
		bios_printf(BIOS_PRINTF_ALL,"NE2000XT Monitor v0.8\n");

		delay_init();

		eth_detect();

		eth_initialize();

		while(1)
		{
			send_dhcp_discover();
			for(int i=0;i<PIT_MSC(5000);i++)
			{
				if(dhcp_poll())
					goto iprecv;
				delay_spin(1);
			}
		}
		iprecv:
		bios_printf(BIOS_PRINTF_ALL,"IP %d.%d.%d.%d\n",
			local_ip[0],local_ip[1],local_ip[2],local_ip[3]);

		const static udp_conn new_conn = {
				{0xff,0xff,0xff,0xff,0xff,0xff},
				{255,255,255,255},
				5556,
				5556
		};
		start_send_udp(&new_conn, 6);
		prepare_dma();
		for(uint16_t i=0;i<6;i++)
			eth_outdma(mac_address[i]);
		fin_send_udp(6);
		still_resend = false;
	}
	uint16_t resend_delay = get_ctr();
	uint16_t ping_pong = 0x00;
	while(1)
	{
		if(still_resend && get_ctr() >= resend_delay)
		{
			resend_delay += PIT_MSC(5000);
			start_send_udp(&conn, 1);
			prepare_dma();
			eth_outdma(0x40);
			fin_send_udp(1);
			resend_delay = get_ctr();
		}
		if(start_recv())
		{
			uint16_t eth_type = process_eth(&conn);
			if(eth_type == 0x0800)
			{
				static uint16_t len;
				if(decode_ip_udp(5555,&len,&conn))
				{
					if (len < 2)
					{
						recv_end();
						continue;
					}
					still_resend = false;
					prepare_dma();
					uint8_t curr_phase = eth_indma();
					len--;
					rx_off+=1;
					bool lost_reply = !(ping_pong & (0x1<<(curr_phase &0xf))) ^ !(curr_phase&0x80);
					if(!lost_reply)
						ping_pong ^= 1<<(curr_phase &0xf);
					switch(eth_indma())
					{
					case 0: //ping
					{
						uint8_t buff[10];
						rx_off+=1;
						paste_restpacket(buff,len-1);
						recv_end();
						start_send_udp(&conn, len);
						eth_outdma(curr_phase);
						for(int i=0;i<len-1;i++)
							eth_outdma(buff[i]);
						fin_send_udp(len);
						break;
					}
					case 1: //outb
					{
						if(!lost_reply)
						{
							uint16_t port;
							uint8_t port_l = eth_indma();
							uint8_t port_h = eth_indma();
							uint8_t val = eth_indma();
							port = port_l | (port_h<<8);
							outb(port,val);
						}
						recv_end();
						start_send_udp(&conn, 1);
						eth_outdma(curr_phase);
						fin_send_udp(1);
						break;
					}
					case 2: //inb
					{
						static uint8_t inb_val_cache;
						if(!lost_reply)
						{
							uint16_t port;
							uint8_t port_l = eth_indma();
							uint8_t port_h = eth_indma();
							port = port_l | (port_h<<8);
							inb_val_cache = (uint8_t)inb(port);
						}
						recv_end();
						start_send_udp(&conn, 2);
						eth_outdma(curr_phase);
						eth_outdma(inb_val_cache);
						fin_send_udp(2);
						break;
					}
					case 3: //putmem
					{
						if(!lost_reply)
						{
							__segment               seg;
							char __based( void ) * segptr;
							uint8_t seg_l = eth_indma();
							uint8_t seg_h = eth_indma();
							uint8_t ptr_l = eth_indma();
							uint8_t ptr_h = eth_indma();
							rx_off+=5;
							seg = seg_l | (seg_h<<8);
							segptr =(char __based(void) *)( ptr_l | (ptr_h <<8));
							len -=5;
							paste_restpacket(seg:>segptr,len);
						}
						recv_end();
						start_send_udp(&conn, 1);
						eth_outdma(curr_phase);
						fin_send_udp(1);
						break;
					}
					case 4: //getmem
					{
						__segment               seg;
						char __based( void ) * segptr;
						uint8_t seg_l = eth_indma();
						uint8_t seg_h = eth_indma();
						uint8_t ptr_l = eth_indma();
						uint8_t ptr_h = eth_indma();
						uint8_t datalen_l = eth_indma();
						uint8_t datalen_h = eth_indma();
						recv_end();

						seg = seg_l | (seg_h<<8);
						segptr =(char __based(void) *)( ptr_l | (ptr_h <<8));
						uint16_t datalen;
						datalen = datalen_l | (datalen_h<<8);
						start_send_udp(&conn, datalen+1);
						uint16_t cnt=datalen;
						prepare_dma();
						eth_outdma(curr_phase);
						while(cnt--) //TODO: make some inline asm
						{
							eth_outdma(*(seg:>segptr));
							segptr++;
						}
						fin_send_udp(datalen+1);
						break;
					}
					case 5: //call x86
					{
						static union REGPACK  r;
						if(!lost_reply)
						{
							uint8_t irq = eth_indma();

							r.h.al = eth_indma();
							r.h.ah = eth_indma();
							r.h.bl = eth_indma();
							r.h.bh = eth_indma();
							r.h.cl = eth_indma();
							r.h.ch = eth_indma();
							r.h.dl = eth_indma();
							r.h.dh = eth_indma();
							uint8_t di_l = eth_indma();
							uint8_t di_h = eth_indma();
							r.w.di = di_l | (di_h<<8);
							uint8_t si_l = eth_indma();
							uint8_t si_h = eth_indma();
							r.w.si = si_l | (si_h<<8);
							uint8_t cf_l = eth_indma();
							uint8_t cf_h = eth_indma();
							r.x.flags = cf_l | (cf_h<<8);
							uint8_t ds_l = eth_indma();
							uint8_t ds_h = eth_indma();
							r.x.ds = ds_l | (ds_h<<8);
							uint8_t es_l = eth_indma();
							uint8_t es_h = eth_indma();
							r.x.es = es_l | (es_h<<8);

							intrf( irq, &r);
						}
						recv_end();

						start_send_udp(&conn, 15);
						prepare_dma();
						eth_outdma(curr_phase);
						eth_outdma(r.h.al);
						eth_outdma(r.h.ah);
						eth_outdma(r.h.bl);
						eth_outdma(r.h.bh);
						eth_outdma(r.h.cl);
						eth_outdma(r.h.ch);
						eth_outdma(r.h.dl);
						eth_outdma(r.h.dh);
						eth_outdma((uint8_t)r.w.di);
						eth_outdma((uint8_t)(r.w.di>>8));
						eth_outdma((uint8_t)r.w.si);
						eth_outdma((uint8_t)(r.w.si>>8));
						eth_outdma((uint8_t)(r.x.flags));
						eth_outdma((uint8_t)(r.x.flags>>8));
						fin_send_udp(15);
						break;
					}
					case 6: //putmem_flipped
					{

						if(!lost_reply)
						{
							__segment               seg;
							char __based( void ) * segptr;
							uint8_t seg_l = eth_indma();
							uint8_t seg_h = eth_indma();
							uint8_t ptr_l = eth_indma();
							uint8_t ptr_h = eth_indma();
							eth_indma();
							rx_off+=6;
							seg = seg_l | (seg_h<<8);
							segptr =(char __based(void) *)( ptr_l | (ptr_h <<8));
							len -=6;
							paste_restpacket_flipped(seg:>segptr,len);
						}
						recv_end();
						start_send_udp(&conn, 1);
						eth_outdma(curr_phase);
						fin_send_udp(1);
						break;
					}
					case 7: // continue boot
					{
						recv_end();
						if(!lost_reply)
							return 1;
						break;
					}
					case 8:
					{
						recv_end();
						start_send_udp(&conn, 29);
						prepare_dma();
						eth_outdma(curr_phase);
						eth_outdma(irq);
						eth_outdma(irq>>8);
						eth_outdma(params->ax);
						eth_outdma(params->ax>>8);
						eth_outdma(params->cx);
						eth_outdma(params->cx>>8);
						eth_outdma(params->dx);
						eth_outdma(params->dx>>8);
						eth_outdma(params->bx);
						eth_outdma(params->bx>>8);
						eth_outdma(params->bp);
						eth_outdma(params->bp>>8);
						eth_outdma(params->si);
						eth_outdma(params->si>>8);
						eth_outdma(params->di);
						eth_outdma(params->di>>8);
						eth_outdma(params->ds);
						eth_outdma(params->ds>>8);
						eth_outdma(params->es);
						eth_outdma(params->es>>8);
						eth_outdma(params->f);
						eth_outdma(params->f>>8);
						eth_outdma(params->ip);
						eth_outdma(params->ip>>8);
						eth_outdma(params->cs);
						eth_outdma(params->cs>>8);
						eth_outdma(params->rf);
						eth_outdma(params->rf>>8);
						fin_send_udp(29);
						break;
					}
					case 9:
					{
						recv_end();
						if(!lost_reply)
							install13h();
						start_send_udp(&conn, 1);
						eth_outdma(curr_phase);
						fin_send_udp(1);
						break;
					}
					case 10:
					{
						recv_end();
						if(!lost_reply)
							return 0;
						break;
					}
					case 11:
					{
						if(!lost_reply)
						{
							params->ax = eth_indma_16be();
							params->cx = eth_indma_16be();
							params->dx = eth_indma_16be();
							params->bx = eth_indma_16be();
							params->bp = eth_indma_16be();
							params->si = eth_indma_16be();
							params->di = eth_indma_16be();
							params->ds = eth_indma_16be();
							params->es = eth_indma_16be();
							params->f = eth_indma_16be();
							params->rf = eth_indma_16be();
						}
						recv_end();
						start_send_udp(&conn, 1);
						eth_outdma(curr_phase);
						fin_send_udp(1);
						break;
					}
					case 0xff:
						recv_end();
						start_send_udp(&conn, 1);
						eth_outdma(curr_phase);
						fin_send_udp(1);
					default:
						recv_end();
					break;
					}
				}
			}
			else if(eth_type == 0x0806){
				process_arp(&conn);
			}
			else
			{
				recv_end();
			}
		}
	}
}

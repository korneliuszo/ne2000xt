int __cdecl start();

#include <i86.h>
#include <stdint.h>
#include "biosint.h"
#include "delay.h"
#include "eth.h"
#include <string.h>

int start()
{
	bios_printf(BIOS_PRINTF_ALL,"NE2000XT Monitor\n");

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
	while(1)
	{
		if(start_recv())
		{
			udp_conn conn;
			uint16_t eth_type = process_eth(&conn);
			if(eth_type == 0x0800)
			{
				uint16_t len;
				if(decode_ip_udp(5555,&len,&conn))
				{
					if (len == 0)
					{
						recv_end();
						continue;
					}
					prepare_dma();
					switch(eth_indma())
					{
					case 0: //ping
					{
						uint8_t buff[1024];
						rx_off+=1;
						paste_restpacket(buff,len-1);
						recv_end();
						start_send_udp(&conn, len-1);
						for(int i=0;i<len-1;i++)
							eth_outdma(buff[i]);
						fin_send_udp(len-1);
						break;
					}
					case 1: //outb
					{
						uint16_t port;
						uint8_t port_l = eth_indma();
						uint8_t port_h = eth_indma();
						uint8_t val = eth_indma();
						recv_end();
						port = port_l | (port_h<<8);
						outb(port,val);
						start_send_udp(&conn, 0);
						fin_send_udp(0);
						break;
					}
					case 2: //inb
					{
						uint16_t port;
						uint8_t port_l = eth_indma();
						uint8_t port_h = eth_indma();
						recv_end();
						port = port_l | (port_h<<8);
						start_send_udp(&conn, 1);
						eth_outdma((uint8_t)inb(port));
						fin_send_udp(1);
						break;
					}
					case 3: //putmem
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
						recv_end();
						start_send_udp(&conn, 0);
						fin_send_udp(0);
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
						start_send_udp(&conn, datalen);
						uint16_t cnt=datalen;
						prepare_dma();
						while(cnt--) //TODO: make some inline asm
						{
							eth_outdma(*(seg:>segptr));
							segptr++;
						}
						fin_send_udp(datalen);
						break;
					}
					case 5: //call x86
					{
						uint8_t irq = eth_indma();
						union REGS  r;
						struct SREGS s;

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
						r.w.cflag = cf_l | (cf_h<<8);
						uint8_t ds_l = eth_indma();
						uint8_t ds_h = eth_indma();
						s.ds = ds_l | (ds_h<<8);
						uint8_t es_l = eth_indma();
						uint8_t es_h = eth_indma();
						s.es = es_l | (es_h<<8);
						recv_end();

						int86x(irq,&r,&r,&s);
						start_send_udp(&conn, 14);
						prepare_dma();
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
						eth_outdma((uint8_t)(r.w.cflag));
						eth_outdma((uint8_t)(r.w.cflag>>8));
						fin_send_udp(14);
						break;
					}
					case 6: //putmem_flipped
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
						recv_end();
						start_send_udp(&conn, 0);
						fin_send_udp(0);
						break;
					}
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

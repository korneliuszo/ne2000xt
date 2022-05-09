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
			(uint8_t)(local_ip>>24),(uint8_t)(local_ip>>16),(uint8_t)(local_ip>>8),(uint8_t)(local_ip>>0));
	while(1)
	{
		if(start_recv())
		{
			uint16_t len;
			uint8_t *udp_pkt = decode_udp(5555,&len);
			if(udp_pkt)
			{
				udp_conn conn;
				fill_udp_conn(&conn);
				switch(udp_pkt[0])
				{
				case 0: //ping
				{
					start_send_udp(&conn, len-1);
					for(int i=1;i<len;i++)
						eth_outdma(udp_pkt[i]);
					fin_send_udp(len-1);
					break;
				}
				case 1: //outb
				{
					uint16_t port;
					port = udp_pkt[1] | (udp_pkt[2]<<8);
					outb(port,udp_pkt[3]);
					start_send_udp(&conn, 0);
					fin_send_udp(0);
					break;
				}
				case 2: //inb
				{
					uint16_t port;
					port = udp_pkt[1] | (udp_pkt[2]<<8);
					start_send_udp(&conn, 1);
					eth_outdma((uint8_t)inb(port));
					fin_send_udp(1);
					break;
				}
				case 4: //putmem
				{
			        __segment               seg;
			        char __based( void ) * segptr;
					seg = udp_pkt[1] | (udp_pkt[2]<<8);
					segptr =(char __based(void) *)( udp_pkt[3] | (udp_pkt[4]<<8));
					len -=5;
					_fmemcpy(seg:>segptr,&udp_pkt[5],len);
					start_send_udp(&conn, 0);
					fin_send_udp(0);
					break;
				}
				case 5: //getmem
				{
			        __segment               seg;
			        char __based( void ) * segptr;
					seg = udp_pkt[1] | (udp_pkt[2]<<8);
					segptr =(char __based(void) *)( udp_pkt[3] | (udp_pkt[4]<<8));
					uint16_t datalen;
					datalen = udp_pkt[1] | (udp_pkt[2]<<8);
					start_send_udp(&conn, datalen);
					while(datalen--) //TODO: make some inline asm
					{
						eth_outdma(*(seg:>segptr));
						segptr++;
					}
					fin_send_udp(datalen);
					break;
				}
				case 6: //call x86
				{
					uint8_t irq = udp_pkt[1];
				    union REGS  r;
				    struct SREGS s;

				    r.h.al = udp_pkt[2];
				    r.h.ah = udp_pkt[3];
				    r.h.bl = udp_pkt[4];
				    r.h.bh = udp_pkt[5];
				    r.h.cl = udp_pkt[6];
				    r.h.ch = udp_pkt[7];
				    r.h.dl = udp_pkt[8];
				    r.h.dh = udp_pkt[9];
				    r.w.di = udp_pkt[10] | (udp_pkt[11]<<8);
				    r.w.si = udp_pkt[12] | (udp_pkt[13]<<8);
				    r.w.cflag = udp_pkt[14] | (udp_pkt[15]<<8);

				    s.ds = udp_pkt[16] | (udp_pkt[17]<<8);
				    s.es = udp_pkt[18] | (udp_pkt[19]<<8);

				    int86x(irq,&r,&r,&s);
					start_send_udp(&conn, 12);
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
					fin_send_udp(12);
					break;
				}
				default:
					break;
				}
			}
			else if(process_arp()){};
		}
	}
}

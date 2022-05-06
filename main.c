int __cdecl start();

#include <i86.h>
#include <stdint.h>
#include "biosint.h"
#include "delay.h"
#include "eth.h"


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
			uint8_t *udp_pkt;
			if((udp_pkt = decode_udp(5555,&len)))
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
				default:
					break;
				}
			}
			else if(process_arp()){};
		}
	}

	return 0;
}

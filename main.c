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
		for(int i=0;i<PIT_MSC(1000);i++)
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
	}

	return 0;
}

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
		udp_conn conn = {
				{0xff,0xff,0xff,0xff,0xff,0xff},
				0xffffffff,
				100,
				55555,
		};
		start_send_udp(&conn, 4);
		eth_outdma('B');
		eth_outdma('e');
		eth_outdma('e');
		eth_outdma('p');
		fin_send_udp(4);
		delay_spin(PIT_MSC(1000));
	}

	return 0;
}

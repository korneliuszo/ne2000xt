int __cdecl start();

#include <i86.h>
#include <stdint.h>
#include "biosint.h"
#include "inlines.h"
#include "delay.h"

int start()
{
	bios_printf(BIOS_PRINTF_ALL,"NE2000XT Monitor\n");

	delay_init();

	uint16_t io_base;
	uint8_t id0,id1;
	for(io_base=0x200;io_base<0x400;io_base+=0x20)
	{
		id0 = inb(io_base+0x0A);
		if(id0 == 0x50)
			break;
	};
	if(io_base == 0x400)
	{
		bios_printf(BIOS_PRINTF_DEBHALT,"Card not found\n");
	}
	id1 = inb(io_base+0x0B);

	bios_printf(BIOS_PRINTF_ALL,"Found card at:0x%x ID:0x%x\n",io_base,id0 | (id1<<8));

	delay_spin(PIT_MSC(1000));


	return 0;
}

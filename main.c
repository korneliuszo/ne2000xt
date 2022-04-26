int __cdecl start();

#include <i86.h>
#include <stdint.h>
#include <string.h>
#include "biosint.h"
#include "inlines.h"
#include "delay.h"
#include "dp8390reg.h"

void barrier(void);
#pragma aux barrier = \
	"jmp L1" \
	"L1:" \
	"push ax" \
	"in al,61h" \
	"pop ax" \
	modify exact [] nomemory;

void writemem(uint16_t io_base, const uint8_t *src, uint16_t dst, size_t len)
{
	barrier();
	outb(io_base+ED_P0_CR,ED_CR_RD2 | ED_CR_PAGE_0 | ED_CR_STA);
	barrier();
	outb(io_base+ED_P0_ISR, ED_ISR_RDC);
	barrier();
	outb(io_base+ED_P0_RBCR0, len);
	outb(io_base+ED_P0_RBCR1, len>>8);
	outb(io_base+ED_P0_RSAR0, dst);
	outb(io_base+ED_P0_RSAR1, dst>>8);
	barrier();
	outb(io_base+ED_P0_CR,ED_CR_RD1 | ED_CR_PAGE_0 | ED_CR_STA);
	barrier();

	for(uint16_t i=0;i<len;i++)
		outb(io_base+0x10,src[i]);
	barrier();

	int maxwait = 100;
	while (((inb(io_base+ED_P0_ISR) & ED_ISR_RDC) !=
	    ED_ISR_RDC) && --maxwait)
		delay_spin(1);
}

void readmem(uint16_t io_base, uint8_t *src, uint16_t dst, size_t len)
{
	barrier();
	outb(io_base+ED_P0_CR,ED_CR_RD2 | ED_CR_PAGE_0 | ED_CR_STA);
	barrier();
	size_t lenr = len + (len&1);
	outb(io_base+ED_P0_RBCR0, lenr);
	outb(io_base+ED_P0_RBCR1, lenr>>8);
	outb(io_base+ED_P0_RSAR0, dst);
	outb(io_base+ED_P0_RSAR1, dst>>8);
	barrier();
	outb(io_base+ED_P0_CR,ED_CR_RD0 | ED_CR_PAGE_0 | ED_CR_STA);
	barrier();

	for(uint16_t i=0;i<len;i++)
		src[i]=inb(io_base+0x10);
}

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

	outb(io_base+0x1f,0);
	barrier();
	delay_spin(PIT_MSC(1));
	uint8_t tmp = inb(io_base+0x1f);
	barrier();
	delay_spin(PIT_MSC(10));
	outb(io_base+0x1f,tmp);
	barrier();
	delay_spin(PIT_MSC(5));

	outb(io_base+ED_P0_CR,ED_CR_RD2 | ED_CR_PAGE_0 | ED_CR_STP);
	barrier();
	delay_spin(PIT_MSC(5));

	tmp = inb(io_base+ED_P0_CR);
	if ((tmp & (ED_CR_RD2 | ED_CR_TXP | ED_CR_STA | ED_CR_STP)) !=
		    (ED_CR_RD2 | ED_CR_STP))
		bios_printf(BIOS_PRINTF_DEBHALT,"Card error1\n");

	tmp = inb(io_base+ED_P0_ISR);
	if ((tmp & ED_ISR_RST) != ED_ISR_RST)
		bios_printf(BIOS_PRINTF_DEBHALT,"Card error2\n");

	char i;

	for (i = 0; i < 100; i++) {
		if ((inb(io_base+ED_P0_ISR) & ED_ISR_RST) ==
		    ED_ISR_RST) {
			/* Ack the reset bit. */
			outb(io_base+ED_P0_ISR, ED_ISR_RST);
			barrier();
			break;
		}
		delay_spin(PIT_MSC(1));
	}
	if (i == 100)
		bios_printf(BIOS_PRINTF_DEBHALT,"Card error3\n");

	outb(io_base+ED_P0_CR,ED_CR_RD2 | ED_CR_PAGE_0 | ED_CR_STA);
	barrier();

	outb(io_base+ED_P0_RCR,ED_RCR_MON);
	barrier();
	outb(io_base+ED_P0_DCR, ED_DCR_FT1 | ED_DCR_LS);

	outb(io_base+ED_P0_PSTART, 8192 >> ED_PAGE_SHIFT);
	outb(io_base+ED_P0_PSTOP, 16384 >> ED_PAGE_SHIFT);

	const uint8_t test_pattern[32] = "THIS is A memory TEST pattern";
	uint8_t test_buffer[32];

	uint16_t buff_start,buff_size;

	writemem(io_base, test_pattern, 8192, sizeof(test_pattern));
	readmem(io_base,test_buffer,8192,sizeof(test_buffer));
	if(memcmp(test_pattern,test_buffer,sizeof(test_pattern)))
	{
		//NE2000?
		outb(io_base+ED_P0_DCR, ED_DCR_FT1 | ED_DCR_LS);

		outb(io_base+ED_P0_PSTART, 16384 >> ED_PAGE_SHIFT);
		outb(io_base+ED_P0_PSTOP, 32768 >> ED_PAGE_SHIFT);
		writemem(io_base, test_pattern, 16384, sizeof(test_pattern));
		readmem(io_base,test_buffer,16384,sizeof(test_buffer));
		if(memcmp(test_pattern,test_buffer,sizeof(test_pattern)))
		{
			bios_printf(BIOS_PRINTF_DEBHALT,"Not working buffer\n");
		}
		bios_printf(BIOS_PRINTF_ALL,"NE2000 buffer space\n");
		buff_start = buff_size = 16384;
	}
	else
	{
		//NE1000
		bios_printf(BIOS_PRINTF_ALL,"NE1000 buffer space\n");
		buff_start = buff_size = 8192;
	}
	barrier();
	outb(io_base+ED_P0_ISR, 0xff);

	uint8_t romdata[16];
	readmem(io_base, romdata, 0, sizeof(romdata));

	uint8_t ethaddr[6];
	for (i = 0; i < 6; i++)
		ethaddr[i] = romdata[2*i];

	bios_printf(BIOS_PRINTF_ALL,"ETHADDR: %2x:%2x:%2x:%2x:%2x:%2x\n",
			ethaddr[0],ethaddr[1],ethaddr[2],ethaddr[3],ethaddr[4],ethaddr[5]);

	return 0;
}

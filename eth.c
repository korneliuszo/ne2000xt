#include "eth.h"
#include "biosint.h"
#include "dp8390reg.h"
#include "inlines.h"
#include "delay.h"
#include <string.h>

uint8_t mac_address[6];

static uint16_t io_base;
static uint16_t io_dma;

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

static inline void eth_outb(uint16_t port, uint8_t val)
{
	eth_barrier();
	eth_outb(io_base+port,val);
}

static inline uint8_t eth_inb(uint16_t port)
{
	eth_barrier();
	return inb(io_base+port);
}

static inline void eth_outdma(uint8_t val)
{
	eth_barrier();
	eth_outb(io_dma,val);
}

static inline uint8_t eth_indma()
{
	eth_barrier();
	return inb(io_dma);
}

void writemem(const uint8_t *src, uint16_t dst, size_t len)
{
	eth_outb(ED_P0_CR,ED_CR_RD2 | ED_CR_PAGE_0 | ED_CR_STA);
	eth_outb(ED_P0_ISR, ED_ISR_RDC);
	eth_outb(ED_P0_RBCR0, len);
	eth_outb(ED_P0_RBCR1, len>>8);
	eth_outb(ED_P0_RSAR0, dst);
	eth_outb(ED_P0_RSAR1, dst>>8);
	eth_outb(ED_P0_CR,ED_CR_RD1 | ED_CR_PAGE_0 | ED_CR_STA);

	for(uint16_t i=0;i<len;i++)
	{
		eth_outdma(src[i]);
	}

	int maxwait = 100;
	while (((eth_inb(ED_P0_ISR) & ED_ISR_RDC) !=
	    ED_ISR_RDC) && --maxwait)
		delay_spin(1);
}

void readmem(uint8_t *src, uint16_t dst, size_t len)
{
	eth_outb(ED_P0_CR,ED_CR_RD2 | ED_CR_PAGE_0 | ED_CR_STA);
	size_t lenr = len + (len&1);
	eth_outb(ED_P0_RBCR0, lenr);
	eth_outb(ED_P0_RBCR1, lenr>>8);
	eth_outb(ED_P0_RSAR0, dst);
	eth_outb(ED_P0_RSAR1, dst>>8);
	eth_outb(ED_P0_CR,ED_CR_RD0 | ED_CR_PAGE_0 | ED_CR_STA);

	for(uint16_t i=0;i<len;i++)
		src[i]=eth_indma();
}


void eth_detect()
{
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
	io_dma = io_base+0x10;
}

void eth_initialize()
{

	eth_outb(0x1f,0);
	delay_spin(PIT_MSC(1));
	uint8_t tmp = eth_inb(0x1f);
	delay_spin(PIT_MSC(10));
	eth_outb(0x1f,tmp);
	delay_spin(PIT_MSC(5));

	eth_outb(ED_P0_CR,ED_CR_RD2 | ED_CR_PAGE_0 | ED_CR_STP);
	delay_spin(PIT_MSC(5));

	tmp = eth_inb(ED_P0_CR);
	if ((tmp & (ED_CR_RD2 | ED_CR_TXP | ED_CR_STA | ED_CR_STP)) !=
		    (ED_CR_RD2 | ED_CR_STP))
		bios_printf(BIOS_PRINTF_DEBHALT,"Card error1\n");

	tmp = eth_inb(ED_P0_ISR);
	if ((tmp & ED_ISR_RST) != ED_ISR_RST)
		bios_printf(BIOS_PRINTF_DEBHALT,"Card error2\n");

	char i;

	for (i = 0; i < 100; i++) {
		if ((eth_inb(ED_P0_ISR) & ED_ISR_RST) ==
		    ED_ISR_RST) {
			/* Ack the reset bit. */
			eth_outb(ED_P0_ISR, ED_ISR_RST);
			break;
		}
		delay_spin(PIT_MSC(1));
	}
	if (i == 100)
		bios_printf(BIOS_PRINTF_DEBHALT,"Card error3\n");

	eth_outb(ED_P0_CR,ED_CR_RD2 | ED_CR_PAGE_0 | ED_CR_STA);

	eth_outb(ED_P0_RCR,ED_RCR_MON);
	eth_outb(ED_P0_DCR, ED_DCR_FT1 | ED_DCR_LS);

	eth_outb(ED_P0_PSTART, 8192 >> ED_PAGE_SHIFT);
	eth_outb(ED_P0_PSTOP, 16384 >> ED_PAGE_SHIFT);

	const uint8_t test_pattern[32] = "THIS is A memory TEST pattern";
	uint8_t test_buffer[32];

	uint16_t buff_start,buff_size;

	writemem(test_pattern, 8192, sizeof(test_pattern));
	readmem(test_buffer,8192,sizeof(test_buffer));
	if(memcmp(test_pattern,test_buffer,sizeof(test_pattern)))
	{
		//NE2000?
		eth_outb(ED_P0_DCR, ED_DCR_FT1 | ED_DCR_LS);

		eth_outb(ED_P0_PSTART, 16384 >> ED_PAGE_SHIFT);
		eth_outb(ED_P0_PSTOP, 32768 >> ED_PAGE_SHIFT);
		writemem(test_pattern, 16384, sizeof(test_pattern));
		readmem(test_buffer,16384,sizeof(test_buffer));
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
	eth_outb(ED_P0_ISR, 0xff);

	uint8_t romdata[16];
	readmem(romdata, 0, sizeof(romdata));

	for (i = 0; i < 6; i++)
		mac_address[i] = romdata[2*i];

	bios_printf(BIOS_PRINTF_ALL,"mac_address: %2x:%2x:%2x:%2x:%2x:%2x\n",
			mac_address[0],mac_address[1],mac_address[2],mac_address[3],mac_address[4],mac_address[5]);

}

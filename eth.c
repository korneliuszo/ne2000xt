#include "eth.h"
#include "biosint.h"
#include "dp8390reg.h"
#include "inlines.h"
#include "delay.h"
#include <string.h>

#define MAX(x, y) (((x) > (y)) ? (x) : (y))

uint8_t mac_address[6];

uint16_t io_base;
uint16_t io_dma;

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

uint16_t buff_start,buff_size;

uint16_t tx_page_start;

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

	uint16_t i;

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

	static const uint8_t test_pattern[32] = "THIS is A memory TEST pattern";
	static uint8_t test_buffer[32];

	writemem(test_pattern, 8192, sizeof(test_pattern));
	readmem(test_buffer,8192,sizeof(test_buffer));
	if(memcmp(test_pattern,test_buffer,sizeof(test_pattern)))
	{
		//NE2000?
		eth_outb(ED_P0_DCR, ED_DCR_FT1 | ED_DCR_LS);

		eth_outb(ED_P0_PSTART, 16384 >> ED_PAGE_SHIFT);
		// RTL8019 support
		//eth_outb(ED_P0_PSTOP, 32768 >> ED_PAGE_SHIFT);
		eth_outb(ED_P0_PSTOP, 24576 >> ED_PAGE_SHIFT);
		writemem(test_pattern, 16384, sizeof(test_pattern));
		readmem(test_buffer,16384,sizeof(test_buffer));
		if(memcmp(test_pattern,test_buffer,sizeof(test_pattern)))
		{
			bios_printf(BIOS_PRINTF_DEBHALT,"Not working buffer\n");
		}
		bios_printf(BIOS_PRINTF_ALL,"NE2000 buffer space\n");
		buff_start = 16384;
		buff_size = 8192; //RTL8019 support
	}
	else
	{
		//NE1000
		bios_printf(BIOS_PRINTF_ALL,"NE1000 buffer space\n");
		buff_start = buff_size = 8192;
	}
	eth_outb(ED_P0_ISR, 0xff);

	static uint8_t romdata[16];
	readmem(romdata, 0, sizeof(romdata));

	for (i = 0; i < 6; i++)
		mac_address[i] = romdata[2*i];

	bios_printf(BIOS_PRINTF_ALL,"mac_address: %2x:%2x:%2x:%2x:%2x:%2x\n",
			mac_address[0],mac_address[1],mac_address[2],mac_address[3],mac_address[4],mac_address[5]);

	eth_outb(ED_P0_CR,ED_CR_RD2 | ED_CR_PAGE_0 | ED_CR_STP);

	delay_spin(PIT_MSC(5));

	tmp = eth_inb(ED_P0_ISR);
	if ((tmp & ED_ISR_RST) == ED_ISR_RST)
		bios_printf(BIOS_PRINTF_DEBHALT,"Card error4\n");

	eth_outb(ED_P0_DCR, ED_DCR_FT1 | ED_DCR_LS);

	tx_page_start = buff_start >> ED_PAGE_SHIFT;
	uint16_t rx_page_start = tx_page_start + ED_TXBUF_SIZE;
	uint16_t rx_page_stop = tx_page_start + (buff_size >> ED_PAGE_SHIFT);
	//uint16_t mem_ring = buff_start + (ED_TXBUF_SIZE << ED_PAGE_SHIFT);
	//uint16_t mem_end = buff_start + buff_size;

	eth_outb(ED_P0_RBCR0, 0);
	eth_outb(ED_P0_RBCR1, 0);

	eth_outb(ED_P0_RCR, ED_RCR_MON);

	eth_outb(ED_P0_TCR, ED_TCR_LB0);

	eth_outb(ED_P0_BNRY, rx_page_start);
	eth_outb(ED_P0_PSTART, rx_page_start);
	eth_outb(ED_P0_PSTOP, rx_page_stop);

	eth_outb(ED_P0_IMR,0);
	eth_outb(ED_P0_ISR,0xff);

	eth_outb(ED_P0_CR,ED_CR_RD2 | ED_CR_PAGE_1 | ED_CR_STP);

	for (i = 0; i < 6; ++i)
		eth_outb(ED_P1_PAR0 + i,
		    mac_address[i]);

	for (i = 0; i < 8; i++)
		eth_outb(ED_P1_MAR0 + i, 0);

	eth_outb(ED_P1_CURR, rx_page_start + 1);

	eth_outb(ED_P0_CR,ED_CR_RD2 | ED_CR_PAGE_0 | ED_CR_STP);

	eth_outb(ED_P0_RCR, ED_RCR_AB);

	eth_outb(ED_P0_TCR, 0);

	eth_outb(ED_P0_CR,ED_CR_RD2 | ED_CR_PAGE_0 | ED_CR_STA);
}

uint32_t local_ip = 0;

void start_send_udp(udp_conn *conn, uint16_t len)
{
	uint16_t eth_len = len + 14 + 20 + 8;
	uint16_t ip_len = len + 20 + 8;
	uint16_t udp_len = len + 8;

	//TODO: check PTX

	eth_outb(ED_P0_CR,ED_CR_RD2 | ED_CR_PAGE_0 | ED_CR_STA);
	eth_outb(ED_P0_ISR, ED_ISR_RDC);
	eth_outb(ED_P0_RBCR0, eth_len);
	eth_outb(ED_P0_RBCR1, eth_len>>8);
	eth_outb(ED_P0_RSAR0, buff_start); // hidden assumption that tx buff is
	eth_outb(ED_P0_RSAR1, buff_start>>8); // on start of memory
	eth_outb(ED_P0_CR,ED_CR_RD1 | ED_CR_PAGE_0 | ED_CR_STA);

	for(uint16_t i=0;i<6;i++)
		eth_outdma(conn->remote_mac[i]);
	for(uint16_t i=0;i<6;i++)
		eth_outdma(mac_address[i]);
	eth_outdma(0x08);
	eth_outdma(0x00); //IPV4

	union {
		uint16_t words[10];
		uint8_t bytes[20];
	} ip_hdr;

	ip_hdr.words[0]=0x0045;
	ip_hdr.words[1]=swap_16(ip_len);
	ip_hdr.words[2]=0x0000; //ID
	ip_hdr.words[3]=0x0000; //Flags
	ip_hdr.words[4]=0x11fa;
	ip_hdr.words[5]=0x0000; //checksum
	ip_hdr.words[6]=local_ip>>16;
	ip_hdr.words[7]=local_ip;
	ip_hdr.words[8]=conn->remote_ip>>16;
	ip_hdr.words[9]=conn->remote_ip;

	uint16_t chksum =0;
	for(uint16_t i=0;i<10;i++)
		chksum+=ip_hdr.words[i];
	ip_hdr.words[5]=chksum; //checksum
	for(uint16_t i=0;i<20;i++)
		eth_outdma(ip_hdr.bytes[i]);

	eth_outdma(conn->local_port>>8);
	eth_outdma(conn->local_port);
	eth_outdma(conn->remote_port>>8);
	eth_outdma(conn->remote_port);
	eth_outdma(udp_len>>8);
	eth_outdma(udp_len);
	eth_outdma(0x00);
	eth_outdma(0x00);
}

void fin_send_udp(uint16_t len)
{
	uint16_t eth_len = len + 14 + 20 + 8;
	eth_len = MAX(eth_len, 64-4);

	while (((eth_inb(ED_P0_ISR) & ED_ISR_RDC) !=
	    ED_ISR_RDC));

	eth_outb(ED_P0_CR,ED_CR_RD2 | ED_CR_PAGE_0 | ED_CR_STA);
	eth_outb(ED_P0_TPSR,tx_page_start);
	eth_outb(ED_P0_TBCR0,eth_len);
	eth_outb(ED_P0_TBCR1,eth_len>>8);
	eth_outb(ED_P0_CR,ED_CR_RD2 | ED_CR_PAGE_0 | ED_CR_TXP | ED_CR_STA);

}

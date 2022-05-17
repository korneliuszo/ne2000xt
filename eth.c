#include "eth.h"
#include "biosint.h"
#include "dp8390reg.h"
#include "inlines.h"
#include "delay.h"
#include <string.h>
#include <stdbool.h>

#define MAX(x, y) (((x) > (y)) ? (x) : (y))

uint8_t mac_address[6];

uint16_t io_base;
uint16_t io_dma;

uint16_t rx_buff_len, rx_buff2_len;
uint16_t rx_off;
uint16_t rx_pkt_off;
uint8_t next_pkt;

void writemem(const uint8_t *src, uint16_t dst, size_t len)
{
	eth_outb(ED_P0_CR,ED_CR_RD2 | ED_CR_PAGE_0 | ED_CR_STA);
	eth_outb(ED_P0_ISR, ED_ISR_RDC);
	eth_outb(ED_P0_RBCR0, len);
	eth_outb(ED_P0_RBCR1, len>>8);
	eth_outb(ED_P0_RSAR0, dst);
	eth_outb(ED_P0_RSAR1, dst>>8);
	eth_outb(ED_P0_CR,ED_CR_RD1 | ED_CR_PAGE_0 | ED_CR_STA);

	prepare_dma();
	for(uint16_t i=0;i<len;i++)
	{
		eth_barrier();
		outb(io_dma_local,src[i]);
	}

	int maxwait = 100;
	while (((eth_inb(ED_P0_ISR) & ED_ISR_RDC) !=
			ED_ISR_RDC) && --maxwait)
		delay_spin(1);
}

#pragma aux insw \
		parm [es di] [cx] [dx] \
		modify [ax cx di bx];
void insw(uint8_t far *buff,size_t len, uint16_t port);

#pragma aux inswf \
		parm [es di] [cx] [dx] \
		modify [ax cx di bx];
void inswf(uint8_t far *buff,size_t len, uint16_t port);

void readmem(uint8_t *src, uint16_t dst, size_t len)
{
	eth_outb(ED_P0_CR,ED_CR_RD2 | ED_CR_PAGE_0 | ED_CR_STA);
	size_t lenr = len + (len&1);
	eth_outb(ED_P0_RBCR0, lenr);
	eth_outb(ED_P0_RBCR1, lenr>>8);
	eth_outb(ED_P0_RSAR0, dst);
	eth_outb(ED_P0_RSAR1, dst>>8);
	eth_outb(ED_P0_CR,ED_CR_RD0 | ED_CR_PAGE_0 | ED_CR_STA);

	insw(src,len,io_dma);
}

void eth_detect()
{
	for(io_base=0x200;io_base<0x400;io_base+=0x20)
	{
		uint8_t reg0 = inb(io_base);
		if(reg0 == 0xff)
			continue;
		outb(io_base+ED_P0_CR,ED_CR_RD2 | ED_CR_PAGE_0 | ED_CR_STP);
		inb(io_base + ED_P0_CNTR0);
		if(inb(io_base + ED_P0_CNTR0) != 0)
		{
			outb(io_base+ED_P0_CR,reg0);
			continue;
		}
		break;
	};
	if(io_base == 0x400)
	{
		bios_printf(BIOS_PRINTF_DEBHALT,"Card not found\n");
	}
	uint8_t id0,id1;

	id0 = inb(io_base+0x0A);
	id1 = inb(io_base+0x0B);

	bios_printf(BIOS_PRINTF_ALL,"Found card at:0x%x ID:0x%x\n",io_base,id0 | (id1<<8));
	io_dma = io_base+0x10;
}

uint16_t buff_start,buff_size;

uint8_t tx_page_start;
uint8_t rx_page_start,rx_page_stop;

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

	const uint8_t test_pattern[32] = "THIS is A memory TEST pattern";
	uint8_t test_buffer[32];

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

	uint8_t romdata[16];
	readmem(romdata, 0, sizeof(romdata));

	for (i = 0; i < 6; i++)
		mac_address[i] = romdata[2*i];

	bios_printf(BIOS_PRINTF_ALL,"mac_address: %2x:%2x:%2x:%2x:%2x:%2x\n",
			mac_address[0],mac_address[1],mac_address[2],mac_address[3],mac_address[4],mac_address[5]);

	eth_outb(ED_P0_CR,ED_CR_RD2 | ED_CR_PAGE_0 | ED_CR_STP);

	delay_spin(PIT_MSC(5)); // condition below can allow shorter delay

	/* check on 86box not passing
	tmp = eth_inb(ED_P0_ISR);
	if ((tmp & ED_ISR_RST) == ED_ISR_RST)
		bios_printf(BIOS_PRINTF_DEBHALT,"Card error4\n");
	 */
	eth_outb(ED_P0_DCR, ED_DCR_FT1 | ED_DCR_LS);

	tx_page_start = buff_start >> ED_PAGE_SHIFT;
	rx_page_start = tx_page_start + ED_TXBUF_SIZE;
	rx_page_stop = tx_page_start + (buff_size >> ED_PAGE_SHIFT);
	//uint16_t mem_ring = buff_start + (ED_TXBUF_SIZE << ED_PAGE_SHIFT);
	//uint16_t mem_end = buff_start + buff_size;

	eth_outb(ED_P0_RBCR0, 0);
	eth_outb(ED_P0_RBCR1, 0);

	eth_outb(ED_P0_RCR, ED_RCR_MON);

	eth_outb(ED_P0_TCR, ED_TCR_LB0);

	eth_outb(ED_P0_BNRY, rx_page_stop-1);
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

	eth_outb(ED_P1_CURR, rx_page_start);

	eth_outb(ED_P0_CR,ED_CR_RD2 | ED_CR_PAGE_0 | ED_CR_STP);

	eth_outb(ED_P0_RCR, ED_RCR_AB);

	eth_outb(ED_P0_TCR, 0);

	eth_outb(ED_P0_CR,ED_CR_RD2 | ED_CR_PAGE_0 | ED_CR_STA);
}

uint8_t local_ip[4] = {0};

bool first_tx = true;

void start_send_udp(udp_conn *conn, uint16_t len)
{
	uint16_t eth_len = len + 14 + 20 + 8;
	uint16_t ip_len = len + 20 + 8;
	uint16_t udp_len = len + 8;

	while(!first_tx) {
		if ((eth_inb(ED_P0_ISR) & ED_ISR_PTX) ==
				ED_ISR_PTX) {
			/* Ack the reset bit. */
			eth_outb(ED_P0_ISR, ED_ISR_PTX);
			break;
		}
	}

	first_tx = false;

	eth_outb(ED_P0_CR,ED_CR_RD2 | ED_CR_PAGE_0 | ED_CR_STA);
	eth_outb(ED_P0_ISR, ED_ISR_RDC);
	eth_outb(ED_P0_RBCR0, eth_len);
	eth_outb(ED_P0_RBCR1, eth_len>>8);
	eth_outb(ED_P0_RSAR0, buff_start); // hidden assumption that tx buff is
	eth_outb(ED_P0_RSAR1, buff_start>>8); // on start of memory
	eth_outb(ED_P0_CR,ED_CR_RD1 | ED_CR_PAGE_0 | ED_CR_STA);

	prepare_dma();
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
	ip_hdr.bytes[12] = local_ip[0];
	ip_hdr.bytes[13] = local_ip[1];
	ip_hdr.bytes[14] = local_ip[2];
	ip_hdr.bytes[15] = local_ip[3];
	ip_hdr.bytes[16] = conn->remote_ip[0];
	ip_hdr.bytes[17] = conn->remote_ip[1];
	ip_hdr.bytes[18] = conn->remote_ip[2];
	ip_hdr.bytes[19] = conn->remote_ip[3];

	uint32_t chksum =0;
	for(uint16_t i=0;i<10;i++)
		chksum+=swap_16(ip_hdr.words[i]);
	chksum=(chksum&0xffff)+(chksum>>16);
	uint16_t chksumf=chksum&0xffff+(chksum>>16);
	ip_hdr.words[5]=swap_16(chksumf^0xffff); //checksum
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

uint8_t assigned_ip[4];

void send_dhcp_packet(uint8_t type, uint8_t serverip[4])
{
	udp_conn conn = {
			{0xff,0xff,0xff,0xff,0xff,0xff},
			{0xff,0xff,0xff,0xff},
			67,
			68,
	};
	uint16_t len = serverip ? (256) : 250;
	start_send_udp(&conn,280);
	prepare_dma();
	eth_outdma(0x01); //type
	eth_outdma(0x01); //hwtype
	eth_outdma(0x06); //hwlen
	eth_outdma(0x00); //hops
	eth_outdma(0xBA); //transid
	eth_outdma(0xDB);
	eth_outdma(0xEE);
	eth_outdma(0xEF);
	eth_outdma(0x00); //seconds
	eth_outdma(0x00);
	eth_outdma(0x00); //flags
	eth_outdma(0x00);
	eth_outdma(assigned_ip[0]); //ciaddr
	eth_outdma(assigned_ip[1]);
	eth_outdma(assigned_ip[2]);
	eth_outdma(assigned_ip[3]);
	eth_outdma(0x00); //yiaddr
	eth_outdma(0x00);
	eth_outdma(0x00);
	eth_outdma(0x00);
	eth_outdma(0x00); //siaddr
	eth_outdma(0x00);
	eth_outdma(0x00);
	eth_outdma(0x00);
	eth_outdma(0x00); //giaddr
	eth_outdma(0x00);
	eth_outdma(0x00);
	eth_outdma(0x00);
	for(uint16_t i=0;i<6;i++) //chaddr
		eth_outdma(mac_address[i]);
	for(uint16_t i=6;i<16;i++) //chaddr
		eth_outdma(0x00);
	for(uint16_t i=0;i<64+128;i++) //sname+file
		eth_outdma(0x00);
	eth_outdma(0x63); //cookie
	eth_outdma(0x82);
	eth_outdma(0x53);
	eth_outdma(0x63);
	eth_outdma(0x35); //message_type
	eth_outdma(0x01);
	eth_outdma(type); //DISCOVER
	eth_outdma(0x0c); // hostname
	eth_outdma(4);
	eth_outdma('X');
	eth_outdma('T');
	eth_outdma('N');
	eth_outdma('E');
	if (serverip)
	{
		eth_outdma(0x36); // sel server
		eth_outdma(4);
		eth_outdma(serverip[0]);
		eth_outdma(serverip[1]);
		eth_outdma(serverip[2]);
		eth_outdma(serverip[3]);
	}
	eth_outdma(0xff); //end

	for(int i= len;i<280;i++)
		eth_outdma(0x00); //pad

	fin_send_udp(280);
}

bool offered = false;

void send_dhcp_discover()
{
	offered=false;
	assigned_ip[0]=0;
	assigned_ip[1]=0;
	assigned_ip[2]=0;
	assigned_ip[3]=0;
	send_dhcp_packet(0x01,NULL);
}

void read_prepare()
{
	if (rx_off >= rx_buff_len)
	{
		rx_off = 0;
		rx_pkt_off = rx_page_start << 8;
		rx_buff_len = rx_buff2_len;
	}
	eth_outb(ED_P0_CR,ED_CR_RD2 | ED_CR_PAGE_0 | ED_CR_STA);
	uint16_t dst = rx_pkt_off;
	uint16_t len = rx_buff_len;
	size_t lenr = len + (len&1);
	eth_outb(ED_P0_RBCR0, lenr);
	eth_outb(ED_P0_RBCR1, lenr>>8);
	eth_outb(ED_P0_RSAR0, dst);
	eth_outb(ED_P0_RSAR1, dst>>8);
	eth_outb(ED_P0_CR,ED_CR_RD0 | ED_CR_PAGE_0 | ED_CR_STA);
}

bool start_recv()
{
	bool ret;
	if((eth_inb(ED_P0_ISR) & ED_ISR_PRX) != ED_ISR_PRX)
		return false;
	uint8_t next = eth_inb(ED_P0_BNRY) +1;
	if (next >= rx_page_stop) next = rx_page_start;
	eth_outb(ED_P0_CR,ED_CR_RD2 | ED_CR_PAGE_1 | ED_CR_STA);
	uint8_t curr = eth_inb(ED_P1_CURR);
	eth_outb(ED_P0_CR,ED_CR_RD2 | ED_CR_PAGE_0 | ED_CR_STA);
	if (curr >= rx_page_stop) curr = rx_page_start;
	if (curr == next) return false;

	eth_outb(ED_P0_RBCR0, 4);
	eth_outb(ED_P0_RBCR1, 0);
	eth_outb(ED_P0_RSAR0, 0);
	eth_outb(ED_P0_RSAR1, next);
	eth_outb(ED_P0_CR,ED_CR_RD0 | ED_CR_PAGE_0 | ED_CR_STA);
	prepare_dma();

	eth_barrier();
	uint8_t status=eth_indma(io_dma_local);
	eth_barrier();
	next_pkt=eth_indma(io_dma_local);
	eth_barrier();
	uint8_t len_lsb = eth_indma(io_dma_local);
	eth_barrier();
	uint8_t len_msb = eth_indma(io_dma_local);
	uint16_t len = (len_msb << 8) | len_lsb;
	if((status & ED_RSR_PRX == 0) || len < 64 || len > 1522)
	{
		ret = false;
	}
	else
	{
		rx_pkt_off = next << 8 | 4;
		len -= 4;
		uint16_t frag = (rx_page_stop << 8) - rx_pkt_off;
		if(len > frag)
		{
			rx_buff2_len = len-frag;
			len = frag;
		}
		rx_buff_len = len;
		rx_off = 0;
		read_prepare();
		ret=true;
	}
	return ret;
}

void recv_end()
{
	if (next_pkt == rx_page_start)
		next_pkt=rx_page_stop;
	eth_outb(ED_P0_BNRY, next_pkt-1);
}

uint16_t process_eth(udp_conn * conn)
{
	prepare_dma();
	for(int i=0;i<6;i++)
	{
		eth_indma(); // drop eth header
	}
	for(int i=0;i<6;i++)
		conn->remote_mac[i]=eth_indma();
	uint8_t proto_b = eth_indma();
	uint8_t proto_l = eth_indma();
	rx_off += 14;
	return (proto_b<<8) | proto_l;
}


bool decode_ip_udp(uint16_t local_port, uint16_t *len, udp_conn * conn)
{
	//if (rx_pkt[12] != 0x08 || rx_pkt[13] != 0x00) // is IP
	//	return NULL;
	prepare_dma();
	if (eth_indma() != 0x45) // no fancy options
		goto rx_fail;
	for(int i=0xF;i<0x17;i++)
	{
		eth_indma();
	}
	if (eth_indma() != 0x11) // udp
		goto rx_fail;
	eth_indma();
	eth_indma(); //checksum

	conn->remote_ip[0]=eth_indma();
	conn->remote_ip[1]=eth_indma();
	conn->remote_ip[2]=eth_indma();
	conn->remote_ip[3]=eth_indma();

	eth_indma();
	eth_indma();
	eth_indma();
	eth_indma(); //local_ip

	uint8_t remote_port_b = eth_indma();
	uint8_t remote_port_l = eth_indma();
	conn->remote_port = (remote_port_b<<8) | remote_port_l;
	if (eth_indma() != local_port>>8)
		goto rx_fail;
	if (eth_indma() != (uint8_t)local_port)
		goto rx_fail;
	conn->local_port = local_port;
	uint8_t len_b = eth_indma();
	uint8_t len_l = eth_indma();
	*len = ((len_b<<8)|len_l)-8;
	eth_indma();
	eth_indma();//checksum
	rx_off += 20+8;
	return true;
	rx_fail:
	recv_end();
	return false;
}

void paste_restpacket(uint8_t far *buff,size_t len)
{
	uint16_t off=0;
	uint16_t head=rx_buff_len - rx_off;
	if (len > head)
	{
		insw(buff,head,io_dma);
		off=head;
		len-=head;
		rx_off +=head;
		read_prepare();
	}
	insw(&buff[off],len,io_dma);

}

void paste_restpacket_flipped(uint8_t far *buff,size_t len)
{
	uint16_t off=0;
	uint16_t head=rx_buff_len - rx_off;
	if (len > head)
	{
		inswf(buff,head,io_dma);
		off=head;
		len-=head;
		rx_off +=head;
		read_prepare();
	}
	inswf(&buff[off],len,io_dma);

}

bool dhcp_poll()
{
	if(start_recv())
	{
		udp_conn recv_conn;
		if(process_eth(&recv_conn) == 0x0800)
		{
			uint16_t udp_len;

			if((decode_ip_udp(68,&udp_len,&recv_conn)))
			{
				uint8_t udp_pkt[1024];
				paste_restpacket(udp_pkt,udp_len);
				recv_end();

				if(udp_pkt[0] != 0x02)
					return false;
				if(udp_pkt[0xEC] != 0x63)
					return false;
				if(udp_pkt[0xED] != 0x82)
					return false;
				if(udp_pkt[0xEE] != 0x53)
					return false;
				if(udp_pkt[0xEF] != 0x63)
					return false;
				uint16_t off=0xF0;
				uint8_t msg_type = 0;
				while(!msg_type)
				{
					switch(udp_pkt[off])
					{
					case 0xff:
						return false;
					case 0x35:
						msg_type=udp_pkt[off+2];
						break;
					default:
						off+=2+udp_pkt[off+1];
					}
					if(off > udp_len)
						return false;
				}
				if(msg_type == 2)
				{
					if(!offered)
					{
						offered=true;
						assigned_ip[0]=udp_pkt[0x10];
						assigned_ip[1]=udp_pkt[0x11];
						assigned_ip[2]=udp_pkt[0x12];
						assigned_ip[3]=udp_pkt[0x13];

						send_dhcp_packet(3, recv_conn.remote_ip);
					}
				}
				if(msg_type == 5)
				{
					local_ip[0]=udp_pkt[0x10];
					local_ip[1]=udp_pkt[0x11];
					local_ip[2]=udp_pkt[0x12];
					local_ip[3]=udp_pkt[0x13];

					return true;
				}
			}

		}
		else
		{
			recv_end();
		}
	}
	return false;
}

void process_arp(udp_conn * conn)
{

	//if (rx_pkt[12] != 0x08 || rx_pkt[13] != 0x06) // is ARP
	//	return false;
	prepare_dma();
	eth_indma();
	eth_indma();//hw
	eth_indma();
	eth_indma();//proto
	eth_indma();//hwsize
	eth_indma();//protosize

	if (eth_indma() != 0x00 || eth_indma() != 0x01) // is request
		goto err;
	eth_indma();
	eth_indma();
	eth_indma();
	eth_indma();
	eth_indma();
	eth_indma();//smac
	uint8_t rip[4];
	rip[0]=eth_indma();
	rip[1]=eth_indma();
	rip[2]=eth_indma();
	rip[3]=eth_indma();//sip
	eth_indma();
	eth_indma();
	eth_indma();
	eth_indma();
	eth_indma();
	eth_indma();//tmac

	if (eth_indma() != (uint8_t)(local_ip[0])) // to us?
		goto err;
	if (eth_indma() != (uint8_t)(local_ip[1])) // to us?
		goto err;
	if (eth_indma() != (uint8_t)(local_ip[2])) // to us?
		goto err;
	if (eth_indma() != (uint8_t)(local_ip[3])) // to us?
		goto err;
	recv_end();

	//reply

	while(!first_tx) {
		if ((eth_inb(ED_P0_ISR) & ED_ISR_PTX) ==
				ED_ISR_PTX) {
			/* Ack the reset bit. */
			eth_outb(ED_P0_ISR, ED_ISR_PTX);
			break;
		}
	}

	first_tx = false;

	eth_outb(ED_P0_CR,ED_CR_RD2 | ED_CR_PAGE_0 | ED_CR_STA);
	eth_outb(ED_P0_ISR, ED_ISR_RDC);
	eth_outb(ED_P0_RBCR0, 42);
	eth_outb(ED_P0_RBCR1, 00);
	eth_outb(ED_P0_RSAR0, buff_start); // hidden assumption that tx buff is
	eth_outb(ED_P0_RSAR1, buff_start>>8); // on start of memory
	eth_outb(ED_P0_CR,ED_CR_RD1 | ED_CR_PAGE_0 | ED_CR_STA);

	for(uint16_t i=0;i<6;i++)
		eth_outdma(conn->remote_mac[i]);
	for(uint16_t i=0;i<6;i++)
		eth_outdma(mac_address[i]);
	eth_outdma(0x08);
	eth_outdma(0x06); //ARP
	eth_outdma(0x00);
	eth_outdma(0x01); //proto Eth
	eth_outdma(0x08);
	eth_outdma(0x00); //proto ip
	eth_outdma(0x06); //hwsize
	eth_outdma(0x04); //ptsize
	eth_outdma(0x00);
	eth_outdma(0x02); //reply
	for(uint16_t i=0;i<6;i++)
		eth_outdma(mac_address[i]);
	eth_outdma(local_ip[0]); //localip
	eth_outdma(local_ip[1]);
	eth_outdma(local_ip[2]);
	eth_outdma(local_ip[3]);
	for(uint16_t i=0;i<6;i++)
		eth_outdma(conn->remote_mac[i]);
	eth_outdma(rip[0]); //targetip
	eth_outdma(rip[1]);
	eth_outdma(rip[2]);
	eth_outdma(rip[3]);

	while (((eth_inb(ED_P0_ISR) & ED_ISR_RDC) !=
			ED_ISR_RDC));

	eth_outb(ED_P0_CR,ED_CR_RD2 | ED_CR_PAGE_0 | ED_CR_STA);
	eth_outb(ED_P0_TPSR,tx_page_start);
	eth_outb(ED_P0_TBCR0,60);
	eth_outb(ED_P0_TBCR1,0);
	eth_outb(ED_P0_CR,ED_CR_RD2 | ED_CR_PAGE_0 | ED_CR_TXP | ED_CR_STA);

	return;
err:
	recv_end();
	return;
}

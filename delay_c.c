
#include "delay.h"
#include "inlines.h"
#include <stdint.h>

volatile uint16_t ctr;

int delay_c_isr()
{
	ctr++;
	return 0;
}

uint16_t get_ctr()
{
	uint16_t ret;
	int_disable();
	ret=ctr;
	int_enable();
	return ret;
}

extern void dch( char );
#pragma aux dch = \
	"mov ah, 0Ah" \
	"mov bx, 00h" \
	"mov cx, 01h" \
	"int 10h" \
	parm [al] modify exact [ax bx cx];

void delay_spin(uint16_t time)
{
	int16_t timeout = time + get_ctr();
	int16_t now = get_ctr();
	static char ch = 0;
	char tbl[] = {'/','|','\\','-'};
	do {
		dch(tbl[ch++]);
		ch &= 0x3;
		halt();
		now = get_ctr();
	} while(now < timeout);
}

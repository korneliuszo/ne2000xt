int __cdecl start();

#include <i86.h>
const char * str="Hello World";



int start()
{
	union REGS inregs,outregs;

	short i;	

	for(i=0;i<11;i++)
	{
    	inregs.h.ah = 0x0E;
    	inregs.h.al = (unsigned char)str[i];
		inregs.h.bh = 0;
		inregs.h.bl = 1;

    	int86(0x10,&inregs,&outregs);
	};

	return 0;
}

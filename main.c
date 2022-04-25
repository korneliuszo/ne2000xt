int __cdecl start();

#include <i86.h>
#include <stdint.h>
#include "biosint.h"
const char * str="Hello World\n";

int start()
{
	bios_printf(BIOS_PRINTF_ALL,str);
	return 0;
}

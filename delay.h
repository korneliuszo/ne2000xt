/*
 * delay.h
 *
 *  Created on: Apr 26, 2022
 *      Author: kosa
 */

#ifndef DELAY_H_
#define DELAY_H_

#include <stdint.h>

int __cdecl delay_init();
int __cdecl delay_c_isr();

#define PIT_HZ (18.2065)

#define PIT_MSC(x) (uint16_t)((((x)*PIT_HZ)+999)/1000 + 1)

void delay_spin(uint16_t time);



#endif /* DELAY_H_ */

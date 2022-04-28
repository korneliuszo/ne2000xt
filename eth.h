#ifndef ETH_H_
#define ETH_H_

#include <stdint.h>

void eth_detect();
void eth_initialize();

extern uint8_t mac_address[6];

#endif /* ETH_H_ */

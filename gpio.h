#ifndef __INCLUDED_GPIO_H_PICLOCK
#define __INCLUDED_GPIO_H_PICLOCK

#include <cstdint>

extern void gpio_init(int gpio_type);

extern uint16_t read_gpio(int gpio_type);

#endif

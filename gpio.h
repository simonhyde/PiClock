#ifndef __INCLUDED_GPIO_H_PICLOCK
#define __INCLUDED_GPIO_H_PICLOCK

#include <cstdint>
#include <string>

extern void gpio_init(int gpio_type, const std::string & pull_up_down);

extern uint16_t read_gpi();
extern void write_gpo(int index, bool value);

#endif

#include "gpio.h"
#include "pifacedigital.h"
#include <iostream>

void gpio_init(int gpio_type)
{
    switch(gpio_type)
    {
        case 0://PiFace Digital or other MCP23S17 device
            pifacedigital_open(0);
        default:
            std::cerr << "Unknown GPIO Mode/Type, ignoring: " << gpio_type;
    }
}

uint16_t read_gpio(int gpio_type)
{
    switch(gpio_type)
    {
        case 0://PiFace Digital or other MCP23S17 device
            return pifacedigital_read_reg(INPUT, 0);
        default:
            std::cerr << "Unknown GPIO Mode/Type, ignoring: " << gpio_type;
    }
    return 0;
}

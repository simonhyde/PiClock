#include "gpio.h"
#include "pifacedigital.h"
#include <iostream>
#include <wiringPi.h>

//Store current GPIO type globally
static int gpio_type = -1;

void gpio_init(int _gpio_type, const std::string & pulls)
{
    gpio_type = _gpio_type;
    switch(gpio_type)
    {
        case 0://PiFace Digital or other MCP23S17 device
            /* Could actually use wiringPi to read the PiFace, but it requires individual bits to be
             * read one at a time, each requiring a slow SPI command, so takes 10 times as long to
             * poll the inputs as the native pifacedigital library. Keeping this minimal is
             * important while we're polling the GPIs every frame.
             */
            pifacedigital_open(0);
            break;
        case 1://WiringPi for Raspberry Pi
            wiringPiSetup();
            //Use first 8 pins for input and last 8 for outputs
            for(int i = 0; i < 8; i++)
            {
                pinMode(i, INPUT);
                pinMode(i + 8, OUTPUT);
                if((uint)i < pulls.length())
                {
                    switch(pulls[i])
                    {
                        case 'U':
                            pullUpDnControl(i, PUD_UP);
                            break;
                        case 'D':
                            pullUpDnControl(i, PUD_DOWN);
                            break;
                        case 'O':
                            pullUpDnControl(i, PUD_OFF);
                            break;
                    }
                }
            }
            break;
        default:
            std::cerr << "Unknown GPIO Mode/Type, when initialising, ignoring: " << gpio_type << "\n";
    }
}

uint16_t read_gpi()
{
    int offset = 0;
    switch(gpio_type)
    {
        case 0://PiFace Digital or other MCP23S17 device
            //INPUT macro is overwritten by wiringPi.h, so have to use 0x13
            return pifacedigital_read_reg(/*INPUT*/ 0x13, 0);
        case 1:
            return digitalRead(offset + 0) | (digitalRead(offset + 1)<<1)
                                           | (digitalRead(offset + 2)<<2)
                                           | (digitalRead(offset + 3)<<3)
                                           | (digitalRead(offset + 4)<<4)
                                           | (digitalRead(offset + 5)<<5)
                                           | (digitalRead(offset + 6)<<6)
                                           | (digitalRead(offset + 7)<<7);

        default:
            std::cerr << "Unknown GPIO Mode/Type, when trying to read GPI, ignoring: " << gpio_type << "\n";
    }
    return 0;
}

void write_gpo(int index, bool value)
{
    //Ignore out of range indices
    if(index >=8 || index < 0)
        return;
    int offset = 8;
    int iValue = value?1:0;
    switch(gpio_type)
    {
        case 0://PiFace Digital or other MCP23S17 device
            pifacedigital_digital_write(index, iValue);
            break;
        case 1:
            digitalWrite(offset + index, iValue);
            break;
        default:
            std::cerr << "Unknown GPIO Mode/Type, when trying to write GPO, ignoring: " << gpio_type << "\n";
    }
}

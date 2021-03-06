/* Teensy RawHID example
 * http://www.pjrc.com/teensy/rawhid.html
 * Copyright (c) 2009 PJRC.COM, LLC
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above description, website URL and copyright notice and this permission
 * notice shall be included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "hid_defines.h"
#include "usb.h"

#define CPU_PRESCALE(n)	(CLKPR = 0x80, CLKPR = (n))

volatile uint8_t do_output=0;
uint8_t buffer[64];

int main(void)
{
	int8_t r;

	// set for 16 MHz clock
	CPU_PRESCALE(0);

	// Initialize the USB, and then wait for the host to set configuration.
	// If the Teensy is powered without a PC connected to the USB port,
	// this will wait forever.
	usb_init();
	while (!usb_configured()) /* wait */ ;

	// Wait an extra second for the PC's operating system to load drivers
	// and do whatever it does to actually be ready for input
	_delay_ms(1000);

	while (1) {
		// if received data, do something with it
        
        usbPutstr("Je kiffe les marmottesJe kiffe les marmottesJe kiffe les marmottesJe kiffe les marmottesJe kiffe les marmottesJe kiffe les marmottesJe kiffe les marmottesJe kiffe les marmottes\r\n");
        usbPrintf("test printf %d\r\n", 22);
        _delay_ms(100);
		r = usb_rawhid_recv(buffer, 0);
		if (r > 0) {
            if(buffer[0] == 'b')
                 usb_keyboard_press(KEY_B, KEY_SHIFT);
			buffer[0] = 'a';
            usb_rawhid_send(buffer, 50);
		}
	}
}




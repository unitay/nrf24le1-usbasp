#ifndef __WIRING_H__
#define __WIRING_H__


#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

/* nrf24LE1 required signals */
#define WIRING_NRF_PROG_PIN		24
#define WIRING_NRF_RESET_PIN	25

/* Wiring functions for bootstraping SPI */
void wiring_init(void);
void wiring_destroy(void);

/* Full-duplex read and write function */
uint8_t wiring_write_then_read(uint8_t* out, 
	                           uint16_t out_len, 
	                   		   uint8_t* in, 
	                   		   uint16_t in_len);

/* Function for setting gpio values */
void wiring_set_gpio_value(uint8_t pin, uint8_t state);

#endif

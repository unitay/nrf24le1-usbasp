/* Name: main.c
nrf24le1 Downloader USBasp
 */

/*
---
*/
#define SPI_PORTX   PORTB
#define SPI_DDRX    DDRB

#define SPI_MISO   4 
#define SPI_MOSI   3
#define SPI_SCK    5
#define SPI_SS     2

#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>  /* for sei() */
#include <util/delay.h>     /* for delay */

#include <avr/pgmspace.h>   /* required by usbdrv.h */
#include "usbdrv.h"
#include "requests.h"       /* The custom request numbers we use */

//------------------------------------------------
//------------------Подпрограммы------------------
//------------------------------------------------
#if HIDMODE
PROGMEM const char usbHidReportDescriptor[22] = {   /* USB report descriptor */
    0x06, 0x00, 0xff,              // USAGE_PAGE (Generic Desktop)
    0x09, 0x01,                    // USAGE (Vendor Usage 1)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x26, 0xff, 0x00,              //   LOGICAL_MAXIMUM (255)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x09, 0x00,                    //   USAGE (Undefined)
    0xb2, 0x02, 0x01,              //   FEATURE (Data,Var,Abs,Buf)
    0xc0                           // END_COLLECTION
};
#endif

/* ------------------------------------------------------------------------- */
/* ----------------------------- USB interface ----------------------------- */
/* ------------------------------------------------------------------------- */

usbMsgLen_t usbFunctionSetup(uchar data[8])
{
usbRequest_t    *rq = (void *)data;
static uchar    dataBuffer[32];  /* buffer must stay valid when usbFunctionSetup returns */

    if(rq->bRequest == 0){ /* echo -- used for reliability tests */
      
        dataBuffer[0] = rq->wValue.bytes[0];
        dataBuffer[1] = rq->wValue.bytes[1];
        dataBuffer[2] = rq->wIndex.bytes[0];
        dataBuffer[3] = rq->wIndex.bytes[1];
        usbMsgPtr = dataBuffer;       /* tell the driver which data to return */
        return 4;
  
}else if(rq->bRequest == 2){ /*отослать или получить байт данных по SPI*/
 

   SPDR = rq->wValue.bytes[0];
   while(!(SPSR & (1<<SPIF)));
   dataBuffer[0] = SPDR;
   
   usbMsgPtr = dataBuffer;

   return 1;
}else if(rq->bRequest ==3){ /*отослать 1 байт данных по SPI + SS */

   SPI_PORTX &= ~(1<<SPI_SS);
   SPDR = rq->wValue.bytes[0];
   while(!(SPSR & (1<<SPIF)));
   SPI_PORTX |= (1<<SPI_SS);
   
}else if(rq->bRequest == 4){ /*отослать и получить 1 байт данных по SPI + SS */
  
   SPI_PORTX &= ~(1<<SPI_SS);
   
   SPDR = rq->wValue.bytes[0];
   while(!(SPSR & (1<<SPIF)));
   
   SPDR = rq->wValue.bytes[0];
   while(!(SPSR & (1<<SPIF)));
   dataBuffer[0] = SPDR;
   SPI_PORTX |= (1<<SPI_SS);
   
   usbMsgPtr = dataBuffer;

   return 1;
   
 }else if(rq->bRequest == 51){ // прочитать 32 байта по SPI
   int i ;
   for (i=0;i<32;i++) {
   SPDR = 0;
   while(!(SPSR & (1<<SPIF)));
   dataBuffer[i] = SPDR;
   }
   
   usbMsgPtr = dataBuffer;

   return 32; 
   
 }else if(rq->bRequest == 52){ /*отослать 4 байта данных по SPI*/
 

   SPDR = rq->wValue.bytes[0];
   while(!(SPSR & (1<<SPIF)));

   
   SPDR = rq->wValue.bytes[1];
   while(!(SPSR & (1<<SPIF)));

   SPDR =rq->wIndex.bytes[0];
   while(!(SPSR & (1<<SPIF)));
   
   SPDR =rq->wIndex.bytes[1];
   while(!(SPSR & (1<<SPIF)));

 return 0;
   
 
}else if(rq->bRequest == 11){ /* Включить PROG*/

PORTD |= (1<<1);
  
}else if(rq->bRequest == 10){ /* Выключить PROG*/

PORTD &= ~(1<<1);

}else if(rq->bRequest == 21){ /* Включить SS */

SPI_PORTX &= ~(1<<SPI_SS);
  
}else if(rq->bRequest == 20){ /* Выключить SS */
  
SPI_PORTX |= (1<<SPI_SS);
}else if(rq->bRequest == 40){ /* Выключить SPI */

SPI_DDRX = (0<<SPI_MOSI)|(0<<SPI_SCK)|(0<<SPI_SS)|(0<<SPI_MISO);
SPI_PORTX = (0<<SPI_MOSI)|(0<<SPI_SCK)|(0<<SPI_SS)|(0<<SPI_MISO);
  
}else if(rq->bRequest == 41){ /* Включить SPI */
  
SPI_DDRX = (1<<SPI_MOSI)|(1<<SPI_SCK)|(1<<SPI_SS)|(0<<SPI_MISO);
SPI_PORTX = (1<<SPI_MOSI)|(1<<SPI_SCK)|(1<<SPI_SS)|(1<<SPI_MISO);
  
    }

    return 0;   /* default for not implemented requests: return no data back to host */
}

/* ------------------------------------------------------------------------- */

int __attribute__((noreturn)) main(void)
{
uchar i;

    wdt_enable(WDTO_1S);

    usbInit();
    usbDeviceDisconnect();  /* enforce re-enumeration, do this while interrupts are disabled! */
    i = 0;
    while(--i){             /* fake USB disconnect for > 250 ms */
        wdt_reset();
        _delay_ms(1);
    }
    usbDeviceConnect();

    
    DDRC |= _BV(0); //1  /* светодиоды на выход */
    DDRC |= _BV(1); //2
    PORTC |= _BV(0); // 1 /* светодиоды погасить */
    PORTC |= _BV(1); // 2 

    
    DDRD |= _BV(0); //7
    DDRD |= _BV(1); //8
  //  PORTC &= ~_BV(0); // 0
  //  PORTC &= ~_BV(1); // 0
    

   sei();

  /*настройка портов ввода-вывода
  все выводы, кроме MISO выходы*/
  
  SPI_DDRX = (1<<SPI_MOSI)|(1<<SPI_SCK)|(1<<SPI_SS)|(0<<SPI_MISO);
  SPI_PORTX = (1<<SPI_MOSI)|(1<<SPI_SCK)|(1<<SPI_SS)|(1<<SPI_MISO);

  SPCR = (1<<SPE)|(0<<DORD)|(1<<MSTR)|(0<<CPOL)|(0<<CPHA)|(1<<SPR1)|(0<<SPR0);

  SPSR = (0<<SPI2X);

  
    for(;;){                /* main event loop */
        PORTC |= _BV(1);
      
        wdt_reset();
	usbPoll();
	
	PORTC &= ~_BV(1);
    }
}

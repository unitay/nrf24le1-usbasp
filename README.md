# nrf24le1-usbasp
A simple command-line interface with Nordic nRF24LE1 using USBasp.

## Features
- Program memory read/write
- Erase All memory
- InfoPage handling

### Miscellaneous

`nrf24le1 test`

Show the FSR register and make sure we can modify it, this demonstrates proper
communication on the SPI bus to the nRF24LE1 module.

`nrf24le1 erase`

Clear all memory: firmware and infopage.

### Reading data from nRF24LE1

`nrf24le1 read firmware`
`nrf24le1 read infopage`

All read operations dump data to files.

### Writing data to nRF24LE1

`nrf24le1 write firmware`
`nrf24le1 write infopage`

All write operations expect data from file.


# References

* Nordic nRF24LE1: <http://www.nordicsemi.com/eng/Products/2.4GHz-RF/nRF24LE1>
* libusb Library: <http://sourceforge.net/projects/libusb-win32/files/libusb-win32-releases/>

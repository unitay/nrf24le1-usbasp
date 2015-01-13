#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <usb.h>        /* this is libusb */
#include "opendevice.h" /* common code moved to separate module */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#if HIDMODE /* device's VID/PID and names */
#include "./firmware/usbconfigHID.h"
#else
#include "./firmware/usbconfig.h"
#endif

typedef enum {
	CMD_UNKNOWN,
	CMD_SHOW,
	CMD_READ_INFO,
	CMD_READ_FIRMWARE,
	CMD_WRITE_INFO,
	CMD_WRITE_FIRMWARE,
	CMD_ERASE
} cmd_e;

static int usage(void)
{
	fprintf(stderr, "Flashing nRF24LE1 through USBasp.  http://homes-smart.ru/\nIt based on https://github.com/derekstavis/nrf24le1-libbcm2835\n\n");
	fprintf(stderr, "Usage: nrf24le1 [cmds...]\n");
	fprintf(stderr, "Commands:\n");
	fprintf(stderr, "\ttest -- show FSR register and ability to modify it\n");
	fprintf(stderr, "\twrite firmware -- Write firmware from file 'main.bin'\n");	
	fprintf(stderr, "\tread firmware -- Read firmware to file 'main-dump.bin'\n");	
	fprintf(stderr, "\n\tread infopage -- Read infopage to file 'IP-dump.bin'\n");
	fprintf(stderr, "\twrite infopage -- Write infopage from file 'IP.bin'\n");
	fprintf(stderr, "\terase -- Erase all, firmware and infopage\n");
	return 1;
}

static cmd_e args_to_cmd(int argc, char **argv, char **filename)
{
	int cmd_read;

	if (argc < 2)
		return CMD_UNKNOWN;

	if (strcmp(argv[1], "test") == 0)
		return CMD_SHOW;
		
	if (strcmp(argv[1], "erase") == 0)
		return CMD_ERASE;

	if (argc < 3)
		return CMD_UNKNOWN;

	if (strcmp(argv[1], "read") == 0)
		cmd_read = 1;
	else if (strcmp(argv[1], "write") == 0)
		cmd_read = 0;
	else
		return CMD_UNKNOWN;

	if (argc >= 4)
		*filename = argv[3];

	if (strcmp(argv[2], "info") == 0 || strcmp(argv[2], "infopage") == 0)
		return (cmd_read ? CMD_READ_INFO : CMD_WRITE_INFO);
	else if (strcmp(argv[2], "firmware") == 0 || strcmp(argv[2], "fw") == 0)
		return (cmd_read ? CMD_READ_FIRMWARE : CMD_WRITE_FIRMWARE);

	return CMD_UNKNOWN;
}







usb_dev_handle      *handle = NULL;
char                buffer[32];
int                 cnt;

#include "wiring.c"
#include "nrf24le1.h"
#include "nrf24le1.c"













static void save_data_bin(FILE *f, uint8_t *buf, uint16_t count)
{
	fwrite(buf, 1, count, f);
}

static void save_data(char *filename, uint8_t *buf, uint16_t count)
{
	FILE *f = stdout;

	if (filename) {
		f = fopen(filename, "wb");
		if (f == NULL) {
			fprintf(stderr, "Error opening file '%s', saving as hex to stdout.\n", filename);
			f = stdout;
		}
	} else {
		f = stdout;
	}

		save_data_bin(f, buf, count);
}

static uint16_t read_data_bin(FILE *f, uint8_t *buf, uint16_t size)
{
	return fread(buf, 1, size, f);
}

static uint16_t read_data(char *filename, uint8_t *buf, uint16_t size)
{
	FILE *f = stdout;

	if (filename) {
		f = fopen(filename, "rb");
		if (f == NULL) {
			fprintf(stderr, "Error opening file '%s', saving as hex to stdout.\n", filename);
			f = stdin;
		}
	} else {
		f = stdin;
	}

		return read_data_bin(f, buf, size);
}

static void read_info(char *filename)
{
	ssize_t ret;
	uint8_t buf[NRF_PAGE_SIZE];

	ret = da_infopage_show(buf);
	if (ret < 0) {
		printf("Error reading infopage, ret=%d\n", ret);
		return;
	}
	save_data(filename, buf, sizeof(buf));
}

int main(int argc, char **argv)
{
	uint8_t bufread[16*1024];
	unsigned long off = 0;
	size_t count = sizeof(bufread);
	cmd_e cmd;
	char *filename = NULL;

	cmd = args_to_cmd(argc, argv, &filename);
	if (cmd == CMD_UNKNOWN)
		return usage();

	const unsigned char rawVid[2] = {USB_CFG_VENDOR_ID}, rawPid[2] = {USB_CFG_DEVICE_ID};
	char                vendor[]  = {USB_CFG_VENDOR_NAME, 0}, product[] = {USB_CFG_DEVICE_NAME, 0};

	int                 vid, pid;

    usb_init();

    /* compute VID/PID from usbconfig.h so that there is a central source of information */
    vid = rawVid[1] * 256 + rawVid[0];
    pid = rawPid[1] * 256 + rawPid[0];
    /* The following function is in opendevice.c: */
    if (usbOpenDevice(&handle, vid, vendor, pid, product, NULL, NULL, NULL) != 0)
	{
        fprintf(stderr, "Could not find USB device \"%s\" with vid=0x%x pid=0x%x\n", product, vid, pid);
        exit(1);
    }

	memset(bufread, 0, sizeof(bufread));

	nrf24le1_init();

	da_enable_program_store(1);

	if (cmd == CMD_SHOW) {
		da_test_show(1);
	} else
	if (cmd == CMD_ERASE) {
		da_erase_all_store();
	} else {
		// First we make sure we have proper SPI connectivity
		if (da_test_show(0) == 0) {
			// Now we run the command
			switch (cmd) {
				case CMD_WRITE_FIRMWARE:
					count = read_data("./main.bin", bufread, sizeof(bufread));
					if (count > 0)
						uhet_write(bufread, count, &off);
					else
						fprintf(stderr, "Failed to read data to program\n");
					break;

				case CMD_WRITE_INFO:
					count = read_data("./IP.bin", bufread, INFO_PAGE_SIZE);
					if (count > 0)
						da_infopage_store(bufread, count);
					else
						fprintf(stderr, "Failed to read data to program\n");
					break;

				case CMD_READ_FIRMWARE:
					count = uhet_read(bufread, count, &off);
					if (count > 0)
						save_data("./main-dump.bin", bufread, count);
					break;

				case CMD_READ_INFO:
					read_info("./IP-dump.bin");
					break;

				default:
					break;
			}
		}
	}

	da_enable_program_store(0);

	nrf24le1_cleanup();
	
	usb_close(handle);
  	return 0;
}

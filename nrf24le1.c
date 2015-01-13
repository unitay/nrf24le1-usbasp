#include "nrf24le1.h"
#include "wiring.h"

#define NAME "nrf24le1"

void uhet_record_init(void);
void uhet_record_end(void);

void _erase_all(void);
void _erase_page(unsigned i);
void _erase_program_pages(void);

int _enable_program = 0;

#define write_then_read(out,n_out,in,n_in) \
({ \
	int __ret = 0; \
\
	__ret = wiring_write_then_read(out,n_out,in,n_in); \
	if (0 > __ret){ \
		debug("Failed operation write_then_read"); \
	} \
\
	__ret; \
})

void nrf24le1_init()
{
	debug("Initializing nRF24LE1\n");
	wiring_init();
}

void nrf24le1_cleanup(void)
{
	wiring_destroy();
}

static void dump_fsr(uint8_t fsr)
{
	printf("-> FSR.RDISMB: %i\n",  (fsr & FSR_RDISMB ? 1 : 0));
	printf("-> FSR.INFEN: %i\n",   (fsr & FSR_INFEN ? 1 : 0));
	printf("-> FSR.RDYN: %i\n",    (fsr & FSR_RDYN ? 1 : 0));
	printf("-> FSR.WEN: %i\n",     (fsr & FSR_WEN ? 1 : 0));
	printf("-> FSR.STP: %i\n",     (fsr & FSR_STP ? 1 : 0));
	printf("-> FSR.ENDEBUG: %i\n", (fsr & FSR_ENDEBUG ? 1 : 0));
}

void _wait_for_ready(void)
{
	uint8_t cmd = { SPICMD_RDSR };
	uint8_t fsr = { 0xAA };
	int count = 0;

	do {
		if (count == 1000) {
			debug("Failed to wait for flash to be ready, FSR: 0x%X",
			      fsr);
			dump_fsr(fsr);
			return;
		}
		count++;

		fsr = wr_spi_one (cmd);
		usleep(300);

	} while (fsr & FSR_RDYN);
}

int _enable_infopage_access(void)
{
	uint8_t cmd[2];
	uint8_t in[1];
	uint8_t fsr_orig;

	// read fsr
	cmd[0] = SPICMD_RDSR;
	write_then_read(cmd, 1, in, 1);
	fsr_orig = in[0];

	// fsr.INFEN = 1
	cmd[0] = SPICMD_WRSR;
	cmd[1] = fsr_orig | FSR_INFEN;
	write_then_read(cmd, 2, NULL, 0);

	// read fsr
	cmd[0] = SPICMD_RDSR;
	write_then_read(cmd, 1, in, 1);

	// comparing writing
	if ((in[0] & FSR_INFEN) == 0) {
		debug("Failed to enable infopage access %X %X", fsr_orig,
		      in[0]);
		return -EINVAL;
	}

	return 0;
}

int _read_infopage(uint8_t *buf)
{
	int i;
	int ret = 0;

	uint8_t cmd[3];
	uint8_t in[N_BYTES_FOR_READ];

	uint16_t *addr = (uint16_t *) (cmd + 1);
	uint8_t *p = buf;

	cmd[0] = SPICMD_READ;
	for (i = 0; i < NRF_PAGE_SIZE; i += N_BYTES_FOR_READ) {

		*addr = htons(i);
		ret = write_then_read(cmd, 3, in, N_BYTES_FOR_READ);
		if (0 != ret)
			return ret;

		memcpy(p, in, N_BYTES_FOR_READ);

		p += N_BYTES_FOR_READ;
	}

	debug("Number of bytes read: %i\n", p - buf);

	return p - buf;
}

int _read_nvm_normal(uint8_t *buf)
{
	int i;
	int ret = 0;

	uint8_t cmd[3];
	uint8_t in[N_BYTES_FOR_READ];

	uint16_t *addr = (uint16_t *) (cmd + 1);
	uint8_t *p = buf;

	cmd[0] = SPICMD_READ;
	for (i = 0; i < NVM_NORMAL_MEM_SIZE; i += N_BYTES_FOR_READ) {

		*addr = htons(i + NVM_NORMAL_PAGE0_INI_ADDR);
		ret = write_then_read(cmd, 3, in, N_BYTES_FOR_READ);
		if (0 != ret)
			return ret;

		memcpy(p, in, N_BYTES_FOR_READ);

		p += N_BYTES_FOR_READ;
	}

	debug("Number of bytes read: %i\n", p - buf);

	return p - buf;
}

int _disable_infopage_access(void)
{
	uint8_t cmd[2];
	uint8_t in[1];
	uint8_t fsr_orig;

	// read fsr
	cmd[0] = SPICMD_RDSR;
	write_then_read(cmd, 1, in, 1);
	fsr_orig = in[0];

	// fsr.INFEN = 0
	cmd[0] = SPICMD_WRSR;
	cmd[1] = fsr_orig & ~(FSR_INFEN);
	write_then_read(cmd, 2, NULL, 0);

	// read fsr
	cmd[0] = SPICMD_RDSR;
	write_then_read(cmd, 1, in, 1);

	// comparing writing
	if ((in[0] & FSR_INFEN) != 0) {
		debug("Failed to disable infopage access %X %X",
		      fsr_orig, in[0]);
		return -EINVAL;
	}

	return 0;
}

ssize_t
da_enable_program_show()
{
	printf("%i\n", _enable_program);
	return 0;
}

ssize_t
da_enable_program_store(uint8_t state)
{
	int ret = 0;

	if (state != 0 && state != 1) {
		ret = -EINVAL;
		goto end;
	}

	switch (state) {

	case 0:
		switch (_enable_program) {
		case 0:
			goto end;

		case 1:
			_enable_program = 0;
			uhet_record_end();
			goto end;
		}

	case 1:
		switch (_enable_program) {
		case 1:
			goto end;

		case 0:
			_enable_program = 1;
			uhet_record_init();
			_wait_for_ready();
			goto end;
		}
	}

end:
	return ret;
}

ssize_t
da_erase_all_store()
{
	int ret = 0;

	if (0 == _enable_program) {
		debug("Failed, enable_program = 0");
		ret = -EINVAL;
		goto end;
	}

	_erase_all();

end:
	return ret;
}

ssize_t
da_test_show(int dump)
{
	uint8_t cmd;
	uint8_t fsr;
	int ret = 0;

	if (0 == _enable_program) {
		debug("Failed, enable_program = 0");
		ret = -EINVAL;
		goto end;
	}

	cmd = SPICMD_RDSR;
	fsr = wr_spi_one (cmd);
	if (dump) {
		printf("* FSR original\n");
		dump_fsr(fsr);
	}

	cmd = SPICMD_WREN;
	write_then_read(&cmd, 1, NULL, 0);

	cmd = SPICMD_RDSR;
	fsr = wr_spi_one (cmd);
	if (dump) {
		printf("* FSR after WREN, WEN must be 1\n");
		dump_fsr(fsr);
	}
	if ((fsr & FSR_WEN) == 0) {
		printf("Failed to set Write Enable bit to 1\n");
		ret = -EFAULT;
	}

	cmd = SPICMD_WRDIS;
	write_then_read(&cmd, 1, NULL, 0);

	cmd = SPICMD_RDSR;
	fsr = wr_spi_one (cmd);
	if (dump) {
		printf("* FSR after WRDIS, WEN must be 0\n");
		dump_fsr(fsr);
	}
	if ((fsr & FSR_WEN) == 1) {
		printf("Failed to set Write Enable bit to 0\n");
		ret = -EFAULT;
	}

end:
	return ret;
}

int _write_infopage(const uint8_t *buf)
{
	int i;
	const uint8_t *infopage = NULL;
	int error_count = 0;

	uint8_t cmd[3 + N_BYTES_FOR_WRITE];
	uint16_t *addr = (uint16_t *) (cmd + 1);

	infopage = buf;

	for (i = 0; i < NRF_PAGE_SIZE; i += N_BYTES_FOR_WRITE) {

		cmd[0] = SPICMD_WREN;
		if (0 > write_then_read(cmd, 1, NULL, 0))
			debug("Failed in SPICMD_WREN");

		_wait_for_ready();

		cmd[0] = SPICMD_PROGRAM;
		*addr = htons(i);
		memcpy(cmd + 3, infopage, N_BYTES_FOR_WRITE);

		if (0 != write_then_read(cmd, 3 + N_BYTES_FOR_WRITE, NULL, 0))
			error_count++;

		infopage += N_BYTES_FOR_WRITE;
		_wait_for_ready();
	}

	return (error_count > 0) ? -error_count : i;
}

int _write_nvm_normal(const uint8_t *buf)
{
	int i;
	const uint8_t *mem = NULL;
	int error_count = 0;

	uint8_t cmd[3 + N_BYTES_FOR_WRITE];
	uint16_t *addr = (uint16_t *) (cmd + 1);

	mem = buf;

	for (i = 0; i < NVM_NORMAL_MEM_SIZE; i += N_BYTES_FOR_WRITE) {

		cmd[0] = SPICMD_WREN;
		if (0 > write_then_read(cmd, 1, NULL, 0))
			debug("Failed in SPICMD_WREN");

		_wait_for_ready();

		cmd[0] = SPICMD_PROGRAM;
		*addr = htons(i + NVM_NORMAL_PAGE0_INI_ADDR);
		memcpy(cmd + 3, mem, N_BYTES_FOR_WRITE);

		if (0 != write_then_read(cmd, 3 + N_BYTES_FOR_WRITE, NULL, 0))
			error_count++;

		mem += N_BYTES_FOR_WRITE;
		_wait_for_ready();
	}

	return (error_count > 0) ? -error_count : i;
}

ssize_t
da_infopage_store(const uint8_t *buf, size_t count)
{
	int ret = 0;
	int size = -1;

	if (0 == _enable_program) {
		debug("Failed, enable_program = 0");
		ret = -EINVAL;
		goto end;
	}

	if (NRF_PAGE_SIZE != count) {
		debug("Count in infopage(%i) is different from NRF_PAGE_SIZE(%i)",
		      count, NRF_PAGE_SIZE);
		ret = -EINVAL;
		goto end;
	}

	ret = _enable_infopage_access();
	if (0 != ret)
		goto end;

	_erase_page(0);

	debug("Initiate writing to infopage");
	ret = _write_infopage(buf);
	if (0 > ret) {
		debug("Number of errors writing to infopage: %i", -1 * ret);
	} else {
		debug("Number of writes written to infopage: %i", ret);
		size = ret;
	}
	debug("Finished writing to infopage");

	ret = _disable_infopage_access();
	if (0 != ret)
		goto end;

	ret = size;

end:
	return ret;
}

ssize_t
da_infopage_show(uint8_t *buf)
{
	int ret;
	int size;

	if (0 == _enable_program) {
		debug("Failed, enable_program = 0");
		ret = -EINVAL;
		goto end;
	}

	debug("begin");

	ret = _enable_infopage_access();
	if (0 != ret)
		goto end;

	size = _read_infopage(buf);
	if (0 > size) {
		debug("Failed reading from infopage, size: %i", size);
	}

	ret = _disable_infopage_access();
	if (0 != ret)
		goto end;

	ret = size;

end:
	debug("end");
	return ret;
}

ssize_t
da_nvm_normal_show(uint8_t* buf)
{
	int ret;
	int size;

	if (0 == _enable_program) {
		debug("Failed, enable_program = 0");
		ret = -EINVAL;
		goto end;
	}

	debug("begin");

	size = _read_nvm_normal(buf);
	if (0 > size) {
		debug("Failed reading nvm, size: %i", size);
	}

	ret = size;

end:
	debug("end");
	return ret;
}

ssize_t
da_nvm_normal_store(const uint8_t *buf, size_t count)
{
	int ret = 0;
	int size = -1;

	if (0 == _enable_program) {
		debug("Failed, enable_program = 0");
		ret = -EINVAL;
		goto end;
	}

	if (NVM_NORMAL_MEM_SIZE != count) {
		debug
		    ("Size of image(%i) differs from NVM_NORMAL_MEM_SIZE(%i)",
		     count, NVM_NORMAL_MEM_SIZE);
		ret = -EINVAL;
		goto end;
	}

	_erase_page(NVM_NORMAL_PAGE0);
	_erase_page(NVM_NORMAL_PAGE1);

	debug("Initating writing to memory");
	ret = _write_nvm_normal(buf);
	if (0 > ret) {
		debug("Number of errors writing to nvm_normal: %i", -1 * ret);
	} else {
		debug("bytes written: %i", ret);
		size = ret;
	}
	debug("Finished writing to memory");

	ret = size;

end:
	return ret;
}

static void set_prog_bit(int bit)
{
	wiring_set_gpio_value(GPIO_PROG, bit);
	sleep(1);
}

void uhet_record_init(void)
{
	printf("Initiate programming\n");
	set_prog_bit(1);
}

void uhet_record_end(void)
{
	set_prog_bit(0);
	printf("Finished programming\n");
}

void _erase_all(void)
{
	uint8_t cmd[1];
	int ret;

	printf("Initiating erase of all pages\n");

	cmd[0] = SPICMD_WREN;
	write_then_read(cmd, 1, NULL, 0);
	_wait_for_ready();

	cmd[0] = SPICMD_ERASEALL;
	ret = write_then_read(cmd, 1, NULL, 0);
	if (0 == ret)
		debug("Error erasing all data");

	_wait_for_ready();

	printf("Done erasing all pages\n");
}


uint8_t __enable_wren(void)
{
        uint8_t cmd, fsr;

	cmd = SPICMD_WREN;
	write_then_read(&cmd, 1, NULL, 0);

	cmd = SPICMD_RDSR;
	write_then_read(&cmd, 1, &fsr, 1);

	if ((fsr & FSR_WEN) == 0)
	{
		printf("Failed to enable flash programming -> FSR.WEN: %i\n", (fsr & FSR_WEN ? 1 : 0));
		return 0;
	}

	return 1;
}

void _erase_page(unsigned i)
{
	uint8_t cmd[2];
	int ret;

	if(!__enable_wren()) return;

	_wait_for_ready();

	cmd[0] = SPICMD_ERASEPAGE;
	cmd[1] = i;
	ret = write_then_read(cmd, 2, NULL, 0);

	if (0 == ret)
		printf("Erased page: %i\n", i);

	_wait_for_ready();
}

void _erase_program_pages(void)
{
	unsigned i;


	for (i = 0; i < N_PAGES; i++) {
		_erase_page(i);
	}
}

ssize_t uhet_write(uint8_t *buf, size_t count, unsigned long *off)
{
	uint8_t cmd[3 + count];
	uint8_t  i =0;
	uint16_t addr = 0;

	if (0 == _enable_program) {
		debug("Failed, enable_pragram = 0");
		return -EINVAL;
	}

	_erase_program_pages(); /* goes first */

	_wait_for_ready();

	for(i = 0; i < 32; i++)
	{
		cmd[0] = SPICMD_PROGRAM;
		cmd[2] = (uint8_t)(addr & 0x00ff);
		cmd[1] = (uint8_t)((addr & 0xff00) >> 8);


		printf("ADDR: %02X %02X\n",((addr & 0xff00) >> 8), (addr & 0x00ff));
		memcpy(cmd + 3, &buf[addr], 512);

		if(!__enable_wren())
			return -EFAULT;

		printf("b0:%02X    b1:%02X   b2: %02X\n",cmd[0], cmd[2], cmd[1]);
		write_then_read(cmd, 3 + 512, NULL, 0);

		addr = addr + 512; /* Updating address */
		_wait_for_ready();
	}

	return count;
}

ssize_t
uhet_read(uint8_t* buf, size_t count, unsigned long *off)
{

	if (0 == _enable_program) {
		debug("Failed, enable_program = 0");
		return -EINVAL;
	}

	printf("Number of bytes to read: %i, max flash size %d\n", count, MAX_FIRMWARE_SIZE);
	// reading from flash
	{
		uint8_t cmd[3];
		uint16_t *addr = (uint16_t *) (cmd + 1);
		//uint8_t data[count];

		cmd[0] = SPICMD_READ;
		cmd[1] = 0;
		cmd[2] = 0;

		write_then_read(cmd, 3, buf, count);

		debug
		    ("read addr: 0x%p, pack header: 0x%X 0x%X 0x%X, bytes read: %i",
		     addr, cmd[0], cmd[1], cmd[2], count);

		return count;
	}

	return 0;
}

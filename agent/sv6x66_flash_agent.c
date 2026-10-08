#include <stdint.h>

#define REG32(address) (*(volatile uint32_t *)(uintptr_t)(address))

#define HSUART_DATA 0xC0000D00u
#define HSUART_LSR 0xC0000D14u
#define HSUART_LSR_DR 0x01u
#define HSUART_LSR_THRE 0x20u

#define SPI_CMD_SRAM 0xC0001000u
#define SPI_DATAIN_SRAM 0xC0001004u
#define SPI_CONTROL 0xC000100Cu
#define SPI_TLEN 0xC0001010u
#define SPI_RLEN 0xC0001014u
#define SPI_BUSY_MASK 0x08u
#define SPI_MODE_MASK 0x06u

#define SPI_CMD_BUFFER 0x00008000u
#define SPI_PAYLOAD_BUFFER 0x00008004u
#define SPI_READ_BUFFER 0x00008400u

#define CMD_READ_STATUS1 0x05u
#define CMD_WRITE_ENABLE 0x06u
#define CMD_READ_JEDEC_ID 0x9Fu
#define CMD_SECTOR_ERASE 0x20u
#define CMD_BLOCK_ERASE_64K 0xD8u
#define CMD_PAGE_PROGRAM 0x02u
#define CMD_FAST_READ 0x0Bu

#define FLASH_BYTES 0x00200000u
#define BLOCK_BYTES 65536u
#define SECTOR_BYTES 4096u
#define PAGE_BYTES 256u
#define SESSION_HEADER_BYTES 24u
#define UART_LIMIT 200000000u
#define CONTROLLER_LIMIT 10000000u
#define STATUS_LIMIT 8000000u
#define ACK 0x06u
#define NAK 0x15u
#define CAN 0x18u

static uint8_t sector_buffer[SECTOR_BYTES];

static uint32_t crc32_update(uint32_t crc, const uint8_t *data, uint32_t length)
{
	while (length--) {
		uint32_t bit;
		crc ^= *data++;
		for (bit = 0u; bit < 8u; bit++)
			crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
	}
	return crc;
}

static uint32_t crc32(const uint8_t *data, uint32_t length)
{
	return crc32_update(0xFFFFFFFFu, data, length) ^ 0xFFFFFFFFu;
}

static uint32_t le32(const uint8_t *data)
{
	return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
		((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static int uart_put(uint8_t value)
{
	uint32_t limit = UART_LIMIT;
	while ((REG32(HSUART_LSR) & HSUART_LSR_THRE) == 0u)
		if (--limit == 0u)
			return 0;
	REG32(HSUART_DATA) = value;
	return 1;
}

static int uart_get(uint8_t *value)
{
	uint32_t limit = UART_LIMIT;
	while ((REG32(HSUART_LSR) & HSUART_LSR_DR) == 0u)
		if (--limit == 0u)
			return 0;
	*value = (uint8_t)REG32(HSUART_DATA);
	return 1;
}

static int uart_read(uint8_t *data, uint32_t length)
{
	while (length--)
		if (!uart_get(data++))
			return 0;
	return 1;
}

static int uart_text(const char *text)
{
	while (*text)
		if (!uart_put((uint8_t)*text++))
			return 0;
	return 1;
}

static void uart_le16(uint16_t value)
{
	uart_put((uint8_t)value);
	uart_put((uint8_t)(value >> 8));
}

static void uart_le32(uint32_t value)
{
	uart_put((uint8_t)value);
	uart_put((uint8_t)(value >> 8));
	uart_put((uint8_t)(value >> 16));
	uart_put((uint8_t)(value >> 24));
}

static void uart_hex8(uint8_t value)
{
	static const char digits[] = "0123456789ABCDEF";
	uart_put((uint8_t)digits[value >> 4]);
	uart_put((uint8_t)digits[value & 0x0Fu]);
}

static void uart_hex32(uint32_t value)
{
	static const char digits[] = "0123456789ABCDEF";
	int shift;
	for (shift = 28; shift >= 0; shift -= 4)
		uart_put((uint8_t)digits[(value >> (uint32_t)shift) & 0x0Fu]);
}

static void uart_dec32(uint32_t value)
{
	uint8_t digits[10];
	uint32_t count = 0u;
	do {
		digits[count++] = (uint8_t)('0' + value % 10u);
		value /= 10u;
	} while (value != 0u);
	while (count != 0u)
		uart_put(digits[--count]);
}

static uint32_t command_word(uint8_t opcode, uint32_t address)
{
	return (uint32_t)opcode | ((address & 0x0000FFu) << 24) |
		((address & 0x00FF00u) << 8) | ((address & 0xFF0000u) >> 8);
}

static int controller_idle(void)
{
	uint32_t limit = CONTROLLER_LIMIT;
	while ((REG32(SPI_CONTROL) & SPI_BUSY_MASK) != 0u)
		if (--limit == 0u)
			return 0;
	return 1;
}

static int issue(uint32_t command, uint32_t transmit, uint32_t receive)
{
	if (!controller_idle())
		return 0;
	REG32(SPI_CMD_BUFFER) = command;
	REG32(SPI_RLEN) = receive;
	REG32(SPI_TLEN) = transmit;
	return controller_idle();
}

static uint8_t read_buffer_byte(uint32_t index)
{
	return (uint8_t)(REG32(SPI_READ_BUFFER + (index & ~3u)) >> ((index & 3u) * 8u));
}

static uint32_t decode_capacity(uint8_t code)
{
	if (code < 0x11u || code > 0x1Fu)
		return 0u;
	return 1u << code;
}

static int read_flash_identity(uint8_t jedec[3], uint32_t *capacity)
{
	uint32_t i;
	if (!issue(CMD_READ_JEDEC_ID, 1u, 3u))
		return 0;
	for (i = 0u; i < 3u; i++)
		jedec[i] = read_buffer_byte(i);
	*capacity = decode_capacity(jedec[2]);
	return *capacity != 0u;
}

static int flash_ready(void)
{
	uint32_t limit = STATUS_LIMIT;
	while (limit--) {
		if (!issue(CMD_READ_STATUS1, 1u, 1u))
			return 0;
		if ((REG32(SPI_READ_BUFFER) & 1u) == 0u)
			return 1;
	}
	return 0;
}

static int write_enable(void)
{
	return issue(CMD_WRITE_ENABLE, 1u, 0u) && flash_ready();
}

static int fast_read(uint32_t address, uint32_t length)
{
	REG32(SPI_PAYLOAD_BUFFER) = 0u;
	return issue(command_word(CMD_FAST_READ, address), 5u, length);
}

static int sector_erase(uint32_t address)
{
	if ((address & (SECTOR_BYTES - 1u)) != 0u || !write_enable())
		return 0;
	if (!issue(command_word(CMD_SECTOR_ERASE, address), 4u, 0u))
		return 0;
	return flash_ready();
}

static int block_erase_64k(uint32_t address)
{
	if ((address & (BLOCK_BYTES - 1u)) != 0u || !write_enable())
		return 0;
	if (!issue(command_word(CMD_BLOCK_ERASE_64K, address), 4u, 0u))
		return 0;
	return flash_ready();
}

static int page_is_erased(const uint8_t *data)
{
	uint32_t i;
	for (i = 0u; i < PAGE_BYTES; i++)
		if (data[i] != 0xFFu)
			return 0;
	return 1;
}

static int page_program(uint32_t address, const uint8_t *data)
{
	uint32_t i;
	if (page_is_erased(data))
		return 1;
	for (i = 0u; i < PAGE_BYTES; i += 4u)
		REG32(SPI_PAYLOAD_BUFFER + i) = (uint32_t)data[i] |
			((uint32_t)data[i + 1u] << 8) | ((uint32_t)data[i + 2u] << 16) |
			((uint32_t)data[i + 3u] << 24);
	if (!write_enable())
		return 0;
	if (!issue(command_word(CMD_PAGE_PROGRAM, address), PAGE_BYTES + 4u, 0u))
		return 0;
	return flash_ready();
}

static int program_sector(uint32_t address)
{
	uint32_t offset;
	for (offset = 0u; offset < SECTOR_BYTES; offset += PAGE_BYTES)
		if (!page_program(address + offset, sector_buffer + offset))
			return 0;
	return 1;
}

static int flash_crc32(uint32_t address, uint32_t length, uint32_t *result)
{
	uint32_t crc = 0xFFFFFFFFu;
	uint32_t offset;
	for (offset = 0u; offset < length; offset += PAGE_BYTES) {
		uint32_t i;
		if (!fast_read(address + offset, PAGE_BYTES))
			return 0;
		for (i = 0u; i < PAGE_BYTES; i++) {
			uint8_t value = read_buffer_byte(i);
			crc = crc32_update(crc, &value, 1u);
		}
	}
	*result = crc ^ 0xFFFFFFFFu;
	return 1;
}

static int session_header_valid(const uint8_t *header, uint32_t capacity, uint32_t *image_crc)
{
	static const uint8_t magic[8] = {'S','V','6','F','W','R','0','2'};
	uint32_t i;
	for (i = 0u; i < 8u; i++)
		if (header[i] != magic[i])
			return 0;
	if (le32(header + 8) != 2u || le32(header + 12) != capacity)
		return 0;
	if (capacity != FLASH_BYTES || le32(header + 20) != crc32(header, 20u))
		return 0;
	*image_crc = le32(header + 16);
	return 1;
}

static int receive_sector(uint16_t sequence, uint32_t address, uint8_t mode)
{
	uint8_t crc_bytes[4];
	uint32_t expected_crc;
	uint32_t actual_crc;
	uint32_t verify_crc;
	uint32_t offset;

	uart_put((uint8_t)'Q');
	uart_le16(sequence);
	uart_le32(address);
	uart_put(mode);
	if (!uart_read(crc_bytes, 4u) || !uart_read(sector_buffer, SECTOR_BYTES))
		return 0;
	expected_crc = le32(crc_bytes);
	actual_crc = crc32(sector_buffer, SECTOR_BYTES);
	if (actual_crc != expected_crc) {
		uart_put(NAK);
		return -1;
	}
	if (mode == (uint8_t)'S' && !sector_erase(address))
		return 0;
	if (!program_sector(address))
		return 0;
	if (!flash_crc32(address, SECTOR_BYTES, &verify_crc) || verify_crc != expected_crc)
		return 0;
	for (offset = 0u; offset < SECTOR_BYTES; offset += PAGE_BYTES) {
		uint32_t i;
		if (!fast_read(address + offset, PAGE_BYTES))
			return 0;
		for (i = 0u; i < PAGE_BYTES; i++)
			if (read_buffer_byte(i) != sector_buffer[offset + i])
				return 0;
	}
	uart_put(ACK);
	return 1;
}

static int receive_sector_with_retry(uint16_t sequence, uint32_t address, uint8_t mode)
{
	uint32_t retries = 3u;
	while (retries--) {
		int result = receive_sector(sequence, address, mode);
		if (result > 0)
			return 1;
		if (result == 0)
			return 0;
	}
	return 0;
}

static int stream_flash(uint32_t *result)
{
	uint32_t crc = 0xFFFFFFFFu;
	uint32_t address;
	for (address = 0u; address < FLASH_BYTES; address += PAGE_BYTES) {
		uint32_t i;
		if (!fast_read(address, PAGE_BYTES))
			return 0;
		for (i = 0u; i < PAGE_BYTES; i++) {
			uint8_t value = read_buffer_byte(i);
			crc = crc32_update(crc, &value, 1u);
			if (!uart_put(value))
				return 0;
		}
	}
	crc ^= 0xFFFFFFFFu;
	uart_le32(crc);
	*result = crc;
	return 1;

}

static int send_full_read(void)
{
	uint32_t crc;
	uart_text("SV6FWR02 READ BYTES=2097152\r\n");
	if (!stream_flash(&crc))
		return 0;
	uart_text("SV6FWR02 READ COMPLETE\r\n");
	return 1;

}

static int send_full_readback(uint32_t expected_crc)
{
	uint32_t crc;
	uart_text("SV6FWR02 READBACK BYTES=2097152\r\n");
	return stream_flash(&crc) && crc == expected_crc;
}

static void restore_xip(uint32_t original_mode)
{
	if (controller_idle()) {
		REG32(SPI_CONTROL) = (REG32(SPI_CONTROL) & ~SPI_MODE_MASK) | original_mode;
		controller_idle();
	}
}

void __attribute__((section(".entry"), noinline, used)) SV6x66_FlashAgentEntry(void)
{
	uint8_t header[SESSION_HEADER_BYTES];
	uint8_t jedec[3];
	uint32_t original_mode;
	uint32_t capacity;
	uint32_t image_crc;
	uint32_t flash_crc;
	uint32_t block;
	uint32_t sector;
	uint16_t sequence = 0u;
	uint8_t command;

	__asm__ volatile("setgie.d\n\tdsb" ::: "memory");
	original_mode = REG32(SPI_CONTROL) & SPI_MODE_MASK;
	if (!controller_idle())
		goto fail_controller;
	REG32(SPI_CONTROL) &= ~SPI_MODE_MASK;
	__asm__ volatile("isb" ::: "memory");
	if ((REG32(SPI_CONTROL) & SPI_MODE_MASK) != 0u)
		goto fail_xip;
	REG32(SPI_CMD_SRAM) = SPI_CMD_BUFFER;
	REG32(SPI_DATAIN_SRAM) = SPI_READ_BUFFER;
	if (!read_flash_identity(jedec, &capacity))
		goto fail_identity;

	uart_text("SV6FWR02 INFO JEDEC=");
	uart_hex8(jedec[0]);
	uart_hex8(jedec[1]);
	uart_hex8(jedec[2]);
	uart_text(" FLASH=");
	uart_dec32(capacity);
	uart_text(" SECTOR=4096 BLOCK=65536 XIP=OFF\r\n");
	if (capacity != FLASH_BYTES)
		goto fail_capacity;
	if (!uart_get(&command))
		goto fail_protocol;
	if (command == (uint8_t)'D') {
		if (!send_full_read())
			goto fail_readback;
		restore_xip(original_mode);
		for (;;) {}
	}
	if (command != (uint8_t)'W')
		goto fail_protocol;
	if (!uart_read(header, SESSION_HEADER_BYTES))
		goto fail_protocol;
	if (!session_header_valid(header, capacity, &image_crc))
		goto fail_header;
	uart_put(ACK);

	for (block = 1u; block < (FLASH_BYTES / BLOCK_BYTES); block++) {
		uint32_t block_address = block * BLOCK_BYTES;
		if (!block_erase_64k(block_address))
			goto fail_flash;
		for (sector = 0u; sector < (BLOCK_BYTES / SECTOR_BYTES); sector++) {
			uint32_t address = block_address + sector * SECTOR_BYTES;
			if (!receive_sector_with_retry(sequence++, address, (uint8_t)'B'))
				goto fail_flash;
		}
	}

	for (sector = 1u; sector < (BLOCK_BYTES / SECTOR_BYTES); sector++)
		if (!receive_sector_with_retry(sequence++, sector * SECTOR_BYTES, (uint8_t)'S'))
			goto fail_flash;
	if (!receive_sector_with_retry(sequence++, 0u, (uint8_t)'S'))
		goto fail_flash;
	if (!flash_crc32(0u, FLASH_BYTES, &flash_crc))
		goto fail_flash;
	if (flash_crc != image_crc)
		goto fail_image_crc;
	uart_text("SV6FWR02 FLASH_CRC=");
	uart_hex32(flash_crc);
	uart_text(" PASS\r\n");
	if (!send_full_readback(image_crc))
		goto fail_readback;
	restore_xip(original_mode);
	uart_text("SV6FWR02 COMPLETE XIP=RESTORED\r\n");
	for (;;) {}

fail_controller:
	uart_text("SV6FWR02 FAIL_CONTROLLER_TIMEOUT\r\n");
	for (;;) {}
fail_xip:
	uart_text("SV6FWR02 FAIL_XIP_ON\r\n");
	for (;;) {}
fail_identity:
	restore_xip(original_mode);
	uart_text("SV6FWR02 FAIL_JEDEC\r\n");
	for (;;) {}
fail_capacity:
	restore_xip(original_mode);
	uart_text("SV6FWR02 REFUSE_CAPACITY\r\n");
	for (;;) {}
fail_protocol:
	restore_xip(original_mode);
	uart_text("SV6FWR02 FAIL_PROTOCOL\r\n");
	for (;;) {}
fail_header:
	restore_xip(original_mode);
	uart_put(CAN);
	uart_text("SV6FWR02 REFUSE_HEADER\r\n");
	for (;;) {}
fail_flash:
	restore_xip(original_mode);
	uart_put(CAN);
	uart_text("SV6FWR02 FAIL_FLASH\r\n");
	for (;;) {}
fail_image_crc:
	restore_xip(original_mode);
	uart_text("SV6FWR02 FAIL_IMAGE_CRC\r\n");
	for (;;) {}
fail_readback:
	restore_xip(original_mode);
	uart_text("SV6FWR02 FAIL_READBACK\r\n");
	for (;;) {}
}

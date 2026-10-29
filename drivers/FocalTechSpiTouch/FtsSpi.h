// SPDX-License-Identifier: BSD-2-Clause-Patent
// FocalTech touch controllers on SPI (the FT3683G of the Nothing Phone (3a)): the wire frame, its CRC-16 and the touch report.
// Written from the frame layout as a fact (register, 0xA0, big-endian length, three dummy bytes, then the data and its CRC-16) and
// the 3a's measurements (UniXNU-asteroids T30-T35); no code from the GPL sources. The same frame and CRC as RigoLigoRLC's FT8203
// driver (unixnu-nativefocaltechtouch, BSD-2-Clause-Patent), whose I/O shape this follows. No board addresses, clocks, pins or
// power here: the caller brings the SPI bus (QcomGeniSpi on Qualcomm boards), resets the chip and serializes every transaction.
#ifndef FTS_SPI_H
#define FTS_SPI_H
#include <stddef.h>
#include <stdint.h>

#define FTS_SPI_HEADER 7          // register, 0xA0 (read | CRC), length (big-endian), 3 dummy bytes
#define FTS_SPI_TRAILER 2         // CRC-16 of the data, little-endian
#define FTS_SPI_REPORT_BYTES 84   // register 0x01: the touch report
#define FTS_SPI_CONTACTS 10
#define FTS_SPI_WIRE_BYTES (FTS_SPI_HEADER + FTS_SPI_REPORT_BYTES + FTS_SPI_TRAILER)

enum {
    FTS_REG_REPORT = 0x01, FTS_REG_CHIP_HI = 0xA3, FTS_REG_CHIP_LO = 0x9F, FTS_REG_FIRMWARE = 0xA6, FTS_REG_VENDOR = 0xA8,
};
#define FTS_CHIP_FT3683G_HI 0x56  // 0xA3 on the 3a's FT3683G (0x9F reads 0x72)

// fts_spi_read results besides 0 and the transfer's own errors: the status byte (rx[3]) had 0xA0 set on every attempt, or the
// data's CRC-16 did not match (the data is still returned: the CRC parameters come from the vendor driver, not the wire).
enum {FTS_SPI_STATUS = -70, FTS_SPI_CRC = -71};

// One complete SPI transaction of `length` bytes with chip select held throughout (0 or the bus's error), and a delay.
struct FtsSpiIO {
    void *context;
    int (*transfer)(void *, const uint8_t *tx, uint8_t *rx, size_t length);
    void (*delay_us)(void *, unsigned);
};
// Counters the caller reports; fts_spi_read adds to them.
struct FtsSpiCounts {uint32_t transfers, xfer_errors, status_errors, crc_errors;};
// A decoded contact in panel pixels.
struct FtsContact {int id, x, y;};

uint16_t fts_spi_crc16(const uint8_t *data, size_t length);
// The read frame for n data bytes of register reg in tx (FTS_SPI_HEADER + n + FTS_SPI_TRAILER bytes, header then zeros); its length.
size_t fts_spi_read_frame(uint8_t *tx, uint8_t reg, unsigned n);
// Reads n bytes of register reg into out: up to three attempts, 150 us with chip select high after each. tx and rx hold
// FTS_SPI_HEADER + n + FTS_SPI_TRAILER bytes; rx keeps the last attempt. *status gets the last status byte.
int fts_spi_read(const struct FtsSpiIO *io, uint8_t *tx, uint8_t *rx, uint8_t reg, uint8_t *out, unsigned n,
                 struct FtsSpiCounts *counts, uint8_t *status);
// A report of all 0xFF after its first byte: no contact (or the firmware just started, when the first byte is 0xFF too).
int fts_spi_idle(const uint8_t *report);
// The contacts still down in a report (register 0x01's data): protocol v2 (type 2: count in byte 1, 8-byte records from byte 4,
// 16x sub-pixel coordinates) or the legacy layout (type 0: 6-byte records from byte 2, 12-bit pixels, id 15 ends). *type gets
// byte 1's top nibble. Lifted contacts are left out.
unsigned fts_spi_decode(const uint8_t *report, unsigned *type, struct FtsContact *out, unsigned max);

#endif

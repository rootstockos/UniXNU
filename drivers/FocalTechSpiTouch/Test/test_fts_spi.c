// SPDX-License-Identifier: BSD-2-Clause-Patent
// Host test of FtsSpi.c: cc -std=c11 -Wall -Wextra -Werror -I.. ../FtsSpi.c test_fts_spi.c -o test_fts_spi && ./test_fts_spi
#include "FtsSpi.h"
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c) do { if (!(c)) {printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++failures;} } while (0)

// A chip that answers every read with `data`, a status byte and a CRC; `bad_status` attempts first set status bit 0x80.
struct Fake {const uint8_t *data; unsigned n; unsigned bad_status, calls, delays; int crc_wrong, xfer_error;};
static int transfer(void *context, const uint8_t *tx, uint8_t *rx, size_t length)
{
    struct Fake *f = context;
    ++f->calls;
    if (f->xfer_error && f->calls == 1) return -5;
    if (length != FTS_SPI_HEADER + f->n + FTS_SPI_TRAILER || tx[1] != 0xA0 || ((tx[2] << 8) | tx[3]) != (int)f->n) return -22;
    memset(rx, 0, length);
    rx[3] = f->calls <= f->bad_status ? 0x80 : 0x00;
    memcpy(rx + FTS_SPI_HEADER, f->data, f->n);
    uint16_t crc = fts_spi_crc16(f->data, f->n) ^ (f->crc_wrong ? 1 : 0);
    rx[FTS_SPI_HEADER + f->n] = (uint8_t)crc;
    rx[FTS_SPI_HEADER + f->n + 1] = (uint8_t)(crc >> 8);
    return 0;
}
static void delay(void *context, unsigned us) {struct Fake *f = context; if (us == 150) ++f->delays;}

int main(void)
{
    // CRC-16/MCRF4XX check value.
    CHECK(fts_spi_crc16((const uint8_t *)"123456789", 9) == 0x6F91);

    uint8_t tx[FTS_SPI_WIRE_BYTES], rx[FTS_SPI_WIRE_BYTES], out[FTS_SPI_REPORT_BYTES], status = 0xee;
    memset(tx, 0x5a, sizeof tx);
    CHECK(fts_spi_read_frame(tx, FTS_REG_CHIP_HI, 1) == 10);
    CHECK(tx[0] == 0xA3 && tx[1] == 0xA0 && tx[2] == 0 && tx[3] == 1 && tx[4] == 0 && tx[9] == 0);
    CHECK(fts_spi_read_frame(tx, FTS_REG_REPORT, FTS_SPI_REPORT_BYTES) == FTS_SPI_WIRE_BYTES && tx[2] == 0 && tx[3] == 84);

    const uint8_t chip[1] = {FTS_CHIP_FT3683G_HI};
    struct Fake f = {chip, 1, 0, 0, 0, 0, 0};
    struct FtsSpiIO io = {&f, transfer, delay};
    struct FtsSpiCounts counts = {0};
    CHECK(fts_spi_read(&io, tx, rx, FTS_REG_CHIP_HI, out, 1, &counts, &status) == 0 && out[0] == 0x56 && status == 0);
    CHECK(counts.transfers == 1 && f.delays == 1);

    // Status bit set twice, then a good frame: retried, counted.
    f = (struct Fake){chip, 1, 2, 0, 0, 0, 0};
    memset(&counts, 0, sizeof counts);
    CHECK(fts_spi_read(&io, tx, rx, FTS_REG_CHIP_HI, out, 1, &counts, &status) == 0 && counts.status_errors == 2 && counts.transfers == 3);
    // Status set on every attempt: refused after three.
    f = (struct Fake){chip, 1, 3, 0, 0, 0, 0};
    memset(&counts, 0, sizeof counts);
    CHECK(fts_spi_read(&io, tx, rx, FTS_REG_CHIP_HI, out, 1, &counts, &status) == FTS_SPI_STATUS && counts.transfers == 3 && status == 0x80);
    // A transfer error, then a good frame.
    f = (struct Fake){chip, 1, 0, 0, 0, 0, 1};
    memset(&counts, 0, sizeof counts);
    CHECK(fts_spi_read(&io, tx, rx, FTS_REG_CHIP_HI, out, 1, &counts, &status) == 0 && counts.xfer_errors == 1);
    // A CRC mismatch is reported once, not retried, and the data still comes back.
    f = (struct Fake){chip, 1, 0, 0, 0, 1, 0};
    memset(&counts, 0, sizeof counts);
    out[0] = 0;
    CHECK(fts_spi_read(&io, tx, rx, FTS_REG_CHIP_HI, out, 1, &counts, &status) == FTS_SPI_CRC && out[0] == 0x56 && counts.crc_errors == 1 &&
          counts.transfers == 1);

    // Protocol v2: two records, the second lifted. Contact 3 at (610, 2384) in 16x sub-pixels.
    uint8_t report[FTS_SPI_REPORT_BYTES];
    memset(report, 0, sizeof report);
    report[1] = 0x22;
    const unsigned x = 610 * 16, y = 2384 * 16;
    uint8_t *r = report + 4;
    r[0] = (uint8_t)(0x80 | ((x >> 12) & 15)); r[1] = (uint8_t)(x >> 4); r[2] = (uint8_t)(3 << 4 | ((y >> 12) & 15)); r[3] = (uint8_t)(y >> 4);
    r[4] = (uint8_t)((x & 15) << 4 | (y & 15));
    report[12] = 0x40;   // the second record: lifted
    struct FtsContact c[FTS_SPI_CONTACTS];
    unsigned type = 9;
    CHECK(fts_spi_decode(report, &type, c, FTS_SPI_CONTACTS) == 1 && type == 2 && c[0].id == 3 && c[0].x == 610 && c[0].y == 2384);
    CHECK(fts_spi_decode(report, &type, c, 0) == 0);
    CHECK(!fts_spi_idle(report));

    // Legacy: one contact, then id 15 ends the list.
    memset(report, 0, sizeof report);
    report[1] = 0x01;
    report[2] = 0x80 | (1067 >> 8); report[3] = (uint8_t)1067; report[4] = (uint8_t)(5 << 4 | (773 >> 8)); report[5] = (uint8_t)773;
    report[8 + 2] = 0xf0;
    CHECK(fts_spi_decode(report, &type, c, FTS_SPI_CONTACTS) == 1 && type == 0 && c[0].id == 5 && c[0].x == 1067 && c[0].y == 773);

    memset(report, 0xff, sizeof report);
    CHECK(fts_spi_idle(report));
    report[0] = 0x00;
    CHECK(fts_spi_idle(report));
    CHECK(fts_spi_decode(report, &type, c, FTS_SPI_CONTACTS) == 0 && type == 15);

    if (!failures) printf("PASS: FtsSpi frame, CRC-16/MCRF4XX, retries (status, transfer), CRC mismatch, v2 and legacy reports, idle\n");
    return failures != 0;
}

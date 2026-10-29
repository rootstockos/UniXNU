// SPDX-License-Identifier: BSD-2-Clause-Patent
#include "FtsSpi.h"
#include <string.h>

// Reflected polynomial 0x8408, initial 0xFFFF, no final xor (CRC-16/MCRF4XX).
uint16_t fts_spi_crc16(const uint8_t *data, size_t length)
{
    uint16_t crc = 0xffff;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (unsigned bit = 0; bit < 8; ++bit) crc = (crc & 1) ? (uint16_t)((crc >> 1) ^ 0x8408) : (uint16_t)(crc >> 1);
    }
    return crc;
}

size_t fts_spi_read_frame(uint8_t *tx, uint8_t reg, unsigned n)
{
    const size_t length = FTS_SPI_HEADER + n + FTS_SPI_TRAILER;
    memset(tx, 0, length);
    tx[0] = reg;
    tx[1] = 0xA0;
    tx[2] = (uint8_t)(n >> 8);
    tx[3] = (uint8_t)n;
    return length;
}

int fts_spi_read(const struct FtsSpiIO *io, uint8_t *tx, uint8_t *rx, uint8_t reg, uint8_t *out, unsigned n,
                 struct FtsSpiCounts *counts, uint8_t *status)
{
    const size_t length = fts_spi_read_frame(tx, reg, n);
    int result = 0;
    for (unsigned attempt = 0; attempt < 3; ++attempt) {
        ++counts->transfers;
        result = io->transfer(io->context, tx, rx, length);
        io->delay_us(io->context, 150);   // chip select high between frames
        if (result) {++counts->xfer_errors; continue;}
        *status = rx[3];
        if (rx[3] & 0xA0) {++counts->status_errors; result = FTS_SPI_STATUS; continue;}
        memcpy(out, rx + FTS_SPI_HEADER, n);
        const uint16_t received = (uint16_t)(rx[FTS_SPI_HEADER + n] | rx[FTS_SPI_HEADER + n + 1] << 8);
        if (fts_spi_crc16(rx + FTS_SPI_HEADER, n) != received) {++counts->crc_errors; return FTS_SPI_CRC;}
        return 0;
    }
    memcpy(out, rx + FTS_SPI_HEADER, n);   // the last attempt's bytes, for the caller's log
    return result;
}

int fts_spi_idle(const uint8_t *report)
{
    return report[1] == 0xff && report[2] == 0xff && report[3] == 0xff && report[4] == 0xff;
}

unsigned fts_spi_decode(const uint8_t *b, unsigned *type, struct FtsContact *out, unsigned max)
{
    *type = b[1] >> 4;
    unsigned n = 0;
    if (*type == 2) {
        const unsigned count = b[1] & 15;
        for (unsigned i = 0; i < count && i < FTS_SPI_CONTACTS && n < max; ++i) {
            const uint8_t *r = b + 4 + 8 * i;
            if ((r[0] >> 6) == 1) continue;   // lifted
            out[n].id = r[2] >> 4;
            out[n].x = (((r[0] & 15) << 12) | (r[1] << 4) | (r[4] >> 4)) / 16;
            out[n].y = (((r[2] & 15) << 12) | (r[3] << 4) | (r[4] & 15)) / 16;
            ++n;
        }
    } else if (*type == 0) {
        for (unsigned i = 0; i < FTS_SPI_CONTACTS && n < max; ++i) {
            const uint8_t *r = b + 2 + 6 * i;
            const int id = r[2] >> 4;
            if (id >= 15) break;
            if ((r[0] >> 6) == 1) continue;   // lifted
            out[n].id = id;
            out[n].x = ((r[0] & 15) << 8) | r[1];
            out[n].y = ((r[2] & 15) << 8) | r[3];
            ++n;
        }
    }
    return n;
}

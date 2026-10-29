# FocalTechSpiTouch

FocalTech touch controllers on SPI: the wire frame, its CRC-16 and the touch report. Verified on the FT3683G of the Nothing Phone (3a)
(asteroids).

`FtsSpi.c` holds no board facts. The board supplies:

- an `FtsSpiIO`: one complete SPI transaction per call with chip select held, and a microsecond delay (QcomGeniSpi on Qualcomm boards);
- power, reset and pins, and the thread that reads reports;
- where the contacts go (the touch coordinate globals that ShengHidInput reports).

## Wire format

A read sends the register, `0xA0`, the length (big-endian) and three dummy bytes; the data follows, then its CRC-16 (reflected
polynomial `0x8408`, initial `0xFFFF`, no final xor, little-endian). Status bits `0xA0` in byte 3 refuse the frame. The same frame
and CRC are used by RigoLigoRLC's FT8203 driver ([unixnu-nativefocaltechtouch](/drivers/community-nativefocaltechtouch)), whose
I/O shape this follows; the FT8203 also needs its firmware loaded, which this driver does not do.

Register `0x01` returns an 84-byte report: protocol v2 (type 2) or the legacy layout (type 0). `fts_spi_decode` returns the contacts still
down in panel pixels. The chip id is `0xA3`/`0x9F` (`0x56`/`0x72` on the FT3683G).

## Use

The asteroids board package (`UniXNU-asteroids` `package/AsteroidsFocalTouch.cpp`) powers the chip, opens GENI SE0, identifies the chip
and reads a report every 10 ms. The controller's interrupt line is not used yet.

## Test

```sh
cd Test && cc -std=c11 -Wall -Wextra -Werror -I.. ../FtsSpi.c test_fts_spi.c -o test_fts_spi && ./test_fts_spi
```

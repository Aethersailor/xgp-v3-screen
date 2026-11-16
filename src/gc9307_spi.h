// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef XGP_GC9307_SPI_H
#define XGP_GC9307_SPI_H

void gc9307_spi_init(unsigned int brightness_percent);
void gc9307_spi_set_brightness(unsigned int brightness_percent);
void gc9307_spi_blank(void);

#endif

/*
 * Load or store DFU files including suffix and prefix
 *
 * Copyright 2014 Tormod Volden <debian.tormod@gmail.com>
 * Copyright 2012 Stefan Schmidt <stefan@datenfreihafen.org>
 *
 *  Modified for use on OS X
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __dfu_util__dfu_file__
#define __dfu_util__dfu_file__

#include <stdint.h>

struct dfu_file
{
    /* File name */
    const char *name;
    /* Pointer to file loaded into memory */
    uint8_t *firmware;
    /* Different sizes */
    struct {
        int total;
        int suffix;
    } size;
    
    /* From DFU suffix fields */
    uint32_t dwCRC;
    uint16_t bcdDFU;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
};

enum suffix_req {
    NO_SUFFIX,
    NEEDS_SUFFIX,
    MAYBE_SUFFIX
};

void dfu_load_file(struct dfu_file *file, enum suffix_req check_suffix);
void *dfu_malloc(size_t size);
void show_suffix_and_prefix(struct dfu_file *file);

#endif /* defined(__dfu_util__dfu_file__) */

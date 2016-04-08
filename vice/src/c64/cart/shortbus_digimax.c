/*
 * shortbus_digimax.c - IDE64 Digimax DAC expansion emulation.
 *
 * Written by
 *  Marco van den Heuvel <blackystardust68@yahoo.com>
 *
 * This file is part of VICE, the Versatile Commodore Emulator.
 * See README for copyright notice.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307  USA.
 *
 */

#include "vice.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cartio.h"
#include "cartridge.h"
#include "cmdline.h"
#include "lib.h"
#include "resources.h"
#include "sound.h"
#include "translate.h"
#include "util.h"

#include "digimaxcore.c"

/*
    Digimax Short Bus expansion

    This cartridge is an 8bit 4-channel digital sound output
    interface.

    When inserted into the short bus port the cart uses 4 registers,
    one for each channel. The base address can be relocated
    to be at either $DE40-$DE47 or $DE48-$DE4F.
*/

/* This flag indicates if the IDE64 cart is active */
static int shortbus_digimax_host_active = 0;

/* This flag indicated if the expansion is active,
   real activity depends on the 'host' active flag */
static int shortbus_digimax_expansion_active = 0;

/* DIGIMAX address */
static int shortbus_digimax_address;

static char *shortbus_digimax_address_list = NULL;

/* ---------------------------------------------------------------------*/

/* some prototypes are needed */
static void shortbus_digimax_sound_store(WORD addr, BYTE value);
static BYTE shortbus_digimax_sound_read(WORD addr);

static io_source_t digimax_device = {
    "ShortBus " CARTRIDGE_NAME_DIGIMAX,
    IO_DETACH_RESOURCE,
    "SBDIGIMAX",
    0xde40, 0xde47, 0x03,
    1, /* read is always valid */
    shortbus_digimax_sound_store,
    shortbus_digimax_sound_read,
    shortbus_digimax_sound_read,
    NULL, /* nothing to dump */
    CARTRIDGE_IDE64,
    0,
    0
};

static io_source_list_t *shortbus_digimax_list_item = NULL;

/* ---------------------------------------------------------------------*/

void shortbus_digimax_sound_chip_init(void)
{
    digimax_sound_chip_offset = sound_chip_register(&digimax_sound_chip);
}

static void shortbus_digimax_sound_store(WORD addr, BYTE value)
{
    digimax_sound_data[addr] = value;
    sound_store((WORD)(digimax_sound_chip_offset | addr), value, 0);
}

static BYTE shortbus_digimax_sound_read(WORD addr)
{
    BYTE value = sound_read((WORD)(digimax_sound_chip_offset | addr), 0);

    return value;
}

/* ---------------------------------------------------------------------*/

void shortbus_digimax_unregister(void)
{
    if (shortbus_digimax_list_item != NULL) {
        io_source_unregister(shortbus_digimax_list_item);
        shortbus_digimax_list_item = NULL;
        digimax_sound_chip.chip_enabled = 0;
    }
    shortbus_digimax_host_active = 0;
}

void shortbus_digimax_register(void)
{
    if (!digimax_sound_chip.chip_enabled && shortbus_digimax_expansion_active) {
        shortbus_digimax_list_item = io_source_register(&digimax_device);
        digimax_sound_chip.chip_enabled = 1;
    }
    shortbus_digimax_host_active = 1;
}

/* ---------------------------------------------------------------------*/

static int set_shortbus_digimax_enabled(int value, void *param)
{
    int val = value ? 1 : 0;

    if (shortbus_digimax_host_active) {
        if (!digimax_sound_chip.chip_enabled && val) {
            shortbus_digimax_list_item = io_source_register(&digimax_device);
            digimax_sound_chip.chip_enabled = 1;
        } else if (digimax_sound_chip.chip_enabled && !val) {
            if (shortbus_digimax_list_item != NULL) {
                io_source_unregister(shortbus_digimax_list_item);
                shortbus_digimax_list_item = NULL;
            }
            digimax_sound_chip.chip_enabled = 0;
        }
    }
    shortbus_digimax_expansion_active = val;

    return 0;
}

static int set_shortbus_digimax_base(int val, void *param)
{
    int addr = val;
    int old = digimax_sound_chip.chip_enabled;

    if (val == shortbus_digimax_address) {
        return 0;
    }

    if (old) {
        set_shortbus_digimax_enabled(0, NULL);
    }

    switch (addr) {
        case 0xde40:
        case 0xde48:
            digimax_device.start_address = (WORD)addr;
            digimax_device.end_address = (WORD)(addr + 3);
            break;
        default:
            return -1;
    }

    shortbus_digimax_address = val;

    if (old) {
        set_shortbus_digimax_enabled(1, NULL);
    }
    return 0;
}

void shortbus_digimax_reset(void)
{
}

/* ---------------------------------------------------------------------*/

static const resource_int_t resources_int[] = {
    { "SBDIGIMAX", 0, RES_EVENT_STRICT, (resource_value_t)0,
      &digimax_sound_chip.chip_enabled, set_shortbus_digimax_enabled, NULL },
    { "SBDIGIMAXbase", 0xde40, RES_EVENT_NO, NULL,
      &shortbus_digimax_address, set_shortbus_digimax_base, NULL },
    { NULL }
};

int shortbus_digimax_resources_init(void)
{
    return resources_register_int(resources_int);
}

void shortbus_digimax_resources_shutdown(void)
{
    if (shortbus_digimax_address_list) {
        lib_free(shortbus_digimax_address_list);
    }
}

/* ---------------------------------------------------------------------*/

static const cmdline_option_t cmdline_options[] =
{
    { "-sbdigimax", SET_RESOURCE, 0,
      NULL, NULL, "SBDIGIMAX", (resource_value_t)1,
      USE_PARAM_STRING, USE_DESCRIPTION_ID,
      IDCLS_UNUSED, IDCLS_ENABLE_SHORTBUS_DIGIMAX,
      NULL, NULL },
    { "+sbdigimax", SET_RESOURCE, 0,
      NULL, NULL, "SBDIGIMAX", (resource_value_t)0,
      USE_PARAM_STRING, USE_DESCRIPTION_ID,
      IDCLS_UNUSED, IDCLS_DISABLE_SHORTBUS_DIGIMAX,
      NULL, NULL },
    { NULL }
};

static cmdline_option_t base_cmdline_options[] =
{
    { "-sbdigimaxbase", SET_RESOURCE, 1,
      NULL, NULL, "SBDIGIMAXbase", NULL,
      USE_PARAM_ID, USE_DESCRIPTION_COMBO,
      IDCLS_P_BASE_ADDRESS, IDCLS_SHORTBUS_DIGIMAX_BASE,
      NULL, NULL },
    { NULL }
};

int shortbus_digimax_cmdline_options_init(void)
{
    char *temp1;

    if (cmdline_register_options(cmdline_options) < 0) {
        return -1;
    }

    temp1 = util_gen_hex_address_list(0xde40, 0xde50, 8);
    shortbus_digimax_address_list = util_concat(". (", temp1, ")", NULL);
    lib_free(temp1);

    base_cmdline_options[0].description = shortbus_digimax_address_list;

    return cmdline_register_options(base_cmdline_options);
}

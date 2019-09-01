/*
 * This file is part of the Skycoin project, https://skycoin.net/
 *
 * Copyright (C) 2014 Pavol Rusnak <stick@satoshilabs.com>
 * Copyright (C) 2018-2019 Skycoin Project
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "memory.h"
#include "sha2.h"
#include <libopencm3/stm32/flash.h>
#include <stdint.h>

#define FLASH_OPTION_BYTES_1 (*(const uint64_t*)0x1FFFC000)
#define FLASH_OPTION_BYTES_2 (*(const uint64_t*)0x1FFFC008)

#ifdef BOOTLOADER
void memory_protect(void)
{
#if MEMORY_PROTECT
    // Reference STM32F205 Flash programming manual revision 5 http://www.st.com/resource/en/programming_manual/cd00233952.pdf
    // Section 2.6 Option bytes
    //                     set RDP level 2                   WRP for sectors 0 and 1            flash option control register matches
    if (((FLASH_OPTION_BYTES_1 & 0xFFEC) == 0xCCEC) && ((FLASH_OPTION_BYTES_2 & 0xFFF) == 0xFFC) && (FLASH_OPTCR == 0x0FFCCCED)) {
        return; // already set up correctly - bail out
    }
    flash_unlock_option_bytes();
    // Section 2.8.6 Flash option control register (FLASH_OPTCR)
    //   Bits 31:28 Reserved, must be kept cleared.
    //   Bits 27:16 nWRP: Not write protect: write protect bootloader code in flash main memory sectors 0 and 1 (Section 2.3; table 2)
    //   Bits 15:8 RDP: Read protect: level 2 chip read protection active
    //   Bits 7:5 USER: User option bytes: no reset on standby, no reset on stop, software watchdog
    //   Bit 4 Reserved, must be kept cleared.
    //   Bits 3:2 BOR_LEV: BOR reset Level: BOR off
    //   Bit 1 OPTSTRT: Option start: ignored by flash_program_option_bytes
    //   Bit 0 OPTLOCK: Option lock: ignored by flash_program_option_bytes
    flash_program_option_bytes(0x0FFCCCEC);
    flash_lock_option_bytes();
#endif
}
#endif

int memory_bootloader_hash(uint8_t* hash)
{
    sha256_Raw(FLASH_PTR(FLASH_BOOT_START), FLASH_BOOT_LEN, hash);
    sha256_Raw(hash, 32, hash);
    return 32;
}

uint8_t memory_rdp_level(void)
{
    static uint8_t rdp_level = 0x7F;

    if (rdp_level > 2) {
        switch (FLASH_OPTION_BYTES_1 & 0xFF00) {
        case 0xCC00:
            rdp_level = 2;
            break;
        case 0xAA00:
            rdp_level = 0;
            break;
        default:
            rdp_level = 1;
            break;
        }
    }
    return rdp_level;
}

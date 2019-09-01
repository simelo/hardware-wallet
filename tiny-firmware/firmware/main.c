/*
 * This file is part of the Skycoin project, https://skycoin.net/
 *
 * Copyright (C) 2018-2019 Skycoin Project
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

#include <libopencm3/stm32/desig.h>

#include "bitmaps.h"
#include "bootloader_integrity.h"
#include "buttons.h"
#include "entropy.h"
#include "factory_test.h"
#include "fastflash.h"
#include "gettext.h"
#include "layout.h"
#include "layout2.h"
#include "memory.h"
#include "oled.h"
#include "rng.h"
#include "setup.h"
#include "skywallet.h"
#include "storage.h"
#include "timer.h"
#include "usb.h"
#include "util.h"

#ifdef __CYGWIN__
#ifdef main
#undef main
#endif
#endif

extern uint32_t storage_uuid[STM32_UUID_LEN / sizeof(uint32_t)];

int main(void)
{
#if defined(EMULATOR) && EMULATOR == 1
    setup();
    __stack_chk_guard = random32(); // this supports compiler provided unpredictable stack protection checks
    oledInit();
#else  // defined(EMULATOR) && EMULATOR == 1
    if (!check_bootloader()) {
        layoutDialog(&bmp_icon_error, NULL, NULL, NULL, "Unknown bootloader", "detected.", NULL,
            "Unplug your Skywallet",
            "contact our support.", NULL);
        for (;;)
            ;
    }
    setupApp();
    __stack_chk_guard = random32(); // this supports compiler provided unpredictable stack protection checks
#endif // defined(EMULATOR) && EMULATOR == 1

#if FASTFLASH
    uint16_t state = gpio_port_read(BTN_PORT);
    if ((state & BTN_PIN_NO) == 0) {
        run_bootloader();
    }
#endif

    timer_init();

#if !defined(EMULATOR) || EMULATOR == 0
    memory_rdp_level();
    desig_get_unique_id(storage_uuid);
    // enable MPU (Memory Protection Unit)
    mpu_config();
#else
    random_buffer((uint8_t*)storage_uuid, sizeof(storage_uuid));
#endif // !defined(EMULATOR) || EMULATOR == 0

#if DEBUG_LINK
    oledSetDebugLink(1);
    storage_wipe();
#endif

    oledDrawBitmap(0, 0, &bmp_skycoin_logo64);
    oledRefresh();

    storage_init();
    layoutHome();
    usbInit();
    for (;;) {
        usbPoll();
        check_lock_screen();
        check_factory_test();
        check_entropy();
    }

    return 0;
}

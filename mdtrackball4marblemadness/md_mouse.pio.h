// ---------------------------------------------------------------- //
// md_mouse.pio (TH=1時TL追従対応版 / 31命令)
// ---------------------------------------------------------------- //

#pragma once

#if !PICO_NO_HARDWARE
#include "hardware/pio.h"
#endif

#ifdef __cpp_constexpr
#define __pio_const constexpr
#else
#define __pio_const const
#endif

#define md_mouse_wrap_target 0
#define md_mouse_wrap 30
#define md_mouse_pio_version 0

#define md_mouse_TL_WAIT 31

static __pio_const uint16_t md_mouse_program_instructions[] = {
            //     .wrap_target
    0xa003, //  0: mov    pins, null
    0x00d8, //  1: jmp    pin, 24
    0xf80b, //  2: set    pins, 11        side 1
    0x6060, //  3: out    null, 32
    0xe040, //  4: set    y, 0
    0x00c0, //  5: jmp    pin, 0
    0xa0c3, //  6: mov    isr, null
    0x4002, //  7: in     pins, 2
    0xa026, //  8: mov    x, isr
    0x00a5, //  9: jmp    x != y, 5
    0x00f0, // 10: jmp    !osre, 16
    0xc000, // 11: irq    nowait 0
    0x2041, // 12: wait   0 irq, 1
    0x8080, // 13: pull   noblock
    0xa0c7, // 14: mov    isr, osr
    0x8000, // 15: push   noblock
    0x6004, // 16: out    pins, 4
    0xe03f, // 17: set    x, 31
    0x0752, // 18: jmp    x--, 18                [7]
    0x0076, // 19: jmp    !y, 22
    0xf840, // 20: set    y, 0            side 1
    0x0005, // 21: jmp    5
    0xf042, // 22: set    y, 2            side 0
    0x0005, // 23: jmp    5
    0xe041, // 24: set    y, 1
    0xa0c3, // 25: mov    isr, null
    0x4002, // 26: in     pins, 2
    0xa026, // 27: mov    x, isr
    0x00be, // 28: jmp    x != y, 30
    0x1001, // 29: jmp    1               side 0
    0x1801, // 30: jmp    1               side 1
            //     .wrap
};

#if !PICO_NO_HARDWARE
static __pio_const struct pio_program md_mouse_program = {
    .instructions = md_mouse_program_instructions,
    .length = 31,
    .origin = -1,
    .pio_version = md_mouse_pio_version,
#if PICO_PIO_VERSION > 0
    .used_gpio_ranges = 0x1
#endif
};

static inline pio_sm_config md_mouse_program_get_default_config(uint offset) {
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + md_mouse_wrap_target, offset + md_mouse_wrap);
    sm_config_set_sideset(&c, 2, true, false);
    return c;
}
#endif
/* SPDX-License-Identifier: MIT
 * Copyright (C) 2026 David Ryan */
/* HALF-LIGHT: harmonic weather. The app's half of the contract with fm1-base (device/unity.c, host/plat_host.c). */
#pragma once
#include <stdint.h>
#include "plat.h"

#define APP_NAME "HALF-LIGHT"
#define APP_TAGLINE "harmonic weather"
#define APP_SPLASH_RGB RGB(74, 63, 143)
#define APP_BLOCK 256u                   /* frames per audio block: 5.8 ms at 44.1 kHz */

/* the store: the project (climate, voice, pins, every setting) at Felucca's settings sectors, the panel calibration
 * (the platform's) in the data area */
enum { OBJ_PROJ, OBJ_PANEL, OBJ_NOBJ };
#define APP_STORE_MAP {0xFC000u, 0xDC000u}

/* the contract: the platform calls these */
void app_init(void);                     /* boot: the engine, then the saved project (or the defaults) */
void app_render(int32_t *lr, uint32_t n, uint32_t gain_q12);   /* audio interrupt: n stereo frames, 24-bit */
void app_panic(void);                    /* silence now (before an update rewrites the flash) */
void app_ui_init(void);
void app_frame(void);
void app_input_only(void);
void app_say(const char *a, const char *b);

/* the host simulator's checks (host/plat_host.c "expect") */
#define APP_HOST_EXPECT
int app_expect(const char *what, int *got);

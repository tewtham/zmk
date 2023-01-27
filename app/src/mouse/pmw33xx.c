/*
 * Copyright (c) 2021 Cedric VINCENT
 *
 * SPDX-License-Identifier: MIT
 */

#include <drivers/sensor.h>
#include <logging/log.h>
#include <zmk/hid.h>
#include <zmk/endpoints.h>
#include <zmk/pmw33xx.h>
#include <zmk/keymap.h>
#include <stdlib.h>

#define LAYER_THRESHOLD 10
#define RESOLUTION_SETTING 0x9 // 0x29 max, 0x09 default
#define RESOLUTION_SETTING_SCROLL 0x01
#define SCROLL_ADJ 30.0f // when scrolling, we'll divide the movement by this much

LOG_MODULE_REGISTER(PMW33XX_MOUSE, CONFIG_ZMK_LOG_LEVEL);

static int mode = DT_PROP(DT_INST(0, pixart_pmw33xx), mode);

void zmk_pmw33xx_set_mode(int new_mode) {
    struct sensor_value attr;
    const struct device *dev;

    const char *label = DT_LABEL(DT_INST(0, pixart_pmw33xx));
    dev = device_get_binding(label);
    if (dev == NULL) {
        LOG_ERR("Cannot get TRACKBALL_PMW33XX device");
        return;
    }

    switch (new_mode) {
        case PMW33XX_MOVE:
        case PMW33XX_SCROLL:
            mode = new_mode;
            break;

       case PMW33XX_TOGGLE:
            mode = mode == PMW33XX_MOVE
                   ? PMW33XX_SCROLL
                   : PMW33XX_MOVE;
            break;

       default:
            break;
    }
}

static int round(float num) {
    return (int)(num < 0 ? (num - 0.5) : (num + 0.5));
}

static void thread_code(void *p1, void *p2, void *p3)
{
    const struct device *dev;
    int result;
    uint8_t xSkips = 0, ySkips = 0;

    /* PMW33XX trackball initialization. */
    const char *label = DT_LABEL(DT_INST(0, pixart_pmw33xx));
    dev = device_get_binding(label);
    if (dev == NULL) {
        LOG_ERR("Cannot get TRACKBALL_PMW33XX device");
        return;
    }

    /* Event loop. */
    while (true) {
        struct sensor_value pos_dx, pos_dy;
        bool send_report = false;
        int clear = PMW33XX_NONE;

        result = sensor_sample_fetch(dev);
        if (result < 0) {
            LOG_ERR("Failed to fetch TRACKBALL_PMW33XX sample");
            return;
        }

        result = sensor_channel_get(dev, SENSOR_CHAN_POS_DX, &pos_dx);
        if (result < 0) {
            LOG_ERR("Failed to get TRACKBALL_PMW33XX pos_dx channel value");
            return;
        }

        result = sensor_channel_get(dev, SENSOR_CHAN_POS_DY, &pos_dy);
        if (result < 0) {
            LOG_ERR("Failed to get TRACKBALL_PMW33XX pos_dy channel value");
            return;
        }

        if (pos_dx.val1 != 0 || pos_dy.val1 != 0) {
            switch(mode) {
                default:
                case PMW33XX_MOVE: {
                    int dx = pos_dx.val1;
                    int dy = pos_dy.val1;
                    zmk_hid_mouse_movement_set(dx, dy);
                    send_report = true;
                    clear = PMW33XX_MOVE;
                    break;
                }

                case PMW33XX_SCROLL: {
                    xSkips++;
                    ySkips++;
                    int dy = round((pos_dy.val1/SCROLL_ADJ) * ySkips);
                    int dx = round((pos_dx.val1/SCROLL_ADJ) * xSkips);

                    // if after adjusting the movement we're not at 0, reset skips
                    if (dy != 0) ySkips = 0;
                    if (dx != 0) xSkips = 0;

                    zmk_hid_mouse_scroll_set(dx, -dy);
                    send_report = true;
                    clear = PMW33XX_SCROLL;
                    break;
                }
            }
        }

        if (send_report) {
            if (abs(pos_dx.val1) > LAYER_THRESHOLD || abs(pos_dy.val1) > LAYER_THRESHOLD) {
                zmk_keymap_layer_to(2);
            }
            zmk_endpoints_send_mouse_report();

            switch (clear) {
                case PMW33XX_MOVE: zmk_hid_mouse_movement_set(0, 0); break;
                case PMW33XX_SCROLL: zmk_hid_mouse_scroll_set(0, 0); break;
                default: break;
            }
        }

        k_sleep(K_MSEC(10));
    }
}

#define STACK_SIZE 1024

static K_THREAD_STACK_DEFINE(thread_stack, STACK_SIZE);
static struct k_thread thread;

int zmk_pmw33xx_init()
{
    k_thread_create(&thread, thread_stack, STACK_SIZE, thread_code,
                    NULL, NULL, NULL, K_PRIO_PREEMPT(8), 0, K_NO_WAIT);
    return 0;
}

SYS_INIT(zmk_pmw33xx_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

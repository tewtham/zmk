/*
 * Copyright (c) 2021 Cedric VINCENT
 *
 * SPDX-License-Identifier: MIT
 */

#include <drivers/sensor.h>
#include <logging/log.h>
#include <zmk/hid.h>
#include <zmk/endpoints.h>
#include <stdlib.h>
#include <dt-bindings/zmk/keys.h>
#include <zmk/event_manager.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/matrix_transform.h>

struct axis_state {
    bool higher;
    bool lower;
    uint8_t label;
};

LOG_MODULE_REGISTER(JOYSTICK_LOOP, CONFIG_ZMK_LOG_LEVEL);

#define THRESHOLD 800

static int do_work(const struct device *dev, struct axis_state *state, uint32_t low_key, uint32_t high_key) {
    struct sensor_value value;

    int err = sensor_sample_fetch(dev);
    if (err < 0) {
        LOG_ERR("Failed to fetch joystick sample");
        return err;
    }

    err = sensor_channel_get(dev, SENSOR_CHAN_PRESS, &value);
    if (err) {
        LOG_ERR("Failed to get joystick channel value");
        return err;
    }

    if ((value.val1 < -THRESHOLD) != state->lower) {
        err = ZMK_EVENT_RAISE(new_zmk_position_state_changed(
            (struct zmk_position_state_changed){.source = ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL,
                                                .state = !state->lower,
                                                .position = low_key,
                                                .timestamp = k_uptime_get()}));
        if (err) {
            LOG_ERR("updating lower key failed %d", state->label);
        } else {
            state->lower = !state->lower;
        }
        LOG_DBG("updating low %d, %s", state->label, state->lower ? "on" : "off");
    }
    
    if ((value.val1 > THRESHOLD) != state->higher) {
        err = ZMK_EVENT_RAISE(new_zmk_position_state_changed(
            (struct zmk_position_state_changed){.source = ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL,
                                                .state = !state->higher,
                                                .position = high_key,
                                                .timestamp = k_uptime_get()}));

        if (err) {
            LOG_ERR("updating higher key failed %d", state->label);
        } else {
            state->higher = !state->higher;
        }
        LOG_DBG("updating high %d, %s", state->label, state->higher ? "on" : "off");
    }

    return 0;
}

static void thread_code(void *p1, void *p2, void *p3)
{
    const struct device *devx, *devy;
    struct axis_state statex, statey;

    uint32_t xhigh_key = zmk_matrix_transform_row_column_to_position(5ul, 3ul);
    uint32_t xlow_key = zmk_matrix_transform_row_column_to_position(5ul, 1ul);
    statex.lower = false;
    statex.higher = false;
    statex.label = 0;

    uint32_t yhigh_key = zmk_matrix_transform_row_column_to_position(5ul, 0ul);
    uint32_t ylow_key = zmk_matrix_transform_row_column_to_position(5ul, 2ul);
    statey.lower = false;
    statey.higher = false;
    statey.label = 1;

    int err;

    const char *label = DT_LABEL(DT_INST(0, joystick));
    devx = device_get_binding(label);
    if (devx == NULL) {
        LOG_ERR("Cannot get joystick device");
        return;
    }

    label = DT_LABEL(DT_INST(1, joystick));
    devy = device_get_binding(label);
    if (devy == NULL) {
        LOG_ERR("Cannot get joystick device");
        return;
    }

    while (true) {
        do_work(devx, &statex, xlow_key, xhigh_key);
        do_work(devy, &statey, ylow_key, yhigh_key);
        k_sleep(K_MSEC(50));
    }
}

#define STACK_SIZE 1024

static K_THREAD_STACK_DEFINE(thread_stack, STACK_SIZE);
static struct k_thread thread;

int zmk_joystick_init()
{
    k_thread_create(&thread, thread_stack, STACK_SIZE, thread_code,
                    NULL, NULL, NULL, K_PRIO_PREEMPT(8), 0, K_NO_WAIT);
    return 0;
}

SYS_INIT(zmk_joystick_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

/*
 * Copyright (c) 2021 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_pmw33xx

#include <devicetree.h>
#include <drivers/behavior.h>
#include <zmk/pmw33xx.h>

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

struct config {
    int mode;
    bool momentary;
};

static int behavior_pmw33xx_init(const struct device *dev) { return 0; };

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    const struct device *device = device_get_binding(binding->behavior_dev);
    const struct config *config = device->config;

    zmk_pmw33xx_set_mode(config->mode);

    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    const struct device *device = device_get_binding(binding->behavior_dev);
    const struct config *config = device->config;

    if (config->momentary) {
        zmk_pmw33xx_set_mode(PMW33XX_TOGGLE);
    }

    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_pmw33xx_driver_api = {
    .binding_pressed  = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
};

#define PMW33XX_INST(n)                                                              \
    static const struct config config_##n = {                                       \
        .mode      = DT_INST_PROP(n, mode),                                         \
        .momentary = DT_INST_PROP(n, momentary)                                     \
    };                                                                              \
                                                                                    \
    DEVICE_DT_INST_DEFINE(n, behavior_pmw33xx_init, device_pm_control_nop, \
                          NULL, &config_##n, APPLICATION,                           \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &behavior_pmw33xx_driver_api);

DT_INST_FOREACH_STATUS_OKAY(PMW33XX_INST)

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */

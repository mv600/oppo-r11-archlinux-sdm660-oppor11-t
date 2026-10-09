/*
 * R11T OSM power-profiles-daemon platform driver.
 *
 * This driver keeps the profile activation contract in PPD instead of
 * requiring an external D-Bus watcher to rewrite cpufreq limits.
 */

#pragma once

#include "ppd-driver-platform.h"

#define R11T_OSM_DRIVER_NAME "r11t_osm"
#define PPD_TYPE_DRIVER_R11T_OSM (ppd_driver_r11t_osm_get_type ())
G_DECLARE_FINAL_TYPE (PpdDriverR11tOsm,
                      ppd_driver_r11t_osm,
                      PPD,
                      DRIVER_R11T_OSM,
                      PpdDriverPlatform)

#ifndef NAVIGATION_H
#define NAVIGATION_H

#include <stdint.h>

/* Distances in cm; tune these for the actual vehicle's stopping distance. */
#define NAV_FRONT_BLOCK_CM 60U
#define NAV_FRONT_CLEAR_CM 85U
#define NAV_SIDE_BLOCK_CM 30U
#define NAV_SIDE_CLEAR_CM 40U
#define NAV_DIRECTION_MARGIN_CM 10U
#define NAV_MIN_TURN_MS 250U
#define NAV_STALE_MS 450U

typedef enum { SENSOR_LEFT, SENSOR_FRONT, SENSOR_RIGHT, SENSOR_COUNT } sensor_id_t;
typedef enum { MOTION_STOP, MOTION_FORWARD, MOTION_LEFT, MOTION_RIGHT } motion_t;
typedef struct {
    uint16_t cm;
    uint8_t valid;
    uint32_t measured_ms;
} distance_t;
typedef struct {
    motion_t motion;
    motion_t preferred_turn;
    uint32_t turn_started_ms;
} navigation_t;

void Navigation_Reset(navigation_t *nav);
motion_t Navigation_Update(navigation_t *nav, const volatile distance_t distances[3],
                           uint8_t running, uint32_t now);

#endif

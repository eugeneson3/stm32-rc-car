#include "navigation.h"

void Navigation_Reset(navigation_t *nav)
{
    nav->motion = MOTION_STOP;
    nav->preferred_turn = MOTION_LEFT;
    nav->turn_started_ms = 0;
}

motion_t Navigation_Update(navigation_t *nav, const volatile distance_t d[3],
                           uint8_t running, uint32_t now)
{
    if (!running) return nav->motion = MOTION_STOP;
    for (unsigned i = 0; i < SENSOR_COUNT; ++i) {
        if (!d[i].valid || (uint32_t)(now - d[i].measured_ms) > NAV_STALE_MS)
            return nav->motion = MOTION_STOP;
    }

    uint8_t turning = nav->motion == MOTION_LEFT || nav->motion == MOTION_RIGHT;
    uint16_t front_limit = turning ? NAV_FRONT_CLEAR_CM : NAV_FRONT_BLOCK_CM;
    uint16_t side_limit = turning ? NAV_SIDE_CLEAR_CM : NAV_SIDE_BLOCK_CM;
    uint8_t clear = d[SENSOR_FRONT].cm >= front_limit &&
                    d[SENSOR_LEFT].cm >= side_limit && d[SENSOR_RIGHT].cm >= side_limit;

    /* Hold a chosen turn briefly, but never override disabled/invalid sensors. */
    if (turning && (uint32_t)(now - nav->turn_started_ms) < NAV_MIN_TURN_MS)
        return nav->motion;
    if (clear) return nav->motion = MOTION_FORWARD;

    motion_t next = nav->preferred_turn;
    if (d[SENSOR_LEFT].cm > d[SENSOR_RIGHT].cm + NAV_DIRECTION_MARGIN_CM)
        next = MOTION_LEFT;
    else if (d[SENSOR_RIGHT].cm > d[SENSOR_LEFT].cm + NAV_DIRECTION_MARGIN_CM)
        next = MOTION_RIGHT;
    if (next != nav->motion) nav->turn_started_ms = now;
    nav->preferred_turn = next;
    return nav->motion = next;
}

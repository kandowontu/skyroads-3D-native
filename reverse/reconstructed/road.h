#ifndef SKYROADS_RECOVERED_ROAD_H
#define SKYROADS_RECOVERED_ROAD_H

#include <stddef.h>
#include <stdint.h>

/*
 * Semantic reconstruction of skyroads.exe 1000:04C0.
 * Rows contain seven 16-bit road-cell descriptors. Distance is 16.16 fixed
 * point; horizontal_position uses the executable's 1/128 coordinate scale.
 */
uint16_t sr_road_cell_at_position(
    const uint16_t *cells,
    size_t row_count,
    uint32_t distance,
    uint16_t horizontal_position);

/* Exact collision predicate reconstructed from 1000:1584 and 1000:1685. */
int sr_test_ship_collision(
    const uint16_t *cells,
    size_t row_count,
    uint32_t distance,
    uint16_t horizontal_position,
    uint16_t ship_height);

/* Exact finish-gate height predicate reconstructed from 1000:0533. */
int sr_test_finish_gate(
    const uint16_t *cells,
    size_t row_count,
    uint32_t distance,
    uint16_t horizontal_position,
    uint16_t ship_height);

typedef struct SrShipPosition {
    uint32_t distance;              /* DS:9628:962A */
    uint16_t horizontal_position;   /* DS:AF2C */
    uint16_t height;                /* DS:AF3C */
} SrShipPosition;

/* Movement subdivision/refinement reconstruction of skyroads.exe 1000:17BE. */
void sr_move_ship_with_collision(
    const uint16_t *cells,
    size_t row_count,
    SrShipPosition *position,
    SrShipPosition target);

#endif

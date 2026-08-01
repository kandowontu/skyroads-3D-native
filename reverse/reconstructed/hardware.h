#ifndef SKYROADS_RECOVERED_HARDWARE_H
#define SKYROADS_RECOVERED_HARDWARE_H

#include <stdint.h>

typedef struct SrBiosRegisters {
    uint16_t ax;
    uint16_t bx;
    uint16_t cx;
    uint16_t dx;
} SrBiosRegisters;

typedef struct SrMachineIo {
    void *context;
    void (*interrupt)(void *context, uint8_t number, SrBiosRegisters *registers);
    uint8_t (*in8)(void *context, uint16_t port);
    void (*out8)(void *context, uint16_t port, uint8_t value);
} SrMachineIo;

/* Exact semantic reconstructions of 1000:5FFD, 1000:5FE9, and 1000:019C. */
int sr_detect_vga(const SrMachineIo *machine);
int sr_detect_ega(const SrMachineIo *machine);
uint16_t sr_sample_pit_seed(const SrMachineIo *machine);

#endif

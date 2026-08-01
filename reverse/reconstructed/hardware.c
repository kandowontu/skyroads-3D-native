#include "hardware.h"

int sr_detect_vga(const SrMachineIo *machine) {
    SrBiosRegisters registers = {0};
    uint8_t al;
    uint8_t bl;

    registers.ax = 0x1a00;
    machine->interrupt(machine->context, 0x10, &registers);
    al = (uint8_t)registers.ax;
    bl = (uint8_t)registers.bx;
    return al == 0x1a && bl >= 7 && bl <= 8;
}

int sr_detect_ega(const SrMachineIo *machine) {
    SrBiosRegisters registers = {0};
    uint8_t bl;

    registers.ax = 0x1200;
    registers.bx = 0x0010;
    machine->interrupt(machine->context, 0x10, &registers);
    bl = (uint8_t)registers.bx;
    return bl != 0x10 && bl >= 3;
}

uint16_t sr_sample_pit_seed(const SrMachineIo *machine) {
    uint8_t ah;
    uint8_t al;

    ah = machine->in8(machine->context, 0x40);
    ah = (uint8_t)(ah + machine->in8(machine->context, 0x40));
    ah ^= machine->in8(machine->context, 0x41);
    al = machine->in8(machine->context, 0x41);
    return (uint16_t)(uint8_t)(al + ah);
}

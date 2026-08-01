bits 16
org 0

%define KEY_RECORD_COUNT 2048
%define KEY_RECORD_BYTES (KEY_RECORD_COUNT * 6)
%define AXIS_VALUE_COUNT 18
%define JOY_RECORD_COUNT (2 * AXIS_VALUE_COUNT * AXIS_VALUE_COUNT)
%define JOY_RECORD_BYTES (JOY_RECORD_COUNT * 14)
%define MOUSE_VALUE_COUNT 4
%define MOUSE_RECORD_COUNT (MOUSE_VALUE_COUNT * MOUSE_VALUE_COUNT * MOUSE_VALUE_COUNT * MOUSE_VALUE_COUNT * 2)
%define MOUSE_RECORD_BYTES (MOUSE_RECORD_COUNT * 22)

; Symbol table consumed by build_oracle_exe.py --input.
dw entry_hook
dw input_harness
dw joystick_axis_hook
dw joystick_button_hook
dw mouse_value_hook
dw mouse_set_hook
dw int6_handler
dw sample_call_segment_relocation
dw axis_return_segment_relocation
dw button_return_segment_relocation
dw mouse_value_return_segment_relocation
dw mouse_set_return_segment_relocation

%macro save_registers 0
    push ax
    push cx
    push dx
    push bx
    push bp
    push si
    push di
%endmacro

%macro restore_registers 0
    pop di
    pop si
    pop bp
    pop bx
    pop dx
    pop cx
    pop ax
%endmacro

entry_hook:
    pushf
    push ax
    push es
    xor ax, ax
    mov es, ax
    cli
    mov word [es:0x18], int6_handler
    push cs
    pop ax
    mov [es:0x1a], ax
    pop es
    pop ax
    popf

    ; Displaced bytes from the MZ entry at 1000:60D0-60D4.
    mov di, 0x066e
    mov ds, di
    retf

; The original main calls the intro at 1000:4575 after CRT/DGROUP setup.  The
; generated input executable diverts that call here, invokes the untouched
; 1000:074C sampler over controlled globals/devices, writes its results, and
; exits without entering the game.
input_harness:
    cld
    ; The enlarged oracle buffer occupies the otherwise unused upper DGROUP
    ; address range.  Freeze the timer ISR while the synthetic harness runs so
    ; its temporary stack frame cannot alias that generated-only buffer.
    cli
    push ss
    pop ds

    ; Exhaust every pressed/not-pressed combination of the eleven key bytes.
    xor si, si
    mov word [cs:key_buffer_cursor], 0
.keyboard_record:
    mov word [0x9602], 0
    mov ax, si
    xor di, di
.keyboard_key:
    xor dx, dx
    shr ax, 1
    jnc .keyboard_store
    mov dl, 0x80
.keyboard_store:
    mov [di+0x0ba2], dl
    inc di
    cmp di, 11
    jb .keyboard_key
    call call_sample
    mov bx, [cs:key_buffer_cursor]
    mov ax, [0x9600]
    mov [cs:key_buffer+bx], ax
    mov ax, [0x933c]
    mov [cs:key_buffer+bx+2], ax
    mov ax, [0x5488]
    mov [cs:key_buffer+bx+4], ax
    add word [cs:key_buffer_cursor], 6
    inc si
    cmp si, KEY_RECORD_COUNT
    jb .keyboard_record

    mov dx, key_name
    mov si, key_buffer
    mov cx, KEY_RECORD_BYTES
    call write_file
    jc input_failed

    ; Joystick thresholds are independent per axis.  Cross every selected
    ; center/value pair for steering and throttle, including 16-bit overflow
    ; boundaries in the executable's wrapped 3*center calculation.
    mov word [cs:joy_kind], 0
    mov word [cs:joy_center_index], 0
    mov word [cs:joy_value_index], 0
    mov word [cs:joy_buffer_cursor], 0
.joy_record:
    mov bx, [cs:joy_center_index]
    shl bx, 1
    mov cx, [cs:axis_values+bx]
    mov bx, [cs:joy_value_index]
    shl bx, 1
    mov dx, [cs:axis_values+bx]
    mov word [0x9602], 1
    cmp word [cs:joy_kind], 0
    jne .joy_throttle
    mov [0x5180], cx
    mov word [0x548a], 100
    mov [cs:synthetic_axis_x], dx
    mov word [cs:synthetic_axis_y], 100
    jmp short .joy_ready
.joy_throttle:
    mov word [0x5180], 100
    mov [0x548a], cx
    mov word [cs:synthetic_axis_x], 100
    mov [cs:synthetic_axis_y], dx
.joy_ready:
    mov ax, [cs:joy_buffer_cursor]
    mov bx, 14
    xor dx, dx
    div bx
    and ax, 1
    mov [cs:synthetic_button], ax
    call call_sample

    mov bx, [cs:joy_buffer_cursor]
    mov ax, [cs:joy_kind]
    mov [cs:joy_buffer+bx], ax
    mov ax, [cs:joy_center_index]
    shl ax, 1
    mov si, ax
    mov ax, [cs:axis_values+si]
    mov [cs:joy_buffer+bx+2], ax
    mov ax, [cs:joy_value_index]
    shl ax, 1
    mov si, ax
    mov ax, [cs:axis_values+si]
    mov [cs:joy_buffer+bx+4], ax
    mov ax, [cs:synthetic_button]
    mov [cs:joy_buffer+bx+6], ax
    mov ax, [0x9600]
    mov [cs:joy_buffer+bx+8], ax
    mov ax, [0x933c]
    mov [cs:joy_buffer+bx+10], ax
    mov ax, [0x5488]
    mov [cs:joy_buffer+bx+12], ax
    add word [cs:joy_buffer_cursor], 14

    inc word [cs:joy_value_index]
    cmp word [cs:joy_value_index], AXIS_VALUE_COUNT
    jb .joy_record
    mov word [cs:joy_value_index], 0
    inc word [cs:joy_center_index]
    cmp word [cs:joy_center_index], AXIS_VALUE_COUNT
    jb .joy_record
    mov word [cs:joy_center_index], 0
    inc word [cs:joy_kind]
    cmp word [cs:joy_kind], 2
    jb .joy_record

    mov dx, joy_name
    mov si, joy_buffer
    mov cx, JOY_RECORD_BYTES
    call write_file
    jc input_failed

    ; Cross values immediately below/at/above all mouse thresholds.  The
    ; synthetic INT 33h replacement also returns distinct sequential reads so
    ; the original's x0/x1/y0/y1/y2 evaluation order is covered.
    mov word [cs:mouse_x0_index], 0
    mov word [cs:mouse_x1_index], 0
    mov word [cs:mouse_y0_index], 0
    mov word [cs:mouse_y1_index], 0
    mov word [cs:mouse_button_index], 0
    mov word [cs:mouse_record_index], 0
    mov word [cs:mouse_buffer_cursor], 0
.mouse_record:
    mov bx, [cs:mouse_x0_index]
    shl bx, 1
    mov ax, [cs:mouse_x_values+bx]
    mov [cs:synthetic_mouse_x0], ax
    mov bx, [cs:mouse_x1_index]
    shl bx, 1
    mov ax, [cs:mouse_x_values+bx]
    mov [cs:synthetic_mouse_x1], ax
    mov bx, [cs:mouse_y0_index]
    shl bx, 1
    mov ax, [cs:mouse_y_values+bx]
    mov [cs:synthetic_mouse_y0], ax
    mov bx, [cs:mouse_y1_index]
    shl bx, 1
    mov ax, [cs:mouse_y_values+bx]
    mov [cs:synthetic_mouse_y1], ax
    mov ax, [cs:mouse_record_index]
    mov [cs:synthetic_mouse_y2], ax
    mov ax, [cs:mouse_button_index]
    mov [cs:synthetic_mouse_button], ax
    mov word [cs:mouse_x_calls], 0
    mov word [cs:mouse_y_calls], 0
    mov word [cs:mouse_set_x], 0xffff
    mov word [cs:mouse_set_y], 0xffff
    mov word [0x9602], 2
    call call_sample

    mov bx, [cs:mouse_buffer_cursor]
%macro mouse_record_word 1
    mov ax, [cs:%1]
    mov [cs:mouse_buffer+bx], ax
    add bx, 2
%endmacro
    mouse_record_word synthetic_mouse_x0
    mouse_record_word synthetic_mouse_x1
    mouse_record_word synthetic_mouse_y0
    mouse_record_word synthetic_mouse_y1
    mouse_record_word synthetic_mouse_y2
    mouse_record_word synthetic_mouse_button
    mov ax, [0x9600]
    mov [cs:mouse_buffer+bx], ax
    add bx, 2
    mov ax, [0x933c]
    mov [cs:mouse_buffer+bx], ax
    add bx, 2
    mov ax, [0x5488]
    mov [cs:mouse_buffer+bx], ax
    add bx, 2
    mouse_record_word mouse_set_x
    mouse_record_word mouse_set_y
%undef mouse_record_word
    add word [cs:mouse_buffer_cursor], 22
    inc word [cs:mouse_record_index]

    inc word [cs:mouse_button_index]
    cmp word [cs:mouse_button_index], 2
    jb .mouse_record
    mov word [cs:mouse_button_index], 0
    inc word [cs:mouse_y1_index]
    cmp word [cs:mouse_y1_index], MOUSE_VALUE_COUNT
    jb .mouse_record
    mov word [cs:mouse_y1_index], 0
    inc word [cs:mouse_y0_index]
    cmp word [cs:mouse_y0_index], MOUSE_VALUE_COUNT
    jb .mouse_record
    mov word [cs:mouse_y0_index], 0
    inc word [cs:mouse_x1_index]
    cmp word [cs:mouse_x1_index], MOUSE_VALUE_COUNT
    jb .mouse_record
    mov word [cs:mouse_x1_index], 0
    inc word [cs:mouse_x0_index]
    cmp word [cs:mouse_x0_index], MOUSE_VALUE_COUNT
    jb .mouse_record

    mov dx, mouse_name
    mov si, mouse_buffer
    mov cx, MOUSE_RECORD_BYTES
    call write_file
    jc input_failed
    mov ax, 0x4c00
    int 0x21

call_sample:
    db 0x9a
    dw 0x074c
sample_call_segment_relocation:
    dw 0
    ret

; Inputs: CS:DX name, CS:SI payload, CX byte count.  Returns carry on failure.
write_file:
    push ax
    push bx
    push dx
    push ds
    mov [cs:write_size], cx
    mov [cs:write_data], si
    push cs
    pop ds
    xor cx, cx
    mov ah, 0x3c
    int 0x21
    jc .error
    mov bx, ax
    mov dx, [write_data]
    mov cx, [write_size]
    mov ah, 0x40
    int 0x21
    jc .close_error
    cmp ax, [write_size]
    jne .close_error
    mov ah, 0x3e
    int 0x21
    pop ds
    pop dx
    pop bx
    pop ax
    clc
    ret
.close_error:
    mov ah, 0x3e
    int 0x21
.error:
    pop ds
    pop dx
    pop bx
    pop ax
    stc
    ret

input_failed:
    mov ax, 0x4c05
    int 0x21

; Replacements for the hardware-facing leaves.  Each returns through the
; original leaf's RET in the original code segment, preserving near-call ABI.
joystick_axis_hook:
    mov bx, sp
    cmp word [ss:bx+2], 1
    jne .y
    mov ax, [cs:synthetic_axis_x]
    jmp short .return
.y:
    mov ax, [cs:synthetic_axis_y]
.return:
    db 0xea
    dw 0x0671
axis_return_segment_relocation:
    dw 0

joystick_button_hook:
    mov ax, [cs:synthetic_button]
    db 0xea
    dw 0x068a
button_return_segment_relocation:
    dw 0

mouse_value_hook:
    mov bx, sp
    mov ax, [ss:bx+2]
    or ax, ax
    jz .x
    cmp ax, 1
    je .y
    mov ax, [cs:synthetic_mouse_button]
    jmp short .return
.x:
    mov bx, [cs:mouse_x_calls]
    inc word [cs:mouse_x_calls]
    or bx, bx
    jz .x0
    mov ax, [cs:synthetic_mouse_x1]
    jmp short .return
.x0:
    mov ax, [cs:synthetic_mouse_x0]
    jmp short .return
.y:
    mov bx, [cs:mouse_y_calls]
    inc word [cs:mouse_y_calls]
    or bx, bx
    jz .y0
    cmp bx, 1
    je .y1
    mov ax, [cs:synthetic_mouse_y2]
    jmp short .return
.y0:
    mov ax, [cs:synthetic_mouse_y0]
    jmp short .return
.y1:
    mov ax, [cs:synthetic_mouse_y1]
.return:
    db 0xea
    dw 0x0706
mouse_value_return_segment_relocation:
    dw 0

mouse_set_hook:
    mov bx, sp
    mov ax, [ss:bx+2]
    mov [cs:mouse_set_x], ax
    mov ax, [ss:bx+4]
    mov [cs:mouse_set_y], ax
    db 0xea
    dw 0x06b8
mouse_set_return_segment_relocation:
    dw 0

int6_handler:
    mov ax, 0x4c06
    int 0x21
    cli
    hlt

key_name db 'ORAKEY.BIN', 0
joy_name db 'ORAJOY.BIN', 0
mouse_name db 'ORAMOUSE.BIN', 0
axis_values dw 0, 1, 2, 3, 50, 80, 100, 149, 150
            dw 151, 169, 170, 171, 0x5555, 0x5556, 0x7fff, 0x8000, 0xffff
mouse_x_values dw 0x0095, 0x0096, 0x00aa, 0x00ab
mouse_y_values dw 0x000e, 0x000f, 0x00b9, 0x00ba

write_size dw 0
write_data dw 0
key_buffer_cursor dw 0
joy_buffer_cursor dw 0
joy_kind dw 0
joy_center_index dw 0
joy_value_index dw 0
synthetic_axis_x dw 0
synthetic_axis_y dw 0
synthetic_button dw 0
mouse_buffer_cursor dw 0
mouse_record_index dw 0
mouse_x0_index dw 0
mouse_x1_index dw 0
mouse_y0_index dw 0
mouse_y1_index dw 0
mouse_button_index dw 0
mouse_x_calls dw 0
mouse_y_calls dw 0
synthetic_mouse_x0 dw 0
synthetic_mouse_x1 dw 0
synthetic_mouse_y0 dw 0
synthetic_mouse_y1 dw 0
synthetic_mouse_y2 dw 0
synthetic_mouse_button dw 0
mouse_set_x dw 0
mouse_set_y dw 0

; The Borland CRT can retain only about 18 KiB beyond the original image after
; switching to DGROUP.  The harness writes each family before the next begins,
; so all three traces safely reuse one buffer sized for the largest family.
key_buffer:
joy_buffer equ key_buffer
mouse_buffer equ key_buffer
times JOY_RECORD_BYTES db 0

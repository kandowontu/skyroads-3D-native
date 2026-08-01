bits 16
org 0

%define GAMEPLAY_TICKS 1702

; Symbol table consumed by build_oracle_exe.py --palette.
dw state_hook
dw palette_hook
dw entry_hook
dw int6_handler
dw state_resume_segment_relocation
dw palette_resume_segment_relocation

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
    save_registers
    push ds
    push es

    xor ax, ax
    mov es, ax
    cli
    mov word [es:0x18], int6_handler
    push cs
    pop ax
    mov [es:0x1a], ax

    push cs
    pop ds
    mov dx, palette_name
    xor cx, cx
    mov ah, 0x3c
    int 0x21
    jc .failed
    mov [palette_handle], ax

    pop es
    pop ds
    restore_registers
    popf

    ; Displaced bytes from the MZ entry at 1000:60D0-60D4.
    mov di, 0x066e
    mov ds, di
    retf

.failed:
    mov ax, 0x4c05
    int 0x21

; Spliced over 1000:6233-6237. BX initially addresses the caller's near-return
; frame, so SS:BX+2/+4/+6 are the DAC base, color count, and far source.
palette_hook:
    mov bx, sp
    push ds
    push es
    pusha
    pushf
    cli

    mov ax, [ss:bx+2]
    mov [cs:record_header], ax
    mov ax, [ss:bx+4]
    mov [cs:record_header+2], ax
    mov [cs:payload_colors], ax
    mov ax, [ss:bx+6]
    mov [cs:payload_pointer], ax
    mov ax, [ss:bx+8]
    mov [cs:payload_pointer+2], ax

    push cs
    pop ds
    mov bx, [palette_handle]
    mov dx, record_header
    mov cx, 4
    mov ah, 0x40
    int 0x21
    jc palette_failed
    cmp ax, 4
    jne palette_failed

    mov cx, [payload_colors]
    mov ax, cx
    shl cx, 1
    add cx, ax
    jcxz .recorded
    lds dx, [cs:payload_pointer]
    mov bx, [cs:palette_handle]
    mov ah, 0x40
    int 0x21
    jc palette_failed
    mov cx, [cs:payload_colors]
    mov dx, cx
    shl cx, 1
    add cx, dx
    cmp ax, cx
    jne palette_failed

.recorded:
    inc word [cs:palette_count]
    popf
    popa
    pop es
    pop ds

    ; Recreate the five displaced bytes, leaving the register-save frame in
    ; exactly the form expected by the original body at 1000:6238.
    mov bx, sp
    push ds
    push es
    pusha
    db 0xea
    dw 0x6238
palette_resume_segment_relocation:
    dw 0

palette_failed:
    mov bx, [cs:palette_handle]
    mov ah, 0x3e
    int 0x21
    mov ax, 0x4c05
    int 0x21

; The palette run keeps the original intro, then terminates after the same
; complete 1,702-tick demo interval used by the state/framebuffer oracles.
state_hook:
    pushf
    save_registers
    push ds
    inc word [cs:gameplay_count]
    cmp word [cs:gameplay_count], GAMEPLAY_TICKS
    jb .restore
    push cs
    pop ds
    mov bx, [palette_handle]
    mov ah, 0x3e
    int 0x21
    mov word [palette_handle], 0xffff
    mov ax, 0x4c00
    int 0x21

.restore:
    pop ds
    restore_registers
    popf
    add word [bp-2], 1
    mov ax, [bp-2]
    mov [0x160c], ax
    db 0xea
    dw 0x22a3
state_resume_segment_relocation:
    dw 0

int6_handler:
    mov bx, [cs:palette_handle]
    cmp bx, 0xffff
    je .terminate
    mov ah, 0x3e
    int 0x21
.terminate:
    mov ax, 0x4c06
    int 0x21
    cli
    hlt

palette_name db 'ORAPAL.BIN', 0
palette_handle dw 0xffff
palette_count dw 0
gameplay_count dw 0
record_header times 4 db 0
payload_colors dw 0
payload_pointer times 4 db 0

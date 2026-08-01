bits 16
org 0

%define FRAME_LIMIT 1702
%define FRAME_BYTES 0xfa00
%define FRAME_COUNT FRAME_LIMIT

; Symbol table consumed by build_oracle_exe.py --frame.
dw state_hook
dw entry_hook
dw int6_handler
dw state_resume_segment_relocation
dw transfer_hook
dw transfer_resume_segment_relocation

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

int6_handler:
    push bp
    mov bp, sp
    save_registers
    push ds
    push cs
    pop ds
    mov ax, [ss:bp+2]
    mov [fault_record], ax
    mov ax, [ss:bp+4]
    mov [fault_record+2], ax
    mov ax, [ss:bp+6]
    mov [fault_record+4], ax
    mov dx, fault_name
    xor cx, cx
    mov ah, 0x3c
    int 0x21
    jc .terminate
    mov bx, ax
    mov dx, fault_record
    mov cx, 6
    mov ah, 0x40
    int 0x21
    mov ah, 0x3e
    int 0x21
.terminate:
    mov ax, 0x4c06
    int 0x21
    cli
    hlt

state_hook:
    pushf
    save_registers
    push ds
    push es
    cld

    cmp byte [cs:trek_dumped], 0
    jne .select_frame
    call dump_trek_record_one
    jc .failed

.select_frame:
    cmp word [cs:gameplay_tick], 1528
    jne .select_transfer
    cmp byte [cs:trek_all_dumped], 0
    jne .select_transfer
    call dump_trek_all
    jc .failed
.select_transfer:
    cmp word [cs:gameplay_tick], 30
    jne .select_post
    cmp byte [cs:transfer_dumped], 0
    jne .select_post
    call dump_transfer_trace
    jc .failed

.select_post:
    cmp word [cs:gameplay_tick], 15
    jne .select_meta_221
    cmp byte [cs:trek_post_dumped], 0
    jne .select_capture
    call dump_trek_record_one_post
    jc .failed

.select_meta_221:
    cmp word [cs:gameplay_tick], 221
    jne .select_capture
    cmp byte [cs:meta_221_dumped], 0
    jne .select_capture
    call dump_renderer_meta_221
    jc .failed

.select_capture:
    mov bx, [cs:next_frame]
    cmp bx, FRAME_COUNT
    jae .advance
    shl bx, 1
    mov ax, [cs:capture_ticks+bx]
    cmp ax, [cs:gameplay_tick]
    jne .advance
    call write_frame
    jc .failed
    inc word [cs:next_frame]

.advance:
    inc word [cs:gameplay_tick]
    cmp word [cs:gameplay_tick], FRAME_LIMIT
    jb .restore
    call close_frame_file
    mov ax, 0x4c00
    int 0x21

.failed:
    call close_frame_file
    mov ax, 0x4c05
    int 0x21

.restore:
    pop es
    pop ds
    restore_registers
    popf

    ; Displaced gameplay-loop clock update at 1000:2ADD.  Resume at the
    ; original outer loop so the frame oracle renders the next state instead
    ; of taking the state oracle's render-skipping fast path at 1000:22A3.
    ; The overwritten probe range extends through 1000:2AE6, so the intact
    ; destination is the branch target at 1000:2AEC rather than 1000:2AE1.
    add word [bp-2], 1
    mov ax, [bp-2]
    mov [0x160c], ax
    db 0xea
    dw 0x2aec
state_resume_segment_relocation:
    dw 0

transfer_hook:
    mov [cs:transfer_ax], ax
    pushf
    save_registers
    push ds
    push es
    cmp word [ss:0x0e36], 28
    jne .restore
    mov bx, [cs:transfer_count]
    cmp bx, 64
    jae .restore
    shl bx, 1
    shl bx, 1
    shl bx, 1
    mov ax, [cs:transfer_ax]
    mov [cs:transfer_records+bx], ax
    mov ax, [ss:0x0e50]
    cmp word [ss:0x0e4e], 0x36d7
    jne .store_depth
    or ax, 0x8000
.store_depth:
    mov [cs:transfer_records+bx+2], ax
    mov ax, [ss:0x0e52]
    mov [cs:transfer_records+bx+4], ax
    mov ax, [ss:0x0e54]
    mov [cs:transfer_records+bx+6], ax
    inc word [cs:transfer_count]
.restore:
    pop es
    pop ds
    restore_registers
    popf

    ; Displaced entry instructions from 1000:38A3-38A8.
    push bx
    push bp
    push ds
    mov [0x0e80], ax
    db 0xea
    dw 0x38a9
transfer_resume_segment_relocation:
    dw 0

write_frame:
    push ds
    mov ax, [0x5486]
    mov [cs:work_segment], ax
    cmp word [cs:frame_handle], 0xffff
    jne .open_work
    push cs
    pop ds
    mov dx, frame_name
    xor cx, cx
    mov ah, 0x3c
    int 0x21
    jc .error
    mov [cs:frame_handle], ax
.open_work:
    cmp word [cs:work_handle], 0xffff
    jne .write_frame
    push cs
    pop ds
    mov dx, work_name
    xor cx, cx
    mov ah, 0x3c
    int 0x21
    jc .error
    mov [cs:work_handle], ax
.write_frame:
    mov bx, [cs:frame_handle]
    mov ax, 0xa000
    mov ds, ax
    xor dx, dx
    mov cx, FRAME_BYTES
    mov ah, 0x40
    int 0x21
    jc .error
    cmp ax, FRAME_BYTES
    jne .error
    mov bx, [cs:work_handle]
    mov ax, [cs:work_segment]
    mov ds, ax
    xor dx, dx
    mov cx, FRAME_BYTES
    mov ah, 0x40
    int 0x21
    jc .error
    cmp ax, FRAME_BYTES
    jne .error
    pop ds
    clc
    ret
.error:
    pop ds
    stc
    ret

dump_trek_record_one:
    push ds
    mov ax, [0x0e84]
    mov [cs:trek_segment], ax
    push cs
    pop ds
    mov dx, trek_name
    xor cx, cx
    mov ah, 0x3c
    int 0x21
    jc .error
    mov bx, ax
    mov ax, [cs:trek_segment]
    mov ds, ax
    xor dx, dx
    mov cx, 0x64af
    mov ah, 0x40
    int 0x21
    jc .close_error
    cmp ax, 0x64af
    jne .close_error
    mov ah, 0x3e
    int 0x21
    mov byte [cs:trek_dumped], 1
    pop ds
    clc
    ret
.close_error:
    mov ah, 0x3e
    int 0x21
.error:
    pop ds
    stc
    ret

dump_trek_record_one_post:
    push ds
    mov ax, [0x0e36]
    mov [cs:renderer_meta], ax
    mov ax, [0x0e76]
    mov [cs:renderer_meta+2], ax
    mov ax, [0x0e28]
    mov [cs:renderer_meta+4], ax
    mov ax, [0x0e2a]
    mov [cs:renderer_meta+6], ax
    mov ax, [0x9628]
    mov [cs:renderer_meta+8], ax
    mov ax, [0x962a]
    mov [cs:renderer_meta+10], ax
    mov ax, [0x0e34]
    mov [cs:renderer_meta+12], ax
    mov ax, [0x0e38]
    mov [cs:renderer_meta+14], ax
    mov ax, [0x0e40]
    mov [cs:renderer_meta+16], ax
    mov ax, [0x0e30]
    mov [cs:renderer_meta+18], ax
    mov ax, [0x0e2e]
    mov [cs:renderer_meta+20], ax
    mov ax, [0x0e3a]
    mov [cs:renderer_meta+22], ax
    mov ax, [0x0e3c]
    mov [cs:renderer_meta+24], ax
    mov ax, [0xaf3c]
    mov [cs:renderer_meta+26], ax
    mov ax, [0x9342]
    mov [cs:renderer_meta+28], ax
    mov ax, [0x160c]
    mov [cs:renderer_meta+30], ax
    mov ax, [0x0e84]
    mov [cs:trek_segment], ax
    push cs
    pop ds
    mov dx, trek_post_name
    xor cx, cx
    mov ah, 0x3c
    int 0x21
    jc .error
    mov bx, ax
    mov ax, [cs:trek_segment]
    mov ds, ax
    xor dx, dx
    mov cx, 0x64af
    mov ah, 0x40
    int 0x21
    jc .close_error
    cmp ax, 0x64af
    jne .close_error
    mov ah, 0x3e
    int 0x21
    push cs
    pop ds
    mov dx, meta_name
    xor cx, cx
    mov ah, 0x3c
    int 0x21
    jc .error
    mov bx, ax
    mov dx, renderer_meta
    mov cx, 32
    mov ah, 0x40
    int 0x21
    jc .close_error
    cmp ax, 32
    jne .close_error
    mov ah, 0x3e
    int 0x21
    mov byte [cs:trek_post_dumped], 1
    pop ds
    clc
    ret
.close_error:
    mov ah, 0x3e
    int 0x21
.error:
    pop ds
    stc
    ret

dump_renderer_meta_221:
    push ds
    mov ax, [ss:0x0e36]
    mov [cs:renderer_meta], ax
    mov ax, [ss:0x0e76]
    mov [cs:renderer_meta+2], ax
    mov ax, [ss:0x0e28]
    mov [cs:renderer_meta+4], ax
    mov ax, [ss:0x0e2a]
    mov [cs:renderer_meta+6], ax
    mov ax, [ss:0x9628]
    mov [cs:renderer_meta+8], ax
    mov ax, [ss:0x962a]
    mov [cs:renderer_meta+10], ax
    mov ax, [ss:0x0e34]
    mov [cs:renderer_meta+12], ax
    mov ax, [ss:0x0e38]
    mov [cs:renderer_meta+14], ax
    mov ax, [ss:0x0e40]
    mov [cs:renderer_meta+16], ax
    mov ax, [ss:0x0e30]
    mov [cs:renderer_meta+18], ax
    mov ax, [ss:0x0e2e]
    mov [cs:renderer_meta+20], ax
    mov ax, [ss:0x0e3a]
    mov [cs:renderer_meta+22], ax
    mov ax, [ss:0x0e3c]
    mov [cs:renderer_meta+24], ax
    mov ax, [ss:0xaf3c]
    mov [cs:renderer_meta+26], ax
    mov ax, [ss:0x9342]
    mov [cs:renderer_meta+28], ax
    mov ax, [ss:0x160c]
    mov [cs:renderer_meta+30], ax
    push cs
    pop ds
    mov dx, meta_221_name
    xor cx, cx
    mov ah, 0x3c
    int 0x21
    jc .error
    mov bx, ax
    mov dx, renderer_meta
    mov cx, 32
    mov ah, 0x40
    int 0x21
    jc .close_error
    cmp ax, 32
    jne .close_error
    mov ah, 0x3e
    int 0x21
    push cs
    pop ds
    mov dx, mask_221_name
    xor cx, cx
    mov ah, 0x3c
    int 0x21
    jc .error
    mov bx, ax
    mov ax, ss
    mov ds, ax
    mov dx, 0x0e92
    mov cx, 957
    mov ah, 0x40
    int 0x21
    jc .close_error
    cmp ax, 957
    jne .close_error
    mov ah, 0x3e
    int 0x21
    push cs
    pop ds
    mov dx, car_221_name
    xor cx, cx
    mov ah, 0x3c
    int 0x21
    jc .error
    mov bx, ax
    mov ax, [cs:renderer_meta+24]
    mov ds, ax
    mov dx, [cs:renderer_meta+22]
    mov cx, 720
    mov ah, 0x40
    int 0x21
    jc .close_error
    cmp ax, 720
    jne .close_error
    mov ah, 0x3e
    int 0x21
    mov byte [cs:meta_221_dumped], 1
    pop ds
    clc
    ret
.close_error:
    mov ah, 0x3e
    int 0x21
.error:
    pop ds
    stc
    ret

dump_transfer_trace:
    push ds
    push cs
    pop ds
    mov dx, transfer_name
    xor cx, cx
    mov ah, 0x3c
    int 0x21
    jc .error
    mov bx, ax
    mov dx, transfer_records
    mov cx, [transfer_count]
    shl cx, 1
    shl cx, 1
    shl cx, 1
    mov ah, 0x40
    int 0x21
    jc .close_error
    mov ah, 0x3e
    int 0x21
    mov byte [cs:transfer_dumped], 1
    pop ds
    clc
    ret
.close_error:
    mov ah, 0x3e
    int 0x21
.error:
    pop ds
    stc
    ret

dump_trek_all:
    push ds
    push si
    push cs
    pop ds
    mov dx, trek_all_name
    xor cx, cx
    mov ah, 0x3c
    int 0x21
    jc .error
    mov [cs:trek_all_handle], ax
    xor si, si
.record:
    mov ax, [ss:si+0x0e82]
    mov ds, ax
    xor dx, dx
    mov cx, [cs:trek_sizes+si]
    mov bx, [cs:trek_all_handle]
    mov ah, 0x40
    int 0x21
    jc .close_error
    cmp ax, [cs:trek_sizes+si]
    jne .close_error
    add si, 2
    cmp si, 16
    jb .record
    mov bx, [cs:trek_all_handle]
    mov ah, 0x3e
    int 0x21
    mov byte [cs:trek_all_dumped], 1
    pop si
    pop ds
    clc
    ret
.close_error:
    mov bx, [cs:trek_all_handle]
    mov ah, 0x3e
    int 0x21
.error:
    pop si
    pop ds
    stc
    ret

close_frame_file:
    mov bx, [cs:frame_handle]
    cmp bx, 0xffff
    je .work
    mov ah, 0x3e
    int 0x21
    mov word [cs:frame_handle], 0xffff
.work:
    mov bx, [cs:work_handle]
    cmp bx, 0xffff
    je .done
    mov ah, 0x3e
    int 0x21
    mov word [cs:work_handle], 0xffff
.done:
    ret

frame_name db 'ORAFRAME.BIN', 0
work_name db 'ORAWORK.BIN', 0
trek_name db 'ORATREK1.BIN', 0
trek_post_name db 'ORATRK1P.BIN', 0
meta_name db 'ORAMETA.BIN', 0
meta_221_name db 'ORAM221.BIN', 0
mask_221_name db 'ORAMSK21.BIN', 0
car_221_name db 'ORACAR43.BIN', 0
transfer_name db 'ORAXFER.BIN', 0
trek_all_name db 'ORATREKS.BIN', 0
fault_name db 'ORAFAULT.BIN', 0
trek_sizes dw 24716, 25775, 26324, 26702, 27278, 26780, 26399, 26153
capture_ticks:
%assign capture_tick 0
%rep FRAME_LIMIT
    dw capture_tick
%assign capture_tick capture_tick + 1
%endrep
gameplay_tick dw 0
next_frame dw 0
frame_handle dw 0xffff
work_handle dw 0xffff
work_segment dw 0
trek_segment dw 0
trek_dumped db 0
trek_post_dumped db 0
meta_221_dumped db 0
transfer_dumped db 0
trek_all_dumped db 0
trek_all_handle dw 0xffff
transfer_ax dw 0
transfer_count dw 0
transfer_records times 64 * 8 db 0
renderer_meta times 32 db 0
fault_record times 6 db 0

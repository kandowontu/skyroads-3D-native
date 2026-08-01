bits 16
org 0

%ifndef TRACE_STATE_LIMIT
%define TRACE_STATE_LIMIT 1702
%endif

%define TRACE_MAGIC          0x0e92
%define TRACE_STATE_HANDLE   0x0e94
%define TRACE_STATE_COUNT    0x0e96
%define TRACE_BLOCK_COUNT    0x0e98
%define TRACE_SOUND_COUNT    0x0e9a
%define TRACE_STATE_BLOCK    0x0e9c
%define TRACE_SOUND_RECORDS  0x115c

; The first two words form a tiny symbol table consumed by build_oracle_exe.py.
dw state_hook
dw sound_hook
dw entry_hook
dw int6_handler
dw state_resume_segment_relocation
dw sound_disabled_segment_relocation
dw sound_enabled_segment_relocation
dw opl_tick_segment_relocation_1
dw opl_tick_segment_relocation_2
dw opl_tick_segment_relocation_3
dw opl_tick_segment_relocation_4
dw opl_tick_segment_relocation_5

%macro store_ss_word 1
    mov ax, [ss:%1]
    stosw
%endmacro

%macro store_local_word 1
    mov ax, [ss:bp-%1]
    stosw
%endmacro

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
    push es
    cld
    push cs
    pop es
    mov di, fault_record
    mov ax, [ss:bp+2]
    stosw
    mov ax, [ss:bp+4]
    stosw
    mov ax, [ss:bp+6]
    stosw

    ; Snapshot the live bytes surrounding the reported IP and the interrupt
    ; stack so a failed splice can be diagnosed without debugger interaction.
    mov si, [ss:bp+2]
    sub si, 8
    mov ax, [ss:bp+4]
    mov ds, ax
    push cs
    pop es
    mov di, fault_code
    mov cx, 32
    rep movsb
    push ss
    pop ds
    mov si, bp
    push cs
    pop es
    mov di, fault_stack
    mov cx, 32
    rep movsb
    push cs
    pop ds
    mov dx, fault_name
    xor cx, cx
    mov ah, 0x3c
    int 0x21
    jc .terminate
    mov bx, ax
    mov dx, fault_record
    mov cx, 70
    mov ah, 0x40
    int 0x21
    mov ah, 0x3e
    int 0x21
.terminate:
    mov ax, 0x4c06
    int 0x21
    cli
    hlt

state_name db 'ORASTATE.BIN', 0
sound_name db 'ORASFX.BIN', 0
fault_name db 'ORAFAULT.BIN', 0

ensure_trace_state:
    cmp word [ss:TRACE_MAGIC], 0x534b
    je .ready
    mov word [ss:TRACE_MAGIC], 0x534b
    mov word [ss:TRACE_STATE_HANDLE], 0xffff
    mov word [ss:TRACE_STATE_COUNT], 0
    mov word [ss:TRACE_BLOCK_COUNT], 0
    mov word [ss:TRACE_SOUND_COUNT], 0
.ready:
    ret

state_hook:
%ifdef PASSTHROUGH_STATE
    jmp state_dispatch
%endif
    pushf
    save_registers
    push ds
    push es
    cld
    call ensure_trace_state

    ; The original timer invokes the OPL scheduler at 180 Hz and advances the
    ; gameplay clock twice in each ten-interrupt phase cycle.  The oracle's
    ; presentation-free gameplay loop advances at 36 Hz, so drive the original
    ; scheduler five times per captured gameplay tick.  Its ISR call site is
    ; disabled in the trace copy, making this independent of host wall time.
    db 0x9a
    dw 0x5a39
opl_tick_segment_relocation_1:
    dw 0
    db 0x9a
    dw 0x5a39
opl_tick_segment_relocation_2:
    dw 0
    db 0x9a
    dw 0x5a39
opl_tick_segment_relocation_3:
    dw 0
    db 0x9a
    dw 0x5a39
opl_tick_segment_relocation_4:
    dw 0
    db 0x9a
    dw 0x5a39
opl_tick_segment_relocation_5:
    dw 0

    cmp word [ss:TRACE_STATE_COUNT], TRACE_STATE_LIMIT
    jb .capture
    jmp .restore

.capture:

    push cs
    pop es
    mov di, state_record
    store_ss_word 0x9628        ; distance low
    store_ss_word 0x962a        ; distance high
    store_ss_word 0xaf2c        ; horizontal position
    store_ss_word 0xaf3c        ; height
    store_ss_word 0x54b8        ; forward speed low
    store_ss_word 0x54ba        ; forward speed high
    store_ss_word 0x4576        ; lateral velocity
    store_ss_word 0x9342        ; vertical velocity
    store_ss_word 0x54a2        ; surface impulse
    store_ss_word 0x54a0        ; fuel
    store_ss_word 0xb14c        ; oxygen
    store_ss_word 0x457c        ; level result
    store_ss_word 0x4566        ; result delay ticks
    store_ss_word 0x4578        ; result ticks
    store_ss_word 0x4568        ; collision adjusted
    store_ss_word 0xaf3e        ; collision speed correction low
    store_ss_word 0xaf40        ; collision speed correction high
    store_local_word 0x0c       ; process surface cell
    store_local_word 0x0e       ; on kind 8
    store_local_word 0x10       ; on kind 2
    store_local_word 0x08       ; jumping
    store_local_word 0x06       ; prediction already run

    ; Buffer 16 records in the probe and amortize DOS I/O outside the hot loop.
    mov ax, [ss:TRACE_BLOCK_COUNT]
    mov cx, 44
    mul cx
    mov di, TRACE_STATE_BLOCK
    add di, ax
    push ss
    pop es
    push cs
    pop ds
    mov si, state_record
    mov cx, 22
    rep movsw

    inc word [ss:TRACE_STATE_COUNT]
    inc word [ss:TRACE_BLOCK_COUNT]
    cmp word [ss:TRACE_BLOCK_COUNT], 16
    jne .check_complete
    call flush_state_block

.check_complete:
    cmp word [ss:TRACE_STATE_COUNT], TRACE_STATE_LIMIT
    jne .restore
    cmp word [ss:TRACE_BLOCK_COUNT], 0
    je .finish
    call flush_state_block
.finish:
    call finish_trace_files
    ; This is a generated trace executable, not the shipped game.  Terminate at
    ; the requested oracle boundary so external capture backends can flush
    ; their recordings without killing DOSBox-X mid-stream.
    mov ax, 0x4c00
    int 0x21
    jmp short .restore

.disable:
    mov word [ss:TRACE_STATE_COUNT], TRACE_STATE_LIMIT

.restore:
    pop es
    pop ds
    restore_registers
    popf

state_dispatch:
    add word [bp-2], 1
    mov ax, [bp-2]
    mov [0x160c], ax
    db 0xea
    dw 0x22a3
state_resume_segment_relocation:
    dw 0

sound_hook:
    pushf
    save_registers
    push ds
    push es
    cld
    call ensure_trace_state

    cmp word [ss:TRACE_STATE_COUNT], TRACE_STATE_LIMIT
    jae .restore
    cmp word [ss:0x9602], 3
    jne .restore

    mov si, [ss:TRACE_SOUND_COUNT]
    cmp si, 120
    jae .restore
    shl si, 1
    mov ax, [ss:bp+4]
    mov [ss:TRACE_SOUND_RECORDS+si], ax
    inc word [ss:TRACE_SOUND_COUNT]

.restore:
    pop es
    pop ds
    restore_registers
    popf
    mov ax, [0x160c]
    mov [0xaf48], ax
    cmp word [0x4528], 0
    je sound_return_enabled
    db 0xea
    dw 0x0472
sound_disabled_segment_relocation:
    dw 0
sound_return_enabled:
    db 0xea
    dw 0x03d8
sound_enabled_segment_relocation:
    dw 0

flush_state_block:
    push cs
    pop ds
    cmp word [ss:TRACE_STATE_HANDLE], 0xffff
    jne .write
    mov dx, state_name
    xor cx, cx
    mov ah, 0x3c
    int 0x21
    jc .failed
    mov [ss:TRACE_STATE_HANDLE], ax
.write:
    mov bx, [ss:TRACE_STATE_HANDLE]
    mov ax, [ss:TRACE_BLOCK_COUNT]
    mov cx, 44
    mul cx
    mov cx, ax
    push ss
    pop ds
    mov dx, TRACE_STATE_BLOCK
    mov ah, 0x40
    int 0x21
    jc .failed
    mov word [ss:TRACE_BLOCK_COUNT], 0
    ret
.failed:
    mov word [ss:TRACE_STATE_COUNT], TRACE_STATE_LIMIT
    ret

finish_trace_files:
    mov bx, [ss:TRACE_STATE_HANDLE]
    cmp bx, 0xffff
    je .sound
    mov ah, 0x3e
    int 0x21
.sound:
    push cs
    pop ds
    mov dx, sound_name
    xor cx, cx
    mov ah, 0x3c
    int 0x21
    jc .done
    mov bx, ax
    push ss
    pop ds
    mov dx, TRACE_SOUND_RECORDS
    mov cx, [ss:TRACE_SOUND_COUNT]
    shl cx, 1
    mov ah, 0x40
    int 0x21
    mov ah, 0x3e
    int 0x21
.done:
    ret

fault_record times 6 db 0
fault_code times 32 db 0
fault_stack times 32 db 0
state_record times 44 db 0

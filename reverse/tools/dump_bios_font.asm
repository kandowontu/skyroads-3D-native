bits 16
org 0x100

start:
    mov ax, 0x1130
    mov bh, 3
    int 0x10
    push es
    push bp

    push cs
    pop ds
    mov dx, filename
    xor cx, cx
    mov ah, 0x3c
    int 0x21
    jc failed
    mov bx, ax

    pop dx
    pop ds
    mov cx, 1024
    mov ah, 0x40
    int 0x21
    jc failed_open

    mov ah, 0x3e
    int 0x21
    mov ax, 0x4c00
    int 0x21

failed_open:
    mov ah, 0x3e
    int 0x21
failed:
    mov ax, 0x4c01
    int 0x21

filename db 'FONT8X8.BIN', 0

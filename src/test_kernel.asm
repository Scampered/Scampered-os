[BITS 16]
[ORG 0x8000]

start:
    cli
    mov ax, 0x0000
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00     ; simple emergency stack
    sti


    ; Print to VGA (BIOS teletype) and to Serial (COM1) so we can always see output
    mov si, msg_start
    call print_string_vga
    mov si, msg_start
    call print_string_serial

    ; --- Disk read test via INT 0x40 ---
    ; Read 1 sector at LBA=200 into ES:DI = 0x0000:0x9000
    mov ah, 0x00      ; our INT40 read function code (stage2's convention)
    mov bx, 1         ; sectors to read
    mov cx, 200       ; LBA
    mov di, 0x9000
    ; set ES safely
    mov ax, 0x0000
    mov es, ax
    int 0x40
    jc disk_err

    ; show buffer we just read (ES:DI was destination) -> print via VGA + serial
    push ds
    mov ax, es
    mov ds, ax
    mov si, 0x9000
    call print_string_vga
    mov si, 0x9000
    call print_string_serial
    pop ds

hang:
    hlt
    jmp hang

disk_err:
    mov si, err_msg
    call print_string_vga
    mov si, err_msg
    call print_string_serial
    jmp hang

; -----------------------
; VGA print (DS:SI -> BIOS int 10 teletype)
; preserves registers except AX
; -----------------------
print_string_vga:
    push ax
    push bx
    push cx
    push dx
    push si
.vga_next:
    lodsb
    or al, al
    jz .vga_done
    mov ah, 0x0E
    int 0x10
    jmp .vga_next
.vga_done:
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

; -----------------------
; Serial routines (COM1 = 0x3F8)
; Very small blocking putchar and string print.
; -----------------------

; send character in AL to COM1 (polling)
serial_putchar:
    push dx
.wait_thr:
    mov dx, 0x3FD   ; check Line Status at 0x3FD (3F8+5). Using 3FD for NASM friendly.
    in  al, dx
    test al, 0x20   ; Transmitter Holding Register Empty
    jz  .wait_thr
    pop dx
    mov dx, 0x3F8
    out dx, al      ; NOTE: out dx, al writes AL to port dx; but we must put char in AL first
    ret

; print string at DS:SI to serial
print_string_serial:
    push ax
    push bx
    push cx
    push dx
    push si
.ser_next:
    lodsb
    or al, al
    jz .ser_done
    ; AL=char, call serial write (we'll implement wrapper)
    push ax
    mov al, [si-1]  ; char we just loaded (lodsb already advanced SI so char is at SI-1)
    call serial_out_char
    pop ax
    jmp .ser_next
.ser_done:
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

; serial_out_char: expects AL with char
serial_out_char:
    push dx
.spin:
    mov dx, 0x3FD
    in  al, dx
    test al, 0x20
    jz .spin
    mov dx, 0x3F8
    out dx, al
    ret

; -----------------------
; Data
; -----------------------
msg_start db 'Kernel loaded. Testing INT 40h...', 0
err_msg  db 'Disk I/O error', 0

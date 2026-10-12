; 88_sio.asm
; Serial io board - 88-SIO
; Github: https://github.com/tommojphillips
;
; Assembly rewrite of src/88_sio.c
;
; 32-bit cdecl calling convention (Windows):
;   SIO* passed as first argument ([ebp+8]).
;   uint8_t/char arguments are promoted to 32-bit ints on the stack.
;   uint8_t* / int return values in EAX.
;
; SIO struct layout (src/88_sio.h):
;   +0  uint8_t status
;   +1  uint8_t control
;   +2  uint8_t output_interrupt
;   +3  uint8_t input_interrupt
;   +4  char    ch

BITS 32

global _sio_reset
global _sio_update
global _sio_read_io
global _sio_write_io
global _sio_status
global _sio_read
global _sio_write
global _sio_control

extern _printf
extern __kbhit
extern __getch

; --- Status byte bit masks ---
%define SIO_OUTPUT_DEVICE_READY  0x80  ; ACTIVE LOW  - output device is ready
%define SIO_DATA_AVAILABLE       0x20  ; ACTIVE HIGH - data Available
%define SIO_DATA_OVERFLOW        0x10  ; ACTIVE HIGH - data Overflow
%define SIO_DATA_EMPTY           0x02  ; ACTIVE HIGH - data buffer is empty
%define SIO_INPUT_DEVICE_READY   0x01  ; ACTIVE LOW  - input device is ready

; --- Port addresses ---
%define PORT_SIO_STATUS          0x10  ; status port (in)
%define PORT_SIO_CONTROL         0x10  ; status port (out)
%define PORT_SIO_DATA            0x11  ; data port (in/out)
%define PORT_SIO_DATA1           0x01  ; data port (in/out)

; --- SIO struct field offsets ---
%define SIO_OFF_STATUS           0
%define SIO_OFF_CONTROL          1
%define SIO_OFF_OUTPUT_INT       2
%define SIO_OFF_INPUT_INT        3
%define SIO_OFF_CH               4

section .rodata
    sio_fmt_backspace db 8, 0
    sio_fmt_char      db "%c", 0

section .text

; void sio_reset(SIO* sio)
_sio_reset:
    mov  eax, [esp+4]                       ; eax = sio
    mov  byte [eax+SIO_OFF_STATUS], SIO_DATA_EMPTY
    ret

; void sio_update(SIO* sio)
_sio_update:
    push ebx
    mov  ebx, [esp+8]                       ; ebx = sio

    call __kbhit
    test eax, eax
    jz   .no_key

    ; if (sio->ch != 0) status |= OVERFLOW; else status &= ~OVERFLOW;
    cmp  byte [ebx+SIO_OFF_CH], 0
    je   .no_overflow
    or   byte [ebx+SIO_OFF_STATUS], SIO_DATA_OVERFLOW
    jmp  .store_ch
.no_overflow:
    and  byte [ebx+SIO_OFF_STATUS], ~SIO_DATA_OVERFLOW
.store_ch:
    call __getch
    mov  [ebx+SIO_OFF_CH], al               ; sio->ch = (char)_getch()

    or   byte [ebx+SIO_OFF_STATUS], SIO_INPUT_DEVICE_READY
    and  byte [ebx+SIO_OFF_STATUS], ~SIO_DATA_EMPTY
    or   byte [ebx+SIO_OFF_STATUS], SIO_DATA_AVAILABLE
    jmp  .done
.no_key:
    and  byte [ebx+SIO_OFF_STATUS], ~SIO_INPUT_DEVICE_READY
.done:
    pop  ebx
    ret

; int sio_read_io(SIO* sio, uint8_t port, uint8_t* value)
_sio_read_io:
    push ebx
    mov  ebx, [esp+8]                       ; ebx = sio
    movzx eax, byte [esp+12]                ; eax = port

    cmp  eax, PORT_SIO_STATUS
    je   .status
    cmp  eax, PORT_SIO_DATA
    je   .data
    xor  eax, eax                           ; default: return 0
    pop  ebx
    ret

.status:
    mov  al, [ebx+SIO_OFF_STATUS]
    mov  ecx, [esp+16]                      ; value*
    mov  [ecx], al
    jmp  .ok
.data:
    push ebx                                ; arg: sio
    call _sio_read
    add  esp, 4
    mov  ecx, [esp+16]                      ; value*
    mov  [ecx], al
.ok:
    mov  eax, 1
    pop  ebx
    ret

; int sio_write_io(SIO* sio, uint8_t port, uint8_t value)
_sio_write_io:
    push ebx
    mov  ebx, [esp+8]                       ; ebx = sio
    movzx eax, byte [esp+12]                ; eax = port

    cmp  eax, PORT_SIO_CONTROL
    je   .control
    cmp  eax, PORT_SIO_DATA
    je   .data
    cmp  eax, PORT_SIO_DATA1
    je   .data
    xor  eax, eax                           ; default: return 0
    pop  ebx
    ret

.control:
    movzx ecx, byte [esp+16]                ; ecx = value
    mov  eax, ecx
    and  eax, 0x03
    mov  [ebx+SIO_OFF_CONTROL], al          ; control = value & 0x03
    mov  eax, ecx
    and  eax, 0x01
    mov  [ebx+SIO_OFF_INPUT_INT], al        ; input_interrupt = value & 0x01
    mov  eax, ecx
    and  eax, 0x02
    shr  eax, 1
    mov  [ebx+SIO_OFF_OUTPUT_INT], al       ; output_interrupt = (value & 0x02) >> 1
    jmp  .ok
.data:
    push dword [esp+16]                     ; arg: value
    call _sio_write
    add  esp, 4
.ok:
    mov  eax, 1
    pop  ebx
    ret

; uint8_t sio_status(SIO* sio)
_sio_status:
    mov  eax, [esp+4]                       ; eax = sio
    movzx eax, byte [eax+SIO_OFF_STATUS]
    ret

; uint8_t sio_read(SIO* sio)
_sio_read:
    mov  edx, [esp+4]                       ; edx = sio
    movzx eax, byte [edx+SIO_OFF_CH]        ; eax = ch
    mov  byte [edx+SIO_OFF_CH], 0           ; sio->ch = 0

    ; if (ch >= 'a' && ch <= 'z') ch -= 0x20;
    cmp  eax, 'a'
    jb   .no_upper
    cmp  eax, 'z'
    ja   .no_upper
    sub  eax, 0x20
.no_upper:
    ; sio->status &= ~INPUT_DEVICE_READY;
    and  byte [edx+SIO_OFF_STATUS], ~SIO_INPUT_DEVICE_READY
    ; sio->status |= DATA_EMPTY;
    or   byte [edx+SIO_OFF_STATUS], SIO_DATA_EMPTY
    ; sio->status &= ~DATA_AVAILABLE;
    and  byte [edx+SIO_OFF_STATUS], ~SIO_DATA_AVAILABLE
    ret

; void sio_write(char ch)
_sio_write:
    movzx eax, byte [esp+4]                 ; eax = ch
    cmp  eax, 0x08
    jne  .normal
    mov  eax, sio_fmt_backspace
    push eax
    call _printf
    add  esp, 4
    ret
.normal:
    push eax
    push sio_fmt_char
    call _printf
    add  esp, 8
    ret

; void sio_control(SIO* sio, uint8_t value)
_sio_control:
    mov  edx, [esp+4]                       ; edx = sio
    movzx ecx, byte [esp+8]                 ; ecx = value

    mov  eax, ecx
    and  eax, 0x03
    mov  [edx+SIO_OFF_CONTROL], al          ; control = value & 0x03

    mov  eax, ecx
    and  eax, 0x01
    mov  [edx+SIO_OFF_INPUT_INT], al        ; input_interrupt = value & 0x01

    mov  eax, ecx
    and  eax, 0x02
    shr  eax, 1
    mov  [edx+SIO_OFF_OUTPUT_INT], al       ; output_interrupt = (value & 0x02) >> 1
    ret

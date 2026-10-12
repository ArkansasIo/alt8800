; file.asm
; Github: https://github.com/tommojphillips
;
; Assembly rewrite of src/file.c
;
; int read_file_into_buffer(const char* filename, void* buff, uint32_t buff_size,
;                           uint32_t offset, uint32_t* file_size, uint32_t expected_size)
;
; 32-bit cdecl calling convention (Windows):
;   [ebp+8]  filename       const char*
;   [ebp+12] buff           void*
;   [ebp+16] buff_size      uint32_t
;   [ebp+20] offset         uint32_t
;   [ebp+24] file_size      uint32_t*
;   [ebp+28] expected_size  uint32_t
;
; Return: 0 on success, 1 on error. EAX = return value.

BITS 32
global _read_file_into_buffer

extern _fopen_s
extern _fclose
extern _fread
extern _fseek
extern _ftell
extern _printf

section .rodata
    fmt_open_error db "Error: could not open file: %s", 10, 0
    fmt_bad_size   db "Error: invalid file size. Expected %u bytes. Got %u bytes", 10, 0
    fmt_too_big    db "Error: file is too big for buffer. Offset: %x. File size: %u bytes. Buffer size: %u bytes", 10, 0
    fmt_loaded     db "%X", 9, "-> %s ( %u bytes )", 10, 0
    mode_rb        db "rb", 0

section .bss
    file_ptr  resd 1          ; FILE* file = NULL
    size_buf  resd 1          ; uint32_t size

section .text

_read_file_into_buffer:
    push ebp
    mov  ebp, esp
    push ebx
    push esi
    push edi
    sub  esp, 12

    ; ebx = filename, esi = buff, edi = offset
    mov  ebx, [ebp+8]
    mov  esi, [ebp+12]
    mov  edi, [ebp+20]

    ; file = NULL
    mov  dword [file_ptr], 0

    ; if (filename == NULL) return 1;
    test ebx, ebx
    jz   .ret_error

    ; fopen_s(&file, filename, "rb")
    mov  dword [esp], file_ptr          ; &file
    mov  [esp+4], ebx                   ; filename
    mov  dword [esp+8], mode_rb         ; "rb"
    call _fopen_s
    add  esp, 12
    cmp  dword [file_ptr], 0
    jne  .file_opened
    ; printf("Error: could not open file: %s\n", filename);
    mov  dword [esp], fmt_open_error
    mov  [esp+4], ebx
    call _printf
    add  esp, 8
    jmp  .ret_error

.file_opened:
    ; fseek(file, 0, SEEK_END) ; SEEK_END = 2
    mov  dword [esp], [file_ptr]
    mov  dword [esp+4], 0
    mov  dword [esp+8], 2
    call _fseek
    add  esp, 12

    ; size = ftell(file)
    mov  dword [esp], [file_ptr]
    call _ftell
    add  esp, 4
    mov  [size_buf], eax

    ; if (file_size != NULL) *file_size = size;
    mov  ecx, [ebp+24]
    test ecx, ecx
    jz   .no_filesize
    mov  edx, [size_buf]
    mov  [ecx], edx
.no_filesize:
    ; fseek(file, 0, SEEK_SET) ; SEEK_SET = 0
    mov  dword [esp], [file_ptr]
    mov  dword [esp+4], 0
    mov  dword [esp+8], 0
    call _fseek
    add  esp, 12

    ; expected size check
    mov  eax, [ebp+28]
    test eax, eax
    jz   .skip_expected
    cmp  eax, [size_buf]
    je   .skip_expected
    ; printf(... expected_size, size)
    mov  dword [esp], fmt_bad_size
    mov  [esp+4], eax                    ; expected_size
    mov  edx, [size_buf]
    mov  [esp+8], edx                    ; size
    call _printf
    add  esp, 12
    ; fclose(file)
    mov  dword [esp], [file_ptr]
    call _fclose
    add  esp, 4
    jmp  .ret_error

.skip_expected:
    ; if (offset + size > buff_size) -> too big
    mov  eax, edi
    add  eax, [size_buf]
    cmp  eax, [ebp+16]
    jbe  .size_ok
    ; printf(fmt_too_big, offset, size, buff_size)
    mov  dword [esp], fmt_too_big
    mov  [esp+4], edi                    ; offset
    mov  edx, [size_buf]
    mov  [esp+8], edx                    ; size
    mov  edx, [ebp+16]
    mov  [esp+12], edx                   ; buff_size
    call _printf
    add  esp, 16
    ; fclose(file)
    mov  dword [esp], [file_ptr]
    call _fclose
    add  esp, 4
    jmp  .ret_error

.size_ok:
    ; bytes_read = fread(buff + offset, 1, size, file)
    mov  eax, esi
    add  eax, edi
    mov  [esp], eax                      ; ptr = buff + offset
    mov  dword [esp+4], 1                ; size = 1
    mov  edx, [size_buf]
    mov  [esp+8], edx                    ; count = size
    mov  edx, [file_ptr]
    mov  [esp+12], edx                   ; stream = file
    call _fread
    add  esp, 16
    mov  [size_buf], eax                 ; reuse size_buf as bytes_read

    ; printf("%X\t-> %s ( %u bytes )\n", offset, filename, bytes_read)
    mov  dword [esp], fmt_loaded
    mov  [esp+4], edi                    ; offset
    mov  [esp+8], ebx                    ; filename
    mov  edx, [size_buf]
    mov  [esp+12], edx                   ; bytes_read
    call _printf
    add  esp, 16

    ; fclose(file)
    mov  dword [esp], [file_ptr]
    call _fclose
    add  esp, 4

    xor  eax, eax                        ; return 0
    jmp  .done

.ret_error:
    mov  eax, 1

.done:
    add  esp, 12
    pop  edi
    pop  esi
    pop  ebx
    pop  ebp
    ret

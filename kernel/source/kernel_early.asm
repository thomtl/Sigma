[bits 64]

section .text

global long_mode_start
long_mode_start:
    mov ax, cs
    cmp ax, CODE_SEG
    jne .no_correct_code_seg

    mov ax, 0
    mov ss, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax


    mov rax, cr3
    add rax, KERNEL_LMA
    mov qword [rax], 0x0
    invlpg [0]

    jmp _kernel_early

.no_correct_code_seg:
    mov al, '6'
    call long_mode_error


long_mode_error:
    mov dword [RAW_VGA_BUFFER], 0x4f524f45
    mov dword [RAW_VGA_BUFFER + 4], 0x4f3a4f52
    mov dword [RAW_VGA_BUFFER + 8], 0x4f204f20
    mov byte [RAW_VGA_BUFFER + 12], al

    cli
    hlt

initialize_sse:
    mov eax, 1
    cpuid
    bt edx, 25
    jnc .no_sse

    mov rax, cr0
    btc eax, 2
    bts eax, 1
    mov cr0, rax

    mov rax, cr4
    bts eax, 9
    bts eax, 10
    mov cr4, rax

    ret

.no_sse:    
    ret

initialize_avx:
    mov eax, 1
    cpuid
    bt ecx, 26
    jnc .no_avx

    mov rax, cr4
    bts rax, 18 ; Set OSXSAVE bit for access to xgetbv and xsetbv
    mov cr4, rax

    mov rcx, 0
    xgetbv
    or eax, 7
    xsetbv

    ret

.no_avx:
    ret

initialize_efer:
    mov ecx, EFER_MSR
    rdmsr
    bts eax, 0 ; Set SCE for the syscall and sysret instructions
    bts eax, 11 ; Set NXE for No-Execute-Support
    wrmsr

    ret

initialize_cr0:
    mov rax, cr0
    bts rax, 16 ; Set Write Protect bit so the CPU will enfore the Writable paging bit in kernel mode
    mov cr0, rax

    ret
 

_kernel_early:



    mov rsp, stack_top
    mov rbp, rsp

    call initialize_sse
    call initialize_avx
    call initialize_efer
    call initialize_cr0


    extern _init
    call _init

    extern _start_multiboot_info
    extern _start_multiboot_magic

    mov rsi, 0
    mov esi, dword [_start_multiboot_magic]

    mov rdi, 0
    mov edi, dword [_start_multiboot_info]
    add rdi, KERNEL_LMA

    cld

    extern kernel_main
    call kernel_main

    extern _fini
    call _fini

    cli

    hlt

global _smp_kernel_early
_smp_kernel_early:
    call initialize_sse
    call initialize_avx
    call initialize_efer
    call initialize_cr0

    cld

    extern smp_kernel_main
    call smp_kernel_main

    cli
    hlt


section .bss

align 16
stack_bottom:
    resb 0x4000
stack_top:


RAW_VGA_BUFFER equ (0xb8000 + KERNEL_LMA)

CODE_SEG equ 0x08
DATA_SEG equ 0x10

KERNEL_LMA equ 0xffffffff80000000

EFER_MSR equ 0xC0000080
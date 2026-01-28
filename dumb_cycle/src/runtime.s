.text
.extern _start_c
.global _start
_start:
        xor %rbp, %rbp
        mov (%rsp), %rdi
        mov %rsp, %rsi
        add $8, %rsi
        call _start_c
        ud2

.global _syscall
_syscall:
        mov %rdi, %rax
        mov %rsi, %rdi
        mov %rdx, %rsi
        mov %rcx, %rdx
        mov %r8, %r10
        mov %r9, %r8
        mov 8(%rsp), %r9
        syscall
        ret

.section .note.GNU-stack,"",@progbits

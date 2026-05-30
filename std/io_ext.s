.globl prints
prints:
    pushq %rbp
    movq %rsp, %rbp
    movq 16(%rbp), %rdi
    movq %rdi, %rsi
    movq $0, %rax
    movq $255, %rcx
    repne scasb
    subq %rsi, %rdi
    movq %rdi, %rcx
    addb (buf), %cl
    jnc prints_below_capacity
    cmpb $255, %cl
    jne prints_below_capacity
    movq $1, %rax
    movq $1, %rdi
    leaq (buf+1), %rsi
    movzbq (buf), %rdx
    syscall
    movb $0, (buf)
prints_below_capacity:
    leaq (buf+1), %rdi
    movzbq (buf), %rbx
    addq %rbx, %rdi
    rep movsb
    movb %cl, (buf)
    movq %rbp, %rsp
    popq %rbp
    ret $8

.globl printc
printc:
    pushq %rbp
    movq %rsp, %rbp
    movq 16(%rbp), %r8
    cmpb $255, (buf)
    jb printc_below_capacity
    movq $1, %rax
    movq $1, %rdi
    leaq (buf+1), %rsi
    movzbq (buf), %rdx
    syscall
    movb $0, (buf)
printc_below_capacity:
    leaq (buf+1), %rdi
    movzbq (buf), %rbx
    addq %rbx, %rdi
    movb %r8b, (%rdi)
    incb (buf)
    movq %rbp, %rsp
    popq %rbp
    ret $8

.globl flush
flush:
    movq $1, %rax
    movq $1, %rdi
    leaq (buf+1), %rsi
    movzbq (buf), %rdx
    syscall
    movb $0, (buf)
    ret

.bss
buf: .zero 256

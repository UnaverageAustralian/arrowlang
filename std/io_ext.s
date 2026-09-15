.globl "std::io::print"
"std::io::print":
    pushq %rbp
    pushq %rbx
    movq %rsp, %rbp
    movq 24(%rbp), %rax
    xorq %rcx, %rcx
    movq $10, %rbx
    testq %rax, %rax
    jns .L_positive
    pushq $'-
    call "std::io::printc"
    negq %rax
    movq $10, %rbx
.L_positive:
    xorq %rdx, %rdx
    idivq %rbx
    pushq %rdx
    incq %rcx
    testq %rax, %rax
    jnz .L_positive
.L_pop_digit:
    addq $'0, (%rsp)
    call "std::io::printc"
    decq %rcx
    jnz .L_pop_digit
    movq %rbp, %rsp
    popq %rbx
    popq %rbp
    ret $8

.globl "std::io::prints"
"std::io::prints":
    pushq %rbp
    pushq %rbx
    movq %rsp, %rbp
    movq 24(%rbp), %rdi
    movq %rdi, %rsi
    xorq %rax, %rax
    movq $255, %rcx
    repne scasb
    subq %rsi, %rdi
    movq %rdi, %rbx
    decb %cl
    jnc .L_prints_below_capacity
    movq $1, %rax
    movq $1, %rdi
    leaq (buf+1), %rsi
    movzbq (buf), %rdx
    syscall
    movb $0, (buf)
.L_prints_below_capacity:
    movq %rbx, %rcx
    leaq (buf+1), %rdi
    movzbq (buf), %rbx
    addq %rbx, %rdi
    movb %cl, %bl
    rep movsb
    addb %bl, (buf)
    movq %rbp, %rsp
    popq %rbx
    popq %rbp
    ret $8

.globl "std::io::printc"
"std::io::printc":
    pushq %rbp
    pushq %rbx
    movq %rsp, %rbp
    movq 24(%rbp), %rbx
    cmpb $255, (buf)
    jb .L_printc_below_capacity
    movq $1, %rax
    movq $1, %rdi
    leaq (buf+1), %rsi
    movzbq (buf), %rdx
    syscall
    movb $0, (buf)
.L_printc_below_capacity:
    movq %rbx, %r8
    leaq (buf+1), %rdi
    movzbq (buf), %rbx
    addq %rbx, %rdi
    movb %r8b, (%rdi)
    incb (buf)
    movq %rbp, %rsp
    popq %rbx
    popq %rbp
    ret $8

.globl "std::io::flush"
"std::io::flush":
    movq $1, %rax
    movq $1, %rdi
    leaq (buf+1), %rsi
    movzbq (buf), %rdx
    syscall
    movb $0, (buf)
    ret

.bss
buf: .zero 256

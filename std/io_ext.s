.globl "io::print"
"io::print":
    pushq %rbp
    pushq %rbx
    movq %rsp, %rbp
    movq 16(%rbp), %rax
    xorq %rcx, %rcx
    movq $10, %rbx
    cmpq $0, %rax
    jge .L_positive
    pushq $45
    call "io::printc"
    negq %rax
    movq $10, %rbx
.L_positive:
    xorq %rdx, %rdx
    idivq %rbx
    pushq %rdx
    incq %rcx
    cmpq $0, %rax
    jg .L_positive
.L_pop_digit:
    addq $'0, (%rsp)
    call "io::printc"
    decq %rcx
    cmpq $0, %rcx
    jg .L_pop_digit
    movq %rbp, %rsp
    popq %rbx
    popq %rbp
    ret $8

.globl "io::prints"
"io::prints":
    pushq %rbp
    pushq %rbx
    movq %rsp, %rbp
    movq 16(%rbp), %rdi
    movq %rdi, %rsi
    movb $0, %al
    movq $255, %rcx
    repne scasb
    subq %rsi, %rdi
    movq %rdi, %rcx
    decb %cl
    jnc .L_prints_below_capacity
    movq $1, %rax
    movq $1, %rdi
    leaq (buf+1), %rsi
    movzbq (buf), %rdx
    syscall
    movb $0, (buf)
.L_prints_below_capacity:
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

.globl "io::printc"
"io::printc":
    pushq %rbp
    pushq %rbx
    movq %rsp, %rbp
    movq 16(%rbp), %r8
    cmpb $255, (buf)
    jb .L_printc_below_capacity
    movq $1, %rax
    movq $1, %rdi
    leaq (buf+1), %rsi
    movzbq (buf), %rdx
    syscall
    movb $0, (buf)
.L_printc_below_capacity:
    leaq (buf+1), %rdi
    movzbq (buf), %rbx
    addq %rbx, %rdi
    movb %r8b, (%rdi)
    incb (buf)
    movq %rbp, %rsp
    popq %rbx
    popq %rbp
    ret $8

.globl "io::flush"
"io::flush":
    movq $1, %rax
    movq $1, %rdi
    leaq (buf+1), %rsi
    movzbq (buf), %rdx
    syscall
    movb $0, (buf)
    ret

.bss
buf: .zero 256

.globl "io::print"
"io::print":
    pushq %rbp
    movq %rsp, %rbp
    movq 16(%rbp), %rax
    xorq %rcx, %rcx
    movq $10, %rbx
    cmpq $0, %rax
    jge positive
    pushq $45
    call "io::printc"
    negq %rax
    movq $10, %rbx
positive:
    xorq %rdx, %rdx
    idivq %rbx
    pushq %rdx
    incq %rcx
    cmpq $0, %rax
    jg positive
pop_digit:
    addq $'0, (%rsp)
    call "io::printc"
    decq %rcx
    cmpq $0, %rcx
    jg pop_digit
    movq %rbp, %rsp
    popq %rbp
    ret $8

.globl "io::prints"
"io::prints":
    pushq %rbp
    movq %rsp, %rbp
    movq 16(%rbp), %rdi
    movq %rdi, %rsi
    movb $0, %al
    movq $255, %rcx
    repne scasb
    subq %rsi, %rdi
    movq %rdi, %rcx
    addb (buf), %cl
    jnc prints_below_capacity
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
    movb %cl, %bl
    rep movsb
    movb %bl, (buf)
    movq %rbp, %rsp
    popq %rbp
    ret $8

.globl "io::printc"
"io::printc":
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

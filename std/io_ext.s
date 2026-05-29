default rel

global prints
$prints:
    push rbp
    mov rbp, rsp
    mov rdi, qword [rbp+16]
    mov rsi, rdi
    mov rax, 0
    mov rcx, 255
    repne scasb
    sub rdi, rsi
    mov rcx, rdi
    add cl, byte [buf]
    jnc prints_below_capacity
    cmp cl, 255
    jne prints_below_capacity
    mov rax, 1
    mov rdi, 1
    mov rsi, buf+1
    movzx rdx, byte [buf]
    syscall
    mov byte [buf], 0
prints_below_capacity:
    mov rdi, buf+1
    movzx rbx, byte [buf]
    add rdi, rbx
    rep movsb
    mov byte [buf], cl
    mov rsp, rbp
    pop rbp
    ret

global printc
$printc:
    push rbp
    mov rbp, rsp
    mov r8, qword [rbp+16]
    cmp byte [buf], 255
    jl printc_below_capacity
    mov rax, 1
    mov rdi, 1
    mov rsi, buf+1
    movzx rdx, byte [buf]
    syscall
    mov byte [buf], 0
printc_below_capacity:
    mov rdi, buf+1
    movzx rbx, byte [buf]
    add rdi, rbx
    mov byte [rdi], r8b
    inc byte [buf]
    mov rsp, rbp
    pop rbp
    ret

global flush
$flush:
    mov rax, 1
    mov rdi, 1
    mov rsi, buf+1
    movzx rdx, byte [buf]
    syscall
    mov byte [buf], 0
    ret

section .bss
buf: resb 256

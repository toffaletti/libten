
;           Copyright Oliver Kowalke 2009.
;  Distributed under the Boost Software License, Version 1.0.
;     (See accompanying file LICENSE_1_0.txt or copy at
;           http://www.boost.org/LICENSE_1_0.txt)

;  ----------------------------------------------------------------------------------
;  |    0    |    1    |    2    |    3    |    4     |    5    |    6    |    7    |
;  ----------------------------------------------------------------------------------
;  |   0x0   |   0x4   |   0x8   |   0xc   |   0x10   |   0x14  |   0x18  |   0x1c  |
;  ----------------------------------------------------------------------------------
;  |        R12        |         R13       |         R14        |        R15        |
;  ----------------------------------------------------------------------------------
;  ----------------------------------------------------------------------------------
;  |    8    |    9    |   10    |   11    |    12    |    13   |    14   |    15   |
;  ----------------------------------------------------------------------------------
;  |   0x20  |   0x24  |   0x28  |  0x2c   |   0x30   |   0x34  |   0x38  |   0x3c  |
;  ----------------------------------------------------------------------------------
;  |        RDI        |        RSI        |         RBX        |        RBP        |
;  ----------------------------------------------------------------------------------
;  ----------------------------------------------------------------------------------
;  |    16   |    17   |   18   |    19    |    20    |    21   |   22    |   23    |
;  ----------------------------------------------------------------------------------
;  |   0x40  |   0x44  |  0x48  |   0x4c   |   0x50   |  0x54   |  0x58   |  0x5c   |
;  ----------------------------------------------------------------------------------
;  |        RSP        |       RIP         |         RCX        |                   |
;  ----------------------------------------------------------------------------------
;  ----------------------------------------------------------------------------------
;  |    24   |    25   |                                                            |
;  ----------------------------------------------------------------------------------
;  |   0x60  |   0x64  |                                                            |
;  ----------------------------------------------------------------------------------
;  |       fc_fp       |                                                            |
;  ----------------------------------------------------------------------------------
;  ----------------------------------------------------------------------------------
;  |    26    |   27  |    28    |    29   |    30   |    31   |    32   |    33    |
;  ----------------------------------------------------------------------------------
;  |   0x68   |  0x6c |   0x70   |   0x74  |   0x78  |   0x7c  |   0x80  |   0x84   |
;  ----------------------------------------------------------------------------------
;  |       sbase      |       slimit       |       fc_link     |      fbr_strg      |
;  ----------------------------------------------------------------------------------

EXTERN  _exit:PROC                  ; standard C library function
EXTERN  boost_fcontext_seh:PROC		; exception handler
.code

boost_fcontext_jump PROC EXPORT FRAME:boost_fcontext_seh
	.endprolog

    mov     [rcx],       r12        ; save R12
    mov     [rcx+08h],   r13        ; save R13
    mov     [rcx+010h],  r14        ; save R14
    mov     [rcx+018h],  r15        ; save R15
    mov     [rcx+020h],  rdi        ; save RDI
    mov     [rcx+028h],  rsi        ; save RSI
    mov     [rcx+030h],  rbx        ; save RBX
    mov     [rcx+038h],  rbp        ; save RBP

    mov     r8,          [rcx+060h]
    fxsave  [r8]                    ; save fp

    mov     r8,          gs:[030h]  ; load NT_TIB
    mov     rax,         [r8+08h]   ; load current stack base
    mov     [rcx+068h],  rax        ; save current stack base
    mov     rax,         [r8+010h]  ; load current stack limit
    mov     [rcx+070h],  rax        ; save current stack limit
    mov     rax,         [r8+018h]  ; load fiber local storage
    mov     [rcx+080h],  rax        ; save fiber local storage

    lea     rax,         [rsp+08h]  ; exclude the return address
    mov     [rcx+040h],  rax        ; save as stack pointer
    mov     rax,         [rsp]      ; load return address
    mov     [rcx+048h],  rax        ; save return address


    mov     r12,        [rdx]       ; restore R12
    mov     r13,        [rdx+08h]   ; restore R13
    mov     r14,        [rdx+010h]  ; restore R14
    mov     r15,        [rdx+018h]  ; restore R15
    mov     rdi,        [rdx+020h]  ; restore RDI
    mov     rsi,        [rdx+028h]  ; restore RSI
    mov     rbx,        [rdx+030h]  ; restore RBX
    mov     rbp,        [rdx+038h]  ; restore RBP

    mov     r8,         [rdx+060h]
    fxrstor [r8]                    ; restore fp

    mov     r8,         gs:[030h]   ; load NT_TIB
    mov     rax,        [rdx+068h]  ; load stack base
    mov     [r8+08h],   rax         ; restore stack base
    mov     rax,        [rdx+070h]  ; load stack limit
    mov     [r8+010h],  rax         ; restore stack limit
    mov     rax,        [rdx+080h]  ; load fiber local storage
    mov     [r8+018h],  rax         ; restore fiber local storage

    mov     rsp,        [rdx+040h]  ; restore RSP
    mov     r8,         [rdx+048h]  ; fetch the address to returned to
    mov     rcx,        [rdx+050h]  ; restore RCX as first argument for called context

    xor     rax,        rax         ; set RAX to zero
    jmp     r8                      ; indirect jump to caller
boost_fcontext_jump ENDP

boost_fcontext_make PROC EXPORT
    mov  [rcx],         rcx         ; store the address of current context
    mov  [rcx+048h],    rdx         ; save the address of the function supposed to run
    mov  [rcx+050h],    r8          ; save the the void pointer
    mov  rdx,           [rcx+068h]  ; load the address where the context stack beginns
    lea  rdx,           [rdx-028h]  ; reserve space for the last frame on stack, (RSP + 8) % 16 == 0
    mov  [rcx+040h],    rdx         ; save the address where the context stack beginns

    mov  rax,       [rcx+078h]      ; load the address of the next context
    mov  [rcx+08h], rax             ; save the address of next context

    mov     rax,         [rcx+060h]
    fxsave  [rax]                   ; save fp

    lea  rax,       boost_fcontext_link   ; helper code executed after fn() returns
    mov  [rdx],     rax             ; set the return address to the helper function

    xor  rax,       rax             ; set RAX to zero
    ret
boost_fcontext_make ENDP

boost_fcontext_link PROC
    test  r13,      r13             ; test if a next context was given
    je    finish                    ; jump to finish

	mov   rcx,      r12             ; first argument eq. address of current context
	mov   rdx,      r13             ; second argumnet eq. address of next context
    call  boost_fcontext_jump       ; install next context

finish:
	xor   rcx,      rcx
    call  _exit                     ; exit application
    hlt
boost_fcontext_link ENDP
END

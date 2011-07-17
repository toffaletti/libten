
;           Copyright Oliver Kowalke 2009.
;  Distributed under the Boost Software License, Version 1.0.
;     (See accompanying file LICENSE_1_0.txt or copy at
;           http://www.boost.org/LICENSE_1_0.txt)

;  --------------------------------------------------------------
;  |    0    |    1    |    2    |    3    |    4     |    5    |
;  --------------------------------------------------------------
;  |    0h   |   04h   |   08h   |   0ch   |   010h   |   014h  |
;  --------------------------------------------------------------
;  |   EDI   |   ESI   |   EBX   |   EBP   |   ESP    |   EIP   |
;  --------------------------------------------------------------
;  --------------------------------------------------------------
;  |    6    |    7    |                                        |
;  --------------------------------------------------------------
;  |   018h  |   01ch  |                                        |
;  --------------------------------------------------------------
;  | fc_mxcsr|fc_x87_cw|                                        |
;  --------------------------------------------------------------
;  --------------------------------------------------------------
;  |    8    |    9    |   10    |   11    |    12    |    13   |
;  --------------------------------------------------------------
;  |  020h   |  024h   |  028h   |  02ch   |   030h   |         |
;  --------------------------------------------------------------
;  | ss_base | ss_limit| fc_link |excpt_lst|fc_strge |          |
;  --------------------------------------------------------------

.386
.XMM
.model flat, c
_exit PROTO, value:SDWORD 
boost_fcontext_seh PROTO, except:DWORD, frame:DWORD, context:DWORD, dispatch:DWORD
.code

boost_fcontext_jump PROC EXPORT
    mov     eax,         [esp+04h]  ; load address of the first fcontext_t arg
    mov     [eax],       edi        ; save EDI
    mov     [eax+04h],   esi        ; save ESI
    mov     [eax+08h],   ebx        ; save EBX
    mov     [eax+0ch],   ebp        ; save EBP

    assume  fs:nothing
    mov     edx,         fs:[018h]  ; load NT_TIB
    assume  fs:error
    mov     ecx,         [edx]      ; load current SEH exception list
    mov     [eax+02ch],  ecx        ; save current exception list
    mov     ecx,         [edx+04h]  ; load current stack base
    mov     [eax+020h],  ecx        ; save current stack base
    mov     ecx,         [edx+08h]  ; load current stack limit
    mov     [eax+024h],  ecx        ; save current stack limit
    mov     ecx,         [edx+010h] ; load fiber local storage
    mov     [eax+030h],  ecx        ; save fiber local storage

    stmxcsr [eax+018h]              ; save SSE2 control word
    fnstcw  [eax+01ch]              ; save x87 control word

    lea     ecx,         [esp+04h]  ; exclude the return address
    mov     [eax+010h],  ecx        ; save as stack pointer
    mov     ecx,         [esp]      ; load return address
    mov     [eax+014h],  ecx        ; save return address


    mov     eax,        [esp+08h]   ; load address of the second fcontext_t arg
    mov     edi,        [eax]       ; restore EDI
    mov     esi,        [eax+04h]   ; restore ESI
    mov     ebx,        [eax+08h]   ; restore EBX
    mov     ebp,        [eax+0ch]   ; restore EBP

    assume  fs:nothing
    mov     edx,        fs:[018h]   ; load NT_TIB
    assume  fs:error
    mov     ecx,        [eax+02ch]  ; load SEH exception list
    mov     [edx],      ecx         ; restore next SEH item
    mov     ecx,        [eax+020h]  ; load stack base
    mov     [edx+04h],  ecx         ; restore stack base
    mov     ecx,        [eax+024h]  ; load stack limit
    mov     [edx+08h],  ecx         ; restore stack limit
    mov     ecx,        [eax+030h]  ; load fiber local storage
    mov     [edx+010h], ecx         ; restore fiber local storage

    ldmxcsr [eax+018h]              ; restore SSE2 control word
    fldcw   [eax+01ch]              ; restore x87 control word

    mov     esp,        [eax+010h]  ; restore ESP
    mov     ecx,        [eax+014h]  ; fetch the address to return to

    xor     eax,        eax         ; set EAX to zero
    jmp     ecx                     ; indirect jump to context
boost_fcontext_jump ENDP

boost_fcontext_make PROC EXPORT
    mov  eax,         [esp+04h]     ; load address of the fcontext_t arg0
    mov  [eax],       eax           ; save the address of current context
    mov  ecx,         [esp+08h]     ; load the address of the function supposed to run
    mov  [eax+014h],  ecx           ; save the address of the function supposed to run
    mov  edx,         [eax+020h]    ; load the stack base
    lea  edx,         [edx-014h]    ; reserve space for last frame on stack, (ESP + 4) % 16 == 0
    mov  [eax+010h],  edx           ; save the address

    mov  ecx,         boost_fcontext_seh    ; set ECX to exception-handler
    mov  [edx+0ch],   ecx               ; save ECX as SEH handler
    mov  ecx,         0ffffffffh        ; set ECX to -1
    mov  [edx+08h],   ecx               ; save ECX as next SEH item
    lea  ecx,         [edx+08h]         ; load address of next SEH item
    mov  [eax+02ch],  ecx               ; save next SEH

    mov  ecx,         [eax+028h]    ; load the address of the next context
    mov  [eax+04h],   ecx           ; save the address of the next context
    mov  ecx,         [esp+0ch]     ; load the address of the void pointer arg2
    mov  [edx+04h],   ecx           ; save the address of the void pointer onto the context stack
    stmxcsr [eax+018h]              ; save SSE2 control word
    fnstcw  [eax+01ch]              ; save x87 control word
    mov  ecx,         boost_fcontext_link ; load helper code executed after fn() returns
    mov  [edx],       ecx           ; save helper code executed adter fn() returns
    xor  eax,         eax           ; set EAX to zero
    ret
boost_fcontext_make ENDP

boost_fcontext_link PROC
    lea   esp,        [esp-0ch]     ; adjust the stack to proper boundaries
    test  esi,        esi           ; test if a next context was given
    je    finish                    ; jump to finish

    push  esi                       ; push the address of the next context on the stack
    push  edi                       ; push the address of the current context on the stack
    call  boost_fcontext_jump       ; install next context

finish:
    xor   eax,        eax           ; set EAX to zero
	push  eax						; exit code is zero
    call  _exit                     ; exit application
    hlt
boost_fcontext_link ENDP
END

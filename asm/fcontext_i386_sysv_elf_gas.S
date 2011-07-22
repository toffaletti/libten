/*
            Copyright Oliver Kowalke 2009.
   Distributed under the Boost Software License, Version 1.0.
      (See accompanying file LICENSE_1_0.txt or copy at
          http://www.boost.org/LICENSE_1_0.txt)
*/

/********************************************************************
 *                                                                  *
 *  --------------------------------------------------------------  *
 *  |    0    |    1    |    2    |    3    |    4     |    5    |  *
 *  --------------------------------------------------------------  *
 *  |   0x0   |   0x4   |   0x8   |   0xc   |   0x10   |   0x14  |  *
 *  --------------------------------------------------------------  *
 *  |   EDI   |   ESI   |   EBX   |   EBP   |   ESP    |   EIP   |  *
 *  --------------------------------------------------------------  *
 *  --------------------------------------------------------------  *
 *  |    6    |    7    |                                        |  *
 *  -------------------i------------------------------------------  *
 *  |   0x18  |   0x1c  |                                        |  *
 *  --------------------------------------------------------------  *
 *  | fc_mxcsr|fc_x87_cw|                                        |  *
 *  --------------------------------------------------------------  *
 *  --------------------------------------------------------------  *
 *  |    8    |    9    |    10   |                              |  *
 *  --------------------------------------------------------------  *
 *  |  0x20   |   0x24  |   0x28  |                              |  *
 *  --------------------------------------------------------------  *
 *  |  sbase  | slimit  | fc_link |                              |  *
 *  --------------------------------------------------------------  *
 *                                                                  *
 * *****************************************************************/

.text
.globl boost_fcontext_jump
.align 2
.type boost_fcontext_jump,@function
boost_fcontext_jump:
    movl    0x4(%esp), %eax         /* load address of the first fcontext_t arg */
    movl    %edi,       (%eax)      /* save EDI */
    movl    %esi,       0x4(%eax)   /* save ESI */
    movl    %ebx,       0x8(%eax)   /* save EBX */
    movl    %ebp,       0xc(%eax)   /* save EBP */

    stmxcsr  0x18(%eax)              /* save SSE2 control and status word */
    fnstcw   0x1c(%eax)              /* save x87 control word */

    leal    0x4(%esp),  %ecx        /* exclude the return address */
    movl    %ecx,       0x10(%eax)  /* save as stack pointer */
    movl    (%esp),     %ecx        /* load return address */
    movl    %ecx,       0x14(%eax)  /* save return address */


    movl    0x8(%esp),   %eax       /* load address of the second fcontext_t arg */
    movl    (%eax),      %edi       /* restore EDI */
    movl    0x4(%eax),   %esi       /* restore ESI */
    movl    0x8(%eax),   %ebx       /* restore EBX */
    movl    0xc(%eax),   %ebp       /* restore EBP */

    ldmxcsr  0x18(%eax)             /* restore SSE2 control and status word */
    fldcw    0x1c(%eax)             /* restore x87 control word */

    movl    0x10(%eax),  %esp       /* restore ESP */
    movl    0x14(%eax),  %ecx       /* fetch the address to return to */

    xorl    %eax,        %eax       /* set EAX to zero */
    jmp     *%ecx                   /* indirect jump to context */
.size boost_fcontext_jump,.-boost_fcontext_jump

.text
.globl boost_fcontext_make
.align 2
.type boost_fcontext_make,@function
boost_fcontext_make:
    movl   0x4(%esp),      %eax         /* load address of the fcontext_t */
    movl   %eax,           (%eax)       /* save the address of current context */
    movl   0x8(%esp),      %ecx         /* load the address of the function supposed to run */
    movl   %ecx,           0x14(%eax)   /* save the address of the function supposed to run */
    movl   0x20(%eax),     %edx         /* load the stack base */
    leal   -0x14(%edx),     %edx        /* reserve space for the last frame on stack, (ESP + 4) % 16 == 0 */
    movl   %edx,           0x10(%eax)   /* save the address as stack pointer */
    movl   0x28(%eax),     %ecx         /* load the address of the next context */
    movl   %ecx,           0x4(%eax)    /* save the address of the next context */
    movl   0xc(%esp),      %ecx         /* load the address of the void pointer */
    movl   %ecx,           0x4(%edx)    /* save the address of the void pointer */
#ifdef __PIC__
	call	1f
1:	popl	%ecx						/* address of label 1 */
	addl	$link_fcontext-1b, %ecx		/* compute address of label link_fcontext */
	movl	%ecx, (%edx)				/* store address of link_fcontext as return address on stack */
#else
	movl	$link_fcontext, (%edx)		/* store address of link_fcontext as return address on stack */
#endif

    xorl   %eax,            %eax        /* set EAX to zero */
    ret

link_fcontext:
	leal   -0xc(%esp),    %esp

#ifdef	__PIC__
	call	2f
2:	popl	%ebx									/* address of label 2 */
	addl	$_GLOBAL_OFFSET_TABLE_+[.-2b], %ebx		/* compute address of GOT and store it in EBX */
#endif

    testl  %esi,           %esi         /* test if a next context was given */
    je     3f                           /* jump to finish */

    pushl  %esi							/* push the address of the next context on the stack */
	pushl  %edi							/* push the address of the current context to stack */
#ifdef __PIC__
    call   boost_fcontext_jump@PLT      /* jump to next context */
#else
    call   boost_fcontext_jump          /* jump to next context */
#endif

3:
    movl    %eax,           %eax
    pushl   %eax                        /* exit code is zero */
#ifdef __PIC__
    call   _exit@PLT                    /* exit application */
#else
    call   _exit                        /* exit application */
#endif
	hlt
.size boost_fcontext_make,.-boost_fcontext_make

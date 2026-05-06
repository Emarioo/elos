#!/usr/bin/env python3


import os, sys

spath = sys.argv[1]

text = f'''
.intel_syntax noprefix

# .section .bss
# isr_stack:
#     .fill 4096, 1, 0
# isr_stack_end:

.section .text
'''

# We may need to save non-volatile registers in the handlers?

for i in range(32):
    text += f'''
isr_stub_{i}:
    cli
    # don't assume stack is ok?
    # lea rsp, isr_stack

    { 'pop rdx' if i == 13 else '' }

    // error code is pushed by CPU

    push rbp
    mov rbp, rsp

    # push rax
    # push rbx
    # push rcx
    # push rdx
    # push rsi
    # push rdi
    # push r8
    # push r9
    # push r10
    # push r11

    mov rdi, {i}  # isr number
    mov rsi, rsp  # pass pointer to stack frame

    call exception_handler
    retq
    '''

text += '''
.section .data
.global isr_stub_table
isr_stub_table:
'''

for i in range(32):
    text += f'''
    .quad isr_stub_{i}    
    '''


with open(spath, "w") as file:
    file.write(text)

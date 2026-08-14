#!/usr/bin/env python3


import os, sys

spath = sys.argv[1]

text = f'''
.intel_syntax noprefix

.section .text
'''

push_volatile = '''
        push rax
        push rcx
        push rdx
        push rsi
        push rdi
        push r8
        push r9
        push r10
        push r11
'''

pop_volatile = '''
        pop r11
        pop r10
        pop r9
        pop r8
        pop rdi
        pop rsi
        pop rdx
        pop rcx
        pop rax
'''

def has_error_code(vector):
    return vector in (8,10,11,12,13,14,17,21)

for i in range(256):
    if i < 32:
        text += f'''
    isr_stub_{i}:
        # don't assume stack is ok?
        # lea rsp, isr_stack

        # Error code is pushed by CPU for some exceptions.
        # If not then we do dummy 0.
        { 'push 0' if not has_error_code(i) else '' }

        {push_volatile}

        mov rdi, {i}  # isr number
        mov rsi, rsp  # pass pointer to stack frame

        # 16-byte alignment
        sub rsp, 8

        call exception_handler
        
        add rsp, 8

        {pop_volatile}

        # pop dummy error code
        add rsp, 8

        iretq
        '''
    else:
        text += f'''
    isr_stub_{i}:

        {push_volatile}

        mov rdi, {i}  # isr number
        mov rsi, rsp  # pass pointer to stack frame

        call interrupt_handler

        {pop_volatile}

        iretq
        '''

text += '''
.section .data
.global isr_stub_table
isr_stub_table:
'''

for i in range(256):
    text += f".quad isr_stub_{i}\n"

with open(spath, "w") as file:
    file.write(text)

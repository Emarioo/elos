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
    elif i == 33:
        text += f'''
    isr_stub_{i}:

        {push_volatile}

        mov rdi, {i}  # isr number
        mov rsi, rsp  # pass pointer to stack frame

        call interrupt_handler

        {pop_volatile}

        iretq
        '''
    elif i == 34:
        text += f'''
    isr_stub_{i}:

        {push_volatile}

        call hpet_isr

        {pop_volatile}

        iretq
        '''
    elif i == 48:
        pass
    #     text += f'''
    # isr_stub_{i}:
    
    #     {push_volatile}

    #     call interrupt_timer

    #     {pop_volatile}

    #     iretq
    #     '''
    elif i == 255:
        text += f'''
    isr_stub_{i}:

        {push_volatile}

        mov rdi, {i}
        mov rsi, rsp

        call unused_handler

        {pop_volatile}

        iretq
        '''
    else:
        continue

text += '''
isr_stub_unused:
    
    mov rdi, -1
    call unused_handler

    iretq

.section .data
.global isr_stub_table
isr_stub_table:
'''

for i in range(256):
    if i < 32:
        text += f".quad isr_stub_{i}\n"
    elif i == 33:
        text += f".quad isr_stub_{i}\n"
    elif i == 34:
        text += f".quad isr_stub_{i}\n"
    elif i == 48:
        text += f".quad timer_isr\n"
    elif i == 255:
        text += f".quad isr_stub_{i}\n"
    else:
        text += f".quad isr_stub_unused\n"

with open(spath, "w") as file:
    file.write(text)

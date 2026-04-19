#!/usr/bin/env python3


import os, sys

spath = sys.argv[1]

text = f'''
.intel_syntax noprefix
.section .text
'''

# We may need to save non-volatile registers in the handlers?

for i in range(32):
    text += f'''
isr_stub_{i}:
    mov rdi, {i}
    { 'pop rsi' if i == 13 else '' }
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

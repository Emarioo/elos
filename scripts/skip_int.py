import gdb

class SkipInterrupts(gdb.Command):
    """Step over INT instructions automatically."""

    def __init__(self):
        super(SkipInterrupts, self).__init__("sit", gdb.COMMAND_USER)

    def invoke(self, arg, from_tty):
        # Read current instruction
        pc = int(gdb.parse_and_eval("$pc"))
        insn = gdb.execute(f"x/i {pc}", to_string=True)
        
        # If it's an INT instruction
        if "int" in insn:
            # Compute address of next instruction
            parts = insn.split()
            size = 2 if parts[1].startswith("0x") else 1  # rough estimate
            gdb.execute(f"tbreak *{pc + size}")
            gdb.execute("continue")
        else:
            gdb.execute("si")

SkipInterrupts()
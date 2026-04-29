# Assumes Linux machine

# Compile kernel and netboot server and run EFI application that downloads kernel.
build.py

# Start boot server (techincally just a custom file transfer server)
# start this right after build.py has finished compiling netboot_server and before
# or while it starts QEMU.
int/netboot

# Kernel should boot up and in server console you should see "Request" and "Finished"

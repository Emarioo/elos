Format specifications


# BIOS
## Disc
- Reading disc sectors - https://www.ctyme.com/intr/rb-0607.htm

- Master boot record - https://en.wikipedia.org/wiki/Master_boot_record

# File systems
## FAT12
- https://www.eit.lth.se/fileadmin/eit/courses/eitn50/Literature/fat12_description.pdf
- https://wiki.osdev.org/FAT12#FAT_12

### Sector layout
|Physical sector|Section|
|-|-|
|0|Boot sector|
|1-9|File allocation table 1|
|10-18|File allocation table 2|
|19-32|Root directory|
|33-2879|Data|


# UEFI
- https://uefi.org/specifications
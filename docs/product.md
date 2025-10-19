At the end of the day we got a raw binary image file `elos.img`.

# For Legacy BIOS
The image looks like this.

For some context, the image is blitted to a USB drive.
The drive contains sectors, we assume 512 bytes each.

We have a Master Boot Record in the first sector.
It has a partion table with one partition with FAT16 file format.

|Physical sector|Section|
|-|-|
|0|Master Boot Record|
|1|Volume Boot record (start of partition #1)|
|2-10|File allocation table 2|
|11-19|File allocation table 2|
|20-33|Root directory|
|34-N|File data (clusters)|


# UEFI
to be done
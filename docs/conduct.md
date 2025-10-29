
# Overall behaviour
- Avoid shell scripts, they are very specific to an operating system (Windows vs Linux). Use python (there will be some `if platform.system() == "Windows/Linux"`).
- Avoid makefiles also OS specific.
- Try to use tools available on Linux and Windows (difficult but we can try).
- All intermediate files are placed in `bin/int` (object files, images, binaries)
- All intermediate files are placed in `bin/int` (object files, images, binaries)
- Write tests. Any test is fine, just write them, at least one when implementing something and integrate it with all tests so you can test them all and know if something broke which worked before.

# Branches
- `main` release branch, always well tested and works as expected.
- `dev` ongoing development, some things may not work, features are merged in here.

# When writing code
- Assembly should have comments
- Do not write half assed code. A feature should be implemented in a complete condition. Finish software components thoroughly before moving on. A secondary part of the component may be left for later for example
  implementhing FAT12, FAT16, FAT32 all at once isn't necessary. Reading and writing to a FAT file system isn't necessary. Reading would be enough but limitations and usage should be clear in the function API.
- Document complicated code.


# Markers
- `@NOTTESTED` Code was written but not tested.
- `@TODO` Should be fixes in the future.
- `@NOTE` Extra information
- `@IMPORTANT` Essential information when programming. Read this before adding or modifying code near this marker.
- `@nocheckin` Left in code that is unfinished. They should be resolved before commiting work.
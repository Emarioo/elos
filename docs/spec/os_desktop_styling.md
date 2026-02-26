
The OS is a desktop OS mostly for programmers and computer enthusiasts.

We want to support customizability for
- Window managers
- Tile managers
- Static and live wallpapers
- Hotbars with icons for applications, CPU/GPU temperature, battery, time and date
- Login manager

The kernel only provides the base terminal "WIN+ESC" which appears
above all other programs. Through this you can spawn programs,
move files, declare startup applications which will run when the computer starts.

The OS will provide my setup as a start which can be tweaked further
or completely replaced with your own desktop software.

The Tile Managers, styling of hotbar, temperature, keybindings
are completely declared in the **Basin** language. No json files, no .css.
Rendering and styling is imperatively written in the language.

We want to support tweaking those styling files and instantly seeing
the change take affect. Realtime update.

We also want to support multiple configs and setups.
Maybe also switching hotbar styling.

It would be nice to just download a couple of desktop setups online and easily try them out with
just a few commands.

We need to consider security. The overall security of the OS is that the user has to give programs access to directories and files. They can't just spawn processess and create or remove files anywhere unless you allow it.

How do we do this as well as NixOS?

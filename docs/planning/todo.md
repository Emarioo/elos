
# Table Of Contents
Categories are standardized in singular and lower case.

The sections are:
- **Operating System**
- **Applications in User Space**

The categories are:
- @current What is currently worked on or planned to be worked on. (may change a lot)
- @urgent Something is broken for 90% of theoretical users
- @feature Literally a new feature, new program, new file system, support for drives, network or whatever
- @optimize Improve software performance
- @bug Large list of known problems, not critical, just annoying
- @experiment Do for fun
- @project Related to project, not so much code or kernel. Can be things like better README, clarify dependencies, improve build guide.

# Operating System
Tasks related to kernel and operating system

## @current
- [ ] A key to reboot kernel.
- [ ] Driver for file system, disk device.

## @urgent

## @bug

## @feature
- [ ] Support multiple mice and keyboards (might be a while but this would be cool and i'm excited for the implementation)
- [ ] Kernel logger. Logs actions in the OS. Files open, written to. Apps started stopped.
- [ ] Support multiple network controllers.
- [ ] Wake On Lan, mainly for testing. I don't need to turn on/off test machine. This assumes the kernel enables WoL first.

## @project
- [ ] Clearly state dependencies of this project. Do fresh Ubuntu install in VirtualBox and compile this project, see what you need.
- [ ] A way to compile the project on Windows? Are there tools for it or is Linux the only?

## @optimize

## @experiment

# Applications in User Space

## @feature
- [ ] Provide C compiler in OS by default. (TinyCC? how do we compile it?)
- [ ] Provide Git by default. (build from source using TinyCC?)

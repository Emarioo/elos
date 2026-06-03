
# Features we want to support
- Get user input, which keys were pressed and released. For mouse we want position and maybe other info? Maybe W key is pressed down 0.7 and in the game we don't walk full speed.
- Register artifical user event publisher. It can read user events and write ones to the OS which distributes to all apps like normal. Nice for playing macros.
- Focuses, orders, priorities. Which application gets to see the event. Some apps want to see all events no matter which app is in "focus".
- User event hooking to override user events to delete or replace them.
- Little overhead when acquiring the events. (few syscalls).
- Little memory overhead on the whole system to store potential buffers.
- You want to know which device the event came from so you can have multiple players on the same computer in the game AND maybe even multiple users on the same computer with their own mouse/keyboard and monitor! Device id must stay the same if it is disconnected. we also need to know when it disconnects and connects. Maybe USER_EVENT_TYPE_DEVICE_CONNECT/DISCONNECT is a part of it?



```c
enum UserEventType {
    KEY, // includes normal mouse and controller buttons?
    MOUSE_MOVE,
    MOUSE_SCROLL, // contains X and Y component (if X is available)
    CONTROLLER_LEFT_JOYSTICK,
    DEVICE_CONNECTED, // mouse,keyboard,controllers
    DEVICE_DISCONNECTED,
}
struct UserEvent {
    UserEventType type;
    // these two values can be mouse position, scrolling x/y, controller movement x/y
    // which key and it's value (pressed, released, between INT_MIN - INT_MAX for how far it's pressed)
    // maybe a third one for scancode? timestamp might be nice? rdtsc
    int device_id;
    int value0;
    int value1; // cast to float and divide by INT_MAX to get actual value from 0.0 - 1.0? Programs that don't want floats can check 'value > INT_MAX/2'
}

// cast to value0 to float and divide by INT_MAX to get actual value from 0.0 - 1.0?
// Programs that don't want floats can check 'value > INT_MAX/2'
// Some events has positional info like mouse no floats?
```

# Implementation

For an application to get user events it must ask the OS. The two ways we support are syscalls or direct memory. Syscalls are relatively slow in a loop. For direct memory you can have a ring buffer per application where events are pushed to by the kernel which the application can read.

```c
ELOS_Error SYS_user_event_listener_create(ELOS_UserEventListener* listener, u64 size);
ELOS_Error SYS_user_event_listener_destroy(ELOS_UserEventListener listener);
ELOS_Error SYS_user_event_listener_info(ELOS_UserEventListener listener, void** buffer, u64* size);
```


How does application push synthetic events to OS?
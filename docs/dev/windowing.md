*Summary from discussion with ChatGPT*

Won't implement this exactly but close to it for now.


Windowing / Display Architecture

Goals
- Applications draw pixels.
- Compositor decides placement.
- Kernel manages displays, surfaces, and input devices.
- Window management remains in userspace.

Kernel
- Owns monitor/framebuffer objects.
- Owns surface memory objects.
- Provides shared memory mapping for surfaces.
- Provides input device events.
- Provides a privileged emergency console independent of the compositor.

Compositor
- Trusted userspace process.
- Has capability to access monitor framebuffers.
- Creates and manages application surfaces.
- Decides:
  - Window position
  - Window size
  - Z-order
  - Focus
  - Monitor placement
  - Fullscreen behavior
- Composites surfaces into monitor framebuffers.

Applications
- Do not receive monitor access.
- Request a surface from the compositor.
- Draw directly into surface memory.
- Notify compositor of updates using:
  - Full surface present
  - Dirty rectangles

Drawing
- Drawing APIs are userspace libraries, not syscalls.
- Examples:
  - draw_rect()
  - draw_line()
  - draw_text()
  - draw_texture()
- Libraries render into surface memory.

Surface Presentation
- surface_present()
  - Full surface update.
- surface_present_rects()
  - Dirty rectangle update.

Capabilities
- DISPLAY_SURFACE
  - Permission to create visible surfaces.
- DISPLAY_OVERLAY
  - Permission to create overlays.
- DISPLAY_PANEL
  - Permission to create desktop panels/status bars.
- DISPLAY_DIRECT
  - Optional permission for direct monitor access.
  - Intended for compositor and special applications only.

Performance
- Initial implementation may copy entire surfaces.
- Dirty rectangles can be added later for optimization.
- Presentation rate limits prevent applications from spamming updates.
- Compositor may throttle applications exceeding presentation quotas.

Emergency Console
- Implemented entirely in kernel.
- Always accessible regardless of compositor state.
- Can inspect, debug, terminate, and manage processes.
- Not affected by userspace scheduling or compositor failures.
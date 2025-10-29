In a font we want:
- Good scaling, keeping quality while changing font size
- Transparency
- Gray scale, we want blurred and smooth edges
- Font describes a subset of UNICODE glyphs (characters).
- Each glyph has a width and height and a center which is the character line it is placed on.
- When drawing text we can specify monospace and padding between glyphs.
- Rendering text at (x = 10, y = 10, height = 10) will place the glyphs neatly in the 10 height box.
  No glyphs are spilling outside. The center line is there for not placed at the complete bottom.
- The data format for glyphs can vary. We support bitmap to begin with (poor scaling quality)

Note that characters are taller than they wide.







https://www.dafont.com/alagard.font

https://www.dafont.com/pixel-operator.font


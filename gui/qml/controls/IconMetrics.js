.pragma library

// Size and spacing of the settings' icons, inside their controls at the left edge (the global QML rule in
// ~/.claude/CLAUDE.md). Verified by screenshot per control type; the values are kept in ~/.claude/qml-icon-padding.md.
// 16 px is what the desktop style draws a ComboBox's own icon at, so every control uses it.
var size = 16;
var margin = 6;                       // from the control's left edge
var gap = 6;                          // between the icon and the control's content
var reserve = margin + size + gap;    // leftPadding (or leftInset for a Slider) of a control holding an icon
var spinButtons = 36;                 // a SpinBox's frame and +/- buttons, added to its widest text
var comboArrow = 34;                  // a ComboBox's frame and drop-down arrow, after its text

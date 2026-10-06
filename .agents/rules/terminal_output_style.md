# Terminal Output Style Rule

- All terminal outputs must be rich in format and text styling.
- Support `--color=[full|none|simple|terminal]` or `-c` switch:
  - `full`: Sixel protocol full-color graphics & icons, real-time RGB gradient progress bars, embedded Fira Code styled mini-terminal with syntax highlighting.
  - `terminal`: 24-bit TrueColor RGB ANSI colors & gradients.
  - `simple`: 16-color basic ANSI palette.
  - `none`: Plain unformatted text for non-interactive scripts / pipes.
- Displays real-time progress information with gradient progress bar during operations.
- Displays a styled mini-terminal window widget with path & syntax highlighting below the progress bar.

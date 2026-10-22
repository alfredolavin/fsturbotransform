# Mapa del proyecto: fsturbotransform

Guía para un agente que llega por primera vez. Verifícala contra el código: describe la intención, no es la verdad.

## Propósito

Renombra, cambia de mayúsculas/minúsculas y aplana árboles de directorios. Hay dos programas: la CLI
`fsturbotransform`, con un panel en vivo en la terminal (barra de degradado y una mini-consola dibujada con
Sixel o Kitty o con caracteres ANSI), y la GUI QML `fsturbotransform-gui`. Ambos usan el mismo motor, que es
solo cabeceras.

## Partes y cómo se conectan

- **Motor** (`src/renamer.hpp`, `TransformerEngine`): recorre el árbol (`walk`, con filtros), aplana
  (`execute_flattening`) y renombra (`execute_renaming`, primero los más profundos). Se configura con
  `RenameOptions` y no conoce la terminal: informa a un `ProgressSink` (`src/progress_sink.hpp`) con
  `begin_phase`, `step`, `error` y `finish`, y consulta `cancelled`. En un dry run, la fase de renombrado ve el
  árbol tal como lo dejaría el aplanado (`dry_flattened_`, `dry_removed_dirs_`).
- **Nombres:** `CaseConverter` (estilos de mayúsculas) y `RegexSpec` (`src/matcher.hpp`):
  `/patrón/reemplazo/flags` sobre PCRE2 (`src/pcre2_regex.hpp`, biblioteca del sistema), con grupos
  `(?<n=>…)` y expresiones `\<n=>{js}` evaluadas por V8. V8 vive en `libfsturbo_js.so`, que se carga con
  `dlopen` solo cuando hace falta (`js_engine.hpp`, `js_bridge.cpp`).
- **Filtros** (`FilterMatcher`): `-e`/`-i` (glob o `/regex/`) y `.gitignore` con la semántica de git.
- **Front end de terminal:** `cli_parser.hpp` → `TerminalSink` (`terminal_sink.hpp`) → `LiveDashboard`
  (`dashboard.hpp`). Este dibuja con Sixel o Kitty (`sixel_renderer.hpp`, `graphics.hpp`, `ttf_font.hpp`:
  rasterizador TrueType propio) o con ANSI. `report_view.hpp` muestra el informe final y `terminal_probe.hpp`
  averigua qué soporta la terminal.
- **Formato de la consola** (`src/console_style.hpp`): un único archivo, `~/.config/fsturbotransform/console.conf`
  (o `$FSTURBO_CONSOLE_STYLE`, o `--console-style`), con `clave = valor`.
  - Contiene la fuente (cualquier TrueType monoespaciada que encuentre fontconfig; Fira Code incrustada por
    defecto), su escala, el título, los colores de la ventana, la barra y cada rol de la línea, y varios
    interruptores.
  - Los colores usan la notación colorspec de `plasmoids/shared`: hexadecimal, `r,g,b` o un JSON con
    `src`, `sys`, `l`, `c`, `h` y `a`. Los ajustes OKLCH están portados de `ColorSpecCore.js`.
  - Un color del sistema se guarda como rol. La CLI lo resuelve leyendo `kdeglobals` (`SystemColors`) y la
    GUI con Kirigami.
  - `style::active()` es la paleta que leen todos los renderizadores (`role_color`, `draw_console_window`,
    `draw_gradient_fill`, el informe).
- **GUI** (`gui/`): `TransformController` ejecuta el motor en un `std::jthread` y publica el progreso y el
  `ChangeModel`. `RegexHighlighter` usa `src/regex_tokens.hpp`, que divide el texto exactamente como el motor.
  `ConsoleFormat` (QML singleton) edita `console.conf`, lo guarda y lo vigila; `image://console` dibuja una
  vista previa con el renderizador de la terminal (`src/console_preview.hpp`). `ConsoleView.qml` es la lista
  de cambios dibujada con ese formato, y `ConsoleFormatDialog.qml` usa los controles compartidos.

## Entidades y claves

- `RenameOptions`: `target_dir`, `case_style`, `regex_rename`, `flatten`, `flatten_regex`, `dry_run`,
  `recursive`, `overwrite`, `filter` y `depth_origin` (la GUI cuenta `depth` desde el directorio elegido).
- `ExecutionStats`: los contadores del informe. `excluded_items` se cuenta una sola vez, en la pasada de
  renombrado.
- `style::kOptions`: la lista ordenada de claves de `console.conf`, con su tipo, valor por defecto y ayuda. El
  archivo, `--write-console-style`, `ConsoleFormat.values` y el diálogo salen de esa lista.

## Convenciones, invariantes y trampas

- C++26 con GCC (`#embed` para la fuente y el icono). Los motores son bibliotecas del sistema, nunca
  copiadas en el repositorio.
- La GUI se compila con `QT_NO_KEYWORDS`, porque el motor tiene funciones llamadas `emit`.
- `regex_tokens.hpp` debe dividir igual que `RegexSpec::parse` y `PatternRule::parse`: si cambias uno,
  cambia el otro.
- Controles compartidos (`gui/qml/controls`, `common`, `colorspec`, `gradientpicker/code`, `gpu`):
  - Son enlaces simbólicos a `plasmoids/shared` (el qrc embebe los archivos; un clon necesita `plasmoids/` al lado) y
    nunca se editan aquí. `gpu/` = primitivas de dibujo en la GPU que usan los de color (sin `Canvas`).
  - Se actualizan con `plasmoids/shared/sync.sh fsturbotransform` y después
    `python3 plasmoids/shared/tools/gen_icons.py gui/qml --also <iconos usados vía ColorSetting>`.
  - Se importan con espacio de nombres (`import "controls" as SC`), porque la raíz del módulo tiene sus
    propios `IconCheckBox`, `PropertyIcon` y demás.
  - Sus `i18n()` necesitan `KLocalization::setupLocalizedContext`.
- Los iconos propios de la GUI están en `gui/icons` y se sirven con `image://icon/<nombre>`. Los Breeze se
  recolorean con la paleta. Cada propiedad tiene su icono (primero Tango, luego Breeze; ver `SOURCES.md`).
- Con Kvantum como QStyle, `main.cpp` cambia a Breeze y restaura la paleta del esquema con
  `KColorScheme`. Si no, los campos salen oscuros y los iconos recoloreados, blancos.
- `ColorSources.css()` devuelve `rgba(…)`, que QML no acepta como color: hay que usar
  `Qt.rgba(c.r, c.g, c.b, c.a)`.
- Un `component` en línea no ve los ids de su archivo.
- Cada diálogo lleva la firma `AuthorSignature` abajo a la derecha. La página «Acerca de» está en `Main.qml`.

## Compilar, ejecutar y probar

- CLI: `make` (o CMake). GUI: `make gui`, o `cmake -B build && cmake --build build`.
- Tests:
  - `tests/regex.sh` (motor, filtros, expresiones, aplanado en dry run);
  - `python3 tests/dashboard.py` (geometría y píxeles de los frames, a través del emulador VT de `tests/vt.py`;
    ignora tu `console.conf`);
  - `ctest` (`regex_tokens`, `console_style` y `gui_controller`).
- Capturas de la GUI:
  `QT_QPA_PLATFORM=offscreen QT_QPA_PLATFORMTHEME=kde XDG_CONFIG_HOME=<aislado> fsturbotransform-gui --screenshot=x.png [--preview] [--console-format] <dir>`.
  Guarda una imagen por ventana visible: `x.png`, `x-1.png`, …

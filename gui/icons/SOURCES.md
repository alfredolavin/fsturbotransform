# Icon sources

Every property shown in the GUI has its own icon, looked up in this order: Inkscape's coloured
Tango theme, then KDE's Breeze, then the internet (none needed). Breeze's monochrome icons draw
with the `.ColorScheme-*` classes, which `gui/icon_provider.hpp` recolours from the palette.

| File | Used for | Source | Licence |
|---|---|---|---|
| app.png | window / launcher icon | `assets/icon.png` (this project), scaled to 256 px | project's own |
| target-folder.svg | target directory | Breeze `places/64/folder-open.svg` | LGPL-3.0-or-later (KDE e.V.) |
| browse.svg | browse for a folder / file | Breeze `actions/22/document-open-folder.svg` | LGPL-3.0-or-later |
| case-style.svg | case conversion | Breeze `actions/22/format-text-capitalize.svg` | LGPL-3.0-or-later |
| regex-rename.svg | -r regular expression | Breeze `actions/22/edit-find-replace.svg` | LGPL-3.0-or-later |
| file.svg | sample name, files scanned, file renames | Breeze `mimetypes/64/text-plain.svg` | LGPL-3.0-or-later |
| folder.svg | directories scanned, directory renames | Breeze `places/64/folder.svg` | LGPL-3.0-or-later |
| rename.svg | new name, rename phase, rename emblem | Breeze `actions/22/edit-rename.svg` | LGPL-3.0-or-later |
| flatten.svg | flatten, files flattened | Tango (Inkscape) `actions/layer-bottom.svg` | GPL-2.0-or-later (Inkscape) |
| flatten-regex.svg | --flatten-regex, filters | Breeze `actions/22/view-filter.svg` | LGPL-3.0-or-later |
| exclude.svg | -e patterns, excluded count | Breeze `actions/22/view-hidden.svg` (Tango `object-hidden` is unreadable at 16–22 px) | LGPL-3.0-or-later |
| include.svg | -i patterns | Tango (Inkscape) `actions/object-visible.svg` | GPL-2.0-or-later (Inkscape) |
| add.svg / remove.svg | add / remove a list entry | Breeze `actions/22/list-add.svg`, `list-remove.svg` (Tango's symbolic ones vanish on dark themes) | LGPL-3.0-or-later |
| gitignore-file.svg | --gitignore files | Breeze `preferences/32/preferences-git.svg` | LGPL-3.0-or-later |
| target-gitignore.svg | the target's own .gitignore | Breeze `places/64/folder-git.svg` | LGPL-3.0-or-later |
| recursive.svg | recurse into subdirectories | Tango (Inkscape) `actions/distribute-graph-directed.svg` | GPL-2.0-or-later (Inkscape) |
| overwrite.svg | --overwrite | Breeze `actions/22/document-replace.svg` | LGPL-3.0-or-later |
| preview.svg | preview (dry run), change list | Breeze `actions/22/document-preview.svg` | LGPL-3.0-or-later |
| apply.svg | apply | Breeze `actions/22/dialog-ok-apply.svg` | LGPL-3.0-or-later |
| stop.svg | stop / cancel | Breeze `actions/22/process-stop.svg` | LGPL-3.0-or-later |
| error.svg | errors | Breeze `status/64/dialog-error.svg` | LGPL-3.0-or-later |
| warning.svg | warnings | Breeze `status/64/dialog-warning.svg` | LGPL-3.0-or-later |
| duration.svg | run duration | Breeze `actions/22/chronometer.svg` | LGPL-3.0-or-later |
| info.svg | expression syntax help | Tango (Inkscape) `actions/info.svg` | GPL-2.0-or-later (Inkscape) |

Tango: `/snap/inkscape/current/share/inkscape/icons/Tango/scalable/actions/` (Inkscape 1.x snap).
Breeze: `/usr/share/icons/breeze/` (kf6-breeze-icon-theme).

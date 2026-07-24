# Editor Guide

The editor workspace has three main areas:

1. **File tree** (left) — browse and open project files
2. **Source editor** (center) — GtkSourceView with syntax highlighting
3. **Tools bar** — save, format, and edit helpers

## File tree

- Click a file to open it in the editor.
- Folders expand and collapse.
- The current project root is shown at the top.

## Formatting tools

| Action | What it does |
|--------|----------------|
| Format Document | Normalize indentation and trim trailing whitespace |
| Format Selection | Format only the selected lines |
| Indent Lines | Increase indent on selected (or current) lines |
| Unindent Lines | Decrease indent on selected (or current) lines |
| Trim Trailing Space | Remove spaces/tabs at end of each line |
| Ensure Final Newline | Guarantee the file ends with a newline |
| Sort Selected Lines | Sort the selected lines alphabetically |
| Toggle Comment | Comment / uncomment C/C++ lines with `//` |

Tab width and spaces-vs-tabs are controlled from **Preferences**
(or GSettings keys `tab-width` / `insert-spaces`).

## Keyboard shortcuts

- `Ctrl+S` — Save
- `Ctrl+Shift+F` — Format document
- `Ctrl+]` — Indent
- `Ctrl+[` — Unindent
- `Ctrl+W` — Close current file / return home
- `Ctrl+Q` — Quit

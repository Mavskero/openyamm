# Keyboard binding conflicts 203

Keyboard and LMB changes now swap a single conflicting action atomically. Ambiguous historical duplicates are preserved with a visible retry prompt; selecting an unused key or Defaults can repair them. INI parsing and format are unchanged. The legacy M map shortcut yields to explicitly assigned actions in both journal input paths.

The controlled Keyboard flow observed Left=D/Right=A swapping, INI persistence, Forward=M without opening the map, and ambiguous conflict feedback. The first run stopped at an early button-release assertion; a focused follow-up passed Defaults and page2 without repeating the successful branches. Details and limitations are in ../ui-function-completion-204/scoped-result.json. No device or campaign acceptance.

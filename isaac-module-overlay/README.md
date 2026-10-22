# Isaac Module Snapshot Overlay

Lector de diagnóstico de solo lectura para Tesla. Se abre encima de Isaac, sin
usar Homebrew Menu ni Álbum. Verifica el Title ID, los Build IDs completos del
loader y de `Repentance.nrs`, y cuatro ventanas de código de 16 bytes. No instala
hooks ni escribe en la memoria del juego.

Instalación: copia `IsaacModuleSnapshot.ovl` a `sd:/switch/.overlays/`. Arranca
Isaac, abre Tesla y selecciona **Isaac Module Snapshot**. El resultado aparece en
pantalla y se guarda en:

`sd:/switch/TheModdingOfIsaac/module-overlay.log`

Si existe el archivo de trucos exacto
`sd:/atmosphere/contents/010021c000b6a000/cheats/b6e5bdb9dc12e1d1.txt`, el lector
se detiene antes de adjuntar el depurador. Una coincidencia de firmas valida esas
ventanas concretas; todavía no demuestra la ABI ni autoriza por sí sola un hook.


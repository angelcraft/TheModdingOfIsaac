# IsaacModuleSnapshot

Diagnóstico independiente para observar los módulos cargados por Isaac en la
Switch. Genera un archivo de texto; todavía no ejecuta el mod ni expone la API de
Isaac.

## Usarlo en la Switch

1. Copia el NRO compilado a
   sd:/switch/TheModdingOfIsaac/IsaacModuleSnapshot.nro.
2. Inicia Isaac y espera a su menú principal.
3. Pulsa HOME, abre Álbum para entrar al Homebrew Menu en modo applet y ejecuta
   IsaacModuleSnapshot. Mantén Isaac abierto; no uses otro juego con R para
   iniciar el Homebrew Menu.
4. Espera al resultado y pulsa PLUS para salir. Regresa al juego normalmente.
5. Comparte sd:/switch/TheModdingOfIsaac/module-snapshot.log.

La prueba sobrescribe solamente ese archivo de diagnóstico y crea sus
directorios si faltan. Si aparece un error, el archivo conserva el motivo y los
datos parciales obtenidos.

## Límites y comprobaciones

Requiere Atmosphère con acceso a dmnt:cht, pm:dmnt, pm:info y ldr:dmnt.
Comprueba el proceso de aplicación, el título 010021C000B6A000 y el Build ID
completo del ejecutable principal:

    B6E5BDB9DC12E1D1A25CBFDA17F4BE24B4754EF5000000000000000000000000

La identidad del ejecutable se consulta a loader antes de intentar adjuntar un
depurador. Si Atmosphère ya está adjunto, se usa esa conexión existente para
leer. Si no lo está, la prueba permite ForceOpen únicamente cuando stat sobre
el archivo de trucos de este título y build devuelve ENOENT:

    sd:/atmosphere/contents/010021c000b6a000/cheats/b6e5bdb9dc12e1d1.txt

Un archivo presente o cualquier otro error bloquea esa operación. ForceOpen de
Atmosphère también carga los trucos guardados; comprobar primero su ausencia
evita activarlos como efecto de este diagnóstico. La prueba cierra sus
conexiones cliente al terminar y conserva cualquier adjunto de depuración;
no fuerza su cierre, porque hacerlo altera el estado global de trucos y puede
guardar sus opciones.

Enumera hasta 4096 regiones, consulta sus permisos y lee como máximo los primeros
128 bytes de cada región ejecutable legible. Registra direcciones, permisos,
bytes del encabezado y el Build ID completo cuando aparece NRO0. Comprueba de
nuevo el proceso, el build y la cantidad de regiones al terminar. Las lecturas
son acotadas, pero no forman una captura atómica: el juego no se pausa mediante
el depurador. La ausencia de NRO0 no demuestra que un NRO no esté cargado.

Si reconoce el Build ID completo del NRO Repentance
91C73FDD575061318D68886316AFEAC72388B2AB000000000000000000000000,
consulta por separado los permisos de cuatro ventanas de código de 16 bytes:
Manager::Update (+0x3F8DB8), Game::Update (+0x351884), Room::Update
(+0x46D788) y EntityList::Update (+0x767C0). Registra los bytes y si coinciden
con las firmas del archivo identificado por SHA-256
CC363208C220F65547EDDA6D8268B3B6C0F7373B14C91AE6FFB378018DE4B66A.
Una coincidencia comprueba esa ventana concreta; por sí sola no verifica el
archivo completo, la ABI ni la seguridad de instalar un hook.

También intenta leer 8 bytes del GOT del ejecutable principal en +0x10CC78.
Anota el puntero de run_repentance y el candidato de base al restarle 0x4C0840.
Ese candidato se marca como no verificado y nunca se usa para llamar código.

No instala hooks ni parches y no incluye comandos de escritura de memoria,
pausa/reanudación del depurador, cierre forzado del adjunto ni cambio de trucos.

## Compilar

El Makefile parte de la plantilla oficial de aplicación de switch-examples.
En un entorno que ya tenga devkitA64 y libnx:

    make

Produce IsaacModuleSnapshot.nro. La compilación para este proyecto se prepara
en GitHub Actions, sin instalar herramientas adicionales en el PC del usuario.
La ejecución correcta en una Switch sigue pendiente de prueba.

## Fuentes primarias

- [Plantilla oficial de aplicación, switch-examples
  669786898205b7beb25ff1731e72982e6d0397d3](https://github.com/switchbrew/switch-examples/blob/669786898205b7beb25ff1731e72982e6d0397d3/templates/application/Makefile).
- [Cliente dmnt:cht de Atmosphère
  6b0c077b4c608a3a2ee712b8dc35fee87ceb62e8](https://github.com/Atmosphere-NX/Atmosphere/blob/6b0c077b4c608a3a2ee712b8dc35fee87ceb62e8/libraries/libstratosphere/source/dmnt/dmntcht.os.horizon.c).
  Los tipos y envoltorios IPC se adaptan de esta implementación, GPL-2.0.
- [Semántica del servidor de trucos, mismo commit](https://github.com/Atmosphere-NX/Atmosphere/blob/6b0c077b4c608a3a2ee712b8dc35fee87ceb62e8/stratosphere/dmnt/source/cheat/impl/dmnt_cheat_api.cpp):
  adjuntar en líneas 783–887, leer/consultar memoria en 353–426 y ejecutar
  trucos en 739–772.
- [Servicios de proceso de libnx
  b156b4774fb2df7b93402169509266da7547fc4c](https://github.com/switchbrew/libnx/blob/b156b4774fb2df7b93402169509266da7547fc4c/nx/include/switch/services/pm.h).
- [Identidad de módulos, mismo libnx](https://github.com/switchbrew/libnx/blob/b156b4774fb2df7b93402169509266da7547fc4c/nx/include/switch/services/ldr.h).
- [Encabezados NRO, mismo libnx](https://github.com/switchbrew/libnx/blob/b156b4774fb2df7b93402169509266da7547fc4c/nx/include/switch/nro.h).

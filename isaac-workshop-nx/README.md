# Isaac Workshop NX — prototipo 0.1 (no probado en Switch)

Gestor independiente de mods de **The Binding of Isaac: Repentance** para
Nintendo Switch con Atmosphère. Código fuente incluido, sin mods de terceros,
sin archivos del juego y sin credenciales de Steam. No sustituye al runtime.

## Qué hace

- Consulta el título y la disponibilidad de un ID o enlace de Steam Workshop
  a través del endpoint público oficial `GetPublishedFileDetails`.
- Descarga **únicamente** si Steam ofrece una URL HTTPS pública con un ZIP.
  **La mayoría de mods de Workshop no exponen esa URL:** no hay SteamCMD,
  autenticación de Steam/Steam Guard ni descarga universal en esta versión.
- Instala ZIPs de mods colocados en `sdmc:/switch/isaac_workshop_nx/inbox/`.
  Admite un ZIP con `main.lua` y `metadata.xml` en la raíz o dentro de una
  única carpeta envoltorio. Para instalar otros formatos, descomprímelos
  primero fuera de la consola y prepara un ZIP normal.
- Detecta automáticamente el destino existente que usa `isaac-mod-runtime`:
  1. `sdmc:/atmosphere/contents/010021C000B6A000/romfs/isaac_mods/mods/`
     (ruta confirmada por Phylos).
  2. `sdmc:/isaac_mods/mods/` (otra opción del runtime nuevo).
- Si ambas rutas existen, **bloquea la instalación**: el runtime nuevo puede
  elegir la segunda aunque el usuario esté usando la primera. No se borra ni
  mueve ninguna de ellas hasta aclararlo.
- No crea por su cuenta la primera ruta, porque activarla ocultaría el árbol
  de mods del overlay anterior. No sobrescribe carpetas existentes.
- Crea staging bajo la carpeta elegida y lo mueve solo cuando se validó todo.
  Rechaza rutas `../`, symlinks ZIP, CRC erróneo, rutas duplicadas, cifrado,
  ZIP64 y ficheros/ZIPs demasiado grandes. Límite: 32 MiB comprimido,
  96 MiB descomprimido, 1.500 entradas.

## Instalar en Switch (una vez compilado el NRO)

1. Instala primero un `isaac-mod-runtime` compatible con tu juego (versión
   de juego **1.7.9b**, compilación soportada por su autor). Verifica que
   el juego puede leer la carpeta `mods` antes de usar esta app.
2. Cierra Isaac **por completo**; no lo dejes suspendido en el HOME.
3. Copia `IsaacWorkshopNX.nro` a `sdmc:/switch/isaac_workshop_nx/`.
4. Desde HBMenu en modo aplicación, abre el NRO. El botón `X` muestra el
   teclado nativo para pegar un ID o URL de Workshop. Si `file_url` está vacío,
   el mensaje indicará que requiere importación de ZIP.
5. Para instalar un ZIP desde Android, transfiérelo a la carpeta `inbox`
   (p. ej., mediante tu `ftpd pro`). Selecciónalo con arriba/abajo y pulsa `A`.
6. Reabre Isaac y gestiona los mods desde el menú del runtime.

> **Importante:** En esta consola el teclado se congelaba con `dns.mitm`
> activo. Esta app usa el teclado nativo; si el fallo reaparece, no asumas
> que es un error de Isaac Workshop NX. Respeta tu configuración de protección
> de red y no desactives bloqueos de servidores sin comprobar las consecuencias.

## Compilar

Requiere `devkitPro`, `devkitA64`, `libnx`, `switch-curl`, `switch-zlib` y
las herramientas `nacptool` y `elf2nro`. Con ese entorno:

```sh
make
```

El proyecto también incluye `.github/workflows/build.yml` con compilación
en el contenedor oficial `devkitpro/devkita64`, para obtener el archivo NRO
desde GitHub Actions, sin ordenador local, **una vez subido a un repositorio**.
No se ha ejecutado ese workflow ni instalado el resultado en hardware.

Para ejecutar las pruebas de seguridad y metadatos sin SDK de Switch:

```sh
make host-test
```

## Seguridad y límites

- No pide usuario, contraseña, cookies ni códigos de Steam Guard.
- Solo usa HTTPS con certificado validado y un máximo de tres redirecciones.
- Los mods pueden ejecutar Lua y recursos del juego: importa solo contenido
  de autores de confianza. La instalación correcta no garantiza compatibilidad
  con el runtime. Mods REPENTOGON o Repentance+ pueden necesitar APIs ausentes.
- No modifica `subsdk`, `main.npdm`, `4IFIR`, `Latitude` ni módulos de Atmosphère.
- No sobrescribe datos previos; al actualizar un mod, retira tú la versión
  anterior **después** de guardar una copia si lo deseas.

**Estado real de la versión 0.1:** código y pruebas host. No hay `.nro`
compilado/validado aquí porque este entorno no tiene devkitA64. La descarga
de Steam Workshop requiere resolver la distribución de contenido protegida
por el cliente Steam para ser universal; un login web en Switch, por sí solo,
no proporciona acceso a las APIs nativas de descarga.

Fuente de API: https://partner.steamgames.com/doc/webapi/ISteamRemoteStorage

Runtime: https://github.com/granule256/isaac-mod-runtime

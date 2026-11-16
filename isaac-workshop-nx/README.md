# Isaac Workshop NX — 0.3 (buscador y puente Android experimentales)

Gestor independiente de mods de **The Binding of Isaac: Repentance** para
Nintendo Switch con Atmosphère. Código fuente incluido, sin mods de terceros,
sin archivos del juego y sin credenciales de Steam. No sustituye al runtime.

## Qué hace

- **Nuevo en 0.3:** desde la ficha de un mod pulsa `L` para recibir el ZIP
  preparado por tu Android, e instalarlo en Granule usando el mismo validador
  y carpeta de staging. Requiere el puente emparejado en la Wi-Fi local. **La
  Switch no contiene ni solicita contrasenas/cookies de Steam.**
- Companion Android/Termux en `android/isaac_workshop_bridge.py`: invoca el
  proyecto SteamRE DepotDownloader (`-app 250900 -pubfile ID`) con login
  interactivo/Steam Guard, prepara un ZIP y opcionalmente lo sirve por Wi-Fi.
  No incluye ni redistribuye mods de terceros.
- **Nuevo en 0.2:** pulsa `R` y busca mods por nombre. Lee la pagina publica
  de busqueda de Steam Workshop, obtiene un maximo de 20 IDs y verifica sus
  datos en el Web API oficial. Navega con arriba/abajo, pulsa `A` para ver
  titulo, ID, descripcion y disponibilidad de descarga. Pulsa `B` para volver.
  La interfaz aun es de texto: los metadatos incluyen una URL de imagen,
  pero **no muestra miniaturas**. Si Steam cambia el HTML, requiere ajuste.
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
4. Desde HBMenu en modo aplicación, abre el NRO. Pulsa `R` para buscar por
   nombre o `X` para pegar un ID/URL de Workshop. El boton `A` abre la ficha
   de un resultado. Si `file_url` esta vacio, la aplicacion indica que requiere
   importacion de ZIP; **no simula haber descargado archivos**.
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

En `angelcraft/TheModdingOfIsaac`, la rama privada
`feature/isaac-workshop-nx-20261116` contiene el workflow
`.github/workflows/build-isaac-workshop-nx.yml`. GitHub Actions compila el NRO
en `devkitpro/devkita64` y lo publica como artefacto descargable. El usuario
confirmo que **0.1 arranco en Switch y reconocio el mod Candy Pills ULTIMATE**;
la version 0.2 sigue pendiente de comprobacion en hardware.

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

**Estado real de 0.3:** 37 pruebas host y comprobacion sintactica del codigo de
la consola superadas. El protocolo del puente se ha probado entre procesos
locales, **no todavia con dos dispositivos reales ni con Steam autenticado**.
No se ha verificado aun el NRO compilado de esta revision. La API oficial `QueryFiles`
requiere clave Steamworks; como alternativa, la busqueda publica de Steam
por HTML no necesita introducir contrasena, pero puede fallar por cambios de
su sitio o limitaciones de acceso. La descarga autenticada universal necesita
un cliente del protocolo de Steam, permisos adecuados y mas investigacion;
**no hay inicio de sesion Steam ni SteamCMD integrados en Horizon**. El login
se realiza por separado, en DepotDownloader desde Android.

## Descargar mods de Steam con Android (sin PC)

Una vez instalado **Termux** desde F-Droid o su GitHub oficial, en el Android:

```sh
pkg install tur-repo
pkg install depotdownloader python
termux-setup-storage
```

Descarga el archivo `android/isaac_workshop_bridge.py` del repositorio privado
a la carpeta Download del telefono y copialo a Termux:

```sh
cp /sdcard/Download/isaac_workshop_bridge.py "$PREFIX/../home/isaac_workshop_bridge.py"
```

Para descargar un mod de Workshop cuyo ID conozcas (ejemplo de sintaxis):

```sh
python "$PREFIX/../home/isaac_workshop_bridge.py" fetch 836319872 --username TU_USUARIO_STEAM
```

DepotDownloader gestiona el inicio de sesion de Steam interactivamente. Nunca
incluyas tu contrasena o codigo Steam Guard en el comando, ni los compartas
con terceros. El programa puede almacenar sus propios tokens de inicio de
sesion en el entorno de Termux: trata ese perfil como confidencial. Steam
puede exigir que tu cuenta tenga licencia del juego, y no todos los mods o
formatos funcionan. El ZIP valido quedara en:

`/sdcard/Download/IsaacWorkshopNX/steam_<ID>.zip`

**Opcion sencilla:** copia ese ZIP por FTPD Pro a la carpeta `inbox` de la
Switch, como en 0.1, y pulsa `A` para instalarlo.

**Opcion puente Wi-Fi, sin copiar cada ZIP:**

1. Con Android y Switch en **la misma Wi-Fi privada**, ejecuta:

   ```sh
   python "$PREFIX/../home/isaac_workshop_bridge.py" serve
   ```

2. El comando crea `/sdcard/Download/IsaacWorkshopNX/bridge.txt` con una
   direccion privada y un codigo de emparejamiento generado aleatoriamente.
3. Por FTPD Pro, copia ese `bridge.txt` una sola vez a
   `sdmc:/switch/isaac_workshop_nx/bridge.txt`.
4. Deja el servidor abierto en Termux. En la Switch, busca el mod, abre su
   ficha con `A`, pulsa **L** y confirma la descarga e instalacion. El ZIP debe
   haber sido descargado y empaquetado en el Android previamente.

**Modo experimental, descarga automatica desde la Switch:** en vez de `serve`
sin argumentos, usa:

```sh
python "$PREFIX/../home/isaac_workshop_bridge.py" serve --auto-fetch --username TU_USUARIO_STEAM
```

Con ese modo, si pides un mod que aun no esta en el Android, el puente intenta
invocar DepotDownloader por su ID y servir el ZIP cuando termine. La Switch
esperara hasta unos 10 minutos; si Steam pide autenticacion, atiende el aviso
en la terminal de Android. Es recomendable ejecutar antes un `fetch` manual
para comprobar la cuenta y sus permisos de Workshop. No se ha comprobado aun
esta ruta con Steam real en un Android y podria necesitar adaptaciones.

El puente solo acepta IPv4 privadas (`10.x`, `172.16-31.x`, `192.168.x`),
puerto **8765**, codigo de emparejamiento y ZIPs con nombre validado. La app
Switch no sigue redirecciones ni usa proxies al recibir el ZIP. El servidor
no permite listar, borrar ni subir archivos; solo sirve ZIPs ya preparados.
**El trafico HTTP local no va cifrado:** utilizalo exclusivamente en redes
Wi-Fi de confianza y no compartas `bridge.txt`. No expongas ni redirijas el
puerto 8765 a Internet.

Si cambias de Wi-Fi o IP, vuelve a copiar el nuevo `bridge.txt` del Android.
No es necesario cambiar la configuracion de Atmosphere, 4IFIR o Latitude.

Fuente de API: https://partner.steamgames.com/doc/webapi/ISteamRemoteStorage

API de busqueda con clave: https://partner.steamgames.com/doc/webapi/IPublishedFileService

Runtime: https://github.com/granule256/isaac-mod-runtime

Descargador SteamRE: https://github.com/SteamRE/DepotDownloader

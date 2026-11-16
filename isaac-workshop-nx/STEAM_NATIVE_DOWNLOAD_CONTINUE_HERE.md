# Steam nativo en Switch: continuar aquí (2026-11-16)

## Objetivo
Descargar mods legales de Steam Workshop de Isaac (AppID 250900) directamente
a Switch, sin PC ni Android, validar contenido y pasarlo al instalador existente.
No modificar Granule, game exefs, saves, Atmosphere ni configuraciones.

## Estado confirmado
- 0.4: el usuario escaneó el QR y la Switch indicó "Steam Guard confirmado".
  Esto confirma el handshake de autenticación de su prueba, NO acceso al CDN.
  Las credenciales se borran y no se almacenan.
- Consulta por ID y búsqueda pública mediante GetPublishedFileDetails.
- Los archivos ZIP locales funcionan; ruta del usuario:
  sdmc:/atmosphere/contents/010021C000B6A000/romfs/isaac_mods/mods/
- Termux + DepotDownloader descargó y empaquetó correctamente 836319872.
  El puente Wi-Fi dio HTTP 0 (red / IP pendientes), pero NO es el objetivo final.
- Esta revisión: el parser público detecta el campo `hcontent_file`,
  lo distingue de `file_url` y lo expone como identificador de manifiesto
  SteamPipe; no utiliza el ID como descarga HTTPS inventada.
  Caso `file_type=2` (colección) no se intenta descargar como un archivo.

## Ruta real confirmada en código de SteamRE
1. `CPublishedFile_GetDetails` devuelve metadatos de contenido; si no tiene
   `file_url`, `hcontent_file` identifica el manifiesto para SteamPipe.
2. SteamKit2 se conecta al servidor Steam CM e inicia sesión con el
   `refresh_token` del QR; antes de completar esa conexión NO hay
   cliente de contenidos autorizado. No persistir ni imprimir tokens.
3. Consultar datos del juego, licencia y `workshopdepot`; solicitar
   clave del depot y código temporal de petición de manifiesto.
4. Acceder al CDN autorizado; descargar manifiesto y fragmentos, verificar
   hash/tamaño y descomprimir dentro de staging limitado; validar archivos
   main.lua y metadata.xml, no escribir directo en mods.
5. Instalar con el instalador ya existente sin sobrescribir mods previos.

Fuente primaria: https://github.com/SteamRE/DepotDownloader/blob/master/DepotDownloader/ContentDownloader.cs
Flujo principal: ProcessPublishedFileAsync -> DownloadPubfileAsync ->
DownloadAppAsync -> GetDepotInfo -> DownloadSteam3Async.
Steam3Session.cs: QR, SteamClient, publicación, clave/código/CDN token.
Protobufs: https://github.com/SteamTracking/Protobufs
SteamKit: https://github.com/SteamRE/SteamKit

## Capa de protocolo ya implementada
Se añadió \`cm_wire.hpp/.cpp\` para codificar/decodificar las tramas
protobuf de Steam CM (EMsg con flag, longitud de cabecera, cabecera y cuerpo).
La capa no tiene acceso a credenciales ni red, acota tamaños y tiene tests
con trama canónica, errores y entradas malformadas aleatorias. Aún falta
implementar WebSocket TLS, protobuf de mensajes y gestión de sesión.

## Próxima tarea de implementación
Crear un cliente **Steam CM** portátil para Horizon. Antes de acceder a
contenido o guardar una sesión:
- comprobar disponibilidad de WebSocket TLS en devkitPro; usar validación CA
  estricta y origen Steam controlado;
- implementar/validar framing y mensajes protobuf usando definiciones abiertas;
- logon efímero con el token emitido por QR, borrado seguro al salir;
- comprobar permiso real del AppID y acceso al workshop depot;
- lograr un solo manifiesto + chunks correctos y verificar hashes en staging.
La conexión debe registrar solamente etapa, EResult, bytes y errores saneados,
NUNCA tokens, request IDs, contraseñas, challenge URLs activas o claves de depot.

## Pruebas y entrega
`make host-test` en Linux, `make` con devkitPro ARM64 en Actions.
En hardware, primero solo una descarga de prueba sin tocar la carpeta de mods;
el instalador se habilita después de validar firma/formato y límites.
No distribuir mods ni archivos propietarios; código en el repositorio privado.
No anunciar "descarga nativa completa" hasta observar bytes, hash y archivos.

Las salidas de CI con nombre NO-NATIVE-DOWNLOAD son diagnósticas:
no sustituyen un cliente Steam completo.

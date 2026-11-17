# Convert

Primera versión de una aplicación de escritorio en **Qt 6.7 / C++17**, con
apariencia basada en la interfaz actual de Radit. El código es nuevo; no se
ha copiado código del repositorio Spotify-Downloader ni de Radit.

## Funciones implementadas

- Arrastrar archivos y enlaces directamente a la lista.
- Pegar URL de YouTube y Spotify sobre la lista con **Ctrl+V** o **botón derecho > Pegar**.
- Conversión con FFmpeg a MP4, MKV, WebM, MP3, WAV, FLAC, M4A, Opus y Ogg.
- **Audio…** configura los valores de las nuevas entradas de audio: tasa de bits
  de 64 a 320 kb/s, frecuencia de muestreo y canales originales, mono o estéreo.
  WAV permite 16, 24 o 32 bits; FLAC, 16 o 24 bits. MP3 admite muestreo
  automático, 32000, 44100 o 48000 Hz; Opus utiliza 48000 Hz.
- **Botón derecho > Opciones de audio…** cambia esos valores para una entrada
  o varias del mismo formato. Los valores se muestran al dejar el puntero sobre
  su celda Formato. Los ajustes generales se conservan entre sesiones; los
  individuales se conservan si está activada la opción de recordar la cola.
  Se aplican a archivos locales y descargas de YouTube y Spotify. Las opciones
  de audio están desactivadas para salidas de vídeo.
- Enlaces YouTube con yt-dlp, incluidos enlaces de listas.
- El registro de YouTube muestra la extracción y el archivo terminado, con
  salida de Python sin almacenamiento intermedio. Para salidas de audio de una
  playlist, primero se descargan los archivos originales de toda la entrada y
  después se convierten al formato elegido y se guardan en el destino. El
  registro anuncia estas fases para distinguir la descarga de la conversión.
- Canciones, álbumes y listas públicas de Spotify mediante **spotDL externo**.
- Cada playlist ocupa una sola entrada. Pegar el enlace no consulta ni enumera
  sus canciones; **Iniciar cola** lo envía directamente a la herramienta de
  descarga con el formato y las opciones elegidos. No hay contador de canciones,
  expansión en filas ni límite de tres minutos para consultar la playlist.
  Spotify procesa las canciones de una en una. Cuando confirma una canción
  terminada, Convert la copia en segundo plano a la salida con el nombre
  `Artista - Título.ext`, sin identificador. Si existe, añade ` (2)`, etc.
  Las canciones ya copiadas permanecen en el destino aunque falle otra posterior.
  Cada copia se comprueba leyendo el destino y comparando su SHA-256 con el
  contenido de origen; entonces se borra su archivo temporal local. Si la copia
  o su verificación fallan, se conserva el original temporal. Si el borrado no
  es posible, se avisa en el registro sin volver a copiar la canción.
  Si todas se verifican y guardan, se retira la entrada; si falla alguna, queda
  con error y se conserva la descarga parcial local. El registro muestra las
  rutas. Si el nombre de una notificación no permite identificar un archivo de
  forma inequívoca, su copia se pospone a la verificación final.
- Las filas se numeran desde 1. La barra inferior muestra entradas actuales,
  pendientes, completadas, errores y el total de la sesión (actuales más
  completadas). Una playlist cuenta como una entrada, no como sus canciones.
- Archivos locales: salida en la carpeta de origen de cada archivo.
- Enlaces: salida en la carpeta de descargas seleccionada.
- Formato individual: pulsar la celda **Formato** de una fila y elegir en su
  desplegable. Spotify ofrece únicamente formatos de audio. También admite F2.
- Para cambiar el formato de varias entradas, seleccionarlas y pulsar
  **Aplicar a selección**. Ese botón aplica también las opciones generales de
  audio y la carpeta **Salida URL**
  a todas las entradas seleccionadas, incluidos los archivos locales.
- Cola secuencial, progreso, registro, cancelación, reintento y apertura del destino.
- **Borrar registro**, encima del log o en su menú del botón derecho, vacía el
  texto mostrado. Puede usarse durante una descarga o conversión; los mensajes
  posteriores continúan apareciendo y la cola y los archivos no se modifican.
- Preferencias de herramientas y restauración opcional de la cola entre sesiones.
- Los trabajos completados correctamente se retiran automáticamente de la lista.
  El resultado permanece en disco y su ubicación queda en el registro.
- **Botón derecho > Borrar lista** vacía toda la cola cuando está detenida,
  conservando los archivos originales y las salidas. La lista se guarda vacía.

Los controles de edición se desactivan durante el procesamiento. **Detener cola**
cancela el trabajo actual y deja los siguientes pendientes. Las entradas
canceladas y fallidas requieren **Reintentar selección** antes de iniciar la cola.
Cambiar su formato también las deja pendientes para un nuevo intento.
Por defecto, el programa abre con la lista vacía. Conserva las rutas, el formato
general y las preferencias, pero no recupera archivos ni URL anteriores. Para
conservar entradas pendientes, activar **Preferencias > Recordar la cola entre
sesiones**. Al recuperar una sesión con esa opción, las entradas que estaban
procesándose quedan canceladas; no se inicia ninguna descarga automáticamente.

Cada archivo arrastrado se convierte en su propia carpeta de origen, incluso
si se añaden archivos de carpetas distintas en una misma operación. Ese es el
destino predeterminado y los originales se conservan. Para utilizar en cambio
la carpeta **Salida URL**, seleccionar los archivos y pulsar **Aplicar a selección**;
el cambio se conserva al cerrar y volver a abrir si está activada la opción de
recordar la cola.
Para elegir otra ubicación en una entrada concreta, usar **botón derecho >
Cambiar carpeta de salida…**. También sirve para varias filas seleccionadas.
El cambio deja las entradas pendientes y se conserva entre sesiones cuando se
activa la opción de recordar la cola.
Esto permite elegir un destino alternativo si la carpeta de origen no admite
escritura, sin cambiar el comportamiento predeterminado del resto de archivos.

Para YouTube y Spotify, la carpeta de descargas predeterminada de una
configuración nueva es `salida/` junto a `convert.exe` (`out/bin/salida`).
Una carpeta guardada anteriormente se conserva. Al cambiar el destino de enlaces
que ya están en la lista, hay que seleccionarlos y pulsar **Aplicar a selección**;
el campo superior por sí solo solo cambia el destino de las URL nuevas.
Se aceptan rutas pegadas entre
comillas y direcciones locales `file:///`; las rutas relativas se resuelven
respecto al ejecutable. El registro muestra la ruta utilizada y se comprueba que
el destino admite escritura antes de iniciar las herramientas.

La lista recibe el foco al abrir el programa para poder pegar URL directamente.
También permite varias URL separadas por espacios o saltos de línea. Los controles
de pegado y arrastre se desactivan mientras se procesa la cola. Si se ha activado
la restauración de la cola, los archivos locales no completados utilizan su carpeta de origen salvo
que se haya elegido explícitamente otro destino.
Los trabajos completados guardados por versiones anteriores se omiten al
restaurar. Los errores y cancelados permanecen en la lista hasta que se reintenten
o se quiten manualmente. El contador de completados corresponde a la sesión actual.

## Compilar en Windows

Kit elegido: **Qt oficial 6.7.0 MinGW 64 bits**, con MinGW 11.2.0 de esa
instalación. No utilizar herramientas ni bibliotecas de MSYS2 con este kit.

### Abrir con qmake en Qt Creator

1. Cerrar el proyecto CMake abierto mediante **Archivo > Cerrar proyecto**.
2. Abrir **Archivo > Abrir archivo o proyecto** y seleccionar **`convert.pro`**.
3. Elegir **Desktop Qt 6.7.0 MinGW 64-bit**. El kit debe tener asignado el
   compilador C++ `C:/Qt/Tools/mingw1120_64/bin/g++.exe`.
4. Activar la compilación fuera del directorio fuente (shadow build) y utilizar,
   por ejemplo, `out/creator-qmake-qt67-release` como directorio de compilación.
5. Pulsar **Configurar proyecto**, **Ctrl+B** para compilar y el botón **Ejecutar**.

`convert.pro` incluye todas las fuentes, cabeceras y recursos. qmake genera
automáticamente las reglas de moc y rcc. El ejecutable se genera en `out/bin`.
No es necesario convertir ni modificar las clases C++ para utilizar qmake.
Un kit sin compilador C++ asignado también debe corregirse para usar qmake.

### Compilar desde PowerShell

Desde PowerShell en la raíz del proyecto:

```powershell
.\build.ps1 -Deploy
.\out\bin\convert.exe
```

El script ejecuta qmake, compila en `out/qmake-qt67-mingw-release` y ejecuta el
`windeployqt` del mismo kit. No instala ni descarga programas externos.
Si tus rutas difieren, admite `-QtRoot` y `-MinGWRoot`.
Para compilar sin copiar las DLL Qt: `./build.ps1`.
El archivo `CMakeLists.txt` se conserva como alternativa; el flujo principal
solicitado ahora utiliza `convert.pro` y no depende de CMake ni de JOM.

En Windows, la compilación ejecuta `prepare-runtime.ps1` para restablecer la
integridad **media**, que es el nivel normal del usuario, en `convert.exe` y los
ejecutables de `tools/` (y las carpetas antiguas `bin/` y `ffmpeg/`). Esto evita que una etiqueta de
integridad baja heredada de la carpeta de compilación haga arrancar Convert o
sus herramientas sin permiso para escribir en carpetas de música normales.
También establece ese nivel en `out/bin` y las carpetas de herramientas para
que los ejecutables nuevos lo hereden cuando el enlazador los sustituya.
El ajuste no modifica los permisos de esas carpetas ni concede privilegios de
administrador. También se puede aplicar sin recompilar:

```powershell
.\prepare-runtime.ps1
```

Después hay que cerrar y volver a abrir Convert, porque un proceso que ya está
en ejecución conserva su nivel anterior. Si se incorpora esta corrección a un
proyecto abierto en Qt Creator, ejecutar **qmake** para actualizar el paso de
poscompilación. Referencia: [integridad obligatoria de Windows](https://learn.microsoft.com/en-us/windows/win32/secauthz/mandatory-integrity-control).

## Herramientas de ejecución

Instalar por separado desde sus proyectos oficiales:

| Herramienta | Uso | Ubicación automática junto al ejecutable |
| --- | --- | --- |
| [FFmpeg](https://ffmpeg.org/download.html) | Conversión y posprocesamiento | `tools/ffmpeg.exe` |
| ffprobe (misma distribución FFmpeg) | Duración y progreso de conversión | `tools/ffprobe.exe` |
| [yt-dlp](https://github.com/yt-dlp/yt-dlp#installation) | Descargas YouTube | `tools/yt-dlp.exe` |
| [spotDL](https://spotdl.readthedocs.io/en/latest/installation/) | Enlaces Spotify | `tools/spotdl.exe` |
| [Deno](https://github.com/denoland/deno/releases) | Motor JavaScript para YouTube | `tools/deno.exe` |

La primera ubicación automática es `tools/` junto a `convert.exe`: en este proyecto,
`C:/Users/victo/Desktop/proyectos qt/convert/out/bin/tools/`.
Después se buscan en las carpetas antiguas `bin/` y `ffmpeg/`, en la carpeta del
ejecutable y en PATH. Si una preferencia apuntaba al antiguo `bin/` de este
ejecutable y el archivo ya no está allí, se actualiza a `tools/` si existe.
**Preferencias** permite seleccionar rutas absolutas que tienen prioridad sobre
la búsqueda automática; deja el campo vacío para utilizarla.
Si instalas spotDL con Python, selecciona el
`spotdl.exe` de la carpeta Scripts de esa instalación o entorno virtual.
FFmpeg y ffprobe deben estar juntos para el posprocesamiento de yt-dlp.
FFmpeg debe incluir los codificadores libx264, libvpx-vp9, libmp3lame, AAC,
FLAC, libopus y libvorbis según el formato elegido. La aplicación muestra
el error de la herramienta si falta un codificador.

Para el soporte completo de YouTube, las versiones recientes de yt-dlp utilizan
yt-dlp-ejs y un runtime JavaScript compatible. Convert detecta `deno.exe` en las
mismas carpetas de herramientas o mediante **Preferencias > deno** y pasa su ruta
explícitamente tanto a yt-dlp como a spotDL. También incorpora esa carpeta al
PATH de sus procesos hijos. Utilizar Deno 2.3 o posterior; el entorno local incluye
Deno 2.9.8 de su distribución oficial para Windows x64, con licencia en
`tools/deno-LICENSE.md`. Al distribuir Convert, conservar también ese ejecutable
y su licencia en la carpeta `tools`. Los ejecutables oficiales de yt-dlp ya incluyen
los componentes EJS correspondientes. Referencia:
[guía EJS de yt-dlp](https://github.com/yt-dlp/yt-dlp/wiki/EJS).

Spotify se integra como en la idea del
[repositorio de referencia](https://github.com/WilliamSchack/Spotify-Downloader):
usa sus enlaces y metadatos para localizar audio en YouTube. Esta versión delega
esa tarea a [spotDL](https://spotdl.readthedocs.io/en/latest/usage/).
No extrae el flujo de audio de Spotify. Las coincidencias dependen de spotDL y
pueden corresponder a otra versión de una canción. No se configura OAuth,
cookies ni acceso a listas privadas en esta primera versión.

## Archivos y calidad

Las conversiones locales se escriben en un archivo temporal único dentro del
destino, sin crear subcarpetas, para admitir carpetas de red que permiten crear
archivos pero restringen la creación de directorios. Solo se renombran tras
finalizar correctamente. Se conserva el nombre original cambiando solo la
extensión: `nombre.ext`. Si ya existe, se usa `nombre (2).ext`, `nombre (3).ext`,
etc. Esto también se aplica cuando origen y salida tienen el mismo formato y
carpeta. No se sobrescribe el archivo de entrada ni una salida
existente. Cancelar una conversión elimina su temporal.

Las descargas se guardan **directamente en la carpeta elegida**, sin crear
subcarpetas `descarga-…`. Los nombres finales no incluyen identificadores:
Spotify utiliza `Artista - Título.ext` y YouTube el título del vídeo. Se conservan
los detalles de la versión que forman parte del título. Si el nombre ya existe,
se añade ` (2)`, ` (3)`, etc.; no se sobrescriben salidas anteriores.

Para comprobar cada descarga sin confundir canciones con el mismo título, se
descarga primero a una carpeta temporal local de trabajo (`convert-download-XXXXXX`
dentro del directorio temporal de herramientas). No se crean carpetas de trabajo
en el servidor de salida. Los identificadores solo aparecen en esos nombres
internos. Tras validar la salida se copian los archivos al destino con el
nombre limpio, en segundo plano y sin sobrescribir archivos existentes. La copia
puede cancelarse; si falla, el registro muestra la causa y se conserva la descarga
local. En Spotify, la copia comienza con cada notificación de canción completada,
sin esperar al final de toda la playlist. Si falla la escritura en el destino,
se detiene spotDL y se conservan los archivos locales para su recuperación.
Las copias terminadas permanecen en el destino; se intenta eliminar la
copia incompleta. Si la descarga falla o se cancela, los archivos parciales se
conservan en esa carpeta de trabajo y el registro muestra su ruta. Reintentar
inicia una descarga nueva. No se renombran archivos ni se mueven las subcarpetas
creadas por versiones anteriores. El progreso YouTube corresponde
al flujo actual de la herramienta, no al total de una lista. Spotify y las
conversiones sin duración disponible muestran progreso indeterminado.

La verificación utiliza las rutas finales informadas por yt-dlp y los
identificadores de la relación de canciones generada por spotDL. Los informes
temporales de spotDL se guardan fuera de la carpeta de salida y sus errores se
copian al registro de la aplicación.
spotDL se ejecuta dentro de su carpeta de trabajo con una plantilla de nombre
relativa, para impedir que su limpieza de rutas cambie la carpeta seleccionada.

## Identidad visual

La referencia es el Radit local actual: paneles `#181e2c`, cabeceras violeta
`#4e4d7a`, controles `#262c3b`, acentos `#80A4AE` y tipografía Segoe UI.
La ventana principal, preferencias, menús, tabla y registro siguen esa paleta.
**Ayuda > Acerca de Convert** abre un diálogo propio con tres columnas, logotipo
central, versión, autor y créditos de las herramientas, con el estilo de Radit.

El icono es una **C** recta, gruesa y redondeada, siguiendo la letra de la
captura de referencia, con el acabado azul grisáceo y las bandas diagonales
de la identidad actual.
Se incluye en la ventana, el encabezado, Acerca de y los recursos del ejecutable
Windows. Sus fuentes están en `svn/convert.svg`; `svn/convert.png` y
`svn/convert.ico` se incluyen para su uso sin dependencias SVG adicionales.
El ICO incluye tamaños de 16 a 256 píxeles y el PNG tiene 512 píxeles.
Al arrancar se muestra una pantalla de inicio basada en la composición de Radit:
fondo oscuro, C grande, nombre blanco, líneas cian, versión y autor. Su imagen
está en `svn/splashscreen.png` a doble resolución (1000 × 572) y se presenta a
500 × 286 puntos, con un breve intervalo de lectura sin bloquear el bucle de
eventos. La versión se dibuja en tiempo de ejecución.
Para regenerar icono, PNG y pantalla de inicio en Windows se puede ejecutar
`./generate-icon.ps1`; utiliza System.Drawing y Arial Rounded MT Bold para el
nombre del splash. La aplicación usa las imágenes ya generadas y no requiere
esa fuente para mostrarlas. Este script genera imágenes, no compila ni ejecuta
la aplicación.

Después de incorporar estos archivos, ejecutar **qmake** en Qt Creator antes
de compilar para actualizar fuentes y el icono del ejecutable.

MP4/MKV locales utilizan H.264/AAC; WebM utiliza VP9/Opus. MP3 y AAC locales se
codifican a 192 kbit/s; Opus a 160 kbit/s. yt-dlp selecciona las pistas disponibles
y convierte o combina según el contenedor solicitado. WAV/FLAC no recuperan la
calidad perdida de una fuente comprimida. No hay controles de calidad avanzados.

La cola, rutas y preferencias se guardan mediante QSettings en el perfil del
usuario (`RaditTools/Convert`); el registro se mantiene en memoria. En Windows,
la cancelación intenta terminar también los procesos hijos mediante un Job
Object. Si Windows no permite agruparlos, se avisa en el registro. El cierre
forzado de la aplicación puede dejar temporales sin limpiar.

Las herramientas reciben `TEMP`, `TMP` y `TMPDIR` apuntando a una carpeta
exclusiva por trabajo dentro de `temp/`, junto a `convert.exe`
(`out/bin/temp/run-XXXXXX`). Esto permite a los ejecutables de yt-dlp y spotDL
empaquetados con PyInstaller descomprimirse sin depender de las rutas temporales
heredadas. Si esa ubicación no permite crear carpetas, se intenta el directorio
de datos locales de Convert y, después, el temporal del sistema. La ruta utilizada
aparece en el registro y se elimina al finalizar o cancelar el trabajo. No se
modifican las variables de entorno de Windows ni de Qt Creator.

En Windows, spotDL recibe además un perfil propio en `out/bin/data/spotdl-profile`
mediante el entorno de su proceso. Su carpeta `.spotdl` queda dentro de ese perfil,
y las rutas AppData de sus dependencias también se redirigen allí. Si no se puede
escribir junto al ejecutable, se intenta el directorio de datos locales de Convert.
Esta carpeta persiste entre trabajos y su ubicación aparece en el registro.
No se modifica `C:/Users/<usuario>/.spotdl` ni los permisos del perfil de Windows.
La redirección es necesaria incluso con `--no-cache`, porque spotDL crea su
carpeta al importar los módulos, antes de leer las opciones de la línea de comandos.

## Idioma de la interfaz

Convert utiliza **inglés** por defecto. En **Preferences → Language / Idioma**
se puede elegir **English** o **Castellano** y pulsar **Save / Guardar**. El cambio
se guarda en `interface/language` mediante QSettings y se aplica al cerrar y
volver a abrir Convert. Cancelar Preferencias descarta el cambio.

La selección afecta a menús, tabla, opciones de audio, Acerca de, pantalla de
inicio, gestor de actualizaciones y mensajes propios del registro e instalador.
Los mensajes emitidos por FFmpeg, yt-dlp, spotDL, Windows y los cuadros de diálogo
nativos conservan el idioma proporcionado por esas herramientas o el sistema.
Los detalles históricos guardados en una cola también conservan su texto original.

Las traducciones inglesas están en `svn/english.json` y los textos castellanos
en las llamadas `i18n::text` del código. Ambos se incluyen en los recursos del
ejecutable, sin necesitar archivos de idioma junto al programa. Se incluye
también `svn/qtbase_es.qm`, procedente de Qt oficial 6.7.0, para los controles
estándar de Qt en castellano; se distribuye con los avisos y licencias de Qt.
Para incorporar mensajes nuevos hay que añadir la traducción al catálogo,
conservando sus marcadores `%1`, `%2`, etc.

## Actualizaciones de Convert y herramientas

**Ayuda → Actualizaciones** muestra la versión instalada, la disponible y el
estado de Convert, FFmpeg/ffprobe/ffplay, yt-dlp, spotDL y Deno. Por defecto se
consulta al arrancar, como máximo una vez al día, y aparece un aviso en la barra
inferior cuando hay novedades. También se puede comprobar manualmente y
desactivar la consulta automática. La descarga e instalación empiezan al pulsar
**Actualizar** sobre el componente elegido; no se actualiza mientras la cola
esté trabajando.

Se consultan las Releases públicas de `valgaba/convert`, `GyanD/codexffmpeg`,
`yt-dlp/yt-dlp`, `spotDL/spotify-downloader` y `denoland/deno`. Se seleccionan
paquetes para Windows x64 y versiones estables; las preliminares de Convert se
pueden habilitar aparte. FFmpeg, ffprobe y ffplay se actualizan juntos. Se exige
el SHA-256 publicado por GitHub y se comprueba antes de instalar. Si no puede
leerse una versión o falta una publicación compatible/verificable, el diálogo
indica el problema y no sustituye ese componente.

Las herramientas nuevas se instalan en `tools/` y quedan seleccionadas en
Preferencias. Si se había elegido una ruta externa, su ejecutable se conserva.
Convert prepara el paquete antes de cerrarse, conserva la cola pendiente durante
el reinicio y vuelve a abrirse al terminar. La actualización de Convert conserva
las herramientas instaladas, las preferencias y los destinos de audio/vídeo.
Hace falta permiso de escritura en la carpeta de instalación.

El instalador guarda los archivos sustituidos en `.update-backup/<identificador>`
y, si falla la sustitución, intenta restaurarlos. El resultado se registra y, en
el caso de Convert, se muestra al abrirse de nuevo. Los paquetes y diagnósticos
quedan en la carpeta `updates` de la caché local de Convert; las copias anteriores
se conservan para recuperación manual. La sustitución final no se puede cancelar;
sí la descarga y la preparación del reinicio.

### Preparar una versión detectable

La versión común de qmake y CMake se lee de `app-version.txt`. Se utiliza este
nombre para evitar conflictos con la cabecera estándar de C++ `<version>` en
Windows. Para una nueva publicación:

1. Cambiar `app-version.txt` a un número mayor, con formato `mayor.menor.parche`.
2. Compilar y desplegar con `./build.ps1 -Deploy`, y validar la aplicación.
3. Preparar `out/bin/licenses/` con las licencias y avisos correspondientes a Qt,
   sus dependencias y las herramientas incluidas. También puede indicarse otra
   carpeta mediante `-LicenseDirectory`.
4. Ejecutar `./package-release.ps1`. Genera una carpeta exclusiva en `out/releases`
   con `convert-<versión>-windows-x64.zip` y `SHA256SUMS.txt`, sin datos personales,
   cola, temporales ni copias de actualizaciones anteriores.
5. Publicar ambos archivos como assets de la Release `v<versión>` del repositorio.
   Una Release marcada como preliminar solo se ofrece si está habilitado ese canal.

El script de empaquetado no compila ni publica. No debe sustituirse una descarga
existente manteniendo su versión para ofrecer una actualización: el comparador
requiere una versión mayor. Las distribuciones anteriores a este gestor necesitan
una primera actualización manual para incorporarlo. Este cambio no genera ni
publica una Release por sí solo.

## Verificación pendiente por el usuario

Siguiendo `../AGENTS.md`, el agente no ha compilado ni ejecutado pruebas.
Esta versión requiere validación antes de distribuirse:

1. Compilar con `./build.ps1 -Deploy` y abrir `out/bin/convert.exe`.
2. Configurar herramientas; convertir un audio y un vídeo con espacios y acentos
   en el nombre. Revisar formato, reproducción y conservación del original.
3. Repetir una conversión al mismo destino y comprobar que usa otro nombre.
4. Probar un enlace YouTube propio y un enlace Spotify público; comprobar las
   salidas y el registro, incluida una lista con algún elemento no disponible.
5. Cancelar durante conversión y descarga; confirmar que cesa la actividad,
   quedan pendientes los siguientes y funciona el reintento.
6. Cerrar y abrir: por defecto la lista debe quedar vacía, conservando formato,
   rutas y geometría. Activar Recordar la cola entre sesiones y comprobar que
   solo entonces se restauran las entradas pendientes, canceladas y fallidas.
7. Probar herramienta ausente, archivo inexistente y destino sin permisos;
   deben aparecer como errores y permitir continuar con el resto de la cola.
8. Arrastrar archivos desde dos carpetas diferentes y comprobar que cada uno
   conserva su carpeta de origen al cambiar el formato desde su propia celda.
   Después pulsar **Aplicar a selección**: las filas seleccionadas deben adoptar
   tanto el formato superior como la carpeta **Salida URL**; si se recuerda la cola,
   comprobar que se conserva también tras reiniciar.
9. Pegar URL con Ctrl+V y mediante el menú contextual, con la lista vacía y con
   entradas existentes; sus destinos deben ser la carpeta de descargas elegida.
10. Elegir formatos diferentes en dos filas y comprobar que solo cambia la fila
    editada. Spotify no debe permitir formatos de vídeo.
11. Completar el primer trabajo de una cola de varios: debe desaparecer y seguir
    el siguiente sin saltarse entradas. Los fallidos y cancelados deben permanecer.
12. Descargar dos canciones o vídeos con el mismo nombre: las salidas deben llevar
    nombres sin identificador y sufijos numéricos, conservando todos los archivos.
13. Abrir **Ayuda → Actualizaciones**, comprobar versiones y avisos sin conexión.
    Validar en una copia de la instalación la actualización de una herramienta,
    la cancelación de una descarga y el reinicio de Convert con una cola pendiente.
    Revisar la copia anterior y la recuperación ante un paquete incompleto o un
    destino sin permisos antes de distribuir el gestor.
14. Sin preferencia de idioma guardada, comprobar el arranque en inglés.
    En Preferences elegir Castellano, guardar y volver a abrir: revisar menús,
    tabla, audio, Acerca de, registro y actualizaciones. Volver a English y repetir.
    Cancelar un cambio de idioma debe mantener la elección anterior; rutas y
    formatos deben conservarse en ambos idiomas.

Se ha preparado el proyecto para Windows. No se ha validado en Linux; la
cancelación de árboles de procesos está implementada específicamente para Windows.

## Estructura y distribución

`svn/` contiene fuentes y recursos; `lib/` documenta dependencias; `out/` contiene
compilación y ejecutables. Esta organización sigue el patrón del proyecto Radit.
No se han incluido binarios de terceros. Para distribuir, conservar las DLL y
plugins generados por windeployqt y revisar las licencias de Qt, FFmpeg y cada
herramienta que se incluya. No se ha creado ni publicado un paquete de distribución.

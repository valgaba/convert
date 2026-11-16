# Convert

Primera versión de una aplicación de escritorio en **Qt 6.7 / C++17**, con
apariencia basada en la interfaz actual de Radit. El código es nuevo; no se
ha copiado código del repositorio Spotify-Downloader ni de Radit.

## Funciones implementadas

- Arrastrar archivos y enlaces a una lista, o añadirlos mediante botones.
- Conversión con FFmpeg a MP4, MKV, WebM, MP3, WAV, FLAC, M4A, Opus y Ogg.
- Enlaces YouTube con yt-dlp, incluidos enlaces de listas.
- Canciones, álbumes y listas públicas de Spotify mediante **spotDL externo**.
- Formato y carpeta por entrada. Para cambiar entradas pendientes o fallidas,
  seleccionarlas y pulsar **Aplicar a selección**.
- Cola secuencial, progreso, registro, cancelación, reintento y apertura del destino.
- Preferencias de herramientas y restauración de la cola entre sesiones.

Los controles de edición se desactivan durante el procesamiento. **Detener cola**
cancela el trabajo actual y deja los siguientes pendientes. Las entradas
canceladas y fallidas requieren **Reintentar selección** antes de iniciar la cola.
Al recuperar una sesión, las entradas que estaban procesándose quedan canceladas;
no se inicia ninguna descarga automáticamente.

La carpeta de salida predeterminada para una configuración nueva es `salida/`
junto a `convert.exe` (`out/bin/salida`). Una carpeta guardada anteriormente
se conserva. Al cambiar el destino de entradas que ya están en la lista, hay que
seleccionarlas y pulsar **Aplicar a selección**; el campo superior por sí solo
solo cambia el destino de las entradas nuevas. Se aceptan rutas pegadas entre
comillas y direcciones locales `file:///`; las rutas relativas se resuelven
respecto al ejecutable. El registro muestra la ruta utilizada y se comprueba que
el destino admite escritura antes de iniciar las herramientas.

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

## Herramientas de ejecución

Instalar por separado desde sus proyectos oficiales:

| Herramienta | Uso | Ubicación automática junto al ejecutable |
| --- | --- | --- |
| [FFmpeg](https://ffmpeg.org/download.html) | Conversión y posprocesamiento | `bin/ffmpeg.exe` |
| ffprobe (misma distribución FFmpeg) | Duración y progreso de conversión | `bin/ffprobe.exe` |
| [yt-dlp](https://github.com/yt-dlp/yt-dlp#installation) | Descargas YouTube | `bin/yt-dlp.exe` |
| [spotDL](https://spotdl.readthedocs.io/en/latest/installation/) | Enlaces Spotify | `bin/spotdl.exe` |

La primera ubicación automática es `bin/` junto a `convert.exe`: en este proyecto,
`C:/Users/victo/Desktop/proyectos qt/convert/out/bin/bin/`.
Después se buscan en `ffmpeg/`, `tools/`, en la carpeta del ejecutable y en PATH.
**Preferencias** permite seleccionar rutas absolutas que tienen prioridad sobre
la búsqueda automática; deja el campo vacío para utilizarla.
Si instalas spotDL con Python, selecciona el
`spotdl.exe` de la carpeta Scripts de esa instalación o entorno virtual.
FFmpeg y ffprobe deben estar juntos para el posprocesamiento de yt-dlp.
FFmpeg debe incluir los codificadores libx264, libvpx-vp9, libmp3lame, AAC,
FLAC, libopus y libvorbis según el formato elegido. La aplicación muestra
el error de la herramienta si falta un codificador.

Para el soporte completo de YouTube, las versiones recientes de yt-dlp utilizan
yt-dlp-ejs y un runtime JavaScript compatible, preferiblemente Deno. Sigue la
[documentación de dependencias de yt-dlp](https://github.com/yt-dlp/yt-dlp#dependencies)
y asegúrate de que Deno esté en PATH antes de iniciar Convert. Los ejecutables
oficiales de yt-dlp ya incluyen los componentes EJS correspondientes.

Spotify se integra como en la idea del
[repositorio de referencia](https://github.com/WilliamSchack/Spotify-Downloader):
usa sus enlaces y metadatos para localizar audio en YouTube. Esta versión delega
esa tarea a [spotDL](https://spotdl.readthedocs.io/en/latest/usage/).
No extrae el flujo de audio de Spotify. Las coincidencias dependen de spotDL y
pueden corresponder a otra versión de una canción. No se configura OAuth,
cookies ni acceso a listas privadas en esta primera versión.

## Archivos y calidad

Las conversiones se escriben en una carpeta temporal del destino y solo se
renombran tras finalizar correctamente. Se utiliza `nombre_convert.ext` y un
sufijo numérico si existe. No se sobrescribe el archivo de entrada ni una salida
existente. Cancelar una conversión elimina su temporal.

Las descargas se guardan **directamente en la carpeta elegida**, sin crear
subcarpetas `descarga-…`. Los nombres de YouTube incluyen el identificador del
vídeo; los de Spotify comienzan por el identificador de la canción, seguido del
artista y el título. Se mantienen las opciones de no sobrescribir archivos.
Los archivos que ya existen se pueden omitir y las partes descargadas se
conservan al cancelar. Al reintentar se vuelve a ejecutar la URL sobre el mismo
destino; la reutilización de partes depende de la herramienta. No se borran ni se
mueven las subcarpetas creadas por versiones anteriores. El progreso YouTube corresponde
al flujo actual de la herramienta, no al total de una lista. Spotify y las
conversiones sin duración disponible muestran progreso indeterminado.

La verificación utiliza las rutas finales informadas por yt-dlp y los
identificadores de la relación de canciones generada por spotDL. Los informes
temporales de spotDL se guardan fuera de la carpeta de salida y sus errores se
copian al registro de la aplicación.

## Identidad visual

La referencia es el Radit local actual: paneles `#181e2c`, cabeceras violeta
`#4e4d7a`, controles `#262c3b`, acentos `#80A4AE` y tipografía Segoe UI.
La ventana principal, preferencias, menús, tabla y registro siguen esa paleta.
**Ayuda > Acerca de Convert** abre un diálogo propio con tres columnas, logotipo
central, versión, autor y créditos de las herramientas, con el estilo de Radit.

El icono es una **C** inclinada con acabado azul grisáceo y bandas diagonales.
Se incluye en la ventana, el encabezado, Acerca de y los recursos del ejecutable
Windows. Sus fuentes están en `svn/convert.svg`; `svn/convert.png` y
`svn/convert.ico` se incluyen para su uso sin dependencias SVG adicionales.
El ICO incluye tamaños de 16 a 256 píxeles y el PNG tiene 512 píxeles.
Para regenerar los recursos en Windows se puede ejecutar `./generate-icon.ps1`;
este script genera imágenes, no compila ni ejecuta la aplicación.

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
6. Cerrar y abrir: comprobar restauración de cola, formato, rutas y geometría.
7. Probar herramienta ausente, archivo inexistente y destino sin permisos;
   deben aparecer como errores y permitir continuar con el resto de la cola.

Se ha preparado el proyecto para Windows. No se ha validado en Linux; la
cancelación de árboles de procesos está implementada específicamente para Windows.

## Estructura y distribución

`svn/` contiene fuentes y recursos; `lib/` documenta dependencias; `out/` contiene
compilación y ejecutables. Esta organización sigue el patrón del proyecto Radit.
No se han incluido binarios de terceros. Para distribuir, conservar las DLL y
plugins generados por windeployqt y revisar las licencias de Qt, FFmpeg y cada
herramienta que se incluya. No se ha creado ni publicado un paquete de distribución.

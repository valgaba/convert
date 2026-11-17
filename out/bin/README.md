Salida de compilación y recursos de ejecución.

Coloca FFmpeg, ffprobe, yt-dlp, spotDL y Deno en la subcarpeta `tools/`
de este directorio, es decir, en `out/bin/tools/`. Es la primera ubicación
de búsqueda automática. Las rutas elegidas en Preferencias tienen prioridad.
Como alternativa se mantienen `bin/`, `ffmpeg/`, la carpeta del ejecutable
y PATH. Conserva las DLL y licencias de sus distribuciones.
`build.ps1 -Deploy` copia las dependencias Qt del kit oficial seleccionado.

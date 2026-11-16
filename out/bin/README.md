Salida de compilación y recursos de ejecución.

Coloca FFmpeg, ffprobe, yt-dlp y spotDL (opcional) en la subcarpeta `bin/`
de este directorio, es decir, en `out/bin/bin/`. Es la primera ubicación
de búsqueda automática. Las rutas elegidas en Preferencias tienen prioridad.
Como alternativa se mantienen `ffmpeg/`, `tools/`, la carpeta del ejecutable
y PATH. Conserva las DLL y licencias de sus distribuciones.
`build.ps1 -Deploy` copia las dependencias Qt del kit oficial seleccionado.

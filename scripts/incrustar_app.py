"""
Script previo a la compilación (PlatformIO, extra_scripts = pre:...).

Comprime app/index.html con gzip y lo escribe como un arreglo de bytes en
src/app_web.h, para que el ESP32 sirva la app sin necesitar LittleFS ni un
paso aparte de "Upload Filesystem". Solo reescribe el .h si cambió, así no
fuerza recompilaciones innecesarias.
"""

import gzip
import os

Import("env")  # noqa: F821  (lo inyecta PlatformIO/SCons)

PROYECTO = env.subst("$PROJECT_DIR")  # noqa: F821
ORIGEN = os.path.join(PROYECTO, "app", "index.html")
DESTINO = os.path.join(PROYECTO, "src", "app_web.h")


def generar():
    with open(ORIGEN, "rb") as f:
        html = f.read()
    comprimido = gzip.compress(html, compresslevel=9, mtime=0)

    lineas = []
    for i in range(0, len(comprimido), 20):
        trozo = comprimido[i:i + 20]
        lineas.append("  " + ", ".join("0x%02x" % b for b in trozo) + ",")

    contenido = (
        "// ARCHIVO GENERADO por scripts/incrustar_app.py. No editar a mano:\n"
        "// edita app/index.html y vuelve a compilar.\n"
        "#pragma once\n"
        "#include <Arduino.h>\n\n"
        "const size_t APP_HTML_GZ_LEN = %d;  // %d bytes sin comprimir\n"
        "const uint8_t APP_HTML_GZ[] PROGMEM = {\n%s\n};\n"
        % (len(comprimido), len(html), "\n".join(lineas))
    )

    previo = None
    if os.path.exists(DESTINO):
        with open(DESTINO, "r", encoding="utf-8") as f:
            previo = f.read()
    if previo != contenido:
        with open(DESTINO, "w", encoding="utf-8", newline="\n") as f:
            f.write(contenido)
        print("App web incrustada: %d bytes (gzip de %d)" % (len(comprimido), len(html)))


generar()

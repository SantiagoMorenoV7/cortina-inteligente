# Cortina inteligente (ESP32 + 28BYJ-48 + filtro de Kalman)

Prototipo de cortina automática que se abre o se cierra según la luz que entra
por la ventana, con una **app web propia** para controlarla desde el celular o
el computador. Es la versión con ESP32 del proyecto descrito en el informe
(LDR + motor paso a paso + finales de carrera + histéresis 320 / 620), con dos
mejoras: un **filtro de Kalman** sobre la lectura de luz y la app.

```
 LDR + 4,7 kΩ ──► ESP32 ──► ULN2003 ──► 28BYJ-48 ──► polea y cordel ──► cortina
   (GPIO 34)       │   ▲                                 │
                   │   └──── finales de carrera ◄────────┘
                   ▼          (GPIO 32 y 33)
           WiFi "Cortina-XXXX"  ──►  app en http://192.168.4.1
```

**Pruébala en línea:** https://santiagomorenov7.github.io/cortina-inteligente/
(en internet funciona en modo demostración, con un simulador que usa la misma
lógica del ESP32; para controlar la cortina real hay que abrir la app que
sirve la propia placa en http://192.168.4.1). Se actualiza sola con cada
`git push` que cambie la carpeta `app/`.

## Instalar en cualquier PC (un solo comando)

Conecta el ESP32 por USB, abre **PowerShell** y pega:

```powershell
irm https://raw.githubusercontent.com/SantiagoMorenoV7/cortina-inteligente/main/instalar.ps1 | iex
```

Eso descarga el proyecto en `Documentos\cortina-inteligente`, instala lo que
falte (Python y PlatformIO; no hace falta VS Code), compila, detecta la placa,
le sube el firmware y abre el monitor serie. La primera vez tarda unos minutos
porque descarga el compilador del ESP32; las siguientes, segundos. Si ya está
descargado, el mismo comando lo actualiza a la última versión.

Si ya tienes la carpeta del proyecto (por ejemplo con `git clone`), basta con:

| Comando | Qué hace |
|---|---|
| `.\desplegar.cmd` | Todo: instala lo que falte, compila, sube al ESP32 y abre el monitor. |
| `.\desplegar.cmd -Puerto COM5` | Igual, pero en ese puerto (si hay varias placas o no la detecta). |
| `.\desplegar.cmd -SoloCompilar` | Solo compila (no necesita la placa). |
| `.\desplegar.cmd -SinMonitor` | Sube el firmware y termina. |
| `.\desplegar.cmd -Demo` | Abre la app en modo demostración, sin placa. |

En Linux o macOS: `./desplegar.sh` (opciones `--puerto`, `--solo-compilar`,
`--sin-monitor`).

> Si la subida se queda en `Connecting....`, mantén presionado el botón
> **BOOT** de la placa hasta que empiece a escribir. Si Windows no reconoce la
> placa, instala el driver de su chip USB: CH340 o CP2102 (el script muestra
> los enlaces).

## Qué hace

- **Automático por luz:** cada 100 ms lee el LDR, lo filtra con Kalman y lo
  compara con dos umbrales (histéresis). Cierra si la luz supera **620**, abre
  si baja de **320** y entre los dos no se mueve. Además, la luz tiene que
  quedarse fuera de la banda **1 s** antes de mover el motor, así una sombra o
  un reflejo corto no la mueven.
- **Manual:** abrir, cerrar, parar o llevarla a un % exacto desde la app, el
  monitor serie o el botón BOOT de la placa. Una orden manual pausa la
  automatización y la cortina vuelve sola a automático a los 30 min
  (configurable).
- **Horarios:** hasta 8 (por ejemplo "abrir de lunes a viernes a las 6:30").
  La hora la toma de internet o, si no hay, del celular al abrir la app.
- **Posición confiable:** al encender abre hasta el final de carrera para tomar
  referencia. Abrir y cerrar avanzan hasta tocar el interruptor (con un margen
  máximo de 503 pasos; si no lo toca, se detiene y avisa en la app).
- **Todo se ajusta desde la app** (umbrales, filtro, velocidad, recorrido, WiFi)
  y queda guardado en la memoria del ESP32.

## Materiales

| Cant. | Componente | Notas |
|---|---|---|
| 1 | ESP32 DevKit (WROOM-32) | Reemplaza al Arduino Nano del informe. |
| 1 | Motor 28BYJ-48 (5 V) + driver ULN2003 | |
| 1 | LDR GL5516 + resistencia de 4,7 kΩ | Divisor de tensión del informe. |
| 2 | Finales de carrera (microswitch) | Abierta y cerrada. |
| 1 | Fuente de 5 V / 2 A | Para el motor. |
| — | Protoboard, jumpers, riel, polea y cordel | |

## Conexiones

**Sensor de luz (divisor de tensión).** Va a **3V3**, no a 5 V: el pin del
ESP32 solo aguanta 3,3 V.

```
3V3 ── LDR ──┬── 4,7 kΩ ── GND
             │
          GPIO 34
```

Con esta conexión la lectura **sube** con la luz, igual que en el informe. Si
usas un módulo LDR con el sensor del lado de tierra (la lectura baja con la
luz), activa *Ajustes → Motor y recorrido → Invertir lectura del sensor*.

**Motor (driver ULN2003)**

| ULN2003 | Conectar a |
|---|---|
| IN1 | GPIO 16 |
| IN2 | GPIO 17 |
| IN3 | GPIO 18 |
| IN4 | GPIO 19 |
| + (5-12 V) | +5 V de la fuente externa |
| − | GND de la fuente **y** GND del ESP32 (tierra común obligatoria) |

**Finales de carrera** (cada uno entre el pin y GND; usa el contacto NO)

| Interruptor | Pin |
|---|---|
| Cortina abierta | GPIO 32 |
| Cortina cerrada | GPIO 33 |

El ESP32 se alimenta por USB (o desde la misma fuente de 5 V por el pin VIN).
El LED de la placa (GPIO 2) se enciende con la cortina cerrada, parpadea lento
mientras se mueve y rápido si hay un error.

> Los GPIO 16 y 17 no existen en las placas WROVER (con PSRAM). Si tu placa es
> WROVER, cambia los pines en `src/config.h`.

## Primer uso

1. Abre la carpeta en VS Code con PlatformIO y pulsa **Upload**. La app se
   comprime y se incrusta sola en el firmware: no hay que subir nada aparte.
   Si la placa no entra en modo de carga, mantén presionado **BOOT** mientras
   empieza la subida.
2. En el celular, conéctate al WiFi **Cortina-XXXX** (clave `cortina123`). La
   app se abre sola. Si no, entra a **http://192.168.4.1**.
3. Ve a **Ajustes → Calibración del recorrido**:
   - Presiona cada final de carrera con la mano: su indicador se pone verde.
   - Con los botones ◀ ▶ mueve la cortina. Si "▶ hacia cerrada" la abre,
     activa *Invertir sentido del motor*.
   - Llévala a cerrada y pulsa **Aquí está cerrada**: eso guarda el recorrido
     real en pasos. También puedes calcularlo con la polea
     (P = d / 2πr · 2048, por defecto 60 cm y r = 1,2 cm → 16 297 pasos).
4. Opcional: en **Ajustes → Red WiFi** conecta la cortina al WiFi de la casa.
   Así la app también queda en **http://cortina.local** desde cualquier
   dispositivo de la red, y la hora se toma de internet.

Si no tienes los finales de carrera puestos todavía, desactiva *Usar finales de
carrera*: la cortina asume que arranca abierta y cuenta pasos.

## La app

| Pestaña | Qué tiene |
|---|---|
| **Inicio** | Ventana animada con la posición real, abrir / parar / cerrar, deslizador de posición, modo automático o manual, nivel de luz sobre la banda de histéresis, cuenta regresiva antes de moverse, próximo horario y actividad reciente. |
| **Sensor** | Gráfica en vivo (1 o 5 min) de la lectura cruda, el promedio de 10 y el Kalman con los umbrales. Valores actuales (N, voltaje, resistencia estimada del LDR). Matrices F, H, Q, R, x, P y K **en vivo**. Ajuste de R y q y botón para medir el ruido del sensor. |
| **Horarios** | Reloj de la cortina, sincronización con el celular y lista de horarios (crear, editar, activar o desactivar, eliminar). |
| **Ajustes** | Nombre, zona horaria, umbrales (con su equivalencia en voltios y kΩ), velocidad, recorrido, calibración, WiFi, actualización de firmware por la app (OTA), reinicio y restablecer de fábrica. |

Funciona en celular y computador, en tema claro y oscuro, y sin internet (no
carga nada de afuera). En el celular se puede usar *Agregar a pantalla de
inicio* para abrirla como una app.

**Modo demostración.** Si abres `app/index.html` directamente en el navegador
(o con `python -m http.server 8080 --directory app`), la app arranca con un
**simulador** que usa la misma lógica del firmware (el mismo Kalman, la misma
histéresis, el mismo motor). Trae controles para simular la luz, pasar la mano
o una nube. Sirve para presentar el proyecto sin el hardware. Para usar esa
copia con una cortina real de tu red: `index.html?dispositivo=192.168.1.57`.

## Filtro de Kalman

El estado es **x = [n, v]ᵀ**: el nivel de luz (0-1023) y su velocidad de cambio.

```
Predicción:  x⁻ = F·x                 P⁻ = F·P·Fᵀ + Q
Corrección:  K = P⁻·Hᵀ·(H·P⁻·Hᵀ + R)⁻¹
             x = x⁻ + K·(z − H·x⁻)    P = (I − K·H)·P⁻

F = [1  Δt]    H = [1  0]    Q = q·[Δt⁴/4  Δt³/2]    R = r
    [0   a]                        [Δt³/2  Δt²  ]
```

con Δt = 0,1 s, **a = e^(−Δt/τ)**, τ = 0,7 s, q = 100 y r = 25 (la varianza
del ruido; se puede medir desde la app). Como H solo toma la primera
componente, H·P⁻·Hᵀ + R es un número y su "inversa" es una división: no hay
que invertir matrices en el microcontrolador.

**¿Por qué a < 1?** Con el modelo clásico de velocidad constante (a = 1), el
filtro **se pasa de largo** en los cambios bruscos de luz. En las simulaciones,
un salto de 100 a 580 (dentro de la banda) lo llevaba hasta 671 durante 3 s, y
eso cerraba la cortina sin motivo. Amortiguar la velocidad lo corrige:

| Filtro | Peor pico tras saltar de 100 a 580 | Tiempo para cruzar 620 (de 400 a 700) | Ruido que queda (entrada σ = 6) |
|---|---|---|---|
| Promedio de 10 (informe) | 588 | 0,70 s | 1,88 (3,2× menos) |
| Kalman con a = 1, q = 10 | 678, 3,3 s sobre 620 (**cierra por error**) | 0,80 s | 1,68 (3,6× menos) |
| Kalman con a = 1, q = 100 | 677, 1,8 s sobre 620 (**cierra por error**) | 0,45 s | 2,22 (2,7× menos) |
| **Kalman con τ = 0,7 s, q = 100** | **591** | **0,90 s** | **1,55 (3,9× menos)** |

El Kalman amortiguado es el que más ruido quita sin cerrar por error, a
cambio de reaccionar 0,2 s más lento que el promedio. Además estima la
velocidad con que cambia la luz, que se ve en la app.

Las cifras salen de 200 simulaciones con ruido distinto. La implementación
en C++ (`src/kalman.h`) y la de la app dan los mismos resultados: la
diferencia máxima fue de 0,0002 en 3000 muestras.

## Histéresis y umbrales

La lectura se lleva a la escala del informe, **N = Vout / Vref · 1023**. Como el
divisor se alimenta con los mismos 3,3 V que usa de referencia el ADC, N
depende solo de la relación entre resistencias. Por eso los umbrales del
informe siguen siendo válidos en el ESP32:

| Umbral | N | Vout (3,3 V) | R del LDR |
|---|---|---|---|
| Abrir | 320 | 1,03 V | ≈ 10,3 kΩ |
| Cerrar | 620 | 2,00 V | ≈ 3,1 kΩ |

## Monitor serie (115200 baudios)

Cada segundo imprime una línea como
`N:  612 | prom:  608.4 | kalman:  610.2 | pos:   0% (0/16297) | abierta | AUTO`
y anota los eventos con `>>>`. Acepta los comandos `abrir`, `cerrar`,
`parar`, `ir 50`, `auto`, `manual`, `buscar`, `red` y `ayuda`.

## API (para quien quiera integrarla con otra cosa)

Todas las respuestas son JSON. Los POST reciben JSON en el cuerpo.

| Método y ruta | Uso |
|---|---|
| `GET /api/estado?ev=<n>` | Estado completo (luz, Kalman, motor, modo, red, hora) y los eventos posteriores a `n`. |
| `GET /api/historial` | Últimos 5 minutos, 1 punto por segundo. |
| `POST /api/comando` | `{"accion":"abrir"\|"cerrar"\|"parar"\|"alternar"\|"auto"\|"manual"}` o `{"accion":"posicion","pct":50}` |
| `POST /api/calibrar` | `{"accion":"mover","pasos":512}`, `buscar`, `fijarAbierta`, `fijarCerrada`, `ruido` |
| `GET` / `POST /api/ajustes` | Leer o cambiar ajustes (solo los campos enviados; se validan rangos). |
| `GET` / `POST /api/horarios` | `{"horarios":[{"activo":true,"hora":6,"minuto":30,"dias":62,"accion":0,"pct":0}]}` (días: bit 0 = domingo; acción: 0 abrir, 1 cerrar, 2 posición, 3 automático) |
| `POST /api/hora` | `{"epoch": <segundos UTC>}` |
| `GET /api/redes`, `POST /api/wifi` | Buscar redes y conectar (`{"ssid","clave"}` o `{"olvidar":true}`). |
| `POST /api/ota` | Subir `firmware.bin` (multipart). |
| `POST /api/reiniciar`, `POST /api/restablecer` | |

## Estructura del código

```
src/config.h      pines y valores por defecto
src/kalman.h      filtro de Kalman 2×2 (sin dependencias)
src/luz.*         lectura del LDR, promedio de 10, Kalman y medición del ruido
src/motor.*       tarea de FreeRTOS del motor paso a paso y finales de carrera
src/control.*     histéresis, modos, horarios, eventos, botón BOOT y LED
src/red.*         WiFi, portal cautivo, mDNS, API y OTA
src/ajustes.*     ajustes guardados en la flash
app/index.html    la app (HTML, CSS y JS en un solo archivo, sin librerías)
scripts/incrustar_app.py   comprime la app y la convierte en src/app_web.h al compilar
```

El motor corre en su propia tarea de FreeRTOS, con más prioridad que el resto,
así mantiene un ritmo de pasos constante (3 ms por paso ≈ 9,8 RPM, recorrido
completo en ≈ 49 s) aunque el WiFi o la app estén ocupados.

## Problemas comunes

- **La app no abre en 192.168.4.1:** revisa que el celular siga conectado al
  WiFi de la cortina. Algunos Android se pasan a datos móviles porque esa red
  "no tiene internet"; elige *Mantener conexión*.
- **El motor vibra pero no gira:** revisa el orden IN1-IN4 y que la fuente de
  5 V tenga la tierra unida al ESP32. Prueba una velocidad más lenta (más ms
  por paso).
- **"No se detectó el final de carrera":** el motor dio todo el recorrido más
  el margen sin tocar el interruptor. Revisa el cableado del interruptor (los
  indicadores de Calibración) y el cordel, y pulsa *Reintentar*.
- **La cortina se cierra o se abre al revés:** activa *Invertir sentido del
  motor*.
- **La lectura baja cuando hay más luz:** activa *Invertir lectura del sensor*.

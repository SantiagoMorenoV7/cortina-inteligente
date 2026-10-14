# Simulador de cortinas automáticas (ESP32 + PlatformIO)

Sensor de luz **MH-Sensor-Series** (fotorresistencia/LDR + módulo LM393).
El valor de luz se reporta en **lux estimados** (ver nota de precisión abajo).

La cortina se mueve de forma **gradual y proporcional** a los lux, no como
un interruptor todo-o-nada: una luz débil o lejana la cierra solo un poco,
una luz fuerte y directa la cierra más.

| Lux | Cortina |
|---|---|
| ≤ 80 (piso, por defecto) | **100% abierta** (servo a 0°) |
| entre 80 y 9000 | cerrada **en proporción** a los lux (0°-180°) |
| ≥ 9000 (tope, por defecto) | **100% cerrada** (servo a 180°, LED encendido) |

> **Precisión de los lux:** esta fotorresistencia es genérica, sin hoja de
> datos exacta, así que el valor de lux es una **estimación** (orden de
> magnitud) basada en la curva típica de una GL5528, no una medición
> calibrada. Si necesitas lux precisos, usa un sensor digital como el
> BH1750. Para este simulador, la estimación es más que suficiente.

## Conexiones

**Módulo de luz → ESP32** (letras impresas en el módulo, de izquierda a
derecha: `AO DO GND VCC` — revisa las de tu módulo, el orden varía según
el fabricante)

| Módulo | ESP32 |
|---|---|
| VCC | **3V3** (no 5V: a 5V la salida AO puede pasar de 3.3 V y dañar el pin) |
| GND | GND |
| AO | GPIO 34 |
| DO | sin conectar |

**Servo SG90 → ESP32**

| Cable | ESP32 |
|---|---|
| Marrón | GND |
| Rojo | VIN (5V del USB) |
| Naranja | GPIO 18 |

> Si el ESP32 se reinicia cuando el servo se mueve, el USB no le da suficiente corriente:
> alimenta el servo con una fuente de 5V externa (uniendo los GND) o pon un capacitor
> de 470 µF entre 5V y GND cerca del servo.

## Uso

1. Abre esta carpeta en VS Code con PlatformIO.
2. **Upload** (flecha →) y luego **Serial Monitor** (115200 baudios).
3. Mira los lux que da tu cuarto y los que da tu linterna (de cerca, de
   lejos, en ángulo) en el Monitor Serie o en `localhost:8080`, y ajusta en
   `src/main.cpp`:
   - `LUX_ABIERTA`: lux por debajo de los cuales la cortina queda 100% abierta.
   - `LUX_CERRADA`: lux por encima de los cuales la cortina queda 100% cerrada
     (el "tope"). Súbelo si notas que cualquier lucecita ya la cierra del todo.
   - `R_FIJO_KOHM`: si los lux se ven muy desviados de la realidad, prueba
     cambiando este valor (10kΩ por defecto, típico de estos módulos).
4. Ajusta si quieres `ANGULO_*` o `MS_POR_GRADO` (velocidad del servo).
5. Si la cortina "tiembla" (cambia de dirección varias veces por segundo),
   baja `FACTOR_SUAVIZADO` (0.2 por defecto); si la ves demasiado lenta
   para seguir a la linterna, súbelo. El suavizado se aplica después del
   sensor y no cambia cómo se lee ni cómo se convierte a lux.

Ejemplo de salida:

```
raw:  620 | lux:    55.0 | cortina: ARRIBA ( 12°)
raw:  180 | lux:   410.8 | cortina: ABAJO  ( 98°)
>>> Llega al tope de lux: cortina 100% cerrada
raw:   40 | lux:  1850.3 | cortina: ABAJO  (180°)
```

## Panel de control local

La carpeta `panel/` tiene una página (`panel/index.html`) que muestra en vivo
la cortina, el % de luz, el valor `raw`, una gráfica de los últimos 2 minutos,
la tabla de lecturas y el registro de eventos.

Para abrirla:

```
python -m http.server 8080 --directory panel
```

Y entra a `http://localhost:8080` en **Chrome o Edge** (necesitan el acceso
a USB vía Web Serial). Pulsa **Conectar ESP32** y elige el puerto COM del
ESP32, o usa **Modo demo** para verla funcionar sin la placa. Cierra el
Serial Monitor de VS Code antes de conectar: solo un programa puede usar el
puerto COM a la vez.

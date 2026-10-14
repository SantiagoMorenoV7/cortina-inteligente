# Simulador de cortinas automáticas (ESP32 + PlatformIO)

Sensor de luz **MH-Sensor-Series** (fotorresistencia/LDR + módulo LM393):

| Luz | Cortina |
|---|---|
| Mucha luz | **Baja** (servo a 180°, LED encendido) |
| Poca luz | **Sube** (servo a 0°, LED apagado) |

## Conexiones

**Módulo de luz → ESP32**

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
3. Calibra con lo que muestra el monitor, en `src/main.cpp`:
   - `VALOR_OSCURO`: valor `raw` con el sensor tapado con la mano.
   - `VALOR_CLARO`: valor `raw` con una luz o linterna apuntando al sensor.
   No importa cuál de los dos sea mayor: el mapeo (`map()`) se ajusta solo.
4. Ajusta si quieres `LUZ_PARA_BAJAR` / `LUZ_PARA_SUBIR` (umbrales), `ANGULO_*`
   o `MS_POR_GRADO` (velocidad).

Ejemplo de salida:

```
raw: 2980 | luz:  98% | cortina: ABAJO  (180°)
>>> Poca luz: SUBIENDO cortina
raw:   40 | luz:   0% | cortina: ARRIBA ( 12°)
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

Necesito que generes un documento Word (.docx) en español, con formato de informe técnico de proyecto universitario, a partir de la información de abajo. El proyecto ya está terminado y funcionando; tu trabajo es documentarlo de forma clara, ordenada y bien presentada. No inventes datos: si necesitas algo que no esté aquí, déjalo marcado como "[pendiente]" para que yo lo complete.

## 1. Datos generales
- Título: "Simulador de cortinas automáticas controladas por luz con ESP32"
- Autor: [Tu nombre]
- Fecha: octubre de 2026
- Herramientas: Visual Studio Code + PlatformIO (framework Arduino), Git, Google Chrome (para el panel web).

## 2. Objetivo
Construir un simulador de cortina automática: un sensor de luz mide la iluminación ambiente y un servomotor baja o sube la cortina de forma **gradual y proporcional** a la cantidad de luz (más luz → más cerrada), evitando cambios bruscos, como lo haría una cortina motorizada real. Un panel web local muestra en tiempo real las lecturas y el estado de la cortina.

## 3. Componentes (lista de materiales)
| Componente | Detalle |
|---|---|
| Placa ESP32 DevKit V1 (30 pines) | Clon con conversor USB-serie CH340 y conector USB-C; se programa por el puerto COM3. Su auto-reset no funciona, así que para cargar firmware hay que mantener presionado el botón BOOT mientras conecta. |
| Módulo sensor de luz "MH-Sensor-Series" | Fotorresistencia (LDR, tipo GL5528) + comparador LM393 + potenciómetro (el potenciómetro solo afecta la salida digital DO). Pines, en el orden impreso en la placa: **AO, DO, GND, VCC**. Se usa la salida analógica AO. |
| Micro servo TowerPro SG90 (9 g) | Giro de 0 a 180°, alimentación 4.8-6 V, cables marrón (GND), rojo (VCC) y naranja (señal PWM a 50 Hz). |
| Portapilas 4×AA con 4 pilas Beston BST-AA 1500 mAh NiMH de 1.2 V | 4.8 V nominales (hasta ~5.6 V recién cargadas). Alimentan únicamente al servo. |
| Protoboard, jumpers macho-macho y hembra-hembra, cable USB-C | Para el montaje. |
| (Descartados) Sensor de humedad de suelo HW-080 (sonda) + HW-103 (módulo comparador) | Al inicio se intentó "simular" la luz con humedad; el módulo HW-103 se dañó por un cortocircuito y el enfoque se reemplazó por un sensor de luz real. |

## 4. Conexiones
**Sensor de luz → ESP32**
| Módulo | ESP32 |
|---|---|
| VCC | 3V3 (no 5 V: la salida AO superaría los 3.3 V que tolera el ADC) |
| GND | GND |
| AO | GPIO 34 (D34, entrada analógica ADC1) |
| DO | sin conectar |

**Servo SG90 alimentado con las pilas**
| Desde | Hacia |
|---|---|
| Pilas rojo (+) | Servo rojo (VCC), y a nada más |
| Pilas negro (−) | Servo marrón (GND) **y** GND del ESP32 (tierra común obligatoria) |
| ESP32 GPIO 18 (D18) | Servo naranja (señal) |

El LED integrado del ESP32 (GPIO 2) se enciende cuando la cortina está más cerrada que abierta. Advertencias: nunca conectar el positivo de las pilas a 3V3 ni a ningún pin del ESP32; el ESP32 se alimenta solo por USB; sin tierra común el servo no interpreta la señal.

## 5. Firmware (src/main.cpp; PlatformIO, placa esp32dev, librería ESP32Servo 3.x)
Flujo que se repite cada 500 ms:
1. **Lectura del sensor:** promedio de 10 muestras del ADC de 12 bits (0-4095) en GPIO 34.
2. **Conversión a lux estimados.** El módulo forma un divisor de voltaje VCC → R_fija (10 kΩ) → AO → LDR → GND, por lo que con más luz el voltaje en AO **baja**. Con r = raw / 4095: R_LDR = R_fija · r / (1 − r). Luego, con la curva típica de una GL5528 (R ≈ 32 · lux^(−0.7) kΩ): lux = (32 / R_LDR[kΩ])^(1 / 0.7). Es una estimación de orden de magnitud, no una medición calibrada (para lux exactos se necesitaría un sensor digital como el BH1750).
3. **Suavizado exponencial (filtro EMA):** luxSuavizado += 0.2 · (lux − luxSuavizado). Elimina el temblor que producía el ruido del sensor, amplificado por la curva exponencial.
4. **Mapeo lineal lux → ángulo:** 0° (cortina abierta) a 80 lux o menos (LUX_ABIERTA); 180° (cerrada) a 9000 lux o más (LUX_CERRADA); entre ambos, proporcional. Es un cierre gradual, no un interruptor todo-o-nada.
5. **Movimiento suave del servo:** avanza 1° cada 15 ms sin bloquear el loop (≈2.7 s para recorrer 180°). Pulsos de 500-2400 µs a 50 Hz.
6. **Mensajes por el puerto serie** a 115200 baudios, una línea por lectura: `raw: 620 | lux: 55.0 | cortina: ARRIBA ( 12°)`, y eventos al llegar a los extremos: `>>> Llega al tope de lux: cortina 100% cerrada` / `>>> Baja del piso de lux: cortina 100% abierta`.

Constantes ajustables: R_FIJO_KOHM = 10, LDR_COEF = 32, LDR_GAMMA = 0.7, LUX_ABIERTA = 80, LUX_CERRADA = 9000, FACTOR_SUAVIZADO = 0.2, ANGULO_SUBIDA = 0, ANGULO_BAJADA = 180, MS_POR_GRADO = 15, INTERVALO_LECTURA_MS = 500.

## 6. Panel web local (panel/index.html)
Página HTML sin dependencias externas, servida con `python -m http.server 8080 --directory panel` y abierta en Chrome o Edge (usa la API Web Serial para leer el puerto COM directamente desde el navegador). Muestra: una ventana animada cuya cortina baja según el ángulo del servo (el cielo pasa de noche a día según los lux, con sol, estrellas y nubes); los lux estimados con un medidor en escala logarítmica (1 a 10 000 lux) y los dos umbrales marcados; el porcentaje de cierre con barra de progreso; un indicador tipo gauge del servo (0-180°); una gráfica de los últimos 2 minutos en escala logarítmica con la zona de cierre gradual sombreada; una tabla de últimas lecturas y un registro de eventos. Tiene modo claro/oscuro automático y un "modo demo" que simula datos sin la placa. Limitación: en Windows solo un programa puede abrir el puerto COM a la vez (hay que cerrar el Monitor Serie de VS Code antes de conectar).

## 7. Calibración y medidas reales obtenidas
- Luz ambiente de un cuarto con luz artificial: ~30-80 lux (raw ≈ 500-900).
- Cuarto a oscuras: ~5-13 lux (raw ≈ 1400-2100).
- Linterna de un iPhone al máximo, pegada al sensor: ~10 200 lux (raw ≈ 0). Por eso el tope se fijó en 9000 lux.
- Con el tope anterior (1500 lux), más del 85 % del rango alcanzable con la linterna caía en "100 % cerrada" sin gradación; con 9000 todo el recorrido (lejos, en ángulo, cerca) se refleja en el ángulo del servo.
- Prueba final: con la linterna el servo avanza gradualmente hasta 180°; al retirarla vuelve gradualmente a 0°, sin temblar.

## 8. Problemas encontrados y cómo se resolvieron (bitácora)
1. **Cortocircuito inicial:** el módulo de humedad HW-103 se conectó con la polaridad invertida y a 5 V; el ESP32 se apagaba al conectar GND. El módulo quedó dañado.
2. **Cambio de enfoque:** en vez de "simular" luz con humedad, se compró un sensor de luz real (MH-Sensor-Series) y se eliminó toda la lógica de humedad del proyecto.
3. **"El sensor no detecta luz":** el orden de pines del módulo es AO-DO-GND-VCC, invertido respecto a lo habitual (VCC-GND-DO-AO); VCC y AO estaban cruzados. Al corregirlo, el rango de señal pasó de 0-90 a 0-2100.
4. **Lecturas en 0 en D34:** cable flojo y VCC del módulo desconectado; se diagnosticó con un puente directo 3V3 → D34 (que debe dar un valor alto).
5. **El ESP32 no entraba en modo de carga:** el clon no hace auto-reset; se carga manteniendo BOOT mientras esptool intenta conectar (se amplió connect_attempts).
6. **Porcentaje de luz poco significativo:** se cambió a lux estimados con el modelo del divisor de voltaje y la curva GL5528.
7. **Cortina "temblorosa"** (cambiaba de dirección varias veces por segundo): la curva amplifica el ruido del ADC; se añadió el filtro EMA. Se evaluó un filtro de Kalman y se descartó porque, para una sola variable sin modelo de velocidad, equivale al EMA con más complejidad.
8. **Panel congelado:** un script de prueba abrió el puerto COM mientras el navegador lo usaba; en Windows el puerto es exclusivo y la sesión del navegador quedó colgada. Solución: una sola conexión a la vez.
9. **Reinicios por el servo:** alimentado desde el USB, los picos de corriente del SG90 (hasta ~650 mA) pueden reiniciar el ESP32; se alimentó con el portapilas de 4×AA NiMH con tierra común.

## 9. Estructura del repositorio (git)
- `platformio.ini`: placa esp32dev, framework arduino, monitor a 115200, librería ESP32Servo.
- `src/main.cpp`: firmware.
- `panel/index.html`: panel web.
- `README.md`: conexiones, uso y calibración.
- `.gitignore`: .pio/, .vscode/, esptool.cfg.
Historial resumido de commits: proyecto inicial → corrección del cableado y cambio a lux → porcentaje de cortina en el panel → cierre gradual → tope de 9000 lux → suavizado EMA → cableado del servo con pilas → rediseño visual del panel.

## 10. Resultados y conclusiones (para que redactes)
El sistema funciona de punta a punta: sensor → lux → suavizado → cortina proporcional → servo con pilas → panel en vivo. Ideas para las conclusiones: la importancia de leer la serigrafía de cada módulo en vez de asumir el orden de pines; la tierra común entre fuentes; alimentar motores con una fuente separada; filtrar señales ruidosas antes de actuar; y el valor de tener un panel de diagnóstico en tiempo real.

## 11. Mejoras futuras (sugerencias)
Sensor digital BH1750 para lux reales; filtro de Kalman con modelo de velocidad; soltar el servo (detach) cuando está quieto para ahorrar pilas; servo de rotación continua o un carrete más grande para más recorrido; interruptor en el portapilas; caja o soporte para la maqueta.

## 12. Formato del documento Word
- Portada (título, autor, fecha), índice automático y numeración de páginas.
- Secciones: 1 Introducción y objetivo · 2 Marco teórico breve (fotorresistencia y divisor de voltaje, ADC del ESP32, control de servo por PWM, filtro exponencial) · 3 Materiales (tabla) · 4 Diseño y conexiones (tablas de pines y un esquema textual; deja un marcador "[insertar foto del montaje]") · 5 Desarrollo del firmware (explica el flujo y las fórmulas; incluye fragmentos de código clave) · 6 Panel web · 7 Calibración y pruebas (tabla de medidas) · 8 Problemas y soluciones · 9 Resultados · 10 Conclusiones · 11 Mejoras futuras · Anexo A: código fuente completo (te adjunto aparte src/main.cpp y panel/index.html; pégalos tal cual).
- Estilo: formal pero claro, en español, títulos jerarquizados, tablas con encabezado, fórmulas legibles.
- Extensión aproximada: 10-15 páginas sin contar el anexo.

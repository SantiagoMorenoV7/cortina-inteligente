/*
 * Simulador de cortinas automáticas con ESP32
 * --------------------------------------------
 * Sensor de luz: módulo MH-Sensor-Series (fotorresistencia/LDR + LM393).
 * Cableado del módulo (letras impresas de izquierda a derecha):
 *   AO -> D34   |   DO -> sin conectar   |   GND -> GND   |   VCC -> 3V3
 *
 *   Muchos lux -> se BAJA la cortina
 *   Pocos lux  -> se SUBE la cortina
 *
 * La cortina se representa con un servo (SG90) y el LED de la placa
 * se enciende mientras la cortina está abajo.
 *
 * Nota sobre los lux: este módulo es una fotorresistencia genérica sin hoja
 * de datos exacta, así que el valor de lux es una ESTIMACIÓN (orden de
 * magnitud), calculada con la curva típica de una GL5528. No es una medición
 * calibrada; para lux precisos se necesitaría un sensor digital (p. ej.
 * BH1750). Aun así sirve de sobra para este simulador.
 */

#include <Arduino.h>
#include <math.h>
#include <ESP32Servo.h>

// ---------------- Pines ----------------
const int PIN_SENSOR = 34;  // AO del módulo de luz (debe ser un pin ADC1: 32-39)
const int PIN_SERVO  = 18;  // Señal del servo
const int PIN_LED    = 2;   // LED integrado del ESP32 DevKit

// ------------- Modelo del sensor (estimación de lux) -------------
// El módulo forma un divisor de voltaje: VCC -> R_FIJO -> (AO) -> LDR -> GND.
// R_FIJO_KOHM es el valor típico de la resistencia fija del módulo (10kΩ en
// la mayoría de estos módulos LM393). Si el rango de lux se ve muy
// desviado de la realidad, ajusta este valor.
const float R_FIJO_KOHM = 10.0;

// Curva aproximada de una fotorresistencia GL5528: R(kΩ) ≈ 32 * lux^-0.7
const float LDR_COEF  = 32.0;
const float LDR_GAMMA = 0.7;

// ------------- Umbrales (con histéresis) -------------
// Entre los dos umbrales la cortina se queda como está, así no "tiembla"
// cuando la lectura está justo en el límite. Ajusta estos valores viendo
// los lux reales de tu cuarto en el Monitor Serie o en localhost:8080.
const float LUX_PARA_BAJAR = 300.0;  // lux a partir del cual se baja
const float LUX_PARA_SUBIR = 100.0;  // lux por debajo del cual se sube

// ------------- Servo -------------
const int ANGULO_SUBIDA = 0;
const int ANGULO_BAJADA = 180;
const int MS_POR_GRADO  = 15;   // velocidad del movimiento (más alto = más lento)

const unsigned long INTERVALO_LECTURA_MS = 500;

enum EstadoCortina { SUBIDA, BAJADA };

Servo servo;
EstadoCortina estado;
int anguloActual;
int anguloObjetivo;
unsigned long ultimoPasoServo = 0;
unsigned long ultimaLectura = 0;

int leerSensorPromedio() {
  long suma = 0;
  for (int i = 0; i < 10; i++) {
    suma += analogRead(PIN_SENSOR);
    delay(2);
  }
  return suma / 10;
}

// Convierte la lectura cruda del ADC (0-4095) a una estimación de lux.
float calcularLux(int raw) {
  float relacion = constrain(raw, 1, 4094) / 4095.0;  // evita 0% y 100% exactos
  float rLdrKohm = R_FIJO_KOHM * relacion / (1.0 - relacion);
  if (rLdrKohm < 0.05) rLdrKohm = 0.05;  // evita valores irreales cerca de 0
  return pow(LDR_COEF / rLdrKohm, 1.0 / LDR_GAMMA);
}

void cambiarEstado(EstadoCortina nuevo) {
  estado = nuevo;
  anguloObjetivo = (estado == BAJADA) ? ANGULO_BAJADA : ANGULO_SUBIDA;
  digitalWrite(PIN_LED, estado == BAJADA ? HIGH : LOW);
  Serial.println(estado == BAJADA ? ">>> Muchos lux: BAJANDO cortina"
                                  : ">>> Pocos lux: SUBIENDO cortina");
}

// Mueve el servo un grado cada MS_POR_GRADO sin bloquear el loop.
void moverServoSuave() {
  if (anguloActual == anguloObjetivo) return;
  if (millis() - ultimoPasoServo < MS_POR_GRADO) return;
  ultimoPasoServo = millis();
  anguloActual += (anguloObjetivo > anguloActual) ? 1 : -1;
  servo.write(anguloActual);
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_LED, OUTPUT);
  analogReadResolution(12);  // 0 - 4095

  ESP32PWM::allocateTimer(0);
  servo.setPeriodHertz(50);
  servo.attach(PIN_SERVO, 500, 2400);

  // Estado inicial según la primera lectura, sin animación.
  float lux = calcularLux(leerSensorPromedio());
  cambiarEstado(lux >= LUX_PARA_BAJAR ? BAJADA : SUBIDA);
  anguloActual = anguloObjetivo;
  servo.write(anguloActual);

  Serial.println("Simulador de cortinas listo.");
}

void loop() {
  moverServoSuave();

  if (millis() - ultimaLectura < INTERVALO_LECTURA_MS) return;
  ultimaLectura = millis();

  int raw = leerSensorPromedio();
  float lux = calcularLux(raw);

  if (estado == SUBIDA && lux >= LUX_PARA_BAJAR) {
    cambiarEstado(BAJADA);
  } else if (estado == BAJADA && lux <= LUX_PARA_SUBIR) {
    cambiarEstado(SUBIDA);
  }

  Serial.printf("raw: %4d | lux: %7.1f | cortina: %s (%d°)\n",
                raw, lux, estado == BAJADA ? "ABAJO " : "ARRIBA", anguloActual);
}

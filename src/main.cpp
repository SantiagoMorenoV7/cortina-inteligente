/*
 * Simulador de cortinas automáticas con ESP32
 * --------------------------------------------
 * Sensor de luz: módulo MH-Sensor-Series (fotorresistencia/LDR + LM393).
 * Cableado del módulo (letras impresas de izquierda a derecha):
 *   AO -> D34   |   DO -> sin conectar   |   GND -> GND   |   VCC -> 3V3
 *
 * La cortina se mueve de forma GRADUAL y proporcional a la cantidad de luz:
 * con poca luz queda abierta, con mucha luz queda cerrada, y con valores
 * intermedios (por ejemplo la linterna de un celular desde lejos o en
 * ángulo) queda parcialmente cerrada. No es un interruptor de todo o nada.
 *
 * La cortina se representa con un servo (SG90) y el LED de la placa se
 * enciende cuando está más cerrada que abierta (pasado el punto medio).
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

// ------------- Rango de lux para la posición de la cortina -------------
// Entre estos dos valores la cortina se mueve GRADUALMENTE, en proporción
// directa a los lux medidos. Pensado para probar con la linterna de un
// celular desde distintas distancias, ángulos e intensidades: mientras más
// cerca y directo apunte, más se cierra; de lejos o en ángulo, se cierra
// solo un poco.
//   LUX_ABIERTA: en este nivel de lux (o menos) la cortina queda 100% abierta.
//   LUX_CERRADA: en este nivel de lux (o más) la cortina queda 100% cerrada.
// Si notas que cualquier lucecita ya cierra la cortina del todo, sube
// LUX_CERRADA. Revisa los lux reales de tu cuarto en el Monitor Serie o en
// el panel localhost:8080 antes de ajustar.
const float LUX_ABIERTA = 80.0;
const float LUX_CERRADA = 9000.0;  // la linterna de un iPhone al máximo y cerca dio ~10.2k lux

// ------------- Servo -------------
const int ANGULO_SUBIDA = 0;
const int ANGULO_BAJADA = 180;
const int MS_POR_GRADO  = 15;   // velocidad del movimiento (más alto = más lento)

const unsigned long INTERVALO_LECTURA_MS = 500;

int anguloActual;
int anguloObjetivo;
unsigned long ultimoPasoServo = 0;
unsigned long ultimaLectura = 0;

Servo servo;

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

// Traduce los lux a un ángulo de servo, interpolando linealmente entre
// LUX_ABIERTA (0°) y LUX_CERRADA (180°). Esto es lo que hace que el cierre
// sea gradual en vez de un interruptor todo-o-nada.
int calcularAnguloObjetivo(float lux) {
  float acotado = constrain(lux, LUX_ABIERTA, LUX_CERRADA);
  float fraccion = (acotado - LUX_ABIERTA) / (LUX_CERRADA - LUX_ABIERTA);
  return (int)round(ANGULO_SUBIDA + fraccion * (ANGULO_BAJADA - ANGULO_SUBIDA));
}

// Mueve el servo un grado cada MS_POR_GRADO sin bloquear el loop.
void moverServoSuave() {
  if (anguloActual == anguloObjetivo) return;
  if (millis() - ultimoPasoServo < MS_POR_GRADO) return;
  ultimoPasoServo = millis();
  anguloActual += (anguloObjetivo > anguloActual) ? 1 : -1;
  servo.write(anguloActual);
  digitalWrite(PIN_LED, anguloActual > 90 ? HIGH : LOW);
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_LED, OUTPUT);
  analogReadResolution(12);  // 0 - 4095

  ESP32PWM::allocateTimer(0);
  servo.setPeriodHertz(50);
  servo.attach(PIN_SERVO, 500, 2400);

  // Posición inicial según la primera lectura, sin animación.
  anguloObjetivo = calcularAnguloObjetivo(calcularLux(leerSensorPromedio()));
  anguloActual = anguloObjetivo;
  servo.write(anguloActual);
  digitalWrite(PIN_LED, anguloActual > 90 ? HIGH : LOW);

  Serial.println("Simulador de cortinas listo.");
}

void loop() {
  moverServoSuave();

  if (millis() - ultimaLectura < INTERVALO_LECTURA_MS) return;
  ultimaLectura = millis();

  int raw = leerSensorPromedio();
  float lux = calcularLux(raw);
  int nuevoObjetivo = calcularAnguloObjetivo(lux);

  if (nuevoObjetivo == ANGULO_BAJADA && anguloObjetivo != ANGULO_BAJADA) {
    Serial.println(">>> Llega al tope de lux: cortina 100% cerrada");
  } else if (nuevoObjetivo == ANGULO_SUBIDA && anguloObjetivo != ANGULO_SUBIDA) {
    Serial.println(">>> Baja del piso de lux: cortina 100% abierta");
  }
  anguloObjetivo = nuevoObjetivo;

  Serial.printf("raw: %4d | lux: %7.1f | cortina: %s (%d°)\n",
                raw, lux, anguloActual > 90 ? "ABAJO " : "ARRIBA", anguloActual);
}

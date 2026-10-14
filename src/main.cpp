/*
 * Simulador de cortinas automáticas con ESP32
 * --------------------------------------------
 * Sensor de luz: módulo MH-Sensor-Series (fotorresistencia/LDR + LM393).
 *
 *   Mucha luz -> se BAJA la cortina
 *   Poca luz  -> se SUBE la cortina
 *
 * La cortina se representa con un servo (SG90) y el LED de la placa
 * se enciende mientras la cortina está abajo.
 */

#include <Arduino.h>
#include <ESP32Servo.h>

// ---------------- Pines ----------------
const int PIN_SENSOR = 34;  // AO del módulo de luz (debe ser un pin ADC1: 32-39)
const int PIN_SERVO  = 18;  // Señal del servo
const int PIN_LED    = 2;   // LED integrado del ESP32 DevKit

// ------------- Calibración del sensor -------------
// Abre el Monitor Serie (o el panel localhost:8080), mira el "raw" y ajusta
// estos dos valores:
//   VALOR_OSCURO: raw con el sensor tapado / en oscuridad
//   VALOR_CLARO:  raw con una luz o linterna pegada al sensor
// En este módulo el sensor funciona al revés de lo habitual: entre más luz,
// el raw es MÁS BAJO. No importa cuál de los dos valores sea más grande,
// el mapeo (map()) se ajusta solo.
const int VALOR_OSCURO = 120;  // casi a oscuras dio 62-70; se deja margen
const int VALOR_CLARO  = 0;    // con linterna pegada dio 0

// ------------- Umbrales (con histéresis) -------------
// Entre los dos umbrales la cortina se queda como está, así no "tiembla"
// cuando la lectura está justo en el límite.
const int LUZ_PARA_BAJAR = 60;  // % de luz a partir del cual se baja
const int LUZ_PARA_SUBIR = 40;  // % de luz por debajo del cual se sube

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

int calcularLuz(int raw) {
  int luz = map(raw, VALOR_OSCURO, VALOR_CLARO, 0, 100);
  return constrain(luz, 0, 100);
}

void cambiarEstado(EstadoCortina nuevo) {
  estado = nuevo;
  anguloObjetivo = (estado == BAJADA) ? ANGULO_BAJADA : ANGULO_SUBIDA;
  digitalWrite(PIN_LED, estado == BAJADA ? HIGH : LOW);
  Serial.println(estado == BAJADA ? ">>> Mucha luz: BAJANDO cortina"
                                  : ">>> Poca luz: SUBIENDO cortina");
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
  int luz = calcularLuz(leerSensorPromedio());
  cambiarEstado(luz >= LUZ_PARA_BAJAR ? BAJADA : SUBIDA);
  anguloActual = anguloObjetivo;
  servo.write(anguloActual);

  Serial.println("Simulador de cortinas listo.");
}

void loop() {
  moverServoSuave();

  if (millis() - ultimaLectura < INTERVALO_LECTURA_MS) return;
  ultimaLectura = millis();

  int raw = leerSensorPromedio();
  int luz = calcularLuz(raw);

  if (estado == SUBIDA && luz >= LUZ_PARA_BAJAR) {
    cambiarEstado(BAJADA);
  } else if (estado == BAJADA && luz <= LUZ_PARA_SUBIR) {
    cambiarEstado(SUBIDA);
  }

  Serial.printf("raw: %4d | luz: %3d%% | cortina: %s (%d°)\n",
                raw, luz, estado == BAJADA ? "ABAJO " : "ARRIBA", anguloActual);
}

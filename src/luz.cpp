#include "luz.h"
#include "ajustes.h"
#include "config.h"

static InfoLuz info;
static float ventana[MUESTRAS_PROMEDIO];
static int idxVentana = 0;
static int llenos = 0;
static uint32_t ultimaMuestra = 0;

// Calibración del ruido: 50 lecturas (5 s) con la luz quieta.
static const int MUESTRAS_CALIBRACION = 50;
static int calCuenta = 0;
static double calSuma = 0, calSuma2 = 0;

void luzIniciar() {
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_LDR, ADC_11db);  // rango completo hasta ~3,1 V
  info = InfoLuz{};
  luzAplicarAjustes();
}

void luzAplicarAjustes() {
  info.kalman.q = ajustes.kalmanQ;
  info.kalman.r = ajustes.kalmanR;
}

static float leerN() {
  // 4 conversiones seguidas para reducir el ruido propio del ADC del ESP32.
  uint32_t suma = 0;
  for (int i = 0; i < 4; i++) suma += analogReadMilliVolts(PIN_LDR);
  const float mv = suma / 4.0f;
  info.mv = (uint16_t)mv;
  float n = mv / VREF_MV * 1023.0f;
  n = constrain(n, 0.0f, 1023.0f);
  if (ajustes.invertirSensor) n = 1023.0f - n;
  return n;
}

bool luzActualizar() {
  const uint32_t ahora = millis();
  if (ultimaMuestra != 0 && ahora - ultimaMuestra < PERIODO_MUESTRA_MS) return false;
  const float dt = ultimaMuestra == 0 ? PERIODO_MUESTRA_MS / 1000.0f
                                      : constrain((ahora - ultimaMuestra) / 1000.0f, 0.02f, 1.0f);
  ultimaMuestra = ahora;

  const float n = leerN();
  info.n = n;

  // Promedio móvil de 10 lecturas (1 s), como en el informe.
  ventana[idxVentana] = n;
  idxVentana = (idxVentana + 1) % MUESTRAS_PROMEDIO;
  if (llenos < MUESTRAS_PROMEDIO) llenos++;
  float suma = 0;
  for (int i = 0; i < llenos; i++) suma += ventana[i];
  info.promedio = suma / llenos;

  // Kalman
  info.kalman.actualizar(n, dt);
  info.filtrada = ajustes.filtro == F_KALMAN ? constrain(info.kalman.n, 0.0f, 1023.0f)
                                             : info.promedio;

  // Calibración del ruido (varianza muestral → R)
  if (info.calibrando) {
    calSuma += n;
    calSuma2 += (double)n * n;
    calCuenta++;
    info.progresoCalibracion = (uint8_t)(calCuenta * 100 / MUESTRAS_CALIBRACION);
    if (calCuenta >= MUESTRAS_CALIBRACION) {
      const double media = calSuma / calCuenta;
      double var = (calSuma2 - calCuenta * media * media) / (calCuenta - 1);
      if (var < 1.0) var = 1.0;          // el ADC nunca es perfecto
      if (var > 10000.0) var = 10000.0;  // la luz cambió durante la medición
      ajustes.kalmanR = (float)var;
      guardarAjustes();
      luzAplicarAjustes();
      info.calibrando = false;
      info.progresoCalibracion = 100;
    }
  }
  return true;
}

const InfoLuz& luzInfo() { return info; }

void luzIniciarCalibracionRuido() {
  calCuenta = 0;
  calSuma = calSuma2 = 0;
  info.progresoCalibracion = 0;
  info.calibrando = true;
}

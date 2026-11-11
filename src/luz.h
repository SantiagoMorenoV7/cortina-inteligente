#pragma once
/*
 * Lectura del LDR (divisor de tensión con resistencia fija de 4,7 kΩ).
 *
 * Cada 100 ms se toma una lectura y se lleva a la escala 0-1023 del informe:
 *     N = Vout / Vref · 1023        (Vref = 3,3 V en el ESP32)
 * Como el divisor también se alimenta con 3,3 V, N depende solo de la
 * relación entre resistencias, igual que en el Arduino a 5 V; por eso los
 * umbrales 320 y 620 del informe siguen siendo válidos.
 *
 * Sobre esa lectura se calculan dos señales filtradas:
 *   - el promedio de las últimas 10 lecturas (método del informe), y
 *   - el filtro de Kalman de 2 estados (kalman.h).
 * La automatización usa la que se elija en los ajustes (Kalman por defecto).
 */
#include <Arduino.h>
#include "kalman.h"

struct InfoLuz {
  float n;          // última lectura (0-1023)
  uint16_t mv;      // voltaje medido en el pin (mV)
  float promedio;   // promedio de las últimas 10 lecturas
  float filtrada;   // la señal que usa la automatización
  Kalman2 kalman;   // estado completo del filtro
  bool calibrando;
  uint8_t progresoCalibracion;  // 0-100
};

void luzIniciar();
bool luzActualizar();               // true si hubo una lectura nueva
const InfoLuz& luzInfo();
void luzAplicarAjustes();           // relee q, r, filtro e inversión
void luzIniciarCalibracionRuido();  // mide la varianza durante 5 s y la usa como R

#pragma once
/*
 * Filtro de Kalman de 2 estados para la lectura de luz.
 *
 * Estado:     x = [ n ]   n = nivel de luz (escala 0-1023)
 *                 [ v ]   v = velocidad de cambio (unidades/s)
 *
 * Modelo (la luz cambia de forma suave y su velocidad se "olvida" en ~0,7 s):
 *     x_k = F x_{k-1} + w,     F = [ 1  dt ]      a = e^(−dt/τ),  τ = 0,7 s
 *                                  [ 0   a ]      (a ≈ 0,867 con dt = 0,1 s)
 *     z_k = H x_k + ruido,     H = [ 1  0 ]
 *
 * Q = q · [ dt⁴/4  dt³/2 ]   (ruido de proceso: aceleración aleatoria)
 *         [ dt³/2  dt²   ]
 * R = r                      (varianza del ruido del sensor)
 *
 * Predicción:     x⁻ = F x            P⁻ = F P Fᵀ + Q
 * Corrección:     y = z − H x⁻        S = H P⁻ Hᵀ + R
 *                 K = P⁻ Hᵀ S⁻¹       x = x⁻ + K y
 *                 P = (I − K H) P⁻
 *
 * Como H solo toma la primera componente, S es un escalar y la inversa
 * S⁻¹ es simplemente 1/S: no hace falta invertir matrices en el micro.
 *
 * ¿Por qué el factor a < 1? Con a = 1 (velocidad constante) el filtro se
 * "pasa de largo" en los cambios bruscos: un salto de luz de 100 a 580 lo
 * llevaba hasta 671 durante 3 s, y eso cruzaba el umbral de 620 y cerraba
 * la cortina sin motivo. Con τ = 0,7 s el peor pico (200 simulaciones con
 * ruido) queda en 591 y la respuesta a un cambio real sigue siendo de 0,9 s.
 *
 * No depende de Arduino: la misma lógica está replicada en la app (modo
 * demostración) y ambas se verificaron numéricamente con los mismos datos.
 */
#include <math.h>

struct Kalman2 {
  // Estado y covarianza
  float n = 0, v = 0;
  float p00 = 0, p01 = 0, p10 = 0, p11 = 0;
  // Parámetros
  float q = 100, r = 25;
  float tau = 0.7f;       // constante de tiempo del amortiguamiento de v (s)
  // Diagnóstico de la última iteración (para la app)
  float k0 = 0, k1 = 0;   // ganancia de Kalman
  float y = 0;            // innovación (medición − predicción)
  float a = 1;            // último factor de amortiguamiento usado en F
  bool iniciado = false;

  void reiniciar(float n0) {
    n = n0;
    v = 0;
    p00 = r;      // la primera medición tiene la incertidumbre del sensor
    p01 = p10 = 0;
    p11 = 1000;   // la velocidad inicial es desconocida
    k0 = k1 = y = 0;
    iniciado = true;
  }

  float actualizar(float z, float dt) {
    if (!iniciado) {
      reiniciar(z);
      return n;
    }
    if (!(dt > 0)) dt = 0.1f;  // también cubre NaN
    a = expf(-dt / tau);

    // --- Predicción: x⁻ = F x,  P⁻ = F P Fᵀ + Q ---
    const float dt2 = dt * dt, dt3 = dt2 * dt, dt4 = dt3 * dt;
    const float nP = n + dt * v;
    const float vP = a * v;
    const float a00 = p00 + dt * (p10 + p01) + dt2 * p11 + q * dt4 * 0.25f;
    const float a01 = a * (p01 + dt * p11) + q * dt3 * 0.5f;
    const float a10 = a * (p10 + dt * p11) + q * dt3 * 0.5f;
    const float a11 = a * a * p11 + q * dt2;

    // --- Corrección ---
    y = z - nP;
    const float s = a00 + r;
    k0 = a00 / s;
    k1 = a10 / s;
    n = nP + k0 * y;
    v = vP + k1 * y;
    p00 = (1 - k0) * a00;
    p01 = (1 - k0) * a01;
    p10 = a10 - k1 * a00;
    p11 = a11 - k1 * a01;
    // P debe ser simétrica; se corrige el error de redondeo.
    const float m = 0.5f * (p01 + p10);
    p01 = p10 = m;

    // Protección ante valores no finitos.
    if (!isfinite(n) || !isfinite(p00) || !isfinite(p11)) reiniciar(z);
    return n;
  }
};

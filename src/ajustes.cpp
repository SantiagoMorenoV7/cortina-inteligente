#include "ajustes.h"
#include <Preferences.h>
#include "config.h"

Ajustes ajustes;

// Si cambia la estructura Ajustes, sube este número: los ajustes guardados
// con otra versión se descartan y se usan los valores por defecto.
static const uint32_t VERSION_AJUSTES = 4;

static void copiar(char* destino, size_t tam, const char* origen) {
  strncpy(destino, origen, tam - 1);
  destino[tam - 1] = '\0';
}

void ajustesPorDefecto(Ajustes& a) {
  memset(&a, 0, sizeof(a));
  copiar(a.nombre, sizeof(a.nombre), "Cortina de la sala");
  a.umbralAbrir = DEF_UMBRAL_ABRIR;
  a.umbralCerrar = DEF_UMBRAL_CERRAR;
  a.confirmarMs = DEF_CONFIRMAR_MS;
  a.reanudarMin = DEF_REANUDAR_MIN;
  a.filtro = F_KALMAN;
  a.kalmanR = DEF_KALMAN_R;
  a.kalmanQ = DEF_KALMAN_Q;
  a.invertirSensor = false;
  a.recorrido = DEF_RECORRIDO;
  a.msPorPaso = DEF_MS_POR_PASO;
  a.invertirMotor = false;
  a.usarFinales = true;
  a.zonaMin = DEF_ZONA_MIN;
  copiar(a.apClave, sizeof(a.apClave), DEF_AP_CLAVE);
  a.numHorarios = 0;
}

void cargarAjustes() {
  ajustesPorDefecto(ajustes);
  Preferences p;
  if (!p.begin("cortina", true)) return;  // aún no existe: valores por defecto
  const uint32_t version = p.getUInt("ver", 0);
  if (version == VERSION_AJUSTES && p.getBytesLength("cfg") == sizeof(Ajustes)) {
    p.getBytes("cfg", &ajustes, sizeof(Ajustes));
    // Garantiza cadenas terminadas aunque la flash tenga basura.
    ajustes.nombre[sizeof(ajustes.nombre) - 1] = '\0';
    ajustes.apClave[sizeof(ajustes.apClave) - 1] = '\0';
    ajustes.staSsid[sizeof(ajustes.staSsid) - 1] = '\0';
    ajustes.staClave[sizeof(ajustes.staClave) - 1] = '\0';
    if (ajustes.numHorarios > MAX_HORARIOS) ajustes.numHorarios = 0;
  }
  p.end();
}

void guardarAjustes() {
  Preferences p;
  if (!p.begin("cortina", false)) return;
  p.putUInt("ver", VERSION_AJUSTES);
  p.putBytes("cfg", &ajustes, sizeof(Ajustes));
  p.end();
}

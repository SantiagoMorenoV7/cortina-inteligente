#include "motor.h"
#include <atomic>
#include "config.h"

// ---------------- Comandos (cola entre el programa y la tarea del motor) ----------------
enum TipoComando : uint8_t {
  C_IR_A,         // valor = pasos objetivo (dentro del recorrido)
  C_RELATIVO,     // valor = pasos a mover (calibración)
  C_PARAR,
  C_BUSCAR,
  C_FIJAR_CERO,
};

struct Comando {
  TipoComando tipo;
  int32_t valor;
};

static QueueHandle_t cola = nullptr;

// ---------------- Estado compartido (lo escribe la tarea, lo lee cualquiera) ----------------
static std::atomic<int32_t> aPos{0};
static std::atomic<int32_t> aObj{0};
static std::atomic<int32_t> aRecorrido{DEF_RECORRIDO};
static std::atomic<uint8_t> aEstado{M_REPOSO};
static std::atomic<uint8_t> aError{E_NINGUNO};
static std::atomic<int8_t> aDir{0};
static std::atomic<bool> aFcA{false};
static std::atomic<bool> aFcC{false};
static std::atomic<bool> aReferenciado{false};
static std::atomic<uint32_t> aPasosTotales{0};

// Configuración (la escribe motorConfigurar, la lee la tarea)
static std::atomic<int> aMsPorPaso{DEF_MS_POR_PASO};
static std::atomic<bool> aInvertir{false};
static std::atomic<bool> aUsarFinales{true};

// Secuencia de paso completo con dos bobinas activas (más torque).
// Columnas: IN1, IN2, IN3, IN4.
static const uint8_t SECUENCIA[4][4] = {
  {1, 1, 0, 0},
  {0, 1, 1, 0},
  {0, 0, 1, 1},
  {1, 0, 0, 1},
};
static const int PINES[4] = {PIN_IN1, PIN_IN2, PIN_IN3, PIN_IN4};

static void escribirFase(int fase) {
  for (int i = 0; i < 4; i++) digitalWrite(PINES[i], SECUENCIA[fase][i] ? HIGH : LOW);
}

static void apagarBobinas() {
  for (int i = 0; i < 4; i++) digitalWrite(PINES[i], LOW);
}

// Lectura de un final de carrera con antirrebote: se exigen 2 lecturas
// seguidas en LOW para darlo por presionado. "posible" indica que la
// lectura actual está en LOW pero aún no se confirmó; en ese caso la tarea
// espera un ciclo sin dar el paso.
struct FinalCarrera {
  int pin;
  uint8_t cuenta = 0;
  bool presionado = false;
  bool posible = false;

  explicit FinalCarrera(int p) : pin(p) {}

  void leer() {
    if (digitalRead(pin) == LOW) {
      if (cuenta < 2) cuenta++;
    } else {
      cuenta = 0;
    }
    presionado = cuenta >= 2;
    posible = cuenta == 1;
  }
};

static void tareaMotor(void*) {
  FinalCarrera fcA{PIN_FC_ABIERTA};
  FinalCarrera fcC{PIN_FC_CERRADA};

  int32_t pos = 0;
  int32_t obj = 0;
  bool buscarTope = false;    // el objetivo es un extremo: avanzar hasta el interruptor
  int32_t pasosBusqueda = 0;  // pasos dados buscando la referencia
  EstadoMotor estado = M_REPOSO;
  int fase = 0;
  bool bobinasOn = false;
  TickType_t ultimo = xTaskGetTickCount();

  auto terminar = [&](EstadoMotor nuevo, ErrorMotor err) {
    estado = nuevo;
    obj = pos;
    buscarTope = false;
    apagarBobinas();
    bobinasOn = false;
    aError = err;
    aDir = 0;
  };

  auto darPaso = [&](int dir) {
    const int sentido = aInvertir ? -dir : dir;
    fase = (fase + sentido + 4) & 3;
    escribirFase(fase);
    bobinasOn = true;
    pos += dir;
    aPasosTotales++;
    aDir = dir;
  };

  for (;;) {
    // ---------- 1. Comandos ----------
    // En reposo se espera el comando hasta 10 ms (la tarea "duerme");
    // en movimiento solo se revisa si hay alguno, sin esperar.
    const bool enMovimiento = estado == M_MOVIENDO || estado == M_BUSCANDO;
    Comando c;
    TickType_t espera = enMovimiento ? 0 : pdMS_TO_TICKS(10);
    while (xQueueReceive(cola, &c, espera) == pdTRUE) {
      espera = 0;
      const int32_t rec = aRecorrido;
      const bool usarFc = aUsarFinales;
      switch (c.tipo) {
        case C_IR_A:
          obj = constrain(c.valor, (int32_t)0, rec);
          buscarTope = usarFc && (obj == 0 || obj == rec);
          estado = M_MOVIENDO;
          aError = E_NINGUNO;
          break;
        case C_RELATIVO:
          obj = pos + c.valor;
          buscarTope = false;
          estado = M_MOVIENDO;
          aError = E_NINGUNO;
          break;
        case C_PARAR:
          terminar(M_REPOSO, E_NINGUNO);
          break;
        case C_BUSCAR:
          if (usarFc) {
            estado = M_BUSCANDO;
            pasosBusqueda = 0;
            aError = E_NINGUNO;
          } else {
            // Sin finales de carrera no hay cómo buscar: se asume abierta.
            pos = 0;
            terminar(M_REPOSO, E_NINGUNO);
            aReferenciado = true;
          }
          break;
        case C_FIJAR_CERO:
          pos = 0;
          terminar(M_REPOSO, E_NINGUNO);
          aReferenciado = true;
          break;
      }
      if ((estado == M_MOVIENDO || estado == M_BUSCANDO) && !enMovimiento) {
        ultimo = xTaskGetTickCount();  // arranca el ritmo de pasos desde ahora
      }
    }

    // ---------- 2. Finales de carrera ----------
    const bool usarFc = aUsarFinales;
    const int32_t rec = aRecorrido;
    fcA.leer();
    fcC.leer();
    aFcA = usarFc && fcA.presionado;
    aFcC = usarFc && fcC.presionado;

    // ---------- 3. Movimiento ----------
    if (estado == M_BUSCANDO) {
      if (fcA.presionado) {
        pos = 0;
        aReferenciado = true;
        terminar(M_REPOSO, E_NINGUNO);
      } else if (!fcA.posible) {
        if (pasosBusqueda >= rec + MARGEN_FINAL_CARRERA) {
          terminar(M_ERROR, E_SIN_FC_ABIERTA);
        } else {
          darPaso(-1);
          pasosBusqueda++;
        }
      }
    } else if (estado == M_MOVIENDO) {
      int dir = (obj > pos) ? 1 : (obj < pos) ? -1 : 0;
      if (buscarTope && dir == 0) dir = (obj == 0) ? -1 : 1;  // seguir hasta el interruptor

      if (dir == 0) {
        terminar(M_REPOSO, E_NINGUNO);
      } else if (usarFc && dir < 0 && fcA.presionado) {
        pos = 0;  // llegó a "abierta": se corrige cualquier paso perdido
        aReferenciado = true;
        terminar(M_REPOSO, E_NINGUNO);
      } else if (usarFc && dir > 0 && fcC.presionado) {
        pos = rec;
        aReferenciado = true;
        terminar(M_REPOSO, E_NINGUNO);
      } else if (usarFc && ((dir < 0 && fcA.posible) || (dir > 0 && fcC.posible))) {
        // posible pulsación: se espera un ciclo para confirmarla
      } else if (buscarTope && dir < 0 && pos <= -MARGEN_FINAL_CARRERA) {
        terminar(M_ERROR, E_SIN_FC_ABIERTA);
      } else if (buscarTope && dir > 0 && pos >= rec + MARGEN_FINAL_CARRERA) {
        terminar(M_ERROR, E_SIN_FC_CERRADA);
      } else {
        darPaso(dir);
      }
    }

    aPos = pos;
    aObj = obj;
    aEstado = estado;

    // ---------- 4. Ritmo ----------
    if (estado == M_MOVIENDO || estado == M_BUSCANDO) {
      const int ms = constrain((int)aMsPorPaso, 2, 20);
      vTaskDelayUntil(&ultimo, pdMS_TO_TICKS(ms));
    } else if (bobinasOn) {
      apagarBobinas();  // por seguridad: nunca dejar bobinas encendidas en reposo
      bobinasOn = false;
    }
  }
}

static void enviar(TipoComando tipo, int32_t valor = 0) {
  if (!cola) return;
  Comando c{tipo, valor};
  xQueueSend(cola, &c, pdMS_TO_TICKS(50));
}

void motorIniciar() {
  for (int i = 0; i < 4; i++) {
    pinMode(PINES[i], OUTPUT);
    digitalWrite(PINES[i], LOW);
  }
  pinMode(PIN_FC_ABIERTA, INPUT_PULLUP);
  pinMode(PIN_FC_CERRADA, INPUT_PULLUP);

  cola = xQueueCreate(8, sizeof(Comando));
  // Núcleo 1 (el del loop) con más prioridad que el loop, para que el
  // ritmo de pasos no dependa de lo que esté haciendo el servidor web.
  xTaskCreatePinnedToCore(tareaMotor, "motor", 4096, nullptr, 3, nullptr, 1);
}

void motorConfigurar(int32_t recorrido, int msPorPaso, bool invertir, bool usarFinales) {
  aRecorrido = max((int32_t)1, recorrido);
  aMsPorPaso = msPorPaso;
  aInvertir = invertir;
  aUsarFinales = usarFinales;
}

void motorAbrir() { enviar(C_IR_A, 0); }
void motorCerrar() { enviar(C_IR_A, aRecorrido); }
void motorIrA(int32_t pasos) { enviar(C_IR_A, pasos); }
void motorParar() { enviar(C_PARAR); }
void motorMoverRelativo(int32_t pasos) { enviar(C_RELATIVO, pasos); }
void motorBuscarOrigen() { enviar(C_BUSCAR); }
void motorFijarAbierta() { enviar(C_FIJAR_CERO); }

InfoMotor motorInfo() {
  InfoMotor i;
  i.pos = aPos;
  i.objetivo = aObj;
  i.recorrido = aRecorrido;
  i.estado = (EstadoMotor)aEstado.load();
  i.error = (ErrorMotor)aError.load();
  i.dir = aDir;
  i.fcAbierta = aFcA;
  i.fcCerrada = aFcC;
  i.referenciado = aReferenciado;
  i.pasosTotales = aPasosTotales;
  return i;
}

const char* motorTextoError(ErrorMotor e) {
  switch (e) {
    case E_SIN_FC_ABIERTA: return "No se detectó el final de carrera de abierta";
    case E_SIN_FC_CERRADA: return "No se detectó el final de carrera de cerrada";
    default: return "";
  }
}

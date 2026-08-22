# ESP32blue_team

Un **firmware de seguridad defensiva y forense** para el ESP32 — herramientas de monitoreo y reconocimiento en lugar de capacidades ofensivas. Diseñado para detectar actividad de red inesperada y dispositivos desde una única placa ESP32.

## Descripción General

ESP32blue_team proporciona capacidades de reconocimiento pasivo: sniffing pasivo de Wi-Fi (captura y análisis de tramas de gestión 802.11 sin transmitir), escaneo de dispositivos BLE y escaneo activo de Wi-Fi. Todos los resultados se pueden registrar en serie o tarjeta SD en formato CSV/JSON, con una interfaz de menú simple para la navegación.

**Principios de diseño fundamental:**
- **Sin acciones ofensivas** — sin deauth, inyección de paquetes o sondeo disruptivo más allá del escaneo activo 802.11 estándar.
- **Pasivo primero** — solo escucha, nunca inicia transmisiones (excepto durante el escaneo activo).
- **Análisis portátil** — la lógica de análisis de tramas (`include/frame_parse.h`) está libre de Arduino (solo C++ estándar) para que se pueda probar en la computadora host (`native` env).
- **Límites claros** — los módulos de escaneo devuelven registros; el logger escribe en sumideros; el menú maneja la interfaz. Los módulos no cruzan estos límites.

## Estado

**Desarrollo activo:**
- ✅ Escaneo activo de WiFi (sondeo activo 802.11)
- ✅ Sniffing pasivo de WiFi (RX promiscuo, arquitectura productor-consumidor-analizador)
- ✅ Escaneo de dispositivos BLE (basado en NimBLE)
- ✅ Tipos de registros y formato (ApRecord, BleRecord, SniffRecord; ayudantes CSV/JSON)
- ✅ Interfaz de menú serie (máquina de estados 3-4 pantallas)
- 🔧 Registro en tarjeta SD — planificado

## Arquitectura

```
include/config.h        Pines de placa, baud, valores por defecto de log, flags de características, ajustables de sniff
include/records.h       ApRecord / BleRecord / SniffRecord + formateadores CSV/JSON
include/frame_parse.h   SniffFrame POD + parseFrame(): bytes 802.11 sin procesar → SniffRecord
include/wifi_auth.h     Análisis de tipo de autenticación/encriptación WiFi

lib/wifi_scan/          Escaneo activo 802.11 (llamada bloqueante → vector<ApRecord>)
lib/wifi_sniff/         Captura de RX promiscua pasiva + salto de canal
                        Productor (callback WiFi RX) → rawQueue → Tarea consumidora → outQueue → sniffPoll()
lib/ble_scan/           Escaneo pasivo NimBLE → vector<BleRecord>
lib/logger/             Enrutar registros a sumideros (Serial/SD), formato mediante records.h
lib/ui/serial_menu/     Máquina de estados de menú 3-4 pantallas; único lugar haciendo E/S Serial

src/main.cpp            setup()/loop(): inicializar logger + menú, ejecutar el menú
```

### Sniffing de WiFi (Pasivo)

El sniffer coloca el ESP32 en **modo RX promiscuo** y escucha tramas de gestión 802.11 (beacons, solicitudes de sonda) sin transmitir. Como la carga de trabajo se ejecuta en el contexto de callback del controlador Wi-Fi, se divide en un patrón **productor-consumidor** sobre colas FreeRTOS:

```
Tarea del controlador WiFi        Núcleo de aplicación (núcleo 1)    Bucle de llamador
┌──────────────────┐  rawQueue  ┌────────────────────┐  outQueue  ┌──────────┐
│ RX promiscua     │ ─ SniffFrame → tarea consumidora│ SniffRec*  → sniffPoll
│ callback         │  (POD)      │ analizar → SniffRecord │ (ptr heap) │ (drenar)
└──────────────────┘            └────────────────────┘            └──────────┘
     ▲ esp_timer: kSniffDwellMs (~250ms) → esp_wifi_set_channel(siguiente)
     └───────────────── salto de canal 1..13 ──────────────────────
```

- **Productor (callback RX):** Filtra solo tramas de gestión, lee metadatos RSSI/canal, memcpy del marco sin procesar limitado en POD `SniffFrame`, empuja a `rawQueue` con tiempo de espera cero. Sin malloc, sin análisis — descarta tramas si la cola está llena (incrementa contador).
- **Consumidor (tarea FreeRTOS):** Fijado al núcleo de aplicación (núcleo 1, separado del núcleo Wi-Fi 0). Sondea `rawQueue`, analiza cada `SniffFrame` en un `SniffRecord` completo (extracción SSID, análisis MAC, etc.), entrega al llamador mediante `outQueue`.
- **Drenar (`sniffPoll()`):** El llamador sondea `outQueue` y decide qué hacer (registrar, mostrar, etc.). Sin E/S Serial en el sniffer mismo — ese es trabajo del logger.
- **Salto de canal:** Un temporizador periódico avanza el sniffer entre canales (rango configurable), para no quedarse atascado en el canal 1.

## Librerías y Dependencias

- **PlatformIO** — sistema de compilación y gestor de paquetes.
- **Framework Arduino** — núcleo ESP32, abstracciones Serial, GPIO.
- **ESP-IDF** — APIs ESP32 de bajo nivel (Wi-Fi, BLE, temporizadores, FreeRTOS).
- **NimBLE** — escaneo BLE (más ligero que la pila Bluetooth completa).
- **FreeRTOS** — planificador de tareas, colas, semáforos, mutexes (ya parte de ESP-IDF).
  - **Colas (`xQueueCreate`):** En el sniffer pasivo se utilizan dos colas principales:
    - `rawQueue`: transporta `SniffFrame` POD (estructura de datos sin asignación dinámica) desde el callback RX (contexto de interrupción del driver Wi-Fi) hacia la tarea consumidora en el núcleo de aplicación. Tamaño configurado en `config.h`.
    - `outQueue`: transporta punteros a `SniffRecord` (asignados en heap) desde la tarea consumidora hacia el llamador (`sniffPoll()`). Permite drenaje de resultados sin bloqueo en el loop principal.
  - **Sincronización:** Las colas emplean timeouts configurables (`pdMS_TO_TICKS()`); `rawQueue` usa timeout cero (descarta si está llena), `outQueue` permite espera para drenaje ordenado.
  - **Afinidad de núcleos:** Productor (callback) en Wi-Fi core 0, consumidor pinned a app core 1 — evita contención entre el driver Wi-Fi y la aplicación.
- **std::string, std::vector, std::atomic** — librería estándar C++ (disponible en ESP32, usada para portabilidad).

## Compilar y Probar

**Compilar:**
```bash
pio run                              # Compilar (env por defecto: esp32dev)
pio run -t upload                    # Compilar + flashear por USB
pio device monitor                   # Abrir consola serie (115200 baud)
```

**Pruebas en la computadora host (solo código portátil):**
```bash
pio test -e native                   # Ejecutar pruebas unitarias para código include/ (requiere g++)
```

## Alcance v1.0

Reconocimiento de la propia red del ESP32 para visibilidad defensiva (ej., detectar dispositivos/tráfico inesperado):
- **Escaneo activo de Wi-Fi** — escaneo 802.11 estándar, reporta SSID/BSSID/canal/RSSI/encriptación.
- **Sniffing pasivo de Wi-Fi** — captura y análisis continuo de tramas 802.11, salto de canal.
- **Escaneo BLE** — descubrir dispositivos BLE cercanos y anuncios.
- **Registro** — escribir registros en Serie y/o tarjeta SD, formato CSV o JSON.
- **Interfaz de menú** — navegación simple de 3-4 pantallas (menú principal, escaneo Wi-Fi, escaneo BLE, log/configuración).

**No incluido:** deauth, inyección de paquetes, sondeo activo más allá del escaneo 802.11 estándar, capacidades ofensivas.

## Futuro

Esta v1.0 se enfoca en reconocimiento y monitoreo de red. Las versiones futuras pueden expandirse con perfiles adicionales de herramientas defensivas/forenses, dependiendo de la dirección del proyecto.
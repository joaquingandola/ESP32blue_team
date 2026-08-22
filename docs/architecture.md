# Arquitectura (Intención)

ESP32blue_team es un firmware de reconocimiento defensivo/forense. v1 realizará
escaneo activo de Wi-Fi, sniffing pasivo de Wi-Fi, escaneo BLE, registro en
Serie/SD en formato CSV o JSON, y un menú serie — sin acciones ofensivas/disruptivas.

## Mapa de módulos

```
include/config.h      pines de placa, baud, valores por defecto de log, flags de características, ajustables de sniff
include/records.h     ApRecord / BleRecord / SniffRecord + ayudantes CSV/JSON portátiles
include/frame_parse.h SniffFrame POD + parseFrame(): bytes 802.11 sin procesar -> SniffRecord

lib/wifi_scan         escaneo activo 802.11  -> vector<ApRecord>
lib/wifi_sniff        captura promiscua pasiva (solo RX, nunca transmite)
lib/ble_scan          escaneo pasivo NimBLE -> vector<BleRecord>
lib/logger            enrutar registros a sumideros (Serie/SD), formato mediante records.h
lib/ui/serial_menu    máquina de estados de 3-4 pantallas; único lugar haciendo E/S Serie

src/main.cpp          setup()/loop(): inicializar logger + menú, ejecutar el menú
```

## Límites previstos

- **records.h es el contrato compartido.** Los módulos de escaneo y el logger
  dependen de los tipos de registro, no el uno del otro. Mantenerlo libre de
  Arduino (solo tipos std) para que el formato pueda ser testeado en la
  computadora host (`pio test -e native`).
- **El logger es el único escritor.** Los módulos de escaneo devuelven registros;
  nunca tocan Serie/SD. Agregar un sumidero o formato debe ser un cambio solo
  del logger.
- **serial_menu es el único propietario de E/S Serial.** Mantener su enum
  `Screen` y navegación independiente del backend para que una vista de pantalla
  en dispositivo pueda añadirse más tarde sin tocar la lógica de escaneo o log.

## Sniffing pasivo de Wi-Fi (lib/wifi_sniff) — diseño

A diferencia de los escaneos activos (una llamada bloqueante que devuelve un
`vector`), el sniffer es un *flujo continuo*. El ESP32 se coloca en modo
**promiscuo RX** y escuchamos tramas de gestión 802.11 sin nunca transmitir. La
carga vive en un callback que se ejecuta en el contexto del driver Wi-Fi, por lo
que el módulo se divide en un **productor** y un **consumidor** conectados por
una cola FreeRTOS.

```
  Tarea del driver Wi-Fi        nuestro núcleo de app (núcleo 1)    bucle de llamador
 ┌──────────────────┐   rawQueue  ┌───────────────────────┐  outQueue ┌──────────┐
 │ callback RX      │ ─ SniffFrame → tarea consumidora     │ SniffRec* → sniffPoll │
 │ promiscua        │  (POD, memcpy)│ analizar -> SniffRecord │ (ptr heap)│ (drenar) │
 │ (productor)      │             └───────────────────────┘           └──────────┘
 └──────────────────┘
        ▲  esp_timer (cada kSniffDwellMs ~250ms) -> esp_wifi_set_channel(siguiente)
        └──────────────── salto de canal 1..kSniffChannelMax ───────────────────
```

- **Productor — el callback RX promiscuo.** Se ejecuta en el contexto de la
  tarea Wi-Fi (no es una ISR de hardware). Debe permanecer pequeño y no bloqueante:
  filtra a tramas de gestión, lee RSSI + canal de `rx_ctrl`, copia una porción
  *limitada* de la trama sin procesar en un POD `SniffFrame`, e la empuja a
  `rawQueue` con timeout cero. Sin heap y sin análisis 802.11 aquí. Si la cola
  está llena **descarta** (e incrementa un contador) en lugar de paralizar la radio.
- **Consumidor — una tarea que nos pertenece.** Fijado al núcleo de app (núcleo 1)
  para que no compita con la tarea Wi-Fi (núcleo 0). Se bloquea en `rawQueue`,
  analiza cada `SniffFrame` en un `SniffRecord` (SSID/MACs), y entrega registros
  terminados al llamador mediante `outQueue`. Todo el trabajo con `std::string`
  sucede aquí, en nuestro contexto controlado.
- **Drenar — `sniffPoll()`.** El llamador (menú/main) sondea `outQueue` desde su
  propio bucle y decide qué hacer con cada registro. El módulo nunca hace E/S
  Serial en sí mismo, preservando el límite anterior. Los registros cruzan
  `outQueue` como punteros a heap (un `SniffRecord` mantiene `std::string`, así
  que no puede ser memcpy'd a través de una cola); `sniffPoll` toma posesión y
  los libera.
- **Salto de canal.** Un `esp_timer` periódico (`kSniffDwellMs`, ~250 ms) avanza
  el canal a través de `kSniffChannelMin..kSniffChannelMax`, para que no nos
  quedemos sordos acampados en el canal 1. El dwell/rango se configuran en
  `include/config.h`.

### Primitivos a implementar (estado)

- [x] `SniffRecord` en `records.h` (+ `macToString`, ayudantes CSV/JSON) — el
      contrato portátil para una trama capturada.
- [x] `SniffFrame` POD + `rawQueue` — traspaso sin asignación entre productor→consumidor.
- [x] Callback RX promiscuo con filtro solo de gestión (`WIFI_PROMIS_FILTER_MASK_MGMT`).
- [x] `parseFrame()` — analizador puro bytes-sin-procesar→`SniffRecord` para beacon
      (subtipo 0x8) y probe-request (0x4); camina parámetros etiquetados para el
      elemento SSID. Vive en `include/frame_parse.h`, libre de Arduino, así que
      la lógica más arriesgada (desplazamientos de encabezado, caminata de elementos,
      controles de longitud) es testeable en la computadora host.
- [x] Tarea consumidora fijada al núcleo de app + `outQueue` + drenaje `sniffPoll()`.
- [x] Salto de canal `esp_timer` sobre el rango configurado.
- [x] Contadores `SniffStats` (capturados / analizados / descartadosRaw / descartadosOut).
- [x] Prueba en computadora host para `parseFrame()` en blobs de bytes beacon/probe-req
      enlatados (`test/test_frame_parse/`): extracción de campos, SSIDs ocultos/wildcard,
      elemento SSID no-primero, subtipos ignorados, y longitudes truncadas/falsas.
      Escrito y compilando; aún no ejecutado (ver Build/Toolchain en CLAUDE.md).
- [x] Cableado de pantalla de menú: pantalla "Wi-Fi passive sniff" en
      `lib/ui/serial_menu.cpp` maneja `sniffStart()`/`sniffPoll()`/`sniffStop()`.
- [ ] Cableado de logger (diferido: `lib/logger` todavía es un esqueleto vacío).

## Restricción pasiva

`wifi_sniff` habilita solo RX promiscuo — nunca transmite. Sin deauth, inyección,
o sondeo más allá del escaneo activo 802.11 estándar.

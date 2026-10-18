# PusiMesh v2 — задание на прошивку PotSquad

**Кому:** этот агент (работает в репозитории PotSquad, C++/PlatformIO).
**Источник истины:** `docs/PUSIMESH-V2-PLAN.md` (спека + фазы). Читать его
целиком перед началом. Совместимость с v1 **не требуется**.

Цель: прошивка реализует PusiMesh v2 полностью (ядро, релей, ARQ, почта,
блобы) на транспортах BLE-ADV и ESP-NOW; даёт чистые кодеки, host-тесты и
симулятор. LoRa-носитель и MQTT — фазы Ф7/Ф8 (после ядра).

---

## Куда класть код (не плодить сущностей)

```
include/pusimesh_v2.h        presence v2 (бывш. pusimesh.h)
include/meshframe_v2.h       MeshFrame: kinds, seal/open, EXT-контейнер
include/meshenvelope_v2.h    MeshEnvelope + routeTag
include/meshrelay_v2.h       managed flood (чистый, host-testable)
include/meshreliable_v2.h    ARQ окно/SACK/REPAIR
include/meshmailbox_v2.h     store-and-forward + MSG_*
include/meshblob_v2.h        BLOB_*/VOICE_*
include/pusimesh_transport_v2.h  интерфейс транспорта (образец: compixi mesh_transport.h)
src/pusimesh_v2.cpp, src/meshframe_v2.cpp, ... (по одному .cpp на заголовок)
src/mesh_v2.cpp              app-состояние (radio-agnostic), как сейчас src/mesh.cpp
src/meshtalk_v2.cpp          runtime: ключ/счётчик/inbox/outbox, как сейчас
src/detection.cpp            NimBLE-половина (BLE-ADV транспорт)
src/pusimesh_espnow.cpp      ESP-NOW транспорт (envelope)
test/pusimesh_v2_test.cpp    golden-векторы + негативы
test/meshrelay_v2_test.cpp   сценарии флуда/дублей/overhear
test/meshreliable_v2_test.cpp ARQ-сценарии
```

Чистые кодеки **не включают** `<Arduino.h>` и не знают про радио. Транспорт
за интерфейсом; `mesh_v2.cpp`/`meshtalk_v2.cpp` радио-агностик.

---

## Фазы и шаги (по порядку)

### Ф0. Спека и векторы
- Создать `docs/PUSIMESH-V2-SPEC.md` (ужать §3–6 плана до wire-спеки) —
  единый источник для трёх платформ.
- Написать `tools/gen_pusimesh_v2_vectors.py`: генерирует
  `test/pusimesh_v2_vectors.h` независимой реализацией (Python
  `cryptography`): presence, каждый kind, envelope encode/decode, routeTag,
  SACK/REPAIR, mailbox, blob. Негативные кейсы обязательны.
- `test/pusimesh_v2_test.cpp` — сверить байт-в-байт; сборка `pio test -e native`
  (если есть) или `make -C test`.

### Ф1. Личность и presence v2
- `nodeId` 8Б: генерация из `esp_random()`, NVS `pusimesh/nodeid`.
- `pusimesh_v2.h`: `encodePresence/decodePresence` по §4.1; короткая форма
  для BLE-ADV (split на `PM2`+`PEX` AD, см. транспорт).
- Карта `linkAddr→nodeId` (RAF, TTL, freshest-wins) в `mesh_v2.cpp`.
- nonce от `nodeId` (§3). Проверка: телефтон с RPA открывает/шлёт.

### Ф2. Ядро кадров
- `meshframe_v2.h/.cpp`: MeshFrame v2, реестр kind (§4.4), `seal/open`.
- EXT-контейнер + базовые подвиды (CONFIRM, IDENTITY, CAPS, PRESENCE_EXT).
- Текстовые алфавиты: латиница + кириллица, 15 частей, флаг алфавита.
- Golden-тесты на каждый kind и на каждый отказ.

### Ф3. Envelope + релей (ESP-NOW)
- `meshenvelope_v2.h/.cpp`: layout §4.3, `encode/decode`, `routeAad`,
  `sealRouteTag/openRouteTag`.
- `meshrelay_v2.h/.cpp`: перенести логику compixi `meshrelay.h`
  (seen-cache, contention, overhear, last-mile, DM-ACK), заменив MAC→nodeId.
- `pusimesh_espnow.cpp`: пакет `[P,E] envelope` в broadcast; presence v2.
- Host-тесты сценарные (как `meshrelay_test.cpp`).

### Ф4. ARQ
- `meshreliable_v2.h/.cpp`: окно/SACK/REPAIR/RTO (перенос `meshreliable.h`).
- Связать с `meshtalk_v2` (outbox, состояния доставки).
- Host-тесты ARQ.

### Ф5. Почта / store-and-forward
- `meshmailbox_v2.h/.cpp`: bounded store (64 записи, TTL 30 мин, групповые
  кадры; DM не хранить), персист в NVS.
- EXT MSG_QUERY/SUMMARY/FETCH/CHUNK/ACK (§5.3). UI-триггер «спросить
  непрочитанное» в CHAT/SQUAD.
- Host-тесты: запрос→summary→fetch→chunk→ack, дедуп, TTL.

### Ф6. Блобы (файлы/голос)
- `meshblob_v2.h/.cpp`: BLOB_MANIFEST/CHUNK/ACK, resume по байт-офсету, SHA-256.
- **Голосовые сообщения 10–15 с** (§5.5 плана): `VOICE_MANIFEST`
  (`codec/rate/durMs/size/sha8/chunkCount`) + данные через BLOB-чанки.
  Кодеки: **Opus 16 кГц ~16 кбит/с** (по умолчанию) и **Codec2 3200** для
  LoRa/узкого канала; профиль по `caps` пира. Лимит клипа 15 с, ≤32 КБ.
  Кодирование/декодирование — libopus (или Codec2) в отдельной задаче;
  никакого Xiaozhi/ESP-ADF-агента, только кодеки.
- ESP32⇄ESP32 — ESP-NOW (broadcast-флуд + ARQ, ~222Б/чанк). На BLE-ADV
  голос невозможен — честный фолбэк «диктовка→текст→TTS».
- Отправитель держит blob до ACK/SHA или TTL; mailbox-докачка офлайн-пиру.
- Также: golden-тесты на **кириллицу** (короткий текст, 15 частей, смешанный
  набор, имя UTF-8 6/12 букв) — часть Ф2, но проверить здесь сквозняком.

### Ф7. LoRa-носитель и сервер (после ядра)
- Только **868 SRD** + token-bucket duty-cycle; никогда любительские полосы.
- Переносить envelope в payload Meshtastic/MeshCore (или RadioLib-режим) —
  решение зафиксировать в `docs/LORA.md` разделом PusiMesh. Голос по LoRa —
  **только Codec2** (клип 6 КБ @3200 и меньше); при нехватке бюджета — отказ.
- Связь с **нашим PusiServer** по собственному протоколу (WS/MQTT),
  **без Xiaozhi**: сервер как релей/хранилище blob и голоса, OTA, агент.

### Ф8. Закалка
- fuzz/негативы, суточный soak, матрица interop, обновить вики.

---

## Требования к качеству

- Кодеки host-testable; ни одного `malloc` в горячем радио-пути.
- Каждый отказ покрыт негативным тестом.
- Не плодить сущностей: расширять существующие, если смысл тот же.
- Коммит на фазу; сообщение осмысленное.
- Каждая фаза: сборка целевого env (`pio run -e cyd-ili9341`) + host-тесты +
  симулятор-рендеры затронутого UI.

## Приёмка (в конце)

- Presence v2: два узла видят друг друга без дублей при ротации адреса.
- Чат латиница+кириллица, эмоуты, READ/SACK/REPAIR, состояния доставки.
- ESP-NOW: 3 узла цепочкой — сообщение через хоп, ACK сквозной.
- Почта: узел ушёл, вернулся — «спросить непрочитанное» отдаёт кадры.
- Блоб: файл/голосовое доходит с resume и совпавшим SHA.
- Суточный soak без утечек и деградации.

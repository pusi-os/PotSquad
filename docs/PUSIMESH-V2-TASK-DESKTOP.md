# PusiMesh v2 — задание на десктоп PixelDesk / PusiDesk

**Кому:** агент №3 (desktop), репозиторий PixelDesk (локально
`/run/media/al/ARCHIVE/code/PixelDesk`; брендовый преемник — `PusiDesk`,
сейчас только README — синхронизировать по мере ребрендинга). **Источник
истины:** `PotSquad/docs/PUSIMESH-V2-SPEC.md` (wire, заморожен) и
`PotSquad/docs/PUSIMESH-V2-PLAN.md` (§3–6). Точка входа и готовый промпт —
`PotSquad/docs/PUSIMESH-V2-AGENT-LAUNCH.md`. Совместимость с v1 **не требуется**.

Цель: десктоп — равноправный узел PusiMesh v2 и одновременно **шлюз/мост**:
BlueZ BLE-ADV/GATT, ESP-NOW через донгл, позже MQTT/PusiServer.

---

## Что уже есть (использовать)

- `pixeldsk/ble_mesh.py` — Python-кодек presence + BlueZ backend.
- `pixeldsk/meshmsg.py` — Python-кодек кадров (уже знает SACK/REPAIR/TEXT_LONG/
  SKIN/EXT с под-видами `EXT_SQ_AIR_*`).
- `pixeldsk/mesh.py` — мост к ESP-NOW донглу (JSON-протокол `[BW {...}]`).
- `dongle/include/{meshflood,meshrelay,meshreliable}.h` — C++ envelope+relay+ARQ
  (общие с compixi) — это уже почти v2, обобщить на `nodeId`.
- `ui-core/fw/meshmsg.h`, `ui-core/fw/clear_visit.*` — нативный движок визита.

---

## Куда класть код (расширять)

```
pixeldsk/pusimesh_v2.py     presence v2 + MeshFrame v2 + EXT (замена ble_mesh/meshmsg)
pixeldsk/meshenvelope_v2.py MeshEnvelope + routeTag
pixeldsk/relay_v2.py        управляемый флуд (порт meshrelay.h → Python, либо reuse C++)
pixeldsk/reliable_v2.py     ARQ (окно/SACK/REPAIR/RTO)
pixeldsk/mailbox_v2.py      store-and-forward + MSG_*
pixeldsk/blob_v2.py         BLOB_*/VOICE_*
pixeldsk/node_identity.py   nodeId + карта linkAddr→nodeId
dongle/                     обновить meshflood/relay/reliable на nodeId+envelope v2
```

Кодек — чистый Python (тестируется `--selftest`, как сейчас `ble_mesh.py`).

---

## Ключевые требования

1. **Идентичность.** `nodeId` 8Б в конфиге (`node_id`), nonce от nodeId, карта
   `linkAddr→nodeId`. Отказаться от привязки к MAC в кодеках.
2. **Presence v2** (§4.1) и **кадры v2** (§4.4) — байт-в-байт по спеке.
3. **Envelope + релей** (§4.3/§5.1) — поднять C++ `meshrelay.h` до v2
   (nodeId, routeTag, squadTag) и/или портировать в Python для BlueZ-пути.
4. **ARQ** (§5.2) — `meshreliable.h` → v2; связать с UI-статусами.
5. **Почта** (§5.3) — узел-хранитель (десктоп удобен как «офлайн-почтальон»):
   bounded store на диске + MSG_QUERY/SUMMARY/FETCH/CHUNK/ACK.
6. **Блобы/голос** (§5.4, §5.5) — файлы и **голосовые сообщения 10–15 с**:
   донгл/ESP-NOW для малого, TCP/PusiServer для крупного; `VOICE_MANIFEST`
   + BLOB-чанки с resume и SHA. Десктоп удобен как приёмник/раздатчик
   (libopus на хосте), включая Codec2-профиль для LoRa-канала.
7. **Русский текст/имена** — кириллический 6-битный набор и UTF-8-имена
   (как в §4.6 плана); проверить сквозняком RU-сообщение и RU-имя.
8. **Сервер — наш.** PusiServer — собственный WS/MQTT-протокол,
   **Xiaozhi не используем нигде**. Топики `pusi/<squadTag>/<nodeId>/…`,
   сервер как непрозрачный релей/хранилище; retained presence, LWT, QoS1.
9. **UI/движок визита** не ломать; менять транспорт/провод под `clear_visit`.

---

## Фазы (синхронно с планом §7)

- **D0.** nodeId, presence v2, карта адресов, nonce от nodeId.
- **D1.** `pusimesh_v2.py` (все kinds, EXT, TEXT_LONG/RU).
- **D2.** BlueZ-транспорт режима C; старт GATT-пути (режим E).
- **D3.** Envelope + relay (C++ донгл обновить + Python для BlueZ).
- **D4.** ARQ.
- **D5.** Почта/local store на диске.
- **D6.** Блобы/голос (10–15 с, VOICE_MANIFEST + BLOB + resume/sha);
  приём и раздача файлов и голоса.
- **D7.** MQTT/WS-мост к нашему PusiServer (без Xiaozhi); сервер как релей и
  хранилище blob/голоса.

## Приёмка

- Десктоп ⇄ прошивка ⇄ Android: presence без дублей, чат RU+EN, эмоуты, DM,
  файл/голос по хопу, почта.
- Десктоп как хранитель: отдаёт «непрочитанное» вернувшемуся узлу.
- MQTT-мост: сообщение из меша доходит до PusiServer и обратно.
- Суточный прогон стабилен.

## Заметки

- Донгл-код (`meshflood/relay/reliable`) — почти v2; главное изменение —
  MAC→nodeId и добавление routeTag/squadTag из §3/§4.3.
- Десктоп — естественное место для **mailbox-хранителя** (диск, always-on) и
  будущего **MQTT-моста**; учесть при проектировании store.
- Wire-спека заморожена: `PotSquad/docs/PUSIMESH-V2-SPEC.md` — байт-в-байт;
  golden-векторы — `PotSquad/test/pusimesh_v2_vectors.h`.
- Прошивка уже на v2 по идентичности (presence v2 + nonce от nodeId); слой
  кадров `'PU'` дочищается (Ф2) — см. `PUSIMESH-V2-PROGRESS.md` §6.

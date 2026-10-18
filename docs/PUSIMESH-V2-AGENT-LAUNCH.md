# PusiMesh v2 — запуск агентов приложений (Android + Desktop)

Статус: готово к запуску, 2026-10-18. Этот файл — **точка входа** для агентов,
которые переводят приложения на PusiMesh v2. Он не заменяет спеку: он говорит,
что читать, что уже работает на проводе, что кодировать и как проверить.

---

## 0. TL;DR

- **Один провод на три платформы.** Контракт заморожен: `docs/PUSIMESH-V2-SPEC.md`.
  Кодировать **байт-в-байт** по нему; golden-векторы — `test/pusimesh_v2_vectors.h`.
- **Прошивка PotSquad уже на v2 целиком для ядра**: presence v2, nonce от
  `nodeId`, и **слой кадров `'PU'`** (CANNED/TEXT латиница+кириллица/EMOTE/HELLO/
  READ/EXT). Проверено на стенде (кубик ⇄ CYD): чат (RU+EN), эмоуты и READ
  ходят сквозь. Приложения могут интегрироваться против живой прошивки.
- **Что ещё нет на прошивке** (не мешает старту приложений, нужно для полной
  приёмки): envelope/релей, ARQ, почта, блобы/голос (Ф3–Ф6). Проверка presence
  и чата — уже сейчас; расширенные сценарии — по мере фаз.

---

## 1. Источники истины (читать в этом порядке)

| файл | что это |
|---|---|
| `docs/PUSIMESH-V2-SPEC.md` | **wire-контракт**, byte-level. Единственный источник для трёх платформ |
| `docs/PUSIMESH-V2-PLAN.md` | фазы, обоснования, надёжность/маршрутизация (§3–6) |
| `docs/PUSIMESH-V2-TASK-ANDROID.md` | задание агента №2 (Android, Kotlin) |
| `docs/PUSIMESH-V2-TASK-DESKTOP.md` | задание агента №3 (Desktop, Python/BlueZ) |
| `test/pusimesh_v2_vectors.h` | golden-векторы (не редактировать) |
| `tools/gen_pusimesh_v2_vectors.py` | независимый генератор векторов (Python `cryptography`) |
| `include/pusimesh_v2.h`, `include/meshframe_v2.h` | эталонные кодеки (C++, компилируются на хосте) |
| `docs/PUSIMESH-V2-PROGRESS.md` | что прошивка уже сделала (статус Ф0/Ф1) |

Проверка векторов на машине разработчика прошивки:
```sh
make -C test out/pusimesh_v2_test && ./test/out/pusimesh_v2_test   # 53 проверки
python3 tools/gen_pusimesh_v2_vectors.py                           # пересобрать векторы
```

---

## 2. Что УЖЕ на проводе (прошивка, 2026-10-18)

Реализовано и проверено на стенде (кубик ⇄ CYD):

- **`nodeId`** — 8 случайных байт, `NVS pusimesh/nodeid`, делается один раз.
- **Presence v2** (§3 спеки): короткая форма 23Б в одном manufacturer-AD
  (`2 company + 23 + 2 AD-заголовок = 27 ≤ 31`); кастомное имя — отдельным
  **PEX**-AD (`'P','X','E'`), чередующимся с presence (окно ~8 с), потому что
  вместе они 31Б не влезают.
- **Гостевая таблица по `nodeId`** (не по адресу) — RPA-ротация не плодит гостей.
- **Nonce кадров от `nodeId`** (`nodeId[0:6] ‖ counter u24 LE ‖ 0×4`) на отправке
  (свой id) и на приёме (id отправителя, разрешённый из presence).

**Ядро кадров v2 реализовано (Ф2 core, 2026-10-18):**

| слой | состояние |
|---|---|
| magic кадра | `'P','U'` ✅ |
| реестр kind | CANNED / TEXT / TEXT_CYR / EMOTE / HELLO / READ / EXT ✅ (TEXT_LONG/SACK/REPAIR/CONFIRM/IDENTITY — контейнер есть, обработка по мере фаз) |
| соль KDF | `"pusimesh/v2/key"`, PBKDF2-HMAC-SHA256 20000 ✅ (старый ключ пересчитывается при загрузке) |
| текст | UTF-8, до 15 частей, латиница **и** кириллица, `END=63` ✅ (клавиатура композера пока ≤48 символов — поднять до 240 — следующий UI-шаг) |
| HELLO | `[maj][min][pat][epoch u32 LE][zone+1]` ✅ |
| EXT | контейнер + под-виды; неизвестный под-вид молча дропается ✅ |

Проверено на стенде (кубик ⇄ CYD): чат (RU+EN), эмоуты, READ — сквозь. Legacy
`'ST'` остаётся только для служебных кадров прошивки (сопряжение/WiFi/обновление).

---

## 3. Контракт — точные величины (детали в спеке)

- company id `0xFFFF`; presence magic `'P','M','2'`; frame magic `'P','U'`;
  version `2`.
- **Ключи:** `squadKey = PBKDF2-HMAC-SHA256(phrase, "pusimesh/v2/key", 20000) → 16Б`;
  `routeKey = HMAC-SHA256(squadKey,"pusimesh/v2/route")[0:16]`;
  `squadTag = SHA256(squadKey ‖ "pusimesh/v2/squad")[0:3]`.
- **Кадр:** AES-128-CCM, tag 8; AAD = заголовок (6Б);
  `nonce = nodeId[0:6] ‖ counter u24 LE ‖ 0×4`; `counter` 24-бит, персист вперёд
  блоками по 64, отказ при исчерпании.
- **Presence v2** — §3 спеки (флаги NAME/PHONE/GROUP, appearance word переиспользован
  из v1, одежда/глаза инлайн, имя — UTF-8 по границе символа).
- **Имена:** presence ≤12Б; длинное имя — кадр `IDENTITY` (kind `0xA`), ростер по
  `nodeId`, никаких `PEER-xxxx`.
- **Текст:** 6-бит, 16/часть, 12Б/часть; латиница (TEXT/TEXT_LONG) и кириллица
  (TEXT_CYR/TEXT_LONG_CYR); `TEXT_MAX=240` (15 частей); `END=63`.

---

## 4. Транспорт BLE-ADV (как это в эфире)

- Один manufacturer-AD, company `0xFFFF`.
- **Неидент. presence (27Б)** и **PEX-имя (≤29Б)** чередуются во времени — оба
  отдельными слотами основного AD.
- **Кадры** идут тем же основным AD (`company + frame ≤ 29Б`), вытесняя presence
  пока идёт передача; сканирование — активное (иначе PEX/кадры не долетают).
- Для DM/файлов/голоса — **BLE-GATT** (режим E, полный envelope), не ADV.
  Android не умеет ESP-NOW: app↔ESP32 — только GATT (или PusiServer).

---

## 5. Задания по платформам

Оба — по фазам, синхронным с планом §7.

- **Android (агент №2):** `docs/PUSIMESH-V2-TASK-ANDROID.md`, фазы A0–A7.
  Кодек — чистые Kotlin-объекты, тест на JVM; эталон надёжности (SACK/REPAIR/
  mailbox) переносится из `mesh/`.
- **Desktop (агент №3):** `docs/PUSIMESH-V2-TASK-DESKTOP.md`, фазы D0–D7.
  Python-кодек + BlueZ; донгл `meshflood/relay/reliable` → nodeId+envelope v2;
  десктоп — естественный mailbox-хранитель и MQTT-мост к нашему PusiServer.

---

## 6. Как проверять

1. **Кодек на хосте:** round-trip к каждому golden-вектору
   (`test/pusimesh_v2_vectors.h`) + негативы (bad magic/version/length/flags,
   END посреди части, обрезка имени по границе UTF-8, неверный nonce/sender).
2. **Presence — против прошивки уже сейчас:** включить RPA на телефоне, убедиться,
   что кубик/CYD видят его как **одного** гостя (не «2–3»), с правильной одеждой;
   сменить BLE-адрес — гость тот же.
3. **Кадры — после Ф2 прошивки:** чат (RU+EN), эмоут, READ; затем SACK/REPAIR,
   DM, файл и голос по хопу, почта.
4. **Логи:** прошивка печатает `[mesh-id] nodeId …`, `[squad] <id> arrived`,
   `[meshtalk] message from …`, `[meshtalk] emote …` — по ним видно, доходит ли
   кадр (и открылся ли он под правильным `nodeId`).

---

## 7. Приёмка интеграции (после Ф2)

- Три платформы в одном скваде: **presence без дублей**, чат латиница+кириллица,
  имена кириллицей, эмоуты, DM, READ/SACK/REPAIR, файл и **голосовое 10–15 с
  через хоп**, почта «спроси непрочитанное».
- Смена MAC/RPA **не** создаёт новый профиль и **не** глушит чат.
- Суточный прогон на 3+ узлах без утечек и деградации.

---

## 8. Готовый промпт для запуска агента

Скопировать в нового агента (два варианта — по платформе).

**Android:**
```
Ты — агент №2. Репозиторий: /run/media/al/ARCHIVE/code/PixelTag-App (Kotlin).
Задача: перевести приложение на PusiMesh v2 как равноправного узла меша.
Источник истины (кодировать байт-в-байт): 
  /run/media/al/ARCHIVE/code/pusi/firmwares/PotSquad/docs/PUSIMESH-V2-SPEC.md
  и /run/media/al/ARCHIVE/code/pusi/firmwares/PotSquad/docs/PUSIMESH-V2-PLAN.md
Задание и фазы: .../PotSquad/docs/PUSIMESH-V2-TASK-ANDROID.md (A0..A7).
Golden-векторы: .../PotSquad/test/pusimesh_v2_vectors.h (генератор — tools/gen_pusimesh_v2_vectors.py).
Важно: прошивка v2 умеет presence v2 + nonce от nodeId уже сейчас (проверь:
телефон с RPA виден как ОДИН гость); кадры 'PU'/kinds/кириллица — общий контракт,
совместимость откроется с прошивочным Ф2.
Сделай: A0 (NodeIdentity, presence v2, карта linkAddr->nodeId, nonce от nodeId),
затем A1 (MeshFrameV2: все kinds §4.1, EXT-контейнер, TEXT_LONG/RU, END=63),
A2 (BLE-ADV режим C + BLE-GATT режим E). JVM-тесты на round-trip к векторам и
негативы. Не ломать юзер-флоу (персонаж, пузыри, карта, компаньон). Отчёт — коммит
на фазу с осмысленным сообщением.
```

**Desktop:**
```
Ты — агент №3. Репозиторий: /run/media/al/ARCHIVE/code/PixelDesk (Python/BlueZ;
брендовый преемник PusiDesk). Задача: десктоп — равноправный узел PusiMesh v2 и
шлюз (BlueZ BLE-ADV/GATT, ESP-NOW через донгл, позже MQTT/PusiServer).
Источник истины: .../PotSquad/docs/PUSIMESH-V2-SPEC.md и PUSIMESH-V2-PLAN.md.
Задание: .../PotSquad/docs/PUSIMESH-V2-TASK-DESKTOP.md (D0..D7).
Golden-векторы: .../PotSquad/test/pusimesh_v2_vectors.h.
Сделай D0 (nodeId, presence v2, карта адресов, nonce от nodeId), D1
(pusimesh_v2.py: все kinds, EXT, TEXT_LONG/RU), D2 (BlueZ режим C, старт GATT).
Чистый Python, --selftest против векторов + негативы. Донгл
(meshflood/relay/reliable) обновить MAC->nodeId, envelope v2. Не ломать движок
визита. Коммит на фазу.
```

---

## 9. Координация

- **Ядро кадров v2 уже на прошивке** (Ф2 core), так что присутствие, чат (RU+EN),
  эмоуты и READ между телефоном/десктопом и устройством проверяемы сразу.
  Остальное (envelope/релей/ARQ/почта/блобы/голос — Ф3–Ф6) — по фазам, параллельно
  с приложениями.
- Прогресс прошивки — `docs/PUSIMESH-V2-PROGRESS.md`; фазы — `PLAN.md` §7.
- Порядок фаз обязателен; каждая — зелёные host-тесты + сборка (`pio run -e
  cyd-ili9341` и `-e xiaozhi-cube-154`) + bench, где есть железо.

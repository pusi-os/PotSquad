# PusiMesh v2 — задание на Android-приложение PixelTag-App

**Кому:** агент №2 (Android, Kotlin), репозиторий `pusi-os/pusi-android-app`
(локально `/run/media/al/ARCHIVE/code/PixelTag-App`, актуализирован до
`650d6b2`). **Источник истины:** `PotSquad/docs/PUSIMESH-V2-SPEC.md` (wire,
заморожен) и `PotSquad/docs/PUSIMESH-V2-PLAN.md` (§3–6). Точка входа и готовый
промпт — `PotSquad/docs/PUSIMESH-V2-AGENT-LAUNCH.md`. Совместимость с v1 **не
требуется**.

Цель: приложение реализует PusiMesh v2 как равноправный узел: presence v2,
кадры, envelope, релей, ARQ, почта, блобы (файлы/голос) — поверх BLE-ADV и
BLE-GATT; позже TCP/PusiServer.

---

## Куда класть код (расширять существующее, не плодить)

```
mesh/PusiMeshV2.kt      presence v2 (заменяет/дополняет mesh/SquachMesh.kt)
mesh/MeshFrameV2.kt     кадры+kind+EXT (на базе mesh/MeshMsg.kt)
mesh/MeshEnvelopeV2.kt  envelope+routeTag
mesh/MeshRelayV2.kt     managed flood (порт логики compixi meshrelay.h)
mesh/MeshReliableV2.kt  ARQ (окно/SACK/REPAIR/RTO)
mesh/MeshMailboxV2.kt   store-and-forward + MSG_*
mesh/MeshBlobV2.kt      BLOB_*/VOICE_*
mesh/BleMeshV2.kt       BLE-ADV + GATT транспорт (на базе mesh/BleMesh.kt)
mesh/NodeIdentity.kt    nodeId + карта linkAddr→nodeId
MeshController.kt       связать всё; UI-состояния доставки
```

Чистые кодеки — object'ы без Android-зависимостей (кроме крипто), тест на JVM.

---

## Ключевые требования

1. **Идентичность без MAC.** `nodeId` 8Б генерируется один раз, хранится в
   `SharedPreferences("mesh")/nodeId`. nonce = `nodeId[0:6]+counter[3]+0×4`.
   Карта `linkAddr→nodeId` из presence; RPA-ротация не рождает «призраков»
   и не ломает nonce. Убрать зависимость от `02:00:00:00:00:00`/`selfMac`.
2. **Presence v2** (§4.1) в manufacturer-data company `0xFFFF`; короткая форма
   ≤31Б: split `PM2` (nodeId+squadTag+appearance) + `PEX` (имя/caps) при
   необходимости. Каждые 1500мс.
3. **Кадры v2** (§4.2) — тот же бюджет 27Б; TEXT_LONG до 15 частей; латиница
   и кириллица (6-бит, флаг алфавита). Заменить `MeshMsg` реестр на §4.4.
4. **Envelope + релей** (§4.3, §5.1): на BLE-ADV — режим C; для DM/файлов/
   голоса — BLE-GATT, несущий полный envelope (MTU 517). Managed flood:
   seen-cache, contention по RSSI, overhear, last-mile, DM-ACK.
5. **ARQ** (§5.2): окно/SACK/REPAIR/RTO; состояния `SENT→MESH→READ→FAILED`,
   ручной «переслать» тем же msgId.
6. **Почта** (§5.3): `EXT MSG_QUERY/SUMMARY/FETCH/CHUNK/ACK`; кнопка «спросить
   непрочитанное у соседа»; хранилище групповых кадров (DM — только свой outbox).
7. **Блобы/голос** (§5.4, §5.5): перевести `bulk/BulkLink.kt` на байт-офсетный
   resume и общий BLOB-формат; **голосовое сообщение 10–15 с** — `VOICE_MANIFEST`
   + BLOB-чанки, Opus 16 кГц ~16 кбит/с по умолчанию (Android умеет Opus
   нативно), Codec2 — если пир просит узкий канал. На BLE-ADV голос
   невозможен — фолбэк «диктовка→текст→TTS» (уже есть).
   **Android не умеет ESP-NOW** → app↔ESP32 только BLE-GATT (режим E) или
   через PusiServer; app↔app — GATT/TCP.
8. **Русский текст и имена.** Кириллический 6-битный набор (33 буквы + пробел
   + цифры + `.,?!'-` = 51 ≤ 64) для сообщений; имена — UTF-8 с обрезкой по
   границе; длинное имя — через `IDENTITY` в ростер. Никаких `?`/обрезаний.
   Проверить сквозняком: RU-сообщение (короткое и 15 частей), RU-имя,
   смешанный набор.
9. **Бэкенд — наш.** Голосовой агент/ASR/TTS и синхронизация — через наш
   **PusiServer** (собственный WS/MQTT), **Xiaozhi не используем нигде**.
   Меш-протокол от сервера не зависит (работает офлайн).
10. **Юзер-флоу чата/гостей** не ломать: персонаж, пузыри, тумблеры, карта —
   всё остаётся; меняется транспорт/провод под ними.

---

## Фазы (синхронно с планом §7)

- **A0.** `NodeIdentity`, presence v2, карта linkAddr→nodeId, nonce от nodeId.
  Проверка на телефоне с включённым RPA.
- **A1.** `MeshFrameV2` (все kinds §4.4, EXT-контейнер, TEXT_LONG/RU).
- **A2.** BLE-ADV транспорт режима C + BLE-GATT транспорт режима E.
- **A3.** `MeshEnvelopeV2` + `MeshRelayV2` (managed flood).
- **A4.** `MeshReliableV2` + UI-состояния доставки.
- **A5.** `MeshMailboxV2` + UI «спросить непрочитанное».
- **A6.** `MeshBlobV2` (файлы/голос), resume/sha.
- **A7.** (позже) TCP/MQTT к PusiServer на том же envelope.

## Приёмка

- Телефон ⇄ прошивка ⇄ десктоп: presence без дублей, чат (RU+EN), эмоуты,
  READ/SACK/REPAIR, DM, файл и голос по хопу, почта.
- Смена BLE-адреса (RPA) не создаёт новый профиль и не глушит чат.
- Суточный прогон без утечек; нет регрессий в анимациях/карте/compаньоне.

## Заметки

- Приложение — **эталон** надёжности (SACK/REPAIR/store-and-forward) и донор
  логики; переносим её в v2, а не переписываем с нуля.
- Wire-спека заморожена: `PotSquad/docs/PUSIMESH-V2-SPEC.md` — следовать ей
  байт-в-байт; golden-векторы — `PotSquad/test/pusimesh_v2_vectors.h`
  (генератор — `tools/gen_pusimesh_v2_vectors.py`).
- Прошивка уже на v2 по идентичности (presence v2 + nonce от nodeId):
  телефон с RPA должен быть виден как ОДИН гость уже сейчас. Слой кадров `'PU'`
  на прошивке ещё дочищается (Ф2) — см. `PUSIMESH-V2-PROGRESS.md` §6.
- Тесты: JVM round-trip кодека к golden-векторам + негативы; unit на
  relay/ARQ/mailbox; на устройстве — `MeshController` лог в logcat.

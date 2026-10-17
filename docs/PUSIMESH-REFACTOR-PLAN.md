# PusiMesh — план работ (после доработки образа)

Статус: черновик-план, составлен при работе над образом. Вернуться к нему
после коммита образа и прошивки платы.

## Контекст (что уже выяснено)

- Экосистема (BroWatch, BroDozor, Compixii, PixelDesk, PusiServer, наше
  PotsSquad) делит один wire-формат: company `0xFFFF`, presence-адверт
  8/20 байт (magic `SQM1`), sealed-кадр MeshMsg 27 байт (AES-128-CCM,
  nonce = MAC+counter). Совместимость сохраняем.
- В Compixii есть готовая абстракция транспорта:
  `main/display/compixi/mesh_transport.h` + `mesh_ble.cc` + `mesh_espnow.cc`
  + `meshbro.cc` (app-слой, радио-агностик). Берём её за образец.
- У нас: `src/mesh.cpp` (app-состояние, радио-агностик),
  `Mesh::radioTick()` в `src/detection.cpp` (NimBLE) и `sim/meshsim.cpp`
  (эмулятор). Адверт-кодек — `include/pusimesh.h` (namespace `PusiMesh`).
- Русские СООБЩЕНИЯ по mesh сейчас невозможны: `TEXT_CHARSET` —
  Latin-only (одинаково у нас и у Compixii). Русский у Compixii — только
  в его LVGL-UI (`assets/locales/ru-RU`), не в mesh-чате.

## Именование

- Протокол/каналы/ключи/UI — **pusimesh**. `bromesh` не использовать.
- `SquachMesh` — только wire-совместимость (magic `SQM1`, `0xFFFF`).
- Выполнено: `PotsMesh` → `PusiMesh`, файлы `pusimesh.*`, UI «PUSIMESH»
  (коммит `532376a`).

## Фазы

- **Ф1. Транспорт-абстракция.** `include/pusimesh_transport.h`: интерфейс
  `Start/Stop/Running/Ready/AdvertPayload/SendPresence/SendFrame/Poll/Tick/
  OwnMac/Channel`, структура `RxFrame{mac,rssi,kind,data}` c `kind`
  `PRESENCE|SEALED`. Текущий BLE-путь заворачиваем за него без смены
  поведения (эмулятор тоже через интерфейс).
- **Ф2. ESP-NOW транспорт.** Packet `[ 'B' 'N' ver kind payload ]`,
  kind0 = company `0xFFFF` + адверт, kind1 = sealed-кадр. RX-callback
  (WiFi task) только копирует в ring; разбор — в `Poll()`.
- **Ф3. Настройка маршрута.** Ряд в меню PusiMesh: BLE ⇄ ESP-NOW (NVS,
  RU). Один радио-владелец за раз. Учесть сосуществование с BLE-сканом
  детекции и WiFi (wardrive) — переключать аккуратно.
- **Ф4. Единое хранилище rx/tx.** Один формат (presence + sealed) для
  обоих каналов; принятое по одному можно корректно переслать по другому
  после переключения. Реестры roster/phone/replay — общие, канало-агностик.
- **Ф5. Расширить адверт.** Добавить shirt/shorts/shoes (3× 8-вариантов)
  ОПЦИОНАЛЬНОЙ extension-AD (отдельная manufacturer-структура), НЕ меняя
  длину legacy 8/20 — старые пиры не должны отвергать нас. Камео обязано
  совпадать с устройством владельца (цвет одежды, аутфит, аура, имя).
- **Ф6. Полнота передачи.** Ростер, инвайты (X25519), состояние и
  триггеры анимаций/эмоций, сообщения, образ — сверить поля, ничего не
  потерять. Инвайты/роутинг — как в оригинальном Squach.
- **Ф7. Русские сообщения.** Расширить `TEXT_CHARSET`/перейти на часть
  UTF-8 в kind2 (wire-изменение) — согласовать с BroWatch/BroDozor/
  Compixii/PixelDesk, обновить `meshmsg_vectors.h` и Python-векторы.
- **Ф8. Документация.** Полная спека pusimesh: `docs/PUSIMESH-SPEC.md`
  в проекте и страница в локальной вики `/home/al/wiki` — для Linux-апп,
  прошивок других устройств и Android-приложения на этом же протоколе.
- **Ф9. Приёмка.** BLE↔ESP-NOW переключение, relay между каналами,
  камео с цветами, host-тесты, сборка `cyd-ili9341`, прошивка платы.

## Сквозные требования

- Wire-совместимость с BroWatch/BroDozor/Compixii/PixelDesk; изменения
  формата — только как опциональные расширения + векторы.
- Не ломать стабильность: следить за free/largest SRAM, NimBLE и ESP-NOW
  — главные потребители.
- 3 кнопки на CYD: у каждой новой функции — доступный жест.
- Тесты: чистый кодек/ростер/инвайты — host-тестами (`test/`).

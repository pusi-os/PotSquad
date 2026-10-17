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

## Ф5 (уточнение). Расширение адверта: текущая одежда и её цвета

Присутствие (`buildSelf`) уже несёт аутфит (5 бит), оттенок, ауру и имя.
Не передаются ЦВЕТА базовой одежды (shirt/shorts/shoes) и компактное
состояние. Длины legacy 8/20/12/24 менять НЕЛЬЗЯ: старый приёмник сверяет
точную длину и отверг бы нас целиком. Поэтому расширение едет ВТОРОЙ
manufacturer-AD (та же company `0xFFFF`), которую новый приёмник читает, а
старый просто пропускает: он берёт существование из первой (legacy) AD.
Приём УЖЕ перебирает все manufacturer-блоки (`detection.cpp`
`getManufacturerDataCount()` → `Mesh::onManufacturerData`), так что RX-путь
готов; legacy-AD шлётся ПЕРВОЙ (пиры, читающие только первый блок, видят нас
как прежде), расширение — второй.

Формат второй AD (после 2 байт company):

```
0..3  magic 'S','Q','X','1'
4     версия расширения (=1)
5     shirt(3) | shorts(3)<<3
6     shoes(3) | state(5)<<3
```

7 байт payload → AD 11 байт. Бюджет legacy 31: безымянный presence (AD 12) +
11 = 23 — влезает; named presence (AD 24) + 11 = 35 — НЕ влезает, поэтому
расширение шлётся best-effort: если `addData` не встал, legacy-адверт остаётся,
цвета такого пира просто не передаются (не ошибка, деградация). RX держит
`s_ext[8]` по MAC (RAM, как phone/squad-таблицы), `Potsy` при рисовке гостя
берёт цвета оттуда (`setClothPreview`), иначе — свои. State передаётся и
хранится, чтобы гость мог зеркалить настроение хоста.

## Ф10. Полнота локализации и контента (сверка с Compixii и BroWatch)

Цель: во ВСЕХ разделах весь пользовательский текст — русский, влезает,
рисуется без `?`, пропусков и иероглифов. Сверить подписи/названия с
Compixii и BroWatch (единый словарь терминов), не меняя индексы протокола.

Найденные непереведённые поверхности (проверить и закрыть):

- **POTSY-DEX**: `dex.cpp` (shortName/kind/rarityName/lore/habitat/quip/hint/
  radio — 17 записей целиком по-английски) и `ui_dex.cpp` («N OF M CAUGHT»,
  «NOT CAUGHT», «NEMESIS», «CAUGHT/FIRST/LAST/CLOSEST/AT NIGHT», «HABITAT»/
  «HINT», «POTSY SAYS»), плюс RU-рендер (printRU/wrapTextRU) для кириллицы.
- **Гость/эмоции на CLEAR**: `ui_clear.cpp` — массивы `EMOTE_*_CALL/BACK/
  SCARED` английские; реплики гостя из `potsy` сверить.
- **Экраны**: `ui_outfit.cpp`, `ui_security.cpp`+`security.cpp`,
  `ui_diagnostics.cpp`, `ui_sysprops.cpp`, `ui_wifipass.cpp`,
  `ui_lorahome.cpp`, `regulars.cpp`, `bingo.cpp`/`ui_bingo.cpp`,
  `blackbox.cpp` — найти и перевести остатки.
- **Счётчики главного экрана** и **режим часов (WATCH/DESK)**: подписи
  счётчиков обнаруженных устройств, чипы типов, заголовки — RU и влезание.
- **Шрифты**: покрытие RuCyr8 (тело) и RuCyr16 (заголовки) для ВСЕХ строк
  (включая добавленные), отсутствие пропущенных глифов; проверить на
  компактных панелях (240x135) влезание длинных RU-строк.
- **Птичья болтовня**: банки Potsy (включая новые строки), emote-скрипты
  (`EmoteScript::line`), `meshwords.cpp` (cannedLine RU), `meshtutor.cpp`.

## Пошаговый план имплементации (последовательно)

1. [x] План дополнен (этот файл).
2. [x] PusiMesh: кодек расширения `encodeLook/decodeLook` (magic `SQX1`),
      host-тест + негативные кейсы (test/pusimesh_test.cpp). Коммит `7cbf4ab`.
3. [x] RX: `mesh.cpp` — таблица `s_look[]`, разбор второй AD, `lookFor()`
      (коммит `7cbf4ab`).
4. [x] TX: `detection.cpp` — `buildSelfLook()` и `addData` второй AD
      (best-effort, только для presence). Коммит `7cbf4ab`.
5. [x] Рендер: `potsy.cpp` `setClothPreview` + использование в базовой
      одежде и рукавах; `ui_clear.cpp` — drawVisit и drawOne. Проверено в
      симуляторе (ext через реальный RX-путь, 1314 px). Коммит `7cbf4ab`.
6. [x] POTSY-DEX RU: `dex.cpp` (контент RU + EN-таблицы рядом),
      `ui_dex.cpp` (подписи + RU-рендер через put/wid/wrap). Коммит `3da32f3`.
7. [~] Остальные экраны/болтовня RU (Ф10). Сделано: банки реплик
      set-piece эмоций в `ui_clear.cpp` (EMOTE_*_RU). ОСТАЁТСЯ: `ui_security`
      (+`security.cpp`), `ui_diagnostics`, `ui_sysprops`, `ui_wifipass`,
      `ui_lorahome`, подписи счётчиков главного экрана и режима часов,
      `regulars` (имена — намеренно EN). Сверить термины с Compixii/BroWatch.
8. [x] Сборка `cyd-ili9341` SUCCESS, host-тесты 39 pass + те же 5 предсущих,
      симулятор-рендеры (dex RU, ext-цвета). 
9. [~] Коммиты `7cbf4ab`, `3da32f3` (+ этот). Прошивка и serial-проверка —
      в конце сессии.

## Открытые вопросы/риски

- Имя пользователя (named presence) урезает бюджет: цвета под custom-именем
  могут не дойти. Решение: приоритет присутствия над цветом (документировано),
  при необходимости позже — перенос части байтов в flags/спар.
- Русские СООБЩЕНИЯ в mesh (не UI) требуют wire-изменения `TEXT_CHARSET`
  (Ф7) — согласовать со всей экосистемой; сейчас чат остаётся Latin-only.
- Изменение размера `Peer`/ростера затронет NVS-формат роuтера — не делаем;
  цвета живут в отдельной RAM-таблице.
```

# Аппаратура, прошивки и репозитории — сводка (PUSI ecosystem)

> Собрано 2026-10-18 из: `compixi/plan/DEVICES.md`, `compixi/plan/GOTCHAS.md`,
> xiaozhi-esp32 `main/boards/*`, `Pixel/docs/ru/09-faza1-cophone-airvision.md`,
> PixelTag-App `docs/BROMESH-SPEC.md`/`BULK-FILES.md`, wiki `~/wiki/PUSI.md`.
> Правило: строка либо «проверено» (файл/лог/команда), либо `НЕ ПРОВЕРЕНО`.

## 1. Устройства (ESP32-S3, 240×240, 3 кнопки + питание, без тача)

| устройство | board (`xiaozhi-esp32/main/boards/…`) | MAC | экран (ST7789 SPI) | аудио (I2S, NoAudioCodecSimplex) | кнопки |
|---|---|---|---|---|---|
| **наш «кубик»** | `compixi/compixi-cube-154` = `nologo/xingzhi-cube-1.54tft-wifi` | `30:ed:a0:c6:ad:9c` | SDA=10 SCL=9 DC=8 CS=14 RES=18 BL=13 | MIC WS=4 SCK=5 DIN=6 / SPK DOUT=7 BCLK=15 LRCK=16 | BOOT=0 VOL+=40 VOL−=39 |
| **zhengchen** (стенд-цель) | `zhengchen/1.54tft-wifi` | `14:c1:9f:3b:38:1c` | SDA=41 SCL=42 RES=45 DC=40 CS=21 BL=20 | MIC WS=4 SCK=5 DIN=6 / SPK DOUT=7 BCLK=15 LRCK=16 | BOOT=0 VOL+=10 VOL−=39 |
| `coolkit-xz-02` (вендор) | нет в апстриме (приватная) | `30:ed:a0:c6:ad:9c`?? `НЕ ПРОВЕРЕНО` | — | — | — |
| ring VIEWE SmartRing PLUS | нет в апстриме | — | ST77916 360×360 SPI + тач CST816S | ES8311 | — |

- **Кубик и zhengchen делят аудио-пины** (`MIC 4/5/6`, `SPK 7/15/16`,
  NoAudioCodecSimplex) — **звуковой код переносится между ними без правок
  пинов**; различаются только экран/кнопки/подсветка. Значит, голосовой слой,
  написанный для кубика, заработает на zhengchen сразу.
- PotSquad собирает кубик из env **`xiaozhi-cube-154`** (board `esp32-s3-devkitc-1`,
  `BOARD_HAS_PSRAM`, `nimble_flags_mesh`, `-DPOTS_MESH=1`, partitions
  `partitions_twatch.csv`); пинмап — `include/xiaozhi_cube_154_user_setup.h`.
- Целевой env zhengchen в PotSquad **ещё не создан** (нужен отдельный env +
  user_setup по пинам выше).

## 2. Прошивка и прошивка на стенде

- Кубик: `/dev/ttyACM0` (native USB-CDC, `ARDUINO_USB_CDC_ON_BOOT=1`),
  115200. `pio run -e xiaozhi-cube-154 -t upload --upload-port /dev/ttyACM0`.
- CYD (второй стенд-пир): `/dev/ttyUSB0`, 2 000 000 бод, env `cyd-ili9341`.
- **zhengchen шьётся ЗА РАЗ, без сброса, из вручную введённого download-режима.**
  В отличие от кубика (native USB, esptool цепляется сам), на zhengchen —
  CH340 + кнопки BOOT/RST: войти в download-hold вручную, и **не сбрасывать**
  по ходу — прошивка одним прогоном (`esptool`/`pio` merged-образом). Команду
  подъёма режима и точный шаг фиксировать при первом прогоне (см. §7 compixi
  DEVICES: снятие полного дампа делали только через download-режим).
- Загрузочный лог zhengchen: DIO, PSRAM 8 MB/80 MHz, CPU 160 MHz, IDF v5.5.2;
  сток 2.2.6 имел баг `Guru Meditation` на `Assets: Refreshing display theme`.
- Полный бэкап zhengchen:
  `/home/al/firmware-backups/target_14c19f3b381c_full16M_20261002.bin`
  (16 MB, md5 `6370be97bff30f5cc9dfe45ea95c9685`).

## 3. Радио и стек (из GOTCHAS — грабли, не повторять)

- **ESP-NOW `esp_now_init` только после старта WiFi** (иначе `LoadProhibited`);
  гейт `esp_wifi_get_channel()==ESP_OK`. ESP-NOW и STA делят радио → канал
  совпадать (в ESP-NOW режиме ch 6, без ассоциации).
- BLE-меш (PotSquad) — NimBLE-Arduino; presence и короткие кадры — **в
  advertising/scan-response**. Сообщение кладётся в **scan response**
  (реклама становится `ADV_SCAN_IND`); кадр ≤27 Б, ≤3 частей текста.
- Роли NimBLE: `nimble_flags_mesh` (кубик) оставляет **broadcaster +
  peripheral**, гасит central, `MAX_CONNECTIONS=1`. Peripheral вернули ради
  BLE-OTA (`ota_ble.cpp`, GATT-сервер). CYD-набор гасит peripheral (обновлений
  по BLE нет).
- Голос по BLE-ADV **невозможен** (27 Б/кадр) → нужен **GATT-коннект**.

## 4. Хосты и сервисы

- **pixel `192.168.1.222`** (SSH `al`/`1100`): Xiaozhi-сервер (OTA `:9902`,
  WS `:9900`, aux `:9903`, MQTT `:1883`), PixelDesk/дашборд APK
  (`uvicorn pixeldsk.dash:app :44242`, отдаёт `dist/`), Vosk-зеркало `:8099`,
  агенты Pi `:8310` / Hermes `:8787` / OpenCode `:43883`.
- **`192.168.1.161` (`comp`/`1100`)** и bench-AP `192.168.5.1` — **retired**.
- Release PixelTag-App: ручной `rsync` на `al@192.168.1.222:/home/al/Projects/
  PixelTag-App/` → `gradle assembleDebug`; дашборд `PixelDesk` отдельный репо.

## 5. Репозитории (`/run/media/al/ARCHIVE/code/`)

| репо | что |
|---|---|
| `pusi/firmwares/PotSquad` | **эта прошивка** (C++/PlatformIO): PusiMesh v2, чат, персонажи, детектор |
| `compixi` | ESP-IDF «умный кубик» (Xiaozhi-совместимый), планы `plan/`, доки `docs/` |
| `xiaozhi/xiaozhi-esp32` | референс голосового стека (Opus, I2S, AFE, WS/MQTT, платы) |
| `xiaozhi/xiaozhi-esp32-server(-java/-golang)` | сервер Xiaozhi (ASR/LLM/TTS, WS) |
| `PixelTag-App` | **Android-приложение** (Kotlin, `dev.pixeltag.app`): персонаж, сквад-меш, компаньон, **CoPhone** |
| `Pixel` | спеки экосистемы (`docs/ru/*`), включая CoPhone §7 |
| `PixelDesk` | дашборд/сервер сборки APK (FastAPI, `:44242`) |
| `SquachWatch-CYD`, `BroWatch`, `BroDozor` | CYD-прошивки-детекторы (персонажи/скины референсы) |
| `PusiServer`, `servers/PusiServer` | наш будущий сервер (без Xiaozhi) |

## 6. Голосовой стек Xiaozhi (референс для нашего слоя)

- Кодек: **Opus** через IDF-компонент `esp_audio_codec`
  (`esp_opus_enc_open/process`, `esp_opus_dec_*`). Параметры:
  **16 кГц, mono, кадр 60 мс**, `ESP_OPUS_BITRATE_AUTO`
  (`xiaozhi-esp32/main/audio/audio_service.{h,cc}`).
- I2S: `NoAudioCodecSimplex` для плат без кодека (наш кубик/zhengchen);
  `audio_codec.h` — `Read/Write(int16_t*, samples)`, DMA 6×240.
- Поток: mic → I2S → AFE (AEC/VAD/WakeNet) → 16 кГц PCM → Opus-задача → сеть;
  обратно: сеть → Opus декод → I2S spk. Транспорт у Xiaozhi — **WS/MQTT**, не BLE.
- Наш путь: кодек тот же (Opus 16 кГц), транспорт и framing — **наши**
  (BLE-GATT, см. план голоса).

## 7. Контракт CoPhone (PixelTag-App ↔ кубик), `Pixel/docs/ru/09…` §7

- Управление (discovery/pairing) — **BroMesh BLE, sealed фразой сквада**;
  голос — BLE **GATT-сервис `cophone`** `8f4b8c1e-…-5e01`.
- Роли: **телефон — GATT peripheral (server), кубик — GATT central (client)**.
- Характеристики: `audio_up …5e02` (кубик→телефон, Opus 16 кГц),
  `audio_down …5e03` (телефон→кубик, notify), `text …5e04` (JSON),
  `ctrl …5e05` (опкоды). Кадры `ctrl`/`text` — 2-байтовый LE-префикс длины;
  аудио — один самодостаточный Opus-кадр на notify.
- Спаривание: SAS (6 цифр, X25519+HKDF), `pairKey` в NVS `cppairkey`; сессия
  аутентифицируется `CP_AUTH_CHAL/RESP/OK`. Опкоды: см. `CoPhoneUuids.kt`.
- Приложение уже реализует эту серверную половину (`cophone/CoPhoneGatt.kt`,
  `CoPhoneOpus.kt` на `MediaCodec`, `CoPhoneOgg.kt`), а также **Bulk**-канал
  файлов/голоса `c9c2b001-…` (client+server, без сопряжения, resume+SHA —
  `docs/BULK-FILES.md`). Прошивочной половины CoPhone в репо **нет**.

## 8. Что где искать (быстрые указатели)

- Пинмапы плат: `xiaozhi-esp32/main/boards/<board>/*.cc` + `config.h`.
- Грабли прошивки/радио: `compixi/plan/GOTCHAS.md`.
- Реестр устройств/стенда: `compixi/plan/DEVICES.md`.
- Wire-спека сквада (app): `PixelTag-App/docs/BROMESH-SPEC.md`.
- Голос/файлы по GATT (app): `PixelTag-App/docs/BULK-FILES.md`.
- CoPhone: `Pixel/docs/ru/09-faza1-cophone-airvision.md` §7.
- Наш mesh v2: `docs/PUSIMESH-V2-SPEC.md`, `docs/PUSIMESH-V2-PLAN.md`.
- Обзор экосистемы PUSI: wiki `~/wiki/PUSI.md`.

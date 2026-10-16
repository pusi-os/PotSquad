// PotsSquad-CYD — the device pages. See include/device_info.h.
//
// Every claim here has to survive being read by somebody standing next to the
// thing. Where the evidence is a chip rather than a product, the page says so:
// an Espressif prefix under FLOCK is "maybe a Flock camera", never "a Flock
// camera", and the grades in docs/DETECTIONS.md are the source for which is
// which. Kept to seven lines of 34 characters -- the MORE INFO panel in
// portrait -- which the host test checks by wrapping each one.
#include "device_info.h"
#include <string.h>
#include <strings.h>

namespace DeviceInfo {

static const char* const AXON_BODY =
    "An Axon body camera's own WiFi, up while it pairs or offloads video. Worn by police officers: it means a camera is near, not necessarily one pointed at you.";
static const char* const AXON_BODY_RU =
    "Собственный WiFi нательной камеры Axon: поднят, пока она вяжется или сливает видео. Носят офицеры: рядом камера, не обязательно направленная на тебя.";

const Device kDevices[] = {
    // ---- HACKER -------------------------------------------------------------
    { DetectionType::HACKER, "FLIPPER ZERO", "Flipper", nullptr,
      "A Flipper Zero: a pocket tool for radio, NFC, RFID and infrared. Mostly a hobby toy, but it can replay signals, copy badges and flood phones with fake Bluetooth popups.",
      "Flipper Zero: карманный инструмент для радио, NFC, RFID и ИК. Чаще игрушка, но умеет повторять сигналы, копировать пропуска и слать фейковые Bluetooth-окна." },
    { DetectionType::HACKER, "PWNAGOTCHI", "Pwnagotchi", nullptr,
      "A Pwnagotchi: a tiny computer that collects WiFi handshakes so passwords can be cracked later. It broadcasts its own name and catch count to find others like it -- that is what matched.",
      "Pwnagotchi: крошечный сборщик WiFi-хендшейков для подбора паролей. Транслирует имя и счет улова, чтобы его находили такие же, - по этому и совпало." },
    { DetectionType::HACKER, "PINEAPPLE", "Pineapple", nullptr,
      "A Hak5 WiFi Pineapple, spotted by its setup network. It pretends to be WiFi your phone already trusts and watches whatever joins. Security testers carry them; so do people up to no good.",
      "WiFi Pineapple от Hak5, виден по настроечной сети. Притворяется знакомым WiFi и смотрит, кто сел. Бывают у тестировщиков - и у темных личностей." },
    { DetectionType::HACKER, "DEAUTHER", "Deauther", nullptr,
      "An ESP8266 or ESP32 deauther, spotted by its default control network, pwned. It kicks devices off WiFi on command. Popular with hobbyists -- or it is just somebody's joke network name.",
      "Деаутентификатор на ESP8266/ESP32, виден по штатной сети pwned. Сгоняет устройства с WiFi по команде. Хобби - или шутка в имени сети." },
    { DetectionType::HACKER, "HAK5 ADDRESS", "Hak5-LA", nullptr,
      "A made-up MAC address Hak5 gear likes to use. Anyone can set one, so it is a hint rather than an ID: maybe a Pineapple, maybe a coincidence. Graded low on purpose.",
      "Выдуманный MAC, который любит железо Hak5. Поставить такой может кто угодно, так что это намек, а не ID: может, Pineapple, может, совпадение. Оценка занижена специально." },

    // ---- FLOCK: names first, then the one registered block, then the maybes --
    { DetectionType::FLOCK, "FLOCK POWER", nullptr, "FS Ext Battery",
      "The Bluetooth name of a Flock Safety external battery, which runs cameras on poles with no mains power. One of these nearby means a Flock unit is close.",
      "Bluetooth-имя внешней батареи Flock Safety - от нее работают камеры на столбах без розетки. Такая рядом - значит, камера Flock близко." },
    { DetectionType::FLOCK, "FLOCK SETUP", "Flock-Setup", "Flock_Setup",
      "A Flock camera's setup network, used while an installer configures it. Near a new pole it usually means a camera going in, or one being serviced.",
      "Настроечная сеть камеры Flock: ею пользуется монтажник. У нового столба обычно значит, что камеру ставят или обслуживают." },
    { DetectionType::FLOCK, "FLOCK CAMERA", "Flock-MA-L", nullptr,
      "A radio on Flock Safety's own registered block: one of their plate-reader cameras, which photograph every passing car and feed a database police search across towns.",
      "Радио из собственного блока Flock Safety: камера-читалка номеров. Снимают каждую машину в базу для поиска полицией через города." },
    { DetectionType::FLOCK, "FLOCK BLE", "Flock-BLE", nullptr,
      "Bluetooth from Flock gear: a XUNTONG radio (Flock's supplier), Flock's own accessory service, or a battery pack named by a bare serial number. XUNTONG sells to others and a number is just a number -- a lead.",
      "Bluetooth от железа Flock: радио XUNTONG, свой сервисный UUID или батарея с серийным номером. XUNTONG продает и другим, а номер - просто номер. Зацепка." },
    { DetectionType::FLOCK, "FLOCK HOTSPOT", "Flock-Cam", nullptr,
      "A network called Flock Camera net., which a Flock camera has been caught putting up. Near a pole camera, it is almost certainly that camera.",
      "Сеть Flock Camera net. - такую точку реально поднимала камера Flock. У камеры на столбе это почти точно она." },
    { DetectionType::FLOCK, "LITEON CHIP", "Flock-Liteo", nullptr,
      "A Liteon wireless module. Flock hardware has used them, and so have millions of laptops. A maybe, not a match.",
      "Беспроводной модуль Liteon. В железе Flock встречались, а еще в миллионах ноутбуков. Версия, а не совпадение." },
    { DetectionType::FLOCK, "WIFI MODULE", "Flock-UGSI|Flock-SiLab", nullptr,
      "A USI or Silicon Labs wireless module. Other Flock detectors have caught Flock cameras on these blocks, and the same modules sit in plenty of other gadgets. A maybe, not a match.",
      "Модуль USI или Silicon Labs. Другие детекторы ловили камеры Flock на этих блоках, но те же модули стоят много где еще. Версия, а не совпадение." },
    { DetectionType::FLOCK, "FACTORY MAC", "Flock-QCA", nullptr,
      "An address still set to the Qualcomm chip's factory default, which a Flock camera's WiFi chip ships with. Most finished products replace it, so it is rare to see -- but not only Flock forgets to.",
      "Адрес с заводским дефолтом чипа Qualcomm - с таким WiFi-чип камеры Flock выходит с завода. Готовые изделия обычно меняют его: видно редко, и забывает не только Flock." },
    { DetectionType::FLOCK, "FLOCK MAYBE", "Flock|Flock-OEM|Flock-DeFlk", nullptr,
      "An address other Flock detectors list, but whose registration does not say Flock -- or names nobody. Kept because it has turned up on Flock gear; graded low because nothing proves it.",
      "Адрес из списков детекторов Flock, но регистрация молчит. Оставлен: на железе Flock встречался. Оценка низкая: ничего не доказано." },

    // ---- AXON ---------------------------------------------------------------
    { DetectionType::AXON, "AXON BODY 2", "Axon-Body2", nullptr, AXON_BODY, AXON_BODY_RU },
    { DetectionType::AXON, "AXON BODY 3", "Axon-Body3", nullptr, AXON_BODY, AXON_BODY_RU },
    { DetectionType::AXON, "AXON BODY 4", "Axon-Body4", nullptr, AXON_BODY, AXON_BODY_RU },
    { DetectionType::AXON, "AXON NETWORK", "Axon-Field", nullptr,
      "A network named AXON-, which Axon uses on its field equipment. It points to police gear nearby: a camera, a charging dock or an in-car system.",
      "Сеть с именем AXON-: так Axon называет свое полевое железо. Рядом полицейская техника: камера, док-зарядка или автомобильная система." },
    { DetectionType::AXON, "AXON TASER", "Axon", nullptr,
      "A radio on Axon's own registered block, from its days as TASER International. Axon body cameras and TASERs both carry these.",
      "Радио из собственного зарегистрированного блока Axon - еще со времен TASER International. Такие стоят и в камерах, и в тейзерах." },
    { DetectionType::AXON, "AXON BODYCAM", "Axon-Body", nullptr,
      "An address other detectors tie to modern Axon body cameras. A lead rather than proof, so it is graded low.",
      "Адрес, который другие детекторы связывают с современными камерами Axon. Скорее зацепка, чем доказательство, - оценка низкая." },
    { DetectionType::AXON, "AXON SIGNAL", "Axon-Signal", nullptr,
      "Axon Signal: small Bluetooth sensors on holsters and patrol cars that tell nearby body cameras to start recording when a gun is drawn or the lights go on. Graded low.",
      "Axon Signal: датчики на кобурах и патрульных машинах. Велят камерам писать, когда достают оружие или включают мигалки. Оценка низкая." },
    { DetectionType::AXON, "AXON BODY CAM", "Axon-BWC", nullptr,
      "An Axon body camera, by the BWCDEVICE tag it puts in its own Bluetooth broadcast. Real cams carried it in field tests, and it survives Axon changing addresses. If a camera is recording, it is this.",
      "Нательная камера Axon по метке BWCDEVICE в ее Bluetooth. Настоящие камеры так светились в полевых тестах; смена адресов метку не убивает. Пишет - значит, она." },
    { DetectionType::AXON, "BODYWORN", "BodyWorn", "BodyWorn",
      "Utility's BodyWorn: a police body camera system. This is its activation remote, which officers clip on to start the camera recording.",
      "BodyWorn от Utility: полицейская система нательных камер. Это пульт на одежде - жмешь, и камера пишет." },
    { DetectionType::AXON, "WATCHGUARD", "WatchGuard", nullptr,
      "A radio on WatchGuard Video's block: Motorola's police body cameras and in-car video. Not the firewall company of the same name.",
      "Радио из блока WatchGuard Video - полицейские нательные камеры и авто-видео от Motorola. Не путать с одноименным firewall." },
    { DetectionType::AXON, "DIGITAL ALLY", "DigitalAlly", nullptr,
      "A radio on Digital Ally's block: body cameras and in-car video for police, ambulance and security fleets.",
      "Радио из блока Digital Ally: нательные камеры и авто-видео для полиции, скорой и охраны." },

    // ---- ALPR ---------------------------------------------------------------
    { DetectionType::ALPR, "MOTOROLA", "ALPR-Mtrla", nullptr,
      "A radio on a Motorola Solutions block. Motorola owns Vigilant, a big plate-reader maker, but also makes police radios and much else: a possible plate reader, not a sure one.",
      "Радио из блока Motorola Solutions. Motorola владеет Vigilant - крупным maker'ом читалок, но делает еще рации и много чего: возможная читалка, не точная." },
    { DetectionType::ALPR, "GENETEC", "ALPR-Gentec", nullptr,
      "A radio on a Genetec block. Genetec's AutoVu reads licence plates for police and parking enforcement, often from cameras mounted on patrol cars.",
      "Радио из блока Genetec. Их AutoVu читает номера для полиции и парковок, часто с камер на патрульных машинах." },

    // ---- SKIMMER: names first -- over Bluetooth the label is just "BLE" -------
    { DetectionType::SKIMMER, "HC-05 MODULE", "Skim-HC05", "HC-0",
      "An HC-series Bluetooth serial module: the $3 part hidden in gas-pump and ATM skimmers. Hobby projects use it too, so where you are matters. At a pump, take it seriously.",
      "Bluetooth-модуль серии HC: деталь за три доллара в скиммерах на колонках и банкоматах. Бывает и в поделках - решает место. На заправке отнесись серьезно." },
    { DetectionType::SKIMMER, "RN42 MODULE", nullptr, "RN42",
      "An RN42 Bluetooth serial module, common in electronics projects and in older card skimmers. Near a card reader, worth a second look.",
      "Bluetooth-модуль RN42: обычен в электронных поделках и в старых скиммерах. У картоприемника стоит глянуть второй раз." },
    { DetectionType::SKIMMER, "BT04-A", nullptr, "BT04",
      "A BT04-A Bluetooth serial module, a cheap HC-05 alternative. Fine inside a robot; suspicious inside a card reader.",
      "Bluetooth-модуль BT04-A, дешевая замена HC-05. В роботе нормально, внутри картоприемника подозрительно." },
    { DetectionType::SKIMMER, "LINVOR", "Skim-Linvor", "linvor",
      "An HC-06-style module on the prefix its linvor boards use: a Bluetooth serial link of the kind skimmers use to hand over stolen card numbers.",
      "Модуль в духе HC-06 на префиксе своих плат linvor: Bluetooth-последовательный канал, каким скиммеры отдают краденые номера карт." },
    { DetectionType::SKIMMER, "BT SERIAL", "Skim-SPP|Skim-CSR", nullptr,
      "Something offering a Bluetooth serial port, the link skimmers use to pass on stolen card data. Printers, scanners and car dongles use it too -- where you are is the tell.",
      "Что-то раздало Bluetooth-serial - канал, каким скиммеры передают краденые данные. Принтеры, сканеры и автосвистки тоже так умеют: решает место." },

    // ---- CAMERA -------------------------------------------------------------
    { DetectionType::CAMERA, "WYZE", "Wyze", nullptr,
      "A Wyze camera or smart-home device, on Wyze's own registered block. Cheap, popular home cameras, often pointed at front doors and driveways.",
      "Камера или домашнее устройство Wyze из их собственного зарегистрированного блока. Дешевые народные камеры - часто смотрят на двери и проезды." },
    { DetectionType::CAMERA, "WYZE MODULE", "Wyze-Mod", nullptr,
      "A wireless module of the kind Wyze builds its cameras around. Other gadgets use it too, so this one is a maybe.",
      "Беспроводной модуль из тех, вокруг которых Wyze строит камеры. Встречается и в другой технике - версия." },
    { DetectionType::CAMERA, "AMAZON", "Amazon", nullptr,
      "A radio on an Amazon block. Could be a Blink camera or a Ring device -- or an Echo, Fire TV or Kindle. Amazon's range is wide, so this is a medium guess.",
      "Радио из блока Amazon. Может быть камера Blink или устройство Ring - а может, Echo, Fire TV или Kindle. У Amazon диапазон широкий: догадка средняя." },
    { DetectionType::CAMERA, "HIKVISION", "Hikvision", nullptr,
      "A Hikvision camera: the world's biggest CCTV maker, barred from US government use over security and human-rights concerns, and common in shops and flats.",
      "Камера Hikvision: крупнейший в мире maker CCTV, отстраненный от госзакупок США из-за безопасности и прав человека. Обычна в магазинах и домах." },
    { DetectionType::CAMERA, "REALTEK", "Realtek", nullptr,
      "A Realtek WiFi chip. Plenty of cheap IP cameras use one -- so do routers, TVs and laptops. A weak camera guess.",
      "WiFi-чип Realtek. На таких полно дешевых IP-камер - как и роутеров, телевизоров, ноутбуков. Слабая догадка про камеру." },
    { DetectionType::CAMERA, "ARLO", "Arlo", nullptr,
      "An Arlo wireless security camera, battery powered and usually mounted outside homes.",
      "Беспроводная охранная камера Arlo: на батарейках, обычно снаружи домов." },
    { DetectionType::CAMERA, "BLINK", "Blink", nullptr,
      "A Blink camera or doorbell, Amazon's budget home-camera brand, usually battery powered on porches and windowsills.",
      "Камера или звонок Blink - бюджетная домашняя марка Amazon, обычно на батарейках над крылечком и на окнах." },
    { DetectionType::CAMERA, "TUYA", "Tuya", nullptr,
      "A Tuya-based smart device. Tuya makes the module inside countless no-name cameras, plugs and bulbs sold under hundreds of brands: a camera is one possibility.",
      "Устройство на Tuya. Tuya делает модуль внутри бесчисленных безымянных камер, розеток и лампочек под сотнями брендов: камера - один из вариантов." },
    { DetectionType::CAMERA, "VERKADA", "Verkada", nullptr,
      "A Verkada camera: cloud-managed cameras in schools, offices and shops. A 2021 breach exposed live feeds from about 150,000 of them.",
      "Камера Verkada: облачные камеры школ, офисов, магазинов. В 2021 утечка открыла живые ленты примерно 150 000 из них." },
    { DetectionType::CAMERA, "AVIGILON", "Avigilon", nullptr,
      "An Avigilon camera, Motorola Solutions' business video line, found in offices, campuses and city systems.",
      "Камера Avigilon - деловая видеолинейка Motorola Solutions. Стоит в офисах, кампусах, городских системах." },
    { DetectionType::CAMERA, "AXIS", "Axis", nullptr,
      "An Axis Communications network camera, from one of the oldest professional CCTV makers. Common in shops, transit and city surveillance.",
      "Сетевая камера Axis Communications - один из старейших профи-производителей CCTV. Обычна в магазинах, транспорте, городском наблюдении." },
    { DetectionType::CAMERA, "I-PRO", "i-PRO", nullptr,
      "An i-PRO camera: Panasonic's old security business, making CCTV and police body cameras. Found in cities, campuses and patrol cars.",
      "Камера i-PRO: бывшее охранное крыло Panasonic, CCTV и полицейские нательные камеры. Стоит в городах, кампусах, патрульных машинах." },

    // ---- META ---------------------------------------------------------------
    { DetectionType::META, "RAY-BAN META", "RayBanMeta", nullptr,
      "Ray-Ban Meta glasses, matched on the Bluetooth ID only they send. They take photos and video from the wearer's eye line; a small white LED is the only warning.",
      "Очки Ray-Ban Meta: совпадение только по Bluetooth-ID, который шлют лишь они. Снимают фото и видео с уровня глаз; предупреждение - мелкий белый светодиод." },
    { DetectionType::META, "META DEVICE", "Meta|Meta-Tech", nullptr,
      "A Bluetooth radio from Meta. It could be camera glasses -- or a Quest headset or controller, which carry the same ID. Look for glasses before worrying.",
      "Bluetooth-радио от Meta. Может, очки с камерой, - а может, шлем Quest или контроллер: ID у них общий. Сначала ищи очки, потом волнуйся." },
    { DetectionType::META, "LUXOTTICA", "Luxottica", nullptr,
      "A radio from Luxottica, the eyewear giant behind Ray-Ban and Oakley, which builds Meta's camera glasses.",
      "Радио от Luxottica - очкового гиганта за Ray-Ban и Oakley, который строит камерные очки для Meta." },
    { DetectionType::META, "SPECTACLES", "Snap", nullptr,
      "Snap Spectacles: camera glasses from Snapchat's maker that record clips from the wearer's point of view.",
      "Spectacles от Snap: очки с камерой от maker'а Snapchat, пишут клипы с точки зрения носителя." },
    { DetectionType::META, "VUZIX", "Vuzix", nullptr,
      "Vuzix smart glasses, sold mostly to warehouses, hospitals and police. Many models have a camera.",
      "Умные очки Vuzix - в основном склады, больницы и полиция. Во многих есть камера." },
};
const uint8_t kDeviceCount = sizeof(kDevices) / sizeof(kDevices[0]);

// Is `s` in the '|'-separated list -- exactly, or as a prefix of it when
// `prefix` is set, ignoring case for names (a person typed those).
static bool inList(const char* list, const char* s, bool prefix) {
    if (!list || !s || !s[0]) return false;
    const size_t sl = strlen(s);
    while (*list) {
        const char* end = strchr(list, '|');
        const size_t n = end ? (size_t)(end - list) : strlen(list);
        if (prefix ? (sl >= n && strncasecmp(s, list, n) == 0)
                   : (sl == n && strncmp(s, list, n) == 0))
            return true;
        if (!end) break;
        list = end + 1;
    }
    return false;
}

const Device* find(DetectionType t, const char* vendor, const char* name) {
    for (uint8_t i = 0; i < kDeviceCount; i++)
        if (kDevices[i].type == t && inList(kDevices[i].names, name, true)) return &kDevices[i];
    for (uint8_t i = 0; i < kDeviceCount; i++)
        if (kDevices[i].type == t && inList(kDevices[i].vendors, vendor, false)) return &kDevices[i];
    return nullptr;
}

}

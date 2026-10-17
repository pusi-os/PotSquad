// PotsSquad-CYD — the POTSY-DEX's entries and this board's record.
#include "dex.h"
#include "clock.h"
#include "detection.h"
#include <Preferences.h>
#include <Arduino.h>
#include <string.h>

namespace Dex {

namespace {

struct Entry {
    DetectionType type;
    Rarity        rarity;
    const char*   shortName;
    const char*   kind;
    const char*   radio;
    const char*   lore;
    const char*   habitat;
    const char*   quip;
    const char*   hint;
};

// In type order, so indexOf() is arithmetic. Rarity is a judgement about
// the world, not a count: a Tile is on every third keyring, a mesh node is
// one hiker's radio.
const Entry ENTRY[ENTRIES] = {
    { DetectionType::FLOCK, Rarity::UNCOMMON, "FLOCK", "POLICE KIT", "WIFI",
      "A plate-reading camera on a pole, sold to towns by the subscription. Photographs every car that passes, keeps the plate, and shares the list with anyone else who pays.",
      "Intersections. Car park exits. The one road into a subdivision.",
      "It photographs cars. I am not a car. Checkmate, Flock.",
      "Looks at every car that passes. On a pole, with a solar panel." },
    { DetectionType::AXON, Rarity::RARE, "AXON", "POLICE KIT", "BLUETOOTH",
      "The body camera on an officer's chest, and the holster that wakes it up. Talks Bluetooth to the phone on their belt, which is how it gives itself away.",
      "Traffic stops. Anywhere with a badge in it.",
      "Say nothing. Say it slowly. I have practiced this.",
      "Worn on a chest. Comes with a badge attached." },
    { DetectionType::META, Rarity::UNCOMMON, "GLASS", "POLICE KIT", "BLUETOOTH",
      "Sunglasses with a camera in the hinge, always paired to a phone. A small light says it is recording. The light is small on purpose.",
      "Coffee shops. Tourist spots. The face of someone keen to tell you.",
      "They built cameras into glasses. I built a better hiding spot.",
      "Somebody is wearing this one. Look for the tiny light." },
    { DetectionType::SKIMMER, Rarity::RARE, "SKIM", "SKIMMER", "BLUETOOTH",
      "A card reader hiding inside a real one, usually at a pump. A cheap Bluetooth module inside so the thief can collect the cards from the car park without touching it.",
      "Fuel pumps. The cash machine nobody else uses.",
      "Card skimmer! Do not swipe there. Do not swipe anywhere. Barter.",
      "Lives inside a card slot. Named like a hobby kit." },
    { DetectionType::MESH, Rarity::UNCOMMON, "MESH", "RADIO", "BLUETOOTH",
      "An off-grid mesh radio: a Meshtastic, MeshCore or RNode node. Caught by Meshtastic's Bluetooth UUID, or by the name the others answer to. It talks to the next node, no tower.",
      "Hikers' packs. Off-grid cabins. Anywhere a phone has no bars.",
      "Talking to nobody, on a network nobody runs. Respectable.",
      "A little radio on a mesh. Matches on its Bluetooth or its name." },
    { DetectionType::AIRTAG, Rarity::COMMON, "TAG", "TRACKER", "BLUETOOTH",
      "Apple's little white puck. Chirps every two seconds to every iPhone in earshot, so Apple can tell its owner where it went. Changes its address every fifteen minutes to keep strangers guessing.",
      "Keychains. Luggage. The bottom of your car, if you are unlucky.",
      "Not one of these was lost. Suspicious.",
      "Small, white, round. Half the airport has one." },
    { DetectionType::DRONE, Rarity::UNCOMMON, "DRONE", "TRACKER", "WIFI",
      "A drone announcing its own serial number, position and pilot. The law now says it has to. The DEX reads it straight off the air.",
      "Parks. Beaches. Above your fence.",
      "Who keeps launching those things? Tell me. I can take it.",
      "Flies. Tells you where its pilot is standing. Rude, honestly." },
    { DetectionType::ALPR, Rarity::UNCOMMON, "ALPR", "SENSOR", "WIFI",
      "A plate reader that is not a Flock. The older kind, on a police car or a tow truck, reads every passing plate and keeps the ones it wants.",
      "The roof of a patrol car. The back of a tow truck.",
      "Plate reader spotted. Classic. I have no plate. Classic.",
      "Reads plates from a moving car. Motorola makes a lot of them." },
    { DetectionType::CAMERA, Rarity::COMMON, "CAM", "CAMERA", "WIFI",
      "A network camera of the ordinary kind, from a vendor known for making them. Some watch a driveway. Some watch a whole car park. All of them are online.",
      "Doorframes. Eaves. The corner of every shop.",
      "Camera detected. Smile, legend. Or do not. Your call.",
      "The plain kind. Under an eave, pointed at a door." },
    { DetectionType::SAMSUNG_TAG, Rarity::COMMON, "STAG", "TRACKER", "BLUETOOTH",
      "Samsung's answer to the AirTag, found by every Galaxy phone that walks past. Same trick, different network, same rolling address.",
      "Keyrings. Bags. Dog collars.",
      "Same trick as the apple one. I am fooled twice a day by it.",
      "A tracker that phones home through Galaxy phones." },
    { DetectionType::GOOGLE_TAG, Rarity::UNCOMMON, "GTAG", "TRACKER", "BLUETOOTH",
      "A tracker on Google's Find My Device network: Chipolo, Pebblebee, Moto Tag. Every Android phone in range reports it, whether its owner is anywhere near.",
      "Wallets. Bikes. The remote.",
      "Third network, same idea. Everyone wants to find the keys.",
      "Found by Android phones. Several brands, one network." },
    { DetectionType::TILE, Rarity::COMMON, "TILE", "TRACKER", "BLUETOOTH",
      "The tracker that came before the others. A small square that chirps when its app asks, and announces itself to every phone with the app installed.",
      "Keys. Wallets. Attached to a cat, at least once.",
      "A tile. Not a floor tile. I checked. Twice.",
      "The square one. Older than the apple." },
    { DetectionType::RING, Rarity::COMMON, "RING", "CAMERA", "WIFI",
      "A doorbell with a camera in it, and the floodlight kind on the garage. Owned by Amazon. Footage has been handed to police without the owner being asked.",
      "Front doors. The whole street, one door at a time.",
      "Doorbell camera. I do not knock. I loom.",
      "On a front door. Has a button. Watches the street." },
    { DetectionType::DEAUTH, Rarity::RARE, "DEAUTH", "ATTACK", "WIFI",
      "Not a device: an attack. Somebody flooding the air with disconnect frames to knock devices off their WiFi. Cameras drop. Doorbells go blind. Nobody nearby means well.",
      "Wherever somebody wants a camera to stop working for a minute.",
      "Somebody is throwing everyone off the WiFi. Not me. I only sniff.",
      "Not a thing you can hold. A flood of go-away frames." },
    { DetectionType::EVILTWIN, Rarity::RARE, "TWIN", "ATTACK", "WIFI",
      "A network wearing another network's name from a second, different box. Phones join it because the name matches, and then everything they send goes through the imposter.",
      "Airports. Cafes. Anywhere a free network is expected.",
      "A network in disguise. I know a disguise when I see one. I am one.",
      "Two networks, one name, two different boxes." },
    { DetectionType::IBEACON, Rarity::COMMON, "BEACON", "TRACKER", "BLUETOOTH",
      "A proximity beacon. Shops and venues plant them so an app on your phone knows which aisle you are standing in. It exists to notice you walk past.",
      "Shelves. Museum walls. Stadium concourses.",
      "It is not following me. It is waiting for me. That is worse.",
      "Fixed to a shelf or a wall. Waits for phones." },
    { DetectionType::HACKER, Rarity::RARE, "HACK", "ATTACK", "WIFI",
      "Radio-attack hardware announcing itself. A Flipper Zero, a Pwnagotchi, a WiFi Pineapple, an ESP deauther. Exact signatures only, so a match means the tool is in the room.",
      "Conferences. The back of a hoodie. Two tables over.",
      "A radio-attack tool is in the room. Act natural. I am a lamp.",
      "A pocket toy for attacking radios. It announces itself." },
};

// The same cards in Russian, in the same type order, so indexOf() serves both
// tables and the UI picks by Settings::lang(). Keeping the two tables beside
// each other -- and switching on the LANGUAGE rather than the string -- is
// what makes "every type has a card in both languages" checkable by reading
// the file, not by rendering seventeen screens. The protocol never sees these
// bytes; they are display only.
const Entry ENTRY_RU[ENTRIES] = {
    { DetectionType::FLOCK, Rarity::UNCOMMON, "ФЛОК", "ПОЛИЦЕЙСКИЙ НАБОР", "WIFI",
      "Камера распознавания номеров на столбе, продаётся городам по подписке. Фотографирует каждую проезжающую машину, хранит номер и делится списком с теми, кто платит.",
      "Перекрёстки. Выезды с парковок. Единственная дорога в район.",
      "Он фотографирует машины. Я не машина. Шах и мат, Флок.",
      "Смотрит на каждую проезжающую машину. На столбе, с солнечной панелью." },
    { DetectionType::AXON, Rarity::RARE, "АКСОН", "ПОЛИЦЕЙСКИЙ НАБОР", "BLUETOOTH",
      "Нательная камера на груди офицера и кобура, которая её будит. Общается по Bluetooth с телефоном на поясе -- так себя и выдаёт.",
      "Остановки на дороге. Везде, где есть значок.",
      "Молчи. Говори медленно. Я тренировался.",
      "Носят на груди. Прилагается значок." },
    { DetectionType::META, Rarity::UNCOMMON, "ОЧКИ", "ПОЛИЦЕЙСКИЙ НАБОР", "BLUETOOTH",
      "Очки с камерой в дужке, всегда в паре с телефоном. Маленький огонёк говорит, что идёт запись. Огонёк маленький специально.",
      "Кофейни. Туристические места. Лицо того, кто рад рассказать.",
      "Они встроили камеры в очки. Я нашёл укрытие получше.",
      "Кто-то это носит. Ищи крошечный огонёк." },
    { DetectionType::SKIMMER, Rarity::RARE, "СКИМ", "СКИММЕР", "BLUETOOTH",
      "Считыватель карт, спрятанный внутри настоящего, обычно на колонке. Внутри дешёвый Bluetooth-модуль, чтобы вор забирал карты с парковки, не подходя.",
      "Топливные колонки. Банкомат, которым больше никто не пользуется.",
      "Скиммер! Не проводи там карту. Нигде не проводи. Меняй.",
      "Живёт внутри щели для карты. Назван как набор для хобби." },
    { DetectionType::MESH, Rarity::UNCOMMON, "МЕШ", "РАДИО", "BLUETOOTH",
      "Автономная меш-радиостанция: узел Meshtastic, MeshCore или RNode. Ловится по Bluetooth-UUID, что рекламирует прошивка Meshtastic (а MeshCore и RNode - по имени, на которое отзываются). Он не в сотовой сети; говорит напрямую со следующим узлом, в стороне от сетей по замыслу.",
      "Рюкзаки походников. Оффгрид-домики. Там, где у телефона нет палок.",
      "Говорит в пустоту, по сети, которой никто не рулит. По-честному уважаю.",
      "Мелкое радио на меше. Ловится по Bluetooth или имени." },
    { DetectionType::AIRTAG, Rarity::COMMON, "МЕТКА", "ТРЕКЕР", "BLUETOOTH",
      "Маленькая белая шайба Apple. Чирикает каждые две секунды всем iPhone в слышимости, чтобы Apple сказала владельцу, куда он делся. Меняет адрес каждые пятнадцать минут, чтобы чужие гадали.",
      "Брелоки. Багаж. Дно твоей машины, если не повезло.",
      "Ни один из них не потерялся. Подозрительно.",
      "Маленький, белый, круглый. У половины аэропорта такой есть." },
    { DetectionType::DRONE, Rarity::UNCOMMON, "ДРОН", "ТРЕКЕР", "WIFI",
      "Дрон объявляет серийный номер, позицию и пилота. Теперь так требует закон. DEX читает это прямо из воздуха.",
      "Парки. Пляжи. Над твоим забором.",
      "Кто всё это запускает? Скажи мне. Я справлюсь.",
      "Летает. Говорит, где стоит пилот. Грубо, если честно." },
    { DetectionType::ALPR, Rarity::UNCOMMON, "АЛПР", "ДАТЧИК", "WIFI",
      "Считыватель номеров, но не Флок. Старый образец на полицейской машине или эвакуаторе читает каждый номер мимо и хранит нужные.",
      "Крыша патрульной машины. Зад эвакуатора.",
      "Считыватель номеров замечен. Классика. У меня нет номера. Классика.",
      "Читает номера с движущейся машины. Motorola делает их немало." },
    { DetectionType::CAMERA, Rarity::COMMON, "КАМ", "КАМЕРА", "WIFI",
      "Сетевая камера обычного вида от известного производителя. Одни смотрят на подъездную дорожку. Другие -- на всю парковку. Все они в сети.",
      "Дверные проёмы. Карнизы. Угол каждого магазина.",
      "Камера обнаружена. Улыбнись, легенда. Или нет. Тебе решать.",
      "Самый обычный. Под карнизом, смотрит на дверь." },
    { DetectionType::SAMSUNG_TAG, Rarity::COMMON, "САМС", "ТРЕКЕР", "BLUETOOTH",
      "Ответ Samsung на AirTag, его находит каждый проходящий Galaxy. Тот же трюк, другая сеть, тот же меняющийся адрес.",
      "Брелоки. Сумки. Собачьи ошейники.",
      "Тот же трюк, что у яблочного. Меня обманывают дважды в день.",
      "Трекер, который звонит домой через телефоны Galaxy." },
    { DetectionType::GOOGLE_TAG, Rarity::UNCOMMON, "ГТАГ", "ТРЕКЕР", "BLUETOOTH",
      "Трекер в сети Google Find My Device: Chipolo, Pebblebee, Moto Tag. Его сообщает каждый Android рядом, где бы ни был его владелец.",
      "Кошельки. Велосипеды. Пульт.",
      "Третья сеть, та же идея. Все хотят найти ключи.",
      "Его находят Android-телефоны. Несколько марок, одна сеть." },
    { DetectionType::TILE, Rarity::COMMON, "ПЛИТ", "ТРЕКЕР", "BLUETOOTH",
      "Трекер, что был раньше остальных. Маленький квадрат, чирикает, когда просит приложение, и объявляет себя каждому телефону с приложением.",
      "Ключи. Кошельки. Однажды его прицепили к коту.",
      "Плитка. Не плитка на полу. Я проверил. Дважды.",
      "Квадратный. Старше яблочного." },
    { DetectionType::RING, Rarity::COMMON, "РИНГ", "КАМЕРА", "WIFI",
      "Дверной звонок с камерой внутри и прожекторная версия на гараже. Принадлежит Amazon. Записи передавали полиции, не спрашивая владельца.",
      "Входные двери. Вся улица, дверь за дверью.",
      "Камера в звонке. Я не стучу. Я нависаю.",
      "На входной двери. Есть кнопка. Следит за улицей." },
    { DetectionType::DEAUTH, Rarity::RARE, "ДЕАУТ", "АТАКА", "WIFI",
      "Не устройство, а атака. Кто-то заливает эфир кадрами разъединения, чтобы сбить устройства с WiFi. Камеры отваливаются. Звонки слепнут. Никто рядом не задумал добро.",
      "Везде, где кто-то хочет заглушить камеру на минуту.",
      "Кто-то сбрасывает всех с WiFi. Не я. Я только слушаю.",
      "Это не то, что можно взять в руки. Поток кадров \"уйди\"." },
    { DetectionType::EVILTWIN, Rarity::RARE, "ДВОЙН", "АТАКА", "WIFI",
      "Сеть под именем другой сети из второй, чужой коробки. Телефоны подключаются, потому что имя совпадает, и всё, что они шлют, идёт через самозванца.",
      "Аэропорты. Кафе. Везде, где ждут бесплатную сеть.",
      "Сеть под маской. Я узнаю маскировку с первого взгляда. Я сам маскировка.",
      "Две сети, одно имя, две разные коробки." },
    { DetectionType::IBEACON, Rarity::COMMON, "МАЯК", "ТРЕКЕР", "BLUETOOTH",
      "Маячок близости. Магазины и площадки ставят их, чтобы приложение на телефоне знало, в каком ты ряду. Он существует, чтобы замечать, как ты проходишь.",
      "Полки. Стены музеев. Коридоры стадионов.",
      "Он не идёт за мной. Он меня ждёт. Это хуже.",
      "Закреплён на полке или стене. Ждёт телефоны." },
    { DetectionType::HACKER, Rarity::RARE, "ХАК", "АТАКА", "WIFI",
      "Железо для радиоатак, объявляющее себя. Flipper Zero, Pwnagotchi, WiFi Pineapple, ESP-деаузер. Только точные сигнатуры, поэтому совпадение значит, что инструмент в комнате.",
      "Конференции. Зад худи. Через два стола.",
      "Инструмент для радиоатак в комнате. Веди себя естественно. Я лампа.",
      "Карманная игрушка для атак на радио. Объявляет себя." },
};

// The rarity label, which the card prints beside the stars.
const char* RARITY_EN[3] = { "COMMON", "UNCOMMON", "RARE" };
const char* RARITY_RU[3] = { "ОБЫЧНЫЙ", "НЕОБЫЧНЫЙ", "РЕДКИЙ" };

Record   s_rec[ENTRIES];bool     s_newBest[ENTRIES];
bool     s_dirty   = false;
uint32_t s_changed = 0;
bool     s_began   = false;
Preferences s_prefs;

const char* NS  = "dex";
const char* KEY = "rec";

void emptyRecord(Record& r) { r.firstEpoch = 0; r.lastEpoch = 0; r.bestRssi = -128; r.night = 0; }

const Entry& entry(DetectionType t) {
    const uint8_t i = indexOf(t);
    return ENTRY[i < ENTRIES ? i : 0];
}

const Entry& entryRU(DetectionType t) {
    const uint8_t i = indexOf(t);
    return ENTRY_RU[i < ENTRIES ? i : 0];
}

}  // namespace

DetectionType typeAt(uint8_t i) { return (DetectionType)(i + 1); }
uint8_t indexOf(DetectionType t) {
    const uint8_t v = (uint8_t)t;
    return (v >= 1 && v < (uint8_t)DetectionType::COUNT) ? (uint8_t)(v - 1) : ENTRIES;
}

Rarity rarity(DetectionType t)       { return entry(t).rarity; }
const char* rarityName(Rarity r) {
    switch (r) {
        case Rarity::RARE:     return "RARE";
        case Rarity::UNCOMMON: return "UNCOMMON";
        default:               return "COMMON";
    }
}
const char* rarityNameRU(Rarity r) {
    switch (r) {
        case Rarity::RARE:     return RARITY_RU[2];
        case Rarity::UNCOMMON: return RARITY_RU[1];
        default:               return RARITY_RU[0];
    }
}
uint8_t stars(Rarity r) { return r == Rarity::RARE ? 3 : r == Rarity::UNCOMMON ? 2 : 1; }

const char* shortName(DetectionType t) { return entry(t).shortName; }
const char* kind(DetectionType t)      { return entry(t).kind; }
const char* lore(DetectionType t)      { return entry(t).lore; }
const char* habitat(DetectionType t)   { return entry(t).habitat; }
const char* quip(DetectionType t)      { return entry(t).quip; }
const char* hint(DetectionType t)      { return entry(t).hint; }
const char* radio(DetectionType t)     { return entry(t).radio; }

const char* shortNameRU(DetectionType t) { return entryRU(t).shortName; }
const char* kindRU(DetectionType t)      { return entryRU(t).kind; }
const char* loreRU(DetectionType t)      { return entryRU(t).lore; }
const char* habitatRU(DetectionType t)   { return entryRU(t).habitat; }
const char* quipRU(DetectionType t)      { return entryRU(t).quip; }
const char* hintRU(DetectionType t)      { return entryRU(t).hint; }
const char* radioRU(DetectionType t)     { return entryRU(t).radio; }

void begin() {
    for (uint8_t i = 0; i < ENTRIES; i++) { emptyRecord(s_rec[i]); s_newBest[i] = false; }
    s_prefs.begin(NS, false);
    // A short read means the type list has grown since it was written; take
    // what is there and leave the rest empty.
    size_t have = s_prefs.getBytesLength(KEY);
    if (have > sizeof s_rec) have = sizeof s_rec;
    if (have >= sizeof(Record)) s_prefs.getBytes(KEY, s_rec, have - have % sizeof(Record));
    s_began = true;
}

void note(DetectionType t, int8_t rssi) {
    const uint8_t i = indexOf(t);
    if (i >= ENTRIES) return;
    Record& r = s_rec[i];
    // Only a clock that has been set gives a date worth keeping; a sighting
    // before that still updates the signal, which needs no clock.
    if (Clock::trusted()) {
        const uint32_t e = Clock::nowEpoch();
        if (!r.firstEpoch) r.firstEpoch = e;
        r.lastEpoch = e;
        if (Clock::night() && r.night < 0xFFFF) r.night++;
    }
    if (rssi > r.bestRssi) {
        if (r.bestRssi > -128) s_newBest[i] = true;   // an improvement, not a first
        r.bestRssi = rssi;
    }
    s_dirty = true;
    s_changed = millis();
}

// Ten seconds after the last change, not on every sighting: a beacon that
// bobs in and out of range would otherwise write flash all afternoon.
void tick(uint32_t now) {
    if (!s_began || !s_dirty || now - s_changed < 10000) return;
    s_dirty = false;
    s_prefs.putBytes(KEY, s_rec, sizeof s_rec);
}

const Record& record(DetectionType t) {
    const uint8_t i = indexOf(t);
    static Record none;
    if (i >= ENTRIES) { emptyRecord(none); return none; }
    return s_rec[i];
}

bool takeNewClosest(DetectionType t) {
    const uint8_t i = indexOf(t);
    if (i >= ENTRIES || !s_newBest[i]) return false;
    s_newBest[i] = false;
    return true;
}

DetectionType nemesis(const DetectionEngine& eng) {
    DetectionType best = DetectionType::UNKNOWN;
    uint32_t n = 9;   // ten or more to count
    for (uint8_t i = 0; i < ENTRIES; i++) {
        const uint32_t c = eng.lifetimeTypeCount(typeAt(i));
        if (c > n) { n = c; best = typeAt(i); }
    }
    return best;
}

void reset() {
    for (uint8_t i = 0; i < ENTRIES; i++) { emptyRecord(s_rec[i]); s_newBest[i] = false; }
    s_dirty = false;
    if (s_began) s_prefs.putBytes(KEY, s_rec, sizeof s_rec);
}

}  // namespace Dex

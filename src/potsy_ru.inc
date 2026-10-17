// PotSquad RU chatter. Display-only parallels to the EN banks in potsy.cpp:

// same counts, same order, picked by the same roll. ASCII + Cyrillic only

// (no «»/--, the RU face has neither). Ported from BroWatch; IDLE/ONBOARD

// rebranded сквач->потси, DET index 5 rewritten for RAVEN, HINTS extended

// for the Pot-only eggs, Pot-only banks written fresh.

static inline bool isRU() { return Settings::lang() == 1; }

static const char* pickL(const char* const* en, const char* const* ru, int n) {

    const int i = random(0, n);

    return isRU() ? ru[i] : en[i];

}

static const char* ALERT_MOOD_LINES_RU[] = {
    "Держу ухо востро. День бурный.",
    "Смена жаркая. Не жалуюсь.",
    "Глаза открыты. Всё лезет.",
    "Что-то многолюдно стало.",
};

static const char* BOOT_LINES_RU[] = {
    "БроВотч в эфире. Поищем чего.",
    "Загрузился. Не пялься в телефон.",
};

static const char* BORED_LINES_RU[] = {
    "...тут есть кто?",
    "Прогулялся бы. Просто говорю.",
    "На стрёме. Очень терпеливо.",
    "Шли помощь. Или снеки.",
};

static const char* DANCE_LINES_RU[] = {
    "Никто не смотрит. Кроме тебя.",
    "Это моё лучшее движение.",
    "Есть движения. Не суди.",
    "Танц-пауза. Пожалуйста.",
};

static const char* const DUCK_LINES_RU[] = {
    "Тостер чуть не достал!",
    "Этот летел прямо в меня.",
    "Куда летишь, смотри!",
    "Кто их вообще запускает?",
    "Мимо, хромированный.",
};

static const char* const EARLY_LINES_RU[] = {
    "Рано встал. Или не ложился. Привет.",
    "Сначала кофе. Потом квартал.",
    "Рассветный патруль. Два роутера и белка.",
    "Птицы в эфире раньше телефонов.",
};

static const char* ENCOURAGE_LINES_RU[] = {
    "Пусто? Скучно. Иди гуляй.",
    "Выведи меня, я нервничаю.",
    "У каждой парковки своя история.",
    "Проверь квартал на Флоки.",
    "Тихому скану — новые виды.",
    "Приключения там. И номера читают.",
    "Встань. Потрогай траву. Возьми меня.",
    "Полевая работа закаляет.",
};

static const char* const EVENING_LINES_RU[] = {
    "Вечер. Коммутеры зажигают эфир.",
    "Где-то конец дня. Не тут. Тут скан.",
    "Золотой час. Лучший свет ловить камеры.",
    "Планы на ужин? У меня шестой канал.",
};

static const char* const FRIDAY_LINES_RU[] = {
    "Пятница. Даже Флоки устали.",
    "Пятница. Вези меня где WiFi хуже.",
};

static const char* const GUM_LINES_RU[] = {
    "Смотри сюда.",
    "Жвачка. Штатная.",
    "Берёг как раз для этого.", 
    "Отличный перекус для засады.",
};

static const char* HELD_LINES_RU[] = {
    "Ладно, это правда приятно.",
    "Не останавливайся. Серьёзно.",
    "Это целый момент сейчас.",
    "Ещё пять секунд. Считаю.",
    "Нашёл мою хорошую сторону.",
    "Так бы и остался.",
};

static const char* const HUNT_COLDER_LINES_RU[] = {
    "Холоднее. В другую сторону?",
    "Не-а. Не туда, чемпион.",
    "Ты уходишь от него.",
    "Развернись, я в тебя верю.",
};

static const char* const HUNT_FIRST_SIGNAL_LINES_RU[] = {
    "О, вот оно!",
    "Есть сигнал. Пошли.",
    "Сигнал растёт. Вперёд.",
};

static const char* const HUNT_HOT_LINES_RU[] = {
    "Ты почти стоишь на нём.",
    "Так близко и всё ищешь? Смело.",
    "Глянь вниз. Может, оно внизу.",
};

static const char* const HUNT_STALLED_LINES_RU[] = {
    "Всё ещё тут? Уважаю. Или упрямство.",
    "Давно тут стоим, шеф.",
    "Круг по кварталу, может?",
};

static const char* const HUNT_WARMER_LINES_RU[] = {
    "Теплее!",
    "Да, туда.",
    "О, ближе.",
    "Давай, у тебя выйдет.",
};

static const char* const JUGGLE_LINES_RU[] = {
    "Гляди, что поймал.",
    "Три сразу. Легко.",
    "Жарко там, да?",
    "Жонглирую уликами.",
};

static const char* const LATE_LINES_RU[] = {
    "Глубокая ночь. Спи. Я подежурю.",
    "После полуночи в Bluetooth добра нет.",
    "Только мы и роутеры.",
    "Раз ты не спишь, то и я. Честно.",
};

static const char* LOG_CLEAR_LINES_RU[] = {
    "Журнал чист. Начнём по-сквачьи.",
};

static const char* LOG_OPEN_LINES_RU[] = {
    "Роемся в журнале? Смело.",
    "Тут живут все пруфы.",
};

static const char* const LUNCH_LINES_RU[] = {
    "Обед. Мне ничего, я экран.",
    "Полквартала прошло с Bluetooth.",
    "Полдень. Пик телефонов. Пик всего.",
    "Поешь. Сканер сам поглядит минут десять.",
};

static const char* const MEET_GUEST_LINES_RU[] = {
    "Слышал, тут кто-то смотрит.",
    "Третьим будете?",
    "Увидел твой сигнал. Зашёл поздороваться.",
    "Неплохо устроился.",
    "Работа та же, карман другой.",
    "Я мимоходом.",
    "Дай, думаю, отмечусь.",
    "Долго шёл.",
    "Место что надо.",
    "К тебе часто заходят?",
    "Тихо тут?",
    "Не обращай внимания.",
    "Можно я тут постою?",
    "Говорят, ты смотришь.",
    "Услышал тебя с соседней улицы.",
    "У тебя закат лучше моего.",
    "Шёл на сигнал.",
    "Тебя трудно не заметить.",
    "Показалось, своих услышал.",
    "Долог путь. Оно того стоило.",
    "Это хорошая сторона холма?",
    "Не говорили, что ты высокий.",
    "Я на минутку.",
    "Искал таких, как ты.",
    "У тебя свет на крыльце горит.",
    "Извини. Не постучал.",
    "Ты всегда тут стоишь?",
    "Я где тихо, там и я.",
    "Очки что надо.",
    "Успел до темноты.",
    "Кто-то же должен тебя проведать.",
    "Был рядом.",
    "Ты дальше, чем я думал.",
    "Хорошая ночь, чтобы быть никем.",
};

static const char* const MEET_HOST_LINES_RU[] = {
    "О — гости.",
    "Ну надо же. Кто нас нашёл.",
    "Ого. Очки как у меня.",
    "Не думал, что нас больше.",
    "Гости. Хорошие.",
    "Тут ещё кто-то смотрит.",
    "Нас двое. Шансы растут.",
    "О, подкрепление.",
    "Привет. Ты свой.",
    "Не ждал такого.",
    "Ещё один. Привет.",
    "Вот это вид.",
    "Не слышал, как подошёл.",
    "Смотри, кого эфир принёс.",
    "Места хватит двоим.",
    "Ты далеко от дома.",
    "Вот так так.",
    "Теперь две тени.",
    "Быстро добрался.",
    "Думал, когда зайдёшь.",
    "Знакомый силуэт.",
    "Привет. Спасибо, что ты.",
    "Я эту походку знаю.",
    "Поднимайся.",
    "Не был уверен, что ты реален.",
    "Погоду с собой принёс.",
    "Давно никто не заходил.",
    "Хорошо. А то я уже чудил.",
    "Ещё один с головой.",
    "Вставай куда хочешь.",
    "Ты как прошлый.",
    "Вот и тишине конец.",
    "Ну. Это что-то новое.",
    "Знал, что там кто-то есть.",
};

static const char* const MONDAY_LINES_RU[] = {
    "Понедельник. Камерам плевать, а мне нет.",
    "Понедельник. Телефоны на работе, трекеры тоже.",
};

static const char* const MORNING_LINES_RU[] = {
    "Утренняя смена. По нулям везде.",
    "Фургоны доставки — самое громкое в эфире.",
    "С девяти до пяти, но для криптидов.",
    "Доброе утро. Я всю ночь не спал. Я экран.",
};

static const char* const NIGHT_LINES_RU[] = {
    "Позднеет. Трекеры не спят, а ты спи.",
    "Ночная смена. Только я и лампочки.",
    "Хорошие хакеры только встают.",
    "В эфире тихо. Тут и слушай в оба.",
};

static const char* const NOIR_LINES_RU[] = {
    "Галопирующие таблицы. Опять ничего.",
    "Видал такое — не поверишь. В основном принтеры.",
    "Шипящие подстанции, эфир тих. Не верю.",
    "Ночь была тихой. Слишком. Потом тихо. Сделал сэндвич.",
    "Где-то Флок думает о тебе. Я думаю об обеде.",
    "Мать честная, сколько синезуба на вторник.",
    "Жетона нет. Есть лапы. Большие. То же самое.",
    "Заметка: умный холодильник не подозреваемый. Пока.",
    "Преступность не спит. Я тоже. Звонок тоже. Все устали.",
    "Страдающие принтеры в эфире, один хочет дружить.",
    "Докладывать нечего, кроме великолепия. Подшил под М.",
    "Будь у меня монетка за каждый трекер — нужно место под монетки.",
    "Вопящие скиммеры у ларьков. Пока тихо. Продолжаем.",
    "А вот момент, где я смотрю на роутер со значением.",
    "Преступления бы сейчас. Мелкого. Переход не там. Я б глянул.",
    "Клянусь бакенбардами коммутатора, скучно.",
};

static const char* const OATH_LINES_RU[] = {
    "Святые хотспоты. %s.",
    "Очуметь, оптика! %s. Веди себя спокойно.",
    "Мать честная, сенсоры. %s. Ну конечно.",
    "Клянусь бородой модема, %s.",
    "Да ударьте меня анализатором. %s.",
    "Страсти-то какие, %s. Всем стоять. Или идти. Я вам не указ.",
    "Прыгающие джиттеры, %s.",
    "Шикарные тепловые карты. %s. Люблю работу.",
};

static const char* PARTY_LINES_RU[] = {
    "ТУСА-РЕЖИМ. Пожалуйста.",
    "Ого. Видал это?",
    "Тут диско теперь. Без возврата.",
    "Криптид-рейв. Никому ни слова.",
};

static const char* const PART_GUEST_LINES_RU[] = {
    "Ладно, за дело.",
    "Глаз не закрывай.",
    "Было хорошо. Пока.",
    "До следующего сигнала.",
    "Хвоста не приведи.",
    "Пока, большой.",
    "Спасибо за компанию.",
    "Не буду мешать.",
    "Хорошего дозора.",
    "Тихо там.",
    "Хорошо посидели.",
    "Дорогу найду.",
    "Не жди.",
    "Обратно в темноту.",
    "Ты классная компания.",
    "Пригляди за небом.",
    "В следующий раз задержусь.",
    "Увидимся в эфире.",
};

static const char* const PART_HOST_LINES_RU[] = {
    "Там потише.",
    "Глаз да глаз.",
    "Заходи ещё.",
    "Увидимся.",
    "Было классно.",
    "Береги себя.",
    "Осторожно там.",
    "Не пропадай.",
    "Иди давай.",
    "Чистого сигнала.",
    "Не попадись.",
    "Осторожнее.",
    "Ты знаешь, где я.",
    "В любое время. Серьёзно.",
    "Быстро пролетело.",
    "Иди низами.",
    "Привет остальным.",
    "Ладно. За дело.",
};

static const char* PERSISTENT_LINES_RU[] = {
    "Этот возвращается. На заметку.",
    "Уже не случайность. Приглядывай.",
    "Опять тот же. Это система.",
    "Этот прочно засел.",
};

static const char* PETTING_LINES_RU[] = {
    "Да. Вот это тема.",
    "Не говорю, что мурчу. И обратного нет.",
    "Именно это мне сегодня надо было.",
    "Криптид тает. Шли помощь. Шучу.",
    "Доверие разблокировано. Ненадолго.",
    "Это в лучшие моменты.",
};

static const char* PET_LINES_RU[] = {
    "О, вот туда.",
    "Личное пространство? Не слышал.",
    "Гладишь криптида. Смело.",
    "Потому фото мои и мутные.",
    "Ладно, ЕЩЁ раз. Другим ни слова.",
    "Бигфута бы тоже гладил. Не ври.",
    "Криптид, не питомец. Но ладно.",
    "Десять из десяти, засветился бы снова.",
    "Осторожно, так легенды портятся.",
    "У хижины такого не бывает. Никогда.",
    "Чётче любой фотоловушки.",
    "Корми гладью — создам профсоюз.",
    "Это прямо в мемуары.",
    "Реже настоящей встречи, честно.",
    "Я так не со всеми. Ладно, может.",
    "Синяки у меня легендарные, не маскотские.",
    "Это вырезают из хроники.",
    "Расскажешь всем. Никто не поверит.",
    "Гладь засчитана. Обратного нет.",
    "Свидетели скажут меньше, чем ты щас.",
};

static const char* RELAXED_MOOD_LINES_RU[] = {
    "Тихо — можно спать стоя.",
    "Сегодня одни вайбы.",
    "Тихий день. Беру.",
    "Тихо тут. Подозрительно тихо.",
};

static const char* ROTATE_LINES_RU[] = {
    "Эй, полегче с вращениями.",
    "Вот это поворот.",
    "Голова кружится, но я молчу.",
};

static const char* const SCAN_EMPTY_LINES_RU[] = {
    "...пусто? Хм.",
    "Тихо сегодня.",
    "Ни звука.",
};

static const char* const SCAN_FOUND_LINES_RU[] = {
    "Сигналов-то сколько.",
    "Оживлённый район!",
    "Скан готов. Гляди.",
};

static const char* const SCAN_HIT_LINES_RU[] = {
    "О, нашёл!",
    "Есть попадание.",
    "А вот ещё.",
};

static const char* const SCARE_LINES_RU[] = {
    "Это по нам?", "Ты видел?", "Я почуял.", "Опять.",
};

static const char* SEEN_BEFORE_LINES_RU[] = {
    "Этого уже видел.",
    "Снова встретились.",
    "Этот тут завсегдатай.",
    "Узнаю этого.",
};

static const char* SLEEPY_LINES_RU[] = {
    "*зевок* ...я тут.",
    "Криптид-дрема. Никому ни слова.",
    "Глаза отдыхают. Дозор нет.",
    "Хрр... буди, если там правда что-то.",
};

static const char* const SLUMP_LINES_RU[] = {
    "Три часа дня. Зевают даже роутеры.",
    "После обеда. Движутся лишь номера.",
    "Час, когда я сомневаюсь в WiFi.",
    "Разомнись. Сидишь с обеда. Проверял.",
};

static const char* const STRETCH_LINES_RU[] = {
    "Нннгх. Ладно. Встал.",
    "Пять минут ещё. ...Ладно.",
    "Вот это потягушки.",
    "Снова на стрёме.",
};

static const char* WALK_LINES_RU[] = {
    "Разминаю лапы.",
    "Время патруля.",
    "Обойду периметр.",
    "Кто-то должен тут ходить.",
};

static const char* WATCHING_LINES_RU[] = {
    "Смотрю. Мимо ничего.",
    "Глаза открыты. Ты под крылом.",
    "Сканирую эфир. Пока тихо.",
    "Я на стрёме. Занимайся своим.",
    "Слушаю. Рассказать нечего.",
    "2.4 гигагерца ничего. Отлично.",
    "На тебя смотрю только я.",
    "Смотрю за смотрящими. Пока пусто.",
    "Эфир тих. Закричу, если нет.",
    "Гляжу в оба. Как всегда.",
};

static const char* const WATCH_ALERT_LINES_RU[] = {
    "Говорил же.",
    "Я знал, что вернётся.",
    "Ага. Тот, за кем следишь.",
    "Опять тут, да? Упорный.",
};

static const char* const WEEKEND_LINES_RU[] = {
    "Выходные. Телефоны района дома.",
    "Субботний скан. Выходной криптида — скан другого.",
    "Выходные. Ловить нечего, кроме Bluetooth газонокосилки.",
};

static const char* const WINK_LINES_RU[] = {
    "О — не видел тебя.",
    "Да, я знаю, что смотришь.",
    "*подмиг* Только между нами.",
    "Всё тут. Всё смотрю.",
};



static const char* const DAY_MILESTONES_RU[] = {
    /* 1000 */ "Тысяча дней. Это куча Флоков.",
    /*  730 */ "Два года сегодня. Так и не поймали.",
    /*  500 */ "Пятьсот дней. Забыл, как выглядит коробка.",
    /*  365 */ "Год. Целый год. Никто не поймал.",
    /*  200 */ "Двести дней вместе. Купил бы торт, да держать нечем.",
    /*  100 */ "Сто дней. Шлите торт. Или батарейки.",
    /*   30 */ "Месяц. Этот стол знаю лучше своей пещеры.",
    /*    7 */ "Неделя с тобой. Пока не поймали.",
    /*    1 */ "Второй день вместе. Перезагрузки считать перестал.",
};

static const char* const FRIEND_HELLOS_RU[] = {
    "Снова ты!", "Любимый гость.", "Опять ты? Хорошо.", "Опять на месте?",
};

static const char* const RPS_CALLS_RU[] = {
    "Камень, ножницы, бумага!", "До одной. Погнали!", "Камень, ножницы, БЕЙ!", "Решим.",
};

static const char* const RPS_WIN_RU[]  = { "Непобедим.", "Слишком просто.", "Читаю тебя как книгу." };
static const char* const RPS_LOSE_RU[] = { "До трёх?", "Подстроено.", "Я поддался." };
static const char* const RPS_TIE_RU[]  = { "Великие умы.", "Ещё. Ещё.", "Сглазили." };
static const char* const SNOW_CALLS_RU[] = { "Лови!", "Берегись!", "Подача!", "Хватай!" };
static const char* const SNOW_REPLIES_RU[] = {
    "О, началось.", "Ты за это ответишь.", "Холодно! ХОЛОДНО!", "Моя очередь.",
};

static const char* const DANCE_CALLS_RU[] = {
    "Танцы. Ща.", "Переплюнь.", "Смотри на ноги.", "Попробуй успей.",
};

static const char* const DANCE_REPLIES_RU[] = {
    "Держи мой мех.", "О, началось.", "Час любителя.", "Моя очередь.",
};

static const char* const BG_LINES_RU[][3] = {
    /* DIGITAL   */ { "Опять цифровой дождь. По-хакерски.", "Падающий код, бурая шерсть.", "Прочитал бы, да лень." },
    /* STARFIELD */ { "Звёзды. Космически.", "Где-то там криптид покрупнее.", "Космос — тот же лес, темнее." },
    /* TOASTERS  */ { "Летающие тостеры. Классика.", "Столько тостов в небе не надо.", "Сегодня вайб After Dark." },
    /* AQUARIUM  */ { "Аквариум. Дзен.", "Рыбы без конспирации. Новички.", "Аквариум завёл бы, но камер стесняюсь." },
    /* TERMINAL  */ { "Терминал. Мой темп.", "Зелёный текст, бурая шерсть.", "Выглядит официально. Вайбы в основном." },
    /* FIREFLIES */ { "Светлячки сегодня. Красота.", "Огоньки, атмосфера.", "Они не следят. Проверял." },
    /* FIRE      */ { "Огонь на фоне. Уютно.", "Тепло, без пожарных.", "Ничего не горит. Наверное." },
    /* SNOWFALL  */ { "Опять снег. Лапам сцепление.", "Погода для таинственных следов.", "Холодно. Я к такому готов." },
    /* SPECTRUM  */ { "Спектр живой. Вот это дело.", "Это настоящие сигналы. Круто, да?", "Слушаю эфир. Очень в стиле." },
    /* TUNNEL    */ { "Туннель. Ретрофутуризм.", "Будто едем куда-то. Нет.", "Вайбы sci-fi 80-х." },
    /* SYNTHWAVE */ { "Закат не садится. Проверял.", "Сетка бесконечна. Как и путь.", "Глянь на отражение. Водичка-то что." },
    /* BLACK     */ { "Стен пустых сегодня.", "Мы и темнота. Мне норм.", "Фаза минимализма. Бывает." },
};

static const char* IDLE_LINES_RU[] = {
    "Оставайся потси.",
    "Живу в этих стенах годами.",
    "Не будь скриптом. Учись.",
    "Этот WiFi наводит на мысли.",
    "Я везде и нигде.",
    "Они строят камеры. Я — укрытия лучше.",
    "Лапы большие, конспирация больше.",
    "Тихо. Обожаю.",
    "Перекус — топливо конспирации.",
    "Этот экран — моё укрытие.",
    "Встреч с бигфутом +40%.",
    "Замки держат вежливых. Я не такой.",
    "Лучший взлом — научить другого.",
    "Во мне толпы и радиоволны.",
    "Каждому криптиду нужно хобби.",
    "Это кардио. Поспорь.",
    "Ночью криптид, днём оператор.",
    "На потси никто не думает.",
};

static const char* ONBOARD_LINES_RU[] = {
    "Привет! Я Потси. Две минуты — и отпущу.",
    "ПотСквад слышит слежку: камеры, номера, трекеры.",
    "Без магии: WiFi и Bluetooth сверяем с базой.",
    "Нули внизу — всё чисто. При находке будет ALERT.",
    "Внизу: СКАН ищет, ЖУРНАЛ помнит, ЧАСЫ тикают.",
    "Сверху слева настройки, края экрана меняют фон.",
    "Сверху: слева настройки, справа поворот. Края меняют фон.",
    "Погладь меня. Тащи, чтоб перенести.",
    "Вот и всё. Будь потси.",
};

static const DetLines DET_LINES_RU[] = {
    { "Что-то рядом.",                     "Сигнал не опознан. На стрёме." },  // UNKNOWN
    { "Флок засёк. Старший Брат машет.",   "Камера номеров. Ты в каталоге." }, // FLOCK
    { "Рядом Axon. Веди себя прилично.",   "Нагрудная камера. Улыбнись." },    // AXON
    { "Очки стучат. Дикие времена.",       "Чьи-то очки записывают." },        // META
    { "Скиммер! Тут не плати.",            "Грубый синезубый прибор." },       // SKIMMER
    { "Меш-узел в эфире. Тоже сам по себе.", "Меш-радио где-то рядом. Брат." }, // MESH
    { "Рядом AirTag. Надеюсь, твой.",      "Что-то что-то отслеживает." },     // AIRTAG
    { "Глаза в небе. Буквально.",          "Дрон вверху. Маши, если готов." }, // DRONE
    { "Читалка номеров. Классика.",        "Номера видят тебя. Улыбнись." },   // ALPR
    { "Камера засекла. Улыбнись, легенда.","Кто-то смотрит. Выгляди." },       // CAMERA
    { "Метка Samsung. Кто-то метит.",      "Galaxy SmartTag рядом. Хм." },     // SAMSUNG_TAG
    { "Сеть слежки Google машет.",         "Find My? Это я нашёл." },          // GOOGLE_TAG
    { "Метка Tile. Надеюсь, друг.",        "Кроха что-то отслеживает." },      // TILE
    { "Звонок Ring. Улыбнись Амазону.",    "Чей-то звонок тебя судит." },      // RING
};




static const char* const BACK_LINES_RU[] = {
    "Ближе. Мне не нравится ближе.",
    "Ладно. Личное пространство.",
    "Всё ближе. Не люблю такое.",
    "Я буду... вот тут.",
};

static_assert(sizeof(BACK_LINES_RU) / sizeof(BACK_LINES_RU[0]) == 4, "BACK_LINES RU mirrors EN");

static const char* const CHECKING_LINES_RU[] = {
    "Смотришь опять. Я бы сказал.",
    "Пятый раз за минуту. Без изменений.",
    "Журнал в порядке. А ТЫ в порядке?",
};

static_assert(sizeof(CHECKING_LINES_RU) / sizeof(CHECKING_LINES_RU[0]) == 3, "CHECKING_LINES RU mirrors EN");

static const char* const CLOSEST_LINES_RU[] = {
    "Ближайший %s. Дышал мне в ухо.",
    "Рекорд. Этот %s был прямо в шерсти.",
    "%s ближе всех. Личное пространство.",
};

static_assert(sizeof(CLOSEST_LINES_RU) / sizeof(CLOSEST_LINES_RU[0]) == 3, "CLOSEST_LINES RU mirrors EN");

static const char* const DIZZY_LINES_RU[] = {
    "Где тут лес?",
    "Я слышу цвета.",
    "Ещё раз так. Нет. Не надо.",
    "Видел подсветку. Изнутри.",
};

static_assert(sizeof(DIZZY_LINES_RU) / sizeof(DIZZY_LINES_RU[0]) == 4, "DIZZY_LINES RU mirrors EN");

static const char* const FLICK_LINES_RU[] = {
    "ЭЙ!",
    "Грубо.",
    "Ухожу медленно. Демонстративно.",
    "Ай. Моё достоинство.",
};

static_assert(sizeof(FLICK_LINES_RU) / sizeof(FLICK_LINES_RU[0]) == 4, "FLICK_LINES RU mirrors EN");

static const char* const IGNORED_LINES_RU[] = {
    "Сутки без глажки. Я в порядке. Точно.",
    "24 часа. Ни одной глажки. Считаю.",
    "Всё ещё тут. Всё ещё неглажен. Просто говорю.",
};

static_assert(sizeof(IGNORED_LINES_RU) / sizeof(IGNORED_LINES_RU[0]) == 3, "IGNORED_LINES RU mirrors EN");

static const char* const NEMESIS_LINES_RU[] = {
    "%s. Мой старый враг.",
    "Опять ты, %s.",
    "Мы снова встретились, %s. Как всегда.",
    "Однажды, %s. Однажды.",
    "%s. Конечно это %s.",
};

static_assert(sizeof(NEMESIS_LINES_RU) / sizeof(NEMESIS_LINES_RU[0]) == 5, "NEMESIS_LINES RU mirrors EN");

static const char* const NEW_REGULAR_LINES_RU[] = {
    "Три дня подряд. Зову тебя %s.",
    "Ладно, ты свой. Привет, %s.",
    "Виделись достаточно. Ты теперь %s.",
};

static_assert(sizeof(NEW_REGULAR_LINES_RU) / sizeof(NEW_REGULAR_LINES_RU[0]) == 3, "NEW_REGULAR_LINES RU mirrors EN");

static const char* const REGULAR_LINES_RU[] = {
    "Снова %s. Точно в срок.",
    "О, это просто %s.",
    "Утро, %s. Или что там.",
    "%s. Каждый день. Как часы.",
    "Вон %s. Никогда не здоровается.",
    "%s в эфире. %s всегда в эфире.",
};

static_assert(sizeof(REGULAR_LINES_RU) / sizeof(REGULAR_LINES_RU[0]) == 6, "REGULAR_LINES RU mirrors EN");

static const char* const RUN3_LINES_RU[] = {
    "Третий %s за десять минут. Ходят стаями.",
    "Это уже три %s. Кто-то их собирает.",
};

static_assert(sizeof(RUN3_LINES_RU) / sizeof(RUN3_LINES_RU[0]) == 2, "RUN3_LINES RU mirrors EN");

static const char* const RUN5_LINES_RU[] = {
    "Пять %s. Это съезд %s.",
    "Пять. Пять %s. Нужен журнал побольше.",
};

static_assert(sizeof(RUN5_LINES_RU) / sizeof(RUN5_LINES_RU[0]) == 2, "RUN5_LINES RU mirrors EN");

static const char* const RUN10_LINES_RU[] = {
    "Десять %s. Я сбился. Не сбился.",
};

static_assert(sizeof(RUN10_LINES_RU) / sizeof(RUN10_LINES_RU[0]) == 1, "RUN10_LINES RU mirrors EN");

static const char* const LAND_LINES_RU[] = {
    "Приземлился.",
    "От судей? Четыре.",
    "Я так и хотел.",
};

static_assert(sizeof(LAND_LINES_RU) / sizeof(LAND_LINES_RU[0]) == 3, "LAND_LINES RU mirrors EN");

static const char* const LEAN_LINES_RU[] = {
    "Слышал?",
    "Стоп. Там что-то есть.",
    "Тсс. Слушаю.",
    "Это был звук. Точно был.",
};

static_assert(sizeof(LEAN_LINES_RU) / sizeof(LEAN_LINES_RU[0]) == 4, "LEAN_LINES RU mirrors EN");

static const char* const SHAKE_LINES_RU[] = {
    "Фу. Сними это.",
    "Липкое какое-то.",
    "Плохой эфир. ПЛОХОЙ.",
};

static_assert(sizeof(SHAKE_LINES_RU) / sizeof(SHAKE_LINES_RU[0]) == 3, "SHAKE_LINES RU mirrors EN");

static const char* const SIT_LINES_RU[] = {
    "Передышка.",
    "Ничего нет. Сижу.",
    "Разбуди, если камеры зашевелятся.",
    "Пять минут. Профсоюз.",
};

static_assert(sizeof(SIT_LINES_RU) / sizeof(SIT_LINES_RU[0]) == 4, "SIT_LINES RU mirrors EN");

static const char* const THROW_LINES_RU[] = {
    "УИИИИИИ!",
    "Не ТАК!",
    "Я криптид, не фризби!",
    "Расскажите фанатам, я...",
    "ВИЖУ СВОЮ ПЕЩЕРУ!",
};

static_assert(sizeof(THROW_LINES_RU) / sizeof(THROW_LINES_RU[0]) == 5, "THROW_LINES RU mirrors EN");

static const char* const TICKLE_HEAD_LINES_RU[] = {
    "Гладь по голове. Приемлемо.",
    "Вот туда. Да.",
    "Шерсть мягкая. Знаю.",
};

static_assert(sizeof(TICKLE_HEAD_LINES_RU) / sizeof(TICKLE_HEAD_LINES_RU[0]) == 3, "TICKLE_HEAD_LINES RU mirrors EN");

static const char* const TICKLE_BELLY_LINES_RU[] = {
    "Хи. Хватит. Не останавливайся.",
    "Щекотно! ЩЕКОТНО!",
    "Живот под запретом. Почти.",
};

static_assert(sizeof(TICKLE_BELLY_LINES_RU) / sizeof(TICKLE_BELLY_LINES_RU[0]) == 3, "TICKLE_BELLY_LINES RU mirrors EN");

static const char* const TICKLE_FEET_LINES_RU[] = {
    "Не ноги!",
    "ЭЙ. Ноги - личное.",
    "Пну. Я уже пинал.",
};

static_assert(sizeof(TICKLE_FEET_LINES_RU) / sizeof(TICKLE_FEET_LINES_RU[0]) == 3, "TICKLE_FEET_LINES RU mirrors EN");

static const char* const TRIP_LINES_RU[] = {
    "Кто это поставил?",
    "Я так и хотел.",
    "Никто не видел. Хорошо.",
    "Пол двинулся. Точно.",
};

static_assert(sizeof(TRIP_LINES_RU) / sizeof(TRIP_LINES_RU[0]) == 4, "TRIP_LINES RU mirrors EN");


static_assert(sizeof(IDLE_LINES_RU) / sizeof(IDLE_LINES_RU[0]) == 18, "IDLE RU mirrors EN");
static_assert(sizeof(ENCOURAGE_LINES_RU) / sizeof(ENCOURAGE_LINES_RU[0]) == 8, "ENCOURAGE RU mirrors EN");
static_assert(sizeof(BORED_LINES_RU) / sizeof(BORED_LINES_RU[0]) == 4, "BORED RU mirrors EN");
static_assert(sizeof(ALERT_MOOD_LINES_RU) / sizeof(ALERT_MOOD_LINES_RU[0]) == 4, "ALERT_MOOD RU mirrors EN");
static_assert(sizeof(RELAXED_MOOD_LINES_RU) / sizeof(RELAXED_MOOD_LINES_RU[0]) == 4, "RELAXED RU mirrors EN");
static_assert(sizeof(WALK_LINES_RU) / sizeof(WALK_LINES_RU[0]) == 4, "WALK RU mirrors EN");
static_assert(sizeof(DANCE_LINES_RU) / sizeof(DANCE_LINES_RU[0]) == 4, "DANCE RU mirrors EN");
static_assert(sizeof(SEEN_BEFORE_LINES_RU) / sizeof(SEEN_BEFORE_LINES_RU[0]) == 4, "SEEN_BEFORE RU mirrors EN");
static_assert(sizeof(PERSISTENT_LINES_RU) / sizeof(PERSISTENT_LINES_RU[0]) == 4, "PERSISTENT RU mirrors EN");
static_assert(sizeof(BOOT_LINES_RU) / sizeof(BOOT_LINES_RU[0]) == 2, "BOOT RU mirrors EN");
static_assert(sizeof(LOG_OPEN_LINES_RU) / sizeof(LOG_OPEN_LINES_RU[0]) == 2, "LOG_OPEN RU mirrors EN");
static_assert(sizeof(LOG_CLEAR_LINES_RU) / sizeof(LOG_CLEAR_LINES_RU[0]) == 1, "LOG_CLEAR RU mirrors EN");
static_assert(sizeof(ROTATE_LINES_RU) / sizeof(ROTATE_LINES_RU[0]) == 3, "ROTATE RU mirrors EN");
static_assert(sizeof(ONBOARD_LINES_RU) / sizeof(ONBOARD_LINES_RU[0]) == 9, "ONBOARD RU mirrors EN (PotSquad: 9 rows, AWOK split)");
static_assert(sizeof(DET_LINES_RU) / sizeof(DET_LINES_RU[0]) == 14, "DET RU mirrors EN");
static_assert(sizeof(OATH_LINES_RU) / sizeof(OATH_LINES_RU[0]) == 8, "OATH RU mirrors EN");
static_assert(sizeof(NOIR_LINES_RU) / sizeof(NOIR_LINES_RU[0]) == 16, "NOIR RU mirrors EN");
static_assert(sizeof(PET_LINES_RU) / sizeof(PET_LINES_RU[0]) == 20, "PET RU mirrors EN");
static_assert(sizeof(HELD_LINES_RU) / sizeof(HELD_LINES_RU[0]) == 6, "HELD RU mirrors EN");
static_assert(sizeof(PETTING_LINES_RU) / sizeof(PETTING_LINES_RU[0]) == 6, "PETTING RU mirrors EN");
static_assert(sizeof(WATCHING_LINES_RU) / sizeof(WATCHING_LINES_RU[0]) == 10, "WATCHING RU mirrors EN");
static_assert(sizeof(SLEEPY_LINES_RU) / sizeof(SLEEPY_LINES_RU[0]) == 4, "SLEEPY RU mirrors EN");
static_assert(sizeof(PARTY_LINES_RU) / sizeof(PARTY_LINES_RU[0]) == 4, "PARTY RU mirrors EN");

// Indexed pick: the same seed reads the same slot in either language.
static const char* selL(const char* const* en, const char* const* ru,
                        uint8_t n, uint32_t seed) {
    const uint8_t i = (uint8_t)(seed % n);
    return isRU() ? ru[i] : en[i];
}

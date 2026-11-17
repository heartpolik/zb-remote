# zb-remote

DIY Zigbee-пульт для Home Assistant: nRF52840 (Pro Micro / nice!nano-клон), 14 кнопок з плати старого ІЧ-пульта, OLED 128×64, живлення від батареї. Працює через Zigbee2MQTT як sleepy end device.

![UI](docs/preview.png)

*Рендер справжнього `src/oled.c` на хості (`tools/preview/render.sh`): головний екран і сповіщення в горизонтальному режимі, праворуч вертикальний режим.*

## Можливості

- **14 кнопок** × 3 дії (`single` / `double` / `hold`) → `action: "<N>_<дія>"` у Z2M.
- **Екран** керується з HA: 3 плитки «іконка + значення», повноекранне сповіщення (іконка ×2, до 3 рядків, мерехтіння), горизонтальна/вертикальна орієнтація, поворот на 180°, зсув зображення.
- **Батарея** (`genPowerCfg`): відсоток і напруга, звіт щогодини та після натискання.
- **Zigbee 3.0 sleepy end device**: long poll 5 с, після натискання turbo poll 10 с; звіти, що з'явились до підключення, ставляться в чергу і відправляються після join.
- Пейринг зберігається між прошивками (окремий NVRAM-розділ).

## Залізо

| Вузол | Що | Піни (силкскрін Pro Micro) |
|---|---|---|
| MCU | Pro Micro nRF52840, Adafruit UF2 bootloader 0.6.0 | — |
| OLED | SSD1306 128×64, I²C `0x3c` | SDA `017`, SCL `020`, живлення з VCC-ключа `013` |
| Клавіатура | плата ІЧ-пульта, мікросхему випаяно, 6 ліній | `006` (GND мікросхеми), `008`, `022`, `024`, `100`, `011` |
| LED | синій, вбудований | `015` |
| Батарея | LiPo на `VDDH` (вимір через `VDDH/5`) | — |

### Клавіатура (pair matrix)

Кожна кнопка замикає **пару** з 6 ліній. Сканування: одна лінія тягнеться в 0, решта читаються з pull-up; період 25 мс, debounce 2 скани. Пари, замкнені без натискання (живлення, конденсатори), запам'ятовуються при старті як baseline й ігноруються.

Таблиця в `src/keymap.h` (визначена 2026-11-17):

| Кнопка | Лінії | Кнопка | Лінії |
|---|---|---|---|
| 1 | 024–100 | 8 | 022–024 |
| 2 | 100–011 | 9 | 006–100 |
| 3 | 022–011 | 10 | 006–024 |
| 4 | 024–011 | 11 | 006–022 |
| 5 | 008–100 | 12 | 006–008 |
| 6 | 008–024 | 13 | 006–011 |
| 7 | 008–022 | 14 | 008–011 |

**Навчання іншої плати.** Заповнити `keymap` значеннями `{-1, -1}` і перезібрати: при натисканні на екрані й у лозі з'являється `пади a-b`. Нумерація там з 1, у `keymap.h` з 0.

## Збірка та прошивка

Потрібен nRF Connect SDK **v3.4.2** з модулем **ncs-zigbee v2.0.0**.

```sh
. ~/ncs/toolchain-env.sh
cd ~/ncs/v3.4.2
# перший раз
west build -b promicro_nrf52840/nrf52840/uf2 -d ~/Developer/pet/zb-remote/build ~/Developer/pet/zb-remote
# далі
west build -d ~/Developer/pet/zb-remote/build
```

Прошивка: двічі швидко натиснути RST, щоб з'явився диск `NICENANO`, і скопіювати:

```sh
cp -X build/zb-remote/zephyr/zephyr.uf2 /Volumes/NICENANO/
```

`-X` обов'язковий: без нього macOS намагається скопіювати xattr і `cp` падає.

**Лог** іде через USB CDC ACM (`/dev/cu.usbmodem*`, 115200). Прошивка до 20 с чекає, поки хост відкриє порт (`CONFIG_APP_WAIT_HOST`).

**Скидання мережі:** утримувати кнопку 1 протягом 10 с.

### Чому конфіг саме такий (особливості цього клону)

| Опція | Причина |
|---|---|
| Legacy USB stack (`USB_DEVICE_STACK_NEXT=n`) | з `device_next` CDC не енумерується (перевірено на `experiments/usb-blinky`) |
| `CLOCK_CONTROL_NRF_K32SRC_RC` | на платі немає кварцу 32 кГц, без цього застосунок висне |
| `ZIGBEE_CHANNEL_SELECTION_MODE_MULTI` | координатор не на 16-му каналі (у нас 11) |
| `ZIGBEE_PRO_CORE_BEHAVIOR_R22` | Z-Stack-координатор не приймає R23 join |
| `sense-edge-mask` у overlay | GPIOTE має лише 8 каналів |
| розділи з `0x26000` | там починається застосунок для UF2 bootloader; NVRAM ZBOSS лежить за кодом і переживає перепрошивку |

## Zigbee2MQTT

1. Скопіювати `z2m/zb-remote-14.mjs` у `<z2m data>/external_converters/` і перезапустити Z2M.
2. Увімкнути permit join і скинути мережу на пульті (кнопка 1, 10 с).

### Кластер дисплея `0xFC10` (manufacturer-specific, ендпоінт 1)

| Атрибут | Тип | Ключ Z2M | Зміст |
|---|---|---|---|
| `0x0000`–`0x0002` | octet string ≤ 48 B | `tile1`–`tile3` | `"іконка\|значення"` |
| `0x0003` | uint8 | `wake` | увімкнути екран на N с |
| `0x0004` | octet string | `alert` | `"іконка\|рядок1\|рядок2\|рядок3"`, порожній рядок прибирає сповіщення |
| `0x0005` | uint8 | `flip` | поворот на 180° |
| `0x0006` / `0x0007` | int8 | `offset_x` / `offset_y` | зсув зображення, на 20 с показує калібрувальну рамку |
| `0x0008` | uint8 | `portrait` | вертикальна орієнтація |

Особливості:
- Z2M викликає конвертер **один раз на повідомлення**, тому всі ключі пишуться одним `ep.write`.
- Рядки треба передавати як `OCTET_STR` + `Buffer`: масив байтів типу `CHAR_STR` дає `MALFORMED_COMMAND`.
- Довгий текст обрізається по межі: у плитці влазить 6 символів, у рядку сповіщення 11.

### Приклади з HA

```yaml
# сповіщення
action: mqtt.publish
data:
  topic: zigbee2mqtt/<friendly_name>/set
  payload: '{"alert": "bell|Домофон|Хтось біля|дверей"}'

# плитки
action: mqtt.publish
data:
  topic: zigbee2mqtt/<friendly_name>/set
  payload: >-
    {"tile1": "temp|{{ states('sensor.kitchen_temperature') }}°",
     "tile2": "bulb|{{ 'ON' if is_state('light.kitchen', 'on') else 'OFF' }}"}
```

```yaml
# тригер на кнопку
triggers:
  - trigger: state
    entity_id: event.<remote>_action   # або sensor.<remote>_action
    to: "3_single"
```

### Іконки

![icons](docs/icons.png)

`bell door door_open lock unlock bulb bulb_off temp power battery home moon sun flame snow drop plug fan check cross link search star cam tv button`

## Структура

```
src/
  main.c             Zigbee: ендпоінт, кластери, звіти, сигнали, логіка кнопок → дисплей
  keypad.c           сканування pair-matrix клавіатури
  keymap.h           кнопка → пара ліній
  buttons.c          single / double / hold (600 / 300 мс)
  oled.c             рендер UI (плитки, сповіщення, статус-бар, орієнтація)
  display_cluster.c  кластер 0xFC10 → oled
  battery.c, led.c
  font_terminus12.c  згенеровано tools/genfont.py (Terminus 12 px)
  icons.c            згенеровано tools/icons.py
boards/              overlay для promicro_nrf52840/nrf52840/uf2
z2m/                 external converter для Zigbee2MQTT
tools/
  icons.py           піксель-арт іконки 24×24 → src/icons.c (+ docs/icons.png)
  genfont.py         TTF → 1-бітний пропорційний шрифт (ASCII + українська)
  preview/           рендер src/oled.c на хості → docs/preview.png
experiments/usb-blinky  мінімальний тест USB CDC (legacy vs device_next)
```

Перегенерація ресурсів (потрібен Pillow):

```sh
python3 tools/icons.py src/icons.c docs/icons.png
# Terminus (TTF): https://files.ax86.net/terminus-ttf/
python3 tools/genfont.py TerminusTTF-4.49.3.ttf 12 src/font_terminus12.c
tools/preview/render.sh
```

**Шрифт:** [Terminus (TTF)](https://files.ax86.net/terminus-ttf/) 12 px (6×12), моноширинний піксельний, з українською. Ліцензія SIL OFL 1.1, див. `fonts/Terminus-OFL.txt`. Кирилична «д» у Terminus схожа на латинську «g», тому в `tools/genfont.py` вона перемальована вручну (`OVERRIDES`).

## Статус і плани

- [x] Zigbee join, дії, батарея, дисплей з HA
- [x] Клавіатура з ІЧ-пульта (14 кнопок, 6 ліній)
- [x] OLED 128×64
- [ ] Перевірити 128×64 і keymap на залізі
- [ ] Батарейна збірка: вимкнути USB-лог, виміряти струм сну
- [ ] Розкладка кнопок → дії в HA, автоматизації
- [ ] ІЧ-бластер на світлодіоді старого пульта (опційно)

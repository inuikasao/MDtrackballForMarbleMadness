# MDtrackballForMarbleMadness

USB トラックボールを、日本版メガドライブの「日本版セガマウス」に見せかけて、
**マーブルマッドネスをトラックボールで遊ぶ**ためのアダプターです(Raspberry Pi Pico 2 使用)。

## 重要: 対象範囲

- **日本版メガドライブ + 日本版セガマウスのプロトコル + 日本のテンゲン版マーブルマッドネス専用**として作っています。
- ほかのソフト、本体、環境で使えるかは未確認。
  Genesis(海外版)の Mega Mouse や、EA 版のマーブルマッドネスでは、たぶん使えません。
- あくまで「マーブルマッドネスをトラックボールでやるためのもの」です。
  **USB マウスをセガマウス代わりに使える汎用アダプターではありません。**
- 左右ボタンと移動量だけ対応です。中ボタン、START、ホイールは非対応です。

## 必要なもの

- Raspberry Pi Pico 2
- OTG アダプタ(micro-B → USB-A メス)
- USB トラックボール(HID ブートマウスとして動くもの)
- メガドライブのコントローラ端子につなぐ D-sub9 ケーブル
- 抵抗 1kΩ × 2

## 配線

この通りに配線してください。

| MD DE-9 | 信号 | Pico 2 | 備考 |
|---|---|---|---|
| 1 | D0 | GP2 | 直結 |
| 2 | D1 | GP3 | 直結 |
| 3 | D2 | GP4 | 直結 |
| 4 | D3 | GP5 | 直結 |
| 5 | +5V | VBUS(40番) | 直結 |
| 6 | TL | GP6 | 直結 |
| 7 | TH | GP7 | 1kΩ を直列に入れる |
| 8 | GND | GND | 直結 |
| 9 | TR | GP8 | 1kΩ を直列に入れる |

- トラックボールは、Pico 2 の micro-B 端子に、OTG アダプタ経由でつなぎます。
  電源は MD の 5V から VBUS へ供給し、トラックボールにも、ここから給電されます。
- **PC と MD に、同時につながないでください。** PC につなぐときは、MD から外します。
- TH と TR は、5V の信号を 1kΩ 経由で、3.3V の Pico に入れます。この構成で動作しましたが、
  5V 耐性は保証できません。自己責任でお願いします。
- 大きなトラックボールは、消費電流が大きい場合があります。

## 書き込み

### 普通に使うとき: UF2 で書き込む

1. Pico 2 の **BOOTSEL** ボタンを押しながら、PC に USB 接続します(MD とトラックボールは外します)。
2. `RP2350` というドライブが出るので、`md_mouse.uf2` をコピーします。
3. 自動で再起動したら、終わりです。

### 感度などを変えたいとき: `.ino` から書き込む

1. Arduino IDE に、**arduino-pico**(Earle Philhower のコア)と、**Adafruit TinyUSB Library** を入れます。
2. 次の 6 ファイルを、`md_mouse/` フォルダに置きます。
   `md_mouse.ino` / `md_mouse.pio` / `md_mouse.pio.h` / `md_frame.h` / `hid_mouse.h` / `md_slot.h`
3. ツールの設定:
   - ボード: **Raspberry Pi Pico 2**
   - USB Stack: **Adafruit TinyUSB Host (native)**
4. `md_mouse.ino` の先頭にある設定を直して、書き込みます(BOOTSEL を押しながら接続)。

| 設定 | 初期値 | 内容 |
|---|---|---|
| `DRIVE_MODE` | `2` | 起動直後から信号を出す。**変えないでください**(マーブルマッドネスは起動時にマウスを確認します) |
| `INVERT_X`, `INVERT_Y` | `0` | 上下・左右が逆なら `1` |
| `SWAP_BUTTONS` | `0` | 左右ボタンが逆なら `1` |
| `SPEED_NUM`, `SPEED_DEN` | `1`, `1` | 感度。例: 半分なら `1` と `2`、2倍なら `2` と `1` |
| `DIAG_LED` | `0` | 診断用の LED 表示。通常は `0` |
| `DEBUG_UART` | `0` | シリアル出力。つないでいなければ `0` |

UF2 は、Arduino IDE の「スケッチ → コンパイルしたバイナリを出力」で、自分で作ることもできます。

`md_mouse.pio` を直したときは、`md_mouse.pio.h` を `pioasm` で作り直す必要があります。

```
<arduino15>/packages/rp2040/tools/pqt-pioasm/<ver>/pioasm -o c-sdk md_mouse.pio md_mouse.pio.h
```

## 使い方

1. MD の電源を切った状態で、アダプターを**コントローラポート1**に挿します。
2. トラックボールを、OTG アダプタ経由で、Pico 2 に接続します。
3. MD の電源を入れ、マーブルマッドネスを起動します。
   マウスが認識されなければ、MD をリセットしてください。

## 技術メモ

- 日本版セガマウスの10ニブルのプロトコルを、PIO で実装しています。
- **TH=1(待機中)でも、TL を TR に追従させています。** マーブルマッドネスは、起動時に
  TH=1 のまま TR=0 にして、TL=0 が返ってくるかを確認します。これがないと、起動時に固まります。
- Y 軸は、日本版に合わせて反転しています。

## 免責

自作の趣味のプロジェクトです。動作の保証はありません。接続や改造は、自己責任でお願いします。


# MD Trackball Adapter for Marble Madness

An adapter that lets a USB trackball pose as a **Japanese Sega Mouse** on a Japanese Mega Drive, so you can **play Marble Madness with a trackball** (built on a Raspberry Pi Pico 2).

## Important: scope

- This is built **only for the Japanese Mega Drive + the Japanese Sega Mouse protocol + the Japanese Tengen release of Marble Madness**.
- I don't know whether it works with other games, consoles, or setups (untested).
  It probably won't work with the Genesis (overseas) Mega Mouse or the EA release of Marble Madness.
- It exists purely to **play Marble Madness with a trackball**.
  **It is not a general-purpose adapter for using a USB mouse in place of a Sega Mouse.**
- Only the left/right buttons and movement are supported. The middle button, START, and the wheel are not.

## What you need

- Raspberry Pi Pico 2
- OTG adapter (micro-B to USB-A female)
- A USB trackball (one that works as an HID boot mouse)
- A DE-9 cable to plug into the Mega Drive controller port
- 1 kΩ resistors x 2

## Wiring

Wire it exactly like this.

| MD DE-9 | Signal | Pico 2 | Notes |
|---|---|---|---|
| 1 | D0 | GP2 | Direct |
| 2 | D1 | GP3 | Direct |
| 3 | D2 | GP4 | Direct |
| 4 | D3 | GP5 | Direct |
| 5 | +5V | VBUS (pin 40) | Direct |
| 6 | TL | GP6 | Direct |
| 7 | TH | GP7 | Put a 1 kΩ resistor in series |
| 8 | GND | GND | Direct |
| 9 | TR | GP8 | Put a 1 kΩ resistor in series |

- Connect the trackball to the Pico 2's micro-B port through the OTG adapter.
  Power comes from the MD's 5V into VBUS, and it also powers the trackball.
- **Never connect the Pico to a PC and the MD at the same time.** Unplug it from the MD when connecting to a PC.
- TH and TR feed 5V signals into the 3.3V Pico through 1 kΩ. This works in my setup,
  but 5V tolerance is not guaranteed. Use at your own risk.
- Large trackballs can draw a lot of current.

## Flashing

### Normal use: flash the UF2

1. Hold the **BOOTSEL** button on the Pico 2 while connecting it to a PC (disconnect it from the MD and the trackball first).
2. A drive named `RP2350` appears. Copy `md_mouse.uf2` onto it.
3. It reboots by itself. That's it.

### To change sensitivity etc.: flash from the `.ino`

1. Install **arduino-pico** (Earle Philhower's core) and the **Adafruit TinyUSB Library** in the Arduino IDE.
2. Put these 6 files in a folder named `md_mouse/`:
   `md_mouse.ino` / `md_mouse.pio` / `md_mouse.pio.h` / `md_frame.h` / `hid_mouse.h` / `md_slot.h`
3. Tools settings:
   - Board: **Raspberry Pi Pico 2**
   - USB Stack: **Adafruit TinyUSB Host (native)**
4. Edit the settings at the top of `md_mouse.ino`, then flash (hold BOOTSEL while connecting).

| Setting | Default | Meaning |
|---|---|---|
| `DRIVE_MODE` | `2` | Drive the signals from power-on. **Do not change this** (Marble Madness checks for the mouse right at boot) |
| `INVERT_X`, `INVERT_Y` | `0` | Set to `1` if left/right or up/down is reversed |
| `SWAP_BUTTONS` | `0` | Set to `1` if the left/right buttons are swapped |
| `SPEED_NUM`, `SPEED_DEN` | `1`, `1` | Sensitivity. e.g. half speed: `1` and `2`; double speed: `2` and `1` |
| `DIAG_LED` | `0` | Diagnostic LED display. Leave at `0` for normal use |
| `DEBUG_UART` | `0` | Serial debug output. Leave at `0` if nothing is connected |

You can also build the UF2 yourself with "Sketch > Export Compiled Binary" in the Arduino IDE.

If you edit `md_mouse.pio`, you must regenerate `md_mouse.pio.h` with `pioasm`:

```
<arduino15>/packages/rp2040/tools/pqt-pioasm/<ver>/pioasm -o c-sdk md_mouse.pio md_mouse.pio.h
```

## Usage

1. With the MD powered off, plug the adapter into **controller port 1**.
2. Connect the trackball to the Pico 2 through the OTG adapter.
3. Power on the MD and start Marble Madness.
   If the mouse isn't recognized, reset the MD.

## Technical notes

- The Japanese Sega Mouse's 10-nibble protocol is implemented in PIO.
- **TL follows TR even while TH=1 (idle).** At boot, Marble Madness drives TR=0 with TH=1
  and checks that TL returns 0. Without this, the game freezes at startup.
- The Y axis is inverted to match the Japanese mouse.

## Disclaimer

This is a hobby project. No warranty. Wiring and modifications are at your own risk.

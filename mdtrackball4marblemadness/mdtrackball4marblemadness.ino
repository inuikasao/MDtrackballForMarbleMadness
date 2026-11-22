/*
  md_mouse.ino
  USBマウス/トラックボール → メガドライブ「日本版セガマウス」変換アダプター (Raspberry Pi Pico 2)

  【Arduino IDE の設定】
    ボードパッケージ : arduino-pico (6.3.0 推奨)
    ボード           : Raspberry Pi Pico 2
    USB Stack        : Adafruit TinyUSB Host (native)    ← Tools メニュー。必須
    CPU Speed        : 150MHz のまま
    ※ USBはホスト専用になるため Serial(USB) は使えません。デバッグは Serial1 (GP0=TX, GP1=RX)。

  【ファイル構成】(同じフォルダに置く)
    md_mouse.ino     このファイル。設定・USBホスト・PIOへのデータ受け渡し
    md_mouse.pio     PIOプログラムのソース (TH=1時TL追従対応版)
    md_mouse.pio.h   ↑をpioasmでアセンブルした結果
    md_frame.h       1フレーム分のデータ生成/復号
    hid_mouse.h      USB HIDレポートの解釈
    md_slot.h        USBマウス1台ごとの状態(型定義)

  【配線】(MDコントローラポート DE-9)
    1 D0 → GP2    2 D1 → GP3    3 D2 → GP4    4 D3 → GP5     (直結)
    6 TL → GP6                                                (直結)
    7 TH → GP7 (1kΩ直列)   9 TR → GP8 (1kΩ直列)               (入力。内部プルなし)
    5 +5V → VBUS(40番)     8 GND → GND
    トラックボールは Pico 2 の micro-B 端子に OTG アダプタ経由で接続。
*/

#include <Adafruit_TinyUSB.h>
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hid_mouse.h"
#include "md_frame.h"
#include "md_slot.h"
#include "md_mouse.pio.h"

#ifndef USE_TINYUSB_HOST
#error "Tools -> USB Stack を 'Adafruit TinyUSB Host (native)' に設定してください"
#endif

// =====================================================================
//  設定 (調整はここだけ)
// =====================================================================
#define PIN_D0 2  // D0..D3 = PIN_D0..+3, TL = +4, TH = +5, TR = +6 (連続ピン必須)

// 感度 = SPEED_NUM / SPEED_DEN (USBのカウント → セガマウスのカウント)
// 動きが速すぎる/遅すぎる場合はここを変える。例: 半分にするなら 1 / 2
#define SPEED_NUM 1
#define SPEED_DEN 1

// 向きの反転。実機で上下/左右が逆なら 0 ↔ 1 を切り替える
#define INVERT_X 0
#define INVERT_Y 0

// 1にすると左右ボタンを入れ替える
#define SWAP_BUTTONS 0

// PIOへ最新データを積み直す間隔 [us]
#define REFRESH_US 500

// 1にするとSerial1(GP0/GP1, 115200bps)に状態を出力する
#define DEBUG_UART 0
// =====================================================================

// PIOとの取り決め (md_mouse.pio と合わせる)
#define FRAME_IRQ 0  // PIO→CPU : フレーム開始
#define LOCK_IRQ 1   // CPU→PIO : FIFO入れ替え中

static PIO g_pio = pio0;
static uint g_sm = 0;
static Adafruit_USBH_Host USBHost;

// 未送信の移動量。符号はセガマウスに送る向きに揃えてある
// (単位: USBカウント × SPEED_NUM。SPEED_DEN で割った値を送る)
static int32_t g_acc_x = 0, g_acc_y = 0;
static uint8_t g_buttons = 0;    // MD_BTN_*
static uint8_t g_btn_latch = 0;  // 未送信の押下ラッチ(短押し取りこぼし対策)

// 統計 (デバッグ用)
static uint32_t g_frames = 0, g_reports = 0;

// ---------------------------------------------------------------------
//  PIO
// ---------------------------------------------------------------------
static uint32_t current_word() {
  const int x = md_clamp127(g_acc_x / SPEED_DEN);
  const int y = md_clamp127(g_acc_y / SPEED_DEN);
  return md_make_word(x, y, g_buttons | g_btn_latch);
}

static void md_pio_init() {
  g_sm = pio_claim_unused_sm(g_pio, true);
  const uint offset = pio_add_program(g_pio, &md_mouse_program);

  pio_sm_config c = md_mouse_program_get_default_config(offset);
  sm_config_set_out_pins(&c, PIN_D0, 4);        // D0-D3 (out / mov pins)
  sm_config_set_set_pins(&c, PIN_D0, 4);        // D0-D3 (set pins)
  sm_config_set_sideset_pins(&c, PIN_D0 + 4);   // TL
  sm_config_set_in_pins(&c, PIN_D0 + 5);        // TH, TR
  sm_config_set_jmp_pin(&c, PIN_D0 + 5);        // TH
  sm_config_set_in_shift(&c, false, false, 32);   // 左シフト
  sm_config_set_out_shift(&c, true, false, 32);   // 右シフト
  sm_config_set_clkdiv(&c, 1.0f);

  for (uint p = PIN_D0; p <= PIN_D0 + 6; p++) pio_gpio_init(g_pio, p);
  gpio_disable_pulls(PIN_D0 + 5);  // TH, TR は5V信号。内部プルは使わない
  gpio_disable_pulls(PIN_D0 + 6);

  pio_sm_init(g_pio, g_sm, offset, &c);
  // ピン値を D=0000, TL=1 にしてから出力にする (グリッチ防止)
  pio_sm_set_pins_with_mask(g_pio, g_sm, 1u << (PIN_D0 + 4), 0x1Fu << PIN_D0);
  pio_sm_set_consecutive_pindirs(g_pio, g_sm, PIN_D0, 5, true);       // D0-D3, TL = 出力
  pio_sm_set_consecutive_pindirs(g_pio, g_sm, PIN_D0 + 5, 2, false);  // TH, TR = 入力

  pio_sm_put(g_pio, g_sm, md_make_word(0, 0, 0));  // 最初のフレーム用
  pio_sm_set_enabled(g_pio, g_sm, true);
}

// PIOが次のフレームで使う1ワードを、最新のものに入れ替える
static void publish() {
  const uint32_t w = current_word();
  noInterrupts();
  g_pio->irq_force = 1u << LOCK_IRQ;           // PIOにワード取得を待たせる
  if (pio_interrupt_get(g_pio, FRAME_IRQ)) {   // もうフレーム処理が始まっている
    pio_interrupt_clear(g_pio, LOCK_IRQ);      // ロックを外して取得を終わらせる
    interrupts();
    return;                                    // → handle_frame() が後始末する
  }
  pio_sm_clear_fifos(g_pio, g_sm);
  pio_sm_put(g_pio, g_sm, w);                  // FIFOは空なので待たない
  pio_interrupt_clear(g_pio, LOCK_IRQ);
  interrupts();
}

// MDが1フレーム読み始めた: 実際に送った分を未送信量から引く
static void handle_frame() {
  const uint32_t t0 = micros();
  while (pio_sm_is_rx_fifo_empty(g_pio, g_sm)) {
    if ((uint32_t)(micros() - t0) > 200) break;  // エコーが来なければ諦める
  }
  if (!pio_sm_is_rx_fifo_empty(g_pio, g_sm)) {
    int x, y;
    const uint32_t sent = pio_sm_get(g_pio, g_sm);
    md_decode_word(sent, &x, &y);
    g_btn_latch &= ~((sent >> 12) & 0xF);  // 実際にMDへ渡したボタンのラッチを解除
    g_acc_x -= (int32_t)x * SPEED_DEN;
    g_acc_y -= (int32_t)y * SPEED_DEN;
    g_frames++;
  }
  pio_interrupt_clear(g_pio, FRAME_IRQ);
  publish();
}

static void service_pio() {
  static uint32_t last_us = 0;
  if (pio_interrupt_get(g_pio, FRAME_IRQ)) {
    handle_frame();
    last_us = micros();
    return;
  }
  const uint32_t now = micros();
  if ((uint32_t)(now - last_us) >= REFRESH_US) {
    publish();
    last_us = now;
  }
}

// ---------------------------------------------------------------------
//  USB HID (マウス)
// ---------------------------------------------------------------------
// (mouse_slot_t は md_slot.h。Arduinoの自動プロトタイプ生成対策で別ファイル化)
static mouse_slot_t g_slots[MD_MAX_DEV][CFG_TUH_HID];

#define slot(dev_addr, idx) \
  (((dev_addr) >= MD_MAX_DEV || (idx) >= CFG_TUH_HID) ? nullptr : &g_slots[(dev_addr)][(idx)])

// マウスの動き(USBの向き: 右+, 下+)とボタン(HID bit0=左, bit1=右)を受け取る
static void on_mouse(int dx, int dy, uint8_t hid_buttons) {
#if INVERT_X
  dx = -dx;
#endif
  // セガマウスのYは「上が正」なのでUSB(下が正)と逆。標準で反転して送る
#if !INVERT_Y
  dy = -dy;
#endif
  g_acc_x += (int32_t)dx * SPEED_NUM;
  g_acc_y += (int32_t)dy * SPEED_NUM;

  uint8_t b = 0;
#if SWAP_BUTTONS
  if (hid_buttons & 0x01) b |= MD_BTN_R;
  if (hid_buttons & 0x02) b |= MD_BTN_L;
#else
  if (hid_buttons & 0x01) b |= MD_BTN_L;
  if (hid_buttons & 0x02) b |= MD_BTN_R;
#endif
  g_buttons = b;
  g_btn_latch |= b;
  g_reports++;
}

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t idx, uint8_t const* desc, uint16_t len) {
  mouse_slot_t* s = slot(dev_addr, idx);
  if (!s) return;
  memset(s, 0, sizeof(*s));

  if (tuh_hid_interface_protocol(dev_addr, idx) == HID_ITF_PROTOCOL_MOUSE &&
      tuh_hid_get_protocol(dev_addr, idx) == HID_PROTOCOL_BOOT) {
    s->active = true;
    s->boot = true;
  } else if (hid_mouse_parse(desc, len, &s->layout)) {
    s->active = true;
  }
#if DEBUG_UART
  Serial1.printf("HID mount dev=%u idx=%u mouse=%d boot=%d\r\n", dev_addr, idx, s->active, s->boot);
#endif
  if (s->active) tuh_hid_receive_report(dev_addr, idx);
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t idx) {
  mouse_slot_t* s = slot(dev_addr, idx);
  const bool was_active = (s && s->active);
  if (s) memset(s, 0, sizeof(*s));
  if (was_active) {
    g_buttons = 0;
    g_btn_latch = 0;
  }
#if DEBUG_UART
  Serial1.printf("HID umount dev=%u idx=%u\r\n", dev_addr, idx);
#endif
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t idx, uint8_t const* report,
                                uint16_t len) {
  mouse_slot_t* s = slot(dev_addr, idx);
  if (s && s->active) {
    int dx = 0, dy = 0;
    uint8_t btn = 0;
    const bool ok = s->boot ? hid_boot_mouse_decode(report, len, &dx, &dy, &btn)
                            : hid_mouse_decode(&s->layout, report, len, &dx, &dy, &btn);
    if (ok) on_mouse(dx, dy, btn);
  }
  tuh_hid_receive_report(dev_addr, idx);  // 次のレポートを要求
}

// ---------------------------------------------------------------------
//  setup / loop
// ---------------------------------------------------------------------
void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  md_pio_init();  // MDの起動直後のマウス検出に間に合わせるため最優先
#if DEBUG_UART
  Serial1.begin(115200);
  Serial1.println("md_mouse: start");
#endif
  USBHost.begin(0);  // ネイティブUSBをホストとして開始
}

// LED: 点滅=MDからの読み出し待ち / 点灯=MDが読みに来ている(フレーム受信あり)
static void led_status() {
  if (g_frames) digitalWrite(LED_BUILTIN, HIGH);
  else digitalWrite(LED_BUILTIN, (millis() / 500) & 1);
}

void loop() {
  led_status();
  USBHost.task();
  service_pio();
#if DEBUG_UART
  static uint32_t last_ms = 0;
  if (millis() - last_ms >= 1000) {
    last_ms = millis();
    Serial1.printf("frames=%lu reports=%lu acc=(%ld,%ld) btn=%u\r\n", (unsigned long)g_frames,
                   (unsigned long)g_reports, (long)g_acc_x, (long)g_acc_y, g_buttons);
  }
#endif
}
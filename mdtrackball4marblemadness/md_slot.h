// USB HIDマウス1台(インターフェース)ごとの状態。
// .ino内に置くとArduino IDEの自動プロトタイプ生成が型定義より前に
// 関数宣言を挿入してエラーになるため、ヘッダに分離している。
#pragma once
#include <Adafruit_TinyUSB.h>
#include "hid_mouse.h"

#define MD_MAX_DEV (CFG_TUH_DEVICE_MAX + 1)  // dev_addr は 1 から

struct mouse_slot_t {
  bool active;
  bool boot;                  // ブートプロトコル(固定フォーマット)
  hid_mouse_layout_t layout;  // ブートでない場合のレポート構造
};

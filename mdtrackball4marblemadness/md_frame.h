// md_frame.h : セガマウス1フレーム分のワード生成/復号 (ハード非依存・PCでもテスト可)
//
// ワード = PIOが1フレームで順に出す8ニブル。nibble k は bit[4k+3:4k]
//   k=0,1 : F, F          (固定)
//   k=2   : フラグ  bit0=X負, bit1=Y負, bit2=Xオーバー, bit3=Yオーバー
//   k=3   : ボタン  bit0=L,   bit1=R,   bit2=M,         bit3=START
//   k=4,5 : X移動量 上位, 下位   (符号付き8bit)
//   k=6,7 : Y移動量 上位, 下位   (符号付き8bit)
#pragma once
#include <stdint.h>

#define MD_BTN_L 0x1
#define MD_BTN_R 0x2

// ±127 に制限 (-128 は使わない)
static inline int md_clamp127(int32_t v) {
  return v > 127 ? 127 : (v < -127 ? -127 : (int)v);
}

// x, y : MDへ送る符号付き値 (±127)。buttons : MD_BTN_*
static inline uint32_t md_make_word(int x, int y, uint8_t buttons) {
  const uint8_t bx = (uint8_t)x, by = (uint8_t)y;
  const uint32_t flags = (x < 0 ? 1u : 0u) | (y < 0 ? 2u : 0u);  // オーバーフローは常に0
  return 0xFu | (0xFu << 4) | (flags << 8) | ((uint32_t)(buttons & 0xFu) << 12) |
         ((uint32_t)(bx >> 4) << 16) | ((uint32_t)(bx & 0xFu) << 20) |
         ((uint32_t)(by >> 4) << 24) | ((uint32_t)(by & 0xFu) << 28);
}

// エコーバックされたワードから、実際に送った移動量を取り出す
static inline void md_decode_word(uint32_t w, int* x, int* y) {
  const uint8_t bx = (uint8_t)((((w >> 16) & 0xFu) << 4) | ((w >> 20) & 0xFu));
  const uint8_t by = (uint8_t)((((w >> 24) & 0xFu) << 4) | ((w >> 28) & 0xFu));
  *x = (int8_t)bx;
  *y = (int8_t)by;
}

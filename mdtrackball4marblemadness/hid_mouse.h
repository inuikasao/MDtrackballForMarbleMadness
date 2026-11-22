// hid_mouse.h : USB HIDマウスのレポート解釈 (ハード非依存・PCでもテスト可)
//  - ブートプロトコルのマウス : buttons(1byte), x(int8), y(int8)
//  - それ以外 : レポートディスクリプタを簡易解析して X / Y / ボタンの位置を求める
//    (レポートID付き、12/16bit のX/Y に対応。Push/Pop と絶対座標は非対応)
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

typedef struct {
  bool valid;               // X と Y が見つかった
  uint8_t report_id;        // 0 = レポートIDなし
  uint16_t x_off, y_off;    // ビット位置 (レポートIDの次のバイトから数える)
  uint8_t x_size, y_size;   // ビット幅
  bool x_signed, y_signed;
  bool has_btn;
  uint16_t btn_off;         // ボタン1のビット位置
  uint8_t btn_count;        // ボタン数 (最大8)
} hid_mouse_layout_t;

// ---- ブートプロトコル ----
static inline bool hid_boot_mouse_decode(const uint8_t* r, uint16_t len, int* dx, int* dy,
                                         uint8_t* buttons) {
  if (len < 3) return false;
  *buttons = r[0] & 0x07;
  *dx = (int8_t)r[1];
  *dy = (int8_t)r[2];
  return true;
}

// ---- ビット取り出し (リトルエンディアン) ----
static inline int32_t hid_get_bits(const uint8_t* d, uint16_t len, uint32_t off, uint8_t size,
                                   bool is_signed) {
  if (size == 0 || size > 32 || (off + size) > (uint32_t)len * 8u) return 0;
  uint32_t v = 0;
  for (uint8_t i = 0; i < size; i++) {
    const uint32_t bit = off + i;
    v |= (uint32_t)((d[bit >> 3] >> (bit & 7)) & 1u) << i;
  }
  if (is_signed && size < 32 && (v & (1u << (size - 1)))) v |= ~0u << size;
  return (int32_t)v;
}

// ---- ディスクリプタ解析 ----
static inline bool hid_mouse_parse(const uint8_t* d, uint16_t len, hid_mouse_layout_t* out) {
  memset(out, 0, sizeof(*out));

  uint16_t usage_page = 0;
  int32_t lmin = 0;
  uint32_t rsize = 0, rcount = 0;
  uint32_t uval[16];
  bool uext[16];
  uint8_t nu = 0;
  uint32_t umin = 0, umax = 0;
  bool umin_ext = false, has_range = false;

  uint8_t ids[8] = {0};     // レポートIDごとのビットオフセット
  uint16_t offs[8] = {0};
  uint8_t nids = 1, cur = 0;
  bool got_x = false, got_y = false;
  uint8_t btn_rid = 0;

  uint16_t i = 0;
  while (i < len) {
    const uint8_t b = d[i++];
    if (b == 0xFE) {  // long item : 読み飛ばす
      if (i >= len) break;
      i += (uint16_t)(1 + d[i] + 1);
      continue;
    }
    uint8_t size = b & 3;
    if (size == 3) size = 4;
    const uint8_t type = (b >> 2) & 3;
    const uint8_t tag = b >> 4;
    if (i + size > len) break;
    uint32_t u = 0;
    for (uint8_t k = 0; k < size; k++) u |= (uint32_t)d[i + k] << (8 * k);
    int32_t s = (size == 1) ? (int8_t)u : (size == 2) ? (int16_t)u : (int32_t)u;
    i += size;

    if (type == 1) {  // Global
      switch (tag) {
        case 0: usage_page = (uint16_t)u; break;
        case 1: lmin = s; break;
        case 7: rsize = u; break;
        case 8: {  // Report ID
          uint8_t n = 0;
          while (n < nids && ids[n] != (uint8_t)u) n++;
          if (n == nids) {
            if (nids < 8) { ids[nids] = (uint8_t)u; offs[nids] = 0; nids++; }
            else n = nids - 1;
          }
          cur = n;
          break;
        }
        case 9: rcount = u; break;
        default: break;
      }
    } else if (type == 2) {  // Local
      switch (tag) {
        case 0:
          if (nu < 16) { uval[nu] = u; uext[nu] = (size == 4); nu++; }
          break;
        case 1: umin = u; umin_ext = (size == 4); has_range = true; break;
        case 2: umax = u; break;
        default: break;
      }
    } else if (type == 0) {  // Main
      if (tag == 8) {        // Input
        if (!(u & 1)) {      // Constant(パディング)でない
          for (uint32_t n = 0; n < rcount; n++) {
            uint32_t page = usage_page, usage = 0;
            if (nu) {
              const uint8_t k = (n < nu) ? (uint8_t)n : (uint8_t)(nu - 1);
              if (uext[k]) { page = uval[k] >> 16; usage = uval[k] & 0xFFFF; }
              else usage = uval[k];
            } else if (has_range) {
              uint32_t m = umin + n;
              if (m > umax) m = umax;
              if (umin_ext) { page = m >> 16; usage = m & 0xFFFF; }
              else usage = m;
            }
            const uint16_t bitoff = (uint16_t)(offs[cur] + n * rsize);
            const uint8_t rid = ids[cur];
            const bool same = !(got_x || got_y) || rid == out->report_id;
            if (!same) continue;
            if (page == 0x01 && usage == 0x30 && !got_x) {
              got_x = true; out->report_id = rid;
              out->x_off = bitoff; out->x_size = (uint8_t)rsize; out->x_signed = (lmin < 0);
            } else if (page == 0x01 && usage == 0x31 && !got_y) {
              got_y = true; out->report_id = rid;
              out->y_off = bitoff; out->y_size = (uint8_t)rsize; out->y_signed = (lmin < 0);
            } else if (page == 0x09 && usage >= 1 && usage <= 8 && rsize == 1) {
              if (usage == 1) { out->has_btn = true; out->btn_off = bitoff; out->btn_count = 0; btn_rid = rid; }
              if (out->has_btn && usage == (uint32_t)out->btn_count + 1) out->btn_count++;
            }
          }
        }
        offs[cur] = (uint16_t)(offs[cur] + rsize * rcount);
      }
      // Main item の後は Local をクリア
      nu = 0; has_range = false; umin = umax = 0; umin_ext = false;
    }
  }
  if (out->has_btn && btn_rid != out->report_id) out->has_btn = false;  // 別レポートのボタンは使わない
  out->valid = got_x && got_y;
  return out->valid;
}

static inline bool hid_mouse_decode(const hid_mouse_layout_t* L, const uint8_t* r, uint16_t len,
                                    int* dx, int* dy, uint8_t* buttons) {
  if (!L->valid) return false;
  const uint8_t* p = r;
  uint16_t n = len;
  if (L->report_id) {
    if (len < 1 || r[0] != L->report_id) return false;
    p = r + 1;
    n = (uint16_t)(len - 1);
  }
  *dx = (int)hid_get_bits(p, n, L->x_off, L->x_size, L->x_signed);
  *dy = (int)hid_get_bits(p, n, L->y_off, L->y_size, L->y_signed);
  uint8_t b = 0;
  if (L->has_btn)
    for (uint8_t k = 0; k < L->btn_count && k < 8; k++)
      b |= (uint8_t)(hid_get_bits(p, n, L->btn_off + k, 1, false) << k);
  *buttons = b;
  return true;
}

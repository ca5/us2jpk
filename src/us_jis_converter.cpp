#include "us_jis_converter.h"
#include <string.h>

UsJisConverter::UsJisConverter()
    : physical_modifiers_(0)
{
    init();
}

void UsJisConverter::init() {
    physical_modifiers_ = 0;
    memset(active_keys_, 0, sizeof(active_keys_));
    buildTables();
}

void UsJisConverter::buildTables() {
    // デフォルト: すべて等価パススルー (0x00〜0xFF)
    for (int i = 0; i < 256; i++) {
        map_unshifted_[i].out_usage = (uint8_t)i;
        map_unshifted_[i].shift_mode = SHIFT_PASS_THROUGH;

        map_shifted_[i].out_usage = (uint8_t)i;
        map_shifted_[i].shift_mode = SHIFT_PASS_THROUGH;
    }

    // --- 数字段のShift記号 ---
    // 2: Shiftで '@' -> JISでは '@' は 0x2F (Shiftなし)
    map_shifted_[0x1F].out_usage = 0x2F;
    map_shifted_[0x1F].shift_mode = FORCE_SHIFT_OFF;

    // 6: Shiftで '^' -> JISでは '^' は 0x2E (Shiftなし)
    map_shifted_[0x23].out_usage = 0x2E;
    map_shifted_[0x23].shift_mode = FORCE_SHIFT_OFF;

    // 7: Shiftで '&' -> JISでは '&' は 0x23 (6キーのShift)
    map_shifted_[0x24].out_usage = 0x23;
    map_shifted_[0x24].shift_mode = FORCE_SHIFT_ON;

    // 8: Shiftで '*' -> JISでは '*' は 0x34 (':'キーのShift)
    map_shifted_[0x25].out_usage = 0x34;
    map_shifted_[0x25].shift_mode = FORCE_SHIFT_ON;

    // 9: Shiftで '(' -> JISでは '(' は 0x25 (8キーのShift)
    map_shifted_[0x26].out_usage = 0x25;
    map_shifted_[0x26].shift_mode = FORCE_SHIFT_ON;

    // 0: Shiftで ')' -> JISでは ')' は 0x26 (9キーのShift)
    map_shifted_[0x27].out_usage = 0x26;
    map_shifted_[0x27].shift_mode = FORCE_SHIFT_ON;

    // --- 記号キー ---
    // '-' / '_' (0x2D):
    // 通常は '-' (0x2D, JISでもそのまま)
    // Shiftで '_' -> JISでは 0x87 (ろキーのShift)
    map_shifted_[0x2D].out_usage = 0x87;
    map_shifted_[0x2D].shift_mode = FORCE_SHIFT_ON;

    // '=' / '+' (0x2E):
    // 通常 '=' -> JISでは 0x2D ('-'キーのShift)
    map_unshifted_[0x2E].out_usage = 0x2D;
    map_unshifted_[0x2E].shift_mode = FORCE_SHIFT_ON;
    // Shift '+' -> JISでは 0x33 (';'キーのShift)
    map_shifted_[0x2E].out_usage = 0x33;
    map_shifted_[0x2E].shift_mode = FORCE_SHIFT_ON;

    // '[' / '{' (0x2F):
    // 通常 '[' -> JISでは 0x30 ('['キーのShiftなし)
    map_unshifted_[0x2F].out_usage = 0x30;
    map_unshifted_[0x2F].shift_mode = FORCE_SHIFT_OFF;
    // Shift '{' -> JISでは 0x30 ('['キーのShift)
    map_shifted_[0x2F].out_usage = 0x30;
    map_shifted_[0x2F].shift_mode = FORCE_SHIFT_ON;

    // ']' / '}' (0x30):
    // 通常 ']' -> JISでは 0x32 (Non-US #のShiftなし)
    map_unshifted_[0x30].out_usage = 0x32;
    map_unshifted_[0x30].shift_mode = FORCE_SHIFT_OFF;
    // Shift '}' -> JISでは 0x32 (Non-US #のShift)
    map_shifted_[0x30].out_usage = 0x32;
    map_shifted_[0x30].shift_mode = FORCE_SHIFT_ON;

    // '\' / '|' (0x31):
    // 通常 '\' -> JISでは 0x89 (INT3/YenキーのShiftなし)
    map_unshifted_[0x31].out_usage = 0x89;
    map_unshifted_[0x31].shift_mode = FORCE_SHIFT_OFF;
    // Shift '|' -> JISでは 0x89 (INT3/YenキーのShift)
    map_shifted_[0x31].out_usage = 0x89;
    map_shifted_[0x31].shift_mode = FORCE_SHIFT_ON;

    // ';' / ':' (0x33):
    // 通常 ';' -> JISでもそのまま 0x33
    // Shift ':' -> JISでは 0x34 (':'キーのShiftなし)
    map_shifted_[0x33].out_usage = 0x34;
    map_shifted_[0x33].shift_mode = FORCE_SHIFT_OFF;

    // '\'' / '"' (0x34):
    // 通常 '\'' -> JISでは 0x24 ('7'キーのShift)
    map_unshifted_[0x34].out_usage = 0x24;
    map_unshifted_[0x34].shift_mode = FORCE_SHIFT_ON;
    // Shift '"' -> JISでは 0x1F ('2'キーのShift)
    map_shifted_[0x34].out_usage = 0x1F;
    map_shifted_[0x34].shift_mode = FORCE_SHIFT_ON;

    // '`' / '~' (0x35):
    // 通常 '`' -> JISでは 0x2F ('@'キーのShift)
    map_unshifted_[0x35].out_usage = 0x2F;
    map_unshifted_[0x35].shift_mode = FORCE_SHIFT_ON;
    // Shift '~' -> JISでは 0x2E ('^'キーのShift)
    map_shifted_[0x35].out_usage = 0x2E;
    map_shifted_[0x35].shift_mode = FORCE_SHIFT_ON;
}

KeyMapEntry UsJisConverter::lookup(uint8_t in_keycode, bool is_shift, bool is_alt) const {
    // 特例: Alt + ` は日本語IME全角/半角切り替えとしてそのまま 0x35 を透過
    if (in_keycode == 0x35 && is_alt) {
        KeyMapEntry entry;
        entry.out_usage = 0x35;
        entry.shift_mode = SHIFT_PASS_THROUGH;
        return entry;
    }

    if (is_shift) {
        return map_shifted_[in_keycode];
    } else {
        return map_unshifted_[in_keycode];
    }
}

void UsJisConverter::onPhysicalKeyPress(uint8_t keycode, uint8_t physical_modifiers) {
    physical_modifiers_ = physical_modifiers;

    // 既にアクティブなら何もしない (リピート対策)
    for (int i = 0; i < 6; i++) {
        if (active_keys_[i].in_keycode == keycode) {
            return;
        }
    }

    bool is_shift = (physical_modifiers_ & (0x02 | 0x20)) != 0;
    bool is_alt   = (physical_modifiers_ & (0x04 | 0x40)) != 0;

    KeyMapEntry entry = lookup(keycode, is_shift, is_alt);

    // 空きスロットを探して登録
    for (int i = 0; i < 6; i++) {
        if (active_keys_[i].in_keycode == 0) {
            active_keys_[i].in_keycode = keycode;
            active_keys_[i].out_usage = entry.out_usage;
            active_keys_[i].shift_mode = entry.shift_mode;
            break;
        }
    }
}

void UsJisConverter::onPhysicalKeyRelease(uint8_t keycode) {
    // 対象キーのスロットをクリア
    for (int i = 0; i < 6; i++) {
        if (active_keys_[i].in_keycode == keycode) {
            active_keys_[i].in_keycode = 0;
            active_keys_[i].out_usage = 0;
            active_keys_[i].shift_mode = SHIFT_PASS_THROUGH;
            break;
        }
    }
}

void UsJisConverter::setPhysicalModifiers(uint8_t modifiers) {
    physical_modifiers_ = modifiers;
}

uint8_t UsJisConverter::getOutputModifiers() const {
    uint8_t send_mod = physical_modifiers_;

    // アクティブなキーのShift要求を反映
    // 直近または現在押されているキーで FORCE_SHIFT_OFF があれば Shift を落とす
    // FORCE_SHIFT_ON があれば Left Shift を付加する
    for (int i = 0; i < 6; i++) {
        if (active_keys_[i].in_keycode != 0) {
            if (active_keys_[i].shift_mode == FORCE_SHIFT_OFF) {
                send_mod &= ~(0x02 | 0x20); // Shift解除
            } else if (active_keys_[i].shift_mode == FORCE_SHIFT_ON) {
                send_mod |= 0x02; // Left Shift付加
            }
        }
    }

    return send_mod;
}

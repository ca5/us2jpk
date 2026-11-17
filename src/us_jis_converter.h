#pragma once

#include <stdint.h>
#include <stdbool.h>

// Shift修飾の制御モード
enum OutputShiftMode : uint8_t {
    SHIFT_PASS_THROUGH = 0, // 物理Shift状態をそのまま維持
    FORCE_SHIFT_OFF    = 1, // 出力時はShiftを強制的に解除
    FORCE_SHIFT_ON     = 2, // 出力時はShiftを強制的に付加
};

// 変換マップのエントリ
struct KeyMapEntry {
    uint8_t out_usage;          // 送信先HID Usage ID (JIS配列PC向け)
    OutputShiftMode shift_mode; // 送信時のShift修飾指定
};

// 押下中のキー情報
struct ActiveKey {
    uint8_t in_keycode;         // US物理キーコード
    uint8_t out_usage;          // JIS送信Usage ID
    OutputShiftMode shift_mode; // Shiftモード
};

class UsJisConverter {
public:
    UsJisConverter();

    // 初期化
    void init();

    // 物理キー押下イベント
    void onPhysicalKeyPress(uint8_t keycode, uint8_t physical_modifiers);

    // 物理キー解放イベント
    void onPhysicalKeyRelease(uint8_t keycode);

    // 物理修飾キー更新
    void setPhysicalModifiers(uint8_t modifiers);
    uint8_t getPhysicalModifiers() const { return physical_modifiers_; }

    // 現在送信すべき修飾キー状態を取得
    uint8_t getOutputModifiers() const;

    // 現在送信すべき6キーを取得 (active_keys)
    const ActiveKey* getActiveKeys() const { return active_keys_; }

    // 変換テーブル参照
    KeyMapEntry lookup(uint8_t in_keycode, bool is_shift, bool is_alt) const;

private:
    KeyMapEntry map_unshifted_[256];
    KeyMapEntry map_shifted_[256];

    uint8_t physical_modifiers_;
    ActiveKey active_keys_[6];

    void buildTables();
};

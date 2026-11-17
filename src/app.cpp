#include <Arduino.h>
#include "USBHost_t36.h"
#include "us_jis_converter.h"

// Teensy 3.6 における KEYBOARD_INTERFACE 定義漏れ対策
#ifndef KEYBOARD_INTERFACE
#define KEYBOARD_INTERFACE 2
#endif

// USB Host オブジェクト定義
USBHost myusb;
USBHub hub1(myusb);
USBHub hub2(myusb);
KeyboardController keyboard1(myusb);
USBHIDParser hid1(myusb);
USBHIDParser hid2(myusb);
USBHIDParser hid3(myusb);

// US-JIS 変換器
static UsJisConverter converter;

static uint8_t physical_modifiers = 0;
static uint8_t keyboard_last_leds = 0;

// USB 出力レポートの同期送信
static void update_usb_output() {
#ifdef KEYBOARD_INTERFACE
    uint8_t out_mod = converter.getOutputModifiers();
    const ActiveKey *keys = converter.getActiveKeys();

    Keyboard.set_modifier(out_mod);
    Keyboard.set_key1(keys[0].out_usage);
    Keyboard.set_key2(keys[1].out_usage);
    Keyboard.set_key3(keys[2].out_usage);
    Keyboard.set_key4(keys[3].out_usage);
    Keyboard.set_key5(keys[4].out_usage);
    Keyboard.set_key6(keys[5].out_usage);
    Keyboard.send_now();
#endif
}

// Raw キー押下ハンドラ
void OnRawPress(uint8_t keycode) {
    if (keycode >= 103 && keycode < 111) {
        // 修飾キー (Left Ctrl .. Right GUI)
        uint8_t bit = 1 << (keycode - 103);
        physical_modifiers |= bit;
        converter.setPhysicalModifiers(physical_modifiers);
        update_usb_output();
    } else {
        // 通常キー
        converter.onPhysicalKeyPress(keycode, physical_modifiers);
        update_usb_output();
    }

#if defined(DEBUG)
    Serial.printf("Press raw: 0x%02X, PhysMod: 0x%02X, OutMod: 0x%02X\n",
                  keycode, physical_modifiers, converter.getOutputModifiers());
#endif
}

// Raw キー解放ハンドラ
void OnRawRelease(uint8_t keycode) {
    if (keycode >= 103 && keycode < 111) {
        // 修飾キー (Left Ctrl .. Right GUI)
        uint8_t bit = 1 << (keycode - 103);
        physical_modifiers &= ~bit;
        converter.setPhysicalModifiers(physical_modifiers);
        update_usb_output();
    } else {
        // 通常キー
        converter.onPhysicalKeyRelease(keycode);
        update_usb_output();
    }

#if defined(DEBUG)
    Serial.printf("Release raw: 0x%02X, PhysMod: 0x%02X, OutMod: 0x%02X\n",
                  keycode, physical_modifiers, converter.getOutputModifiers());
#endif
}

// メディアキー・特殊キー押下ハンドラ
void OnHIDExtrasPress(uint32_t top, uint16_t key) {
#ifdef KEYBOARD_INTERFACE
#ifdef KEYMEDIA_INTERFACE
    if (top == 0xc0000) {
        Keyboard.press(0xE400 | key);
    }
#endif
#endif
}

// メディアキー・特殊キー解放ハンドラ
void OnHIDExtrasRelease(uint32_t top, uint16_t key) {
#ifdef KEYBOARD_INTERFACE
#ifdef KEYMEDIA_INTERFACE
    if (top == 0xc0000) {
        Keyboard.release(0xE400 | key);
    }
#endif
#endif
}

void setup() {
#if defined(DEBUG)
    Serial.begin(115200);
#endif

    // オンボードLED (Pin 13) を動作インジケータとして点灯
    pinMode(13, OUTPUT);
    digitalWrite(13, HIGH);

    converter.init();

    // USB Host 開始
    myusb.begin();

    // コールバック登録
    keyboard1.attachRawPress(OnRawPress);
    keyboard1.attachRawRelease(OnRawRelease);
    keyboard1.attachExtrasPress(OnHIDExtrasPress);
    keyboard1.attachExtrasRelease(OnHIDExtrasRelease);
}

void loop() {
    // USB ホストタスク実行 (パケット処理)
    myusb.Task();

#ifdef KEYBOARD_INTERFACE
    // PCホスト側からのLED状態（CapsLock, NumLock等）をキーボードへ反映
    if (keyboard_leds != keyboard_last_leds) {
        keyboard_last_leds = keyboard_leds;
        keyboard1.LEDS(keyboard_leds);
    }
#endif
}

#include <Arduino.h>
#include "USBHost_t36.h"
#include "us_jis_converter.h"

// Teensy 3.6 における KEYBOARD_INTERFACE / MOUSE_INTERFACE 定義漏れ対策
#ifndef KEYBOARD_INTERFACE
#define KEYBOARD_INTERFACE 2
#endif
#ifndef MOUSE_INTERFACE
#define MOUSE_INTERFACE 3
#endif

// USB Host オブジェクト定義 (ハブ・複数デバイス対応)
USBHost myusb;
USBHub hub1(myusb);
USBHub hub2(myusb);
USBHub hub3(myusb);

KeyboardController keyboard1(myusb);
KeyboardController keyboard2(myusb);

MouseController mouse1(myusb);
MouseController mouse2(myusb);

USBHIDParser hid1(myusb);
USBHIDParser hid2(myusb);
USBHIDParser hid3(myusb);
USBHIDParser hid4(myusb);
USBHIDParser hid5(myusb);
USBHIDParser hid6(myusb);

// US-JIS 変換器
static UsJisConverter converter;

static uint8_t physical_modifiers = 0;
static uint8_t keyboard_last_leds = 0;
static uint8_t mouse_buttons_prev = 0;
static bool kbd_was_connected = false;

// KVM 自動リカバリ用タイマー
static uint32_t last_recovery_attempt = 0;
static uint32_t last_device_seen_time = 0;
static uint32_t last_led_blink = 0;
static bool led_state = true;

// USB 出力レポートの同期送信 (キーボード)
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

// マウス入力の完全パススルー処理
static void process_mouse(MouseController &mouse) {
#if defined(MOUSE_INTERFACE)
    if (mouse.available()) {
        int mx = mouse.getMouseX();
        int my = mouse.getMouseY();
        int wheel = mouse.getWheel();
        int wheelH = mouse.getWheelH();

        if (mx != 0 || my != 0 || wheel != 0 || wheelH != 0) {
            Mouse.move(mx, my, wheel, wheelH);
        }

        uint8_t btns = mouse.getButtons();
        if (btns != mouse_buttons_prev) {
            // Mouse.set_buttons(left, middle, right, back, forward)
            // btns: bit 0 = Left, bit 1 = Right, bit 2 = Middle, bit 3 = Back, bit 4 = Forward
            Mouse.set_buttons(btns & 1, btns & 4, btns & 2, btns & 8, btns & 16);
            mouse_buttons_prev = btns;
        }

        mouse.mouseDataClear();
    }
#endif
}

void setup() {
#if defined(DEBUG)
    Serial.begin(115200);
#endif

    pinMode(13, OUTPUT);
    digitalWrite(13, HIGH);

    converter.init();

#if defined(MOUSE_INTERFACE)
    Mouse.begin();
#endif

    // USB Host 開始
    myusb.begin();

    // キーボード コールバック登録 (keyboard1, keyboard2)
    keyboard1.attachRawPress(OnRawPress);
    keyboard1.attachRawRelease(OnRawRelease);
    keyboard1.attachExtrasPress(OnHIDExtrasPress);
    keyboard1.attachExtrasRelease(OnHIDExtrasRelease);

    keyboard2.attachRawPress(OnRawPress);
    keyboard2.attachRawRelease(OnRawRelease);
    keyboard2.attachExtrasPress(OnHIDExtrasPress);
    keyboard2.attachExtrasRelease(OnHIDExtrasRelease);

    last_device_seen_time = millis();
}

void loop() {
    uint32_t now = millis();

    // USB ホストタスク実行 (パケット処理)
    myusb.Task();

    // マウス入力処理 (mouse1, mouse2)
    process_mouse(mouse1);
    process_mouse(mouse2);

    // デバイス認識状況の確認
    bool kbd_now = (bool)keyboard1 || (bool)keyboard2;
    bool mouse_now = ((USBHIDInput&)mouse1) || ((USBHIDInput&)mouse2);
    bool any_device_online = kbd_now || mouse_now;

    if (any_device_online) {
        last_device_seen_time = now;
        digitalWriteFast(13, HIGH); // 認識中はLED常時点灯
    } else {
        // 未認識・切替中はLEDを点滅
        if (now - last_led_blink >= 250) {
            last_led_blink = now;
            led_state = !led_state;
            digitalWriteFast(13, led_state);
        }
    }

    // 切替器切り替え時のキーボード切断検知 (スタック防止)
    if (kbd_now) {
        kbd_was_connected = true;
    } else if (kbd_was_connected) {
        kbd_was_connected = false;
        converter.init();
        physical_modifiers = 0;
        update_usb_output();
    }

    // --- KVM / 切替器 切り替え復帰の自動リカバリ ---
    // 切替器で物理的に接続されている(CCS=1)のに、デバイスが未認識の状態が続いた場合、
    // 自動的にポートリセットを発行して抜き差しと同じ再列挙を実行する
    if (!any_device_online && (now - last_device_seen_time >= 800)) {
        if (now - last_recovery_attempt >= 1500) {
            last_recovery_attempt = now;
            uint32_t portstat = USBHS_PORTSC1;
            // bit 0: CCS (Current Connect Status)
            if (portstat & USBHS_PORTSC_CCS) {
                // ポートリセット (PR) を発行して自動再接続を開始
                USBHS_PORTSC1 |= USBHS_PORTSC_PR;
            }
        }
    }

#ifdef KEYBOARD_INTERFACE
    // PCホスト側からのLED状態（CapsLock, NumLock等）をキーボードへ反映
    if (keyboard_leds != keyboard_last_leds) {
        keyboard_last_leds = keyboard_leds;
        if (keyboard1) keyboard1.LEDS(keyboard_leds);
        if (keyboard2) keyboard2.LEDS(keyboard_leds);
    }
#endif
}

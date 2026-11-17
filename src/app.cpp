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
static uint32_t last_device_seen_time = 0;
static uint32_t last_led_blink = 0;
static bool led_state = true;

extern volatile uint32_t g_dbg_stage;

// USB 出力レポートの同期送信 (キーボード)
static void update_usb_output() {
#ifdef KEYBOARD_INTERFACE
    g_dbg_stage = 250;
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
    g_dbg_stage = 251;
#endif
}

static volatile bool hotkey_reset_requested = false;

// Raw キー押下ハンドラ
void OnRawPress(uint8_t keycode) {
    uint32_t now = millis();

    // 復帰用ホットキー: Right Shiftを素早く3回連続で押すと切替器ポートを強制リセット
    // ※ ISR内から直接リセットを実行するとデッドロックするため、フラグを立ててloop()側で安全に実行
    static uint32_t last_shift_time = 0;
    static uint8_t shift_tap_count = 0;
    if (keycode == 108) { // Right Shift
        if (now - last_shift_time <= 700) {
            shift_tap_count++;
            if (shift_tap_count >= 3) {
                shift_tap_count = 0;
                hotkey_reset_requested = true;
                return;
            }
        } else {
            shift_tap_count = 1;
        }
        last_shift_time = now;
    } else {
        shift_tap_count = 0;
    }

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

extern volatile uint32_t g_dbg_stage;
static volatile uint32_t last_loop_heartbeat = 0;
static IntervalTimer stuckTimer;

static void stuck_monitor_isr() {
    if (millis() - last_loop_heartbeat >= 2000) {
        Serial.printf("[STUCK] stage=%lu PORTSC=0x%08X USBSTS=0x%08X\n",
                      g_dbg_stage, USBHS_PORTSC1, USBHS_USBSTS);
    }
}

void setup() {
    Serial.begin(115200);

    pinMode(13, OUTPUT);
    digitalWrite(13, HIGH);

    // 復帰用ボタントリガー: Pin 0 (GNDと短絡/押しボタンスイッチで復帰)
    pinMode(0, INPUT_PULLUP);

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
    last_loop_heartbeat = millis();
    stuckTimer.priority(64);
    stuckTimer.begin(stuck_monitor_isr, 1000000); // 1 second
    Serial.println("[BOOT] Teensy 3.6 US-JIS Converter ready.");
}

void loop() {
    uint32_t now = millis();
    last_loop_heartbeat = now;
    g_dbg_stage = 1;

    // USB ホストタスク実行 (パケット処理)
    g_dbg_stage = 2;
    myusb.Task();
    g_dbg_stage = 3;

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

    // 機器の応答不整合検知（自動バスリセット）
    static uint32_t offline_since = 0;
    static uint32_t last_auto_reset = 0;

    if (any_device_online) {
        offline_since = 0;
    } else {
        // 切替器またはケーブルが接続されているが、キーボード/マウスが応答しない状態
        if (USBHS_PORTSC1 & USBHS_PORTSC_CCS) {
            if (offline_since == 0) {
                offline_since = now;
            } else if (now - offline_since >= 1500) {
                // 1.5秒以上機器が無応答の場合、OS相当のUSBバスリセット（5V電源再同期）を自動実行
                if (now - last_auto_reset >= 4000) {
                    last_auto_reset = now;
                    Serial.println("[AUTO-RECOVERY] Device unresponsive on connected port. Executing USB bus reset!");
                    USBHost::reset_host_port();
                }
            }
        } else {
            offline_since = 0;
        }
    }

    // 1秒ごとのリアルタイム診断ログ出力
    static uint32_t last_stat_print = 0;
    if (now - last_stat_print >= 1000) {
        last_stat_print = now;
        uint16_t r_vid = 0, r_pid = 0;
        uint8_t r_est = 0, r_err = 0, r_has = 0;
        USBHost::get_debug_info(r_vid, r_pid, r_est, r_err, r_has);
        Serial.printf("[DEBUG] t=%lu s | KBD=%d | MOUSE=%d | PORTSC=0x%08X | PSTATE=%d | ROOT=%d(0x%04X:0x%04X est=%d err=%d)\n",
                      now / 1000,
                      (int)kbd_now,
                      (int)mouse_now,
                      USBHS_PORTSC1,
                      (int)USBHost::get_port_state(),
                      (int)r_has, r_vid, r_pid, (int)r_est, (int)r_err);
    }

    // 復帰用ホットキートリガー (Right Shift 3回押し)
    if (hotkey_reset_requested) {
        hotkey_reset_requested = false;
        Serial.println("[HOTKEY] Triple-RightShift recovery triggered!");
        USBHost::reset_host_port();
    }

    // 復帰用ボタントリガー (Pin 0 を GND に接続/プッシュスイッチで復帰)
    // 切替器から戻った際に Pin 0（端のピン）をGNDにショートさせるか、
    // 外付けボタンを押すと即座に切替器給電遮断＆再認識を実行します
    static uint32_t last_button_trigger = 0;
    if (digitalRead(0) == LOW) {
        if (now - last_button_trigger >= 1500) {
            last_button_trigger = now;
            Serial.println("[TRIGGER] Pin 0 button recovery triggered!");
            USBHost::reset_host_port();
        }
    }

    // シリアルからのデバッグコマンド受信 ('r' でホストポート・切替器電源リセット)
    if (Serial.available()) {
        char c = Serial.read();
        if (c == 'r' || c == 'R') {
            Serial.println("[CMD] Manual reset_host_port triggered via Serial!");
            USBHost::reset_host_port();
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

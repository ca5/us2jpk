#include <assert.h>
#include <stdio.h>
#include "../src/us_jis_converter.h"

void test_basic_letters() {
    UsJisConverter conv;
    // 'a' (0x04) unshifted
    KeyMapEntry e = conv.lookup(0x04, false, false);
    assert(e.out_usage == 0x04);
    assert(e.shift_mode == SHIFT_PASS_THROUGH);

    // 'a' shifted -> 'A'
    e = conv.lookup(0x04, true, false);
    assert(e.out_usage == 0x04);
    assert(e.shift_mode == SHIFT_PASS_THROUGH);
    printf("test_basic_letters PASSED\n");
}

void test_numbers_and_symbols() {
    UsJisConverter conv;

    // '2' unshifted -> '2'
    KeyMapEntry e = conv.lookup(0x1F, false, false);
    assert(e.out_usage == 0x1F);
    assert(e.shift_mode == SHIFT_PASS_THROUGH);

    // '2' shifted ('@') -> JIS '@' (0x2F, SHIFT_OFF)
    e = conv.lookup(0x1F, true, false);
    assert(e.out_usage == 0x2F);
    assert(e.shift_mode == FORCE_SHIFT_OFF);

    // '6' shifted ('^') -> JIS '^' (0x2E, SHIFT_OFF)
    e = conv.lookup(0x23, true, false);
    assert(e.out_usage == 0x2E);
    assert(e.shift_mode == FORCE_SHIFT_OFF);

    // '7' shifted ('&') -> JIS '&' (0x23, SHIFT_ON)
    e = conv.lookup(0x24, true, false);
    assert(e.out_usage == 0x23);
    assert(e.shift_mode == FORCE_SHIFT_ON);

    // '8' shifted ('*') -> JIS '*' (0x34, SHIFT_ON)
    e = conv.lookup(0x25, true, false);
    assert(e.out_usage == 0x34);
    assert(e.shift_mode == FORCE_SHIFT_ON);

    // '9' shifted ('(') -> JIS '(' (0x25, SHIFT_ON)
    e = conv.lookup(0x26, true, false);
    assert(e.out_usage == 0x25);
    assert(e.shift_mode == FORCE_SHIFT_ON);

    // '0' shifted (')') -> JIS ')' (0x26, SHIFT_ON)
    e = conv.lookup(0x27, true, false);
    assert(e.out_usage == 0x26);
    assert(e.shift_mode == FORCE_SHIFT_ON);

    printf("test_numbers_and_symbols PASSED\n");
}

void test_bracket_and_symbols() {
    UsJisConverter conv;

    // '-' unshifted -> 0x2D
    KeyMapEntry e = conv.lookup(0x2D, false, false);
    assert(e.out_usage == 0x2D);
    assert(e.shift_mode == SHIFT_PASS_THROUGH);

    // '-' shifted ('_') -> JIS 0x87 (ろ), SHIFT_ON
    e = conv.lookup(0x2D, true, false);
    assert(e.out_usage == 0x87);
    assert(e.shift_mode == FORCE_SHIFT_ON);

    // '=' unshifted ('=') -> JIS 0x2D ('-'), SHIFT_ON
    e = conv.lookup(0x2E, false, false);
    assert(e.out_usage == 0x2D);
    assert(e.shift_mode == FORCE_SHIFT_ON);

    // '=' shifted ('+') -> JIS 0x33 (';'), SHIFT_ON
    e = conv.lookup(0x2E, true, false);
    assert(e.out_usage == 0x33);
    assert(e.shift_mode == FORCE_SHIFT_ON);

    // '[' unshifted ('[') -> JIS 0x30, SHIFT_OFF
    e = conv.lookup(0x2F, false, false);
    assert(e.out_usage == 0x30);
    assert(e.shift_mode == FORCE_SHIFT_OFF);

    // '[' shifted ('{') -> JIS 0x30, SHIFT_ON
    e = conv.lookup(0x2F, true, false);
    assert(e.out_usage == 0x30);
    assert(e.shift_mode == FORCE_SHIFT_ON);

    // ']' unshifted (']') -> JIS 0x32, SHIFT_OFF
    e = conv.lookup(0x30, false, false);
    assert(e.out_usage == 0x32);
    assert(e.shift_mode == FORCE_SHIFT_OFF);

    // ']' shifted ('}') -> JIS 0x32, SHIFT_ON
    e = conv.lookup(0x30, true, false);
    assert(e.out_usage == 0x32);
    assert(e.shift_mode == FORCE_SHIFT_ON);

    // '\' unshifted ('\') -> JIS 0x89 (Yen), SHIFT_OFF
    e = conv.lookup(0x31, false, false);
    assert(e.out_usage == 0x89);
    assert(e.shift_mode == FORCE_SHIFT_OFF);

    // '\' shifted ('|') -> JIS 0x89 (Yen), SHIFT_ON
    e = conv.lookup(0x31, true, false);
    assert(e.out_usage == 0x89);
    assert(e.shift_mode == FORCE_SHIFT_ON);

    // ';' shifted (':') -> JIS 0x34, SHIFT_OFF
    e = conv.lookup(0x33, true, false);
    assert(e.out_usage == 0x34);
    assert(e.shift_mode == FORCE_SHIFT_OFF);

    // '\'' unshifted ('\'') -> JIS 0x24 ('7'), SHIFT_ON
    e = conv.lookup(0x34, false, false);
    assert(e.out_usage == 0x24);
    assert(e.shift_mode == FORCE_SHIFT_ON);

    // '\'' shifted ('"') -> JIS 0x1F ('2'), SHIFT_ON
    e = conv.lookup(0x34, true, false);
    assert(e.out_usage == 0x1F);
    assert(e.shift_mode == FORCE_SHIFT_ON);

    // '`' unshifted ('`') -> JIS 0x2F ('@'), SHIFT_ON
    e = conv.lookup(0x35, false, false);
    assert(e.out_usage == 0x2F);
    assert(e.shift_mode == FORCE_SHIFT_ON);

    // '`' shifted ('~') -> JIS 0x2E ('^'), SHIFT_ON
    e = conv.lookup(0x35, true, false);
    assert(e.out_usage == 0x2E);
    assert(e.shift_mode == FORCE_SHIFT_ON);

    // '`' with Alt -> Hankaku/Zenkaku (0x35, PASS_THROUGH)
    e = conv.lookup(0x35, false, true);
    assert(e.out_usage == 0x35);
    assert(e.shift_mode == SHIFT_PASS_THROUGH);

    printf("test_bracket_and_symbols PASSED\n");
}

void test_key_press_release_rollover() {
    UsJisConverter conv;

    // Shift(0x02) を押す
    conv.setPhysicalModifiers(0x02);
    assert(conv.getOutputModifiers() == 0x02);

    // Shift押しながら '2' (0x1F, '@') を押す
    conv.onPhysicalKeyPress(0x1F, 0x02);
    // 出力時は JIS '@' (0x2F) で Shift は OFF になるべき！
    assert(conv.getActiveKeys()[0].out_usage == 0x2F);
    assert(conv.getOutputModifiers() == 0x00); // Shift解除！

    // '2' を離す
    conv.onPhysicalKeyRelease(0x1F);
    assert(conv.getActiveKeys()[0].in_keycode == 0);
    // Shift状態に戻るべき
    assert(conv.getOutputModifiers() == 0x02);

    // Shiftなしの状態で '=' (0x2E) を押す
    conv.setPhysicalModifiers(0x00);
    conv.onPhysicalKeyPress(0x2E, 0x00);
    // 出力時は JIS 0x2D ('-') で Shift は ON になるべき！
    assert(conv.getActiveKeys()[0].out_usage == 0x2D);
    assert((conv.getOutputModifiers() & 0x02) != 0); // Shift ON！

    conv.onPhysicalKeyRelease(0x2E);
    assert(conv.getOutputModifiers() == 0x00);

    printf("test_key_press_release_rollover PASSED\n");
}

int main() {
    test_basic_letters();
    test_numbers_and_symbols();
    test_bracket_and_symbols();
    test_key_press_release_rollover();
    printf("All converter tests passed successfully!\n");
    return 0;
}

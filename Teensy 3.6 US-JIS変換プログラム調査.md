# **Teensy 3.6向けUSキーボード入力・JIS配列出力変換プログラムの調査と実装指針**

公開リポジトリ、開発者フォーラム、技術記事を調査した範囲では、Teensy 3.6に書き込むだけで「US配列キーボード入力→JIS配列キーボード出力」をそのまま完結できる完成済みの専用プログラムは確認できなかった。Teensy 3.6はMicro-USB側（USB Device）とUSB Host用の5ピンヘッダ側の2つの独立したUSBポートを同時に駆動できるハードウェア構成を備えているものの1、公開されているTeensy 3.6向けUSBホストプログラムは入力をそのままPCへ中継するパススルーサンプルやプロキシ／キーロガー実装にとどまっている4。  
そのため、Teensy 3.6で目的の変換器を実現する最も現実的な方法は、PJRC公式ライブラリ USBHost\_t36 に同梱されている転送サンプルスケッチ KeyboardForeward.ino（および生スキャンコード転送の参考実装である Winston-Lu/Teensy-3.6-Keylogger）を土台とし、受信したUS配列のキーコードとShift状態を変換テーブルで読み替えてUSB Device側へ送出する処理を追加する構成となる4。ただし、Teensy 3.6では KeyboardForeward が標準のままではPC側へキー入力を出力しない不具合が報告されており、フォーラムで提案されている \#define KEYBOARD\_INTERFACE 回避策の検証を要する点に注意が必要である4。また、ゼロから配列変換プログラムを実装する手間を避けたい場合は、RP2040搭載の「Keyboard Quantizer」や市販変換アダプター「かえうち」の利用が実用上の代替案となる9。

## **Teensy 3.6対応プログラムおよび配列変換ファームウェアの比較**

Teensy 3.6で動作する主要なUSBホスト関連プログラムと、USB-to-USB配列変換に用いられる代表的な他プラットフォーム製ファームウェア／製品の比較は以下の通りである。

| プログラム / プロジェクト名 | Teensy 3.6対応 | USB入力→USB出力の基盤 | US→JIS変換機能 | 現状と適用上の評価 |
| :---- | :---- | :---- | :---- | :---- |
| USBHost\_t36（KeyboardForeward.ino）4 | 対応（既知不具合の回避策が必要）4 | あり（パススルー実装）4 | なし（自前実装が必要） | Teensyduino標準添付のサンプル。T3.xコアの定義漏れを回避できれば最有力ベースとなる4。 |
| Winston-Lu/Teensy-3.6-Keylogger5 | 対応（専用設計）5 | あり（修飾キー含む転送）5 | なし（US ANSI 101/104のみ検証）5 | Rawコールバックを用いた修飾キー状態保持転送の実装例として参考になる5。 |
| phorton1/Arduino-libraries-USBHost\_t3615 | 対応（T3.6 / T4.0向け）15 | Rawキー転送の変更を含む15 | なし（記述なし） | 公式 USBHost\_t36 の約6年前のコミットをベースにRawキー転送へ手を加えたフォーク15。 |
| TMK USB-to-USB Keyboard Converter（tmk\_keyboard）7 | 試作報告止まり（完成版未確認）16 | あり（主にAVR等向け）16 | 配列変換・JISキー知見あり7 | GeekhackでTeensy 3.6適用の議論とCapsLock動作の試作言及があるが、完成ファームは確認できない16。 |
| QMK / Keyboard Quantizer（Rev.3 / Rev.4 / mini）11 | 非対応（ATmega32U4 / RP2040用）12 | あり11 | 標準搭載（Func01 等）11 | QMK本体にはTeensy 3.6対応作業の言及があるものの19、本機はTeensy 3.6では動作しない12。 |
| 市販変換アダプター『かえうち』9 | 対象外（専用ハードウェア） | あり9 | 標準対応10 | 日本の環境でUS→JIS変換の実績が多く、開発不要で導入したい場合の選択肢となる10。 |

Teensy 3.6上で自前改造のベースとする場合、第一候補となるのはPJRC公式ライブラリ PaulStoffregen/USBHost\_t36 に含まれる Examples/USBHost\_t36/KeyboardForeward/KeyboardForeward.ino（マウス併用時は MouseKeyboardForward）である4。ファイル名は Forward ではなく Foreward として収録されている点に留意したい4。この公式サンプルに加え、Winston-Lu/Teensy-3.6-Keylogger はTeensy 3.6とArduino IDE環境で動作するプロキシ実装として公開されており、単発の文字送信ではなくキーの押し下げ・解放およびCtrl・Alt・Shift等の修飾キー状態を維持したままホストへ出力するV2コードを含んでいるため、実装構造の参考として有用である5。  
一方で、キーボード変換分野で広く知られるTMK（tmk\_keyboard）は、JIS配列固有キーのUSB HID Usage整理やShift状態を一時解除して別キーを送信する変換手法など豊富な設計知見を持つものの7、Geekhack上のフォーラムスレッドでもTeensy 3.6のUSBホストポートを使ったCapsLock確認の試作言及にとどまり、完成済みのTeensy 3.6用USB-to-USB変換ファームウェアとしては確立されていない16。また、Webツール（RemapおよびKeyboard Quantizer Configurator）上から Func01（JIS OS向けUSキーボードオーバーライド）を即座に有効化できる「Keyboard Quantizer」は、Rev.3がATmega32U4、Rev.4およびminiがRP2040を採用しており、Teensy 3.6向けではない11。

## **USBHost\_t36をベースにした実装上の要点と既知の落とし穴**

USBHost\_t36 の KeyboardForeward や関連実装をベースにTeensy 3.6でプログラムを構築する際には、T3.x系コアおよびライブラリ仕様に起因するいくつかの制約を踏まえておく必要がある。  
第一の要点は、**Teensy 3.6における KEYBOARD\_INTERFACE 未定義問題**である4。PJRC公式フォーラムでの報告によると、Teensy 4.1やMicroModでは KeyboardForeward が変更なしでそのまま動作する一方、Teensy 3.6ではArduino IDE（v2.3.4等）のUSB Type設定で Keyboard や Serial \+ Keyboard \+ Mouse \+ Joystick などを選択しても、USBデバイス側のキー出力がPCに届かない事象が確認されている4。原因はT3.x系コア（usb\_desc.h 等）において選択したUSB Typeに応じた KEYBOARD\_INTERFACE マクロが適切に定義されず、スケッチ側の条件分岐がシリアルモニタ出力専用の \#define SHOW\_KEYBOARD\_DATA 側へ分岐したり、USBキーボード出力機能が有効化されなかったりすることにあると指摘されている4。フォーラムでは解決策として、スケッチ冒頭に \#define KEYBOARD\_INTERFACE を直接記述する、あるいは USB\_KEYBOARDONLY・USB\_HID・USB\_SERIAL\_HID・USB\_EVERYTHING のいずれかが定義されている場合に KEYBOARD\_INTERFACE を定義するプリプロセッサ分岐を補う手法が提案されているが、提案者自身が網羅性の無保証を付記しており、スレッド上でTeensy 3.6実機での最終的な動作完了報告までは確認できていない4。加えて、Teensy 4.1の環境ながら開発用PC以外に接続すると KeyboardForeward が動作しないという報告もあるため22、実装後は実際に接続するターゲットPC上での認識検証が不可欠である。  
第二の要点は、**attachPress / attachRelease ではなく attachRawPress / attachRawRelease を用いること**である5。USBHost\_t36 の keyboard.cpp の実装を確認すると、標準の key\_press() は内部の convert\_to\_unicode() によってキー入力をUnicode文字へ変換し、key\_release() はNum Lock・Caps Lock・Scroll Lockのトグル処理を行う構造になっている8。実際、Teensy-3.6-Keylogger でもV1の通常コールバック方式ではCtrlキーなどの修飾キー併用時に反応しなかったため、V2でRawコールバック方式へ移行した経緯が記されている5。process\_boot\_keyboard\_format() から呼び出される rawKeyPressedFunction と rawKeyReleasedFunction を使用すれば、8バイトのブートレポート（Byte 2〜7の最大6キー）に含まれる生のHIDスキャンコードを取得できるほか、修飾キーを表すByte 0のビット0〜7の変化も 103 \+ i（i は0〜7のビット位置）のコードとして取得できる8。なお、Teensy-3.6-Keylogger の作者は当時の USBHost\_t36/keyboard.cpp（256, 265, 275, 283行目）におけるコールバック登録判定が機能せずコメントアウトで対処した旨や、Scroll Lock／Caps Lock／Num LockのLED挙動異常、さらに使用したArduino側ソフトウェアの制約により左右Ctrlキーが同一キーとして扱われた点を制限事項として報告している5。そのため、現行版の USBHost\_t36 でRawコールバックが正常に発火するか、また左右修飾キーをどこまで独立制御できるかを実機で確認しながら実装を進める必要がある。  
第三の要点は、**ブートプロトコルとNキーロールオーバー（NKRO）、USBハブ、LED制御の仕様**である8。KeyboardController は原則としてUSBブートプロトコル（SET\_PROTOCOL BOOT）による8バイト固定長レポートで最大6キー同時押し（6KRO）の差分を処理し、一部の特定ベンダー／製品ID（0x04D9 系や 0x046D:0xC547 のLogicool Unifyingレシーバ等）についてはHIDモード／強制ブートモードのテーブルで挙動を制御している8。この設計の影響により、NKRO対応のメカニカルキーボードを接続した際に7キー目以降の同時押しが認識されない事例が報告されており、認識しない場合はサンプルの HIDDeviceinfo スケッチや USBHost\_t36.h（63行目付近）のデバッグ出力を有効化してディスクリプタや生パケットを確認する手順が推奨されている24。また、USBハブを経由する場合や内部にハブを持つキーボードを扱う場合は、スケッチ内で USBHost と KeyboardController だけでなく USBHub インスタンスを宣言する必要がある25。キーボードLEDについては numLock()・capsLock()・scrollLock()・LEDS() でUSB SET\_REPORT を送出できるが8、Apple Magic Keyboard A1843のCaps LED対応イシューなど機種固有の課題も存在する23。

## **US入力からJIS出力へ変換するロジックの設計要件**

RawコールバックからUS配列のHID Usage IDを取得した後に組み込む変換ロジックは、単純なキーコードの1対1置換では正しく機能しない。OS側のキーボード設定がJIS配列である場合、キーボードから送信された同一のHID Usage IDであってもOSはJIS配列の定義に基づいて文字を決定する（例えば、US配列の \[ に相当する LEFT\_BRACKET のHIDコード 0x2F を送信すると、JIS環境のOSでは @ が入力される）27。さらに重要な点として、数字キー上段（Usage ID 0x1E〜0x27）や各種記号キーは、無変換（Shiftなし）時とShift押下時の文字ペアがUS配列とJIS配列で大きく食い違っている20。市販キーボード内蔵の単純なキー入れ替え機能（NiZキーボード等）ではShiftやCtrlとの組み合わせごとに別キーへ割り当てることができないため、変換プログラム側で修飾状態を含めた制御を行う必要がある20。  
このShift状態の不一致を解決するには、USキーボード側の物理的な左Shift・右Shiftの押下状態を変換プログラム内の変数で常時追跡し、US側の「入力キー＋物理Shift有無」の組に対してJIS側の「出力すべきUsage ID＋出力Shift有無」を割り当てる設計が求められる7。具体的には、US側でShiftを押しながら入力した文字がJIS配列ではShiftなしの位置にある場合（例としてJIS環境で 0x2F を打つことで得られる @27）、送信時のみ出力側のShift修飾ビットを一時的に解除して該当Usage IDを送信し、その後に物理Shift状態を復元する必要がある（TMKがドイツ語配列向け解説で示している CANCEL\_SHIFT\_ADD\_RALT のような一時的なShiftキャンセルと復元ロジックや7、別の自作JIS変換器で採用されている「出力関数 SendSc に一元化して出力Shift状態を指定する」設計が参考になる29）。逆に、US側ではShiftなしで入力できる文字がJIS側ではShift併用を要する場合には、送信時のみ一時的にShiftを付与して出力する処理が必要となる7。  
日本語配列特有のキーコードおよび接続先OSによる挙動差にも対処が必要である。US配列の左上にある \` / \~（0x35）はUSB HID仕様上でJIS配列の 全角/半角 キーと同一のUsage IDであるため7、そのまま通過させると記号入力ではなくIMEの切り替えが発生してしまう。IME切り替えを別キー操作（例：Altとの組み合わせや、F14/F15 あるいは 無変換/変換 等への割り当て）へ逃がすか7、\` / \~ 入力用にJIS配列の該当コードへ変換するかを設計上決めておく必要がある。また、US配列がバックスラッシュ／パイプに用いる 0x31 は、JISキーボード（少なくともHHKB Professional JP等）では実装されず 0x32（Non-US \# / \~）が使われ、JIS特有の \\ / \_ キーには 0x87（Keyboard International1 / JP\_RO）が割り当てられるなど、物理キーとUsage IDの対応が異なる18。なお、macOSはサードパーティ製のUSBキーボードをデフォルトでANSI（US）配列として認識するほか、Windows/Linux向けのJIS固有キーである 無変換（0x8B）・変換（0x8A）・ひらがな（0x88）を標準では認識しない（TMKの知見ではmacOS向けに MACE［英数］と MACJ［かな］を用い、手動でKeyboard Setup Assistant設定を行う必要がある）7。さらに、他マイコン（Pro Micro等）向けArduino IDE環境では Keyboard.cpp 側の USAGE\_MAXIMUM の制限によって 0x65 より大きいUsage ID（¥・\|・\_ 等）が無効化され、USAGE\_MAXIMUM の変更（記事内では 0x65 または 0xE7 への修正と言及）を要した事例が報告されているため20、TeensyduinoのUSB出力側でも 0x87 や 0x88・0x8A・0x8B 等の拡張コードがPCへ正しく通るか事前にテストすることが望ましい。

## **Teensy 3.6のハードウェア仕様とUSBホスト電源の注意点**

Teensy 3.6はすでにメーカー販売終了（EOL）となっているボードであるため1、USBホスト回路の配線や電源供給における電気的定格を厳守して破損を防ぐ必要がある。主要なハードウェア仕様は以下の通りである。

| 項目 | 仕様・定格値 | 配線・設計上の注意点 |
| :---- | :---- | :---- |
| 搭載MCU・基本仕様 | MK66FX1M0VMD18（Cortex-M4, 180 MHz, Flash 1 MB, RAM 256 KB, EEPROM 4 KB）1 | 480 Mbps USB HSホストポートと12 Mbps USB FSデバイスポートを独立搭載1。 |
| USB Host 5ピンヘッダ | 5ピン構成（5 V電源、D+、D-、基板上で共通接続の4・5番ピンGND）3 | PJRC純正またはSparkFun CAB-28389（20 cm / 8インチ）等の5ピン→USB-Aメス変換ケーブルを接続3。 |
| USB Host VBUS保護IC | Texas Instruments TPD3S014（電流制限スイッチ＋逆流防止）36 | データシート準拠の実制限は 0.67–1.01 A（室温標準 0.85 A）、過電流応答 2 µs36。 |
| VIN入力上限 | VIN最大 6 V39 | レギュレータだけでなく TPD3S014 にVINが直結しており、同IC定格が6 V上限のため **6 V超は厳禁**39。 |
| I/Oピン耐圧・出力電流 | 0–3.3 V（**5 Vトレラント非対応**）、GPIO推奨最大 10 mA1 | Teensy 3.5と異なり5 V入力不可1。3.3 V外部供給は推奨最大 250 mA40。 |

USBホスト運用における最大の注意点は、突入電流と給電能力である。PJRC公式仕様によれば、Teensy 3.6のUSB Hostポートには電流制限スイッチ（TPD3S014）と大容量コンデンサが搭載されており、USB機器のホットプラグ時に生じる突入電流によるTeensy本体の電源低下を緩和する設計になっている37。しかし、TPD3S014 の標準的な電流制限値は 0.85 A 付近（製品ばらつきを含め 0.67–1.01 A）であるため36、消費電力の大きいイルミネーション付きキーボードや複数のUSB機器を接続する場合は、電力負荷を切り離すためにセルフパワー型のUSBハブを間に接続することが推奨される41。また、外部電源をVINに接続する場合も、USBホスト保護ICの破損を防ぐため必ず5 V系の安定した電源を使い 6 V を超えないように管理する必要がある39。

## **実装手順と代替手段の選択基準**

手元のTeensy 3.6を使用してUS→JIS変換プログラムを立ち上げる場合は、Arduino IDEとTeensyduinoを導入した環境で以下の手順に沿って段階的に検証と実装を進めるのが確実である1。

> 1. Teensy 3.6の5ピンUSB Hostヘッダに市販の5ピン→USB-Aメス変換ケーブルを接続し、変換元となるUS配列キーボードを繋ぐ（電力消費が大きいキーボードや複数機器を接続する場合はセルフパワーUSBハブを併用する）3。  
> 2. Arduino IDEで Examples → USBHost\_t36 → KeyboardForeward を開き、ToolsメニューのUSB Typeで Keyboard（または Serial \+ Keyboard \+ Mouse \+ Joystick）を選択した上で、T3.xコアでの既知の不具合に対するフォーラム提案の回避策としてスケッチ冒頭に \#define KEYBOARD\_INTERFACE を追記してTeensy 3.6へ書き込み、開発機およびターゲットPCの両方でパススルー入力ができるか確認する4。  
> 3. パススルー動作を確認したのち、コールバックを attachRawPress / attachRawRelease ベースへ置き換え、修飾キー（103 \+ i）の押下状態管理とUS→JIS変換テーブル（Shiftキャンセル／付与制御、0x1E–0x27・0x2F・0x35・0x31/0x32/0x87 等を含む）を組み込み、拡張Usage IDが接続先OS上で期待通り入力されるかを検証する7。

「Teensy 3.6上で動かすこと」自体が必須要件ではなく、手早く実用的なUS→JISハードウェア変換器を手に入れることが目的であれば、別のプラットフォームを選択する方が開発コストを抑えられる。例えば、RP2040マイコンを搭載した「Keyboard Quantizer（Rev.4 または mini）」であれば、Webブラウザ上の設定ツールから Func01（JIS OS向けUSキーボードオーバーライド）や6KRO固定パーサをコーディングなしで即座に構成でき、OSごとの自動切替機能も利用できる11。また、日本の環境で多数の動作実績がある市販品「かえうち」を導入すれば、ファームウェア開発自体を行わずにUSキーボードをJIS環境で利用できる9。一方で、既存所有のTeensy 3.6を活用することや独自のマクロ・特殊変換ロジックをC++で自由に作り込むことを優先する場合は、USBHost\_t36 の KeyboardForeward.ino と \#define KEYBOARD\_INTERFACE 回避策を出発点に、Winston-Lu/Teensy-3.6-Keylogger のRaw転送構造とTMK／自作変換器のShift制御手法を組み合わせて実装するアプローチが最も確実である4。

#### **引用文献**

> 1. Teensy 3.6--販売終了 \- スイッチサイエンス, [https\://www\.switch-science.com/products/2989](https://www.switch-science.com/products/2989)  
> 2. Simultaneous USB Host and Device mode \- Teensy Forum, [https\://forum.pjrc.com/index.php?threads/simultaneous-usb-host-and-device-mode.56715/](https://forum.pjrc.com/index.php?threads/simultaneous-usb-host-and-device-mode.56715/)  
> 3. USB Host Cable For Teensy 4.1 \| For Sale at SparkFun, [https\://www\.sparkfun.com/usb-host-cable-for-teensy-4-1-and-teensy-3-6.html](https://www.sparkfun.com/usb-host-cable-for-teensy-4-1-and-teensy-3-6.html)  
> 4. Mouse/Keyboard Forward Examples From USBHost\_t36 Not, [https\://forum.pjrc.com/index.php?threads/mouse-keyboard-forward-examples-from-usbhost\_t36-not-working-as-expected.76449/](https://forum.pjrc.com/index.php?threads/mouse-keyboard-forward-examples-from-usbhost_t36-not-working-as-expected.76449/)  
> 5. GitHub \- Winston-Lu/Teensy-3.6-Keylogger: A key-logger made on a, [https\://github.com/Winston-Lu/Teensy-3.6-Keylogger](https://github.com/Winston-Lu/Teensy-3.6-Keylogger)  
> 6. The USB Host feature is not working on my Teensy 4.1 board., [https\://forum.pjrc.com/index.php?threads/the-usb-host-feature-is-not-working-on-my-teensy-4-1-board.70114/](https://forum.pjrc.com/index.php?threads/the-usb-host-feature-is-not-working-on-my-teensy-4-1-board.70114/)  
> 7. FAQ Keymap · tmk/tmk\_keyboard Wiki \- GitHub, [https\://github.com/tmk/tmk\_keyboard/wiki/FAQ-Keymap](https://github.com/tmk/tmk_keyboard/wiki/FAQ-Keymap)  
> 8. USBHost\_t36/keyboard.cpp at master \- GitHub, [https\://github.com/PaulStoffregen/USBHost\_t36/blob/master/keyboard.cpp](https://github.com/PaulStoffregen/USBHost_t36/blob/master/keyboard.cpp)  
> 9. かえうち2, [https\://kaeuchi.jp/summary/](https://kaeuchi.jp/summary/)  
> 10. キーボード配列変換器『かえうち』の自分カスタマイズ, [https\://eng-blog.iij.ad.jp/archives/21896](https://eng-blog.iij.ad.jp/archives/21896)  
> 11. keyboard-quantizer-doc/rev4.md at master \- GitHub, [https\://github.com/sekigon-gonnoc/keyboard-quantizer-doc/blob/master/rev4.md](https://github.com/sekigon-gonnoc/keyboard-quantizer-doc/blob/master/rev4.md)  
> 12. [https\://qiita.com/a\_k\_/items/f884ab0ab5486220b494](https://qiita.com/a_k_/items/f884ab0ab5486220b494)  
> 13. PaulStoffregen/USBHost\_t36: USB Host Library for Teensy 3.6 and 4.0, [https\://github.com/PaulStoffregen/USBHost\_t36](https://github.com/PaulStoffregen/USBHost_t36)  
> 14. Prototyping System for Teensy 4.1 \- Working With USB Host, [https\://protosupplies.com/learn/prototyping-system-for-teensy-4-1-working-with-usb-host/](https://protosupplies.com/learn/prototyping-system-for-teensy-4-1-working-with-usb-host/)  
> 15. phorton1/Arduino-libraries-USBHost\_t36: My slightly modified fork of, [https\://github.com/phorton1/Arduino-libraries-USBHost\_t36](https://github.com/phorton1/Arduino-libraries-USBHost_t36)  
> 16. TMK USB to USB keyboard converter \- Geekhack, [https\://geekhack.org/index.php?topic=69169.350](https://geekhack.org/index.php?topic=69169.350)  
> 17. TMK USB to USB keyboard converter \- Geekhack, [https\://geekhack.org/index.php?topic=69169.150](https://geekhack.org/index.php?topic=69169.150)  
> 18. ISO International Keys · tmk/tmk\_keyboard Wiki \- GitHub, [https\://github.com/tmk/tmk\_keyboard/wiki/ISO-International-Keys](https://github.com/tmk/tmk_keyboard/wiki/ISO-International-Keys)  
> 19. Thanks for mentioning QMK \[0\], it's not something I've heard of, [https\://news.ycombinator.com/item?id=24290922](https://news.ycombinator.com/item?id=24290922)  
> 20. USキーボードをJISキーボードとして利用する｜okapi\_kun \- note, [https\://note.com/okapi\_kun/n/ndde1749ad134](https://note.com/okapi_kun/n/ndde1749ad134)  
> 21. Adjusting (USB) keyboard code mapping \- Teensy Forum \- PJRC, [https\://forum.pjrc.com/index.php?threads/adjusting-usb-keyboard-code-mapping.76475/](https://forum.pjrc.com/index.php?threads/adjusting-usb-keyboard-code-mapping.76475/)  
> 22. Teensy keyboard forwarder doesn't work on other computers, [https\://forum.pjrc.com/index.php?threads/teensy-keyboard-forwarder-doesnt-work-on-other-computers.74766/](https://forum.pjrc.com/index.php?threads/teensy-keyboard-forwarder-doesnt-work-on-other-computers.74766/)  
> 23. Issues · PaulStoffregen/USBHost\_t36 \- GitHub, [https\://github.com/PaulStoffregen/USBHost\_t36/issues](https://github.com/PaulStoffregen/USBHost_t36/issues)  
> 24. Using N-Key rollover with USBHost\_t36 library \| Teensy Forum \- PJRC, [https\://forum.pjrc.com/index.php?threads/using-n-key-rollover-with-usbhost\_t36-library.77593/](https://forum.pjrc.com/index.php?threads/using-n-key-rollover-with-usbhost_t36-library.77593/)  
> 25. get feature report with USBHost\_t36 \- Teensy Forum, [https\://forum.pjrc.com/index.php?threads/get-feature-report-with-usbhost\_t36.74192/](https://forum.pjrc.com/index.php?threads/get-feature-report-with-usbhost_t36.74192/)  
> 26. 3.6 USBHost\_t36 USB Hub examples? \- Teensy Forum, [https\://forum.pjrc.com/index.php?threads/3-6-usbhost\_t36-usb-hub-examples.51108/](https://forum.pjrc.com/index.php?threads/3-6-usbhost_t36-usb-hub-examples.51108/)  
> 27. JIS配列に配慮したzmkファームウェアの書き方TIPS \| FAQ \| エレキット, [https\://www\.elekit.co.jp/support/faq/5262](https://www.elekit.co.jp/support/faq/5262)  
> 28. \[\#SPRESENSE 2026\]Cインタプリタ(VM)を作ったにょ\[ch9350編\], [https\://elchika.com/article/bb6690e3-5f79-46b9-9365-c44652c4fe31/](https://elchika.com/article/bb6690e3-5f79-46b9-9365-c44652c4fe31/)  
> 29. Windowsの内蔵USで外部JISをJIS配列で使う \- MIHO Soft Design, [https\://www\.mihosoftdsgn.net/?p=256](https://www.mihosoftdsgn.net/?p=256)  
> 30. USB付きAdafruit Feather RP2040を使ったQMK nicolaアダプター, [https\://note.com/ja7rhk/n/n85328641b1c1](https://note.com/ja7rhk/n/n85328641b1c1)  
> 31. USB: HID Usage Table · tmk/tmk\_keyboard Wiki \- GitHub, [https\://github.com/tmk/tmk\_keyboard/wiki/USB:-HID-Usage-Table](https://github.com/tmk/tmk_keyboard/wiki/USB:-HID-Usage-Table)  
> 32. ADB German Keyboard layout ^ and \< reversed (Windows) · Issue \#35, [https\://github.com/tmk/tmk\_keyboard/issues/35](https://github.com/tmk/tmk_keyboard/issues/35)  
> 33. Teensy 3.6 USB Host support \| Page 5, [https\://forum.pjrc.com/index.php?threads/teensy-3-6-usb-host-support.37615/page-5](https://forum.pjrc.com/index.php?threads/teensy-3-6-usb-host-support.37615/page-5)  
> 34. Fluke 9100 System Troubleshooter USB Adapter · GitHub, [https\://github.com/HammysHangout/FlukeUSBAdapter](https://github.com/HammysHangout/FlukeUSBAdapter)  
> 35. USB Host Cable for Teensy 4.1 and 3.6, 5-pin to USB-A female, 20 cm, [https\://www\.informaticait.it/en/21854-usb-host-cable-for-teensy-41-and-36-5-pin-to-usb-a-female-20-cm-sparkfun-cab-28389.html](https://www.informaticait.it/en/21854-usb-host-cable-for-teensy-41-and-36-5-pin-to-usb-a-female-20-cm-sparkfun-cab-28389.html)  
> 36. TPD3S014 data sheet, product information and support \| TI.com, [https\://www\.ti.com/product/TPD3S014](https://www.ti.com/product/TPD3S014)  
> 37. USB Host 5V backfeed \- Teensy Forum, [https\://forum.pjrc.com/index.php?threads/usb-host-5v-backfeed.73329/](https://forum.pjrc.com/index.php?threads/usb-host-5v-backfeed.73329/)  
> 38. Teensy 4.1 USB Host max. allowed Power (current), [https\://forum.pjrc.com/index.php?threads/teensy-4-1-usb-host-max-allowed-power-current.68057/](https://forum.pjrc.com/index.php?threads/teensy-4-1-usb-host-max-allowed-power-current.68057/)  
> 39. Teensy 3.6 max input voltage, [https\://forum.pjrc.com/index.php?threads/teensy-3-6-max-input-voltage.49480/](https://forum.pjrc.com/index.php?threads/teensy-3-6-max-input-voltage.49480/)  
> 40. Teensy ® 3.6 Development Board \- PJRC, [https\://www\.pjrc.com/store/teensy36.html](https://www.pjrc.com/store/teensy36.html)  
> 41. Teensy 4.1 Best Practices \- GitHub Gist, [https\://gist.github.com/somebox/d969f8a97e5a4362af5049ed554a9e69](https://gist.github.com/somebox/d969f8a97e5a4362af5049ed554a9e69)
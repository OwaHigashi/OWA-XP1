# Claude Worklog

## 2026-04-28

### 概要

M5Core2 ベースの MIDI トランスポーザーに、MIDI メッセージ管理機能を追加した。従来の転調機能に加えて、`FILTER` と `MAPPER` を持つ `MIDI Manager` グループを実装し、画面遷移、UI、実機操作、書き込みまで確認した。

### 実装内容

- 既存スケッチを `M5Core2-MIDITransposerBT.ino` から `M5Core2-MIDIXposeFilBT.ino` へ整理
- `MIDI Manager` を追加
- `FILTER` を追加
- `MAPPER` を追加
- 処理順を `FILTER -> MAPPER -> Transpose -> OUT` に変更
- `FILTER` / `MAPPER` それぞれに独立した `BYPASS` / `ACTIVE` を追加
- Common / Realtime を含む MIDI メッセージ分類を追加

### 画面と操作

- メニューを 2 階層化
- 長押し `C` で `転調グループ <-> MIDI Manager`
- 短押し `C` で現在グループ内のサブモード切替
- 左上タイトルを `MIDI Transposer` / `MIDI Manager` で切替
- `FILTER` / `MAPPER` / `BYPASS` の下の空き領域を使って編集 UI を再配置
- 下段の `<` / `>`、`EN/DIS`、`ADD`、`DEL`、`UP`、`DOWN` の操作領域を拡大
- `DOWN` 表記へ統一

### ボタン仕様

- 転調グループ中の `B`
  - `DIRECT`: レンジ切替
  - `KEY`: 上位転調/通常転調切替
- `FILTER` 中の `B`
  - `Type` の順送り
- `MAPPER` 中の `B`
  - `PG1/PG2` 切替

### 調整・修正履歴

- `MIDI Manager` 用見出しを別行で出していた構成をやめ、左上タイトル切替へ変更
- `FILTER` の `Type` を単独循環ボタン化
- `BtnB` が `FILTER` 中に誤って `MAPPER` 側の挙動へ流れる問題を修正
- `BtnB` 処理後に同一フレームで他ボタン処理へ落ちる経路を整理

### 実機確認

- `arduino-cli compile --fqbn m5stack:esp32:m5stack_core2 D:\M5\M5Core2-MIDIXposeFilBT`
- `arduino-cli upload -p COM4 --fqbn m5stack:esp32:m5stack_core2 D:\M5\M5Core2-MIDIXposeFilBT`
- `FILTER` 中の `B` が `Type` 切替として動作することを最終確認

## 2026-04-28 USB serial / screenshot

### 追加内容

- USB シリアル経由のコマンドインタフェースを追加
- 外部からの `BUTTON` 注入を追加
- 外部からの `TOUCH x y` 注入を追加
- `STATUS` / `MODE` / `GROUP` / `SET TRANSPOSE` を追加
- `SCREENSHOT PPM` と `SCREENSHOT RGB888` を追加

### 目的

- PC から本体 UI を遠隔操作できるようにする
- 後続の大画面 GUI 実装で、本体側仕様を流用できるようにする
- 画面キャプチャを使って、初心者向けマニュアル作成をしやすくする

## 2026-04-29 画面レイアウト + スクリーンショット安定化 + マニュアル

### UI レイアウト修正

- ヘッダを 3 行 → 2 行構成に整理
  - Row1 (y=2): タイトル + I/O カウンタ + Trans 値
  - Row2 (y=22): AllOff (色付き) + BT (色付き) + ボタン補助
  - y=40 に区切り線
- ヘッダが y=42-55 に重なって `AllOff: ...` が二重に見える問題を解消
- 各モードのレイアウトを再計算 (DIRECT 60px ボタン、KEY 鍵盤縮小、SEQUENCE 上シフト 等)
- KEY モードの Major / Minor Keys ラベルが鍵盤に潰れる問題を解消

### スクリーンショット機能の試行錯誤

調査した内容と最終解:
- `M5.Lcd.readRectRGB` の行ごと呼び出しは LCD アドレスポインタのドリフトで色化けする
- TFT_eSprite の PSRAM 配置 + `fillRect` (`memcpy` で行を複製) はキャッシュ整合性で塗りつぶし矩形の下半分が別色に化ける ([TFT_eSPI #198](https://github.com/Bodmer/TFT_eSPI/issues/198), [#697](https://github.com/Bodmer/TFT_eSPI/issues/697))
- 解: 公式 [M5Stack `TFT_Screen_Capture/screenServer.ino`](https://github.com/m5stack/M5Stack/blob/master/examples/Advanced/Display/TFT_Screen_Capture/screenServer.ino) と同じく
  - **PSRAM スプライトを使わず LCD に直接描画**
  - **画面取得は LCD GRAM から行ごと `readRectRGB`**

### マニュアル

- `manual.html` を作成 (1 ファイル完結、ダークテーマ、画像 8 枚埋込)
- 8 章構成: 概要、ハード、画面、操作、各モード、MIDI Manager、フロー、USB シリアル、トラブル

### 画像取得済み (最新)

- `screenshots/01-direct.ppm` 〜 `08-mapper-pg2.ppm` (各 PPM/PNG セット)

## 2026-05-02 テストトーンSMF対応 + PLAYER中のMIDI INミックス

### 概要

演奏メニューの `TEST TONE` を、SDカード上の `SMF` フォルダ内の `testtone.smf` / `testtone.mid` / `testtone.midi` を優先して再生する形に変更した。該当ファイルが見つからない場合は、代替として `CDEFGABC` の簡易フレーズを再生する。

同時に、`PLAY` モード中でも MIDI IN を継続監視して出力へ通すようにした。これにより、SMF 再生中でも外部 MIDI 入力をミックスできる。

### 実装内容

- `TEST PHRASE` ボタンを `TEST TONE` に変更
- `sendPlayTestPhrase()` を SMF 読み込み優先に変更
- SMF が無い場合のフォールバックとして `CDEFGABC` を再生
- `PLAY` 中でも `processMIDI()` の再生ループを維持
- テストトーン再生をブロッキング `delay()` ベースから協調的な再生処理へ変更
- SD 内の `SMF` フォルダを走査して `.mid/.midi/.smf` を拾う処理を追加

### 確認状況

- ソースの差分反映済み
- `arduino-cli` はユーザー領域で利用可能に設定済み
  - 実体: `C:\Users\west\AppData\Local\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe`
  - シェルからは `arduino-cli` で起動可能
- この時点では実機焼き込みと実動作確認は未実施

### 次回の確認ポイント

- SD の `SMF` フォルダに `testtone.smf` か `testtone.mid` を置いた場合に、その曲が再生されること
- ファイルが無い場合に `CDEFGABC` が鳴ること
- `PLAY` 中に外部 MIDI キーボード入力が継続して通ること
- SMF 再生中の MIDI IN 監視が停止しないこと

## 2026-05-03 SMF Player 統合 + 派手スプラッシュ + バグ修正 + 姉妹プロジェクト同期

### 概要

`PLAY` モードの `C` 短押しに **SMF Player** を統合した (移植元: `../M5Core2-SMF-Player`)。
SD `/smf` フォルダの .mid/.smf を再生し、画面下部に 16 ch 鍵盤をリアルタイム表示する。
SysEx メッセージにも対応。
あわせて起動スプラッシュを ~4 秒の派手アニメ (虹色グロー脈動) に拡張。

### バグ修正

- **SEQUENCE モード入室時のクラッシュ**: `advanceSubMode()` 内で 4 要素の
  `modeNames[]` を `currentMode` (DisplayMode 0..5) で添字。SEQUENCE_MODE=4 で
  配列範囲外アクセスし ESP32 がパニック→再起動していた。`getDisplayModeLabel()`
  に置換して解消。
- **auto-prototype エラー**: testtone.smf 系の型 (`PlayToneSourceKind`,
  `SmfTrackState` 等) がファイル中段に定義されていたため、Arduino auto-prototype
  生成位置 (ファイル冒頭) から型が見えずビルド不可。型定義をファイル先頭へ移動。

### 実装内容

- `SMF_PLAYER_MODE` を `DisplayMode` enum に追加
- `MD_MIDIFile` ライブラリ (移植元のまま) を `src/MD_MIDIFile/` に同梱
- SD バスは `SD.h` ⇄ `SdFat` をモード入退室で動的に切替
- `A`=前曲 / `B`=再生停止 / `C 短押し`=次曲 / `C 長押し`=PLAY 復帰
- Note On/Off に追従する 16ch×128 ノートのリアルタイム鍵盤表示
- 4 秒スプラッシュ: スイープインバー + スパークル + 虹色脈動タイトル + ローディングバー

### 姉妹プロジェクト同期

- `../M5Core2-MIDIXposeFilBT` (M5 MIDI Module2 / G13/G14 版) を当プロジェクトと
  完全同期。差分は MIDI ピンと Wire.end / INPUT_PULLUP の有無のみ。
- `../M5Core2-MIDITransposerBT` にも 4 秒スプラッシュを追加 (subtitle のみ
  "MIDI Transposer" に差し替え)。

### テスト/検証

- `tests/test_midi_mapper.cpp` を作成。MAPPER ロジックの単体ホスト試験 (g++)。
  17 ケース PASS / 100 万呼び出し 13 ns/call。
- `tests/diag_smf_entry.py`, `tests/probe_smf_state.py` を作成。USB シリアル
  経由で SMF 入退室・モード遷移を再現する診断スクリプト。
- `scripts/capture_smf_screenshots.ps1` で SMF Player 画面のキャプチャを自動化。

### ドキュメント

- README.md / manual.html を 2 つの FilBT プロジェクトで更新。
- 新規スクリーンショット: `09-smf-stop.png` / `10-smf-playing.png`

## 2026-05-24

### Button C release エッジ修正の確定 + 横断影響調査

- commit ab4349c: Button C 短押しを `wasReleasefor` の release エッジで発火(down エッジ発火
  だと長押し群切替に到達できないバグの修正)。PLAY テスト音ボタンはトグル停止に対応。
- 同種バグ(同一ボタンに長押し+down短押し)の横断調査を実施。実害があったのは本 UM 版と
  非UM版 ../M5Core2-MIDIXposeFilBT のみ。Tab 系(M5Unified/シリアル経由)と
  ../M5Core2-PuyoVaders(release 発火)は非該当。詳細は FilBT 側 worklog 参照。
- bin 更新: 再コンパイル済みで SDimg/00MIDIXposeFilBTUM-Black.bin はこの修正を含む
  (1,415,408 B, 2026-05-24)。

### SMF プレーヤー(16ch 鍵盤表示)操作インタフェース刷新

意図: `C` ボタンを「モード切り替え専用」に統一したい。従来 SMF 画面で `C` が
曲操作に使われていた競合を解消する。

新しい割り当て(SMF プレーヤー画面):

- 再生 / 停止 = 画面右上の ▶(再生) / ■(停止) **タッチボタン**へ移譲
  (停止中は ▶ が緑、再生中は ■ が赤で点灯)。
- `A` 短押し = 曲戻り / `B` 短押し = 曲送り(再生中なら新しい曲を続けて再生、
  停止中は選曲のみ)。
- `A` / `B` **長押し** = 「便利な即選曲」画面(フォルダブラウザ)の開閉。
  サブフォルダをたどれる。`[..]` で 1 階層上、`<<`/`>>`(または `A`/`B`)で
  ページ送り、曲タッチで即再生、右上 `X` で閉じる。基底 `/smf` より上には
  行かない。選曲したフォルダが以後の曲送り/戻りの対象になる。
- `C` 短押し・長押し = いずれもモード切替(SMF を抜けて PLAY へ)。

実装ポイント:

- `g_smfFolder` を `const char*` → `char[SMF_MAX_PATH]` 化。基底フォルダ
  `g_smfBaseFolder` と閲覧フォルダ `g_smfBrowseDir` を追加。
- スキャンを汎用化: `smfPlayerLoadSongsFromFolder()`(曲のみ) と
  `smfPlayerScanBrowseDir()`(フォルダ→曲の順で `g_smfEntries[]` に列挙)。
- ブラウザ表示中はメインループの鍵盤差分描画と再生/停止時の再描画を抑止
  (`g_smfBrowserActive` ガード、`smfPlayerRefreshScreen()`)。曲は裏で
  鳴り続けられる。
- `processHardwareButtons()`: SMF 時のみ `A`/`B` も release エッジ短押し +
  700ms 長押し方式に(他モードは従来の `wasPressed` 即時動作のまま)。
- タッチ: `dispatchTouchPoint()` の SMF 分岐を `processSmfPlayerTouch()` に接続。
- コンパイル OK (core 2.1.4, 1,412,885 B / 21%, グローバル +約7KB は
  `g_smfEntries[200]` 分)。

ドキュメント: README.md / manual.html の SMF 操作節・操作早見表・ナビ要約・
図キャプションを更新。スクリーンショット(09/10)は新トランスポート反映のため
要再キャプチャ(`scripts/capture_smf_screenshots.ps1`)。

### 横断影響(他モデル)

- SMF プレーヤーのコードを共有するのは非UM版 `../M5Core2-MIDIXposeFilBT`
  (synth=0)のみ(Tab 系・他ゲームは非共有、`OLD/` は旧バックアップ)。
- **今回は本 UM 版のみ実装**(ユーザー指示)。非UM版は未適用。移植する場合の
  注意: 非UM版は `MIDIXPOSE_HAS_LOCAL_SYNTH=0` でモード遷移経路が異なり、
  SMF は群サイクルに含まれる(`C` でのモード切替先が PLAY ではなく転調群)。
  `handleButtonCLongAction()` の `#if` 分岐がそのまま効くので C 統一はそのまま
  移植可能だが、A/B 長押しブラウザとタッチ移譲は同一に移植してよい。

### SMF 選曲画面の鍵盤残像修正 + タッチ式マスターボリューム

実機フィードバック2件への対応(本UM版のみ、非UM版未適用):

1. **選曲画面の鍵盤残像**: 鍵盤差分描画ガード(`if(!g_smfBrowserActive)`)自体は
   効いていたが、「開いた瞬間の1フレームだけ残る」。鍵盤→選曲画面の遷移フレームで
   紛れ込む残像と判断。`smfBrowserOpen()` で `g_smfBrowserSettle=3` をセットし、
   `smfPlayerProcessLoop()` で選曲表示中の最初の3ティックだけ `drawSmfBrowserScreen()`
   を描き直して残像をクリア(以降はガードで描かない)。切り分け用に open/close の
   Serial ログも追加。
2. **タッチ式マスターボリューム**: 端末に物理ボリュームが無い(Unit MIDI 出力は
   Core2 スピーカーを経由しない)ため、プレーヤー画面下部(y=224)に横バーを追加。
   左端=0 / 右端=127 をタッチで設定。**GM ユニバーサル Master Volume SysEx**
   `F0 7F 7F 04 01 00 vol F7` で全体音量を変更(曲ごとの CC7 には触れずミックス維持)。
   `g_masterVolume` 保持。曲頭の GM リセットが音量を最大に戻すので、再生開始の
   即時送出に加え 800ms 後に再送(`g_smfVolumeResendAt`)。`smfPlayerEnter()` でも送出。
   **要実機検証**: SAM2695 が Universal Master Volume を honor するか。効かない場合の
   代替は全16chへ CC7 送出(ただし SMF の CC7 と競合)。コンパイル OK
   (core 2.1.4, 1,413,545 B / 21%)。FLASH 済 COM16。
   ドキュメント(README/manual)は音量の実機動作確認後に確定更新予定。

### 実機確認OK → 非UM版へ横展開 + bin/SDimg 更新

ユーザー実機で残像解消・ボリューム動作とも確認OK。以下を実施:

- **ドキュメント確定**: UM 版 README.md / manual.html に音量バー(GM Master Volume SysEx)を追記。
- **非UM版 (../M5Core2-MIDIXposeFilBT) へポーティング**: UM の SMF 改修差分を
  `git diff HEAD -- *.ino` でパッチ化し、非UM へ `git apply`。機種差(synth=0 /
  MIDIピン13・14 / スプラッシュ文言 / Wire.end・pinMode 無し / TESTTONE トグル無し)
  には一切触れずに SMF 改修だけを移植。2方向 diff で検証済:
  (a) 非UM新 vs 非UM旧 = SMF改修のみ(588行); (b) 非UM新 vs UM = 既知の機種差9点のみ。
  非UM の MIDI_MAX_TRACKS は既に 32(同期済)。非UM の README/manual も更新
  (入退室は転調群サイクル経由・C で転調へ戻る、と非UM 仕様に合わせて記述)。
- **bin 作成 & SDimg 更新**:
  - UM: 1,419,840 B → `SDimg/00MIDIXposeFilBTUM-Black.bin`(デバイス焼込みと同一)。
  - 非UM: 1,419,600 B → `SDimg/00MIDIXposeFilBT-Gray.bin`。
  - 両 repo の `build/*.ino.bin` も更新。
- 非UM側の詳細は `../M5Core2-MIDIXposeFilBT/.claude/worklog.md` にも記録。

### SD カード作成ツール + マニュアル PDF (D:\M5 直下)

- **`D:\M5\SDimg-COPY.bat`**: SDimg を SD カードへ完全同期(ミラー)するワンクリック
  バッチ。引数なし=F:、引数でドライブ変更可 (`SDimg-COPY.bat E`)。robocopy /MIR で
  コピー+余分削除。実行前に対象ドライブ確認 + choice 確認、ドライブ非存在なら安全終了。
- **マニュアル PDF**: 各ツールの `manual.html` を headless Edge で PDF 化し
  `SDimg/TOOL-DOC/` に配置 (`M5Core2-MIDIXposeFilBTUM_manual.pdf` /
  `M5Core2-MIDIXposeFilBT_manual.pdf`)。**今後 manual.html 更新時は PDF も再生成**
  (ユーザー指示)。再生成用に **`D:\M5\MANUAL-PDF.bat`** を用意 (両 manual.html →
  TOOL-DOC PDF をワンクリック)。
- **ハマりどころ**: .bat は UTF-8/LF だと cmd が誤読(`%~dp0` 空・行崩れ)。
  **Shift-JIS(CP932)+CRLF** + `chcp 932` で保存し直して解決。両バッチとも動作確認済。

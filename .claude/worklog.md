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

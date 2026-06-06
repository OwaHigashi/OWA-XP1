# OWA-XP1

M5Core2 + M5 Unit MIDI (SAM2695) を使った **MIDI 演奏機 / トランスポーザー / メッセージ管理ツール / SMF プレーヤー** です。
起動直後は `PLAY` モード (Unit MIDI 内蔵 GM 音源を直接鳴らす) に入り、`C` 長押しで転調機能・MIDI 加工機能、`C` 短押しで SMF プレーヤーへ移れます。

現在のスケッチ本体は [OWA-XP1.ino](./OWA-XP1.ino) です。

## ハードウェア

- **本体**: M5Stack Core2
- **音源**: M5 Unit MIDI (SAM2695)
- **接続**: Core2 の **Port A (Grove)** に Unit MIDI を挿す
- **UART**: G33=RX / G32=TX, 31250 bps (`Serial2`)
- **注意**: 起動時に `Wire.end()` を呼んで Port A の I2C を解放するため、Port A 上の他の I2C デバイスは併用できません。

## 起動スプラッシュ

電源投入時に約 4 秒のオープニングが流れます。
"OWAMIDICON" のロゴが虹色グローで脈動し、上下のグラデーションバーがスイープイン、ローディングバーがゆっくり充填します。

## 概要 — 演奏 (PLAY) モード

起動直後の基本モードです。Unit MIDI 内蔵 GM 音源を直接鳴らせます。

- 外部 MIDI キーボードから届いた Note は、`FILTER / MAPPER / Transpose` を経由せずそのまま発音されます。
- 画面の音色名バーをタップすると 128 音色から選択できます (Page 切替対応)。
- `VOL- / VOL+` で CC#7 を、`PRG- / PRG+` で Program を、`PB- / PB+` で Pitch Bend を、`SUS` で CC#64 を、`INIT` で再初期化を送れます。
- `TEST TONE` を押すと、SD カードの `/SMF/testtone.smf` (または `.mid` / `.midi`) を優先して再生します。該当ファイルがない場合は `CDEFGABC` の代替フレーズが鳴ります。
- `PLAY` 画面に入った時点で Roland `GS Reset` を送り、現在の `Volume` / `Program` / `Pitch Bend` / `Sustain` を再送します。
- `PLAY` 中でも `MIDI IN` を継続監視し、外部 MIDI 入力をミックスできます。

## 概要 — SMF プレーヤー

`PLAY` 中に `C` 短押しで切り替わる **SMF プレーヤー画面**です。
SD カードの `/smf` (または `/SMF`) フォルダにある `.mid` / `.smf` ファイルを再生します。

- **16 チャンネル分の鍵盤を画面下部に表示**します。再生中は Note On に応じてキーが緑 (白鍵) / オレンジ (黒鍵) で点灯します。
- **System Exclusive メッセージにも対応**します (受信したペイロードはそのまま MIDI OUT に流れます)。
- 内部の SMF パーサは `MD_MIDIFile` ライブラリ (本リポジトリ `src/MD_MIDIFile/` 同梱) を使用しています。
- **再生 / 停止は画面右上の ▶ (再生) / ■ (停止) をタッチ**します。
- `A` で**曲戻り**、`B` で**曲送り**。`A` / `B` の**長押しで「便利な即選曲」画面 (フォルダブラウザ)** が開き、サブフォルダをたどって曲を選べます。
- `C` は短押し・長押しともに**モード切り替え** (`PLAY` に戻る) 専用です。
- **画面下部のバーをタッチでマスターボリューム調整** (左端=0 / 右端=最大)。本体に物理ボリュームが無いため、GM ユニバーサル Master Volume SysEx (`F0 7F 7F 04 01 00 vol F7`) で音源全体の音量を変えます (曲ごとの CC7 には触れません)。
- 曲終了で自動的に次の曲へ進みます (曲送り/戻りは再生中の曲があるフォルダ内で循環)。
- 入室時に SD バスを `SD.h` から `SdFat` に一旦譲渡し、退出時に戻します。

## 概要 — 転調 / MIDI 管理

`PLAY` / SMF プレーヤー以外のモードでは、入力された MIDI メッセージに対して次の順序で処理します。

1. `FILTER`
2. `MAPPER`
3. `Transpose`
4. MIDI OUT

`FILTER` と `MAPPER` はそれぞれ独立して `BYPASS` / `ACTIVE` を切り替えられます。
両方を `BYPASS` にすれば、従来どおりの低遅延な転調処理だけを使えます。

## モード構成

画面は `演奏`、`SMF プレーヤー`、`転調`、`MIDI 管理` の 4 つを軸にした構成です。
グループ移動は次のとおりです:

- `C` 短押し (PLAY モード時): `演奏 -> SMF プレーヤー`
- `C` 短押し・長押し (SMF プレーヤー時): `SMF プレーヤー -> 演奏`
- `C` 長押し (PLAY モード時以外): `演奏 -> 転調 -> MIDI 管理 -> 演奏 ...`

### 0. 演奏モード (PLAY)

Unit MIDI 内蔵 SAM2695 を直接鳴らすモードです。

操作:

- 画面音色名バー: 128 音色ピッカーを開く
- `VOL- / VOL+`: CC#7 を 8 ステップ単位 (0–127)
- `PRG- / PRG+`: Program 番号を 1 つ前後
- `PB- / PB+`: Pitch Bend を 256 ステップ単位 (0–16383, 中央 8192)
- `SUS`: CC#64 (Sustain) を ON / OFF
- `INIT`: GS Reset → Volume / Program / Bend / Sustain を再送
- `TEST TONE`: SD `/SMF/testtone.smf` を再生 (未配置時は `CDEFGABC`)
- `B`: 初期化 (`INIT` と同じ)
- `C` 短押し: **SMF プレーヤーを開く**
- `C` 長押し: `演奏 -> 転調`

### 1. SMF プレーヤー (新規)

- 画面右上 ▶ タッチ: 再生 / ■ タッチ: 停止
- `A` 短押し: 曲戻り / `B` 短押し: 曲送り (選曲画面表示中はページ送り)
- `A` / `B` 長押し: 便利な即選曲画面 (フォルダブラウザ) を開く / 閉じる
- 画面下部のバー タッチ: マスターボリューム (左端=0 / 右端=127, GM Master Volume SysEx)
- `C` 短押し・長押し: `SMF プレーヤー -> 演奏` (再生中は停止し All Notes Off)

**便利な即選曲画面 (フォルダブラウザ)**

- `[DIR] 名前/` タッチ: サブフォルダを開く / `[..] up` タッチ: 1 階層戻る
- 曲名タッチ: 即再生 (そのフォルダが曲送り/戻りの対象になる)
- 上部 `<<` / `>>` (または `A`/`B`): ページ送り / 右上 `X`: 閉じる
- `/smf` (基底) より上の階層へは移動しません

### 2. 転調グループ

短押し `C` で次を巡回します。

- `DIRECT`
- `KEY`
- `INSTANT`
- `SEQUENCE`

### 3. MIDI 管理グループ

長押し `C` で `転調 -> MIDI 管理` に移ります。
短押し `C` で次を巡回します。

- `FILTER`
- `MAPPER`

M5 Unit MIDI を接続している場合は、ここで処理した MIDI をそのまま Unit MIDI から発音できます。
MIDI フィルタやマッパーを使いながら、実音を確認できます。

## ハードウェアボタン

### 共通

- `A`: All Notes Off の有効/無効切替 (SMF 中は曲戻り、長押しで即選曲画面)
- `B`: モード別アクション (PLAY: INIT / SMF: 曲送り・長押しで即選曲画面 / FILTER: Type 送り / 他)
- `C` 短押し: 現在グループ内の次モードへ (PLAY: SMF へ / SMF: `演奏` へ戻る)
- `C` 長押し: グループ巡回 (`演奏 -> 転調 -> MIDI 管理`) / SMF 中は `演奏` へ戻る
- SMF プレーヤーの再生 / 停止は画面右上の ▶ / ■ タッチ

### 転調グループ中の `B`

- `DIRECT`: レンジ切替
  - `0..+11`
  - `-11..0`
  - `-5..+6`
- `KEY`: 上位転調/通常転調の切替
- `INSTANT`: 転調値を `0` にリセット
- `SEQUENCE`: 次のステップへ進む（画面右の「次」ボタンと同じ）

### MIDI 管理グループ中の `B`

- `FILTER`: `Type` を次のメッセージ種別へ進める
- `MAPPER`: `PG1/PG2` 切替

## 転調機能

### DIRECT

12 ボタンの直接選択方式です。
現在レンジ内の転調値をそのまま選択します。

### KEY

メジャー/マイナーのキー指定で転調値を決定します。

### INSTANT

よく使う転調値をワンタップで呼び出します。
`B` ボタンを押すと転調値を `0` にリセットできます。

- `0`
- `+1`
- `+2`
- `+3`
- `+5`
- `-1`
- `-2`
- `-3`
- `-5`

### SEQUENCE

複数ステップの転調値パターンを順番に呼び出します。
ステップ値編集、ステップ移動、パターン切替、SD 保存に対応しています。
`B` ボタンは画面右の「次」ボタンと同じで、次のステップへ進みます。

## MIDI Manager

### FILTER

不要な MIDI メッセージをブロックします。
一致したメッセージは `MAPPER` と `Transpose` に進まず、その場で破棄されます。

現状のルール定義項目:

- `EN/DIS`
- `Type`
- `Ch`
- `ADD`
- `DEL`
- `UP`
- `DOWN`

`Type` はタップまたは `B` ボタンで順送りします。
`Ch` は `ALL` または `Ch1..Ch16` を切り替えます。

#### 対応メッセージ種別

- `NoteOff` / `NoteOn` / `KeyPrs` / `PrgChg` / `CtrlChg` / `ChPrs` / `Bend`
- `SysEx` / `MTC` / `SongPos` / `SongSel` / `TuneReq`
- `Clock` / `Start` / `Cont` / `Stop` / `ActSn` / `Reset`

### MAPPER

MIDI メッセージの再割り当て/変換を行います。
リスト先頭から順に評価し、最初に一致したルールだけを適用します。

現状のルール定義項目:

- `EN/DIS`
- `ADD`
- `DEL`
- `UP`
- `DOWN`
- `PG1/PG2`

#### PG1 (変換元) / PG2 (変換先)

- `Type`
- `Ch`
- `Data1`
- `Min`
- `Max`

補足:

- `Data1` は `ANY` / `KEEP` を使う項目があります
- `Min/Max` は値レンジ変換に使います
- `FILTER` の後に `MAPPER` が動作します

`tests/test_midi_mapper.cpp` に `MAPPER` 単体の動作検証ハーネス (PC ホストでビルド) を同梱しています。
`tests/build_and_run.sh` でビルド & 実行できます。

## 画面サンプル

| 画面 | スクリーンショット |
|------|--------------------|
| PLAY モード | `screenshots/00-play.png` |
| 音色ピッカー | `screenshots/00-play-picker.png` |
| DIRECT | `screenshots/01-direct.png` |
| KEY | `screenshots/02-key.png` |
| INSTANT | `screenshots/03-instant.png` |
| SEQUENCE | `screenshots/04-sequence.png` |
| FILTER (BYPASS) | `screenshots/05-filter.png` |
| FILTER (ACTIVE) | `screenshots/06-filter-active.png` |
| MAPPER PG1 | `screenshots/07-mapper-pg1.png` |
| MAPPER PG2 | `screenshots/08-mapper-pg2.png` |
| **SMF Player (停止)** | `screenshots/09-smf-stop.png` |
| **SMF Player (再生中)** | `screenshots/10-smf-playing.png` |

## 基本的な使い方

### 転調だけを使う場合

1. 長押し `C` で `MIDI Manager` に入っている場合は、もう一度長押し `C` で転調グループへ戻します。
2. 必要に応じて短押し `C` で `DIRECT` / `KEY` / `INSTANT` / `SEQUENCE` を選びます。
3. `MIDI Manager` を経由させたくない場合は、`FILTER` と `MAPPER` の両方を `BYPASS` にして使います。

### SMF を再生する場合

1. SD カードに `/smf` または `/SMF` フォルダを作り、`.mid` / `.smf` ファイルを置きます。
2. 起動後 `PLAY` モードで `C` 短押し → SMF プレーヤー画面が開きます (初回はスキャンに数秒かかります)。
3. `A` (曲戻り) / `B` (曲送り) で曲を選び、画面右上 ▶ で再生 / ■ で停止。`A` か `B` の長押しで「便利な即選曲」画面 (フォルダブラウザ) を開いてサブフォルダから選曲。
4. 戻るには `C` 長押し。

### FILTER を設定する場合

1. 長押し `C` で `MIDI Manager` に入ります。
2. 短押し `C` で `FILTER` を表示します。
3. `ADD` でルールを追加し、対象ルールを一覧から選びます。
4. `Type` をタップ、または `B` ボタンでブロック対象のメッセージ種別を切り替えます。
5. `Ch` で `ALL` または `Ch1..Ch16` を選びます。
6. `EN/DIS` でそのルールを有効化します。
7. 画面上部の `BYPASS` / `ACTIVE` で、FILTER 全体を即座に有効/無効化できます。

### MAPPER を設定する場合

1. `MIDI Manager` 内で短押し `C` を使って `MAPPER` を表示します。
2. `ADD` でルールを追加し、対象ルールを一覧から選びます。
3. `B` ボタンで `PG1` と `PG2` を切り替えます。
4. `PG1` で変換元の `Type` / `Ch` / `Data1` / `Min` / `Max` を設定します。
5. `PG2` で変換先の `Type` / `Ch` / `Data1` / `Min` / `Max` を設定します。
6. `UP` / `DOWN` でルール順を変更します。評価順はリスト先頭からなので、上にあるルールほど優先されます。
7. `EN/DIS` で個別ルールを有効化し、上部の `BYPASS` / `ACTIVE` で MAPPER 全体の有効/無効を切り替えます。

### すぐに効果を確認する場合

- `FILTER` または `MAPPER` を `ACTIVE` にすると、その場で受信 MIDI に対して処理が反映されます。
- 効果比較をしたい場合は、上部の `BYPASS` と `ACTIVE` を切り替えるだけで元の経路と比較できます。
- 低遅延の従来動作に戻したい場合は、`FILTER` と `MAPPER` の両方を `BYPASS` にします。

## タッチ操作

`FILTER` / `MAPPER` / `BYPASS(ACTIVE)` は上段の大ボタンです。
一覧から対象ルールを選び、下段の操作ボタンと編集ボックスで設定します。

現状の UI 方針:

- 上段: ページ/バイパス切替
- 中段: ルール一覧
- 下段: ルール操作
- 最下段: 編集項目

SMF プレーヤー画面では、再生 / 停止と選曲 (即選曲画面) はタッチ、曲送り / 戻りと選曲画面の開閉は `A` / `B` ボタン、モード切り替えは `C` ボタンで操作します。

## MIDI 処理仕様メモ

- Realtime / Common メッセージも分類して処理
- `FILTER` はメッセージ単位でブロック
- `MAPPER` は最初に一致した 1 ルールを適用
- `Transpose` は主に Note On / Note Off へ適用
- `All Notes Off` は全 16ch に送信
- SMF 再生はファイル内のテンポ/ティック情報に従い、SysEx も含めて送出

現状の制限:

- `SysEx` はフィルタ対象だが、ペイロード変換は未実装
- SMF プレーヤー入室中は他機能の SD カード書き込みが一時停止します (退出時に復帰)

## ファイル構成

- `OWA-XP1.ino`: メインスケッチ
- `src/`: Bluetooth HID 関連コード
- `src/MD_MIDIFile/`: SMF パーサライブラリ (移植元: `../M5Core2-SMF-Player`)
- `tests/`: PC ホストで動かす MAPPER テストハーネスとシリアル診断スクリプト
- `screenshots/`: 各モードの画面キャプチャ
- `scripts/`: 画面キャプチャ用 PowerShell スクリプト

## ビルドと書き込み

`arduino-cli` はスケッチ名とフォルダ名の一致を要求するため、本リポジトリでは
ビルド用のサブディレクトリにコピー (またはジャンクションを作成) してからコンパイルしています。

```bash
# 例: Git Bash 上での 1 セット
mkdir -p /tmp/sketch_build/OWA-XP1
cp OWA-XP1.ino /tmp/sketch_build/OWA-XP1/
cp -r src /tmp/sketch_build/OWA-XP1/

arduino-cli compile --fqbn m5stack:esp32:m5stack_core2 /tmp/sketch_build/OWA-XP1
arduino-cli upload  -p COM3 --fqbn m5stack:esp32:m5stack_core2 /tmp/sketch_build/OWA-XP1
```

`-p` オプションには本機が見えている COM ポート (USB) を指定します
(`arduino-cli board list` で確認可能)。

## USB シリアルコマンド

PC から USB シリアルで本体を操作できます。
シリアル速度は `115200bps`、改行は `LF` または `CRLF` です。

起動後に `HELP` を送ると、利用できるコマンド一覧を返します。

### 主なコマンド

- `HELP`
- `STATUS`
- `REDRAW`
- `BUTTON A`
- `BUTTON B`
- `BUTTON C`
- `BUTTON C LONG`
- `TOUCH x y`
- `MODE PLAY`
- `MODE DIRECT`
- `MODE KEY`
- `MODE INSTANT`
- `MODE SEQUENCE`
- `MODE FILTER`
- `MODE MAPPER`
- `GROUP PLAY`
- `GROUP TRANSPOSE`
- `GROUP MIDI`
- `SET TRANSPOSE n`
- `INFO SCREEN`
- `SCREENSHOT PPM`
- `SCREENSHOT RGB888`

`STATUS` は現モード (`mode=PLAY/DIRECT/.../SMF_PLAYER` を含む)、転調値、FILTER/MAPPER の状態、MIDI 入出力カウントなどを 1 行で返します。

### 使い方の考え方

- `BUTTON` は本体の A/B/C ボタン操作を外部から再現します (SMF プレーヤーへの入退室にも使えます)。
- `TOUCH x y` は画面の指定座標をタップしたのと同じ扱いです。
- `MODE` と `GROUP` は、目的の画面へ直接切り替えたいときに使います (SMF プレーヤーへの直接遷移は `BUTTON C` を経由します)。

## スクリーンキャプチャ

画面のスクリーンショットは USB シリアル経由で取得できます。

### `SCREENSHOT PPM`

初心者向けのマニュアル作成や静止画保存に向く形式です。
コマンド送信後、最初に次のようなヘッダ行が返ります。

```text
OK SCREENSHOT format=PPM width=320 height=240 bytes=230415
```

その直後に、バイナリの `PPM(P6)` データ本体が流れます。
指定バイト数を読み切ると、最後に `OK SCREENSHOT_DONE` が返ります。

### `SCREENSHOT RGB888`

PC 側 GUI で直接扱いやすい、生の `RGB888` バイト列です。
返し方は `PPM` と同じで、先頭ヘッダだけが `format=RGB888` になります。

`scripts/capture_screenshots.ps1` (転調系) と `scripts/capture_smf_screenshots.ps1` (SMF プレーヤー) でキャプチャを自動化できます。

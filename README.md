# Plugin Standalone

VST3 / AudioUnit プラグインを DAW なしで使うための軽量ホストアプリケーション。

ギターアンプシミュレーターなどのプラグインを、DAW を起動せずにスタンドアロンで使用できます。

## 機能

- VST3 / AU プラグインの読み込みと UI 表示
- オーディオ入出力の自動接続
- プラグインの状態とオーディオ設定の保存・復元（次回起動時に自動復元）
- macOS では AU を優先（VST3 との重複を排除）
- OS 内蔵プラグインを非表示にし、サードパーティプラグインのみ表示

## AG03MK2 + Paradise Guitar Studio

`Audio Settings...` で入力・出力ともに `Yamaha AG03MK2` を選択します。
入力Rを使用する場合、Active input channels は **Channel 2 のみ**を有効にし、
Active output channels は **Channel 1 と Channel 2** を有効にします。
選択したモノラル入力はプラグインの左右入力に送られ、出力はステレオを維持します。
AG03MK2 側は MIX MINUS ON を前提とします。

`Load Plugin...` から `UADx Paradise Guitar Studio (AudioUnit)` を選択してください。
44.1 kHz / 128 samples を開始点にし、音切れがある場合はバッファを増やします。
通常終了すると、プラグイン状態・入出力機器・有効チャンネル・サンプルレート・
バッファサイズを保存し、次回起動時に復元します。

## ビルド

CMake 3.22 以上と C++17 対応コンパイラが必要です。JUCE は CMake の FetchContent で自動取得されます。

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

macOS の場合、ビルド成果物は以下に生成されます:

```
build/PluginStandalone_artefacts/Release/Plugin Standalone.app
```

## ダウンロード

[Releases](https://github.com/noshut/plugin-standalone/releases) ページからビルド済みバイナリをダウンロードできます。

### macOS での初回起動

署名されていないため、初回起動時に警告が表示されます。ターミナルで以下を実行してください:

```bash
xattr -cr "/Applications/Plugin Standalone.app"
```

または「システム設定」→「プライバシーとセキュリティ」→「このまま開く」でも起動できます。

### Windows での初回起動

SmartScreen の警告が表示される場合は「詳細情報」→「実行」で起動できます。

## 技術スタック

- [JUCE](https://juce.com/) 8.0.4 - クロスプラットフォームオーディオアプリケーションフレームワーク
- CMake - ビルドシステム

## ライセンス

GPL-3.0（JUCE の AGPLv3 ライセンスに準拠）

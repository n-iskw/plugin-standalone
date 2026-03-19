# Simple Plugin Host

VST3 / AudioUnit プラグインを DAW なしで使うための軽量ホストアプリケーション。

ギターアンプシミュレーターなどのプラグインを、DAW を起動せずにスタンドアロンで使用できます。

## 機能

- VST3 / AU プラグインの読み込みと UI 表示
- オーディオ入出力の自動接続
- プラグインの状態とオーディオ設定の保存・復元（次回起動時に自動復元）
- macOS では AU を優先（VST3 との重複を排除）
- OS 内蔵プラグインを非表示にし、サードパーティプラグインのみ表示

## ビルド

CMake 3.22 以上と C++17 対応コンパイラが必要です。JUCE は CMake の FetchContent で自動取得されます。

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

macOS の場合、ビルド成果物は以下に生成されます:

```
build/SimplePluginHost_artefacts/Release/Simple Plugin Host.app
```

## ダウンロード

[Releases](https://github.com/noshut/simple-plugin-host/releases) ページからビルド済みバイナリをダウンロードできます。

### macOS での初回起動

署名されていないため、初回起動時に警告が表示されます。右クリック →「開く」→「開く」で起動できます。

### Windows での初回起動

SmartScreen の警告が表示される場合は「詳細情報」→「実行」で起動できます。

## 技術スタック

- [JUCE](https://juce.com/) 8.0.4 - クロスプラットフォームオーディオアプリケーションフレームワーク
- CMake - ビルドシステム

## ライセンス

GPL-3.0（JUCE の AGPLv3 ライセンスに準拠）

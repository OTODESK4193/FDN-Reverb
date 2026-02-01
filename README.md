# FDN Reverb

<div align="center">
  <img src="./logo.png" width="400" alt="FDN Reverb Logo">
  <p><i>Advanced 16-Channel Physics-Based Reverb & SFX Engine</i></p>
</div>

---
## 🚀 Overview

**FDN Reverb** は、16チャンネルのフィードバック・ディレイ・ネットワーク（FDN）を核とした、物理音響シミュレーター兼SFXエフェクト・エンジンです。

34種類の実在・架空のマテリアル、7種類の空間形状、そして気温・湿度による空気吸収シミュレーションを搭載。現実のホールトーンから、プラズマや筋肉、声道といった異界のサウンドまでを1台で生成します。

![スクリーンショット](./Screenshot.jpg)
---

## ✨ Features

* **16-Band FDN Engine**: 16本の独立した遅延回路が相互に干渉し、密度の高い残響を生成。
* **Material Modeling**: コンクリート、木材、大理石から「プラズマ」「筋肉」といった特殊素材まで34種類をシミュレート。
* **Physical Atmosphere**: 気温と湿度に基づき、高域の空気吸収（周波数依存の減衰）をリアルタイム演算。
* **Dynamics Control**: Ducking（ダッキング）およびBloom（ブルーム）機能を内蔵。サイドチェイン不要でリバーブの定位を制御。
* **SFX Mode**: 床素材に特殊素材を選択することで、物理演算を超越した「異形」のサウンドへと変貌。
* **Visual Feedback**: リアルタイム3Dルーム・ビジュアライザーと6バンドRT60特性グラフを搭載。
* **Modern Workflow**: ワンノブでトーンを決定するTilt EQ、入出力フィルタ、高品質オーバーサンプリング完備。

---

## 💿 Downloads & Installation

### 📥 Download

最新のビルド済みプラグイン（VST3/AU）は、以下の **Releases** ページからダウンロードしてください。

👉 **[Download FDN Reverb v1.0.0 (GitHub Releases)](https://github.com/OTODESK4193/FDN-Reverb/releases/tag/v1.0.0)**

![Downloads](https://img.shields.io/github/downloads/OTODESK4193/FDN-Reverb/total.svg)

### 🛠 Installation

1. ダウンロードしたファイルを解凍します。
2. OSごとのプラグインフォルダに移動させてください。
* **Windows**: `C:\Program Files\Common Files\VST3`

## 🖥️ システム要件
- Windows 10/11 (64bit)
- VST3対応DAW (Abletonで動作確認済み)
- 4GB RAM以上推奨


---

## 📖 Documentation

操作方法の詳細や、各マテリアルの解説については、以下の公式オンラインマニュアルおよびリファレンスを参照してください。

* 📘 **[FDN Reverb 公式ユーザーマニュアル](./FDN_Reverb_Basic_Guide.pdf)**
* 📚 **[マテリアル＆パラメータ・リファレンス](./FDN_Reverb_CompleteReferenceGuide.pdf)**

⚠️ 【重要】聴覚および再生機器の保護について
本プラグインは、高度なフィードバック・アルゴリズムを使用しており、設定の組み合わせによっては極めて強力な音響エネルギーが発生する場合があります。安全にご使用いただくため、以下の事項を必ずお守りください。

1. モニター音量の管理
初めて使用する場合や、プリセットを切り替える際、あるいはマテリアル（材質）を変更する際には、必ず再生システムの音量を十分に下げた状態で試聴を開始してください。

2. リミッターの設置
不測の事態（発振や急激なピーク）から耳とスピーカーを保護するため、本プラグインの後段にマスタリンググレードのリミッター、または安全のためのセーフティ・クリッパーを挿入することを強く推奨します。

3. SFXモード時の挙動
床（Floor）に特定の特殊素材（Plasma Field, Vocal Tract等）を選択すると、エンジンが通常の物理演算を逸脱した動作モードに入ります。この際、Decayノブを上げすぎると、入力信号がなくても音が鳴り止まない「発振状態」になることがあります。異常を感じた場合は、即座に画面上部の [PANIC] ボタンを押してください。

4. 高負荷時（Quality設定）
Quality (x4) 等の高設定ではCPUに大きな負荷がかかります。CPUの処理能力を超えた場合、デジタルノイズやグリッチ音が発生し、聴覚にダメージを与える恐れがあります。ご使用の環境に適した設定を選択してください。

免責事項
本ソフトウェアの使用によって生じた聴覚への障害、スピーカーやヘッドホン等の機材トラブル、およびその他のいかなる損害についても、開発者は一切の責任を負いません。すべて自己責任において、慎重な操作をお願いいたします。
---

## 🔧 Technical Details (for Developers)

このプロジェクトは **C++ / JUCE Framework** を使用して開発されています。

### Build Requirements

* Projucer (JUCE 7.0+)
* Visual Studio 2022 (Windows) / Xcode (macOS)
* C++17 compliant compiler

## 🤝 コミュニティ
[![X](https://img.shields.io/badge/X-%40kijyoumusic-black?logo=x&logoColor=white)](https://x.com/kijyoumusic)
---

## 📄 License

詳細は [LICENSE](./LICENSE) ファイルを参照してください。
---

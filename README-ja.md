# 📸 Img2Vec: ビットマップをベクターグラフィックに変換！ 🖌️

ビットマップ画像（JPGやPNG）を美しいベクターグラフィック（SVGやEPS）に変換するツール、それが **Img2Vec** です！ 🎉 シンプルな操作で、画像をスケーラブルなアートに変身させましょう！ 💻

## 🚀 ビルド方法

プロジェクトをビルドするのは超簡単！以下のコマンドを叩くだけ！ 🔨

```bash
$ make
```

これで準備完了！ 🛠️

## 🎨 使い方

`img2vec` コマンドを使って、画像をベクターに変換します。豊富なオプションで、あなた好みの結果をゲット！ ✨

### 基本構文
```bash
$ ./img2vec [オプション] 入力ファイル
```

### オプション一覧
- `-h` ❓ : ヘルプメッセージを表示
- `-o <出力ファイル名>` 📝 : 出力ファイル名を指定（デフォルト: `img2vec.eps`）
- `-svg` 🖼️ : 出力形式をSVGに変更（デフォルト: EPS）
- `-c <数値>` 🎨 : 色数を削減（デフォルト: 32）
- `-b <スケール>` 🌫️ : 画像をぼかす
- `-a` 🌟 : アルファチャンネルを考慮
- `-s <スケール>` 📏 : 画像のスケールを調整
- `-x` 🔧 : ディレイト処理を有効化
- `-cx <ビット数>` ⚙️ : 色深度を調整
- `-e <数値>` 🧹 : エッジ検出の閾値を設定
- `-r <数値>` 🔄 : 解像度を調整
- `-p <数値>` 🖌️ : パスの簡略化レベルを指定
- `-t <数値>` 🎯 : トレース精度を調整
- `-m` 🌈 : マルチカラーグラデーションを有効化

### 使用例
ここでは、実際に試した例をいくつかご紹介！ 🖌️

```bash
$ ./img2vec girl-1118419_1280.jpg -c 2 -o girl-1118419.eps
```
👉 色数を2に減らしてEPS形式で出力！

```bash
$ ./img2vec publicdomainq-0041064ikt.jpg -o publicdomainq-0041064ikt.svg -svg -a -e 0.5
```
👉 SVG形式で、アルファチャンネルを考慮し、エッジ検出閾値を0.5に設定！

```bash
$ ./img2vec sparkler-677774_1920.jpg -o sparkler-677774.svg -svg -s 0.4 -c 64 -m
```
👉 スケール0.4、64色、マルチカラーグラデーションを有効にしてSVGを出力！キラキラ✨

## 🖼️ サンプル画像
オリジナル画像と`img2vec`の出力を比較！以下のリンクでビフォーアフターをチェック！ 👀

Original image (https://pixabay.com/ja/illustrations/%E5%A5%B3%E3%81%AE%E5%AD%90-%E7%8C%AB-%E8%8A%B1-%E3%81%8A%E3%81%A8%E3%81%8E%E8%A9%B1-1118419/
)
![Original](girl-1118419_1280.jpg)

img2vec output
![Output](girl-1118419.svg)

Original image
![Original](publicdomainq-0041064ikt.jpg)

img2vec output
![Output](publicdomainq-0041064ikt.svg)

Original image
![Original](publicdomainq-0017653mro.jpg)

img2vec output
![Output](publicdomainq-0017653mro.svg)

Original image
![Original](night-4926430_1920.jpg)

img2vec output
![Output](night-4926430.svg)

Original image
![Original](girl-4716186_1920.jpg)

img2vec output
![Output](girl-4716186.svg)

Original image
![Original](sparkler-677774_1920.jpg)

img2vec output
![Output](sparkler-677774.svg)

Original image
![Original](2435687439_17e1f58a9c_o.jpg)

img2vec output
![Output](2435687439_17e1f58a9c_o.svg)

## ℹ️ プロジェクトについて
- **目的**: ビットマップをベクターグラフィックに変換 🖌️
- **対応形式**: SVG, PDF, JPG, PNG, EPS 📂
- **言語**: C (55.7%), PostScript (44.2%), Makefile (0.1%) 💾
- **ライセンス**: オープンソースで自由に使えます！ 🆓

## 🌟 コントリビュート
`yui0/img2vec` に貢献して、プロジェクトをさらにパワーアップ！ 🚀 バグ報告や新機能の提案はGitHubでどうぞ！

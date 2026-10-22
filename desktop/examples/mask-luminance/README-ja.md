# AlphaとLuminanceで同じ画像を抜く

180×80の合成画像だけで作った編集例です。外部ファイルは不要です。
`01-alpha.json`と`02-luminance.json`は同じ画像・配置で、MaskのImage modeが
違います。対応SVGも同梱しています。PNG比較と画面証拠はDの成果物に保存します。

Graphの「Shape mask」を選び、Image modeをAlpha／Luminance (sRGB)へ
切り替えてください。Alphaは素材の不透明度、LuminanceはsRGBの加重輝度に
素材alphaを掛けた値を使います。素材の色自体で元画像を着色しません。

素材の帯は左から次の順です。下側の元画像が不透明な位置での8-bit coverageです。

| 素材 | 素材alpha | Alpha | Luminance |
|---|---:|---:|---:|
| 黒 |255|255|0|
| 白 |255|255|255|
| 灰色 #808080 |255|255|128|
| 赤 |255|255|54|
| 緑 |255|255|182|
| 青 |255|255|18|
| 半透明の白 |128|128|128|
| 半透明の灰色 |128|128|64|
| 完全透明の白 |0|0|0|

元画像の上側の赤はalpha128です。半透明の灰色と重なる部分は、Alphaなら64、
Luminanceなら32になります。Feather σを4などにすると生成したcoverageの
境界だけをぼかし、Invertはその後の補数を使います。Feather0で完全透明の白を
Invertすると、元画像のalphaがそのまま現れます。

Shape / imageを「Remove material green」に替えると、既存Keyerで緑を
抜いた後の素材を利用できます。「Alpha material」のPosition/Sizeで素材を
動かせます。Mask後のappearance Transformは元画像とマスクを一緒に変換します。
閉Pathを接続した場合は従来の幾何マスクとなり、Image mode欄は無効です。

モード編集は1回のUndo/Redoで戻せます。8-bit sRGB CanvasでJSON再読込と
Canvas/SVG/PNGのRGBA一致を確認しています。linear、ICC、AEとの同等性は
主張しません。実OSの保存／再オープン／PNGダイアログ、通常cold reopen、
Luminance固有のnative WebView確認は未検証です。

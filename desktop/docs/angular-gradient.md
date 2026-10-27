# Angular Gradient Fill / 角度グラデーション

Add Node から `角度グラデーション` / `Conic Gradient` で検索できます。
Circle などのパス → Angular Gradient Fill → Output の順で接続します。
既存の単色 Stroke は維持します。今回の対象は Fill のみです。

[編集用JSON](../examples/angular-gradient-wheel.json) と [SVG](../examples/angular-gradient-wheel.svg)。

## 編集項目

- Center: アートボード上の中心座標。
- Angle: 開始角。0°が右、正が時計回り、既定−90°が上。
- Clockwise: true は時計回り、false は反時計回りの色の進行。
- Start Color / Color1 / Color2 / End Color: 初期版は4色。各色は独立したalphaを持ちます。
- Position1 / Position2: 一周に対する中間2色の位置。0 < Position1 < Position2 < 1。

StartとEndは同じ位置で接します。同色なら継ぎ目が連続し、別色なら意図的な段差になります。
色はalphaでpremultiplyしたencoded-sRGB空間で補間します。HSV色相補間ではありません。

## 保存と描画の契約

JSONは中心・角度・方向・4色・位置を保持し、保存／再読込／Undo／Redoが可能です。
SVGに標準のconic paintは使わず、一周720個のベクトル扇形による0.5°刻みの近似をpatternとして出力します。
viewer・SVG・PNGは同じ描画関数を使います。拡大時には角度方向の段階が見える場合があります。
透明色もalphaを保持します。無断のラスタライズはしません。

## 明示的な未対応

任意個数のstop追加／削除、Strokeへの角度塗り、HSV/HSL補間、出力SVG patternの再importは未対応です。
編集を続ける場合はJSONを保存してください。輝度サンプリング系のmodifierは角度塗りをまだ扱わず、明示エラーになります。
Geometry／Transform／Repeaterは原則この塗りの前へ置きます。塗りの座標は絶対アートボード座標です。

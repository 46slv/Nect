# Live Blend: Coral to Indigo

PR #324 / Issue #302 の編集用作例です。対応する Blend 実装が必要です。

1. Nect の **Project → Open JSON** で `live-blend.json` を開きます。
2. `Live Blend · edit Steps` を選び、Steps を変更します。初期値は 7、両端を含めて 9 図形です。
3. `A · Coral circle` の radius、`B · Indigo rectangle` の size / center を変えると、中間形状も追従します。色は `A · Coral` / `B · Indigo` の Fill から変更できます。
4. Undo で戻し、Project → Save As で別名保存すると元の作例を残せます。

A/B と Output は接続済みです。元の円と長方形を保持した live graph であり、焼き込み画像ではありません。`live-blend.svg` は同じ初期状態のベクター出力です。外部画像やフォントは不要です。

単一の閉じた輪郭・solid paint の最初の対応範囲を使っています。限定DistanceとSpine Stepsは実装済みです。対応範囲と別作例は[距離・曲線ガイド](blend-spacing.md)を参照してください。Steps と元図形の変更、Undo、JSON 再読後の SVG 一致、正式 CLI の PNG 出力を確認しました。native ウィンドウでの実操作受入を完了したという意味ではありません。

# 画像の背景色を抜き、矩形で切り抜く

80×80の合成画像を使う編集例です。左半分が緑、右上がalpha128の赤、
右下が不透明の赤。画像・色抜き・マスク枠は別ノードのまま残っています。

- [採色前のJSON](../examples/keyer-mask/01-pick-background.json)：ここから操作します。
- [完成後のJSON](../examples/keyer-mask/02-keyed-mask.json)：緑を抜いた状態です。
- [完成後のSVG](../examples/keyer-mask/02-keyed-mask.svg)：描画結果の確認用です。

接続は `Source image → Remove green (Keyer) → Shape mask (Mask) → Masked output`。
`Mask rectangle (Rectangle)` はMaskの **Shape** に接続済みです。

1. **Project → Open** で採色前のJSONを開き、Graphの **Fit all** で全ノードを表示して
   **Remove green** を選びます。
2. **Key color → Pick from input** を押し、入力プレビュー左半分の緑をクリックします。
   画像座標なら `[20,20]` が目印です。緑が透明になり、右半分の赤だけが残ります。
   画面のcheckerboardではなく、Keyer直前の画像RGBを採っています。
3. **Mask rectangle** を選び、**Position** と **size** を変えると切り抜き範囲が変わります。
   初期値はPosition `[40,40]`、size `[56,56]`。枠の範囲は `[12,12]–[68,68]` です。
4. **Shape mask → Invert** で内外を反転できます。Undoで戻せます。

**Project → Save As** は再編集用JSON、**Export → Export SVG / Export PNG** は描画結果です。
完成後も右上の赤の半透明は保持されます。再編集にはSVGやPNGではなくJSONを使います。
元画像を見直すときは **Source image → Viewer 1/2**、抜け具合はViewerの **Alpha** で確認できます。

## この例で確認した範囲

既存model/evaluatorとheadlessの製品Web版で、入力採色、矩形マスク、
期待alpha、Undo、JSON再読込、SVGとWeb PNGダウンロードの画素一致を確認済みです。
通常OSのOpen/Save/PNG保存ダイアログや、この例のnative実入力は未検証です。
新しいnative GUIプロセスは起動せず、待機中の別試験プロセスには触れません。

生成と検証（desktop内で実行。既存のNode/Edgeを使用。出力先は新規ディレクトリ）：

```powershell
node scripts/keyer-mask-example.js D:\keyer-mask-example-new
node scripts/verify-keyer-mask-example.mjs D:\keyer-mask-example-new
```

合成画像の出典は既存 `tests/image-effects.e2e.js` の緑／赤／alpha試験です。
外部画像やサービスは使いません。Keyer/Mask/採色の製品実装は変更していません。

# 画像のalphaを別の画像のマスクに使う

合成画像だけで作った80×80の編集例です。外部ファイルは不要です。

`01-image-alpha.json`を開き、Graphの「Shape mask」を選びます。
「Shape / image alpha」には「Alpha material」が接続されています。
素材の緑と赤は同じalpha128なので、色が違っても同じ濃さで抜けます。
黒はalpha255なので完全に残り、透明部分は抜けます。元画像の部分的な
alphaも掛け合わされます。

素材を色抜きして使う場合は、同じ入力を「Remove material green」に
切り替えます。このKeyerは緑を抜き、赤のalpha128と黒のalpha255は
保ちます。「Key color」は既存の「Pick from input」でも指定できます。
素材画像そのものと、Keyer後の素材を区別して選んでください。

`02-keyer-feather.json`はKeyerのalphaを使い、Feather σを4文書単位に
した例です。画像の色や内部模様はぼかさず、抜く境界だけを柔らかくします。
Invertで同じalphaを反転できます。対応SVGも同梱しています。

「Alpha material」のPositionとSizeを編集すると、素材の位置と大きさ
だけが変わります。入力は文書座標を共有します。Mask後のappearance
Transformは元画像とマスクを一緒に動かします。Image入力自体に新しい
回転Transform経路は追加していません。残してある「Mask rectangle」へ
切り替えれば、従来の閉Pathマスクにも戻せます。

接続・Feather・Invertの編集は既存Undo/Redoで戻せます。この例はJSON
再読込とSVG→PNGの画素一致をheadless Web版で確認しています。PNGと
画面証拠はDの専用成果フォルダに保存し、Gitには画像binaryを追加して
いません。実OSのOpen/Save/PNGダイアログと通常cold reopenは未検証です。

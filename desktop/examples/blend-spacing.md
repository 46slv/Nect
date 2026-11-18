# Blendを距離と曲線で並べる

PR327/328対応版で、Project → Open JSONから開く編集用の作例です。
外部画像・フォントは不要です。両方ともA/B、Fill、Blend、Outputが接続済みです。

## 同じ形を最大間隔で並べる

[blend-distance.json](blend-distance.json)を開き、`Blend · edit Distance`を選びます。
Distanceは40、移動長は600なので、15区間・両端を含む16図形です。
Distanceを60にすると10区間・11図形になります。

指定値は厳密な間隔ではなく最大間隔です。端数は全区間へ均等配分します。
同じ大きさ・形の平行移動だけが対象です。片方のradiusだけを変更すると診断になります。
形を変えながら補間したい場合は、operationを明示的にStepsへ切り替えてください。
色は`A · Coral`、`B · Indigo`のFillから変更できます。

## 曲線に沿って並べる

[blend-spine.json](blend-spine.json)を開き、`Blend · edit Steps`を選びます。
operationはSpine Steps、Stepsは7なので両端を含む9図形です。Stepsを3にすると5図形です。
`Spine · cubic arch`は単一の開いた三次曲線で、弧長に沿って配置します。
図形は曲線の向きへ回転しません。元図形の基準点が曲線の始点・終点へ移ります。

StepsまたはDistanceへ明示的に切り替えると、spine接続は残りますが配置には使いません。
曲線上のDistance配置、閉じた曲線、複数spineは今回の対象外です。

## 編集を戻して保存する

数値入力を確定し、上部のUndo／Redoボタンを使います。
Project → Save Asで別名保存すれば元作例を残せます。
[Distanceの初期SVG](blend-distance.svg)と[Spineの初期SVG](blend-spine.svg)も付属しています。
JSON再読・SVG一致・パラメータ編集効果をheadlessで検証しています。
この2作例のnativeウィンドウでの操作受入は未確認です。

# 共有色を「値」と「参照」で使い分ける

同じ見た目の2作品を開いて、配色を独立させる場合と連動させる場合を試せます。

- [値を貼り付けた作品](shared-color-value.json)：左のCompositionは元の共有色を参照、右はコピー時の `#cccccc / alpha 1` をローカル値として保持。
- [参照を貼り付けた作品](shared-color-reference.json)：左右のCompositionの A · Coral Fill が、同じ元の共有色を参照。

どちらも左の円から右のIndigo四角へ変形するBlendです。既存の名前「Coral」は残っていますが、円の表示色は薄いグレーです。上部の2つのタブは両方「Composition 1」。左が元作品、右が取り込んだ作品です。

## 開いて編集する

1. JSONをダウンロードし、**Project → Open JSON** で開きます。既存作品を残す場合は先に保存してください。
2. 左の色アイコンから **Shared Colors** を開きます。保存色の行が見えなければ **Advanced** を展開し、パネル内を下へスクロールして **Saved colors** を表示します。
3. 同名の Color 1 が2行あります。先頭（ID末尾 `be40`）が元の共有色、次（`…rt-1`）が取り込み時に作られた独立した色です。同じHEX・同じ名前でも別の色です。
4. 先頭の色のLを少し変えて、左右のCompositionタブを切り替えます。「値」の作品では左だけ、「参照」の作品では左右の円とBlendが変わります。Indigoの端点は変わりません。Ctrl+Zで編集を戻せます。

## 自分の作品で再利用する

保存色の **Actions… → Copy value in Nect** の後、対象FillのColor欄で **Paste value** を押すと、コピー時のHEX/alphaが独立した値になります。既に色を参照していた欄でも、そのリンクを置き換えます。

**Copy reference in Nect → Paste link** は、同じ開いているプロジェクトの色IDを参照します。別Compositionにも使えますが、別プロジェクトへ持ち越せる参照ではありません。これはNect内のコピー機能で、システムクリップボードの貼り付けではありません。

共有色セレクターを **Local color** にすると、現在の表示色を保持してリンクを外します。昔のローカル色へ戻す操作ではありません。接続・式・ロックで管理されたColor欄は対象外です。

**Project → Save As…** で別名保存すれば作例を残せます。編集はCtrl+Z / Ctrl+Shift+Zで戻せますが、保存JSONにUndo履歴は含まれません。

## 作例の範囲

2026-11-18のWindows native操作で保存した2つのJSONを、そのまま収録しています。元と取り込み先のID、独立した2つの共有色を保全しています。自動配色・実験的Key color・linked Captureの作例ではありません。

値/参照貼付、参照貼付のUndo/Redo、保存再読は観測済み。最後のプロセス終了は確認しましたが、終了操作の主体は未確定です。ここでは正常終了手順の新たな受入とは扱いません。仕様の詳細は[Shared colors contract](../docs/shared-colors-panel.md)を参照してください。

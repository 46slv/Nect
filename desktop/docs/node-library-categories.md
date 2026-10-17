# Add-node library categories

The existing Add node picker now offers eight read-only built-in scopes: 作成,
パス編集, 変形, 合成, 配置・反復, 色・描画, 画像・トーン, and 値・制御.
It opens with an empty query and すべて each time. The destination composition,
current scope and result count stay visible. 全体で検索 removes only the scope,
retaining the query; a scoped empty result never silently becomes global.

`src/node-catalog.js` is editor metadata, not document data. Each normally addable
core type has one primary category. New unknown definitions remain visible as
未分類, while the coverage test fails until metadata is supplied. Image, Image
Output and Output remain excluded from generic creation. Join Geometry retains
its stored ID and its Join Paths display label.

Search retains NFKC normalization, case-insensitive AND word matching, and searches
English IDs/labels, Japanese names, aliases and short descriptions. Ranking is
exact name, name prefix, aliases, then remaining matches; ties retain definition
order. Examples: 角丸, 塗り, ベクトル, Join Geometry and Join Paths.

Category selection and candidate highlighting do not change Graph selection,
Inspector, Viewer bindings, Overview, evaluation, document dirty state or Undo.
Click/arrow navigation highlights; Enter in the search field, the Add button or
double-click commits through the existing create-node Session command. Category
Enter, IME composition and repeated Enter do not commit. Closed picker elements
cannot add twice. Escape/Cancel returns focus to the connected opener, with Graph
as fallback if it has disappeared. A changed composition, lock or busy edit state
refuses creation; the captured Session envelope also rejects stale revisions or
replaced documents. Successful creation uses existing selection/reveal and one
Undo entry.

Category controls and search ranking are opt-in options of the shared dialog;
font family/weight/favorite filtering remains separate. No custom folders,
favorites persistence, placed-node groups, Frame, schema or native package changes
are included.

## Verification boundary

`node --test tests/node-catalog.test.js tests/node-picker-dom.test.js` covers
metadata completeness, unknown types, search ranking and aliases, scoped zero
results/reset, highlight preservation, cancellation/focus, synthetic composition
and repeated-key guards, stale elements, font-filter compatibility, real Add
Session/history integration and changed/stale target rejection. Synthetic DOM
checks are not real Japanese IME, WebView, visual layout or scaling acceptance.
Those remain to be checked on the actual host, including 125/150/200% scaling.

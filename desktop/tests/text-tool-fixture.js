// Disposable Note Chrome fixture. Production UI and Session, isolated artwork.
import fs from 'node:fs/promises';
const target = new URL('../test-results/req210/', import.meta.url);
await fs.mkdir(target, { recursive: true });
await fs.writeFile(
  new URL('fixture.html', target),
  `<!doctype html>
<html><head><meta charset="utf-8"><title>REQ-210 fixture</title></head>
<body><div id="app"></div><output id="req210-evidence" hidden></output>
<script type="module">
import '/src/main.js';
import { CommandSession, blankDocument, serialize } from '/src/core.js';
const s = new CommandSession(blankDocument());
const send = (type, payload) => s.dispatch({...s.state, type, payload});
const old = send('create-artwork', {artworkType:'Text',at:[180,180]});
send('set-text-content', {layerId:old.layerId,nodeId:old.nodeId,text:'Existing AB'});
send('set-parameter', {layerId:old.layerId,nodeId:old.nodeId,key:'size',value:38});
const baseline = s.snapshot();
window.nect.load(serialize(baseline));
const publish = () => {
  const d = window.nect.document;
  document.querySelector('#req210-evidence').textContent = JSON.stringify({
    file:window.nect.fileState, command:window.nect.commandState,
    document:d, existingPreserved:JSON.stringify(d.layers.find(l=>l.id===old.layerId))===JSON.stringify(baseline.layers.at(-1)),
    undo:!document.querySelector('[data-action="undo"]').disabled,
    redo:!document.querySelector('[data-action="redo"]').disabled
  });
};
setInterval(publish, 100); publish();
</script></body></html>`,
);

// Generate a disposable real-browser fixture from the current production picker.
// Run from desktop, then open /test-results/toggle-list/short-tail.html on Vite.
import fs from 'node:fs/promises';

const source = await fs.readFile(new URL('../src/main.js', import.meta.url), 'utf8');
const start = source.indexOf('function openSearchPanel(');
const end = source.indexOf('\n}\n', start) + 2;
if (start < 0 || end < start) throw Error('Picker function boundary not found');
const target = new URL('../test-results/toggle-list/', import.meta.url);
await fs.mkdir(target, { recursive: true });
await fs.writeFile(
  new URL('short-tail.html', target),
  `<!doctype html><html><head><meta charset="utf-8"><title>Toggle short-tail fixture</title>
<link rel="stylesheet" href="/src/style.css"></head><body><script type="module">
${source.slice(start, end)}
openSearchPanel({
  title: 'Toggle fixture', action: 'Add',
  categories: [{id:'a',label:'First'},{id:'b',label:'Middle'},{id:'c',label:'Last'}],
  items: [
    {id:'Alpha',label:'Alpha',categoryId:'a'},
    {id:'Beta',label:'Beta',categoryId:'b'},
    {id:'Gamma',label:'Gamma',categoryId:'c'}
  ],
  onCommit: () => { throw Error('Browsing must not commit'); }
});
</script></body></html>`,
);

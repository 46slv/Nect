import { prepareImage } from './raster.js';
import { imageURL } from './images.js';
import { invoke } from '@tauri-apps/api/core';
import { desktop } from './storage.js';
import { readSVGSource, importSVGTree } from './svg-import.js';
import { exportSVG } from './core.js';

const browserRoots = new Map();
let mounted;
let closeMounted;
export const getAssetLibraryPanel = () => mounted || null;
export function closeAssetLibrary() {
  closeMounted?.();
}
const MAX_FILE = 12_000_000;
function relativePath(path) {
  if (
    typeof path !== 'string' ||
    path.length > 2048 ||
    /[\\:\u0000-\u001f]/.test(path) ||
    path.split('/').some((p) => !p || p === '.' || p === '..')
  )
    throw Error('Invalid library relative path');
  return path;
}
export async function svgAssetContent(contents) {
  if (typeof contents !== 'string' || new TextEncoder().encode(contents).length > MAX_FILE)
    throw Error('SVG exceeds 12 MB');
  const source = readSVGSource(contents);
  // Preview only canonical output from the same supported-subset importer.
  // Untrusted original XML never enters innerHTML or an image URL.
  const preview = exportSVG(importSVGTree(source));
  const sha256 = [
    ...new Uint8Array(await crypto.subtle.digest('SHA-256', new TextEncoder().encode(contents))),
  ]
    .map((v) => v.toString(16).padStart(2, '0'))
    .join('');
  return { source, preview, sha256, kind: 'svg' };
}
async function prepareContent(opened, name) {
  const bytes = new Uint8Array(opened.bytes);
  if (name.toLowerCase().endsWith('.svg'))
    return svgAssetContent(new TextDecoder('utf-8', { fatal: true }).decode(bytes));
  const asset = await prepareImage(new File([bytes], name));
  const sha256 = [...new Uint8Array(await crypto.subtle.digest('SHA-256', bytes))]
    .map((v) => v.toString(16).padStart(2, '0'))
    .join('');
  return { asset, preview: imageURL(asset), sha256, kind: 'image' };
}
function element(tag, text, attrs = {}) {
  const el = document.createElement(tag);
  if (text !== undefined) el.textContent = text;
  for (const [key, value] of Object.entries(attrs)) el.setAttribute(key, value);
  return el;
}
function previewImage(svg) {
  return svg.startsWith('data:image/png;')
    ? svg
    : 'data:image/svg+xml;charset=utf-8,' + encodeURIComponent(svg);
}

export function mountAssetLibrary({ capture, insert, onClose = () => {} }) {
  if (mounted) {
    mounted.querySelector('[data-asset-search]').focus();
    return;
  }
  const panel = element('aside', undefined, {
    class: 'asset-library',
    'aria-label': 'Graphic assets',
    'data-asset-library': '',
  });
  mounted = panel;
  let alive = true,
    epoch = 0,
    roots = [],
    listing = null,
    selected = null,
    busy = false,
    shown = 48,
    choiceSequence = 0;
  const prepared = new Map();
  let folderPickIntent = null;
  const header = element('header');
  header.append(element('strong', 'Graphic assets'));
  const close = element('button', 'Close', { 'aria-label': 'Close assets' });
  header.append(close);
  const folders = element('div', undefined, { class: 'asset-library-folders' });
  const rootSelect = element('select', undefined, { 'aria-label': 'Asset folder' });
  const add = element('button', 'Add folder');
  const refresh = element('button', 'Refresh');
  const remove = element('button', 'Remove', { title: 'Unregister folder' });
  folders.append(rootSelect, add, refresh, remove);
  const search = element('input', undefined, {
    type: 'search',
    placeholder: 'Search assets',
    'aria-label': 'Search assets',
    'data-asset-search': '',
  });
  const hierarchy = element('select', undefined, { 'aria-label': 'Asset subfolder' });
  const filters = element('div', undefined, { class: 'asset-library-filters' });
  filters.append(search, hierarchy);
  const status = element('output', '', { 'aria-live': 'polite', 'data-asset-status': '' });
  const warnings = element('details');
  warnings.hidden = true;
  const grid = element('div', undefined, { class: 'asset-library-grid', 'aria-label': 'assets' });
  const more = element('button', 'Show more');
  more.hidden = true;
  const detail = element('div', undefined, { class: 'asset-library-detail' });
  const name = element('strong', '');
  const image = element('img', undefined, { alt: '', class: 'asset-library-preview' });
  const location = element('span', '', { class: 'asset-library-location' });
  const fields = {};
  detail.append(image, name, location);
  for (const label of ['Author', 'Source', 'License']) {
    const row = element('label', label);
    const input = element('input', undefined, {
      'aria-label': `Asset ${label.toLowerCase()}`,
      maxlength: '2048',
      placeholder: label === 'License' ? 'Unspecified' : '',
    });
    row.append(input);
    detail.append(row);
    fields[label.toLowerCase()] = input;
  }
  const joinLabel = element('label', undefined, { class: 'asset-library-join' });
  const join = element('input', undefined, { type: 'checkbox' });
  joinLabel.append(join, document.createTextNode('Join before Out'));
  const place = element('button', 'Place in new layer', { 'data-asset-place-layer': '' });
  const graph = element('button', 'Add to this graph', { 'data-asset-place-graph': '' });
  const actions = element('div', undefined, { class: 'asset-library-actions' });
  actions.append(place, graph, joinLabel);
  detail.append(actions);
  detail.hidden = true;
  const picker = element('input', undefined, {
    type: 'file',
    webkitdirectory: '',
    multiple: '',
    'aria-label': 'Choose asset folder',
    'data-asset-folder-input': '',
  });
  picker.hidden = true;
  panel.append(header, folders, filters, status, warnings, grid, more, detail, picker);
  document.body.append(panel);
  function message(text, error = false) {
    status.textContent = text;
    status.dataset.error = String(error);
  }
  function buttons() {
    grid.inert = busy;
    search.disabled = hierarchy.disabled = more.disabled = busy;
    for (const field of Object.values(fields)) field.disabled = busy;
    place.disabled = busy || !selected || !!selected.error;
    graph.disabled = place.disabled || selected?.kind === 'image';
    join.disabled = graph.disabled;
    add.disabled = refresh.disabled = remove.disabled = busy;
    refresh.disabled ||= !roots.length;
    remove.disabled ||= !roots.length;
    rootSelect.disabled = busy || !roots.length;
  }
  function closePanel() {
    if (!alive) return;
    alive = false;
    epoch++;
    panel.remove();
    mounted = null;
    closeMounted = null;
    document.removeEventListener('keydown', keydown);
    onClose();
  }
  closeMounted = closePanel;
  function keydown(e) {
    if (
      e.key === 'Escape' &&
      !e.defaultPrevented &&
      !e.isComposing &&
      panel.contains(e.target) &&
      !document.querySelector('dialog[open]')
    ) {
      e.preventDefault();
      closePanel();
    }
  }
  close.onclick = closePanel;
  document.addEventListener('keydown', keydown);
  function drawRoots(id = rootSelect.value) {
    rootSelect.replaceChildren();
    for (const r of roots) rootSelect.append(element('option', r.name, { value: r.id }));
    if (roots.some((r) => r.id === id)) rootSelect.value = id;
    buttons();
  }
  async function open(item) {
    if (desktop)
      return invoke('open_asset_source', {
        rootId: listing.root.id,
        relativePath: item.relativePath,
        generation: listing.generation,
      });
    const r = browserRoots.get(listing.root.id),
      file = r?.files.get(item.relativePath);
    if (!file) throw Error('Asset source is missing; refresh the folder');
    return {
      bytes: new Uint8Array(await file.arrayBuffer()),
      sourceLocation: `${r.name}/${item.relativePath}`,
    };
  }
  async function prepare(item) {
    if (prepared.has(item.relativePath)) return prepared.get(item.relativePath);
    const promise = (async () => {
      try {
        const opened = await open(item);
        return { ...(await prepareContent(opened, item.relativePath)), ...opened };
      } catch (error) {
        return { error: error.message };
      }
    })();
    prepared.set(item.relativePath, promise);
    return promise;
  }
  async function choose(item, token) {
    const choice = ++choiceSequence;
    selected = null;
    detail.hidden = true;
    buttons();
    const value = await prepare(item);
    if (!alive || token !== epoch || choice !== choiceSequence) return;
    selected = { ...item, ...value };
    for (const field of Object.values(fields)) field.value = '';
    detail.hidden = false;
    name.textContent = item.relativePath;
    location.textContent = value.sourceLocation || '';
    image.hidden = !!value.error;
    if (value.preview) image.src = previewImage(value.preview);
    else image.removeAttribute('src');
    for (const button of grid.querySelectorAll('button'))
      button.setAttribute('aria-pressed', String(button.dataset.path === item.relativePath));
    message(value.error || `${item.bytes.toLocaleString()} bytes`, !!value.error);
    buttons();
  }
  function drawItems() {
    const token = ++epoch;
    selected = null;
    detail.hidden = true;
    buttons();
    grid.replaceChildren();
    const query = search.value.trim().toLocaleLowerCase();
    const items = (listing?.items || []).filter(
      (i) =>
        i.relativePath.toLocaleLowerCase().includes(query) &&
        (!hierarchy.value || i.relativePath.startsWith(hierarchy.value)),
    );
    const visible = items.slice(0, shown);
    more.hidden = items.length <= shown;
    message(
      `${items.length} assets${listing?.warnings?.length ? ` · ${listing.warnings.length} warnings` : ''}`,
    );
    // Four workers bound XML/geometry and file IPC in-flight work.
    let next = 0;
    const entries = visible.map((item) => {
      const button = element('button', undefined, {
        'aria-label': item.relativePath,
        'aria-pressed': 'false',
        'data-path': item.relativePath,
      });
      const img = element('img', undefined, { alt: '', loading: 'lazy' });
      const label = element('span', item.relativePath);
      const state = element(
        'small',
        item.relativePath.toLowerCase().endsWith('.svg') ? 'SVG' : 'Image',
      );
      button.append(img, label, state);
      button.onclick = () => choose(item, token);
      grid.append(button);
      return { item, img, state };
    });
    async function worker() {
      while (alive && token === epoch && next < entries.length) {
        const { item, img, state } = entries[next++];
        const value = await prepare(item);
        if (!alive || token !== epoch) return;
        if (value.error) {
          state.textContent = 'Unsupported';
          state.title = value.error;
          img.hidden = true;
        } else img.src = previewImage(value.preview);
      }
    }
    for (let i = 0; i < 4; i++) void worker();
  }
  function acceptListing(value) {
    listing = value;
    warnings.replaceChildren(element('summary', `Folder warnings (${listing.warnings.length})`));
    for (const warning of listing.warnings) warnings.append(element('div', warning));
    warnings.hidden = listing.warnings.length === 0;
    prepared.clear();
    shown = 48;
    hierarchy.replaceChildren(element('option', 'All subfolders', { value: '' }));
    const paths = new Set();
    for (const item of listing.items) {
      const parts = item.relativePath.split('/');
      parts.pop();
      for (let i = 1; i <= parts.length; i++) paths.add(parts.slice(0, i).join('/') + '/');
    }
    for (const p of [...paths].sort()) hierarchy.append(element('option', p, { value: p }));
    drawItems();
  }
  async function refreshRoot() {
    const id = rootSelect.value;
    epoch++;
    selected = null;
    detail.hidden = true;
    prepared.clear();
    grid.replaceChildren();
    listing = null;
    warnings.hidden = true;
    if (!id) {
      message('Add an asset folder');
      buttons();
      return;
    }
    busy = true;
    buttons();
    message('Reading folder…');
    const token = epoch;
    try {
      const value = desktop
        ? await invoke('refresh_asset_folder', { rootId: id })
        : browserRoots.get(id);
      if (!alive || token !== epoch) return;
      acceptListing(
        desktop
          ? value
          : {
              root: value,
              generation: 'browser',
              items: [...value.files].map(([relativePath, file]) => ({
                relativePath,
                bytes: file.size,
              })),
              warnings: [],
            },
      );
    } catch (error) {
      if (alive) message(String(error.message || error), true);
    } finally {
      busy = false;
      if (alive) buttons();
    }
  }
  async function addFolder() {
    if (!desktop) {
      folderPickIntent = { mode: 'add' };
      picker.value = '';
      picker.click();
      return;
    }
    busy = true;
    buttons();
    const token = ++epoch;
    try {
      const result = await invoke('register_asset_folder');
      if (!alive || token !== epoch) return;
      if (!result) {
        message('Folder selection cancelled');
        return;
      }
      if (
        !Array.isArray(result.roots) ||
        !result.roots.some((root) => root.id === result.selectedRootId)
      )
        throw Error('Selected folder is unavailable');
      roots = result.roots;
      drawRoots(result.selectedRootId);
      await refreshRoot();
    } catch (error) {
      if (alive) message(String(error.message || error), true);
    } finally {
      busy = false;
      if (alive) buttons();
    }
  }
  picker.onchange = async () => {
    const intent = folderPickIntent;
    folderPickIntent = null;
    try {
      if (!alive) return;
      if (!intent) throw Error('Choose Add folder or Refresh first');
      if (!picker.files.length) {
        message(
          intent.mode === 'refresh'
            ? 'Catalog unchanged. Remove folder to clear.'
            : 'Folder selection unchanged',
        );
        return;
      }
      if (picker.files.length > 2048) throw Error('Folder exceeds 2048 entries');
      const previous = intent.mode === 'refresh' ? browserRoots.get(intent.rootId) : null;
      if (intent.mode === 'refresh' && !previous) throw Error('Folder was removed; add it again');
      if (!previous && browserRoots.size >= 16) throw Error('At most 16 library folders');
      const files = new Map();
      let total = 0;
      let folderName;
      for (const file of picker.files) {
        const parts = file.webkitRelativePath.split('/');
        if (!folderName) folderName = parts[0];
        if (parts.shift() !== folderName || parts.length > 9)
          throw Error('Choose one folder with at most 8 nested directories');
        const relative = relativePath(parts.join('/'));
        if (!/\.(svg|png|jpe?g)$/i.test(relative)) continue;
        if (file.size > MAX_FILE) throw Error(`${relative} exceeds 12 MB`);
        total += file.size;
        if (total > 32_000_000) throw Error('Folder asset content exceeds 32 MB');
        files.set(relative, file);
      }
      if (previous && previous.name !== folderName)
        throw Error(`Choose ${previous.name} to refresh this folder`);
      const id = previous?.id || 'browser-' + crypto.randomUUID();
      const root = { id, name: folderName, files };
      browserRoots.set(id, root);
      roots = [...browserRoots.values()];
      drawRoots(id);
      await refreshRoot();
    } catch (error) {
      message(error.message, true);
    }
  };
  picker.addEventListener('cancel', () => {
    const refreshing = folderPickIntent?.mode === 'refresh';
    folderPickIntent = null;
    if (alive)
      message(
        refreshing ? 'Catalog unchanged. Remove folder to clear.' : 'Folder selection unchanged',
      );
  });
  add.onclick = () => void addFolder();
  refresh.onclick = () => {
    if (desktop) void refreshRoot();
    else {
      folderPickIntent = { mode: 'refresh', rootId: rootSelect.value };
      picker.value = '';
      picker.click();
    }
  };
  remove.onclick = async () => {
    busy = true;
    buttons();
    try {
      const id = rootSelect.value;
      if (desktop) roots = await invoke('remove_asset_folder', { rootId: id });
      else {
        browserRoots.delete(id);
        roots = [...browserRoots.values()];
      }
      drawRoots();
      await refreshRoot();
    } catch (error) {
      message(String(error.message || error), true);
    } finally {
      busy = false;
      if (alive) buttons();
    }
  };
  rootSelect.onchange = () => void refreshRoot();
  search.oninput = () => {
    shown = 48;
    drawItems();
  };
  hierarchy.onchange = () => {
    shown = 48;
    drawItems();
  };
  more.onclick = () => {
    shown += 48;
    drawItems();
  };
  async function placeAsset(route) {
    if (!selected || selected.error || busy) return;
    const item = selected,
      root = listing.root,
      request = capture(),
      token = epoch;
    busy = true;
    buttons();
    message('Reading asset…');
    try {
      // Never place cached thumbnail bytes. Reopen through the exact refresh
      // ticket and compare content, then guard the original Session envelope.
      const opened = await open(item);
      const value = await prepareContent(opened, item.relativePath);
      if (!alive || token !== epoch) return;
      if (value.sha256 !== item.sha256) throw Error('Asset changed; refresh the library folder');
      const provenance = {
        version: 1,
        mode: 'embedded',
        rootId: root.id,
        relativePath: item.relativePath,
        sourceLocation: opened.sourceLocation,
        sha256: value.sha256,
        author: fields.author.value,
        source: fields.source.value,
        license: fields.license.value,
      };
      insert(request, {
        route,
        source: value.source,
        asset: value.asset,
        name: item.relativePath.split('/').at(-1),
        provenance,
        joinBeforeOut: route === 'graph' && join.checked,
      });
      message('Asset added');
    } catch (error) {
      if (alive) message(String(error.message || error), true);
    } finally {
      busy = false;
      if (alive) buttons();
    }
  }
  place.onclick = () => void placeAsset('layer');
  graph.onclick = () => void placeAsset('graph');
  void (async () => {
    try {
      roots = desktop ? await invoke('list_asset_folders') : [...browserRoots.values()];
      if (!alive) return;
      drawRoots();
      await refreshRoot();
    } catch (error) {
      message(String(error.message || error), true);
    }
  })();
  buttons();
  search.focus();
  return panel;
}

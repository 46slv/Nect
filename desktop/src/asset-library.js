import { prepareImage } from './raster.js';
import { imageURL } from './images.js';
import { invoke } from '@tauri-apps/api/core';
import { desktop } from './storage.js';
import { readSVGSource, importSVGTree } from './svg-import.js';
import { exportSVG } from './core.js';

const browserRoots = new Map();
// Browser directory access and its opaque IDs last only for this page session.
const browserFavorites = new Set();
const browserAssetFavorites = new Map();
export const ASSET_FOLDER_FAVORITES_KEY = 'nect.asset-folder-favorites.v1';
export const ASSET_ITEM_FAVORITES_KEY = 'nect.asset-item-favorites.v1';
const MAX_FAVORITES = 128;
// Presentation only: root IDs still own registration, refresh and placement.
const rootDisplayNumbers = new Map();
let nextRootDisplayNumber = 1;
let mounted;
let closeMounted;
const composingInputs = new WeakSet();
export const getAssetLibraryPanel = () => mounted || null;
export function canDeactivateAssetLibrary() {
  const active = mounted?.ownerDocument.activeElement;
  return !(active && mounted.contains(active) && composingInputs.has(active));
}
export function closeAssetLibrary() {
  if (canDeactivateAssetLibrary()) closeMounted?.();
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
function assetFavoriteKey(rootId, path) {
  return JSON.stringify([rootId, path]);
}
function validAssetFavorite(value) {
  if (
    !value ||
    typeof value !== 'object' ||
    Array.isArray(value) ||
    typeof value.rootId !== 'string' ||
    !value.rootId ||
    value.rootId.length > 256 ||
    /[\u0000-\u001f]/.test(value.rootId)
  )
    return false;
  try {
    relativePath(value.relativePath);
    return true;
  } catch {
    return false;
  }
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

export function mountAssetLibrary({
  capture,
  insert,
  resolveDrop,
  onClose = () => {},
  onDragState = () => {},
  focus = true,
  initiallyHidden = false,
}) {
  if (mounted) {
    if (focus) mounted.querySelector('[data-asset-search]').focus();
    return;
  }
  const panel = element('aside', undefined, {
    class: 'asset-library',
    'aria-label': 'Graphic assets',
    'data-asset-library': '',
  });
  panel.hidden = initiallyHidden;
  mounted = panel;
  let alive = true,
    epoch = 0,
    folderEpoch = 0,
    folderPickerPending = false,
    pendingBootstrap = null,
    folderRegistrationError = null,
    roots = [],
    currentRootId = '',
    listing = null,
    selected = null,
    busy = false,
    searchDuringRefresh = false,
    refreshSequence = 0,
    checkingRoots = true,
    favorites = [],
    assetFavorites = [],
    favoriteStorage = null,
    favoriteStorageError = null,
    favoriteReadFailed = false,
    assetFavoriteStorageError = null,
    assetFavoriteReadFailed = false,
    shown = 48,
    choiceSequence = 0;
  const failedRoots = new Set();
  const failedAssets = new Set();
  if (desktop) {
    try {
      favoriteStorage = panel.ownerDocument.defaultView.localStorage;
      const stored = favoriteStorage.getItem(ASSET_FOLDER_FAVORITES_KEY);
      const saved = stored === null ? [] : JSON.parse(stored);
      if (
        !Array.isArray(saved) ||
        saved.length > MAX_FAVORITES ||
        saved.some(
          (id) => typeof id !== 'string' || !id || id.length > 256 || /[\u0000-\u001f]/.test(id),
        ) ||
        new Set(saved).size !== saved.length
      )
        throw Error('Invalid Quick access settings');
      favorites = saved;
    } catch {
      favoriteReadFailed = true;
      favoriteStorageError = 'Quick access could not be loaded';
    }
    try {
      favoriteStorage ||= panel.ownerDocument.defaultView.localStorage;
      const stored = favoriteStorage.getItem(ASSET_ITEM_FAVORITES_KEY);
      const saved = stored === null ? [] : JSON.parse(stored);
      if (
        !Array.isArray(saved) ||
        saved.length > MAX_FAVORITES ||
        saved.some((entry) => !validAssetFavorite(entry)) ||
        new Set(saved.map((entry) => assetFavoriteKey(entry.rootId, entry.relativePath))).size !==
          saved.length
      )
        throw Error('Invalid asset Quick access settings');
      assetFavorites = saved.map(({ rootId, relativePath: path }) => ({
        rootId,
        relativePath: path,
      }));
    } catch {
      assetFavoriteReadFailed = true;
      assetFavoriteStorageError = 'Asset Quick access could not be loaded';
    }
  } else {
    favorites = [...browserFavorites];
    assetFavorites = [...browserAssetFavorites.values()].map((entry) => ({ ...entry }));
  }
  const prepared = new Map();
  let dragTicket = null;
  const dragType = 'application/x-nect-library-ticket';
  let folderPickIntent = null;
  let pointerSequence = 0;
  let assetFocusRequest = null;
  panel.addEventListener('compositionstart', (event) => composingInputs.add(event.target), true);
  panel.addEventListener('compositionend', (event) => composingInputs.delete(event.target), true);
  function guardComposingTransition(event) {
    if (event.type === 'pointerdown') pointerSequence++;
    if (canDeactivateAssetLibrary()) return;
    const target = event.target;
    if (
      target === panel.ownerDocument.activeElement ||
      (!panel.contains(target) &&
        !target.closest?.(
          '[data-workspace-panel],[data-viewer-mode="viewers"],[data-action="align-panel"],[data-action="asset-library"]',
        ))
    )
      return;
    event.preventDefault();
    event.stopImmediatePropagation();
  }
  document.addEventListener('pointerdown', guardComposingTransition, true);
  document.addEventListener('click', guardComposingTransition, true);
  const header = element('header');
  header.append(element('strong', 'Graphic assets'));
  const close = element('button', 'Close', { 'aria-label': 'Close assets' });
  header.append(close);
  const folders = element('div', undefined, { class: 'asset-library-folders' });
  const rootSelect = element('select', undefined, { 'aria-label': 'Asset folder' });
  const add = element('button', 'Add folder');
  const refresh = element('button', 'Refresh');
  const remove = element('button', 'Remove', { title: 'Unregister folder' });
  const pin = element('button', 'Pin folder', { 'aria-pressed': 'false' });
  folders.append(rootSelect, add, refresh, remove, pin);
  const quick = element('div', undefined, { role: 'group', 'aria-label': 'Quick access' });
  // Keep a long list from consuming the catalog at narrow workspace widths.
  Object.assign(quick.style, {
    display: 'none',
    alignItems: 'center',
    flex: '0 0 auto',
    gap: '6px',
    maxWidth: '100%',
    minWidth: '0',
    overflowX: 'auto',
  });
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
  const metadata = element('details', undefined, { class: 'asset-library-metadata' });
  metadata.append(element('summary', 'Asset details'));
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
    metadata.append(row);
    fields[label.toLowerCase()] = input;
  }
  const joinLabel = element('label', undefined, { class: 'asset-library-join' });
  const join = element('input', undefined, { type: 'checkbox' });
  joinLabel.append(join, document.createTextNode('Join before Out'));
  const pinAsset = element('button', 'Pin asset', { 'aria-pressed': 'false' });
  const place = element('button', 'Place in new layer', { 'data-asset-place-layer': '' });
  const graph = element('button', 'Add to this graph', { 'data-asset-place-graph': '' });
  const actions = element('div', undefined, { class: 'asset-library-actions' });
  actions.append(pinAsset, place, graph, joinLabel);
  detail.append(actions, metadata);
  detail.hidden = true;
  const picker = element('input', undefined, {
    type: 'file',
    webkitdirectory: '',
    multiple: '',
    'aria-label': 'Choose asset folder',
    'data-asset-folder-input': '',
  });
  picker.hidden = true;
  panel.append(header, folders, quick, filters, status, warnings, grid, more, detail, picker);
  document.body.append(panel);
  function message(text, error = false) {
    // Automatic bootstrap/refresh must not erase a failed registration. A
    // subsequent explicit interaction clears it and can report its own state.
    const persistent = [
      folderRegistrationError,
      favoriteStorageError,
      assetFavoriteStorageError,
    ].filter(Boolean);
    if (persistent.length) {
      if (error && !persistent.includes(text)) persistent.push(text);
      text = persistent.join(' · ');
      error = true;
    }
    status.textContent = text;
    status.dataset.error = String(error);
  }
  function buttons() {
    const choosingFolder = folderPickIntent !== null;
    const blocked = busy || choosingFolder;
    grid.inert = blocked;
    search.disabled = choosingFolder || (busy && !searchDuringRefresh);
    hierarchy.disabled = more.disabled = blocked;
    for (const field of Object.values(fields)) field.disabled = blocked;
    place.disabled = blocked || !selected || !!selected.error;
    graph.disabled = place.disabled;
    join.disabled = graph.disabled || selected?.kind === 'image';
    add.disabled = refresh.disabled = remove.disabled = blocked;
    refresh.disabled ||= !roots.length;
    remove.disabled ||= !roots.length;
    rootSelect.disabled = blocked || !roots.length;
    const rootId = rootSelect.value;
    pin.disabled = blocked || favoriteReadFailed || !roots.some((root) => root.id === rootId);
    pin.textContent = favorites.includes(rootId) ? 'Unpin folder' : 'Pin folder';
    pin.setAttribute('aria-pressed', String(favorites.includes(rootId)));
    const selectedRef =
      selected && listing?.root?.id === rootId
        ? { rootId, relativePath: selected.relativePath }
        : null;
    const assetKey = validAssetFavorite(selectedRef)
      ? assetFavoriteKey(rootId, selected.relativePath)
      : null;
    const assetPinned =
      assetKey !== null &&
      assetFavorites.some(
        (entry) => assetFavoriteKey(entry.rootId, entry.relativePath) === assetKey,
      );
    pinAsset.disabled =
      blocked || assetFavoriteReadFailed || !assetKey || (!!selected?.error && !assetPinned);
    pinAsset.textContent = assetPinned ? 'Unpin asset' : 'Pin asset';
    pinAsset.setAttribute('aria-pressed', String(assetPinned));
    for (const button of quick.querySelectorAll('button')) {
      button.disabled =
        blocked ||
        (button.hasAttribute('data-quick-remove') && favoriteReadFailed) ||
        (button.hasAttribute('data-quick-remove-asset') && assetFavoriteReadFailed);
      if (button.hasAttribute('data-quick-open')) {
        const active = button.dataset.quickOpen === rootId;
        button.setAttribute('aria-pressed', String(active));
        button.style.borderColor = active ? '#326aa2' : '';
        button.style.backgroundColor = active ? '#e8f1fa' : '';
        button.style.boxShadow = active ? 'inset 0 0 0 1px #326aa2' : '';
      }
      if (button.hasAttribute('data-quick-asset')) {
        const active = button.dataset.quickAsset === assetKey;
        button.setAttribute('aria-pressed', String(active));
        button.style.borderColor = active ? '#326aa2' : '';
        button.style.backgroundColor = active ? '#e8f1fa' : '';
      }
    }
  }
  function closePanel() {
    if (!alive) return;
    alive = false;
    epoch++;
    panel.remove();
    mounted = null;
    closeMounted = null;
    document.removeEventListener('keydown', keydown);
    document.removeEventListener('pointerdown', guardComposingTransition, true);
    document.removeEventListener('click', guardComposingTransition, true);
    document.removeEventListener('dragover', dragover);
    document.removeEventListener('drop', drop);
    dragTicket = null;
    onDragState(false);
    onClose();
  }
  closeMounted = closePanel;
  function keydown(e) {
    if (!canDeactivateAssetLibrary()) {
      if (e.key === 'Tab' && !e.defaultPrevented) {
        e.preventDefault();
        e.stopImmediatePropagation();
      }
      return;
    }
    // Native Escape cancels an in-flight drag; it must not destroy the source panel.
    if (e.key === 'Escape' && dragTicket) return;
    if (
      e.key === 'Escape' &&
      !e.defaultPrevented &&
      !e.isComposing &&
      e.keyCode !== 229 &&
      panel.contains(e.target) &&
      !document.querySelector('dialog[open]')
    ) {
      e.preventDefault();
      closePanel();
    }
  }
  close.onclick = closePanel;
  document.addEventListener('keydown', keydown);
  function drawFavorites() {
    if (!alive) return;
    const focused = panel.ownerDocument.activeElement;
    const focusedAction = ['quickOpen', 'quickRemove', 'quickAsset', 'quickRemoveAsset'].find(
      (action) => focused?.dataset?.[action] !== undefined,
    );
    const focusedId = focusedAction && focused.dataset[focusedAction];
    quick.replaceChildren();
    quick.style.display = favorites.length || assetFavorites.length ? 'flex' : 'none';
    if (favorites.length || assetFavorites.length) {
      const label = element('span', 'Pinned');
      label.style.whiteSpace = 'nowrap';
      quick.append(label);
    }
    for (const id of favorites) {
      const root = roots.find((entry) => entry.id === id);
      const option = [...rootSelect.options].find((entry) => entry.value === id);
      const label = root
        ? option?.textContent || root.name
        : `${checkingRoots ? 'Checking' : 'Unavailable'} folder ${id}`;
      if (!root) {
        const unpin = element('button', undefined, {
          'data-quick-remove': id,
          'aria-label': `Remove ${label} from Quick access`,
          title: `Remove ${label} from Quick access`,
        });
        Object.assign(unpin.style, {
          display: 'flex',
          alignItems: 'center',
          flex: '0 0 auto',
          gap: '6px',
          maxWidth: 'min(13rem, calc(100% - 50px))',
        });
        const missingLabel = element('span', label);
        Object.assign(missingLabel.style, {
          minWidth: '0',
          overflow: 'hidden',
          textOverflow: 'ellipsis',
          whiteSpace: 'nowrap',
        });
        unpin.append(missingLabel, element('span', '×', { 'aria-hidden': 'true' }));
        unpin.onclick = () => changeFavorites(favorites.filter((entry) => entry !== id));
        quick.append(unpin);
        continue;
      }
      const openButton = element('button', label, {
        'data-quick-open': id,
        'aria-label': `Open ${label}`,
        'aria-pressed': String(rootSelect.value === id),
        title: root?.path || id,
      });
      if (failedRoots.has(id)) {
        openButton.textContent = `${label} (Unavailable)`;
        openButton.setAttribute('aria-label', `Open ${label} (unavailable)`);
      }
      Object.assign(openButton.style, {
        flex: '0 0 auto',
        maxWidth: '13rem',
        overflow: 'hidden',
        textOverflow: 'ellipsis',
        whiteSpace: 'nowrap',
      });
      openButton.onclick = () => {
        if (!alive || busy || folderPickIntent || !roots.some((entry) => entry.id === id)) return;
        folderRegistrationError = null;
        rootSelect.value = id;
        buttons();
        void refreshRoot();
      };
      quick.append(openButton);
    }
    for (const [index, entry] of assetFavorites.entries()) {
      const key = assetFavoriteKey(entry.rootId, entry.relativePath);
      const root = roots.find((item) => item.id === entry.rootId);
      const option = [...rootSelect.options].find((item) => item.value === entry.rootId);
      const context = `${root ? option?.textContent || root.name : entry.rootId} / ${entry.relativePath}`;
      const filename = entry.relativePath.split('/').at(-1);
      const label = `${assetFavorites.length > 1 ? `${index + 1} · ` : ''}${filename}`;
      if (!root || failedRoots.has(entry.rootId) || failedAssets.has(key)) {
        const state = !root && checkingRoots ? 'Checking' : 'Unavailable';
        const unpin = element('button', `${state} ${label} ×`, {
          'data-quick-remove-asset': key,
          'aria-label': `Remove ${state} ${context} from Quick access`,
          title: `Remove ${state} ${context} from Quick access`,
        });
        Object.assign(unpin.style, {
          flex: '0 0 auto',
          maxWidth: 'min(13rem, calc(100% - 50px))',
          overflow: 'hidden',
          textOverflow: 'ellipsis',
          whiteSpace: 'nowrap',
        });
        unpin.onclick = () => changeAssetFavorites(assetFavorites.filter((item) => item !== entry));
        quick.append(unpin);
        continue;
      }
      const openButton = element('button', label, {
        'data-quick-asset': key,
        'aria-label': `Open ${context}`,
        'aria-pressed': 'false',
        title: context,
      });
      Object.assign(openButton.style, {
        flex: '0 0 auto',
        maxWidth: '13rem',
        overflow: 'hidden',
        textOverflow: 'ellipsis',
        whiteSpace: 'nowrap',
      });
      openButton.onclick = () =>
        void openAssetFavorite(entry, panel.ownerDocument.activeElement === openButton);
      quick.append(openButton);
    }
    buttons();
    const focusInterrupted =
      assetFocusRequest !== null &&
      assetFocusRequest.key === focusedId &&
      assetFocusRequest.pointerAtStart !== pointerSequence;
    if (focusedId && !focusInterrupted) {
      const replacement = [...quick.querySelectorAll('button')].find(
        (button) => button.dataset[focusedAction] === focusedId && !button.disabled,
      );
      if (replacement) replacement.focus({ preventScroll: true });
      else if (!pin.disabled) pin.focus({ preventScroll: true });
    }
  }
  function changeFavorites(next) {
    if (!alive || busy || folderPickIntent || favoriteReadFailed) return;
    folderRegistrationError = null;
    if (next.length > MAX_FAVORITES) {
      message('Quick access is full; remove an unavailable folder first', true);
      return;
    }
    try {
      if (desktop) favoriteStorage.setItem(ASSET_FOLDER_FAVORITES_KEY, JSON.stringify(next));
      else {
        browserFavorites.clear();
        for (const id of next) browserFavorites.add(id);
      }
    } catch {
      favoriteStorageError = 'Quick access could not be saved';
      message(favoriteStorageError, true);
      return;
    }
    favorites = next;
    favoriteStorageError = null;
    drawFavorites();
    message('Quick access updated');
  }
  function changeAssetFavorites(next) {
    if (!alive || busy || folderPickIntent || assetFavoriteReadFailed) return;
    if (next.length > MAX_FAVORITES) {
      message('Asset Quick access is full', true);
      return;
    }
    if (next.some((entry) => !validAssetFavorite(entry))) {
      message('Asset cannot be pinned', true);
      return;
    }
    try {
      if (desktop) favoriteStorage.setItem(ASSET_ITEM_FAVORITES_KEY, JSON.stringify(next));
      else {
        browserAssetFavorites.clear();
        for (const entry of next)
          browserAssetFavorites.set(assetFavoriteKey(entry.rootId, entry.relativePath), {
            ...entry,
          });
      }
    } catch {
      assetFavoriteStorageError = 'Asset Quick access could not be saved';
      message(assetFavoriteStorageError, true);
      return;
    }
    assetFavorites = next;
    assetFavoriteStorageError = null;
    drawFavorites();
    message('Quick access updated');
  }
  pinAsset.onclick = () => {
    if (!alive || busy || folderPickIntent || !selected || !listing) return;
    const entry = { rootId: listing.root.id, relativePath: selected.relativePath };
    if (!validAssetFavorite(entry) || rootSelect.value !== entry.rootId) return;
    const key = assetFavoriteKey(entry.rootId, entry.relativePath);
    const pinned = assetFavorites.some(
      (item) => assetFavoriteKey(item.rootId, item.relativePath) === key,
    );
    if (selected.error && !pinned) return;
    changeAssetFavorites(
      pinned
        ? assetFavorites.filter((item) => assetFavoriteKey(item.rootId, item.relativePath) !== key)
        : [...assetFavorites, entry],
    );
  };
  pin.onclick = () => {
    if (!alive || busy || folderPickIntent) return;
    const id = rootSelect.value;
    if (!roots.some((root) => root.id === id)) return;
    changeFavorites(
      favorites.includes(id) ? favorites.filter((entry) => entry !== id) : [...favorites, id],
    );
  };
  function drawRoots(id = rootSelect.value) {
    if (!alive) return;
    const ids = new Set(roots.map((root) => root.id));
    for (const key of rootDisplayNumbers.keys()) if (!ids.has(key)) rootDisplayNumbers.delete(key);
    const names = roots.map((root) =>
      String(root.name ?? '')
        .normalize('NFC')
        .replace(/\s+/g, ' ')
        .trim(),
    );
    const ambiguous = new Set(names).size !== roots.length;
    rootSelect.replaceChildren();
    for (const r of roots) {
      if (!rootDisplayNumbers.has(r.id)) rootDisplayNumbers.set(r.id, nextRootDisplayNumber++);
      const path = typeof r.path === 'string' ? r.path : '';
      const label = ambiguous
        ? `${rootDisplayNumbers.get(r.id)}. ${r.name}${path ? ' — ' + path : ''}`
        : r.name;
      rootSelect.append(element('option', label, { value: r.id, title: path || label }));
    }
    const selectedId = roots.some((r) => r.id === id) ? id : roots[0]?.id;
    if (selectedId !== undefined) rootSelect.value = selectedId;
    currentRootId = selectedId ?? '';
    drawFavorites();
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
    folderRegistrationError = null;
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
    if (!listing) {
      more.hidden = true;
      return;
    }
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
      return { item, img, state, button };
    });
    async function worker() {
      while (alive && token === epoch && next < entries.length) {
        const { item, img, state, button } = entries[next++];
        const value = await prepare(item);
        if (!alive || token !== epoch) return;
        if (value.error) {
          state.textContent = 'Unsupported';
          state.title = value.error;
          img.hidden = true;
        } else {
          img.src = previewImage(value.preview);
          img.draggable = false;
          button.draggable = !!resolveDrop;
          if (resolveDrop)
            button.title =
              'Drag to the canvas for a new layer, or to the graph for an unconnected node';
          button.ondragstart = (event) => {
            if (busy || token !== epoch || !alive || !event.dataTransfer) {
              event.preventDefault();
              return;
            }
            dragTicket = {
              id: crypto.randomUUID(),
              item: { ...item, ...value },
              request: capture(),
              epoch: token,
              metadata:
                selected?.relativePath === item.relativePath
                  ? Object.fromEntries(
                      Object.entries(fields).map(([key, field]) => [key, field.value]),
                    )
                  : { author: '', source: '', license: '' },
            };
            event.dataTransfer.setData(dragType, dragTicket.id);
            event.dataTransfer.effectAllowed = 'copy';
            // Defer until native dragstart has returned; keep the source node alive.
            requestAnimationFrame(() => {
              if (alive && dragTicket) onDragState(true);
            });
          };
          button.ondragend = () => {
            dragTicket = null;
            onDragState(false);
          };
        }
      }
    }
    for (let i = 0; i < 4; i++) void worker();
  }
  function acceptListing(value) {
    listing = value;
    const presentPaths = new Set(listing.items.map((item) => item.relativePath));
    for (const entry of assetFavorites)
      if (entry.rootId === listing.root.id && presentPaths.has(entry.relativePath))
        failedAssets.delete(assetFavoriteKey(entry.rootId, entry.relativePath));
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
    if (!alive || folderPickIntent) return;
    const id = rootSelect.value;
    currentRootId = id;
    const refreshRequest = ++refreshSequence;
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
    // Folder discovery can settle while the user is already typing. Keep that
    // field focused and apply its latest query when the listing arrives.
    searchDuringRefresh = panel.ownerDocument.activeElement === search;
    busy = true;
    buttons();
    message('Reading folder…');
    const token = epoch;
    try {
      const value = desktop
        ? await invoke('refresh_asset_folder', { rootId: id })
        : browserRoots.get(id);
      if (!alive || token !== epoch || refreshRequest !== refreshSequence) return;
      if (
        desktop &&
        (value?.root?.id !== id || !Array.isArray(value.items) || !Array.isArray(value.warnings))
      )
        throw Error('Invalid asset folder response');
      if (!value) throw Error('Folder is unavailable');
      failedRoots.delete(id);
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
      if (alive && token === epoch && refreshRequest === refreshSequence) {
        failedRoots.add(id);
        message(String(error.message || error), true);
      }
    } finally {
      if (alive && refreshRequest === refreshSequence) {
        busy = false;
        searchDuringRefresh = false;
        drawFavorites();
      }
    }
  }
  async function openAssetFavorite(entry, restoreFocus) {
    if (!alive || busy || folderPickIntent || !roots.some((root) => root.id === entry.rootId))
      return;
    const key = assetFavoriteKey(entry.rootId, entry.relativePath);
    if (!assetFavorites.some((item) => assetFavoriteKey(item.rootId, item.relativePath) === key))
      return;
    folderRegistrationError = null;
    rootSelect.value = entry.rootId;
    const request = refreshSequence + 1;
    const pointerAtStart = pointerSequence;
    const focusRequest = { key, pointerAtStart };
    assetFocusRequest = focusRequest;
    try {
      await refreshRoot();
      if (
        !alive ||
        request !== refreshSequence ||
        currentRootId !== entry.rootId ||
        listing?.root?.id !== entry.rootId
      )
        return;
      const item = listing.items.find((value) => value.relativePath === entry.relativePath);
      if (!item) {
        failedAssets.add(key);
        drawFavorites();
        message('Asset unavailable; refresh the folder or remove it from Quick access', true);
        return;
      }
      failedAssets.delete(key);
      search.value = entry.relativePath;
      hierarchy.value = '';
      shown = 48;
      drawItems();
      await choose(item, epoch);
    } finally {
      if (
        restoreFocus &&
        alive &&
        request === refreshSequence &&
        pointerAtStart === pointerSequence &&
        panel.ownerDocument.activeElement === panel.ownerDocument.body
      ) {
        const replacement = [...quick.querySelectorAll('button')].find(
          (button) => button.dataset.quickAsset === key || button.dataset.quickRemoveAsset === key,
        );
        if (replacement && !replacement.disabled) replacement.focus({ preventScroll: true });
      }
      if (assetFocusRequest === focusRequest) assetFocusRequest = null;
    }
  }
  function beginBrowserFolderPick(intent) {
    if (!alive || busy || folderPickIntent) return;
    folderPickIntent = { ...intent, previousRootId: rootSelect.value };
    picker.value = '';
    buttons();
    try {
      picker.click();
    } catch (error) {
      folderPickIntent = null;
      buttons();
      message(String(error.message || error), true);
    }
  }
  async function addFolder() {
    if (!desktop) {
      beginBrowserFolderPick({ mode: 'add' });
      return;
    }
    busy = true;
    buttons();
    folderPickerPending = true;
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
      folderEpoch++;
      pendingBootstrap = null;
      checkingRoots = false;
      roots = result.roots;
      drawRoots(result.selectedRootId);
      await refreshRoot();
    } catch (error) {
      if (alive) {
        folderRegistrationError = String(error.message || error);
        message(folderRegistrationError, true);
      }
    } finally {
      folderPickerPending = false;
      busy = false;
      if (alive) {
        buttons();
        if (pendingBootstrap) {
          const pending = pendingBootstrap;
          pendingBootstrap = null;
          settleBootstrap(pending);
        }
      }
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
      checkingRoots = false;
      roots = [...browserRoots.values()];
      drawRoots(id);
      await refreshRoot();
    } catch (error) {
      if (alive) message(error.message, true);
    } finally {
      if (alive) buttons();
    }
  };
  picker.addEventListener('cancel', () => {
    const refreshing = folderPickIntent?.mode === 'refresh';
    folderPickIntent = null;
    if (alive) {
      buttons();
      message(
        refreshing ? 'Catalog unchanged. Remove folder to clear.' : 'Folder selection unchanged',
      );
    }
  });
  add.onclick = () => {
    if (!alive || busy || folderPickIntent) return;
    folderRegistrationError = null;
    void addFolder();
  };
  refresh.onclick = () => {
    if (!alive || busy || folderPickIntent) return;
    folderRegistrationError = null;
    if (desktop) void refreshRoot();
    else {
      beginBrowserFolderPick({ mode: 'refresh', rootId: rootSelect.value });
    }
  };
  remove.onclick = async () => {
    if (!alive || busy || folderPickIntent) return;
    folderRegistrationError = null;
    busy = true;
    buttons();
    try {
      const id = rootSelect.value;
      if (desktop) roots = await invoke('remove_asset_folder', { rootId: id });
      else {
        browserRoots.delete(id);
        roots = [...browserRoots.values()];
      }
      // The unregister may finish after Close; do not mutate a newer view or refresh it.
      if (!alive) return;
      drawRoots();
      await refreshRoot();
    } catch (error) {
      message(String(error.message || error), true);
    } finally {
      busy = false;
      if (alive) buttons();
    }
  };
  rootSelect.onchange = () => {
    if (!alive) return;
    if (folderPickIntent) {
      rootSelect.value = folderPickIntent.previousRootId;
      return;
    }
    if (busy) {
      rootSelect.value = currentRootId;
      return;
    }
    folderRegistrationError = null;
    void refreshRoot();
  };
  search.oninput = () => {
    folderRegistrationError = null;
    shown = 48;
    if (busy && searchDuringRefresh) return;
    drawItems();
  };
  hierarchy.onchange = () => {
    folderRegistrationError = null;
    shown = 48;
    drawItems();
  };
  more.onclick = () => {
    folderRegistrationError = null;
    shown += 48;
    drawItems();
  };
  function dragover(event) {
    if (!dragTicket || busy || dragTicket.epoch !== epoch || !resolveDrop?.(event)) return;
    event.preventDefault();
    event.dataTransfer.dropEffect = 'copy';
  }
  function drop(event) {
    const ticket = dragTicket;
    const placement = ticket && resolveDrop?.(event);
    if (
      !ticket ||
      !placement ||
      ticket.epoch !== epoch ||
      event.dataTransfer?.getData(dragType) !== ticket.id
    )
      return;
    event.preventDefault();
    event.stopPropagation();
    dragTicket = null;
    void placeAsset(placement.route, placement, ticket);
  }
  document.addEventListener('dragover', dragover);
  document.addEventListener('drop', drop);
  async function placeAsset(route, placement = {}, ticket = null) {
    const chosen = ticket?.item || selected;
    if (!chosen || chosen.error || busy) return;
    const item = chosen,
      root = listing.root,
      request = ticket?.request || capture(),
      token = ticket?.epoch ?? epoch;
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
        author: ticket?.metadata.author ?? fields.author.value,
        source: ticket?.metadata.source ?? fields.source.value,
        license: ticket?.metadata.license ?? fields.license.value,
      };
      insert(request, {
        ...placement,
        route,
        source: value.source,
        asset: value.asset,
        name: item.relativePath.split('/').at(-1),
        provenance,
        joinBeforeOut: route === 'graph' && value.kind === 'svg' && !ticket && join.checked,
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
  function settleBootstrap(result) {
    if (!alive || result.version !== folderEpoch) return;
    checkingRoots = false;
    if (result.error) {
      drawFavorites();
      message(String(result.error.message || result.error), true);
      return;
    }
    if (!Array.isArray(result.roots)) {
      drawFavorites();
      message('Invalid asset folder list', true);
      return;
    }
    roots = result.roots;
    drawRoots();
    void refreshRoot();
  }
  drawFavorites();
  message('Reading folders…');
  void (async () => {
    // Item redraws do not supersede folder discovery. A successful folder
    // registration does; while the picker is pending, retain this result.
    const version = folderEpoch;
    try {
      const initialRoots = desktop
        ? await invoke('list_asset_folders')
        : [...browserRoots.values()];
      if (!alive || version !== folderEpoch) return;
      const result = { version, roots: initialRoots };
      if (folderPickerPending) pendingBootstrap = result;
      else settleBootstrap(result);
    } catch (error) {
      if (!alive || version !== folderEpoch) return;
      const result = { version, error };
      if (folderPickerPending) pendingBootstrap = result;
      else settleBootstrap(result);
    }
  })();
  buttons();
  if (focus) search.focus();
  return panel;
}

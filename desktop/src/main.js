import { installLayerReorder } from './layer-reorder.js';
import { wrapExtent, basicTextWrap, withWrapExtent } from './text-wrap.js';
import {
  ARTBOARD_UNITS,
  displayLengthToDocumentUnits,
  formatDocumentLength,
} from './artboard-units.js';
import { SVG_EXPORT_PRECISIONS } from './svg-number-format.js';
import { applyExportPreset, loadExportPresets, saveExportPreset } from './export-presets.js';
import { standardLigatureSupport, textFeatureReason } from './opentype-features.js';
import { initialLayerId } from './initial-layer.js';
import { hasDotControls } from './dither-controls.js';
import { repeaterControls } from './repeater.js';
import { svgPartTarget } from './svg-part.js';
import {
  directSelectionSource,
  transformDirectPath,
  inverseAffineDelta,
  inverseAffinePoint,
} from './direct-selection.js';
import { hasLayerSolo, layerIsVisible } from './layer-visibility.js';
import { osTextIntent } from './os-font-outline.js';
import { desktopTextIntent } from './desktop-font-text.js';
import { queryShapeSources, directGeometryUses } from './source-navigation.js';
import { nodeCategories, nodePickerItems, searchNodeItems } from './node-catalog.js';
import { createViewerState, mountViewers, updateViewers } from './node-viewers.js';
import { createCompositionViewStateStore } from './composition-view-state.js';
import { attachExpressionCompletion } from './expression-completion.js';
import { matchingNamedInstance } from './variable-font/named-instances.js';
import { collectCharacterSnapGeometry, characterGapGuidePath } from './character-snapping.js';
import { transformCharacterPoint } from './character-geometry.js';
import {
  constrainDirectionDrag,
  snapDirectionTranslation,
  directionGuidePath,
  snapGraphDirection,
} from './direction-snapping.js';
import { prepareImage, preflightImages, pngDimensions, rasterizeSVG } from './raster.js';
import { captureFocus, restoreFocus } from './focus.js';
import { graphemeDeletionRange, graphemeMovementCorrection } from './grapheme-editing.js';
import { textToolGroup } from './text-tool.js';
import { handlePolar, commitHandlePolar } from './handle-polar.js';
import './style.css';
import { ensureCommandMenus } from './command-menus.js';
const graphemeEditingAvailable = typeof Intl.Segmenter === 'function';
// Shared keyboard picker. Document changes stay in the caller's typed Session.
function openSearchPanel({
  title,
  items,
  selectedId,
  query = '',
  action,
  onSelect,
  onCommit,
  onCancel,
  isFavorite,
  toggleFavorite,
  fontFilters = false,
  categories,
  filterItems,
  context,
  fallbackFocus,
}) {
  const previousFocus = document.activeElement;
  const dialog = document.createElement('dialog');
  dialog.className = 'search-panel';
  dialog.setAttribute('aria-label', title);
  dialog.innerHTML =
    '<header><strong></strong><button type="button" class="picker-close" aria-label="Close">×</button></header><div class="picker-search"><input type="search" autocomplete="off" spellcheck="false"><button type="button" class="picker-favorites" aria-label="Show favorite fonts" aria-pressed="false" title="Favorites">☆</button></div><div class="picker-list" role="listbox"></div><output class="picker-status" role="status"></output><footer><button type="button" class="picker-cancel">Cancel</button><button type="button" class="picker-apply"></button></footer>';
  dialog.querySelector('strong').textContent = title;
  const input = dialog.querySelector('input'),
    list = dialog.querySelector('.picker-list'),
    status = dialog.querySelector('output'),
    apply = dialog.querySelector('.picker-apply'),
    favorites = dialog.querySelector('.picker-favorites');
  const listId = `picker-${Math.random().toString(36).slice(2)}`;
  list.id = listId;
  list.setAttribute('aria-label', fontFilters ? 'Available font faces and styles' : title);
  input.setAttribute('aria-label', `${title} search`);
  input.setAttribute('role', 'combobox');
  input.setAttribute('aria-controls', listId);
  input.setAttribute('aria-expanded', 'true');
  input.setAttribute('aria-autocomplete', 'list');
  input.placeholder = `Search ${title.toLowerCase()}`;
  input.value = query;
  apply.textContent = action;
  favorites.hidden = !toggleFavorite;
  let activeId = selectedId,
    rows = [],
    closed = false,
    composing = false,
    favoriteOnly = false,
    lastNotified;
  let familyFilter = '',
    weightFilter = '';
  let familySelect, weightSelect;
  if (fontFilters) {
    const filters = document.createElement('div');
    filters.className = 'picker-search picker-font-filters';
    function select(label) {
      const wrapper = document.createElement('label');
      wrapper.textContent = label + ' ';
      const control = document.createElement('select');
      control.setAttribute('aria-label', label);
      wrapper.append(control);
      filters.append(wrapper);
      return control;
    }
    function option(control, value, label) {
      const entry = document.createElement('option');
      entry.value = value;
      entry.textContent = label;
      control.append(entry);
    }
    familySelect = select('Font family');
    weightSelect = select('Available weight');
    option(familySelect, '', 'All families / flat list');
    for (const family of [
      ...new Set(items.map((item) => item.metadata?.family).filter(Boolean)),
    ].sort())
      option(familySelect, `family:${family}`, family);
    option(familySelect, '__unknown__', 'Unknown family / legacy faces');
    function weights() {
      weightSelect.replaceChildren();
      option(weightSelect, '', 'All available weights');
      const faces = items.filter(
        (item) =>
          !familyFilter ||
          (familyFilter === '__unknown__'
            ? !item.metadata?.family
            : `family:${item.metadata?.family}` === familyFilter),
      );
      for (const weight of [
        ...new Set(faces.map((item) => item.metadata?.weightClass).filter((w) => w != null)),
      ].sort((a, b) => a - b))
        option(weightSelect, String(weight), String(weight));
      if (faces.some((item) => item.metadata?.weightClass == null))
        option(weightSelect, '__unknown__', 'Unknown weight');
      weightFilter = '';
    }
    familySelect.onchange = () => {
      familyFilter = familySelect.value;
      weights();
      refresh();
    };
    weightSelect.onchange = () => {
      weightFilter = weightSelect.value;
      refresh();
    };
    weights();
    dialog.querySelector('.picker-search').after(filters);
    const note = document.createElement('p');
    note.className = 'font-note';
    note.textContent =
      'Choose a real face/style below. Unknown metadata stays unknown; no synthetic bold or italic.';
    filters.after(note);
  }
  let categoryId = '';
  const expandedCategories = new Set(categories?.slice(0, 1).map((category) => category.id));
  let optionElements = [];
  const scrollSpace = document.createElement('div');
  scrollSpace.setAttribute('aria-hidden', 'true');
  let categorySelect, scope, clearScope;
  if (categories) {
    dialog.classList.add('node-search-panel');
    const filters = document.createElement('div');
    filters.className = 'picker-search picker-category-filters';
    const label = document.createElement('label');
    label.textContent = '分類（内蔵） ';
    categorySelect = document.createElement('select');
    categorySelect.setAttribute('aria-label', 'ノード分類');
    for (const category of [{ id: '', label: 'すべて' }, ...categories]) {
      const option = document.createElement('option');
      option.value = category.id;
      option.textContent = category.label;
      categorySelect.append(option);
    }
    label.append(categorySelect);
    scope = document.createElement('span');
    scope.className = 'picker-scope';
    scope.setAttribute('role', 'status');
    clearScope = document.createElement('button');
    clearScope.type = 'button';
    clearScope.textContent = '全体で検索';
    clearScope.className = 'picker-clear-scope';
    categorySelect.onchange = () => {
      categoryId = categorySelect.value;
      refresh();
    };
    clearScope.onclick = () => {
      categoryId = categorySelect.value = '';
      refresh();
      input.focus();
    };
    filters.append(label, scope, clearScope);
    dialog.querySelector('.picker-search').after(filters);
    const destination = document.createElement('p');
    destination.className = 'picker-context';
    destination.textContent = context;
    filters.before(destination);
  }
  const active = () => rows.find((item) => item.id === activeId && !item.disabled);
  // Scroll only this viewport. scrollIntoView can also move the dialog/page.
  function reveal(row, atTop = false) {
    if (!row) return;
    const viewport = list.getBoundingClientRect();
    const bounds = row.getBoundingClientRect();
    const top = bounds.top - viewport.top - list.clientTop + list.scrollTop;
    if (atTop) {
      // A short final category still needs enough trailing room to reach the top.
      const contentEnd =
        scrollSpace.getBoundingClientRect().top - viewport.top - list.clientTop + list.scrollTop;
      scrollSpace.style.height = `${Math.max(0, top + list.clientHeight - contentEnd)}px`;
      list.scrollTop = top;
    } else if (top < list.scrollTop) list.scrollTop = top;
    else if (top + bounds.height > list.scrollTop + list.clientHeight)
      list.scrollTop = top + bounds.height - list.clientHeight;
  }
  function highlight(scroll = !categories) {
    for (const row of optionElements)
      row.setAttribute('aria-selected', String(row.dataset.id === activeId));
    const index = rows.findIndex((item) => item.id === activeId);
    if (index >= 0) {
      input.setAttribute('aria-activedescendant', `${listId}-${index}`);
      if (scroll) reveal(optionElements[index]);
    } else input.removeAttribute('aria-activedescendant');
    apply.disabled = !active();
    if (lastNotified !== activeId) {
      lastNotified = activeId;
      status.textContent = '';
      onSelect?.(active());
    }
  }
  function refresh() {
    const words = input.value.trim().normalize('NFKC').toLowerCase().split(/\s+/).filter(Boolean);
    if (closed) return;
    rows = filterItems
      ? filterItems(items, input.value, categoryId)
      : items.filter(
          (item) =>
            (!favoriteOnly || isFavorite(item)) &&
            (!familyFilter ||
              (familyFilter === '__unknown__'
                ? !item.metadata?.family
                : `family:${item.metadata?.family}` === familyFilter)) &&
            (!weightFilter ||
              (weightFilter === '__unknown__'
                ? item.metadata?.weightClass == null
                : String(item.metadata?.weightClass) === weightFilter)) &&
            words.every((word) =>
              `${item.label} ${item.detail || ''}`.normalize('NFKC').toLowerCase().includes(word),
            ),
        );
    if (scope) {
      const label = categories.find((item) => item.id === categoryId)?.label || 'すべて';
      scope.textContent = `${categoryId ? label + '内' : '全体'} ${rows.length}件`;
      clearScope.hidden = !categoryId;
    }
    const grouped = categories && !input.value.trim() && !categoryId;
    const candidates = rows;
    if (grouped)
      rows = categories.flatMap((category) =>
        expandedCategories.has(category.id)
          ? candidates.filter((item) => item.categoryId === category.id)
          : [],
      );
    list.setAttribute('role', grouped ? 'tree' : 'listbox');
    input.setAttribute('aria-haspopup', grouped ? 'tree' : 'listbox');
    const scrollTop = list.scrollTop;
    if (!grouped) scrollSpace.style.height = '0px';
    if (!rows.some((item) => item.id === activeId && !item.disabled))
      activeId = rows.find((item) => !item.disabled)?.id;
    list.replaceChildren();
    optionElements = [];
    const groups = new Map();
    if (grouped) {
      for (const category of categories) {
        const count = candidates.filter((item) => item.categoryId === category.id).length;
        if (!count) continue;
        const group = document.createElement('div');
        group.className = 'picker-category';
        group.dataset.category = category.id;
        group.setAttribute('role', 'treeitem');
        group.setAttribute('aria-label', category.label);
        group.setAttribute('aria-expanded', String(expandedCategories.has(category.id)));
        group.tabIndex = 0;
        const heading = document.createElement('div');
        heading.className = 'picker-category-heading';
        heading.textContent = `${expandedCategories.has(category.id) ? '▾' : '▸'} ${category.label} (${count})`;
        const children = document.createElement('div');
        children.setAttribute('role', 'group');
        children.hidden = !expandedCategories.has(category.id);
        group.append(heading, children);
        const toggle = () => {
          if (closed || composing) return;
          const opening = !expandedCategories.has(category.id);
          if (opening) expandedCategories.add(category.id);
          else expandedCategories.delete(category.id);
          refresh();
          const next = [...list.children].find((entry) => entry.dataset.category === category.id);
          next?.focus({ preventScroll: true });
          if (opening) reveal(next.querySelector('.picker-category-heading'), true);
        };
        heading.onclick = toggle;
        group.onkeydown = (event) => {
          if (event.target !== group || event.isComposing || event.keyCode === 229 || composing)
            return;
          if (['Enter', ' ', 'ArrowRight', 'ArrowLeft'].includes(event.key)) {
            event.preventDefault();
            if (event.repeat) return;
            if (event.key === 'ArrowRight' && expandedCategories.has(category.id)) return;
            if (event.key === 'ArrowLeft' && !expandedCategories.has(category.id)) return;
            toggle();
          } else if (['ArrowDown', 'ArrowUp'].includes(event.key)) {
            event.preventDefault();
            const next =
              event.key === 'ArrowDown' ? group.nextElementSibling : group.previousElementSibling;
            next?.focus({ preventScroll: true });
            reveal(next?.querySelector('.picker-category-heading'));
          }
        };
        list.append(group);
        groups.set(category.id, children);
      }
    }
    rows.forEach((item, index) => {
      const row = document.createElement('div');
      row.className = 'picker-option';
      row.dataset.id = item.id;
      row.id = `${listId}-${index}`;
      row.setAttribute('role', grouped ? 'treeitem' : 'option');
      row.setAttribute('aria-disabled', String(!!item.disabled));
      const name = document.createElement('span');
      name.textContent = item.label;
      row.append(name);
      if (item.detail) {
        const detail = document.createElement('small');
        detail.textContent = item.detail;
        row.append(detail);
      }
      if (toggleFavorite) {
        const star = document.createElement('button');
        star.type = 'button';
        star.className = 'picker-star';
        star.textContent = isFavorite(item) ? '★' : '☆';
        star.setAttribute('aria-label', `Favorite ${item.label}`);
        star.setAttribute('aria-pressed', String(isFavorite(item)));
        star.onclick = (event) => {
          event.stopPropagation();
          toggleFavorite(item);
          refresh();
          input.focus();
        };
        row.append(star);
      }
      row.onclick = () => {
        if (closed || item.disabled) return;
        activeId = item.id;
        highlight();
        input.focus({ preventScroll: true });
      };
      row.ondblclick = () => {
        if (!closed && !item.disabled && rows.includes(item)) {
          activeId = item.id;
          commit();
        }
      };
      (groups.get(item.categoryId) || list).append(row);
      optionElements.push(row);
    });
    if (grouped) {
      list.append(scrollSpace);
      // Keep the current viewport when closing a short category as well.
      const contentEnd =
        scrollSpace.getBoundingClientRect().top -
        list.getBoundingClientRect().top -
        list.clientTop +
        list.scrollTop;
      scrollSpace.style.height = `${Math.max(parseFloat(scrollSpace.style.height) || 0, scrollTop + list.clientHeight - contentEnd)}px`;
    }
    highlight();
    if (categories) list.scrollTop = scrollTop;
    if (!candidates.length)
      status.textContent = categories ? '該当するノードはありません' : 'No matches';
  }
  const panel = {
    get closed() {
      return closed;
    },
    get activeId() {
      return activeId;
    },
    status(text, ready = true) {
      if (closed) return;
      status.textContent = text;
      apply.disabled = !ready || !active();
    },
    close(cancel = false) {
      if (closed) return;
      closed = true;
      if (cancel) onCancel?.();
      dialog.close();
      dialog.remove();
      if (previousFocus?.isConnected) previousFocus.focus({ preventScroll: true });
      else fallbackFocus?.();
    },
  };
  function commit() {
    if (!closed && !composing && active()) onCommit(active());
  }
  dialog.querySelector('.picker-close').onclick = () => panel.close(true);
  dialog.querySelector('.picker-cancel').onclick = () => panel.close(true);
  apply.onclick = commit;
  favorites.onclick = () => {
    favoriteOnly = !favoriteOnly;
    favorites.setAttribute('aria-pressed', String(favoriteOnly));
    refresh();
    input.focus();
  };
  dialog.addEventListener('cancel', (event) => {
    event.preventDefault();
    panel.close(true);
  });
  dialog.addEventListener('compositionstart', () => {
    composing = true;
  });
  dialog.addEventListener('compositionend', () => {
    composing = false;
    refresh();
  });
  input.oninput = () => {
    if (!composing) refresh();
  };
  dialog.addEventListener('keydown', (event) => {
    event.stopPropagation();
    if (event.isComposing || event.keyCode === 229 || composing) return;
    if (event.key === 'Escape') {
      event.preventDefault();
      panel.close(true);
    } else if (['ArrowDown', 'ArrowUp'].includes(event.key) && event.target === input) {
      event.preventDefault();
      const enabled = rows.filter((item) => !item.disabled);
      if (!enabled.length) return;
      const index = enabled.findIndex((item) => item.id === activeId);
      activeId =
        enabled[(index + (event.key === 'ArrowDown' ? 1 : -1) + enabled.length) % enabled.length]
          .id;
      highlight(true);
    } else if (event.key === 'Enter') {
      // Buttons retain their normal action (favorites/cancel), search commits.
      if (event.target !== input) return;
      event.preventDefault();
      if (!event.repeat) commit();
    }
  });
  document.body.append(dialog);
  dialog.showModal();
  input.focus();
  // Return the handle before a preview callback can use it.
  queueMicrotask(() => {
    if (!closed) refresh();
  });
  return panel;
}

// Keep native select semantics for existing connection dialogs while adding search.
function searchConnectionOptions(dialog) {
  const select = dialog.querySelector('select'),
    apply = dialog.querySelector('[data-confirm]');
  const input = document.createElement('input');
  input.type = 'search';
  input.placeholder = 'Search nodes or output IDs';
  input.setAttribute('aria-label', 'Search connection nodes');
  select.before(input);
  const currentOption = select.querySelector('option[selected]');
  if (currentOption) select.value = currentOption.value;
  const options = [...select.options].map((option) => option.cloneNode(true));
  let selected = select.value;
  select.addEventListener('change', () => {
    selected = select.value;
  });
  input.addEventListener('input', () => {
    const words = input.value.trim().normalize('NFKC').toLowerCase().split(/\s+/).filter(Boolean);
    const matches = options.filter((option) =>
      words.every((word) =>
        `${option.textContent} ${option.value}`.normalize('NFKC').toLowerCase().includes(word),
      ),
    );
    select.replaceChildren(...matches.map((option) => option.cloneNode(true)));
    if (matches.some((option) => option.value === selected)) select.value = selected;
    apply.disabled = !matches.some((option) => !option.disabled);
  });
  input.addEventListener('keydown', (event) => {
    if (event.isComposing || event.keyCode === 229) return;
    if (event.key === 'Enter') {
      event.preventDefault();
      if (!apply.disabled) apply.click();
    }
    if (['ArrowDown', 'ArrowUp'].includes(event.key)) {
      event.preventDefault();
      const options = [...select.options].filter((option) => !option.disabled);
      if (!options.length) return;
      const index = options.findIndex((option) => option.value === select.value);
      select.value =
        options[
          (index + (event.key === 'ArrowDown' ? 1 : -1) + options.length) % options.length
        ].value;
      selected = select.value;
    }
  });
  dialog.addEventListener('keydown', (event) => {
    if (event.key === 'Escape' && !event.isComposing) {
      event.preventDefault();
      closeEditorDialog(dialog);
    }
  });
  input.focus();
}

import {
  resolveArtworkDuplicateTarget,
  resolveArtworkDuplicateSelection,
} from './artwork-creation.js';
import { resolveArtworkMoveTarget } from './artwork-move-target.js';
import { resolveArtworkMoveSelection, translateArtworkTargets } from './artwork-multi-move.js';
import { artworkAlignments } from './artwork-alignment.js';
import { artworkDistributions } from './artwork-distribution.js';
import { artworkGapDistributions } from './artwork-gap-distribution.js';
import { resolveArtworkOrderTarget } from './artwork-ordering.js';
import { moveArtworkLayerBefore } from './artwork-ordering.js';
import {
  findFreeNodePosition,
  adjacentNodeOffset,
  fitScale,
  unionBounds,
  anchoredPan,
  anchoredScroll,
  wheelPixels,
} from './layout.js';
import { readSVGSource } from './svg-import.js';
import {
  desktop,
  saveFile,
  openFile,
  openSVGSource,
  writeProject,
  releaseProject,
  chooseUnsaved,
  registerClose,
  listSystemFonts,
  readSystemFont,
  listOsOutlineFonts,
  outlineOsText,
  listDesktopFontFaces,
  listDesktopFontAxes,
  shapeDesktopText,
  DocumentLifecycle,
  beginRecovery,
  writeRecovery,
  listRecovery,
  readRecovery,
  acknowledgeRecovery,
} from './storage.js';
import { RecoveryAutosave } from './recovery-autosave.js';
import { readOsFontCatalog, osFontAvailability } from './os-font-catalog.js';
import {
  parseFont,
  resolveFonts,
  makeVariableFontAsset,
  MAX_FONT_BYTES,
  MAX_PROJECT_BYTES,
} from './fonts.js';
import {
  activeCompositionDocument,
  deserializeCompositionProject,
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
  validateCompositionProject,
} from './composition-project-model.js';
import { createCompositionSessionOwner } from './composition-session-owner-core.js';
import {
  blankDocument,
  hasEmptyOutput,
  node,
  point,
  uid,
  clone,
  cloneDocument,
  sameDocument,
  definitions,
  parameters,
  activeParameters,
  booleanInputKeys,
  booleanPendingInputKey,
  operationOptions,
  setOperation,
  bypassContract,
  parseStrokeDash,
  radialSettings,
  assertClosedOffsetChange,
  textPathTargets,
  parameter,
  outputType,
  outputPorts,
  vectorReferenceTargets,
  expressionReferenceTargets,
  referenceTarget,
  socketCompatibleFor,
  evaluate as evaluateCore,
  enableRemapPoint,
  pathData,
  signature,
  connect,
  disconnect,
  deserialize,
  exportSVG,
  renderElement,
  createVariableDocumentPreparer,
  scrubNumericValues,
  guideDefaults,
  guideSettings,
  resolveGridOrigin,
  snapTranslation,
  collectSnapLayerBounds,
  pathBounds,
  layoutGridDefaults,
  layoutGridLines,
  characterOccurrences,
  textEditGeometry,
  nearestTextCaret,
  resolveNodeOutputTargets,
  isTransform,
  transformExtensions,
  transformParameter,
  pointControlTargets,
  hitPointControl,
  hitPathSegment,
  insertionRemapDependents,
  inverseCharacterDelta,
  characterBoxPoints,
  hitCharacter,
  setCharacterOffsets,
  characterPairFromIds,
  characterPairInRange,
  CHARACTER_STYLE_KEYS,
  characterIdsInRange,
  createCharacterPositions,
  objectResizeHandles,
  resizedObjectValue,
  anchorFromOutputDelta,
  transformHandleGeometry,
  scaleFromHandle,
  rotationHandleAngle,
  rotationHandleDelta,
  rotationFromHandle,
  expression,
} from './core.js';
const app = document.querySelector('#app');
const osTextDrafts = new Map();
let osFontCatalog = { families: null, faces: null, familiesError: false, facesError: false };
let osFontCatalogRequest = 0;
let osFontCatalogPending = false;
let editorPicker = null,
  fontPreview = null;
const ALIGN_PREFERENCES_KEY = 'nect.alignment-panel.v1';
let alignmentPreferences = readAlignmentPreferences();
function readAlignmentPreferences() {
  try {
    const value = JSON.parse(localStorage.getItem(ALIGN_PREFERENCES_KEY) || '{}');
    return {
      open: value.open !== false,
      reference: value.reference === 'artboard' ? 'artboard' : 'selection',
      spacing:
        Number.isFinite(value.spacing) && Math.abs(value.spacing) <= 100000 ? value.spacing : null,
    };
  } catch {
    return { open: true, reference: 'selection', spacing: null };
  }
}
function saveAlignmentPreferences() {
  try {
    localStorage.setItem(ALIGN_PREFERENCES_KEY, JSON.stringify(alignmentPreferences));
  } catch {
    notify('Alignment preferences could not be saved', true);
  }
}
function editAlignmentSpacing(input, commit = false) {
  const value = input.value.trim() ? Number(input.value) : null;
  if (
    input.validity?.badInput ||
    (value !== null && (!Number.isFinite(value) || Math.abs(value) > 100000))
  ) {
    for (const button of app.querySelectorAll('[data-distribute-gap]')) button.disabled = true;
    if (commit) {
      input.value = alignmentPreferences.spacing ?? '';
      editAlignmentSpacing(input);
      notify('Spacing must be within ±100000 px', true);
    }
    return;
  }
  alignmentPreferences.spacing = value;
  saveAlignmentPreferences();
  const resolved = alignmentTargets();
  const eligible =
    resolved.scope !== 'nodes' &&
    resolved.move.supported &&
    resolved.ids.length >= (value === null ? 3 : 2);
  for (const button of app.querySelectorAll('[data-distribute-gap]')) {
    button.dataset.distributeSpacing = String(value ?? '');
    button.dataset.eligible = String(!!eligible);
    button.disabled = !eligible || alignmentBusy();
    button.title =
      value === null
        ? 'Equal gaps; fixed outer edges'
        : `Fixed gap ${value} px; fixed leading edge`;
  }
}
// Opt-in variable fonts keep active prepared geometry separate from the next candidate.
let bundledFont = null;
let variableBridge = null,
  variableAction = null,
  variablePreviewFonts = null;
const hasVariableFonts = (doc) =>
  Object.values(doc.fontAssets || {}).some((a) => a.kind === 'variable-glyf-v1');
const variableNode = (id) => current().nodes.find((n) => n.id === id)?.variation;
const activeFonts = () => ({
  ...(bundledFont ? { default: bundledFont } : {}),
  ...(variableBridge?.fonts() || {}),
});
function cancelVariableAction() {
  variableAction?.controller.abort();
  variableAction?.owner.cancelPrepared(variableAction.bridge);
  if (variableAction?.opening) variableAction.bridge.dispose();
  variableAction = null;
  variablePreviewFonts = null;
  fontPreview = null;
}
function variableError(error) {
  const reasons = {
    VARIABLE_VERTICAL_UNSUPPORTED: 'Variable fonts support horizontal text only.',
    VARIABLE_WRAP_UNSUPPORTED: 'Variable fonts do not support wrapping yet.',
    VARIABLE_PATH_UNSUPPORTED: 'Variable fonts do not support text on a path yet.',
    VARIABLE_ARRANGEMENT_UNSUPPORTED:
      'Variable fonts do not support maintained character arrangements yet.',
    VARIABLE_RANGE_UNSUPPORTED:
      'Variable fonts support whole-text styling only; remove character adjustments first.',
    INVALID_AXES: 'Enter a finite value within the advertised axis range.',
    UNSUPPORTED_MAPPING:
      'This experimental font path supports simple, single-line Latin text only. Complex scripts, combining marks, emoji and line breaks are unsupported.',
    TEXT_LIMIT: 'Variable text must contain 1–1024 characters.',
    TIMEOUT: 'Variable font preparation timed out; try a smaller text or another font.',
    VARIABLE_FEATURES_UNSUPPORTED: 'Variable fonts currently require automatic kerning.',
    UNSUPPORTED_FORMAT:
      'Choose a supported monochrome glyf/gvar variable TTF. CFF2, color and extended variation tables are not supported.',
  };
  return reasons[error.code || error.message] || error.message;
}
async function preparedChange(
  type,
  payload,
  text = 'Updated',
  envelope = session.state,
  preview = false,
) {
  cancelVariableAction();
  variableBridge ||= createVariableDocumentPreparer();
  const action = {
    owner: session,
    bridge: variableBridge,
    revision: envelope.expectedRevision,
    controller: new AbortController(),
  };
  variableAction = action;
  notify('Preparing variable text… Escape cancels');
  try {
    const token = await action.owner.prepareCommand(
      { ...envelope, type, payload: { layerId: current().id, ...payload } },
      variableBridge,
      { signal: action.controller.signal },
    );
    if (variableAction !== action || session !== action.owner) return null;
    if (preview) {
      const prepared = action.owner.preparedSnapshot(token);
      fontPreview = prepared.document;
      variablePreviewFonts = prepared.fonts;
      updateCanvas(true);
      notify('Variable axis preview · finish editing to apply; Escape cancels');
      return token;
    }
    const result = action.owner.commitPrepared(token);
    variableAction = null;
    notify(text);
    render();
    return result;
  } catch (error) {
    if (variableAction === action) {
      variableAction = null;
      notify(
        `Variable font edit rejected; current document preserved: ${variableError(error)}`,
        true,
      );
      render();
    }
    return null;
  }
}
async function prepareOpenDocument(next, options = {}) {
  const project = asCompositionProject(next),
    activeCompositionId = options.activeCompositionId ?? project.compositions[0].id,
    document = activeCompositionDocument(project, activeCompositionId);
  if (!hasVariableFonts(document)) {
    replaceDocument(project, { ...options, activeCompositionId });
    return;
  }
  cancelVariableAction();
  const bridge = createVariableDocumentPreparer();
  const owner = session,
    revision = session.state.expectedRevision;
  const action = { owner, bridge, opening: true, revision, controller: new AbortController() };
  variableAction = action;
  notify('Preparing variable fonts before opening… Escape cancels');
  try {
    const ticket = await bridge.prepareDocument(document, { signal: action.controller.signal });
    if (
      variableAction !== action ||
      session !== owner ||
      session.state.expectedRevision !== revision
    )
      throw Error('Document changed while preparing; open the file again');
    bridge.activate(ticket);
    variableBridge?.dispose();
    variableBridge = bridge;
    variableAction = null;
    replaceDocument(project, { ...options, activeCompositionId, prepared: true });
  } catch (error) {
    if (variableAction === action) variableAction = null;
    bridge.dispose();
    throw Error(variableError(error));
  }
}

document.addEventListener(
  'keydown',
  (event) => {
    if (event.key !== 'Escape' || (!variableAction && !fontRequest?.variable)) return;
    event.preventDefault();
    event.stopImmediatePropagation();
    cancelVariableAction();
    if (fontRequest?.variable) fontRequest = null;
    notify('Variable font preparation cancelled; current document preserved');
    render();
  },
  true,
);
const sessionOptions = {
  fonts: activeFonts,
  onCommitted: (state) => {
    // Let the operation finish a document-lifetime transition (e.g. SVG import)
    // before associating its committed state with a recovery document identity.
    queueMicrotask(() => {
      if (state.sessionId !== session.id) return;
      try {
        updateDocumentStatus();
      } catch (error) {
        recovery?.report(recovery.current, {
          phase: 'failed',
          error: error.message || String(error),
        });
      }
    });
  },
  // A numeric pointer reservation first commits any typed value, before it
  // captures a gesture snapshot. Only its started phase owns an authored edit.
  busy: () =>
    !!(
      penDraft ||
      (drag && !drag.reservingGesture && (drag.type !== 'numeric' || drag.started)) ||
      wiring ||
      composing
    ),
};
function asCompositionProject(value) {
  if (
    value &&
    typeof value === 'object' &&
    (Object.hasOwn(value, 'compositionSchemaVersion') || Object.hasOwn(value, 'compositions'))
  )
    return validateCompositionProject(value);
  return migrateLegacyDocumentToCompositionProject(value);
}
function deserializeProject(text) {
  if (typeof text !== 'string') throw Error('Project JSON must be text');
  if (new TextEncoder().encode(text).byteLength > MAX_PROJECT_BYTES)
    throw Error('Project exceeds 50 MB');
  return deserializeCompositionProject(text, deserialize);
}
function serializeProject(project) {
  const text = serializeCompositionProject(project);
  if (new TextEncoder().encode(text).byteLength > MAX_PROJECT_BYTES)
    throw Error('Project exceeds 50 MB');
  return text;
}

let sourceEpoch = 0,
  sourcePicker = null,
  sourceReturn = null;
let viewerState = createViewerState();
let session = createCompositionSessionOwner(
  migrateLegacyDocumentToCompositionProject(blankDocument()),
  sessionOptions,
);
let history = session.historyView,
  activeLayer = history.doc.layers[0].id,
  selected = null,
  artworkTarget = null,
  artworkSelection = new Set(),
  layerClipboard = null,
  selectedNodePreset = null,
  tool = 'select',
  textTools = null,
  zoom = 1,
  canvasFit = true,
  composing = false,
  selectedGuide = null,
  guidesOpen = false,
  gridOpen = false,
  layoutOpen = false,
  gridOpacityDraft = null,
  layoutDraft = null,
  layoutPreview = false,
  snapMatches = [],
  pan = [0, 0],
  graphHeight = 42,
  graphZoom = 1,
  graphOverviewOpen = false,
  graphOverviewObserver = null,
  graphOverviewNodes = null,
  message = 'Ready',
  error = false,
  fonts = {},
  fontRequest = null,
  fontRequestId = 0,
  systemFonts = [],
  systemFontStatus = desktop ? 'Reading Windows fonts…' : '',
  wiring = null,
  drag = null,
  penId = null,
  penDraft = null;
// Composition project History is the file/recovery unit. Switching the active
// Composition does not change the saved project.
let encodedSession = null,
  encodedProjectRevision = null,
  encodedText;
function readDocument() {
  const revision = session.projectRevision;
  if (session !== encodedSession || revision !== encodedProjectRevision) {
    encodedText = serializeProject(session.projectSnapshot());
    encodedSession = session;
    encodedProjectRevision = revision;
  }
  return encodedText;
}
const documents = new DocumentLifecycle({
  read: readDocument,
  write: writeProject,
  release: releaseProject,
  choose: chooseUnsaved,
  hasDraft: () =>
    !!(
      penDraft ||
      drag ||
      wiring ||
      composing ||
      session.hasTextComposition ||
      session.hasGesture ||
      [...textDrafts.values()].some((draft) => draft.state.changed) ||
      osTextDrafts.size > 0 ||
      [...app.querySelectorAll('textarea[data-text]')].some(
        (el) =>
          variableNode(el.dataset.text) &&
          el.value !== current().nodes.find((n) => n.id === el.dataset.text)?.text,
      )
    ),
});
const recovery = desktop
  ? new RecoveryAutosave({
      begin: beginRecovery,
      write: writeRecovery,
      acknowledge: acknowledgeRecovery,
      status: updateRecoveryStatus,
    })
  : null;
let openRequest = 0,
  imageRequest = 0,
  imagePick = null,
  svgPartRequest = 0,
  svgPartPick = null,
  artboardUnit = 'px',
  svgExportUnit = 'px',
  svgExportSizing = 'fixed',
  svgExportPrecision = 'default',
  pngScale = 1,
  pngExportMode = 'scale',
  pngPpi = 300,
  selectedSvgExportPreset = null,
  svgExportPresetName = '',
  selectedPngExportPreset = null,
  pngExportPresetName = '',
  exportInFlight = false;
let exportPresetStorage = null,
  exportPresets = [],
  exportPresetLoadError = '';
try {
  if (typeof localStorage !== 'undefined') {
    exportPresetStorage = localStorage;
    exportPresets = loadExportPresets(exportPresetStorage);
  }
} catch (reason) {
  exportPresetLoadError = reason?.message || 'Saved export presets could not be loaded';
}
let selection = new Set();
let nodeContextMenu = null;
let contextMenuScroll = new Map();
const detailsState = new Map();
const compositionViewStates = createCompositionViewStateStore();
const selectionHistory = new WeakMap();
let focusedCompositionTabId = null,
  tabRenderSession = null,
  tabRenderCompositionId = null;
function closeTransientPanels(except = '') {
  if (except !== 'align') alignmentPreferences.open = false;
  if (except !== 'grid') gridOpen = false;
  if (except !== 'layout') layoutOpen = false;
  if (except !== 'guides') guidesOpen = false;
}
function selectionSnapshot(withEditSelection = false) {
  return {
    activeLayer,
    selected,
    artworkTarget,
    artworkSelection: new Set(artworkSelection),
    selection: new Set(selection),
    ...(withEditSelection
      ? {
          restoreEditSelection: true,
          characterEdit: characterEdit ? structuredClone(characterEdit) : null,
          selectedGuide,
          remapBatches: structuredClone(remapBatches),
          pointSelection: structuredClone(pointSelection),
          pointFocus: pointFocus ? { ...pointFocus } : null,
        }
      : {}),
  };
}
function stepHistory(redo = false) {
  if (osTextDrafts.size) {
    notify('Apply or cancel the OS Text edit before Undo/Redo', true);
    return;
  }
  if (session.hasTextComposition) {
    notify('Finish or cancel text composition before Undo/Redo', true);
    return;
  }
  const priorCompositionId = session.activeCompositionId;
  saveCompositionViewState();
  const priorUI = selectionSnapshot(selectionHistory.get(history.doc)?.restoreEditSelection);
  resetTransient();
  selectionHistory.set(history.doc, priorUI);
  const target = (redo ? history.future : history.past).at(-1);
  if (hasVariableFonts(history.doc) || (target && hasVariableFonts(target))) {
    void preparedChange(redo ? 'redo' : 'undo', {}, redo ? 'Redone' : 'Undone').then((result) => {
      if (result) {
        compositionViewStates.prune(session.projectSnapshot().compositions);
        if (session.activeCompositionId !== priorCompositionId) {
          const graphScroll = restoreCompositionViewState();
          render(false, null, graphScroll);
        } else {
          activeLayer =
            history.doc.layers.find((l) => l.id === activeLayer)?.id || history.doc.layers[0].id;
          render();
        }
      }
    });
    return;
  }
  session.dispatch({ ...session.state, type: redo ? 'redo' : 'undo' });
  const compositionChanged = session.activeCompositionId !== priorCompositionId;
  compositionViewStates.prune(session.projectSnapshot().compositions);
  const state = compositionChanged ? null : selectionHistory.get(history.doc);
  if (state) {
    ({ activeLayer, selected, artworkTarget } = state);
    artworkSelection = new Set(state.artworkSelection);
    selection = new Set(state.selection);
    characterEdit = state.characterEdit ? structuredClone(state.characterEdit) : null;
    if (state.restoreEditSelection) {
      selectedGuide = state.selectedGuide;
      const owner = current().nodes.find((n) => n.id === state.pointSelection?.nodeId);
      let ids = new Set(owner?.points?.map((p) => p.id) || []);
      if (owner?.type === 'Path Remap')
        try {
          ids = new Set(
            evaluate(current(), resolveFonts(history.doc, activeFonts()), history.doc.imageAssets)
              .run(owner.id)
              .contours.flatMap((c) => c.points.map((p) => p.id)),
          );
        } catch {
          ids.clear();
        }
      pointSelection = {
        nodeId: owner?.id || null,
        ids: new Set([...(state.pointSelection?.ids || [])].filter((id) => ids.has(id))),
      };
      pointFocus =
        state.pointFocus && ids.has(state.pointFocus.pointId) ? { ...state.pointFocus } : null;
      for (const [id, batch] of structuredClone(state.remapBatches)) remapBatches.set(id, batch);
    }
  }
  if (!history.doc.layers.some((layer) => layer.id === activeLayer)) {
    activeLayer = history.doc.layers[0].id;
    selected = null;
    artworkTarget = null;
    artworkSelection = new Set();
    selection = new Set();
  }
  if (compositionChanged) {
    compositionViewStates.prune(session.projectSnapshot().compositions);
    return restoreCompositionViewState();
  }
  return null;
}
const remapBatches = new Map();
let characterEdit = null;
let nameEdit = null,
  lastNameClick = null;
const textDrafts = new Map();
const textRanges = new Map();
const textRangeKey = (el) => `${el.dataset.text}:${el.closest('.node') ? 'node' : 'inspector'}`;
function remapBatch(id) {
  if (!remapBatches.has(id)) remapBatches.set(id, { selected: new Set(), delta: [0, 0] });
  return remapBatches.get(id);
}
let renderEvaluation = null;
let snapLayerBounds = { guides: [], artwork: [] };
const esc = (s) =>
  String(s ?? '').replace(
    /[&<>"']/g,
    (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[c],
  );
const current = () =>
  penDraft?.layer || history.doc.layers.find((l) => l.id === activeLayer) || history.doc.layers[0];
// Read-only display projection. Authoring adapters continue to resolve committed
// targets through current(); the detached preview is never installed in History.
let textPreview = null,
  textPreviewRevision = null,
  canvasTextEdit = null,
  pendingCanvasTextClick = null,
  lastCanvasTextClick = null;
const displayDocument = () =>
  fontPreview ||
  drag?.preview ||
  (textPreviewRevision === session.state.expectedRevision && textPreview) ||
  history.doc;
const evaluate = (layer, fonts = {}, imageAssets = {}, doc = displayDocument()) =>
  evaluateCore(layer, fonts, imageAssets, doc);
const displayLayer = () => {
  const doc = displayDocument();
  return penDraft?.layer || doc.layers.find((layer) => layer.id === activeLayer) || doc.layers[0];
};
const canvasLayers = (doc = history.doc) =>
  penDraft
    ? penDraft.resuming
      ? doc.layers.map((l) => (l.id === penDraft.layer.id ? penDraft.layer : l))
      : [...doc.layers, penDraft.layer]
    : doc.layers;
const selectedNode = () =>
  artworkTarget ? undefined : current().nodes.find((n) => n.id === selected);
function selectArtwork(layerId, { toggle = false, preserve = false } = {}) {
  if (!artworkTarget) artworkSelection = new Set();
  if (toggle) {
    if (artworkSelection.has(layerId)) artworkSelection.delete(layerId);
    else artworkSelection.add(layerId);
  } else if (!preserve || !artworkSelection.has(layerId)) artworkSelection = new Set([layerId]);
  activeLayer = artworkSelection.has(layerId) ? layerId : [...artworkSelection].at(-1) || layerId;
  artworkTarget = artworkSelection.size ? activeLayer : null;
  // Retain the established empty-composition UI for a plain single selection.
  if (!toggle && artworkSelection.size === 1 && hasEmptyOutput(current())) {
    artworkTarget = null;
    artworkSelection = new Set();
  }
  selected = null;
  selection = new Set();
  characterEdit = null;
  selectedGuide = null;
  pointSelection = { nodeId: null, ids: new Set() };
  pointFocus = null;
  pointHover = null;
}
function artworkMove() {
  return artworkSelection.size === 1
    ? resolveArtworkMoveTarget(history.doc, artworkTarget, { fonts })
    : resolveArtworkMoveSelection(history.doc, artworkSelection, { fonts });
}
function alignmentTargets(artworkResolution = null) {
  if (artworkTarget)
    return {
      scope: 'artwork',
      ids: [...artworkSelection],
      move: artworkResolution || artworkMove(),
    };
  if (!selection.size) return { ids: [], move: { supported: false } };
  const ids = [...selection],
    move = resolveNodeOutputTargets(history.doc, activeLayer, ids, { fonts });
  return { scope: 'nodes', ids, count: move.members?.length || ids.length, move };
}
function sameArtworkMove(target, expected) {
  if (expected.scope === 'artwork-selection')
    return (
      target?.scope === expected.scope &&
      target.targets.length === expected.targets.length &&
      target.targets.every((t, i) => sameArtworkMove(t, expected.targets[i]))
    );
  return (
    target?.layerId === expected.layerId &&
    target?.kind === expected.kind &&
    target?.nodeId === expected.nodeId &&
    target?.parameter === expected.parameter &&
    target?.outputNodeId === expected.outputNodeId &&
    (target.kind !== 'curve-point-positions' ||
      (JSON.stringify(target.pointIds) === JSON.stringify(expected.pointIds) &&
        JSON.stringify(target.closed) === JSON.stringify(expected.closed))) &&
    JSON.stringify(target.route) === JSON.stringify(expected.route)
  );
}
function alignmentIcon(action, distribute = false) {
  const vertical = ['top', 'center-y', 'bottom', 'gap-y'].includes(action);
  const edge = ['left', 'top'].includes(action) ? 0 : ['right', 'bottom'].includes(action) ? 2 : 1;
  let shape;
  if (action.startsWith('gap-'))
    shape =
      '<rect x="2" y="5" width="4" height="14"/><rect x="18" y="5" width="4" height="14"/><path d="M8 12h8m-6-3-3 3 3 3m4-6 3 3-3 3" fill="none" stroke="currentColor" stroke-width="1.4"/>';
  else if (distribute)
    shape = [4, 11, 18]
      .map(
        (x, i) =>
          `<rect x="${x}" y="${edge === 0 ? 5 : edge === 2 ? 19 - [9, 14, 7][i] : 12 - [9, 14, 7][i] / 2}" width="3" height="${[9, 14, 7][i]}"/>`,
      )
      .join('');
  else
    shape =
      `<path d="M${edge === 0 ? 3 : edge === 2 ? 21 : 12} 2v20" fill="none" stroke="currentColor" stroke-width="1.5"/>` +
      [8, 13, 6]
        .map(
          (width, i) =>
            `<rect x="${edge === 0 ? 5 : edge === 2 ? 19 - width : 12 - width / 2}" y="${4 + i * 6}" width="${width}" height="3"/>`,
        )
        .join('');
  return `<svg viewBox="0 0 24 24" aria-hidden="true" fill="currentColor" ${vertical ? 'style="transform:rotate(90deg)"' : ''}>${shape}</svg>`;
}
function alignmentPanel(move) {
  if (!alignmentPreferences.open) return '';
  const resolved = alignmentTargets(move);
  move = resolved.move;
  const count = resolved.count ?? resolved.ids.length;
  const allowed = count > 0 && move.supported;
  const alignReady = allowed && count >= (alignmentPreferences.reference === 'artboard' ? 1 : 2);
  const spreadReady = resolved.scope !== 'nodes' && allowed && count >= 3;
  const gapReady =
    resolved.scope !== 'nodes' &&
    allowed &&
    count >= (alignmentPreferences.spacing === null ? 3 : 2);
  const selectedIds = esc(JSON.stringify(resolved.ids));
  const busy = alignmentBusy();
  const order = ['left', 'center-x', 'right', 'top', 'center-y', 'bottom'];
  const alignButtons = order
    .map((action) => {
      const label = artworkAlignments.find(([id]) => id === action)[1];
      return `<button data-align-artwork="${action}" data-align-selection="${selectedIds}" data-align-active="${esc(activeLayer)}" data-align-reference="${alignmentPreferences.reference}" data-eligible="${alignReady}" ${!alignReady || busy ? 'disabled' : ''} aria-label="Align artwork ${label.toLowerCase()}" title="${esc(label)}"><span>${alignmentIcon(action)}</span></button>`;
    })
    .join('');
  const spreadButtons = order
    .map((action) => {
      const label = artworkDistributions.find(([id]) => id === action)[1];
      return `<button data-distribute-artwork="${action}" data-distribute-selection="${selectedIds}" data-distribute-active="${esc(activeLayer)}" data-eligible="${spreadReady}" ${!spreadReady || busy ? 'disabled' : ''} aria-label="Distribute artwork ${action}" title="${esc(spreadReady ? label + '; fixed endpoints' : allowed ? 'Select at least three artwork layers' : move?.reason || 'Select artwork layers')}">${alignmentIcon(action, true)}</button>`;
    })
    .join('');
  const gapButtons = artworkGapDistributions
    .map(
      ([action, label]) =>
        `<button data-distribute-gap="${action}" data-distribute-selection="${selectedIds}" data-distribute-active="${esc(activeLayer)}" data-distribute-spacing="${alignmentPreferences.spacing ?? ''}" data-eligible="${gapReady}" ${!gapReady || busy ? 'disabled' : ''} aria-label="隙間を等間隔 ${label}" title="${alignmentPreferences.spacing === null ? 'Equal gaps; fixed outer edges' : `Fixed gap ${alignmentPreferences.spacing} px; fixed leading edge`}">${alignmentIcon(action)}</button>`,
    )
    .join('');
  return `<section class="alignment-panel" aria-label="Alignment panel"><div class="alignment-heading"><strong>Align</strong><output aria-label="Alignment selection">${count} selected</output><button data-action="align-panel" aria-label="Hide alignment panel" title="Hide alignment panel">×</button></div><div class="alignment-icon-row" role="group" aria-label="Align objects">${alignButtons}</div><div class="alignment-icon-row" role="group" aria-label="Distribute objects">${spreadButtons}</div><div class="alignment-gap-row" role="group" aria-label="Distribute gaps"><span>Spacing</span>${gapButtons}<input type="number" data-align-spacing aria-label="Artwork gap spacing" min="-100000" max="100000" step="any" placeholder="Auto" value="${alignmentPreferences.spacing ?? ''}" title="Blank: equal gaps with outer edges fixed. Number: fixed gap from leading artwork."><span>px</span></div><div class="alignment-reference"><label>Align to <select data-align-reference aria-label="Alignment reference"><option value="selection" ${alignmentPreferences.reference === 'selection' ? 'selected' : ''}>Selection</option><option value="artboard" ${alignmentPreferences.reference === 'artboard' ? 'selected' : ''}>Artboard</option></select></label></div>${allowed || (!count && (artworkTarget || !selection.size)) ? '' : `<output class="alignment-error" role="status">${esc(move.reason)}</output>`}</section>`;
}
function notify(text, bad = false) {
  message = text;
  error = bad;
  let s = document.querySelector('.status');
  if (s) {
    s.classList.toggle('error', bad);
    s.querySelector('.message').textContent = text;
  }
}
let pendingRender = null,
  needsRender = false,
  pointerActive = false;
document.addEventListener(
  'pointerdown',
  (event) => {
    pointerActive = true;
    const title = event.target.closest?.('[data-rename-node], [data-select-layer]');
    if (
      lastNameClick &&
      (!title ||
        event.button !== 0 ||
        event.shiftKey ||
        event.ctrlKey ||
        event.metaKey ||
        lastNameClick.layerId !== (title.dataset.selectLayer || current().id) ||
        lastNameClick.nodeId !== (title.dataset.renameNode || null))
    )
      lastNameClick = null;
  },
  true,
);
for (const event of ['pointerup', 'pointercancel'])
  document.addEventListener(
    event,
    () => {
      pointerActive = false;
      if (needsRender) scheduleRender();
    },
    true,
  );
function scheduleRender() {
  needsRender = true;
  if (pendingRender === null)
    pendingRender = setTimeout(() => {
      pendingRender = null;
      if (!pointerActive) render();
    }, 0);
}
function commandChange(type, payload, text, deferRender = false, envelope = session.state) {
  if (
    type === 'set-text-settings' &&
    (variableNode(payload.nodeId) ||
      history.doc.fontAssets?.[payload.settings?.font]?.kind === 'variable-glyf-v1')
  ) {
    if (Object.hasOwn(payload.settings || {}, 'font'))
      void preparedChange(
        'set-variable-font',
        {
          nodeId: payload.nodeId,
          fontId: payload.settings.font,
          axes:
            history.doc.fontAssets?.[payload.settings.font]?.kind === 'variable-glyf-v1'
              ? {}
              : null,
        },
        text,
        envelope,
      );
    else void preparedChange(type, payload, text, envelope);
    return null;
  }
  const beforeDocument = history.doc,
    oldPast = history.past.at(-1),
    pointUI = ['translate-points', 'delete-point', 'set-point-mode'].includes(type)
      ? selectionSnapshot(true)
      : null;
  let result = null;
  try {
    result = session.dispatch({
      ...envelope,
      type,
      payload: { layerId: current().id, ...payload },
    });
    if (pointUI && history.doc !== beforeDocument && history.past.at(-1) !== oldPast) {
      selectionHistory.set(history.past.at(-1), pointUI);
      selectionHistory.set(history.doc, selectionSnapshot(true));
    }
    notify(text || 'Updated');
  } catch (error) {
    notify(error.message, true);
  }
  if (deferRender) scheduleRender();
  else render();
  return result;
}
function resetTransient() {
  osTextDrafts.clear();
  gridOpacityDraft = null;
  closeNodeContextMenu();
  pointPanelViews.clear();
  pointFocus = null;
  pointSelection = { nodeId: null, ids: new Set() };
  pointHover = null;
  penSegmentHover = null;
  for (const { face } of editorFonts.values()) document.fonts?.delete(face);
  editorFonts.clear();
  textPreview = null;
  canvasTextEdit = null;
  lastCanvasTextClick = null;
  pendingCanvasTextClick = null;
  editorPicker?.close(true);
  discardPenDraft();
  composing = false;
  nameEdit = null;
  lastNameClick = null;
  stopGraphDrag();
  imageRequest++;
  imagePick = null;
  svgPartPick = null;
  svgPartRequest++;
  layoutDraft = null;
  layoutPreview = false;
  if (drag?.gesture)
    session.dispatch({
      ...session.state,
      type: 'cancel-gesture',
      payload: { gestureId: drag.gesture.gestureId },
    });
  selectedGuide = null;
  snapMatches = [];
  remapBatches.clear();
  characterEdit = null;
  for (const draft of textDrafts.values())
    session.dispatch({
      ...session.state,
      type: 'cancel-text-draft',
      payload: { draftId: draft.state.draftId },
    });
  textDrafts.clear();
  textRanges.clear();
  closeEditorDialog(document.querySelector('.character-choice'));
  drag = null;
  const oldWire = wiring;
  wiring = null;
  releaseWirePointer(oldWire);
  penId = null;
}
// Bounds are shared by rendered controls and pointer scrubbing.
function numericLimits(n, key) {
  if (n.type === 'Keyer' && ['tolerance', 'softness'].includes(key)) return [0, 1];
  if (n.params.tonePower && key === 'tonePower') return [0.1, 10];
  if (hasDotControls(n.type)) {
    if (key === 'dotSize') return [1, 1024];
    if (key === 'dotSides') return [3, 32];
    if (key === 'dotRotation') return [-360, 360];
    if (key === 'starInnerRatio') return [0.05, 0.95];
  }
  if (['Luminance Instances', 'Luminance to Points'].includes(n.type)) {
    if (['minScale', 'maxScale'].includes(key)) return [0, 100];
    if (['minRotation', 'maxRotation'].includes(key)) return [-360, 360];
  }
  if (
    [
      'Halftone',
      'Ordered Dither',
      'Line Screen',
      'Error Diffusion Dither',
      'Luminance Instances',
      'Luminance to Points',
    ].includes(n.type) &&
    key === 'pitch'
  )
    return [1, 1024];
  if (
    [
      'Halftone',
      'Ordered Dither',
      'Line Screen',
      'Error Diffusion Dither',
      'Luminance Instances',
      'Luminance to Points',
    ].includes(n.type) &&
    key === 'gridOrigin'
  )
    return [-100000, 100000];
  if (n.type === 'Radial Gradient Fill') {
    if (key === 'center') return [-100000, 100000];
    if (key === 'radius') return [0.0001, 100000];
  }
  if (n.type === 'Closed Path Offset') {
    if (key === 'miterLimit') return [1, 100];
    if (key === 'amount') return [-100000, 100000];
  }
  if (isTransform(n) && key === 'skew') return [-89.89, 89.89];
  if (n.type === 'Stroke') {
    if (key === 'width') return [0, 100000];
    if (key === 'miterLimit') return [1, 10000];
    if (key === 'dashOffset') return [-100000, 100000];
  }
  if (n.type === 'Expand Stroke' && ['startWidthMultiplier', 'endWidthMultiplier'].includes(key))
    return [0, 4];
  if (n.type === 'Dash Path' && key === 'dashLength') return [0.0001, 100000];
  if (n.type === 'Dash Path' && key === 'gapLength') return [0, 100000];
  if (n.type === 'Dash Path' && key === 'offset') return [-100000, 100000];
  if (n.type === 'Auto Handles' && key === 'strength') return [0, 1];
  if (n.type === 'Taper' && key === 'amount') return [-50, 50];
  if (n.type === 'Bend Points' && key === 'angle') return [-180, 180];
  if (n.type === 'Twist Points' && key === 'angle') return [-360, 360];
  if (n.type === 'Wave Points' && key === 'amplitude') return [-100000, 100000];
  if (n.type === 'Wave Points' && key === 'wavelength') return [0.0001, 100000];
  if (n.type === 'Wave Points' && key === 'phase') return [-360, 360];
  if (n.type === 'Pucker & Bloat' && key === 'amount') return [-100, 100];
  if (n.type === 'Simplify Path' && key === 'tolerance') return [0, 100000];
  if (n.type === 'Smooth Path' && key === 'factor') return [0, 1];
  if (n.type === 'Smooth Path' && key === 'iterations') return [1, 50];
  if (n.type === 'Roughen' && key === 'amount') return [0, 100000];
  if (n.type === 'Roughen' && key === 'seed') return [0, 2147483647];
  if (n.type === 'Resample Path' && key === 'count') return [2, 1024];
  if (n.type === 'Resample Path' && key === 'spacing') return [0.0001, 100000];
  if (n.type === 'Subdivide Path' && key === 'segments') return [1, 32];
  if (n.type === 'Corner Round' && key === 'radius') return [0, 100000];
  if (n.type === 'Trim Path' && ['start', 'end'].includes(key)) return [0, 1];
  if (n.type === 'Set Path Start' && key === 'start') return [0, 1];
  if (n.type === 'Geometry Repeater') {
    if (key === 'count') return [n.operation === 'ae-2d' ? 0 : 1, 256];
    if (key === 'offset') return [-256, 256];
    if (key === 'scale') return [-1000, 1000];
    if (['startOpacity', 'endOpacity'].includes(key)) return [0, 100];
  }
  if (['Polygon', 'Star'].includes(n.type)) {
    if (['sides', 'points'].includes(key)) return [3, 256];
    if (key === 'radius' || key === 'outerRadius')
      return [n.type === 'Star' ? n.params.innerRadius.value : Number.MIN_VALUE, 100000];
    if (key === 'innerRadius') return [Number.MIN_VALUE, n.params.outerRadius.value];
  }
  return [-Number.MAX_VALUE, Number.MAX_VALUE];
}
function numericAttributes(n, key) {
  const [min, max] = numericLimits(n, key);
  return `${min === -Number.MAX_VALUE ? '' : `min="${min}" max="${max}"`} data-scrub-step="${['scale', 'scaleWidth'].includes(key) && isTransform(n) ? 1 : key === 'scale' || (n.type === 'Resample Path' && key === 'spacing') || (n.type === 'Smooth Path' && key === 'factor') || (n.type === 'Auto Handles' && key === 'strength') || (n.type === 'Expand Stroke' && ['startWidthMultiplier', 'endWidthMultiplier'].includes(key)) || (n.type === 'Trim Path' && ['start', 'end'].includes(key)) || (n.type === 'Set Path Start' && key === 'start') ? 0.01 : 1}" title="Drag horizontally to adjust; click to type"`;
}
function numericControlTarget(input) {
  const d = input.dataset,
    payload = { layerId: current().id };
  let command;
  if (d.dimension) {
    command = 'set-document-settings';
    payload.key = d.dimension;
  } else if (d.guidePosition) {
    command = 'set-guide';
    Object.assign(payload, { guideId: d.guidePosition, key: 'position' });
  } else if (d.guideSetting === 'gridSize') {
    command = 'set-guide-setting';
    payload.key = 'gridSize';
  } else if (d.remap) {
    command = 'edit-remap';
    Object.assign(payload, {
      nodeId: d.node,
      action: 'set',
      pointId: d.remap,
      component: d.component,
      axis: Number(d.axis),
    });
  } else if (d.textPathStart) {
    command = 'set-text-settings';
    payload.nodeId = d.textPathStart;
  } else if (
    d.arrangementGap &&
    !current().nodes.find((n) => n.id === d.arrangementGap)?.characterArrangement
  )
    return { localNumeric: true };
  else if (d.layoutField || d.batchAxis !== undefined || d.alignSpacing !== undefined)
    return { localNumeric: true };
  return command
    ? { commandTarget: { kind: 'numeric-command', command, payload, initial: Number(input.value) } }
    : null;
}
function setLocalNumeric(input, value) {
  const d = input.dataset;
  if (d.layoutField) {
    layoutDraft = { ...layoutConfig(), [d.layoutField]: value };
    updateCanvas();
  } else if (d.batchAxis !== undefined) remapBatch(d.node).delta[Number(d.batchAxis)] = value;
  else if (d.alignSpacing !== undefined) editAlignmentSpacing(input);
}
function moveOtherNumeric(d, e, dx) {
  if (!d.started) {
    if (d.commandTarget && d.capture.value !== d.capture.defaultValue)
      d.capture.dispatchEvent(new window.Event('change', { bubbles: true }));
    d.originalInput = d.capture.value;
    d.original = Number(d.capture.value || 0);
    if (!Number.isFinite(d.original)) return;
    if (d.commandTarget) {
      const initial = d.capture.dataset.dimension
        ? displayLengthToDocumentUnits(d.original, d.capture.dataset.dimensionUnit || 'px')
        : d.original;
      d.gesture = beginGraphGesture({ ...d.commandTarget, initial });
      if (!d.gesture) return;
    }
    d.started = true;
    d.capture.setPointerCapture?.(e.pointerId);
    d.capture.classList.add('scrubbing');
  }
  e.preventDefault();
  const value = scrubNumericValues(
    [d.original],
    dx,
    Number(d.capture.dataset.scrubStep),
    Number(d.capture.min || -Number.MAX_VALUE),
    Number(d.capture.max || Number.MAX_VALUE),
  )[0];
  d.capture.value = String(value);
  if (d.localNumeric) {
    setLocalNumeric(d.capture, value);
    return;
  }
  try {
    const commandValue = d.capture.dataset.dimension
      ? displayLengthToDocumentUnits(value, d.capture.dataset.dimensionUnit || 'px')
      : value;
    d.gesture = session.dispatch({
      ...session.state,
      type: 'update-gesture',
      payload: { ...d.gesture, update: { value: commandValue } },
    });
    d.preview = session.previewSnapshot(d.gesture.gestureId).document;
    if (d.commandTarget.command === 'set-document-settings') fitCanvas();
    updateCanvas();
  } catch (error) {
    notify(error.message, true);
  }
}
// Character and layout controls are source edits, not graph parameter fields.
// Resolve their real targets explicitly; the floating palette has no .field wrapper.
function textNumericTarget(input) {
  const data = input.dataset;
  if (data.arrangementGap) {
    const n = current().nodes.find((n) => n.id === data.arrangementGap);
    return n?.characterArrangement ? { textKind: 'arrangement-gap', nodeId: n.id } : null;
  }
  if (data.textLeadingValue || data.textWrapWidth)
    return {
      textKind: data.textLeadingValue ? 'leading' : 'wrap',
      nodeId: data.textLeadingValue || data.textWrapWidth,
    };
  const context = characterContext();
  if (!context) return null;
  if (data.characterKerning !== undefined) {
    const pair = pairContext(context);
    return pair && matchesPairInput(input, context, pair)
      ? {
          textKind: 'kerning',
          nodeId: context.n.id,
          ids: pair.ids,
          pairKey: pair.key,
          targetText: context.n.text,
        }
      : null;
  }
  const textKind =
      data.characterAxis !== undefined
        ? 'offset'
        : data.characterTransform
          ? 'transform'
          : data.characterStyle && data.characterStyle !== 'font'
            ? 'style'
            : null,
    rows = textKind === 'style' ? context.typographySelected : context.selected;
  if (
    !textKind ||
    !rows.length ||
    (textKind === 'style' ? context.typographyReason : context.layout.reason)
  )
    return null;
  return {
    textKind,
    nodeId: context.n.id,
    ids: rows.map((g) => g.id),
    key: data.characterTransform || data.characterStyle,
    axis: Number(data.characterAxis || 0),
    factor: ['width', 'height'].includes(data.characterStyle) ? 100 : 1,
  };
}
function textNumericState(d) {
  const layer = displayLayer(),
    n = layer.nodes.find((n) => n.id === d.nodeId);
  if (n?.type !== 'Text' || (d.started && n.text !== d.sourceText)) return null;
  if (d.textKind === 'arrangement-gap')
    return n.characterArrangement ? { n, values: [n.characterArrangement.gap] } : null;
  if (d.textKind === 'leading')
    return n.leading?.mode === 'manual' ? { n, values: [n.leading.value] } : null;
  if (d.textKind === 'wrap') return n.textWrap ? { n, values: [wrapExtent(n.textWrap)] } : null;
  const context = characterContext(evaluate(layer, fonts, displayDocument().imageAssets));
  if (d.textKind === 'kerning') {
    const pair = pairContext(context);
    return context?.n.id === d.nodeId && n.text === d.targetText && pair?.key === d.pairKey
      ? { n, values: [pair.value], manual: pair.manual }
      : null;
  }
  if (
    context?.n.id !== d.nodeId ||
    (d.textKind === 'style' ? context.typographyReason : context.layout.reason) ||
    characterEdit.selected.size !== d.ids.length ||
    !d.ids.every((id) => characterEdit.selected.has(id))
  )
    return null;
  const selectedRows = d.textKind === 'style' ? context.typographySelected : context.selected,
    rows = d.ids.map((id) => selectedRows.find((g) => g.id === id));
  if (rows.some((g) => !g)) return null;
  return {
    n,
    rows,
    values: rows.map((g) => (d.textKind === 'offset' ? g.offset[d.axis] : g[d.key] * d.factor)),
  };
}
function moveTextNumeric(d, event, dx) {
  let state = textNumericState(d);
  if (!state) {
    cancelInteraction();
    return;
  }
  if (!d.started && d.capture.value !== d.capture.defaultValue) {
    const text = d.capture.value,
      expected = Number(text) / (d.factor || 1);
    d.capture.dispatchEvent(new window.Event('change', { bubbles: true }));
    state = textNumericState(d);
    if (
      !text.trim() ||
      !Number.isFinite(expected) ||
      !state ||
      !state.values.every(
        (value) => Math.abs(value - expected) <= 1e-12 * Math.max(1, Math.abs(expected)),
      )
    ) {
      cancelInteraction();
      return;
    }
  }
  if (!d.started) {
    d.sourceText = state.n.text;
    d.originalValues = state.values;
    d.originalManual = state.manual;
    d.originalInput =
      d.textKind === 'kerning' && !state.manual ? '' : mixedNumericDisplay(state.values);
    d.gesture = beginGraphGesture({
      kind: 'text-values',
      layerId: d.layerId,
      nodeId: d.nodeId,
      textKind: d.textKind,
      ids: d.ids,
      key: d.key,
      axis: d.axis,
      pairKey: d.pairKey,
    });
    if (!d.gesture) {
      cancelInteraction();
      return;
    }
    d.started = true;
    d.capture.setPointerCapture?.(event.pointerId);
    d.capture.classList.add('scrubbing');
  }
  event.preventDefault();
  const values = scrubNumericValues(
    d.originalValues,
    dx,
    Number(d.capture.dataset.scrubStep),
    Number(d.capture.min),
    Number(d.capture.max),
  );
  try {
    d.gesture = session.dispatch({
      ...session.state,
      type: 'update-gesture',
      payload: {
        ...d.gesture,
        update: {
          values: d.textKind === 'style' ? values.map((value) => value / d.factor) : values,
        },
      },
    });
    d.preview = session.previewSnapshot(d.gesture.gestureId).document;
  } catch (error) {
    notify(error.message, true);
    return;
  }
  d.capture.value =
    d.textKind === 'kerning' && !d.originalManual && values[0] === d.originalValues[0]
      ? ''
      : mixedNumericDisplay(values);
  updateCanvas();
}
function mixedNumericDisplay(values) {
  return values.some((value) => value !== values[0]) ? '' : String(values[0]);
}
const nodeTypeLabel = (type) => (type === 'Join Geometry' ? 'Join Paths' : type);
const nodeDisplayName = (n) =>
  n.type === 'Join Geometry' && n.name === 'Join Geometry' ? 'Join Paths' : n.name;

function booleanInputNote(n) {
  return n.type === 'Boolean'
    ? '<p class="font-note"><span aria-hidden="true">\u2139</span> Closed paths only. Subtract removes inputs after A from A; Difference is symmetric difference.</p>'
    : '';
}
function editorParameters(n) {
  if (!isTransform(n)) {
    const entries = activeParameters(n);
    if (n.type === 'Boolean' && !n.booleanInputOrder && (n.params.b.source || n.params.b.locked))
      entries.push([booleanPendingInputKey, { type: 'Path', value: null, locked: false }]);
    return entries;
  }
  const keys = [
    n.type === 'Transform' ? 'path' : 'vector',
    'anchor',
    'position',
    'uniformScale',
    'scale',
    'scaleWidth',
    'skew',
    'skewAxis',
    'rotation',
    'offset',
  ];
  return keys.map((key) => [key, parameter(n, key)]);
}
function field(n, key, q) {
  let source = current().nodes.find((x) => x.id === q.source),
    locked =
      q.locked ||
      (n.type === 'Text' && key === 'letterSpacing' && maintainedAxis(n) !== null) ||
      !!q.source ||
      (isTransform(n) &&
        key === 'scaleWidth' &&
        (renderEvaluation?.transformValues(n.id).uniformScale ?? true));
  const factor = isTransform(n) && ['scale', 'scaleWidth'].includes(key) ? 100 : 1;
  let name =
    n.type === 'Join Geometry'
      ? !q.source && !q.locked && key === n.joinInputOrder.at(-1)
        ? 'Add path'
        : `Path ${n.joinInputOrder.indexOf(key) + 1}`
      : [
            'Halftone',
            'Ordered Dither',
            'Line Screen',
            'Error Diffusion Dither',
            'Luminance Instances',
            'Luminance to Points',
          ].includes(n.type)
        ? {
            path: 'Source fill',
            motif: 'Motif',
            minScale: 'Scale at white',
            maxScale: 'Scale at black',
            minRotation: 'Angle at white (°)',
            maxRotation: 'Angle at black (°)',
            pitch:
              n.type === 'Luminance to Points'
                ? 'Point spacing (px; smaller = denser)'
                : hasDotControls(n.type)
                  ? 'Dot spacing (px; smaller = denser)'
                  : 'Cell pitch (px)',
            dotSize: n.type === 'Halftone' ? 'Maximum dot size (px)' : 'Dot size (px)',
            dotSides: 'Polygon sides / star points',
            dotRotation: 'Dot rotation (degrees)',
            starInnerRatio: 'Star inner radius ratio',
            invert: 'Invert luminance',
            tonePower: 'Tone power · darkness response',
            gridOrigin: 'Grid origin · cell (0, 0) top-left',
          }[key] || key
        : n.type === 'Instance on Points'
          ? { path: 'Source', points: 'Points', alignTangent: 'Align Tangent' }[key] || key
          : isTransform(n)
            ? {
                anchor: 'Anchor Point',
                position: 'Position',
                uniformScale: 'Uniform Scale',
                scale: 'Scale Height (%)',
                scaleWidth: 'Scale Width (%)',
                skew: 'Skew',
                skewAxis: 'Skew Axis',
                rotation: 'Rotation',
                offset: 'Legacy Offset',
              }[key] || key
            : n.type === 'Expand Stroke'
              ? {
                  startWidthMultiplier: 'Start width multiplier',
                  endWidthMultiplier: 'End width multiplier',
                }[key] || key
              : n.type === 'Dash Path'
                ? {
                    dashLength: 'Dash Length (px)',
                    gapLength: 'Gap Length (px)',
                    offset: 'Pattern Offset (px)',
                  }[key] || key
                : n.type === 'Simplify Path' && key === 'tolerance'
                  ? 'Tolerance (px)'
                  : n.type === 'Smooth Path'
                    ? {
                        factor: 'Factor',
                        iterations: 'Iterations',
                        keepEndpoints: 'Keep Endpoints',
                      }[key] || key
                    : n.type === 'Trim Path'
                      ? { start: 'Start (0–1)', end: 'End (0–1)' }[key] || key
                      : n.type === 'Set Path Start' && key === 'start'
                        ? 'Start (0–1)'
                        : n.type === 'Set Closure' && key === 'closed'
                          ? 'Closed'
                          : n.type === 'Roughen'
                            ? {
                                amount: 'Amount (px per axis)',
                                seed: 'Seed',
                                keepEndpoints: 'Keep Endpoints',
                              }[key] || key
                            : n.type === 'Resample Path' && key === 'count'
                              ? 'Points per Contour'
                              : n.type === 'Resample Path' && key === 'spacing'
                                ? 'Maximum spacing (px)'
                                : n.type === 'Wave Points'
                                  ? {
                                      amplitude: 'Amplitude (px)',
                                      wavelength: 'Wavelength (px)',
                                      phase: 'Phase (°)',
                                      keepEndpoints: 'Keep Endpoints',
                                    }[key] || key
                                  : n.type === 'Taper' && key === 'amount'
                                    ? 'Amount (%)'
                                    : n.type === 'Bend Points' && key === 'angle'
                                      ? 'Angle (°)'
                                      : n.type === 'Twist Points' && key === 'angle'
                                        ? 'Angle (°)'
                                        : n.type === 'Pucker & Bloat'
                                          ? {
                                              amount: 'Amount (%)',
                                              autoCenter: 'Automatic Center',
                                              center: 'Center',
                                            }[key] || key
                                          : n.type === 'Subdivide Path' && key === 'segments'
                                            ? 'Segments per Edge'
                                            : n.type === 'Mirror'
                                              ? {
                                                  center: 'Axis Center',
                                                  angle: 'Axis Angle (°)',
                                                  keepOriginal: 'Keep Original',
                                                }[key] || key
                                              : key === 'center' &&
                                                  [
                                                    'Circle',
                                                    'Ellipse',
                                                    'Rectangle',
                                                    'Polygon',
                                                    'Star',
                                                  ].includes(n.type)
                                                ? 'Position'
                                                : key.includes('.')
                                                  ? key.split('.').at(-1)
                                                  : key;
  if (n.type === 'Keyer')
    name =
      { path: 'Image', keyColor: 'Key color', tolerance: 'Range', softness: 'Softness' }[key] ||
      key;
  if (n.type === 'Mask') name = { path: 'Appearance', mask: 'Shape', invert: 'Invert' }[key] || key;
  if (n.type === 'Geometry Repeater' && n.operation === 'ae-2d')
    name =
      {
        count: 'Copies',
        offset: 'Offset',
        anchor: 'Anchor',
        position: 'Position',
        scale: 'Scale (%)',
        rotation: 'Rotation (degrees)',
        startOpacity: 'Start Opacity (%)',
        endOpacity: 'End Opacity (%)',
        copiesAbove: 'Copies Above',
      }[key] || key;
  if (n.type === 'Boolean') {
    const index = booleanInputKeys(n).indexOf(key);
    name =
      key === booleanPendingInputKey ||
      (!q.source && !q.locked && key !== 'b' && key === booleanInputKeys(n).at(-1))
        ? 'Add input'
        : index >= 0 && index < 26
          ? String.fromCharCode(65 + index)
          : `Input ${index + 1}`;
  }
  let control;
  const connectLabel =
    n.type === 'Convert to Path' && key === 'path'
      ? 'Connect Path or PaintedPath…'
      : `Connect ${q.type}…`;
  let displayed = q.value;
  if (
    isTransform(n) &&
    !n.params[key] &&
    ['position', 'scaleWidth'].includes(key) &&
    renderEvaluation
  )
    displayed = renderEvaluation.transformValues(n.id)[key];
  if (source && renderEvaluation) {
    try {
      displayed = renderEvaluation.run(source.id, q.sourcePort);
    } catch {}
  }
  if (['Path', 'PaintedPath', 'Image'].includes(q.type))
    control = `<button class="mini" data-search-source aria-label="Search ${esc(n.name + ' ' + name)} sources" ${q.locked || current().locked ? 'disabled' : ''}>Search…</button><select data-path-source aria-label="${esc(n.name + ' ' + name)}"><option value="">${connectLabel}</option>${current()
      .nodes.filter((x) => socketCompatibleFor(x, n, key) && x.id !== n.id)
      .map(
        (x) =>
          `<option value="${x.id}" ${q.source === x.id ? 'selected' : ''}>${esc(x.name)} · ${x.id.slice(-4)}</option>`,
      )
      .join('')}</select>`;
  else if (q.type === 'Color') {
    const color = displayed || { hex: current().fill, alpha: 1 };
    control = `<input type="color" data-color="hex" aria-label="${esc(n.name + ' ' + name)} color" value="${esc(color.hex)}" ${locked ? 'disabled' : ''}>${n.type === 'Keyer' && key === 'keyColor' ? '' : `<input type="number" data-scrub-step="0.01" data-color="alpha" aria-label="${esc(n.name + ' ' + name)} alpha" title="Alpha: 0 transparent, 1 opaque" min="0" max="1" step="0.01" value="${color.alpha}" ${locked ? 'disabled' : ''}>`}`;
  } else if (q.type === 'Boolean')
    control = `<input aria-label="${esc(name)}" data-value type="checkbox" ${displayed ? 'checked' : ''} ${locked ? 'disabled' : ''}>`;
  else if (['Scalar', 'Vector2'].includes(q.type) && (q.expression || q.source)) {
    const baseLocked =
      current().locked ||
      q.locked ||
      (n.type === 'Text' && key === 'letterSpacing' && maintainedAxis(n) !== null) ||
      (isTransform(n) &&
        key === 'scaleWidth' &&
        (renderEvaluation?.transformValues(n.id).uniformScale ?? true));
    const baseInputs = (q.type === 'Vector2' ? q.value : [q.value])
      .map(
        (v, i) =>
          `<label class="base-component">${q.type === 'Vector2' ? (i ? 'Base Y' : 'Base X') : 'Base'}<input type="number" step="any" ${numericAttributes(n, key)} data-value-factor="${factor}" aria-label="${esc(n.name + ' ' + name + ' base' + (q.type === 'Vector2' ? (i ? ' Y' : ' X') : ''))}" data-base data-axis="${i}" value="${v * factor}" ${baseLocked ? 'disabled' : ''}></label>`,
      )
      .join('');
    let result;
    try {
      const effective = renderEvaluation.parameterValue(n.id, key);
      result = (q.type === 'Vector2' ? effective : [effective])
        .map((v) => Number((v * factor).toFixed(3)))
        .join(', ');
    } catch {
      result = '— Cannot evaluate';
    }
    control = `<div class="authored-control"><div class="base-inputs">${baseInputs}</div><output class="parameter-result" aria-label="${esc(name)} result">${q.source ? 'Connected result' : 'Result'} · ${esc(result)}</output>${q.source ? '<small class="base-hint">Base saved · connection takes priority.</small>' : ''}${q.expression ? `<details class="parameter-expression"><summary>${q.source ? 'Expression paused by connection' : 'Expression'}</summary><input class="expression" aria-label="${esc(name)} expression" data-expression value="${esc(q.expression)}" ${baseLocked ? 'disabled' : ''}><small class="base-hint">${q.type === 'Vector2' ? 'X; Y · value = each component’s base' : 'value = base'}${factor !== 1 ? ' · expression units: ×, display: %' : ''}</small></details>` : ''}</div>`;
  } else
    control = (q.type === 'Vector2' ? displayed : [displayed])
      .map(
        (v, i) =>
          `<input type="number" step="${n.type === 'Subdivide Path' && key === 'segments' ? '1' : 'any'}" ${numericAttributes(n, key)} data-value-factor="${factor}" aria-label="${esc(n.name + ' ' + name + (q.type === 'Vector2' ? (i ? ' Y' : ' X') : ''))}" data-value data-axis="${i}" value="${Number(n.type === 'Radial Gradient Fill' ? v : ((v * factor).toFixed?.(3) ?? v))}" ${locked ? 'disabled' : ''}>`,
      )
      .join('');
  return `<div class="field ${q.source ? 'connected' : ''}" data-node="${n.id}" data-key="${esc(key)}" data-type="${q.type}"><label>${esc(name)}${n.type === 'Stroke' && key === 'width' ? ' (px)' : ''}</label><div class="field-tools">${!['Path', 'PaintedPath', 'Image'].includes(q.type) ? `<button class="mini" data-connect-menu title="Connect ${esc(name)} input" aria-label="Connect ${esc(n.name + ' ' + name)} input">↗</button>` : ''}<button class="lock" data-lock title="${q.locked ? 'Unlock' : 'Lock'} ${esc(name)}">${q.locked ? '🔒' : '○'}</button>${['Scalar', 'Vector2'].includes(q.type) ? `<button class="mini" data-fx title="Expression and parameter references: Vector2 uses X; Y">ƒ</button>` : ''}</div><div class="field-controls" title="${q.source ? esc('Saved fallback: ' + JSON.stringify(q.value)) : ''}">${q.source ? `<button class="socket input" title="Drag to empty space to disconnect; right-click to disconnect" aria-label="Disconnect ${esc(name)}"></button>` : ''}${control}</div>${source && !['Path', 'PaintedPath', 'Image'].includes(q.type) ? `<span class="source-name">← ${esc(source.name + (q.sourcePort ? ' ' + (outputPorts(source).find((p) => p.id === q.sourcePort)?.label || q.sourcePort) : ''))} · connected value</span>` : ''}</div>`;
}
// Presentation-only state; Canvas selection remains the single point selection owner.
const pointPanelViews = new Map();
const POINT_PANEL_PAGE_SIZE = 24;
function pointRowClass(n, id) {
  return `${selectedPointIds(n).includes(id) ? 'point-selected' : ''} ${pointFocus?.nodeId === n.id && pointFocus.pointId === id ? 'point-active' : ''}`;
}
function pointPanel(n, entries, renderRows) {
  const selectedIds = new Set(selectedPointIds(n));
  const chosen = entries.filter(({ pt }) => selectedIds.has(pt.id));
  const view = pointPanelViews.get(n.id) || { all: false, page: 0 };
  const candidates = view.all ? entries : chosen;
  const pages = Math.max(1, Math.ceil(candidates.length / POINT_PANEL_PAGE_SIZE));
  const page = Math.min(view.page, pages - 1);
  const rows = candidates.slice(page * POINT_PANEL_PAGE_SIZE, (page + 1) * POINT_PANEL_PAGE_SIZE);
  return `<div class="point-panel" data-point-panel="${n.id}"><p class="point-panel-summary">${entries.length} points · ${chosen.length} Canvas selected</p>${!chosen.length && !view.all ? '<p class="point-panel-hint">Select points on the Canvas to edit their values. Shift-click adds or removes points.</p>' : ''}<button class="mini" data-point-panel-mode="${n.id}">${view.all ? 'Show selected points' : 'Browse all points'}</button>${candidates.length > POINT_PANEL_PAGE_SIZE ? `<div class="point-panel-pages"><button class="mini" data-point-panel-page="${n.id}" data-page="${page - 1}" ${page === 0 ? 'disabled' : ''}>Previous</button><span>Page ${page + 1} / ${pages}</span><button class="mini" data-point-panel-page="${n.id}" data-page="${page + 1}" ${page === pages - 1 ? 'disabled' : ''}>Next</button></div>` : ''}<div class="point-panel-rows">${renderRows(rows)}</div></div>`;
}
function ownedHandlePolarFields(n, pt, key, index) {
  const q = pt[key];
  let value = q.value,
    unavailable = false;
  try {
    value = renderEvaluation?.parameterValue(n.id, `${pt.id}.${key}`) ?? value;
  } catch {
    unavailable = true;
  }
  let polar;
  try {
    polar = handlePolar(value);
  } catch {
    polar = { angle: 0, length: 0 };
    unavailable = true;
  }
  const locked =
    current().locked ||
    n.locked ||
    n.bypassed ||
    q.locked ||
    q.source ||
    q.expression ||
    unavailable;
  const name = key === 'handleA' ? 'Incoming' : 'Outgoing';
  return `<div class="field handle-polar"><label>${name}</label><div class="field-controls">${['angle', 'length'].map((component) => `<label>${component === 'angle' ? 'Angle°' : 'Length'}<input type="number" step="any" ${component === 'length' ? 'min="0"' : ''} data-handle-polar="${component}" data-node="${n.id}" data-point="${pt.id}" data-handle="${key}" aria-label="Point ${index + 1} ${name} ${component}" title="${component === 'angle' ? 'Degrees clockwise from +X; zero-length angle is 0 and unavailable' : 'Document units; growing a zero handle starts along +X'}" value="${Number(polar[component].toFixed(3))}" ${locked || (component === 'angle' && !polar.length) ? 'disabled' : ''}></label>`).join('')}</div></div>`;
}
function ownedPointFields(n) {
  if (!n.points.length) return '';
  return pointPanel(
    n,
    n.points.map((pt, index) => ({ pt, index })),
    (rows) =>
      rows
        .map(
          ({ pt, index: i }) =>
            `<details data-point-row="${pt.id}" class="${pointRowClass(n, pt.id)}" data-detail="${pt.id}" open><summary><button class="mini" data-select-panel-point="${pt.id}" data-node="${n.id}" aria-pressed="${selectedPointIds(n).includes(pt.id)}">Point ${i + 1}</button> <button class="mini" data-delete-point="${pt.id}" data-node="${n.id}" title="Delete point">×</button></summary>${['position', 'handleA', 'handleB'].map((k) => field(n, `${pt.id}.${k}`, pt[k]) + (k === 'position' ? '' : ownedHandlePolarFields(n, pt, k, i))).join('')}<select data-handle-type="${pt.id}" data-node="${n.id}" aria-label="Point ${i + 1} handle type">${['corner', 'smooth', 'symmetric'].map((t) => `<option ${pt.handleType === t ? 'selected' : ''}>${t}</option>`).join('')}</select></details>`,
        )
        .join(''),
  );
}
function prepareNumericInputs(root) {
  for (const input of root.querySelectorAll('input[type="number"]')) {
    if (!input.dataset.scrubStep)
      input.dataset.scrubStep = input.step && input.step !== 'any' ? input.step : '1';
    input.title ||= 'Drag horizontally to adjust; click to type';
  }
}
function revealActivePointRow() {
  for (const row of document.querySelectorAll('.graph .point-panel-rows > .point-active')) {
    row.open = true;
    row.parentElement.scrollTop = row.offsetTop;
  }
}
function refreshPointPanels() {
  // Do not rebuild the stage or disturb an active numeric/IME edit.
  if (composing) return;
  const result = evaluate(
    current(),
    resolveFonts(history.doc, activeFonts()),
    history.doc.imageAssets,
  );
  for (const panel of document.querySelectorAll(
    '.graph [data-point-panel], .inspector [data-point-panel]',
  )) {
    if (panel.contains(document.activeElement)) continue;
    const n = current().nodes.find((node) => node.id === panel.dataset.pointPanel);
    if (!n) continue;
    panel.outerHTML = n.type === 'Path Remap' ? remapFields(n, result, true) : ownedPointFields(n);
  }
  for (const panel of document.querySelectorAll(
    '.graph [data-point-panel], .inspector [data-point-panel]',
  ))
    prepareNumericInputs(panel);
  revealActivePointRow();
  requestAnimationFrame(() => {
    updateGraphBounds();
    drawWires();
  });
}
function remapFields(n, result, panelOnly = false) {
  let input;
  try {
    if (n.params.path.source) input = result.run(n.params.path.source);
  } catch {}
  if (!input) return '<p class="muted">Connect a path to edit its vertices.</p>';
  const batch = remapBatch(n.id);
  const entries = input.contours.flatMap((c, ci) => c.points.map((pt, pi) => ({ pt, ci, pi })));
  const panel = pointPanel(n, entries, (rows) =>
    rows
      .map(
        ({ pt, ci, pi }) =>
          `<details data-point-row="${pt.id}" class="${pointRowClass(n, pt.id)}" data-detail="remap-${pt.id}" open><summary><input type="checkbox" data-batch-point="${pt.id}" data-node="${n.id}" aria-label="Select contour ${ci + 1} point ${pi + 1} for batch edit" ${batch.selected.has(pt.id) ? 'checked' : ''}> <button class="mini" data-select-panel-point="${pt.id}" data-node="${n.id}" aria-pressed="${selectedPointIds(n).includes(pt.id)}">Contour ${ci + 1} · Point ${pi + 1}</button></summary>${[
            'position',
            'handleA',
            'handleB',
          ]
            .map((k) => {
              let edit = n.edits.find((e) => e.pointId === pt.id);
              if (edit?.params) return field(n, `remap.${pt.id}.${k}`, edit.params[k]);
              return `<div class="field"><label>Δ ${k}</label>${[0, 1].map((axis) => `<input aria-label="Remap ${pi + 1} ${k} ${axis ? 'Y' : 'X'}" type="number" step="any" data-remap="${pt.id}" data-component="${k}" data-axis="${axis}" data-node="${n.id}" value="${edit?.[k]?.[axis] || 0}">`).join('')}</div>`;
            })
            .join(
              '',
            )}${n.edits.some((e) => e.pointId === pt.id && e.params) ? '' : `<button class="mini" data-enable-remap="${pt.id}" data-node="${n.id}">Enable typed point controls</button>`}${n.edits.some((e) => e.pointId === pt.id && input.unstable && e.fingerprint !== input.fingerprint) ? `<button class="mini danger" data-rebind-remap="${pt.id}" data-node="${n.id}">Rebind retained edit to this point</button>` : ''}</details>`,
      )
      .join(''),
  );
  if (panelOnly) return panel;
  return (
    panel +
    `<div class="batch-edit"><p>${batch.selected.size} batch checked · Δ position</p>${batch.selected.size ? `<div class="field-controls">${batch.delta.map((v, axis) => `<input type="number" step="any" data-batch-axis="${axis}" data-node="${n.id}" aria-label="Batch delta ${axis ? 'Y' : 'X'}" value="${v}">`).join('')}</div><button class="mini" data-apply-batch="${n.id}">Move selected points</button><button class="mini" data-clear-batch="${n.id}">Clear batch selection</button>` : '<p class="point-panel-hint">Check points in the list for batch Δ editing.</p>'}</div><button class="mini" data-clear-edits="${n.id}">Clear vertex edits</button>`
  );
}
// A name edit belongs to stable document IDs, never the currently selected row.
function nameInput(kind, id) {
  return `<input class="inline-name" data-rename-name="${id}" aria-label="${kind === 'node' ? 'Node' : 'Layer'} name" value="${esc(nameEdit.value)}" maxlength="100000" spellcheck="false">`;
}
function beginNameEdit(layerId, nodeId = null) {
  if (penDraft && !finishPenDraft()) return;
  const l = history.doc.layers.find((l) => l.id === layerId),
    target = nodeId ? l?.nodes.find((n) => n.id === nodeId) : l;
  if (!target || l.locked) {
    notify('Composition is locked', true);
    return;
  }
  cancelInteraction();
  nameEdit = { layerId, nodeId, original: target.name, value: target.name, composing: false };
  lastNameClick = null;
  render();
  const input = document.querySelector('[data-rename-name]');
  input.focus({ preventScroll: true });
  input.select();
}
function finishNameEdit(commit, deferred = false) {
  const edit = nameEdit;
  if (!edit) return;
  if (commit && edit.composing) {
    edit.pendingCommit = true;
    return;
  }
  nameEdit = null;
  lastNameClick = null;
  if (commit) {
    const l = history.doc.layers.find((l) => l.id === edit.layerId),
      target = edit.nodeId ? l?.nodes.find((n) => n.id === edit.nodeId) : l,
      value = edit.value.trim();
    if (!target || l.locked || target.name !== edit.original)
      notify('Name edit cancelled: target changed or composition is locked', true);
    else if (!value) notify('Name cannot be empty', true);
    else {
      commandChange(
        'rename',
        {
          layerId: edit.layerId,
          ...(edit.nodeId ? { nodeId: edit.nodeId } : {}),
          name: value,
          expectedName: edit.original,
        },
        'Name updated',
        true,
      );
    }
  }
  if (deferred) scheduleRender();
  else render();
}
// Structural renders and graph pointer capture can remove the native dblclick target.
// Recognize two completed, stationary title clicks by stable IDs as well.
function nameClicked(layerId, nodeId, event) {
  const now = performance.now(),
    previous = lastNameClick;
  lastNameClick = { layerId, nodeId, time: now, x: event.clientX, y: event.clientY };
  if (
    previous &&
    previous.layerId === layerId &&
    previous.nodeId === nodeId &&
    now - previous.time < 500 &&
    Math.hypot(event.clientX - previous.x, event.clientY - previous.y) < 5
  ) {
    beginNameEdit(layerId, nodeId);
    return true;
  }
  return false;
}
function bypassControl(n, compact = false) {
  const contract = bypassContract(n, current()),
    label = n.bypassed ? 'Restore effect' : 'Bypass effect';
  return `<button class="mini node-bypass ${n.bypassed ? 'active' : ''}" data-bypass="${n.id}" aria-label="${label}: ${esc(nodeDisplayName(n))}" aria-pressed="${!!n.bypassed}" title="${esc(contract.reason)}" ${current().locked || !contract.supported ? 'disabled' : ''}>${compact ? (n.bypassed ? 'Bypassed' : 'Bypass') : label}</button>`;
}
function nodeHeader(n) {
  return `<div class="node-head"><button class="node-collapse" data-collapse="${n.id}" aria-expanded="${!n.collapsed}" aria-label="Toggle ${esc(nodeDisplayName(n))} node details" title="${n.collapsed ? 'Expand' : 'Collapse'} node">${n.collapsed ? '▸' : '▾'}</button>${nameEdit?.nodeId === n.id && nameEdit.layerId === current().id ? nameInput('node', n.id) : `<strong data-rename-node="${n.id}" tabindex="0" role="button" aria-label="Rename ${esc(nodeDisplayName(n))}" title="Double-click or F2 to rename; right-click for node actions">${esc(nodeDisplayName(n))}</strong>`}${bypassControl(n, true)}<span class="type-tag">${outputType(n) || 'Scalar X/Y'}</span>${outputPorts(
    n,
  )
    .map(
      (port) =>
        `<span class="output-port">${port.label}<button class="socket output" data-source="${n.id}" ${port.id ? `data-source-port="${port.id}"` : ''} aria-label="${esc(n.name + (port.label ? ' ' + port.label : ''))} output" title="${port.type} output${port.id === 'position' ? ': evaluated Text Position, before downstream Transform' : ': drag to a compatible field'}"></button></span>`,
    )
    .join('')}</div>`;
}
function compactFields(n) {
  return activeParameters(n)
    .map(([key, q]) => {
      const label =
        n.type === 'Instance on Points'
          ? { path: 'Source', points: 'Points', alignTangent: 'Align Tangent' }[key] || key
          : key;
      return `<div class="field compact-field ${q.source ? 'connected' : ''}" data-node="${n.id}" data-key="${esc(key)}" data-type="${q.type}"><button class="socket ${q.source ? 'input' : 'available-input'}" ${q.source ? '' : 'data-connect-menu'} aria-label="${esc(n.name + ' ' + label)} input" title="${esc(label)} · ${q.type}${q.source ? ' · connected; drag or right-click to disconnect' : ' · connect input'}"></button></div>`;
    })
    .join('');
}
function textPathControls(n) {
  const attachment = n.pathAttachment,
    targets = textPathTargets(current(), n.id, fonts, history.doc.imageAssets),
    value = attachment ? JSON.stringify([attachment.nodeId, attachment.contourId]) : '',
    missing =
      attachment &&
      !targets.some((t) => t.nodeId === attachment.nodeId && t.contourId === attachment.contourId),
    disabled = current().locked ? 'disabled' : '';
  return `<section class="text-path-controls" aria-label="Type on Path"><h4>Type on Path</h4><label>Explicit path · contour<select data-text-path="${n.id}" aria-label="Type on Path contour" ${disabled}><option value="">Detached (ordinary text)</option>${missing ? `<option value="${esc(value)}" selected disabled>Unavailable: ${esc(attachment.contourId)}</option>` : ''}${targets.map((t) => `<option value="${esc(JSON.stringify([t.nodeId, t.contourId]))}" ${value === JSON.stringify([t.nodeId, t.contourId]) ? 'selected' : ''}>${esc(t.name)} · ${esc(t.contourId)} · ${t.closed ? 'closed' : 'open'}</option>`).join('')}</select></label>${attachment ? `<label>Start unit<select data-text-path-mode="${n.id}" aria-label="Type on Path start unit" ${disabled}>${['distance', 'normalized'].map((mode) => `<option value="${mode}" ${mode === attachment.startMode ? 'selected' : ''}>${mode === 'distance' ? 'Distance px' : 'Normalized (0–1)'}</option>`).join('')}</select></label><label>Start anchor<input type="number" step="any" data-text-path-start="${n.id}" aria-label="Type on Path start" value="${attachment.start}" ${disabled}></label><label class="text-path-reverse"><input type="checkbox" data-text-path-reversed="${n.id}" aria-label="Type on Path reverse" ${attachment.reversed ? 'checked' : ''} ${disabled}>Reverse traversal</label><p class="font-note">Alignment below anchors the full advance span at this start. Position and flat reference controls are retained for detach. Typography reflows advances; character rotation/scale and local XY are applied before tangent projection. Canvas character dragging is unavailable while attached.</p>` : ''}<p class="font-note">One horizontal line on one stable contour. Closed paths wrap once; overflow rejects the edit. Generated/unstable contours and suspended Path Remap edits are unavailable. Detach restores original flat Position, references and character adjustments.</p></section>`;
}
// This source arrangement is independent from transient canvas snap guides.
function arrangementLayout(n) {
  try {
    return (
      renderEvaluation || evaluate(displayLayer(), fonts, displayDocument().imageAssets)
    ).textLayout(n.id);
  } catch (error) {
    return { arrangementEligibility: { eligible: false, reason: error.message } };
  }
}
function maintainedAxis(n) {
  return n?.characterArrangement && arrangementLayout(n)?.arrangementStatus?.active
    ? n.characterArrangement.axis === 'y'
      ? 1
      : 0
    : null;
}
function arrangementControls(n) {
  const layout = arrangementLayout(n),
    status = layout?.arrangementStatus,
    eligibility = layout?.arrangementEligibility,
    reason = current().locked ? 'Composition is locked' : eligibility?.reason,
    disabled = current().locked || !eligibility?.eligible;
  return `<section class="character-arrangement" aria-label="Maintain equal ink gaps"><h4>Maintain equal ink gaps</h4><label>Gap · Text-local px<input type="number" min="0" max="100000" step="any" data-scrub-step="1" data-arrangement-gap="${n.id}" aria-label="Maintained ink gap" value="${n.characterArrangement?.gap ?? 10}" ${current().locked ? 'disabled' : ''}></label><button data-arrangement-apply="${n.id}" ${disabled ? 'disabled' : ''}>${n.characterArrangement ? 'Reapply to current text' : 'Apply'}</button>${n.characterArrangement ? `<button data-arrangement-clear="${n.id}" ${current().locked ? 'disabled' : ''}>Disable maintained gaps</button>` : ''}<output data-arrangement-status role="status">${esc(status?.suspended ? 'Suspended: ' + status.reason : status?.active ? 'Active · equal ink gaps' : reason || 'Off · Apply to the whole line or column')}</output><p class="font-note">Text-local ink gaps; downstream effects may change visible spacing. Font and size remain editable. Maintained ink gaps control spacing; disable to edit tracking, kerning or axial positions. Disable restores authored positions and may move letters.</p></section>`;
}
function flushArrangementText(nodeId, scope) {
  const draft = textDrafts.get(nodeId);
  if (draft?.composition) {
    notify('Finish or cancel text composition first', true);
    return false;
  }
  const el = (scope || document).querySelector(`[data-text="${nodeId}"]`),
    n = current().nodes.find((n) => n.id === nodeId);
  if (el && (el.value !== n.text || draft?.state.changed))
    el.dispatchEvent(new Event('change', { bubbles: true }));
  return (
    !textDrafts.get(nodeId)?.state.changed &&
    (!el || el.value === current().nodes.find((n) => n.id === nodeId)?.text)
  );
}
async function selectVariableInstance(el, preview = false) {
  const n = current().nodes.find((n) => n.id === el.dataset.variableInstance),
    instances = fonts[n?.font]?.nectVariable?.namedInstances;
  if (!n?.variation || !/^(0|[1-9][0-9]*)$/.test(el.value)) return;
  const instance = instances?.[Number(el.value)];
  if (!instance) return;
  await preparedChange(
    'set-variable-font',
    { nodeId: n.id, fontId: n.font, axes: { ...instance.coordinates } },
    'Variable named instance updated',
    session.state,
    preview,
  );
}
function openTypeControls(n) {
  const reason = textFeatureReason(n, fonts[n.font]),
    supported = !reason && standardLigatureSupport(fonts[n.font]),
    value = n.fontFeatures === undefined ? '' : n.fontFeatures.liga ? 'on' : 'off';
  return `<div class="field"><label>Standard ligatures (liga)</label><select data-text-ligatures="${n.id}" aria-label="Standard ligatures (liga)" ${current().locked || reason ? 'disabled' : ''}><option value="" ${value === '' ? 'selected' : ''}>Legacy layout</option><option value="off" ${value === 'off' ? 'selected' : ''}>Off</option><option value="on" ${value === 'on' ? 'selected' : ''} ${supported ? '' : 'disabled'}>On</option></select></div><p class="font-note" data-text-feature-status>${esc(reason || (supported ? 'Latin standard ligatures only. Joined glyphs use Graph Text editing; character and Canvas caret editing require ligatures Off. Other OpenType tags are unsupported.' : 'This font has no supported Latin liga lookup. Other OpenType tags are unsupported.'))}</p>`;
}
function variableControls(n) {
  const metadata = fonts[n.font]?.nectVariable,
    axes = metadata?.axes,
    selectedInstance = metadata ? matchingNamedInstance(metadata, n.variation?.axes) : -1,
    instances = metadata?.namedInstances || [];
  const presets = instances.length
    ? `<div class="field"><label>Named instance</label><select data-variable-instance="${n.id}" aria-label="Variable named instance"><option value="" ${selectedInstance < 0 ? 'selected' : ''} disabled>Custom axes</option>${instances.map((instance, index) => `<option value="${index}" ${selectedInstance === index ? 'selected' : ''}>${esc(instance.name)}</option>`).join('')}</select></div>`
    : '';
  return `<button class="mini" data-load-variable-font="${n.id}">Load variable TTF (experimental)…</button>${
    axes
      ? `<p class="font-note">Experimental variable font: whole, single-line horizontal Latin text only. No wrap, path, vertical, character overrides or maintained arrangement. Axis changes are prepared before commit; Escape cancels pending work.</p>${presets}${Object.entries(
          axes,
        )
          .map(
            ([tag, range]) =>
              `<div class="field"><label>${esc(tag)} (${range.min}–${range.max}; default ${range.default})</label><input type="number" step="any" min="${range.min}" max="${range.max}" value="${n.variation?.axes[tag] ?? range.default}" data-variable-axis="${esc(tag)}" data-variable-node="${n.id}" aria-label="Variable axis ${esc(tag)}"></div>`,
          )
          .join('')}`
      : ''
  }`;
}
function desktopAxisControls(n) {
  const axes = n.osFont.axes || [];
  if (!axes.length)
    return n.osFont.version === 2
      ? `<button class="mini" data-load-os-axes="${n.id}">Check variable axes</button>`
      : '';
  const field = (a) =>
    `<label>${esc(a.name)} (${esc(a.tag)})<input type="number" step="any" min="${a.min}" max="${a.max}" value="${a.value}" data-os-axis="${esc(a.tag)}" data-os-node="${n.id}" aria-label="Windows Text axis ${esc(a.tag)}"></label>`;
  return `<fieldset><legend>Variable axes${axes.some((a) => a.value !== a.baseValue) ? ' · Custom' : ''}</legend>${axes
    .filter((a) => !a.hidden)
    .map(field)
    .join('')}${
    axes.some((a) => a.hidden)
      ? `<details><summary>More axes</summary>${axes
          .filter((a) => a.hidden)
          .map(field)
          .join('')}</details>`
      : ''
  }<button class="mini" data-reset-os-axes="${n.id}">Reset to font defaults</button></fieldset>`;
}
function desktopTypographyControls(n) {
  if (![2, 3, 4, 5].includes(n.osFont?.version)) return '';
  const disabled = !desktop || current().locked || n.locked || osTextDrafts.has(n.id);
  return `<fieldset ${disabled ? 'disabled' : ''}><legend>Typography</legend>${[
    ['size', 'Size', n.params.size.value],
    ['tracking', 'Tracking', n.params.letterSpacing.value],
    ['lineSpacing', 'Line spacing', n.params.lineSpacing.value],
  ]
    .map(
      ([key, label, value]) =>
        `<label>${label}<input type="number" step="any" value="${value}" data-os-setting="${key}" data-os-node="${n.id}" aria-label="Windows Text ${label}"></label>`,
    )
    .join(
      '',
    )}<label><input type="checkbox" data-os-setting="kerning" data-os-node="${n.id}" ${n.kerning !== false ? 'checked' : ''}>Kerning</label><label><input type="checkbox" data-os-setting="ligatures" data-os-node="${n.id}" ${n.osFont.outline.ligatures ? 'checked' : ''}>Standard ligatures</label><label>Alignment<select data-os-setting="alignment" data-os-node="${n.id}">${['start', 'center', 'end'].map((a) => `<option ${n.alignment === a ? 'selected' : ''}>${a}</option>`).join('')}</select></label><label>Writing mode<select data-os-setting="writingMode" data-os-node="${n.id}">${[
    ['horizontal-tb', 'Horizontal'],
    ['vertical-rl', 'Vertical'],
  ]
    .map(
      ([value, label]) =>
        `<option value="${value}" ${n.writingMode === value ? 'selected' : ''}>${label}</option>`,
    )
    .join(
      '',
    )}</select></label>${desktopAxisControls(n)}<label>Line composition<select data-os-wrap="${n.id}" aria-label="Windows Text wrap mode">${wrapModeOptions(n)}</select></label>${n.textWrap ? `<label>Wrap ${n.writingMode === 'vertical-rl' ? 'height' : 'width'} px<input type="number" step="any" min="0.01" max="100000" data-os-wrap-width="${n.id}" aria-label="Windows Text wrap ${n.writingMode === 'vertical-rl' ? 'height' : 'width'}" value="${wrapExtent(n.textWrap)}"></label>` : ''}<p class="font-note">Long unbreakable groups may overflow.</p>${n.osFont.outline.wrapOverflow ? '<p role="status">Wrap overflow · all artwork retained</p>' : ''}</fieldset>`;
}
function textControls(n) {
  if (n.type !== 'Text') return '';
  if (n.osFont) {
    const draft = osTextDrafts.get(n.id),
      availability = osFontAvailability(n.osFont, {
        ...osFontCatalog,
        desktop,
        pending: osFontCatalogPending,
      });
    return `<div class="text-controls"><div class="field"><label>OS font</label><span>${esc(n.osFont.family)} · ${n.osFont.version >= 2 ? esc(n.osFont.face.name) + ' · ' + n.osFont.face.weight : ['Regular', 'Bold', 'Italic', 'Bold Italic'][n.osFont.style]}</span></div><button class="mini" data-os-text-font="${n.id}">Choose OS font…</button><button class="mini" data-refresh-os-fonts>Refresh fonts</button><output role="status">${esc(availability)}</output><label>Text<textarea rows="3" data-os-text="${n.id}" aria-label="OS font text" ${current().locked || n.locked ? 'disabled' : ''}>${esc(draft?.value ?? n.text)}</textarea></label><button class="mini" data-apply-os-text="${n.id}" ${draft?.pending ? 'disabled' : ''}>Apply text</button><button class="mini" data-cancel-os-text="${n.id}">Cancel text edit</button><button class="mini" data-bake-text-outline="${n.id}">Convert to paths</button>${desktopTypographyControls(n)}<button class="mini" data-clear-os-text="${n.id}">Use DejaVu Sans</button></div>`;
  }
  const osControls = desktop
    ? `<section class="os-font-outlines"><button class="mini" data-os-text-font="${n.id}" ${current().locked ? 'disabled' : ''}>Choose OS font…</button><button class="mini" data-os-text-outline="${n.id}" ${current().locked ? 'disabled' : ''}>OS font / Adobe Fonts → paths…</button><p class="font-note">Replaces Text with paths. Undo restores Text.</p></section>`
    : '';
  return (
    osControls +
    `<div class="text-controls"><button class="mini" data-bake-text-outline="${n.id}" ${current().locked ? 'disabled' : ''}>Convert text to editable paths</button><p class="font-note">Freezes the current outlines into separate path units. Text and font editing are removed; Undo restores the Text node.</p><div class="field"><label>Text</label><textarea rows="3" data-text="${n.id}" aria-label="Text content">${esc(n.text)}</textarea></div><button class="mini" data-style-text-range="${n.id}" ${current().locked ? 'disabled' : ''}>Style selected text</button><button class="mini" data-kern-text-cursor="${n.id}" ${maintainedAxis(n) !== null || current().locked ? 'disabled' : ''}>Kerning at cursor</button><p class="font-note">Highlight text above, then Style selected text. Or use Character position to select on the canvas. Font and size below are defaults; selected-character overrides stay in place.</p><div class="field"><label>Default font</label><select data-font="${n.id}" aria-label="Text font" ${fontRequest?.node === n.id ? 'disabled' : ''}><option value="default" ${n.font === 'default' ? 'selected' : ''}>DejaVu Sans (Latin)</option>${Object.entries(
      history.doc.fontAssets || {},
    )
      .map(
        ([id, asset]) =>
          `<option value="${id}" ${n.font === id ? 'selected' : ''}>${esc(asset.name)}</option>`,
      )
      .join(
        '',
      )}${desktop && systemFonts.length ? `<optgroup label="Installed Windows fonts">${systemFonts.map(([id, name, reason]) => `<option value="${esc(id)}" ${reason ? 'disabled' : ''}>${esc(name)}${reason ? ` — ${esc(reason)}` : ''}</option>`).join('')}</optgroup>` : ''}</select></div>${desktop ? `<p class="font-note" role="status">${fontRequest?.node === n.id ? 'Loading font…' : esc(systemFontStatus)}</p>` : ''}<button class="mini" data-load-font="${n.id}">Load TTF / OTF…</button>${variableControls(n)}${openTypeControls(n)}<p class="font-note">Font bytes stay local and are included in saved JSON. Use a font licensed for editable embedding; check its license before sharing. File flags do not guarantee license permission.</p>${arrangementControls(n)}${textPathControls(n)}${wrapControls(n)}${leadingControls(n)}<div class="field"><label>Writing</label><select data-writing-mode="${n.id}" aria-label="Writing direction"><option value="horizontal-tb" ${n.writingMode !== 'vertical-rl' ? 'selected' : ''}>Horizontal →</option><option value="vertical-rl" ${n.writingMode === 'vertical-rl' ? 'selected' : ''}>Vertical ↓ / columns ←</option></select></div><div class="field"><label>Position reference ${n.writingMode === 'vertical-rl' ? 'Y' : 'X'}</label><select data-text-alignment="${n.id}" aria-label="Text alignment">${['start', 'center', 'end'].map((v, i) => `<option value="${v}" ${(n.alignment || 'start') === v ? 'selected' : ''}>${(n.writingMode === 'vertical-rl' ? ['Top', 'Center', 'Bottom'] : ['Left', 'Center', 'Right'])[i]}</option>`).join('')}</select></div><div class="field"><label>Reference bounds</label><select data-reference-bounds="${n.id}" aria-label="Text reference bounds">${['visible', 'advance'].map((v) => `<option value="${v}" ${(n.referenceBounds || 'advance') === v ? 'selected' : ''}>${v === 'visible' ? 'Visible outline' : 'Advance (legacy)'}</option>`).join('')}</select></div><div class="field"><label>Position reference ${n.writingMode === 'vertical-rl' ? 'X' : 'Y'}</label><select data-cross-alignment="${n.id}" aria-label="Text cross-axis reference">${['origin', 'start', 'center', 'end'].map((v, i) => `<option value="${v}" ${(n.crossAlignment || 'origin') === v ? 'selected' : ''}>${(n.writingMode === 'vertical-rl' ? ['First column center', 'Left', 'Center', 'Right'] : ['First baseline', 'Top', 'Center', 'Bottom'])[i]}</option>`).join('')}</select></div><div class="field"><label>Automatic font kerning (horizontal)</label><input type="checkbox" data-text-kerning="${n.id}" aria-label="Text kerning" ${maintainedAxis(n) !== null ? 'disabled' : ''} ${n.kerning !== false ? 'checked' : ''}></div><p class="font-note">Position reference holds each line/column’s chosen visible edge, or legacy advance including spaces. Cross-axis edges use the whole visible block. letterSpacing: inherited tracking in px. lineSpacing: auto-leading factor; ignored by manual leading. Enter starts a hard line or column. Wrap width adds derived horizontal lines without changing the source text. Changing a reference keeps Position fixed and moves the outline; font and size changes keep the chosen reference fixed.</p><p class="font-note">Position OUT is the evaluated Vector2 at this reference, before downstream Transform. Connect it to another Position, or Separate X/Y and Combine to keep one axis independent. Add Vector Math for an offset. Move or snap the source Text; connected followers do not edit it.</p></div>`
  );
}
function unitPointIds(n, unit) {
  return n.contours.filter((c) => unit.contours.includes(c.id)).flatMap((c) => c.points);
}
function multiPathControls(n) {
  if (n.type !== 'Multi Path') return '';
  return `<section aria-label="Contained path units"><h4>Path units</h4><p class="font-note">Select a unit, then drag its move marker or a selected anchor to move its outline and holes together. Shift selects additional units. Arrow buttons move one unit by 1 px. Edit individual anchors normally; Alt-click creates Bézier handles and Alt-drag pulls them out. Use Subdivide Path to add points.</p>${n.units
    .map(
      (unit, i) =>
        `<div><button class="mini" data-path-unit="${unit.id}" data-node="${n.id}" aria-pressed="${unitPointIds(n, unit).every((id) => selectedPointIds(n).includes(id))}">Path ${i + 1}</button>${[
          ['←', -1, 0],
          ['→', 1, 0],
          ['↑', 0, -1],
          ['↓', 0, 1],
        ]
          .map(
            ([label, x, y]) =>
              `<button class="mini" data-move-path-unit="${unit.id}" data-node="${n.id}" data-delta="${x},${y}" aria-label="Move Path ${i + 1} ${label} 1 px" ${current().locked ? 'disabled' : ''}>${label}</button>`,
          )
          .join('')}</div>`,
    )
    .join('')}</section>`;
}
function positionReferenceControls(n) {
  if (!['Circle', 'Ellipse', 'Rectangle', 'Polygon', 'Star', 'Image'].includes(n.type)) return '';
  const options = [
    ['origin', n.type === 'Image' ? 'Origin (top left)' : 'Origin (construction center)'],
  ];
  for (const y of ['top', 'middle', 'bottom'])
    for (const x of ['left', 'center', 'right']) options.push([`${x}-${y}`, `${y} / ${x}`]);
  return `<div class="field"><label>Position reference</label><select data-position-reference="${n.id}" aria-label="Object Position reference">${options.map(([value, label]) => `<option value="${value}" ${(n.positionReference || 'origin') === value ? 'selected' : ''}>${label}</option>`).join('')}</select></div><p class="font-note">Bounds exclude stroke. Position stays fixed when the reference or size changes.${n.type === 'Image' ? ' Image bounds include transparent pixels.' : ''}</p>`;
}
function referenceControls(n, result, compact = false) {
  if (n.type === 'Composition Reference') {
    const targetId = n.compositionReference?.compositionId || '',
      source = history.doc.layers.find((l) => l.id === targetId);
    return `<div class="field"><label>Source Composition</label><select data-composition-target="${n.id}" aria-label="Source Composition" ${current().locked ? 'disabled' : ''}><option value="">Choose Composition…</option>${targetId && !source ? `<option value="${esc(targetId)}" selected disabled>Missing: ${esc(targetId)}</option>` : ''}${history.doc.layers
      .filter((l) => l.id !== current().id)
      .map(
        (l) =>
          `<option value="${esc(l.id)}" ${l.id === targetId ? 'selected' : ''}>${esc(l.name)}</option>`,
      )
      .join(
        '',
      )}</select></div>${source ? `<button class="mini" data-composition-jump="${esc(source.id)}">Open source</button>` : ''}`;
  }
  if (n.type !== 'Reference') return '';
  const targets = vectorReferenceTargets(current(), n.id),
    value = n.reference ? JSON.stringify([n.reference.nodeId, n.reference.portId]) : '',
    target = targets.find(
      (t) => t.nodeId === n.reference?.nodeId && t.portId === n.reference?.portId,
    );
  let diagnostic = '',
    displayed = '';
  try {
    referenceTarget(current(), n);
    displayed = result
      .run(n.id)
      .map((v) => Number(v.toFixed(3)))
      .join(', ');
  } catch (e) {
    diagnostic = e.message;
  }
  const label = target
    ? `${target.name} · ${target.label}`
    : n.reference
      ? `${n.reference.nodeId} · ${n.reference.portId}`
      : 'No target selected';
  const status = `<p class="font-note reference-source ${diagnostic ? 'reference-error' : ''}" data-reference-status="${n.id}" title="${esc(n.reference ? n.reference.nodeId + ' / ' + n.reference.portId : '')}">← ${esc(label)}<br>${diagnostic ? esc(diagnostic) : `<output data-reference-value="${n.id}">[${esc(displayed)}] · Vector2</output>`}</p>`;
  const jump = target
    ? `<button class="mini" data-reference-jump="${n.id}" title="Select and reveal the source node">Jump to source</button>`
    : '';
  if (compact) return status + jump;
  return `<div class="field"><label>Vector2 source · same layer</label><button class="mini" data-search-source ${current().locked ? 'disabled' : ''} aria-label="Search reference nodes">Search…</button><select data-reference-target="${n.id}" aria-label="Reference source and output" ${current().locked ? 'disabled' : ''}><option value="" ${!n.reference ? 'selected' : ''}>Choose Vector2 output…</option>${n.reference && !target ? `<option value="${esc(value)}" selected disabled>Missing: ${esc(label)}</option>` : ''}${targets.map((t) => `<option value="${esc(JSON.stringify([t.nodeId, t.portId]))}" ${t === target ? 'selected' : ''}>${esc(t.name + ' · ' + t.label + ' · ' + t.nodeId)}</option>`).join('')}</select></div>${status}${jump}<p class="font-note">Read-only value reference by stable node and output IDs. Rename keeps the link; deleting the source leaves an explicit missing target. Text Position is before downstream Transform. Vector values keep their source meaning; no world-coordinate conversion.</p>`;
}
function smoothControls(n) {
  return n.type === 'Smooth Path'
    ? '<p class="font-note">Move points toward their connected neighbors without adding points. Factor 0–1; Iterations 1–50. Keep Endpoints pins open-path ends. Each iteration uses the previous positions simultaneously. Relative Bézier handles stay unchanged; this can shrink or collapse shapes and is not curve fitting. Use Resample Path first for a finer polyline.</p>'
    : '';
}
function instancePointsControls(n) {
  return n.type === 'Instance on Points'
    ? '<p class="font-note">Place the source path bounds center on every authored anchor in Points. Luminance to Points supplies per-point size and rotation; these override Align Tangent. Connect any custom Curve, Star, Polygon or other Path as Source. Align Tangent turns the source so its local +X follows the target tangent: outgoing handle B, reverse incoming handle A, then neighboring anchor chords. Curves are not arc-length-sampled; use Resample Path first when even spacing or denser target anchors are wanted. Target groups and curve interiors are ignored. Copies are materialized geometry with bounded output.</p>'
    : '';
}
function luminanceControls(n) {
  if (n.type === 'Luminance to Points')
    return '<p class="font-note">Sample Source fill or an embedded PNG into disconnected points with per-point scale and rotation. Connect this Path to Instance on Points: Points, and connect any Curve, Star, Polygon or other Path to its Source. Add Fill afterward. Spacing sets grid-center distance in px independently of motif size. Scale/angle interpolate linearly from white to black after linear-light luminance, alpha over white, Invert and Tone power. Zero scale omits a point. Centers are selected inside the source mask/rectangle, including holes; complete motifs are not clipped to cells or the source, and may overlap. Fill opacity is applied afterward; overlapping groups retain independent alpha. Up to 256 active points, 4096 sampled cells and existing geometry/work limits; reduce density or motif complexity if exceeded. Point attributes are derived, saved through node parameters, and consumed by Instance on Points; other Path operations may discard them. Bypass is unavailable because source paint and point output differ.</p>';
  if (n.type === 'Luminance Instances')
    return '<p class="font-note">Place a Path motif at grid centers inside the source mask or image rectangle. Remapped darkness linearly maps white/black scale and angle endpoints; Invert swaps darkness. Pivot is the whole motif bounds center. Zero scale omits a copy. Partial cells whose center is outside are omitted; whole motifs remain unclipped, preserving cubic handles and groups. Output is an unpainted Path: add Fill or Stroke. Bypass passes the motif unchanged. Up to 256 copies/groups and 100000 points; the shared 4096 sampled-cell cap remains. Filled vector sources must be closed and ungrouped; supported PNG images sample nearest texels in linear light over white. Tone power (0.1–10, default 1) raises darkness to that power after luminance/alpha and Invert: below 1 makes heavier marks, above 1 makes lighter marks. The source paint/image stays unchanged.</p>';

  if (n.type === 'Halftone')
    return '<p class="font-note">Dot shape: Legacy calibrated preserves existing cell-clipped circle-area screening and uses spacing for size. Choose Square, Circle, Polygon or Star for independent maximum size and rotation (polygon sides / star points 3–32; star inner radius ratio 0.05–0.95). Size is square side, circle diameter, or polygon/star outer diameter in px at full darkness; each dimension scales by square root of darkness. Dot spacing is center-to-center px: below maximum size allows overlap, clipped only to the source boundary. Overlaps make opaque black silhouettes with no alpha stacking or smoothing. Cells outside source bounds are not sampled. Limits: 4096 cells, 100000 generated vertices and existing mask work/output budgets; reduce density or shape detail if exceeded. Convert a solid or two-stop gradient fill, or an embedded Image, into black dots. Grid origin sets the top-left document-coordinate corner of cell (0, 0); move it to shift the texture phase without moving the source. Document Y increases downward. Vector luminance uses sRGB stop interpolation; image samples use nearest texels. Both convert to linear light and composite alpha over white. Partial image edge cells clamp to the nearest image texel, and generated marks are clipped to the placed image rectangle (transparent pixels do not define a contour). In Legacy calibrated mode, dot polygon area tracks darkness and each dot is clipped to its cell. Custom shapes are clipped only to the closed source mask. Curves are flattened at about 0.1 document unit. Image processing is limited to supported RGB/RGBA PNGs up to 4 megapixels and the 4096-cell grid cap. Output is not print-calibrated. Vector inputs must be ungrouped and closed; stroke-only inputs are unsupported. Tone power (0.1–10, default 1) raises darkness to that power after luminance/alpha and Invert: below 1 makes heavier marks, above 1 makes lighter marks. The source paint/image stays unchanged.</p>';
  if (n.type === 'Ordered Dither')
    return '<p class="font-note">Convert a solid or two-stop gradient fill, or an embedded Image, to black dots using a fixed Bayer 8×8 threshold pattern. Grid origin sets the top-left document-coordinate corner of cell (0, 0); move it to shift the texture phase without moving the source. Document Y increases downward. Vector gradients interpolate in sRGB; image samples use nearest texels. Both convert to linear light and composite alpha over white. Partial image edge cells clamp to the nearest image texel, and marks clip to the placed image rectangle (transparent pixels do not define a contour). Image processing is limited to supported RGB/RGBA PNGs up to 4 megapixels and the 4096-cell grid cap. Dots are clipped to the closed source mask.  Dot shape selects Square, Circle, Polygon or Star (3–32 sides / points, rotation ±360°, star inner radius ratio 0.05–0.95). Dot size is the square side or circle diameter in document px, independent of Dot spacing (center-to-center px; smaller is denser). Size above spacing overlaps neighboring dots; only the source boundary clips dots. Changing spacing changes the sampling grid, without resizing dots. Circle polygons stay within 0.1 px of the circle. Polygon and Star size is the outer circumcircle diameter. Overlaps are opaque black silhouettes, without alpha stacking or smoothing. Limits: 4096 sampled cells, 100000 generated vertices and existing mask work/output budgets; reduce density or shape detail if exceeded. This is ordered thresholding, not error diffusion; output is not print-calibrated. Vector inputs must be ungrouped and closed; stroke-only inputs are unsupported. Tone power (0.1–10, default 1) raises darkness to that power after luminance/alpha and Invert: below 1 makes heavier marks, above 1 makes lighter marks. The source paint/image stays unchanged.</p>';
  if (n.type === 'Error Diffusion Dither')
    return '<p class="font-note">Floyd–Steinberg error diffusion of a solid or two-stop gradient fill, or an embedded Image, into black dots. Scan left-to-right, top-to-bottom; darkness ≥ 0.5 is black. Residual error goes right 7/16, down-left 3/16, down 5/16, down-right 1/16. Missing neighbors discard error without renormalizing. Dot shape selects Square or Circle. Dot size is the square side or circle diameter in document px, independent of Dot spacing (center-to-center px; smaller is denser). Size above spacing overlaps neighboring dots; only the source boundary clips dots. Changing spacing changes the sampling grid, without resizing dots. Circle polygons stay within 0.1 px of the circle. All bounds cells participate before clipping, including cells outside a vector mask or inside holes. Grid origin sets the top-left corner of cell (0, 0); document Y increases downward. sRGB samples convert to linear light and composite alpha over white. Images use nearest texels, edge clamping and rectangle clipping; supported RGB/RGBA PNGs up to 4 megapixels only. The 4096-cell and existing geometry/work caps apply. Vector inputs must be filled, ungrouped and closed. Output is not print-calibrated. Tone power (0.1–10, default 1) raises darkness to that power after luminance/alpha and Invert: below 1 makes heavier marks, above 1 makes lighter marks. The source paint/image stays unchanged.</p>';
  if (n.type === 'Line Screen')
    return '<p class="font-note">Map sampled darkness from a solid or two-stop gradient fill, or an embedded Image, to centered black stripes on the grid set by Grid origin, the top-left document-coordinate corner of cell (0, 0). Move it to shift texture phase without moving the source; document Y increases downward. Horizontal makes full-cell-width strips; Vertical swaps them. Each strip is a cell-sampled rectangle with thickness darkness × cell pitch, so neighboring cells form stepped variable-width lines. White is empty and black fills the cell. Image samples use nearest texels after sRGB-to-linear conversion and alpha compositing over white; partial image edge cells clamp to the closest texel, then marks clip to the placed rectangle. Only supported RGB/RGBA PNGs up to 4 megapixels are processed; the 4096-cell cap remains. Vector marks clip to the closed source mask. This is not continuous engraving or print-calibrated screening. Vector inputs must be ungrouped and closed; stroke-only inputs are unsupported. Tone power (0.1–10, default 1) raises darkness to that power after luminance/alpha and Invert: below 1 makes heavier marks, above 1 makes lighter marks. The source paint/image stays unchanged.</p>';
  return '';
}
function roughenControls(n) {
  return n.type === 'Roughen'
    ? '<p class="font-note">Static seeded point displacement. Amount bounds each X/Y displacement in document px; the same seed and source point IDs repeat the same pattern. Keep Endpoints pins open-path ends. Relative Bézier handles stay unchanged. Use Resample Path before Roughen for finer density; this does not add points or repair self-intersections.</p>'
    : '';
}
function resampleControls(n) {
  return n.type === 'Resample Path'
    ? '<p class="font-note">Approximate each contour with equally spaced arc-length samples joined by straight segments. Count is per contour (2–1024; closed paths need 3). Spacing uses a maximum arc-length gap (0.0001–100000 px): segments are rounded up to a whole number per contour, so spacing is approximate and each resulting gap is no larger than requested. Open endpoints stay fixed; closed paths do not duplicate the seam point. Curves become polylines; corners and small features can change. This is not topology-preserving simplification. Edit upstream geometry or either control at any time.</p>'
    : '';
}
function cornerRoundControls(n) {
  return n.type === 'Corner Round'
    ? '<p class="font-note">Round sharp corners before Fill / Stroke, including Text. Straight-only paths retain circular cubic approximations. Regular curved junctions use tangent-continuous (G1) cubic blends: Radius is a nominal strength in document px, not a true circular fillet radius. Retained curves are exact subcurves; smooth font joins and open endpoints stay fixed. Shared short segments limit trims. Cusps, ambiguous collisions and precision-budget cases stay unchanged with warnings. Zero radius preserves the input. Conservative local support only; no Boolean cleanup, self-intersection repair or full font/shaping guarantee.</p>'
    : '';
}
function dashControls(n) {
  return n.type === 'Dash Path'
    ? '<p class="font-note">Generate actual dash geometry by arc length before Stroke. Positive offset advances the pattern; negative values wrap by the period. Gap zero keeps the full source. Cubic segments are retained; seam-crossing closed-path dashes are joined. Dash length must be positive: for visual dots use the Stroke Dotted preset. Expansion of curved dashes needs Resample Path before Stroke.</p>'
    : '';
}
function autoHandlesControls(n) {
  return n.type === 'Auto Handles'
    ? '<p class="font-note">Preserve anchors and generate symmetric tangent handles, bounded by one third of the shorter adjacent chord times Strength. Open endpoints use one-sided handles. Strength 0 makes straight edges; bypass restores original handles. Acute turns may still overshoot or self-intersect. Edit upstream Text for direct character editing.</p>'
    : '';
}
function taperControls(n) {
  return n.type === 'Taper'
    ? '<p class="font-note">Scale across the chosen axis using whole-input anchor and control-point bounds. Positive Amount grows the perpendicular dimension toward the maximum-axis end; negative reverses it. Amount is limited to ±50%, keeping control-point scale factors 0.5–1.5. This maps cubic controls, not exact curve interiors; Resample Path first for finer deformation. Edit upstream Text for direct character editing.</p>'
    : '';
}
function bendControls(n) {
  return n.type === 'Bend Points'
    ? '<p class="font-note">Bend the whole path into a circular strip around the shared control-bounds midpoint. Angle is −180° to 180°; positive bows toward the positive cross-axis direction and negative bows oppositely. Anchors and absolute Bézier controls are mapped separately, so curve interiors are approximate; Resample Path first for finer deformation. Folds are rejected. Edit upstream Text for direct character editing.</p>'
    : '';
}
function twistControls(n) {
  return n.type === 'Twist Points'
    ? '<p class="font-note">Twist around the midpoint of shared anchor and absolute-control bounds. Angle is −360° to 360°; positive rotates clockwise in screen coordinates. Influence eases from full at center to zero at the bounds radius, so corners at R stay unchanged. Anchors and absolute Bézier controls are mapped separately, so curve interiors are approximate; Resample Path first for finer detail. Edit upstream Text for direct character editing.</p>'
    : '';
}
function waveControls(n) {
  return n.type === 'Wave Points'
    ? '<p class="font-note">Static sine displacement of existing anchors: vertical shifts Y from X; horizontal shifts X from Y. Wavelength and amplitude use document px. Resample Path first for finer wave detail; no points are added and relative Bézier handles stay unchanged. This is not an exact continuous warp of curve interiors. Put Transform after Wave to move the result without changing its phase.</p>'
    : '';
}
function puckerBloatControls(n) {
  return n.type === 'Pucker & Bloat'
    ? '<p class="font-note">Positive Amount bloats: anchors move inward while absolute Bézier controls move outward. Negative puckers in the opposite direction. Automatic Center uses all input anchor bounds, not exact curve extrema or separate group centers. Turn it off to set Center manually. Zero preserves geometry. Large values can fold or collapse shapes; this is not a promise of Illustrator numeric parity.</p>'
    : '';
}
function simplifyControls(n) {
  return n.type === 'Simplify Path'
    ? '<p class="font-note">Reduce redundant straight-edge points with a document-pixel tolerance. Open endpoints stay fixed; closed rings retain at least three vertices and their winding, falling back to the original ring if needed. Curves require Resample Path first. Zero tolerance preserves input. Small features and intersections may change; this is approximation, not topology repair.</p>'
    : '';
}
function expandStrokeControls(n) {
  return n.type === 'Expand Stroke'
    ? '<p class="font-note">Convert an upstream solid Stroke into editable outline geometry for Boolean operations. Width / cap / join come from that Stroke node; Start / End width multipliers (0–4) linearly scale width by normalized centerline arc length on open contours. Values of 1 keep constant width; unequal values reject closed contours. Fill and paint color are not carried into the result. Add Fill after expansion. Cubic centerlines are flattened to polylines at 0.1 px tolerance when the bounds permit; endpoint caps follow the first / last tangent as represented by that flattening. Limits: 32 contours, 256 authored points, 256 flattened vertices, 4096 subdivision nodes and depth 24. This bounds centerline flattening, not total outline error. Nonzero dash patterns are unsupported. This type conversion cannot be bypassed as a same-type effect.</p>'
    : '';
}
function convertToPathControls(n) {
  return n.type === 'Convert to Path'
    ? '<p class="font-note">Accepts Path or painted vector geometry, including Halftone, Ordered Dither and Line Screen output. Keeps contours, groups, handles and editing provenance while dropping fill and stroke. Direct Image input is not supported. PaintedPath conversion changes the output type, so bypass is unavailable for that input.</p>'
    : '';
}
function trimControls(n) {
  return n.type === 'Trim Path'
    ? '<p class="font-note">Trim each contour by arc-length fractions: Start and End range from 0 (contour start) to 1 (contour end). Cubic Bézier segments are retained exactly between the selected positions. A full 0–1 range preserves closed contours; partial selections are open and follow the contour seam without wrapping. Start must not exceed End. Edit the upstream source Text for direct character editing.</p>'
    : '';
}
function setPathStartControls(n) {
  return n.type === 'Set Path Start'
    ? '<p class="font-note">Move the seam of each closed contour by an arc-length fraction from 0 to 1 while preserving forward winding and the exact Bézier shape. Open contours pass through unchanged; 0 and 1 keep the original seam. Interior positions split one curve without duplicating a vertex. Edit upstream Text for direct character editing.</p>'
    : '';
}
function setClosureControls(n) {
  return n.type === 'Set Closure'
    ? '<p class="font-note">Sets actual closure on every contour. Fill can close an open path visually without changing its geometry. No endpoints are welded.</p>'
    : '';
}
function reversePathControls(n) {
  return n.type === 'Reverse Path'
    ? '<p class="font-note">Reverse contour direction without changing the exact Bézier shape: anchors reverse, handles swap, and each closed contour keeps its first anchor seam. This changes start/end-sensitive results such as Trim Path and Dash Path. Edit upstream Text for direct character editing.</p>'
    : '';
}
function subdivideControls(n) {
  return n.type === 'Subdivide Path'
    ? '<p class="font-note">Split every existing edge into this many equal-parameter Bézier portions (1–32); this is not equal arc-length Resample. Exact cubic shape and endpoint tangents are retained. Open endpoints remain fixed; closed seams are not duplicated. Nect counts segments per edge, whereas Blender’s Subdivide Curve exposes Cuts. Straight edges with zero handles stay polyline-friendly. Edit upstream Text for direct character editing.</p>'
    : '';
}
function mirrorControls(n) {
  return n.type === 'Mirror'
    ? '<p class="font-note">Mirror geometry before Fill / Stroke. Left-right reflects across the vertical line through center; up-down across the horizontal line. Angle is the axis direction, clockwise from horizontal (degrees). Enable keepOriginal for a live symmetric copy. Edit the upstream source to update both sides. Overlaps remain separate; use Boolean Union when a merged outline is needed.</p>'
    : '';
}
function offsetControls(n) {
  return n.type === 'Closed Path Offset'
    ? '<p class="font-note">Closed nonzero regions. Explicit contour groups offset independently and overlapping groups remain separate; ungrouped geometry, including Compound Text, stays one global region collection. Positive expands; negative insets (empty erosion is valid). Distance is artboard px after upstream Transform. Place before Fill / Stroke. Curves become polygons (0.1 px flatten/arc tolerances, 0.0001 px grid); these are component tolerances, not a final offset error bound. Shared interactive caps across all groups: 32 contours / 256 authored points, 256 flattened input / 512 output vertices, 4096 subdivision nodes, 300000 topology checks and 4096 estimated join vertices; 8 nonzero Offset nodes. Exhausted budgets reject remaining groups. Unsupported or over-budget edits roll back.</p>'
    : '';
}
function gradientControls(n) {
  if (n.type === 'Radial Gradient Fill')
    return '<p class="font-note">Two colors in absolute artboard coordinates (px). Concentric circular ramp with radius from 0.0001 to 100000 px; center coordinates ±100000 px. No focal offsets or elliptical transform. Geometry / Transform / Repeater go before paint; repeated copies share one ramp. Each stop keeps its alpha. Save JSON to preserve the full procedural graph. Import SVG reopens our exported two-stop gradients as editable paths and paint nodes.</p>';
  return n.type === 'Linear Gradient Fill'
    ? '<p class="font-note">Two colors in absolute artboard coordinates (px). Start and end must differ. Geometry / Transform / Repeater go before paint; repeated copies share one ramp. Each stop keeps its alpha. Save JSON to preserve the full procedural graph. Import SVG reopens our exported two-stop gradients as editable paths and paint nodes.</p>'
    : '';
}
function strokeControls(n) {
  if (n.type !== 'Stroke') return '';
  const dash = n.strokeDash || [];
  const preset = !dash.some((v) => v !== 0)
    ? 'solid'
    : dash.length === 2 && dash[0] === 12 && dash[1] === 6 && (n.strokeCap || 'butt') === 'butt'
      ? 'dashed'
      : dash.length === 2 && dash[0] === 0 && dash[1] === 8 && n.strokeCap === 'round'
        ? 'dotted'
        : 'custom';
  return `<div class="field"><label>Stroke style</label><select data-stroke-preset="${n.id}" aria-label="Stroke style preset">${[
    ['solid', 'Solid'],
    ['dashed', 'Dashed'],
    ['dotted', 'Dotted'],
    ['custom', 'Custom'],
  ]
    .map(
      ([value, label]) =>
        `<option value="${value}" ${preset === value ? 'selected' : ''} ${value === 'custom' ? 'disabled' : ''}>${label}</option>`,
    )
    .join(
      '',
    )}</select></div><div class="field"><label>Line cap</label><select data-stroke-cap="${n.id}" aria-label="Stroke line cap">${['butt', 'round', 'square'].map((v) => `<option value="${v}" ${(n.strokeCap || 'butt') === v ? 'selected' : ''}>${v}</option>`).join('')}</select></div><div class="field"><label>Line join</label><select data-stroke-join="${n.id}" aria-label="Stroke line join">${['miter', 'round', 'bevel'].map((v) => `<option value="${v}" ${(n.strokeJoin || 'miter') === v ? 'selected' : ''}>${v}</option>`).join('')}</select></div><div class="field"><label>Dash / gap (px)</label><input data-stroke-dash="${n.id}" aria-label="Stroke dash pattern" value="${esc((n.strokeDash || []).join(' '))}" placeholder="Solid, or e.g. 12 6"></div><p class="font-note">Odd patterns repeat twice. Empty/none or all zero is solid. Miter limit is a ratio; dashOffset is px.</p>`;
}
function operationControls(n) {
  const options = operationOptions[n.type];
  if (!options) return '';
  return `<div class="field"><label>${hasDotControls(n.type) ? 'Dot shape' : n.type === 'Closed Path Offset' ? 'Join' : 'Operation'}</label><select data-operation="${n.id}" aria-label="${esc(nodeDisplayName(n))} ${hasDotControls(n.type) ? 'dot shape' : 'operation'}">${options.map((op) => `<option value="${op}" ${n.operation === op ? 'selected' : ''}>${n.type === 'Halftone' && op === 'subtract' ? 'Legacy calibrated' : op[0].toUpperCase() + op.slice(1)}</option>`).join('')}</select></div>`;
}
function expandedNodeBody(n, result) {
  return `<div class="node-body">${operationControls(n)}${strokeControls(n)}${gradientControls(n)}${offsetControls(n)}${mirrorControls(n)}${cornerRoundControls(n)}${trimControls(n)}${setPathStartControls(n)}${setClosureControls(n)}${reversePathControls(n)}${subdivideControls(n)}${expandStrokeControls(n)}${convertToPathControls(n)}${simplifyControls(n)}${puckerBloatControls(n)}${waveControls(n)}${taperControls(n)}${autoHandlesControls(n)}${bendControls(n)}${twistControls(n)}${dashControls(n)}${resampleControls(n)}${roughenControls(n)}${smoothControls(n)}${radialControls(n)}${positionReferenceControls(n)}${transformControls(n)}${positionControllerControls(n)}${referenceControls(n, result)}${booleanInputNote(n)}${instancePointsControls(n)}${luminanceControls(n)}${n.type === 'Join Geometry' ? `${n.joinInputOrder.length >= 1024 ? '<p class="font-note">Input socket limit reached (1,024). Disconnect an unlocked additional input to free a socket.</p>' : ''}<p class="font-note">Connect paths; the last input adds a spare automatically (up to 1,024 sockets; 256 geometry groups). Bundle inputs without cutting or welding. Shared transforms and paint apply to all; overlapping transparent groups remain independently painted. Use Boolean Union to merge silhouettes.</p>` : ''}${repeaterControls(n)}${textControls(n)}${multiPathControls(n)}${editorParameters(
    n,
  )
    .filter(
      ([k]) =>
        isTransform(n) ||
        Object.hasOwn(n.params, k) ||
        (n.type === 'Boolean' && k === booleanPendingInputKey),
    )
    .map(([k, q]) => field(n, k, q))
    .join(
      '',
    )}${ownedPointFields(n)}${['Curve', 'Path Remap'].includes(n.type) ? '<p class="font-note">Alt-click an anchor to create Bézier handles; Alt-drag pulls out symmetric handles. Shift-click selects multiple anchors.</p>' : ''}${n.type === 'Curve' ? `<button class="mini" data-add-point="${n.id}">+ Point</button>` : ''}${n.type === 'Path Remap' ? remapFields(n, result) : ''}<div class="node-actions">${['Path', 'PaintedPath', 'Image'].includes(outputType(n)) && !['Output', 'Image Output'].includes(n.type) ? `<button class="mini" data-preview="${n.id}">Use as output</button>` : ''}<button class="mini" data-duplicate="${n.id}">Duplicate</button>${!['Output', 'Image Output'].includes(n.type) ? `<button class="mini danger" data-delete="${n.id}">Delete</button>` : ''}</div></div>`;
}
function nodeHTML(n, result) {
  const body = n.collapsed
    ? `<div class="node-body compact-body">${compactFields(n)}${referenceControls(n, result, true)}</div><div hidden><button data-duplicate="${n.id}">Duplicate</button>${!['Output', 'Image Output'].includes(n.type) ? `<button data-delete="${n.id}">Delete</button>` : ''}</div>`
    : expandedNodeBody(n, result);
  return `<section class="node ${n.bypassed ? 'bypassed' : ''} ${selection.has(n.id) ? 'selected' : ''} ${n.collapsed ? 'collapsed' : ''}" data-id="${n.id}" style="left:${n.position[0]}px;top:${n.position[1]}px;width:${n.width || 264}px">${nodeHeader(n)}${body}<div class="node-resize" title="Drag to resize node" aria-hidden="true"></div></section>`;
}
function updateArtboardUnitDisplay() {
  for (const input of app.querySelectorAll('[data-dimension]')) {
    const dimension = input.dataset.dimension,
      shortName = dimension === 'width' ? 'W' : 'H';
    const displayValue = formatDocumentLength(history.doc[dimension], artboardUnit);
    input.dataset.dimensionUnit = artboardUnit;
    input.defaultValue = displayValue;
    input.value = displayValue;
    input.setAttribute('aria-label', `Artboard ${dimension} (${artboardUnit})`);
    const label = input.closest('.field')?.querySelector('label');
    if (label) label.textContent = `Artboard ${shortName} (${artboardUnit})`;
  }
  const size = app.querySelector('[data-artboard-size]');
  if (size) {
    size.textContent = `${formatDocumentLength(history.doc.width, artboardUnit)} × ${formatDocumentLength(
      history.doc.height,
      artboardUnit,
    )} ${artboardUnit}`;
  }
}
function pngExportSettings() {
  return pngExportMode === 'ppi'
    ? { mode: 'ppi', ppi: pngPpi }
    : { mode: 'scale', scale: pngScale };
}
function exportPresetPanelMarkup() {
  const options = (format, selected) =>
    '<option value=""' +
    (selected ? '' : ' selected') +
    '>Select ' +
    format.toUpperCase() +
    ' preset</option>' +
    exportPresets
      .filter((preset) => preset.format === format)
      .map(
        (preset) =>
          '<option value="' +
          esc(preset.name) +
          '"' +
          (preset.name === selected ? ' selected' : '') +
          '>' +
          esc(preset.name) +
          '</option>',
      )
      .join('');
  return (
    '<details id="export-presets"><summary>Export presets</summary><div class="export-preset-menu">' +
    '<div class="export-preset-row" role="group" aria-label="SVG export presets"><label for="svg-export-preset">SVG</label><select id="svg-export-preset" aria-label="SVG export preset">' +
    options('svg', selectedSvgExportPreset) +
    '</select><input id="svg-export-preset-name" type="text" maxlength="100" placeholder="SVG preset name" aria-label="SVG preset name" value="' +
    esc(svgExportPresetName) +
    '"><button type="button" data-action="save-svg-export-preset"></button></div>' +
    '<div class="export-preset-row" role="group" aria-label="PNG export presets"><label for="png-export-preset">PNG</label><select id="png-export-preset" aria-label="PNG export preset">' +
    options('png', selectedPngExportPreset) +
    '</select><input id="png-export-preset-name" type="text" maxlength="100" placeholder="PNG preset name" aria-label="PNG preset name" value="' +
    esc(pngExportPresetName) +
    '"><button type="button" data-action="save-png-export-preset"></button></div>' +
    (exportPresetLoadError
      ? '<output class="export-preset-error" role="alert">' +
        esc(exportPresetLoadError) +
        '</output>'
      : '') +
    '</div></details>'
  );
}
function updateExportPresetSaveLabel(format) {
  const name = app.querySelector('#' + format + '-export-preset-name')?.value.trim() || '',
    exists = exportPresets.some((preset) => preset.format === format && preset.name === name),
    button = app.querySelector('[data-action="save-' + format + '-export-preset"]');
  if (button)
    button.textContent = (exists ? 'Update ' : 'Save ') + format.toUpperCase() + ' preset';
}
function clearExportPresetSelection(format) {
  if (format === 'svg') {
    selectedSvgExportPreset = null;
    const select = app.querySelector('#svg-export-preset');
    if (select) select.value = '';
  } else {
    selectedPngExportPreset = null;
    const select = app.querySelector('#png-export-preset');
    if (select) select.value = '';
  }
}
function saveCurrentExportPreset(format) {
  const nameInput = app.querySelector('#' + format + '-export-preset-name'),
    settings =
      format === 'svg'
        ? { sizing: svgExportSizing, unit: svgExportUnit, precision: svgExportPrecision }
        : pngExportSettings(),
    result = saveExportPreset(exportPresetStorage, {
      format,
      name: nameInput?.value ?? '',
      settings,
    });
  exportPresets = result.presets;
  exportPresetLoadError = '';
  if (format === 'svg') {
    selectedSvgExportPreset = result.preset.name;
    svgExportPresetName = result.preset.name;
  } else {
    selectedPngExportPreset = result.preset.name;
    pngExportPresetName = result.preset.name;
  }
  render();
  notify(format.toUpperCase() + ' preset saved');
}
function applySavedExportPreset(format, name) {
  const preset = exportPresets.find((item) => item.format === format && item.name === name);
  if (!preset) throw Error('Saved export preset was not found');
  const settings = applyExportPreset(preset, format);
  if (format === 'svg') {
    svgExportSizing = settings.sizing;
    svgExportUnit = settings.unit;
    svgExportPrecision = settings.precision;
    selectedSvgExportPreset = preset.name;
    svgExportPresetName = preset.name;
  } else {
    pngExportMode = settings.mode;
    if (settings.mode === 'scale') pngScale = settings.scale;
    else pngPpi = settings.ppi;
    selectedPngExportPreset = preset.name;
    pngExportPresetName = preset.name;
  }
  render();
  notify(format.toUpperCase() + ' preset applied');
}
function pngExportPreview(doc) {
  try {
    const size = pngDimensions(doc, pngExportSettings());
    return { size, label: `${size.width} × ${size.height} px` };
  } catch (error) {
    return { size: null, label: error.message || 'Invalid PNG dimensions' };
  }
}
function updatePngExportPreview() {
  const output = app.querySelector('[data-png-dimensions]');
  if (!output) return;
  const preview = pngExportPreview(history.doc);
  output.textContent = preview.label;
  output.dataset.valid = String(!!preview.size);
  const button = app.querySelector('[data-action="export-png"]');
  if (button) button.disabled = !preview.size;
}
function render(resetView = false, artworkResolution = null, graphScrollOverride = null) {
  textTools?.cancel();
  const osOwners = new Set(
    history.doc.layers.flatMap((layer) => layer.nodes.filter((n) => n.osFont).map((n) => n.id)),
  );
  for (const id of osTextDrafts.keys()) if (!osOwners.has(id)) osTextDrafts.delete(id);
  const pointOwner = current().nodes.find((n) => n.id === pointSelection.nodeId);
  if (pointOwner && ['Curve', 'Multi Path'].includes(pointOwner.type)) {
    const ids = new Set(pointOwner.points.map((p) => p.id));
    pointSelection.ids = new Set([...pointSelection.ids].filter((id) => ids.has(id)));
    if (pointFocus?.nodeId === pointOwner.id && !ids.has(pointFocus.pointId)) pointFocus = null;
  }
  gridOpacityDraft = null;
  closeNodeContextMenu();
  if (
    variableAction &&
    (variableAction.owner !== session || variableAction.revision !== session.state.expectedRevision)
  )
    cancelVariableAction();
  if (composing || nameEdit?.composing) {
    needsRender = true;
    return;
  }
  needsRender = false;
  if (pendingRender !== null) {
    clearTimeout(pendingRender);
    pendingRender = null;
  }
  if (drag?.type === 'graph-overview') cancelInteraction();
  graphOverviewObserver?.disconnect();
  const focused = resetView ? null : captureFocus(app);
  const exportPresetsOpen = app.querySelector('#export-presets')?.open ?? false;
  const pointScroll =
    !resetView && document.activeElement?.closest('.point-panel')
      ? [...app.querySelectorAll('.point-panel-rows')].map((el) => el.scrollTop)
      : null;
  const canvasEditor =
    !resetView && canvasTextEdit ? app.querySelector('.canvas-text-editor') : null;
  const canvasEditorFocused = canvasEditor === document.activeElement;
  const inspectorScroll = resetView ? 0 : document.querySelector('.inspector')?.scrollTop || 0;
  const layersScroll = resetView ? 0 : document.querySelector('.layers')?.scrollTop || 0;
  const guideScroll = resetView
    ? [0, 0]
    : ['.guides-panel', '.guide-list'].map(
        (selector) => document.querySelector(selector)?.scrollTop || 0,
      );
  for (const el of document.querySelectorAll('.graph details[data-detail]'))
    detailsState.set(`${el.closest('.node').dataset.id}/${el.dataset.detail}`, el.open);
  if (artworkTarget && !history.doc.layers.some((l) => l.id === artworkTarget)) {
    artworkTarget = null;
    artworkSelection = new Set();
  }
  if (selected && !current().nodes.some((n) => n.id === selected)) selected = null;
  if (penId && !current().nodes.some((n) => n.id === penId && n.type === 'Curve')) penId = null;
  if (!selection.has(selected)) selection = new Set(selected ? [selected] : []);
  selection = new Set([...selection].filter((id) => current().nodes.some((n) => n.id === id)));
  let scroll = document.querySelector('.graph-scroll'),
    oldScroll =
      graphScrollOverride ||
      (scroll && !resetView ? [scroll.scrollLeft, scroll.scrollTop] : [0, 0]);
  const doc = history.doc,
    pngPreview = pngExportPreview(doc),
    aids = guideSettings(doc),
    l = current();
  activeLayer = l.id;
  const n = selectedNode();
  if (
    characterEdit &&
    (characterEdit.node !== n?.id || characterEdit.layer !== l.id || l.locked || !layerShown(l))
  )
    characterEdit = null;
  fonts = resolveFonts(doc, activeFonts());
  let results = canvasLayers().map((x) => [x, evaluate(x, fonts, history.doc.imageAssets)]),
    result = results.find(([x]) => x.id === l.id)[1];
  renderEvaluation = result;
  // One synchronous status resolution for both hint and Inspector; callers may
  // reuse a just-resolved unchanged document, never a persistent cached target.
  const move = artworkTarget ? artworkResolution || artworkMove() : null;
  const duplication =
    artworkSelection.size > 1
      ? resolveArtworkDuplicateSelection(doc, artworkSelection, { fonts })
      : artworkTarget
        ? resolveArtworkDuplicateTarget(doc, artworkTarget, { fonts }, move)
        : null;
  snapLayerBounds = collectSnapLayerBounds(
    doc,
    results,
    artworkTarget ? artworkSelection : l.id,
    pathBounds,
  );
  app.innerHTML = `<header><div class="brand"><b>Nect</b><small>SVG</small></div><button data-action="new">New document</button><button data-action="open">Open JSON</button>${
    session.projectSnapshot().compositions.length > 1
      ? `<label>Composition <select data-active-composition aria-label="Composition">${session
          .projectSnapshot()
          .compositions.map(
            (composition) =>
              `<option value="${esc(composition.id)}" ${composition.id === session.activeCompositionId ? 'selected' : ''}>${esc(composition.name)}</option>`,
          )
          .join('')}</select></label>`
      : ''
  }<button data-action="add-composition">Add composition</button><button data-action="add-svg-part" title="Add vector SVG to the current Composition">Add SVG node</button><button data-action="import-svg" title="Replace the document with imported SVG">Import SVG document</button><button data-action="import-image">Import image</button><button data-action="save" title="Ctrl+S">Save</button><button data-action="save-as" title="Ctrl+Shift+S">Save As…</button><span data-document-status class="muted"></span>${desktop ? '<button data-recovery-status data-recovery-retry></button><button data-recovery-open>Recovery copies</button>' : ''}<button data-action="export">Export SVG</button><select id="svg-export-sizing" data-svg-export-sizing aria-label="SVG output sizing"><option value="fixed" ${svgExportSizing === 'fixed' ? 'selected' : ''}>Fixed</option><option value="responsive" ${svgExportSizing === 'responsive' ? 'selected' : ''}>Responsive</option></select><select id="svg-export-unit" data-svg-export-unit aria-label="SVG export units" title="SVG width and height units" ${svgExportSizing === 'fixed' ? '' : 'hidden'}>${ARTBOARD_UNITS.map((unit) => `<option value="${unit}" ${unit === svgExportUnit ? 'selected' : ''}>${unit}</option>`).join('')}</select><select id="svg-export-precision" data-svg-export-precision aria-label="SVG geometry precision" title="Round geometry numbers in the SVG copy; Default keeps the current output"><option value="default" ${svgExportPrecision === 'default' ? 'selected' : ''}>Default</option>${SVG_EXPORT_PRECISIONS.map((digits) => `<option value="${digits}" ${String(digits) === svgExportPrecision ? 'selected' : ''}>${digits} decimals</option>`).join('')}</select><select id="png-export-mode" aria-label="PNG size mode" title="Choose scale or physical PPI"><option value="scale" ${pngExportMode === 'scale' ? 'selected' : ''}>Scale</option><option value="ppi" ${pngExportMode === 'ppi' ? 'selected' : ''}>Physical × PPI</option></select><select id="png-scale" aria-label="PNG export scale" ${pngExportMode === 'scale' ? '' : 'hidden'}>${[0.5, 1, 2, 4].map((v) => `<option value="${v}" ${v === pngScale ? 'selected' : ''}>${v}×</option>`).join('')}</select><label ${pngExportMode === 'ppi' ? '' : 'hidden'}>PPI<input id="png-ppi" type="number" step="any" value="${Number.isFinite(pngPpi) ? pngPpi : ''}" aria-label="PNG resolution (PPI)"></label><output data-png-dimensions data-valid="${!!pngPreview.size}" aria-label="PNG output dimensions" aria-live="polite">${esc(pngPreview.label)}</output><button data-action="export-png" ${pngPreview.size ? '' : 'disabled'} title="Transparent artboard · ${esc(pngPreview.label)}">Export PNG</button><span class="divider"></span><button data-action="undo" ${history.past.length || penDraft ? '' : 'disabled'} title="Ctrl+Z">↶</button><button data-action="redo" ${history.future.length && !penDraft ? '' : 'disabled'} title="Ctrl+Shift+Z">↷</button><span class="spacer"></span><span class="muted">${esc(doc.name)}</span><button data-action="help">?</button></header><div class="workspace"><nav class="tools" aria-label="Tools">${[
    ['direct', 'A', 'Direct selection (A)'],
    ['select', '↖', 'Select / move'],
    ['pen', '⌁', 'Pen / Curve'],
    ['Circle', '○', 'Add Circle'],
    ['Rectangle', '□', 'Add Rectangle'],
    ['Polygon', '⬡', 'Add Polygon'],
    ['Star', '☆', 'Add Star'],
    ['Text', 'T', 'Horizontal Text'],
  ]
    .map(
      ([id, icon, label]) =>
        `<button data-tool="${id}" title="${label}" aria-label="${label}" class="${tool === id ? 'active' : ''}">${icon}</button>`,
    )
    .join(
      '',
    )}</nav><main class="middle"><div class="canvas-wrap"><div class="canvas-toolbar" role="toolbar" aria-label="Artboard view"><button data-action="zoom-out" aria-label="Artboard zoom out" title="Zoom out (−)">−</button><output data-zoom="canvas" aria-label="Artboard zoom"></output><button data-action="zoom-in" aria-label="Artboard zoom in" title="Zoom in (+)">+</button><button data-action="fit" title="Fit artboard (Shift+1)">Fit artboard</button><button data-action="actual-size" title="Actual size: one document unit per CSS pixel (Ctrl/Cmd+1)">100%</button><button data-action="snap" class="${aids.snap ? 'active' : ''}" aria-pressed="${aids.snap}" title="Snap to enabled guides, artwork, artboard, grid and 45° directions; hold Alt to bypass">Snap</button><button data-action="align-panel" aria-pressed="${alignmentPreferences.open}" title="Show alignment panel">Align</button><button data-action="grid-panel" aria-expanded="${gridOpen}">Grid</button><button data-action="layout-panel" aria-expanded="${layoutOpen}">Layout Grid</button><button data-action="guides-panel" aria-expanded="${guidesOpen}">Guides</button><span data-artboard-size aria-label="Artboard size">${formatDocumentLength(doc.width, artboardUnit)} &#215; ${formatDocumentLength(doc.height, artboardUnit)} ${artboardUnit}</span>${penDraft ? '<button data-pen-finish>Finish path</button><button data-pen-cancel>Cancel path</button>' : ''}</div>${gridPanel()}${layoutOpen ? `<section class="guides-panel" aria-label="Layout Grid">${layoutPanel()}</section>` : ''}${guidesPanel()}<div class="canvas-stage" tabindex="0" aria-label="Artboard viewport" aria-describedby="canvas-navigation-hint" title="Wheel: pan · Alt or Ctrl/Cmd+wheel: zoom · Space or middle drag: pan"><svg class="artboard" viewBox="0 0 ${doc.width} ${doc.height}" aria-label="Artwork canvas">${gridOverlay()}${results
    .filter(([x, r]) => layerShown(x) && r.path && !r.error)
    .map(([x, r]) => layerShape(x, r.path))
    .join(
      '',
    )}${guidesOverlay()}${canvasOverlay(n, result)}</svg>${characterFloating()}</div><div class="hint"><span id="canvas-navigation-hint" class="visually-hidden">Space drag: pan · Ctrl/Cmd wheel: zoom</span><span>${tool === 'pen' ? 'Pen' : artworkTarget ? `Artwork · ${artworkSelection.size > 1 ? `${artworkSelection.size} selected` : esc(l.name)} · ${esc(move.supported ? 'Move artwork' : move.reason)}` : n ? `Node · ${esc(nodeDisplayName(n))}${n.type === 'Transform' ? ' · Selected Transform output' : ''}${selection.size > 1 ? ` + ${selection.size - 1} generators` : ``} · ${esc(l.name)}${l.locked ? ' · Locked' : ''}` : 'Select artwork'}</span></div></div><div class="splitter" role="separator" aria-label="Resize node graph" tabindex="0"></div><section class="graph-panel" style="height:${graphHeight}%"><div class="panel-head graph-toolbar" role="toolbar" aria-label="Node graph view"><strong>Node graph</strong><span class="muted graph-layer-name" title="${esc(l.name)}">${esc(l.name)}</span><span class="spacer"></span><select id="add-node" aria-label="Add node" ${penDraft ? 'disabled' : ''}><option value="">+ Add node</option>${Object.keys(
    definitions,
  )
    .filter((x) => !['Output', 'Image Output', 'Image'].includes(x))
    .map((x) => `<option value="${x}">${nodeTypeLabel(x)}</option>`)
    .join(
      '',
    )}</select><button data-action="find-node" title="Search existing nodes in this composition">Find node</button><button data-action="graph-zoom-out" aria-label="Node graph zoom out" title="Zoom out (−)">−</button><output data-zoom="graph" aria-label="Node graph zoom"></output><button data-action="graph-zoom-in" aria-label="Node graph zoom in" title="Zoom in (+)">+</button><button data-action="graph-fit" title="Fit all nodes (Shift+1)">Fit all</button><button data-action="graph-fit-selection" title="Fit selected nodes (Shift+2)">Fit selection</button><button data-action="graph-reset" title="Reset graph to 100% and origin">Reset view</button><button data-action="graph-overview" aria-label="Toggle node graph overview" aria-controls="graph-overview" aria-expanded="${graphOverviewOpen}" title="Show or hide graph overview">Overview</button></div>${nodePresetControls()}<div class="graph-body"><div id="graph-overview" class="graph-overview" ${graphOverviewOpen ? '' : 'hidden'}><svg class="graph-overview-map" tabindex="0" role="img" aria-label="Node graph overview. Click or drag to navigate; arrow keys pan; Escape closes." preserveAspectRatio="none"></svg></div><div class="graph-scroll" tabindex="0" aria-label="Node graph viewport" aria-describedby="graph-navigation-hint" title="Space, middle or Ctrl/Cmd+Shift drag: pan · Alt or Ctrl/Cmd+wheel: zoom · Ctrl/Cmd+1: actual size"><div class="graph"><svg class="wires"></svg><svg class="graph-snap-guides" aria-hidden="true"></svg>${penDraft ? '<div inert class="pen-graph-preview">' : ''}${l.nodes.map((x) => nodeHTML(x, result)).join('')}${penDraft ? '</div>' : ''}</div></div></div><div class="visually-hidden" id="graph-navigation-hint">Space drag: pan · Alt or Ctrl/Cmd wheel: zoom · Shift+1: fit all · Shift+2: fit selection · Node drag: align edges/centers; Shift locks horizontal, vertical or 45°; Ctrl/Cmd bypasses snapping</div></section></main><aside class="right"><section class="layers"><div class="panel-head"><strong>Layers</strong><span class="spacer"></span><button class="mini" data-action="add-blank-layer" aria-label="Add blank composition" title="Add a blank composition with an empty vector Output">+ Blank</button></div><div class="layer-solo-status" role="status">${hasLayerSolo(doc) ? `Solo ${doc.layers.filter((l) => l.solo === true).length} · SVG/PNG <button class="mini" data-action="clear-layer-solo" aria-label="Clear all layer Solo">Clear</button>` : 'All visible layers · SVG/PNG'}<small>Hidden layers stay hidden</small></div>${[
    ...doc.layers,
  ]
    .reverse()
    .map(
      (x) =>
        `<div class="layer ${!x.visible ? 'is-hidden' : ''} ${x.guide ? 'is-guide' : ''} ${(artworkTarget ? artworkSelection.has(x.id) : x.id === l.id) ? 'selected' : ''}" data-layer="${x.id}"><button class="layer-visibility" data-visible="${x.id}" aria-label="${x.visible ? 'Hide' : 'Show'} layer: ${esc(x.name)}" aria-pressed="${x.visible}" title="${x.visible ? 'Hide' : 'Show'} layer in canvas and export">${x.visible ? 'Hide' : 'Show'}</button><button class="layer-solo" data-layer-solo="${x.id}" aria-label="Solo layer: ${esc(x.name)}" aria-pressed="${x.solo === true}" title="Solo in preview and SVG/PNG; multiple allowed; Hide remains in effect">S</button>${nameEdit?.layerId === x.id && !nameEdit.nodeId ? nameInput('layer', x.id) : `<button class="name" data-select-layer="${x.id}" aria-pressed="${artworkTarget ? artworkSelection.has(x.id) : false}" title="Click to select; Shift-click to add/remove. Double-click or F2 to rename">${esc(x.name)}</button>`}<button data-layer-guide="${x.id}" class="${x.guide ? 'active' : ''}" aria-label="${x.guide ? 'Make artwork layer' : 'Make guide layer'}: ${esc(x.name)}" aria-pressed="${!!x.guide}" title="${x.guide ? 'Guide layer: excluded from SVG' : 'Convert to guide layer'}" ${x.locked ? 'disabled' : ''}>${x.guide ? 'Guide' : '◇'}</button><button data-layer-lock="${x.id}" title="Toggle layer lock">${x.locked ? '🔒' : '○'}</button><div class="layer-actions">${['backward', 'forward'].map((direction) => `<button data-order-layer="${x.id}" data-order-direction="${direction}" aria-label="${direction === 'forward' ? 'Forward' : 'Backward'} one position: ${esc(x.name)}">${direction === 'forward' ? 'Forward ↑' : 'Backward ↓'}</button>`).join('')}<button class="danger" data-delete-layer="${x.id}" aria-label="Delete layer: ${esc(x.name)}" ${x.locked || deletionBusy() ? 'disabled' : ''} title="Delete this entire layer and its graph (Undo available)">Delete layer</button></div></div>`,
    )
    .join(
      '',
    )}${penDraft ? '<div class="layer selected" data-pen-draft role="status">Curve · Draft (not saved)</div>' : ''}</section>${alignmentPanel(move)}<section class="inspector"><div class="panel-head"><strong>Inspector</strong></div><div class="inspector-body">${sourceNavigationPanel()}${penDraft ? '<p role="status">Draft curve. Finish to edit its graph and properties; Cancel path discards it.</p><div inert>' : ''}${
    n
      ? `<h3>${esc(nodeDisplayName(n))}</h3>${bypassControl(n)}<p>Node · ${esc(nodeTypeLabel(n.type))} · ${outputType(n) || 'Scalar X/Y'}</p>${['Rectangle', 'Ellipse', 'Circle', 'Polygon', 'Star', 'Text'].includes(n.type) ? `<p class="font-note">Dimension handles edit selected ${esc(nodeTypeLabel(n.type))} output before downstream nodes. Position and its reference stay fixed.${n.type === 'Text' ? ' Font size changes glyph size, not a layout box width.' : ''}</p>` : ''}${operationControls(n)}${strokeControls(n)}${gradientControls(n)}${offsetControls(n)}${mirrorControls(n)}${cornerRoundControls(n)}${trimControls(n)}${setPathStartControls(n)}${setClosureControls(n)}${reversePathControls(n)}${subdivideControls(n)}${expandStrokeControls(n)}${convertToPathControls(n)}${simplifyControls(n)}${puckerBloatControls(n)}${waveControls(n)}${taperControls(n)}${autoHandlesControls(n)}${bendControls(n)}${twistControls(n)}${dashControls(n)}${resampleControls(n)}${roughenControls(n)}${smoothControls(n)}${radialControls(n)}${positionReferenceControls(n)}${transformControls(n)}${positionControllerControls(n)}${referenceControls(n, result)}${booleanInputNote(n)}${instancePointsControls(n)}${luminanceControls(n)}${n.type === 'Join Geometry' ? `${n.joinInputOrder.length >= 1024 ? '<p class="font-note">Input socket limit reached (1,024). Disconnect an unlocked additional input to free a socket.</p>' : ''}<p class="font-note">Connect paths; the last input adds a spare automatically (up to 1,024 sockets; 256 geometry groups). Bundle inputs without cutting or welding. Shared transforms and paint apply to all; overlapping transparent groups remain independently painted. Use Boolean Union to merge silhouettes.</p>` : ''}${repeaterControls(n)}${characterControls(n)}${textControls(n)}${multiPathControls(n)}${['Curve', 'Multi Path'].includes(n.type) ? ownedPointFields(n) : ''}${editorParameters(
          n,
        )
          .filter(
            ([k, q]) =>
              (isTransform(n) ||
                Object.hasOwn(n.params, k) ||
                (n.type === 'Boolean' && k === booleanPendingInputKey)) &&
              !['Path', 'PaintedPath', 'Image'].includes(q.type),
          )
          .map(([k, q]) => field(n, k, q))
          .join('')}`
      : artworkTarget
        ? `<h3>Artwork · ${artworkSelection.size > 1 ? `${artworkSelection.size} selected` : esc(l.name)}</h3><button data-duplicate-artwork="${esc(l.id)}" ${!duplication.supported || deletionBusy() ? 'disabled' : ''} title="${esc(duplication.supported ? 'Create independent copies immediately above each source at the same coordinates (one Undo)' : duplication.reason)}">Duplicate in place</button><p class="artwork-duplicate-status">${esc(duplication.supported ? '' : duplication.reason)}</p>${artworkSelection.size > 1 ? `<p class="artwork-active">Active layer ${esc(l.name)}</p>` : ''}<p class="artwork-move-status">${esc(move.supported ? 'Drag artwork to move' : move.reason)}</p>`
        : hasEmptyOutput(l)
          ? '<h3>Empty composition</h3>'
          : '<p>Select artwork or a graph node.</p>'
  }<hr style="border:0;border-top:1px solid #d8dde2"><div class="field"><label>Layer name</label><input class="string" data-layer-name value="${esc(l.name)}" aria-label="Layer name"></div><div class="field"><label for="artboard-unit">Artboard units</label><select id="artboard-unit" data-artboard-unit aria-label="Artboard units">${ARTBOARD_UNITS.map((unit) => `<option value="${unit}" ${unit === artboardUnit ? 'selected' : ''}>${unit}</option>`).join('')}</select></div><div class="field"><label>Artboard W (${artboardUnit})</label><input type="number" step="any" data-dimension="width" data-dimension-unit="${artboardUnit}" value="${formatDocumentLength(doc.width, artboardUnit)}" aria-label="Artboard width (${artboardUnit})"></div><div class="field"><label>Artboard H (${artboardUnit})</label><input type="number" step="any" data-dimension="height" data-dimension-unit="${artboardUnit}" value="${formatDocumentLength(doc.height, artboardUnit)}" aria-label="Artboard height (${artboardUnit})"></div>${results
    .filter(([, r]) => r.error || r.warnings.length)
    .map(
      ([x, r]) => `<p class="warning">${esc(x.name)}: ${esc(r.error || r.warnings.join(' '))}</p>`,
    )
    .join(
      '',
    )}${penDraft ? '</div>' : ''}</div></section></aside></div><footer class="status ${error ? 'error' : ''}"><span class="message">${esc(message)}</span><span class="spacer"></span><span>Local document · ${l.nodes.length} nodes</span></footer><input id="image-file" type="file" accept=".png,.jpg,.jpeg,image/png,image/jpeg" hidden><input id="font-file" type="file" accept=".ttf,.otf,font/ttf,font/otf" hidden><input id="open-file" type="file" accept=".json,application/json" hidden><input id="import-svg-file" type="file" accept=".svg,image/svg+xml" hidden><input id="svg-part-file" type="file" accept=".svg,image/svg+xml" hidden>`;
  renderCompositionTablist();
  mountViewers(app, viewerState, {
    doc,
    fonts,
    layer: l,
    node: n,
    canAssign: selection.size === 1,
    busy: !!(penDraft || drag || wiring || composing || canvasTextEdit),
    evaluations: new Map(results.map(([layer, result]) => [layer.id, result])),
  });
  const pngExportButton = app.querySelector('[data-action="export-png"]');
  if (pngExportButton) {
    pngExportButton.insertAdjacentHTML('afterend', exportPresetPanelMarkup());
    const panel = app.querySelector('#export-presets');
    if (panel) panel.open = exportPresetsOpen;
    updateExportPresetSaveLabel('svg');
    updateExportPresetSaveLabel('png');
  }
  const svgPrecision = app.querySelector('#svg-export-precision');
  if (svgPrecision) svgPrecision.value = svgExportPrecision;
  prepareNumericInputs(app);
  for (const row of app.querySelectorAll('.layer[data-layer]')) row.tabIndex = 0;
  // Keep controls and actionable states visible; put reference prose in
  // one optional help disclosure per editing region.
  for (const region of app.querySelectorAll('.node-body, .inspector-body')) {
    const notes = [...region.querySelectorAll('.font-note')].filter(
      (note) => !note.hasAttribute('role') && !note.textContent.startsWith('At a wrap break'),
    );
    if (!notes.length) continue;
    const help = document.createElement('details');
    help.className = 'control-help';
    const summary = document.createElement('summary');
    summary.textContent = 'Help';
    help.append(summary, ...notes);
    region.append(help);
  }
  for (const control of app.querySelectorAll(
    '#add-node, [data-font], [data-character-style="font"]',
  )) {
    control.setAttribute('aria-haspopup', 'dialog');
    control.title =
      control.id === 'add-node' ? 'Search nodes (Ctrl+Space)' : 'Search and preview fonts';
  }
  const duplicateButton = document.querySelector('[data-duplicate-artwork]');
  if (duplicateButton) {
    duplicateButton.dataset.duplicateSelection = JSON.stringify([...artworkSelection]);
    duplicateButton.dataset.eligible = String(duplication.supported);
    duplicateButton.dataset.availableTitle = duplicateButton.title;
  }
  const instanceControl = document.querySelector('[data-character-instance]');
  if (instanceControl && characterEdit) instanceControl.value = characterEdit.route || '';
  const characterList = document.querySelector('[data-character-selection]');
  if (characterList && characterEdit)
    for (const option of characterList.options)
      option.selected = characterEdit.selected.has(option.value);
  if (resetView) {
    graphZoom = 1;
    canvasFit = true;
    pan = [0, 0];
  }
  document.querySelector('.graph').style.zoom = graphZoom;
  updateGraphBounds(oldScroll);
  const gs = document.querySelector('.graph-scroll');
  gs.scrollLeft = oldScroll[0];
  gs.scrollTop = oldScroll[1];
  for (const el of document.querySelectorAll('.graph details[data-detail]')) {
    const key = `${el.closest('.node').dataset.id}/${el.dataset.detail}`;
    if (detailsState.has(key)) el.open = detailsState.get(key);
  }
  observeGraphOverview();
  revealActivePointRow();
  for (const select of app.querySelectorAll('[data-stroke-preset]'))
    select.value = select.querySelector('option[selected]').value;
  fitCanvas();
  compactTextInspector();
  if (canvasEditor && canvasTextEdit && current().id === canvasTextEdit.layerId)
    app.querySelector('.canvas-stage').append(canvasEditor);
  textTools?.sync();
  updateNavigation();
  restoreFocus(app, focused);
  if (pointScroll)
    app.querySelectorAll('.point-panel-rows').forEach((el, i) => {
      el.scrollTop = pointScroll[i] ?? 0;
    });
  if (canvasEditorFocused && canvasEditor?.isConnected) canvasEditor.focus({ preventScroll: true });
  document.querySelector('.inspector').scrollTop = inspectorScroll;
  document.querySelector('.layers').scrollTop = layersScroll;
  ['.guides-panel', '.guide-list'].forEach((selector, index) => {
    const el = document.querySelector(selector);
    if (el) el.scrollTop = guideScroll[index];
  });
  requestAnimationFrame(() => {
    updateGraphBounds();
    drawWires();
  });
  if (fontPreview || textPreview) updateCanvas(true);
}
function editGuides(type, payload) {
  snapMatches = [];
  return commandChange(type, payload, 'Editing aids updated');
}
function layerShown(l) {
  return layerIsVisible(history.doc, l) && (!l.guide || guideSettings(history.doc).visible);
}
function layerShape(l, path) {
  return renderElement(
    path,
    { ...l, name: l.id },
    history.doc.imageAssets,
    `class="shape ${l.guide ? 'guide-layer-shape' : ''}"`,
    `nect-gradient-${history.doc.layers.indexOf(l)}`,
  );
}
function layoutConfig() {
  return layoutDraft || guideSettings(history.doc).layoutGrid || layoutGridDefaults();
}
function layoutPanel() {
  const a = guideSettings(history.doc),
    config = layoutConfig();
  let error = '',
    lines = [];
  try {
    lines = layoutGridLines(history.doc, config);
  } catch (e) {
    error = e.message;
  }
  const locked = a.locked || a.guides.some((g) => g.origin === 'layout' && g.locked);
  return `<div class="layout-grid-panel"><strong>Layout grid</strong><div class="layout-grid-fields">${[
    ['columns', 'Columns'],
    ['rows', 'Rows'],
    ['marginX', 'Margin X'],
    ['marginY', 'Margin Y'],
    ['gutterX', 'Gutter X'],
    ['gutterY', 'Gutter Y'],
  ]
    .map(
      ([key, label]) =>
        `<label>${label}<input type="number" data-layout-field="${key}" aria-label="Layout ${label}" min="${['columns', 'rows'].includes(key) ? 1 : 0}" max="${['columns', 'rows'].includes(key) ? 40 : 100000}" step="${['columns', 'rows'].includes(key) ? 1 : 'any'}" value="${Number.isFinite(config[key]) ? config[key] : ''}"></label>`,
    )
    .join(
      '',
    )}</div><label><input type="checkbox" data-layout-preview ${layoutPreview ? 'checked' : ''}>Preview layout</label><p class="layout-grid-summary ${error ? 'warning' : ''}">${esc(error || `${lines.length} guides · artboard-based · margins on both sides`)}</p><div class="guide-add"><button data-layout-apply ${error || locked ? 'disabled' : ''}>${a.layoutGrid ? 'Update layout' : 'Create layout'}</button><button data-layout-cancel>Cancel</button><button data-layout-remove ${!a.layoutGrid || locked ? 'disabled' : ''}>Remove layout</button></div><p>Updates replace only layout guides. Manual guides stay. Unlock layout guides before updating.</p></div>`;
}
function layoutOverlay() {
  if (!layoutPreview || !layoutOpen) return '';
  try {
    return layoutGridLines(history.doc, layoutConfig())
      .map((g) => guideLine(g.axis, g.position, 'class="layout-preview"'))
      .join('');
  } catch {
    return '';
  }
}
function guidesPanel() {
  if (!guidesOpen) return '';
  const a = guideSettings(history.doc);
  return `<section class="guides-panel" aria-label="Guides and snapping"><div class="guide-toggles">${[
    ['visible', 'Show guides'],
    ['locked', 'Lock all guides'],
    ['snapGuides', 'Snap guides'],
    ['snapArtboard', 'Snap artboard'],
    ['snapArtwork', 'Snap artwork'],
  ]
    .map(
      ([key, label]) =>
        `<label><input type="checkbox" data-guide-setting="${key}" ${a[key] ? 'checked' : ''}>${label}</label>`,
    )
    .join(
      '',
    )}</div><div class="guide-add"><button data-add-guide="y" ${a.locked ? 'disabled' : ''}>+ Horizontal</button><button data-add-guide="x" ${a.locked ? 'disabled' : ''}>+ Vertical</button></div><div class="guide-list">${a.guides.map((g, i) => `<div class="guide-row ${selectedGuide === g.id ? 'selected' : ''}"><button data-select-guide="${g.id}" aria-label="Select guide ${i + 1}" aria-pressed="${selectedGuide === g.id}">${g.origin === 'layout' ? '▦ ' : ''}${g.axis === 'x' ? 'V · X' : 'H · Y'}</button><input type="number" step="any" min="-100000" max="100000" data-guide-position="${g.id}" aria-label="Guide ${i + 1} position" value="${g.position}" ${a.locked || g.locked ? 'disabled' : ''}><button data-lock-guide="${g.id}" aria-label="${g.locked ? 'Unlock' : 'Lock'} guide ${i + 1}" aria-pressed="${g.locked}" ${a.locked ? 'disabled' : ''}>${g.locked ? '🔒' : '○'}</button><button data-delete-guide="${g.id}" aria-label="Delete guide ${i + 1}" ${a.locked || g.locked ? 'disabled' : ''}>×</button></div>`).join('')}</div><p>Drag a guide to move it. Hidden guides do not snap; locked guides still snap. Snap artwork aligns to other visible layers’ geometry edges and centers (without stroke), including locked layers. The editing layer is excluded. Alt bypasses snapping.</p></section>`;
}
function gridPanel() {
  if (!gridOpen) return '';
  const a = guideSettings(history.doc);
  return `<section class="guides-panel" aria-label="Grid"><div class="guide-toggles">${[
    ['gridVisible', 'Show grid'],
    ['snapGrid', 'Snap grid'],
  ]
    .map(
      ([key, label]) =>
        `<label><input type="checkbox" data-guide-setting="${key}" ${a[key] ? 'checked' : ''}>${label}</label>`,
    )
    .join(
      '',
    )}</div><label>Grid spacing <input type="number" step="any" min="1" max="10000" data-guide-setting="gridSize" aria-label="Grid spacing" value="${a.gridSize}"></label><label>Grid opacity <input type="range" min="0" max="100" step="1" data-grid-opacity aria-label="Grid opacity" value="${a.gridOpacity * 100}"><input type="number" min="0" max="100" step="1" data-grid-opacity aria-label="Grid opacity percent" value="${a.gridOpacity * 100}">%</label><p>${a.gridAnchor === 'center' ? 'Origin: exact artboard center' : 'Origin: legacy top-left <button data-action="center-grid">Center grid</button>'}</p><p>Grid snapping works even when hidden or transparent. Alt bypasses snapping.</p></section>`;
}
function gridOverlay() {
  const a = guideSettings(history.doc);
  if (!a.gridVisible) return '';
  const [x, y] = resolveGridOrigin(history.doc, a);
  return `<defs><pattern id="editing-grid" patternTransform="translate(${x} ${y})" width="${a.gridSize}" height="${a.gridSize}" patternUnits="userSpaceOnUse"><path d="M ${a.gridSize} 0 H 0 V ${a.gridSize}" class="editing-grid-line"/></pattern></defs><rect data-grid-overlay width="${history.doc.width}" height="${history.doc.height}" opacity="${gridOpacityDraft ?? a.gridOpacity}" fill="url(#editing-grid)" pointer-events="none"/>`;
}
function previewGridOpacity(value) {
  gridOpacityDraft = value;
  app.querySelector('[data-grid-overlay]')?.setAttribute('opacity', value);
  for (const input of app.querySelectorAll('[data-grid-opacity]'))
    input.value = String(value * 100);
}
function cancelGridOpacity() {
  if (gridOpacityDraft === null) return;
  previewGridOpacity(guideSettings(history.doc).gridOpacity);
  gridOpacityDraft = null;
}
function guideLine(axis, position, attributes) {
  return `<line ${axis === 'x' ? `x1="${position}" x2="${position}" y1="0" y2="${history.doc.height}"` : `y1="${position}" y2="${position}" x1="0" x2="${history.doc.width}"`} ${attributes}/>`;
}
function guidesOverlay() {
  const a = guideSettings(displayDocument());
  return (
    (a.visible
      ? a.guides
          .map(
            (g) =>
              guideLine(
                g.axis,
                g.position,
                `class="editing-guide ${selectedGuide === g.id ? 'selected' : ''} ${a.locked || g.locked ? 'locked' : ''}" data-guide="${g.id}" data-axis="${g.axis}"`,
              ) +
              guideLine(
                g.axis,
                g.position,
                `class="guide-hit ${a.locked || g.locked ? 'locked' : ''}" data-guide="${g.id}" data-axis="${g.axis}"`,
              ),
          )
          .join('')
      : '') +
    layoutOverlay() +
    snapMatches
      .map((m) =>
        m.kind === 'direction'
          ? `<path class="snap-match" data-snap-kind="direction" d="${directionGuidePath(m)}" fill="none"/>`
          : m.kind === 'character-gap'
            ? `<path class="snap-match" data-snap-kind="character-gap" d="${characterGapGuidePath(m, canvasScale())}" fill="none"/>`
            : guideLine(
                m.axis ? 'y' : 'x',
                m.position,
                `class="snap-match" data-snap-kind="${m.kind}"`,
              ),
      )
      .join('')
  );
}
function transformControls(n) {
  if (n.type !== 'Transform') return '';
  const disabled =
    current().locked ||
    n.bypassed ||
    ['anchor', 'position'].some((key) => {
      const q = n.params[key];
      return q && (q.locked || q.source || q.expression);
    });
  return `<section aria-label="Reposition anchor"><h4>Reposition anchor</h4><div style="display:grid;grid-template-columns:repeat(3,1fr);gap:3px">${['Top left', 'Top center', 'Top right', 'Middle left', 'Center', 'Middle right', 'Bottom left', 'Bottom center', 'Bottom right'].map((label, index) => `<button class="mini" data-anchor-preset="${index}" data-anchor-node="${n.id}" ${disabled ? 'disabled' : ''}>${label}</button>`).join('')}</div><p class="font-note">Keep artwork in place while moving its rotation/scale pivot. Presets use input path bounds. Drag the canvas Anchor marker for a custom pivot. Controls show selected Transform output before downstream nodes. Anchor and Position must be unlocked and undriven. Collapsed scales support presets only.</p></section>`;
}

function positionControllerControls(n) {
  if (n.type === 'Vector')
    return `<div class="field"><label>Canvas position handle <input type="checkbox" data-vector-position-handle="${n.id}" aria-label="Canvas position handle" ${n.canvasPositionHandle === true ? 'checked' : ''} ${current().locked ? 'disabled' : ''}></label></div><p class="font-note">Optional: interpret Value XY as an artwork position for canvas editing. Off by default for direction, size or offset vectors. When on, select this node and drag its diamond; Alt bypasses snapping. The handle changes Value directly, before downstream Transform, and never exports. Output stays Vector2. Connected, expression-driven or locked Value is read-only on canvas.</p>`;
  return n.type === 'Null'
    ? '<p class="font-note">Position-only controller. Select this node and drag its crosshair; Alt bypasses snapping. Only the selected Null is shown, and the marker never exports. Connect Position OUT to Text, shape Position, or Vector Math. Coordinates are before downstream Transform. Connected, expression-driven or locked Position is read-only on canvas.</p>'
    : '';
}
// Only dedicated Nulls and explicitly opted-in Vectors are canvas positions.
// Both edit the same parameter shown in numeric fields; no separate point state.
function positionController(n) {
  if (n?.type === 'Null') return { key: 'position', kind: 'null', label: 'Position' };
  if (n?.type === 'Vector' && n.canvasPositionHandle === true)
    return { key: 'value', kind: 'vector', label: 'Value as canvas position' };
  return null;
}
function radialControls(n) {
  return ['Polygon', 'Star'].includes(n.type)
    ? '<p class="font-note">Closed straight-edge path. Count: 3–256. Radii: >0 to 100000 px; inner ≤ outer. Rotation: clockwise degrees from the top vertex. Count changes suspend existing vertex edits.</p>'
    : '';
}
function canvasScale() {
  const ctm = document.querySelector('.artboard')?.getScreenCTM();
  return Math.hypot(ctm?.a ?? 1, ctm?.b ?? 0) || 1;
}
function snapped(
  sources,
  delta,
  event,
  excludedGuide = null,
  direction = null,
  origin = drag?.directionOrigin || drag?.start || [0, 0],
  offset = drag?.directionOffset || [0, 0],
) {
  const scale = canvasScale();
  const result = snapDirectionTranslation(
    (allowed, value, targetScale) =>
      snapTranslation(
        history.doc,
        allowed,
        value,
        targetScale,
        event.altKey,
        excludedGuide,
        snapLayerBounds.guides,
        snapLayerBounds.artwork,
        drag?.type === 'character' ? drag.snapCandidates : [],
      ),
    sources,
    delta,
    { direction, scale, origin, offset, enabled: guideSettings(history.doc).snap && !event.altKey },
  );
  snapMatches = result.matches;
  return result.delta;
}
// Apply the constraint before snapping and suppress every forbidden-axis target.
// Character drags call this before their inverse output-transform conversion.
function artworkDelta(sources, delta, event) {
  const offset = drag.directionOffset || [0, 0];
  const handle =
    ['pen', 'point-handles'].includes(drag.type) ||
    (['point', 'remap'].includes(drag.type) && drag.key !== 'position');
  const constraint = constrainDirectionDrag(
    handle ? {} : drag,
    delta.map((v, i) => v + offset[i]),
    [event.clientX - drag.startClient[0], event.clientY - drag.startClient[1]],
    event.shiftKey,
  );
  const allowed = sources.map((values, axis) =>
    constraint.pending || (constraint.direction && !constraint.direction[axis]) ? [] : values,
  );
  const movement = constraint.pending ? [0, 0] : constraint.delta.map((v, i) => v - offset[i]);
  return snapped(allowed, movement, event, null, constraint.direction);
}
const artworkDrag = () =>
  ['character', 'position-controller', 'shape', 'point', 'remap', 'artwork'].includes(drag?.type);
const directionDrag = () =>
  artworkDrag() ||
  ['pen', 'point-handles', 'path-unit'].includes(drag?.type) ||
  (drag?.type === 'transform' && drag.key === 'anchor');
function captureArtworkPointer(event, capture) {
  if (['point', 'remap'].includes(drag.type) && drag.key !== 'position') {
    const context = directPointContext(selectedNode());
    const path = context?.path || renderEvaluation.run(drag.nodeId);
    const p = path.contours.flatMap((c) => c.points).find((p) => p.id === drag.pointId);
    drag.directionOrigin = [...p.position];
    drag.directionOffset = [...p[drag.key]];
  }
  drag.startClient = [event.clientX, event.clientY];
  drag.pointerId = event.pointerId;
  drag.capture = capture;
  capture.setPointerCapture?.(event.pointerId);
}
// Character position is a source edit, with an explicit final-output occurrence.
function characterContext(result = renderEvaluation) {
  const layer = displayLayer(),
    n = artworkTarget ? undefined : layer.nodes.find((node) => node.id === selected);
  if (
    !characterEdit ||
    n?.id !== characterEdit.node ||
    current().id !== characterEdit.layer ||
    n.type !== 'Text'
  )
    return null;
  let typographyCharacters = [],
    typographyReason = null;
  try {
    const ids = new Set(characterIdsInRange(n.text, n.characterPositions, 0, n.text.length));
    const fresh = createCharacterPositions(n.text),
      record = n.characterPositions?.basis === fresh.basis ? n.characterPositions : fresh;
    const defaults = result.textDefaults(n.id);
    typographyCharacters = record.units
      .map((unit, sourceIndex) => ({
        id: unit.id,
        text: unit.text,
        sourceIndex,
        whitespace: /^\s$/.test(unit.text),
        ...defaults,
        ...(record.styles?.[unit.id] || {}),
      }))
      .filter((unit) => ids.has(unit.id));
    characterEdit.selected = new Set([...characterEdit.selected].filter((id) => ids.has(id)));
  } catch (error) {
    typographyReason = error.message;
  }
  const typographySelected = typographyCharacters.filter((unit) =>
    characterEdit.selected.has(unit.id),
  );
  try {
    const layout = result.textLayout(n.id),
      occurrences = characterOccurrences(result.path)
        .filter((o) => o.source === n.id)
        .map((o) => {
          let reason = o.reason;
          try {
            inverseCharacterDelta(o.matrix, [0, 0]);
          } catch (e) {
            reason ||= e.message;
          }
          return { ...o, reason };
        });
    const editable = occurrences.filter((o) => !o.reason);
    if (!characterEdit.route && editable.length === 1) characterEdit.route = editable[0].route;
    if (characterEdit.route && !editable.some((o) => o.route === characterEdit.route))
      characterEdit.route = null;
    return {
      n,
      typographyCharacters,
      typographySelected,
      typographyReason,
      layout,
      occurrences,
      editable,
      occurrence: editable.find((o) => o.route === characterEdit.route),
      selected: layout.characters.filter((g) => characterEdit.selected.has(g.id)),
    };
  } catch (e) {
    return {
      n,
      typographyCharacters,
      typographySelected,
      typographyReason,
      layout: { characters: [], reason: e.message },
      occurrences: [],
      editable: [],
      selected: [],
    };
  }
}
function characterFields(context, floating = false) {
  const disabled = current().locked || !context.selected.length || !!context.layout.reason;
  return `<div class="character-values">${['X', 'Y']
    .map((axis, i) => {
      const values = context.selected.map((g) => g.offset[i]),
        mixed = values.some((v) => v !== values[0]);
      return `<label>Δ${axis}<input type="number" step="any" min="-100000" max="100000" data-scrub-step="1" title="Drag horizontally to adjust; click to type" data-character-axis="${i}" aria-label="${floating ? 'Floating ' : ''}Character delta ${axis}" placeholder="${mixed ? 'Mixed' : '0'}" value="${mixed ? '' : (values[0] ?? 0)}" ${disabled || maintainedAxis(context.n) === i ? 'disabled' : ''}></label>`;
    })
    .join(
      '',
    )}<button data-character-reset="selected" ${maintainedAxis(context.n) !== null || disabled ? 'disabled' : ''} title="Reset selected XY offsets only">Reset XY</button></div><div class="character-values character-shape-values">${[
    ['rotation', 'Rotate °', -36000, 36000, 0],
    ['scale', 'Scale ×', 0.01, 100, 1],
  ]
    .map(([key, label, min, max, fallback]) => {
      const values = context.selected.map((g) => g[key]),
        mixed = values.some((v) => v !== values[0]);
      return `<label>${label}<input type="number" step="${key === 'scale' ? '0.01' : '1'}" min="${min}" max="${max}" data-scrub-step="${key === 'scale' ? 0.01 : 1}" title="Drag horizontally to adjust; click to type" data-character-transform="${key}" aria-label="${floating ? 'Floating ' : ''}Character ${key}" placeholder="${mixed ? 'Mixed' : fallback}" value="${mixed ? '' : (values[0] ?? fallback)}" ${disabled ? 'disabled' : ''}></label>`;
    })
    .join(
      '',
    )}<button data-character-shape-reset ${disabled ? 'disabled' : ''} title="Reset selected rotation and scale only">Reset shape</button></div>`;
}
function initialManualLeading(n) {
  let distance;
  try {
    distance = renderEvaluation.textLayout(n.id)?.lineOffsets?.[1];
  } catch {
    // Missing font coverage must not prevent editing layout defaults.
  }
  if (!Number.isFinite(distance)) {
    const defaults = renderEvaluation.textDefaults(n.id);
    distance = defaults.size * defaults.lineSpacing;
  }
  return Math.min(100000, Math.max(0.01, distance));
}
function wrapModeOptions(n) {
  const mode = n.textWrap ? (n.textWrap.policy ? 'basic' : 'width') : 'off';
  return [
    ['off', 'Off · explicit newlines'],
    ['basic', 'Japanese basic'],
    ...(n.writingMode === 'vertical-rl' ? [] : [['width', 'Legacy horizontal wrap']]),
  ]
    .map(
      ([value, label]) =>
        `<option value="${value}" ${mode === value ? 'selected' : ''}>${label}</option>`,
    )
    .join('');
}
function wrapControls(n) {
  const disabled = current().locked ? 'disabled' : '',
    extent = n.writingMode === 'vertical-rl' ? 'height' : 'width';
  return `<section class="text-wrap-controls" aria-label="Text wrap"><label>Line composition<select data-text-wrap="${n.id}" aria-label="Text wrap mode" ${disabled}>${wrapModeOptions(n)}</select></label>${n.textWrap ? `<label>Wrap ${extent} px<input type="number" step="any" min="0.01" max="100000" data-scrub-step="1" title="Drag horizontally to adjust; click to type" data-text-wrap-width="${n.id}" aria-label="Text wrap ${extent}" value="${wrapExtent(n.textWrap)}" ${disabled}></label>` : ''}<p class="font-note">Long unbreakable groups may overflow.</p></section>`;
}
function leadingControls(n) {
  const mode = n.leading?.mode || 'legacy';
  return `<div class="text-leading-controls"><label>Line / column leading<select data-text-leading="${n.id}" aria-label="Text leading mode" ${current().locked ? 'disabled' : ''}>${[
    ['legacy', 'Legacy spacing'],
    ['auto', 'Auto · font metrics'],
    ['manual', 'Manual · exact px'],
  ]
    .map(
      ([value, label]) =>
        `<option value="${value}" ${mode === value ? 'selected' : ''}>${label}</option>`,
    )
    .join(
      '',
    )}</select></label>${mode === 'manual' ? `<label>Baseline / column distance px<input type="number" step="any" min="0.01" max="100000" data-scrub-step="1" title="Drag horizontally to adjust; click to type" data-text-leading-value="${n.id}" aria-label="Text manual leading" value="${n.leading.value}" ${current().locked ? 'disabled' : ''}></label>` : ''}<p class="font-note">Auto uses adjacent font metrics and lineSpacing. Manual sets the exact baseline or column distance and can overlap. This is not paragraph spacing.</p></div>`;
}
function pairContext(context = characterContext()) {
  if (!context || context.typographyReason || context.layout.reason) return null;
  const rows = context.typographySelected;
  try {
    const ids = rows.map((g) => g.id),
      key = characterPairFromIds(context.n.text, context.n.characterPositions, ids),
      evaluated = context.layout.pairKerning?.[key];
    if (!evaluated) return null;
    const line = (index) =>
      context.layout.lines?.findIndex((l) => index >= l.sourceStart && index < l.sourceEnd);
    return {
      ...evaluated,
      key,
      ids,
      rows,
      dormant: line(rows[0].sourceIndex) !== line(rows[1].sourceIndex),
    };
  } catch {
    return null;
  }
}
function matchesPairInput(input, context, pair) {
  return (
    pair &&
    context.n.id === input.dataset.kerningNode &&
    JSON.stringify(context.n.text) === input.dataset.kerningText &&
    pair.key === input.dataset.characterKerning
  );
}
function pairKerningControls(context) {
  const pair = pairContext(context),
    disabled = current().locked || !pair || maintainedAxis(context.n) !== null;
  const shown = pair
    ? pair.rows.map((g) => (g.whitespace ? '[space]' : esc(g.text))).join(' | ')
    : 'Select exactly two adjacent characters';
  return `<section class="pair-kerning" aria-label="Pair kerning"><h4>Pair kerning</h4><p>${shown}${pair ? ` · source ${pair.rows.map((g) => g.sourceIndex + 1).join(', ')} · ${pair.manual ? 'Manual' : 'Inherited'}` : ''}</p><label>Pair kerning (1/1000 em)<input type="number" min="-100000" max="100000" step="any" data-scrub-step="1" title="Drag horizontally to adjust; click to type" data-character-kerning="${pair?.key || ''}" data-kerning-node="${esc(context.n.id)}" data-kerning-text="${esc(JSON.stringify(context.n.text))}" aria-label="Pair kerning (1/1000 em)" placeholder="${pair && !pair.manual ? `Inherited (${pair.value})` : ''}" value="${pair?.manual ? pair.value : ''}" ${disabled ? 'disabled' : ''}></label><button data-character-kerning-inherit ${disabled || !pair?.manual ? 'disabled' : ''}>Inherit pair</button>${pair?.dormant ? '<p class="font-note">At a wrap break; applies when these characters share a line.</p>' : ''}<p class="font-note">${pair ? `${Number(pair.value.toFixed(5))} units / ${Number(pair.pixels.toFixed(5))} px. ` : ''}Manual values replace this pair’s font kerning; 0 explicitly removes it. Tracking remains additive. The left character’s font size and inline scale determine the em distance. Use Kerning at cursor above or select exactly two adjacent characters.</p></section>`;
}
function typographyControls(context) {
  const disabled =
    current().locked || !context.typographySelected.length || !!context.typographyReason;
  const values = context.typographySelected.map((g) => g.font);
  const mixed = values.some((v) => v !== values[0]);
  const fontValue = mixed ? '__mixed' : values[0];
  return `<section class="character-typography" aria-label="Selected character typography"><h4>Selected typography</h4>
    <label>Font<select data-character-style="font" aria-label="Selected character font" ${disabled || fontRequest ? 'disabled' : ''}>
    <option value="__mixed" disabled ${mixed ? 'selected' : ''}>Mixed</option><option value="">Inherit default font</option>
    <option value="default" ${fontValue === 'default' ? 'selected' : ''}>DejaVu Sans (Latin)</option>
    ${Object.entries(history.doc.fontAssets || {})
      .map(
        ([id, asset]) =>
          `<option value="${esc(id)}" ${fontValue === id ? 'selected' : ''}>${esc(asset.name)}</option>`,
      )
      .join('')}
    ${desktop && systemFonts.length ? `<optgroup label="Installed Windows fonts">${systemFonts.map(([id, name, reason]) => `<option value="${esc(id)}" ${reason ? 'disabled' : ''}>${esc(name)}${reason ? ` — ${esc(reason)}` : ''}</option>`).join('')}</optgroup>` : ''}</select></label>
    <button class="mini" data-load-character-font="${context.n.id}" ${disabled || fontRequest ? 'disabled' : ''}>Load font for selection…</button>
    <div class="typography-fields">${[
      ['size', 'Font size px', 0.01, 100000, 1],
      ['width', 'Horizontal width %', 1, 1000, 100],
      ['height', 'Vertical scale %', 1, 1000, 100],
      ['tracking', 'Tracking px', -100000, 100000, 1],
      ['baselineShift', 'Baseline shift px', -100000, 100000, 1],
    ]
      .map(([key, label, min, max, factor]) => {
        const vals = context.typographySelected.map((g) => g[key] * factor),
          isMixed = vals.some((v) => v !== vals[0]);
        return `<div class="typography-field"><label>${label}<input type="number" step="any" min="${min}" max="${max}" data-scrub-step="1" title="Drag horizontally to adjust; click to type" data-character-style="${key}" aria-label="Selected character ${key}" placeholder="${isMixed ? 'Mixed' : ''}" value="${isMixed ? '' : (vals[0] ?? '')}" ${disabled || (key === 'tracking' && maintainedAxis(context.n) !== null) ? 'disabled' : ''}></label><button data-character-style-inherit="${key}" ${(key === 'tracking' && maintainedAxis(context.n) !== null) || disabled ? 'disabled' : ''} title="Remove selected ${label} overrides" aria-label="Inherit selected ${key}">Inherit</button></div>`;
      })
      .join(
        '',
      )}</div><button data-character-style-reset ${disabled ? 'disabled' : ''}>Reset selected typography</button>
    <p class="font-note">Width and vertical scale affect screen X and Y independently; the inline scale also reflows advances. Tracking adds px after each selected character except the last in its line; it does not replace pair kerning. Positive baseline shift moves up in horizontal text or right in vertical text, before rotation/shape scale/XY. Auto leading follows the adjusted metrics.</p></section>`;
}
function characterControls(n) {
  if (n.type !== 'Text') return '';
  const context = characterContext();
  return `<section class="character-panel" aria-label="Character position"><div class="character-actions"><button data-character-mode="${n.id}" aria-pressed="${!!context}" ${current().locked ? 'disabled' : ''}>${context ? 'Finish character position' : 'Character position'}</button><button data-edit-text="${n.id}">Edit text</button></div>${
    context
      ? `<p>${maintainedAxis(n) !== null ? 'Maintained gaps: select characters normally. Only cross-axis numeric edits or cross-axis Shift-drag can move them; disable maintained gaps for axial movement.' : 'Click a character, then drag. Shift-click selects several; Shift-drag locks horizontal or vertical. Arrows move 1 px; Shift moves 10 px.'}</p>${context.layout.reason ? `<p class="warning">${esc(context.layout.reason)}. Undo, restore the prior text, or reset all adjustments.</p>` : ''}${!context.editable.length ? `<p class="warning">${esc(context.occurrences[0]?.reason || 'This Text does not reach the visible Output.')} Local numeric editing remains available when character mapping is valid.</p>` : ''}${context.occurrences.length > 1 ? `<p>Editing shared Text: affects ${context.occurrences.length} displayed instances.</p>` : ''}${
          context.editable.length > 1
            ? `<label>Displayed instance<select data-character-instance aria-label="Character displayed instance"><option value="">Choose an instance</option>${context.editable
                .map(
                  (o, i) =>
                    `<option value="${esc(o.route)}" ${o.route === characterEdit.route ? 'selected' : ''}>Instance ${i + 1} · ${esc(
                      o.route
                        .split('/')
                        .slice(1)
                        .map(
                          (part) =>
                            current().nodes.find((n) => part.startsWith(n.id))?.name || part,
                        )
                        .join(' → ') || 'Text',
                    )}</option>`,
                )
                .join('')}</select></label>`
            : ''
        }<label>Characters <select multiple size="${Math.min(6, Math.max(2, context.typographyCharacters.length))}" data-character-selection aria-label="Select characters">${context.typographyCharacters.map((g) => `<option value="${g.id}" ${characterEdit.selected.has(g.id) ? 'selected' : ''}>${g.sourceIndex + 1} · ${g.whitespace ? '[space]' : esc(g.text)}</option>`).join('')}</select></label><p>${context.typographySelected.length} selected${context.typographySelected.some((g) => g.whitespace) ? ' · Moving a space changes its visual position only. Use letterSpacing/kerning for spacing.' : ''}</p>${typographyControls(context)}${pairKerningControls(context)}${characterFields(context)}<button data-character-reset="all" ${current().locked ? 'disabled' : ''}>Reset all adjustments and maintained gaps</button><p class="font-note">Local X points right; Y points down. Rotation (clockwise) and uniform scale use each normal-layout advance-cell center, then XY is added, before downstream Transform. Selected characters receive the same absolute value around their own pivots. These shape controls preserve font size and advance. Position and Position OUT stay fixed.</p>`
      : ''
  }</section>`;
}
function characterFloating() {
  const context = characterContext();
  if (!context?.selected.length || !context.occurrence) return '';
  return `<div class="character-floating" aria-label="Quick character position">${characterFields(context, true)}</div>`;
}
function positionCharacterFloating() {
  const toolbar = document.querySelector('.character-floating'),
    stage = document.querySelector('.canvas-stage'),
    svg = document.querySelector('.artboard'),
    context = characterContext();
  if (!toolbar || !stage || !context?.occurrence || !context.selected.length) return;
  const ctm = svg.getScreenCTM(),
    rect = stage.getBoundingClientRect();
  if (!ctm) return;
  const points = context.selected
    .flatMap((g) => characterBoxPoints(g, context.occurrence.matrix))
    .map((p) => new DOMPoint(...p).matrixTransform(ctm));
  const box = [
    Math.min(...points.map((p) => p.x)) - rect.left,
    Math.min(...points.map((p) => p.y)) - rect.top,
    Math.max(...points.map((p) => p.x)) - rect.left,
    Math.max(...points.map((p) => p.y)) - rect.top,
  ];
  const width = toolbar.offsetWidth || 250,
    height = toolbar.offsetHeight || 44,
    gap = 10;
  const clamp = (v, max) => Math.max(6, Math.min(Math.max(6, max - 6), v));
  const candidates = [
    [box[0], box[1] - height - gap],
    [box[0], box[3] + gap],
    [box[2] + gap, box[1]],
    [box[0] - width - gap, box[1]],
  ].map(([x, y]) => [clamp(x, stage.clientWidth - width), clamp(y, stage.clientHeight - height)]);
  const overlap = ([x, y]) =>
    Math.max(0, Math.min(x + width, box[2]) - Math.max(x, box[0])) *
    Math.max(0, Math.min(y + height, box[3]) - Math.max(y, box[1]));
  candidates.sort((a, b) => overlap(a) - overlap(b));
  // Keep the palette under the pointer while its own numeric input is captured.
  if (!(drag?.type === 'numeric' && drag.started && toolbar.contains(drag.capture))) {
    toolbar.style.left = candidates[0][0] + 'px';
    toolbar.style.top = candidates[0][1] + 'px';
  }
  for (const input of document.querySelectorAll(
    '[data-character-axis], [data-character-transform]',
  ))
    if (input !== document.activeElement && input !== drag?.capture) {
      const property = input.dataset.characterTransform,
        fallback = property === 'scale' ? 1 : 0;
      const values = context.selected.map((g) =>
          property ? g[property] : g.offset[Number(input.dataset.characterAxis)],
        ),
        mixed = values.some((v) => v !== values[0]);
      input.value = mixed ? '' : String(values[0] ?? fallback);
      input.placeholder = mixed ? 'Mixed' : String(fallback);
    }
}
function characterOverlay(result) {
  const context = characterContext(result);
  if (!context || current().locked) return '';
  return context.editable
    .map(
      (o, instance) =>
        `<g class="character-occurrence" data-character-route="${esc(o.route)}" transform="matrix(${o.matrix.join(' ')})">${context.layout.characters
          .map((g) => {
            const [x1, y1, x2, y2] = g.bounds,
              chosen = characterEdit.selected.has(g.id),
              hovered = characterEdit.hover === `${o.route}:${g.id}`;
            return `<rect class="character-hit ${chosen ? 'selected' : ''} ${hovered ? 'hovered' : ''} ${g.whitespace ? 'whitespace' : ''}" data-character="${g.id}" data-character-instance-index="${instance}" x="${x1}" y="${y1}" width="${Math.max(0.01, x2 - x1)}" height="${Math.max(0.01, y2 - y1)}"/><path class="character-outline ${chosen ? 'selected' : ''}" d="${esc(g.path)}"/>${chosen ? `<path class="character-base" data-character-pivot="${g.id}" d="M${g.pivot[0] + g.offset[0] + (g.arrangementOffset?.[0] || 0) - 3} ${g.pivot[1] + g.offset[1] + (g.arrangementOffset?.[1] || 0)}h6 M${g.pivot[0] + g.offset[0] + (g.arrangementOffset?.[0] || 0)} ${g.pivot[1] + g.offset[1] + (g.arrangementOffset?.[1] || 0) - 3}v6"/>` : ''}`;
          })
          .join('')}</g>`,
    )
    .join('');
}
function chooseCharacter(hit, additive = false) {
  characterEdit.route = hit.occurrence.route;
  if (additive) {
    if (characterEdit.selected.has(hit.glyph.id)) characterEdit.selected.delete(hit.glyph.id);
    else characterEdit.selected.add(hit.glyph.id);
  } else if (!characterEdit.selected.has(hit.glyph.id))
    characterEdit.selected = new Set([hit.glyph.id]);
}
function selectOverlappingCharacters(hits, additive) {
  const dialog = document.createElement('dialog');
  dialog.className = 'character-choice';
  dialog.innerHTML = `<h3>Choose overlapping character</h3><p>Each entry identifies a character and displayed instance.</p>${hits.map((hit, i) => `<button data-choice="${i}">${hit.glyph.sourceIndex + 1} · ${esc(hit.glyph.whitespace ? '[space]' : hit.glyph.text)} · Instance ${hit.instance + 1}</button>`).join('')}<button data-close-choice>Cancel</button>`;
  dialog.addEventListener('click', (e) => {
    const b = e.target.closest('button');
    if (!b) return;
    if (b.dataset.choice !== undefined && characterEdit)
      chooseCharacter(hits[Number(b.dataset.choice)], additive);
    closeEditorDialog(dialog);
    render();
    document.querySelector('.canvas-stage').focus({ preventScroll: true });
  });
  dialog.addEventListener('cancel', () => closeEditorDialog(dialog));
  showEditorDialog(dialog);
}

function objectResizeContext(n, result) {
  if (!n || n.pathAttachment || selection.size !== 1 || !selection.has(n.id)) return [];
  try {
    return objectResizeHandles(n.type, result.objectResizeState(n.id), canvasScale());
  } catch {
    return [];
  }
}
function objectResizeOverlay(n, result) {
  const handles = objectResizeContext(n, result);
  if (!handles.length) return '';
  return `<g class="object-resize-overlay" aria-label="Selected ${esc(nodeTypeLabel(n.type))} output before downstream nodes">${handles
    .map((h, index) => {
      const q = n.params[h.key],
        readonly = current().locked || q.locked || q.source || q.expression;
      return `<g class="object-resize-control ${readonly ? 'readonly' : ''}" data-object-resize="${index}" data-object-node="${n.id}" style="cursor:${readonly ? 'not-allowed' : h.axis ? 'ns-resize' : 'ew-resize'}" transform="translate(${h.point.join(' ')})"><title>${esc(h.label)} · selected ${esc(nodeTypeLabel(n.type))} output before downstream nodes${readonly ? ' · Locked or driven by an input' : ' · Drag to resize; position stays fixed'}</title><g transform="scale(${1 / canvasScale()})"><rect x="-6" y="-6" width="12" height="12"/><text x="12" y="4">${esc(h.label)} · selected ${esc(nodeTypeLabel(n.type))}</text></g></g>`;
    })
    .join('')}</g>`;
}

function transformContext(n, result) {
  if (n?.bypassed || n?.type !== 'Transform' || selection.size !== 1 || !selection.has(n.id))
    return null;
  const state = result.transformState(n.id);
  if (!state.input?.contours.some((c) => c.points.length)) return null;
  return {
    state,
    geometry: transformHandleGeometry(state, pathBounds(state.input), canvasScale()),
  };
}
function transformOverlay(n, result) {
  let context;
  try {
    context = transformContext(n, result);
  } catch {
    return '';
  }
  if (!context) return '';
  const { geometry: g } = context;
  const size = 1 / canvasScale();
  const anchorReadonly =
    current().locked ||
    ['anchor', 'position'].some((key) => {
      const q = n.params[key];
      return q && (q.locked || q.source || q.expression);
    });
  return `<g class="transform-overlay" data-transform-stage="${n.id}" aria-label="Selected Transform output before downstream nodes"><line x1="${g.pivot[0]}" y1="${g.pivot[1]}" x2="${g.rotation[0]}" y2="${g.rotation[1]}"/><g class="transform-control ${anchorReadonly ? 'readonly' : ''}" data-transform-handle="anchor" data-transform-node="${n.id}" aria-label="Reposition anchor" transform="translate(${g.pivot.join(' ')})"><title>${anchorReadonly ? 'Anchor or Position is locked or driven' : 'Drag Anchor without moving artwork'}</title><g transform="scale(${size})"><circle r="10" fill="transparent"/><text x="12" y="16">Anchor</text><path class="transform-pivot" d="M-7 0H7 M0-7V7"/><circle class="transform-pivot" r="3"/><text x="10" y="-12">Selected Transform output</text></g></g>${[
    'scale',
    'rotation',
  ]
    .map((key) => {
      const q = n.params[key],
        readonly =
          current().locked ||
          q.locked ||
          q.source ||
          q.expression ||
          (key === 'scale' && !context.state.uniformScale);
      return `<g class="transform-control ${readonly ? 'readonly' : ''}" data-transform-handle="${key}" data-transform-node="${n.id}" transform="translate(${g[key].join(' ')})" aria-label="${esc(nodeDisplayName(n))} ${key}${readonly ? ' (read-only)' : ''}"><title>${key === 'scale' ? 'Uniform scale · drag along pivot ray' : 'Rotation · Shift snaps to 15°'}${readonly ? ' · Locked or driven by an input' : ''}</title><g class="transform-marker-size" transform="scale(${size})">${key === 'scale' ? '<rect x="-6" y="-6" width="12" height="12"/>' : '<circle r="7"/>'}<text x="11" y="4">${key === 'scale' ? 'Scale' : 'Rotate'}</text></g></g>`;
    })
    .join('')}</g>`;
}

function directPointContext(n, doc = displayDocument()) {
  if (!n || !['direct', 'pen'].includes(tool)) return null;
  const fonts = resolveFonts(doc, activeFonts());
  if (penDraft?.resuming && n.id === penDraft.token.nodeId) {
    const matrix = penDraft.matrix;
    return {
      supported: true,
      matrix,
      path: transformDirectPath(evaluate(penDraft.layer, fonts, doc.imageAssets).run(n.id), matrix),
      nodeId: n.id,
    };
  }
  const result = directSelectionSource(doc, current().id, { sourceId: n.id, fonts });
  return tool === 'pen' && !result.supported ? null : result;
}
function canvasOverlay(n, result) {
  if (canvasTextEdit) return canvasTextOverlay(result);
  if (characterEdit) return characterOverlay(result);
  if (artworkTarget) {
    return [...artworkSelection]
      .map((id) => {
        const l = history.doc.layers.find((l) => l.id === id);
        if (!l || !layerShown(l)) return '';
        const r = id === activeLayer ? result : evaluate(l, fonts, history.doc.imageAssets);
        if (
          !r.path ||
          r.error ||
          (r.path.kind !== 'image' && !r.path.contours.some((c) => c.points.length))
        )
          return '';
        const b = pathBounds(r.path);
        return `<rect class="guide artwork-selection" data-artwork-selection="${esc(id)}" x="${b[0][0]}" y="${b[1][0]}" width="${b[0][2] - b[0][0]}" height="${b[1][2] - b[1][0]}" stroke-dasharray="5 4"/>`;
      })
      .join('');
  }
  if (!n || !layerShown(current()) || n.bypassed) return '';
  let path;
  try {
    const direct = directPointContext(n);
    if (direct && !direct.supported) return '';
    path = direct?.path || result.run(n.id);
  } catch {
    return '';
  }
  const controller = positionController(n);
  if (controller) {
    const { key, kind, label } = controller,
      q = n.params[key],
      readonly = current().locked || q.locked || q.source || q.expression;
    return `<g class="position-marker ${kind}-marker ${readonly ? 'readonly' : ''}" data-position-handle="${n.id}" data-${kind}-handle="${n.id}" transform="translate(${path[0]} ${path[1]})" aria-label="${esc(nodeDisplayName(n))} ${label} handle${readonly ? ' (read-only)' : ''}"><title>${esc(nodeDisplayName(n))} · ${label}${readonly ? ' · Locked or driven by an input' : ' · Drag to move; Alt bypasses snap'}</title><g class="position-marker-size ${kind}-marker-size">${kind === 'vector' ? '<polygon points="0,-12 12,0 0,12 -12,0"/><text x="16" y="-12">Vector XY</text>' : '<circle r="11"/>'}<path d="M-8 0H8 M0-8V8"/></g></g>`;
  }
  if (path?.kind === 'image')
    return (
      `<rect class="guide" x="${path.position[0]}" y="${path.position[1]}" width="${path.size[0]}" height="${path.size[1]}" stroke-dasharray="5 4"/>` +
      nodeStageMoveMarker(n, path)
    );
  if (n.type === 'Composition Reference' || (n.type === 'Transform' && n.paintMode))
    return (
      `<path class="guide" d="${esc(pathData(path))}" stroke-dasharray="5 4"/>` +
      nodeStageMoveMarker(n, path) +
      (n.type === 'Transform' ? transformOverlay(n, result) : '')
    );
  if (outputType(n) !== 'Path') return '';
  let out =
    tool === 'direct'
      ? `<path class="guide" d="${esc(pathData(path))}" stroke-dasharray="5 4"/>`
      : `<path class="node-stage-hit" data-node-stage-move="${n.id}" data-node-stage-layer="${current().id}" d="${esc(pathData(path))}"/><path class="guide" d="${esc(pathData(path))}" stroke-dasharray="5 4"/>` +
        nodeStageMoveMarker(n, path);
  const insertionPreview =
    drag?.type === 'path-insert'
      ? !drag.cancelled && drag.payload.nodeId === n.id
        ? drag
        : null
      : tool === 'pen' &&
          !penDraft &&
          penSegmentHover?.nodeId === n.id &&
          penSegmentHover.revision === session.state.expectedRevision
        ? penSegmentHover
        : null;
  if (insertionPreview) {
    const [x, y] = insertionPreview.position,
      r = 5 / canvasScale();
    out += `<g pointer-events="none" class="guide"><circle cx="${x}" cy="${y}" r="${r}"/><path d="M${x - r} ${y}H${x + r} M${x} ${y - r}V${y + r}"/></g>`;
  }
  if (['Curve', 'Multi Path', 'Path Remap'].includes(n.type)) {
    const scale = canvasScale(),
      focused = [
        ...selectedPointIds(n),
        ...[pointFocus, pointHover].filter((t) => t?.nodeId === n.id).map((t) => t.pointId),
      ];
    for (const target of pointControlTargets(path, scale, focused)) {
      const [x, y] = target.position,
        key = target.key,
        selected =
          selectedPointIds(n).includes(target.pointId) ||
          (pointFocus?.nodeId === n.id && pointFocus?.pointId === target.pointId);
      const attrs = `data-point="${target.pointId}"${key === 'position' ? '' : ` data-handle="${key}"`}`;
      if (key !== 'position') {
        const p = path.contours.flatMap((c) => c.points).find((p) => p.id === target.pointId);
        out += `<line class="guide" x1="${p.position[0]}" y1="${p.position[1]}" x2="${x}" y2="${y}"/>`;
      }
      out += `<circle class="point-hit" ${attrs} cx="${x}" cy="${y}" r="${target.radius / scale}"/><circle class="${key === 'position' ? 'vertex' : 'handle'} point-marker ${selected ? 'active' : ''}" ${attrs} cx="${x}" cy="${y}" r="${(key === 'position' ? 3 : 2.5) / scale}"><title>${key === 'position' ? 'Point' : key} · ${target.pointId}</title></circle>`;
    }
  }
  if (n.type === 'Multi Path') {
    const selectedIds = selectedPointIds(n);
    for (const [index, unit] of n.units.entries()) {
      if (!unitPointIds(n, unit).every((id) => selectedIds.includes(id))) continue;
      const contours = path.contours.filter((c) => unit.contours.includes(c.id));
      const bounds = pathBounds({ ...path, contours }),
        x = bounds[0][1],
        y = bounds[1][0] - 12 / canvasScale();
      if (![x, y].every(Number.isFinite)) continue;
      out += `<g class="node-stage-move" data-path-unit-move="${unit.id}" transform="translate(${x} ${y}) scale(${1 / canvasScale()})" aria-label="Move Path ${index + 1}"><title>Drag Path ${index + 1}, including its holes</title><rect x="-8" y="-8" width="16" height="16" rx="3"/><path d="M-5 0H5 M0-5V5"/></g>`;
    }
  }
  if (n.type === 'Transform') out += transformOverlay(n, result);
  out += objectResizeOverlay(n, result);
  return out;
}
function nodeStageMoveMarker(n, path) {
  if (penDraft || (path.kind !== 'image' && !path.contours?.some((c) => c.points.length)))
    return '';
  if (
    ![
      'Circle',
      'Ellipse',
      'Rectangle',
      'Polygon',
      'Star',
      'Text',
      'Curve',
      'Multi Path',
      'Transform',
      'Image',
      'Composition Reference',
    ].includes(n.type)
  )
    return '';
  const b = pathBounds(path),
    x = b[0][1],
    y = b[1][0] - 24 / canvasScale();
  if (![x, y].every(Number.isFinite)) return '';
  return `<g class="node-stage-move" data-node-stage-move="${n.id}" data-node-stage-layer="${current().id}" transform="translate(${x} ${y}) scale(${1 / canvasScale()})" aria-label="Move selected node before downstream nodes"><title>Move selected node before downstream nodes</title><rect x="-10" y="-10" width="20" height="20" rx="3"/><path d="M-6 0H6 M0-6V6"/><text x="14" y="4">Move node stage</text></g>`;
}
let penSegmentHover = null;
let pointFocus = null,
  pointHover = null,
  pointSelection = { nodeId: null, ids: new Set() };
function selectedPointIds(n) {
  return pointSelection.nodeId === n?.id ? [...pointSelection.ids] : [];
}
function selectPointTarget(n, target, event) {
  if (target.key !== 'position') {
    refreshPointPanels();
    return true;
  }
  if (pointSelection.nodeId !== n.id) pointSelection = { nodeId: n.id, ids: new Set() };
  if (event.shiftKey && pointSelection.ids.has(target.pointId)) {
    pointSelection.ids.delete(target.pointId);
    pointFocus = null;
    pointPanelViews.delete(n.id);
    refreshPointPanels();
    updateCanvas(true);
    return false;
  }
  if (!event.shiftKey && !pointSelection.ids.has(target.pointId)) pointSelection.ids.clear();
  pointSelection.ids.add(target.pointId);
  pointPanelViews.delete(n.id);
  // Keep the active point visible even when a large selection spans pages.
  const ordered =
    n.type === 'Path Remap'
      ? evaluate(current(), resolveFonts(history.doc, activeFonts()), history.doc.imageAssets)
          .run(n.params.path.source)
          .contours.flatMap((c) => c.points)
      : n.points;
  const index = ordered
    .filter((p) => pointSelection.ids.has(p.id))
    .findIndex((p) => p.id === target.pointId);
  pointPanelViews.set(n.id, {
    all: false,
    page: Math.max(0, Math.floor(index / POINT_PANEL_PAGE_SIZE)),
  });
  refreshPointPanels();
  updateCanvas(true);
  return true;
}
let canvasFrame = null;
function updateCanvas(immediate = false) {
  if (immediate) {
    if (canvasFrame !== null) window.cancelAnimationFrame(canvasFrame);
    drawCanvas();
  } else if (canvasFrame === null) canvasFrame = requestAnimationFrame(drawCanvas);
}
function drawCanvas() {
  canvasFrame = null;
  const svg = document.querySelector('.artboard');
  if (!svg) return;
  const display = displayDocument(),
    fonts = resolveFonts(display, { ...activeFonts(), ...(variablePreviewFonts || {}) }),
    l =
      penDraft?.layer ||
      display.layers.find((layer) => layer.id === activeLayer) ||
      display.layers[0];
  if (drag?.before) {
    try {
      assertClosedOffsetChange(
        l,
        drag.before.layers.find((x) => x.id === l.id),
        fonts,
        display.imageAssets,
        fonts,
        display,
        drag.before,
      );
    } catch (error) {
      cancelInteraction();
      notify(error.message, true);
      return;
    }
  }
  const r = evaluate(l, fonts, display.imageAssets);
  svg.innerHTML =
    gridOverlay() +
    canvasLayers(display)
      .filter((x) => layerShown(x))
      .map((x) => {
        const value = x === l ? r : evaluate(x, fonts, display.imageAssets);
        return value.path && !value.error ? layerShape(x, value.path) : '';
      })
      .join('') +
    guidesOverlay() +
    canvasOverlay(artworkTarget ? undefined : l.nodes.find((node) => node.id === selected), r);
  renderEvaluation = r;
  updateViewers(app, viewerState, display, fonts, new Map([[l.id, r]]));
  sizePositionHandle();
  positionCharacterFloating();
  positionCanvasTextEditor();
}
function sizePositionHandle() {
  document
    .querySelector('.position-marker-size')
    ?.setAttribute('transform', `scale(${1 / canvasScale()})`);
}
const CANVAS_LIMITS = [0.00001, 8],
  GRAPH_LIMITS = [0.05, 2];
function zoomLabel(scale) {
  return `${Number((scale * 100).toFixed(scale < 0.01 ? 3 : scale < 0.1 ? 1 : 0))}%`;
}
function updateNavigation() {
  const toolbar = document.querySelector('.graph-toolbar');
  if (toolbar && !toolbar.querySelector('.graph-pan-pad')) {
    const pad = document.createElement('span');
    pad.className = 'graph-pan-pad';
    pad.setAttribute('role', 'group');
    pad.setAttribute('aria-label', 'Pan node graph');
    for (const [action, label, glyph] of [
      ['graph-pan-up', 'Pan node graph up', '^'],
      ['graph-pan-left', 'Pan node graph left', '<'],
      ['graph-pan-right', 'Pan node graph right', '>'],
      ['graph-pan-down', 'Pan node graph down', 'v'],
    ]) {
      const button = document.createElement('button');
      button.type = 'button';
      button.dataset.action = action;
      button.setAttribute('aria-label', label);
      button.title = label;
      button.textContent = glyph;
      pad.append(button);
    }
    toolbar.querySelector('[data-action="graph-fit"]')?.before(pad);
  }
  updateGraphOverview();
  updateLayerDeletionControls();
  for (const [view, value, limits, prefix] of [
    ['canvas', zoom, CANVAS_LIMITS, ''],
    ['graph', graphZoom, GRAPH_LIMITS, 'graph-'],
  ]) {
    const output = document.querySelector(`[data-zoom="${view}"]`);
    if (output) {
      output.textContent = zoomLabel(value);
      output.title = `${view === 'canvas' ? 'Artboard' : 'Node graph'} actual scale · ${limits[0] * 100}–${limits[1] * 100}%`;
    }
    const minus = document.querySelector(`[data-action="${prefix}zoom-out"]`),
      plus = document.querySelector(`[data-action="${prefix}zoom-in"]`);
    if (minus) minus.disabled = value <= limits[0];
    if (plus) plus.disabled = value >= limits[1];
  }
  const fitSelection = document.querySelector('[data-action="graph-fit-selection"]');
  if (fitSelection) {
    fitSelection.disabled = !selection.size;
    fitSelection.title = selection.size
      ? 'Fit selected nodes (Shift+2)'
      : 'Select a node to fit selection';
  }
  for (const [selector, type] of [
    ['.canvas-stage', 'pan'],
    ['.graph-scroll', 'graph-pan'],
  ]) {
    const viewport = document.querySelector(selector);
    viewport?.classList.toggle('pan-ready', space && !navigationBusy());
    viewport?.classList.toggle('panning', drag?.type === type);
  }
}
function fitCanvas() {
  const stage = document.querySelector('.canvas-stage'),
    svg = document.querySelector('.artboard');
  if (!stage || !svg) return;
  // Fit follows viewport resize; manual zoom keeps its real scale and center world point.
  if (canvasFit && stage.clientWidth && stage.clientHeight)
    zoom = fitScale(history.doc, [stage.clientWidth, stage.clientHeight], 35, CANVAS_LIMITS).scale;
  const shownDoc = displayDocument();
  svg.setAttribute('viewBox', `0 0 ${shownDoc.width} ${shownDoc.height}`);
  const oldWidth = svg.style.width,
    oldHeight = svg.style.height;
  svg.style.width = shownDoc.width * zoom + 'px';
  svg.style.height = shownDoc.height * zoom + 'px';
  // Compare CSSOM-normalized values so float serialization cannot turn a pan into a redraw.
  const scaleChanged = svg.style.width !== oldWidth || svg.style.height !== oldHeight;
  svg.style.transform = `translate(${pan[0]}px,${pan[1]}px)`;
  sizePositionHandle();
  positionCharacterFloating();
  positionCanvasTextEditor();
  updateNavigation();
  // Keep screen-sized handles and their hit targets atomic with the new view scale.
  // Only the SVG changes; graph/input drafts and focus stay intact.
  if (selectedNode() && scaleChanged) updateCanvas(true);
}
function zoomCanvas(next, clientX, clientY) {
  const r = document.querySelector('.artboard').getBoundingClientRect();
  next = Math.max(CANVAS_LIMITS[0], Math.min(CANVAS_LIMITS[1], next));
  const center = [r.left + r.width / 2, r.top + r.height / 2];
  // A toolbar zoom anchors the viewport center, including the stage's unequal padding.
  const anchor =
    clientX === undefined ? pan.map((v) => -v) : [clientX - center[0], clientY - center[1]];
  pan = anchoredPan(pan, anchor, next / zoom);
  canvasFit = false;
  zoom = next;
  fitCanvas();
}
function fitArtboard() {
  canvasFit = true;
  pan = [0, 0];
  fitCanvas();
}
function drawWires() {
  updateGraphOverview();
  let graph = document.querySelector('.graph'),
    svg = document.querySelector('.wires');
  if (!graph) return;
  let rect = graph.getBoundingClientRect(),
    html = '';
  for (const n of current().nodes)
    for (const [key, q] of parameters(n)) {
      if (!q.source) continue;
      let from = graph.querySelector(
          `[data-source="${q.source}"]${q.sourcePort ? `[data-source-port="${q.sourcePort}"]` : ':not([data-source-port])'}`,
        ),
        to = [...graph.querySelectorAll('.field')]
          .find((el) => el.dataset.node === n.id && el.dataset.key === key)
          ?.querySelector('.input');
      if (!from || !to) continue;
      let a = from.getBoundingClientRect(),
        b = to.getBoundingClientRect(),
        x = (a.x + a.width / 2 - rect.x) / graphZoom,
        y = (a.y + a.height / 2 - rect.y) / graphZoom,
        xx = (b.x + b.width / 2 - rect.x) / graphZoom,
        yy = (b.y + b.height / 2 - rect.y) / graphZoom;
      html += `<path class="wire" d="M${x},${y} C${x + 80},${y} ${xx - 80},${yy} ${xx},${yy}"/>`;
    }
  if (wiring?.pointer) {
    const target = wiring.disconnect && current().nodes.find((n) => n.id === wiring.node);
    const q = target ? parameter(target, wiring.key) : null;
    const source = q?.source || wiring.source,
      port = q?.sourcePort || wiring.port;
    const from = graph.querySelector(
      `[data-source="${source}"]${port ? `[data-source-port="${port}"]` : ':not([data-source-port])'}`,
    );
    if (from) {
      const a = from.getBoundingClientRect(),
        x = (a.x + a.width / 2 - rect.x) / graphZoom,
        y = (a.y + a.height / 2 - rect.y) / graphZoom,
        xx = (wiring.pointer[0] - rect.x) / graphZoom,
        yy = (wiring.pointer[1] - rect.y) / graphZoom;
      html += `<path class="wire wire-preview" d="M${x},${y} C${x + 80},${y} ${xx - 80},${yy} ${xx},${yy}"/>`;
    }
  }
  svg.innerHTML = html;
}
app.addEventListener(
  'scroll',
  (e) => {
    if (e.target.matches('.graph-scroll')) {
      updateGraphOverview(false);
      if (wiring) drawWires();
    }
  },
  true,
);
function updateGraphBounds(requiredScroll = null) {
  const viewport = document.querySelector('.graph-scroll');
  requiredScroll ||= viewport ? [viewport.scrollLeft, viewport.scrollTop] : null;
  const graph = document.querySelector('.graph');
  if (!graph) return;
  const nodes = [...graph.querySelectorAll('.node')];
  graph.style.width =
    Math.max(
      2400,
      requiredScroll
        ? (Math.max(0, requiredScroll[0]) + document.querySelector('.graph-scroll').clientWidth) /
            graphZoom
        : 0,
      ...nodes.map((el) => el.offsetLeft + el.offsetWidth + 80),
    ) + 'px';
  graph.style.height =
    Math.max(
      1600,
      requiredScroll
        ? (Math.max(0, requiredScroll[1]) + document.querySelector('.graph-scroll').clientHeight) /
            graphZoom
        : 0,
      ...nodes.map((el) => el.offsetTop + el.offsetHeight + 100),
    ) + 'px';
}
function freePosition(type) {
  const measurements = {};
  for (const el of document.querySelectorAll('.graph .node'))
    measurements[el.dataset.id] = { width: el.offsetWidth, height: el.offsetHeight };
  return findFreeNodePosition(current().nodes, measurements, {
    width: 264,
    height: type === 'Curve' ? 520 : 260,
  });
}
// Source navigation is editor-local. It never dispatches a document command.
function sourceRoot() {
  const l = current();
  const id =
    artworkTarget && artworkSelection.size === 1
      ? l.output
      : !artworkTarget && selection.size === 1 && selection.has(selected)
        ? selected
        : null;
  const n = l.nodes.find((item) => item.id === id);
  return n && ['Path', 'PaintedPath'].includes(outputType(n))
    ? { documentEpoch: sourceEpoch, layerId: l.id, nodeId: n.id, port: undefined }
    : null;
}
function sameSourceRoot(a, b) {
  return (
    a &&
    b &&
    a.documentEpoch === b.documentEpoch &&
    a.layerId === b.layerId &&
    a.nodeId === b.nodeId &&
    a.port === b.port
  );
}
function sourceNavigationBusy() {
  // Conservatively require leaving an editor control explicitly, even for a clean value.
  // This runs on pointerdown, BEFORE a button could blur/commit that control.
  return !!(
    navigationBusy() ||
    penDraft ||
    nameEdit ||
    canvasTextEdit ||
    editorPicker ||
    variableAction ||
    session.hasGesture ||
    session.hasTextComposition ||
    layoutDraft ||
    gridOpacityDraft ||
    document.querySelector('dialog[open]') ||
    editingControl(document.activeElement) ||
    [...textDrafts.values()].some((draft) => draft.state.changed)
  );
}
function sourceNavigationPanel() {
  const root = sourceRoot();
  if (sourcePicker && !sameSourceRoot(sourcePicker.root, root)) sourcePicker = null;
  const n = selectedNode();
  let html =
    '<section class="source-navigation" aria-label="Source editing"><h4 data-source-heading tabindex="-1">Source editing</h4>';
  if (sourceReturn) {
    html += `<button data-source-nav="back">Back: ${esc(sourceReturn.label)}</button><p>Back restores selection and graph view, not edits.</p>`;
    if (n && ['Text', 'Curve'].includes(n.type)) {
      const uses = directGeometryUses(current(), n.id);
      html += `<p>Source: ${esc(n.type)} · ${esc(nodeDisplayName(n))}. Editing original-stage values; downstream effects may change the final result.</p><p>${uses.directInputs} saved direct geometry inputs (${uses.directNodes} nodes), same composition. Shared uses also follow edits. This is not a visible instance count.</p>${current().locked ? '<p>Locked composition · read-only source.</p>' : ''}`;
    }
  }
  if (!root) html += '<p>Select one artwork or shape-output node to find its source.</p>';
  else if (
    ['Text', 'Curve'].includes(current().nodes.find((item) => item.id === root.nodeId)?.type)
  )
    html += '<p>This node is already a source. Use its existing fields below.</p>';
  else html += '<button data-source-nav="open">Edit source…</button>';
  if (sourcePicker) {
    const result = sourcePicker.result;
    html += `<div role="group" aria-label="Shape-content source candidates"><p role="status">${result.candidates.length} distinct Text / Curve sources. ${result.complete ? 'Known geometry routes searched.' : 'Incomplete search: other routes are unconfirmed. Choose explicitly.'}</p>`;
    for (const candidate of result.candidates) {
      const route = candidate.route
        .map((edge) => {
          const owner = current().nodes.find((item) => item.id === edge.nodeId);
          return `${owner?.name || edge.nodeId} / ${edge.input}`;
        })
        .join(' ← ');
      html += `<p><button data-source-nav="enter" data-source-id="${esc(candidate.nodeId)}">${esc(candidate.type)} · ${esc(candidate.name)} · ${esc(candidate.detail)}</button><small> ID: ${esc(candidate.nodeId)} · ${esc(route)} · ${candidate.directInputs} saved direct geometry inputs (${candidate.directNodes} nodes)</small></p>`;
    }
    for (const diagnostic of result.diagnostics)
      html += `<p class="warning">${esc(diagnostic.reason)} · ${esc(diagnostic.nodeId)}</p>`;
    html += '<button data-source-nav="cancel">Cancel source search</button></div>';
  }
  return html + '</section>';
}
function focusSourceHeading() {
  app.querySelector('[data-source-heading]')?.focus({ preventScroll: true });
}
function clearSourceEditSelection() {
  characterEdit = null;
  pointSelection = { nodeId: null, ids: new Set() };
  pointFocus = null;
  pointHover = null;
  penSegmentHover = null;
  remapBatches.clear();
  selectedGuide = null;
  snapMatches = [];
}
function sourceNavigationAction(button) {
  if (sourceNavigationBusy()) {
    notify(
      'Finish or cancel the current field edit or interaction before source navigation.',
      true,
    );
    return;
  }
  const action = button.dataset.sourceNav;
  if (action === 'cancel') {
    sourcePicker = null;
    render();
    app.querySelector('[data-source-nav="open"]')?.focus({ preventScroll: true });
    return;
  }
  if (action === 'back') {
    const ticket = sourceReturn;
    sourceReturn = null;
    sourcePicker = null;
    if (!ticket || ticket.epoch !== sourceEpoch) return;
    const l = history.doc.layers.find((item) => item.id === ticket.ui.activeLayer);
    clearSourceEditSelection();
    if (!l) {
      selected = null;
      selection.clear();
      artworkTarget = null;
      artworkSelection.clear();
      render();
      focusSourceHeading();
      notify('Return composition no longer exists.', true);
      return;
    }
    activeLayer = l.id;
    selection = new Set([...ticket.ui.selection].filter((id) => l.nodes.some((n) => n.id === id)));
    selected = l.nodes.some((n) => n.id === ticket.ui.selected) ? ticket.ui.selected : null;
    artworkSelection = new Set(
      [...ticket.ui.artworkSelection].filter((id) =>
        history.doc.layers.some((item) => item.id === id),
      ),
    );
    artworkTarget = artworkSelection.has(ticket.ui.artworkTarget) ? ticket.ui.artworkTarget : null;
    // Conservative correspondence boundary: any committed edit invalidates old point/character scopes.
    if (ticket.doc === history.doc) {
      characterEdit = ticket.ui.characterEdit;
      selectedGuide = ticket.ui.selectedGuide;
      for (const [id, batch] of ticket.ui.remapBatches) remapBatches.set(id, batch);
      pointSelection = ticket.points;
      pointFocus = ticket.pointFocus;
    }
    graphZoom = ticket.graphZoom;
    render();
    const viewport = app.querySelector('.graph-scroll');
    const scroll = ticket.center.map((value, axis) =>
      Math.max(0, value * graphZoom - (axis ? viewport.clientHeight : viewport.clientWidth) / 2),
    );
    updateGraphBounds(scroll);
    viewport.scrollLeft = scroll[0];
    viewport.scrollTop = scroll[1];
    app.querySelector('.inspector').scrollTop = ticket.inspectorScroll;
    focusSourceHeading();
    notify(
      selected || artworkTarget
        ? 'Returned. Source edits are retained.'
        : 'Returned; original selection no longer exists.',
    );
    return;
  }
  const root = sourceRoot();
  if (!root) return;
  if (action === 'open') {
    sourcePicker = {
      root,
      doc: history.doc,
      result: queryShapeSources(history.doc, root, session.state.expectedRevision),
    };
    render();
    return;
  }
  if (action !== 'enter' || !sourcePicker || !sameSourceRoot(root, sourcePicker.root)) return;
  if (
    sourcePicker.doc !== history.doc ||
    sourcePicker.result.revision !== session.state.expectedRevision
  ) {
    sourcePicker = {
      root,
      doc: history.doc,
      result: queryShapeSources(history.doc, root, session.state.expectedRevision),
    };
    render();
    notify('Sources changed. Review the refreshed list and choose again.');
    return;
  }
  const fresh = queryShapeSources(history.doc, root, session.state.expectedRevision);
  const candidate = fresh.candidates.find((item) => item.nodeId === button.dataset.sourceId);
  if (!candidate) {
    sourcePicker.result = fresh;
    render();
    notify('Source is no longer available.', true);
    return;
  }
  if (!sourceReturn) {
    const viewport = app.querySelector('.graph-scroll');
    sourceReturn = {
      epoch: sourceEpoch,
      doc: history.doc,
      ui: selectionSnapshot(true),
      points: structuredClone(pointSelection),
      pointFocus: structuredClone(pointFocus),
      graphZoom,
      center: [
        (viewport.scrollLeft + viewport.clientWidth / 2) / graphZoom,
        (viewport.scrollTop + viewport.clientHeight / 2) / graphZoom,
      ],
      inspectorScroll: app.querySelector('.inspector').scrollTop,
      label: artworkTarget
        ? 'Artwork'
        : nodeDisplayName(current().nodes.find((item) => item.id === root.nodeId)),
    };
  }
  sourcePicker = null;
  selected = candidate.nodeId;
  selection = new Set([selected]);
  artworkTarget = null;
  artworkSelection = new Set();
  clearSourceEditSelection();
  render();
  app.querySelector('.inspector').scrollTop = 0;
  focusSourceHeading();
  // Do not expand authored collapsed cards, change Output, or bind a Viewer.
  const destination = selected,
    epoch = sourceEpoch;
  requestAnimationFrame(() => {
    if (epoch !== sourceEpoch || selected !== destination) return;
    [...app.querySelectorAll('.node')]
      .find((el) => el.dataset.id === destination)
      ?.scrollIntoView({ block: 'nearest', inline: 'nearest' });
  });
}
app.addEventListener(
  'pointerdown',
  (event) => {
    if (!event.target.closest?.('[data-source-nav]')) return;
    if (sourceNavigationBusy()) {
      event.preventDefault();
      event.stopImmediatePropagation();
      notify(
        'Finish or cancel the current field edit or interaction before source navigation.',
        true,
      );
    }
  },
  true,
);

app.addEventListener(
  'keydown',
  (event) => {
    if (
      event.key !== 'Escape' ||
      event.isComposing ||
      composing ||
      !sourcePicker ||
      !event.target.closest?.('.source-navigation')
    )
      return;
    event.preventDefault();
    event.stopImmediatePropagation();
    const cancel = app.querySelector('[data-source-nav="cancel"]');
    if (cancel) sourceNavigationAction(cancel);
  },
  true,
);

function revealNode(id) {
  requestAnimationFrame(() =>
    document
      .querySelector(`.node[data-id="${id}"]`)
      ?.scrollIntoView({ block: 'nearest', inline: 'nearest' }),
  );
}
function addNode(type, envelope = session.state, layerId = current().id) {
  const result = commandChange(
    'create-node',
    { layerId, nodeType: type, position: freePosition(type) },
    `Added ${type}`,
    true,
    envelope,
  );
  if (result) {
    selected = result.nodeId;
    selection = new Set([selected]);
    artworkTarget = null;
    artworkSelection = new Set();
    render();
    revealNode(selected);
  }
  return result;
}

function openNodePicker() {
  if (current().locked) {
    notify('Composition is locked. Unlock it or select another Composition.', true);
    return;
  }
  if (
    editorPicker ||
    document.querySelector('dialog[open]') ||
    navigationBusy() ||
    penDraft ||
    nameEdit
  )
    return;
  const envelope = session.state,
    layerId = current().id;
  editorPicker = openSearchPanel({
    title: 'ノードを追加',
    action: '追加',
    context: `作成先: ${current().name}`,
    categories: nodePickerItems(definitions).some((item) => item.categoryId === 'uncategorized')
      ? [...nodeCategories, { id: 'uncategorized', label: '未分類' }]
      : nodeCategories,
    items: nodePickerItems(definitions, nodeTypeLabel),
    filterItems: searchNodeItems,
    fallbackFocus: () => document.querySelector('.graph-scroll')?.focus({ preventScroll: true }),
    onCommit(item) {
      if (!item || !editorPicker || editorPicker.closed) return;
      const picker = editorPicker;
      if (
        current().id !== layerId ||
        current().locked ||
        penDraft ||
        navigationBusy() ||
        nameEdit
      ) {
        picker.status(
          '作成先または編集状態が変わりました。閉じてから再度追加してください。',
          false,
        );
        return;
      }
      editorPicker = null;
      picker.close();
      if (addNode(item.id, envelope, layerId))
        document.querySelector('.graph-scroll')?.focus({ preventScroll: true });
    },
    onCancel() {
      editorPicker = null;
    },
  });
}

const FONT_FAVORITES_KEY = 'nect.font-favorites.v1';
function fontFavorites() {
  try {
    const value = JSON.parse(localStorage.getItem(FONT_FAVORITES_KEY) || '[]');
    return new Set(Array.isArray(value) ? value.filter((item) => typeof item === 'string') : []);
  } catch {
    return new Set();
  }
}
function openFontPicker(control, query = '') {
  if (
    editorPicker ||
    document.querySelector('dialog[open]') ||
    current().locked ||
    navigationBusy() ||
    penDraft
  )
    return;
  document.activeElement?.blur();
  const context = control.dataset.characterStyle === 'font' ? characterContext() : null;
  const nodeId = context?.n.id || control.dataset.font;
  if (!nodeId || (context && (!context.typographySelected.length || context.typographyReason)))
    return;
  if (variableNode(nodeId)) {
    if (context) {
      notify('Variable Text currently supports whole-text font selection only', true);
      return;
    }
    editorPicker = openSearchPanel({
      title: 'Font',
      action: 'Apply font',
      selectedId: current().nodes.find((n) => n.id === nodeId)?.font,
      query,
      items: [
        { id: 'default', label: 'DejaVu Sans (Latin)' },
        ...Object.entries(history.doc.fontAssets || {}).map(([id, asset]) => ({
          id,
          label: asset.name,
        })),
      ],
      onCommit(item) {
        if (!item) return;
        const picker = editorPicker;
        editorPicker = null;
        picker.close();
        void preparedChange(
          'set-variable-font',
          {
            nodeId,
            fontId: item.id,
            axes: history.doc.fontAssets?.[item.id]?.kind === 'variable-glyf-v1' ? {} : null,
          },
          'Font updated',
        );
      },
      onCancel() {
        editorPicker = null;
      },
    });
    return;
  }
  const owner = session,
    envelope = session.state;
  let token;
  try {
    token = owner.dispatch({
      ...envelope,
      type: 'begin-gesture',
      payload: {
        target: {
          kind: 'text-font',
          layerId: current().id,
          nodeId,
          ...(context ? { characterIds: [...characterEdit.selected] } : {}),
        },
      },
    });
  } catch (error) {
    notify(error.message, true);
    return;
  }
  const items = [
    ...(context ? [{ id: '', label: 'Inherit default font', key: 'inherit' }] : []),
    {
      id: 'default',
      label: 'DejaVu Sans (Latin)',
      key: 'default',
      metadata: bundledFont?.nectMetadata,
    },
    ...Object.entries(history.doc.fontAssets || {}).map(([id, asset]) => ({
      id,
      label: asset.name,
      key: `asset:${id}`,
      legacyKey: `font:${asset.name}`,
      metadata: fonts[id]?.nectMetadata,
      variable: asset.kind === 'variable-glyf-v1',
      disabled: !!context && asset.kind === 'variable-glyf-v1',
      detail:
        asset.kind === 'variable-glyf-v1'
          ? 'Experimental variable font · whole-text only · prepared on Apply'
          : '',
    })),
    ...systemFonts.map(([id, name, reason, metadata]) => ({
      id,
      label: name,
      key: `installed:${id}`,
      legacyKey: `font:${name}`,
      metadata,
      detail: reason,
      disabled: !!reason,
      installed: true,
    })),
  ];
  // Names describe a face; only its opaque ID chooses the bytes. Keep duplicate
  // names as separate rows, including separate favorite controls.
  for (const item of items) {
    if (item.id === '') continue;
    const m = item.metadata;
    const style = m?.subfamily || 'Unknown style';
    const weight = m?.weightClass == null ? 'weight unknown' : `weight ${m.weightClass}`;
    const family = m?.family || 'Unknown family';
    item.detail = `${family} · ${style} · ${weight}${item.detail ? ` · ${item.detail}` : ''}`;
  }
  const duplicateNames = new Set(
    items
      .filter((item, i) => items.findIndex((other) => other.label === item.label) !== i)
      .map((item) => item.label),
  );
  for (const item of items) if (duplicateNames.has(item.label)) item.detail += ` · face ${item.id}`;
  const favorites = fontFavorites();
  // Migrate name-based favorites to the currently visible IDs without merging
  // same-name faces when the user subsequently toggles one of them.
  for (const item of items)
    if (item.legacyKey && favorites.has(item.legacyKey)) favorites.add(item.key);
  for (const item of items) if (item.legacyKey) favorites.delete(item.legacyKey);
  let picker,
    request = 0,
    pending = Promise.resolve(),
    readyId,
    committing = false;
  async function preview(item) {
    const serial = ++request;
    readyId = undefined;
    if (!item) return;
    if (item.variable) {
      // Variable geometry cannot enter the synchronous static font gesture.
      // Leave the active bank intact; prepare the whole-text switch on Apply.
      fontPreview = null;
      readyId = item.id;
      updateCanvas(true);
      picker.status('Experimental variable font · prepared on Apply');
      return;
    }
    picker.status(item.installed ? 'Loading font…' : '', false);
    try {
      const update = item.installed
        ? { bytes: await readSystemFont(item.id), fileName: item.label }
        : { font: item.id };
      if (picker.closed || serial !== request || session !== owner) return;
      token = owner.dispatch({
        ...envelope,
        type: 'update-gesture',
        payload: { ...token, update },
      });
      fontPreview = owner.previewSnapshot(token.gestureId).document;
      readyId = item.id;
      updateCanvas(true);
      picker.status('');
    } catch (error) {
      if (!picker.closed && serial === request)
        picker.status(error.message || String(error), false);
    }
  }
  picker = editorPicker = openSearchPanel({
    title: 'Font',
    fontFilters: true,
    action: 'Apply font',
    items,
    query,
    selectedId: control.value,
    isFavorite: (item) => favorites.has(item.key),
    toggleFavorite(item) {
      if (favorites.has(item.key)) favorites.delete(item.key);
      else favorites.add(item.key);
      try {
        localStorage.setItem(FONT_FAVORITES_KEY, JSON.stringify([...favorites]));
      } catch {
        picker.status('Favorites could not be saved', false);
      }
    },
    onSelect(item) {
      pending = preview(item);
    },
    async onCommit(item) {
      if (!item || committing) return;
      committing = true;
      const serial = request;
      try {
        await pending;
        if (picker.closed || request !== serial || readyId !== item.id || session !== owner) return;
        if (item.variable) {
          owner.dispatch({ ...envelope, type: 'cancel-gesture', payload: token });
          fontPreview = null;
          editorPicker = null;
          picker.close();
          await preparedChange(
            'set-variable-font',
            { nodeId, fontId: item.id, axes: {} },
            'Font updated',
            envelope,
          );
          return;
        }
        owner.dispatch({ ...envelope, type: 'commit-gesture', payload: token });
        fontPreview = null;
        editorPicker = null;
        picker.close();
        notify('Font updated');
        render();
      } catch (error) {
        picker.status(error.message || String(error), false);
      } finally {
        committing = false;
      }
    },
    onCancel() {
      request++;
      if (owner.hasGesture && owner.gestureState?.gestureId === token.gestureId)
        owner.dispatch({ ...owner.state, type: 'cancel-gesture', payload: token });
      fontPreview = null;
      editorPicker = null;
      updateCanvas(true);
    },
  });
}

// Retain the standard controls for accessibility; route normal pointer/keyboard
// browsing to a searchable picker with an explicit commit/cancel boundary.
app.addEventListener(
  'pointerdown',
  (event) => {
    const control = event.target.closest?.('#add-node, [data-font], [data-character-style="font"]');
    if (!control || event.button !== 0 || control.disabled) return;
    event.preventDefault();
    event.stopImmediatePropagation();
    control.id === 'add-node' ? openNodePicker() : openFontPicker(control);
  },
  true,
);
app.addEventListener(
  'keydown',
  (event) => {
    if (event.isComposing || event.keyCode === 229 || composing) return;
    const control = event.target.closest?.('#add-node, [data-font], [data-character-style="font"]');
    if (!control || control.disabled || event.ctrlKey || event.metaKey || event.altKey) return;
    if (!['ArrowDown', 'ArrowUp', 'Enter', ' '].includes(event.key) && event.key.length !== 1)
      return;
    event.preventDefault();
    event.stopImmediatePropagation();
    control.id === 'add-node'
      ? openNodePicker()
      : openFontPicker(control, event.key.length === 1 && event.key !== ' ' ? event.key : '');
  },
  true,
);
document.addEventListener(
  'keydown',
  (event) => {
    if (
      !(event.ctrlKey && event.code === 'Space') ||
      event.altKey ||
      event.shiftKey ||
      event.metaKey ||
      event.repeat ||
      event.isComposing ||
      event.keyCode === 229 ||
      composing ||
      editingControl(event.target) ||
      event.target.closest('dialog')
    )
      return;
    event.preventDefault();
    event.stopImmediatePropagation();
    openNodePicker();
  },
  true,
);
function deletionBusy() {
  return !!(penDraft || navigationBusy() || nameEdit || document.querySelector('dialog[open]'));
}
function alignmentBusy() {
  return deletionBusy() || tool !== 'select' || space;
}
function updateLayerDeletionControls() {
  const busy = deletionBusy();
  for (const button of document.querySelectorAll(
    '[data-align-artwork], [data-distribute-artwork], [data-distribute-gap]',
  ))
    button.disabled = alignmentBusy() || button.dataset.eligible !== 'true';
  for (const button of document.querySelectorAll('[data-order-layer]')) {
    const target = resolveArtworkOrderTarget(
      history.doc,
      button.dataset.orderLayer,
      button.dataset.orderDirection,
    );
    // Boundary controls stay focusable so a keyboard user keeps the same exact
    // row/direction after a swap. The handler enforces aria-disabled as well.
    button.disabled = busy || (!target.supported && target.code !== 'boundary');
    button.setAttribute('aria-disabled', String(busy || !target.supported));
    button.title = busy
      ? 'Finish or cancel the current edit before ordering a layer'
      : target.supported
        ? `Move this layer ${button.dataset.orderDirection} one paint position (Undo available)`
        : target.reason;
  }
  for (const button of document.querySelectorAll('[data-delete-layer]')) {
    const target = history.doc.layers.find((layer) => layer.id === button.dataset.deleteLayer);
    button.disabled = busy || !target || target.locked;
    button.title = busy
      ? 'Finish or cancel the current edit before deleting a layer'
      : target?.locked
        ? 'Unlock this layer before deleting it'
        : 'Delete this entire layer and its graph (Undo available)';
  }
  for (const button of document.querySelectorAll('[data-duplicate-artwork]')) {
    button.disabled =
      busy ||
      button.dataset.duplicateArtwork !== artworkTarget ||
      button.dataset.duplicateSelection !== JSON.stringify([...artworkSelection]) ||
      button.dataset.eligible !== 'true';
    if (busy) button.title = 'Finish or cancel the current edit before duplicating artwork';
    else button.title = button.dataset.availableTitle;
  }
}
// Keep modal availability tied to editor lifecycle, independent of DOM observers.
function closeEditorDialog(dialog) {
  dialog?.remove();
  updateLayerDeletionControls();
}
function showEditorDialog(dialog) {
  document.body.append(dialog);
  // Native Escape/close clears `open` before this event; explicit buttons remove directly.
  dialog.addEventListener('close', () => closeEditorDialog(dialog));
  dialog.showModal();
  updateLayerDeletionControls();
}
function deleteArtwork(layerId) {
  // Reject before taking a history snapshot or touching any detached draft.
  if (deletionBusy()) return;
  try {
    const target = history.doc.layers.find((layer) => layer.id === layerId);
    if (!target) throw Error('Layer is unavailable; select it again');
    if (target.locked) throw Error('Composition is locked');
    const priorUI = selectionSnapshot(true);
    const deleted = session.dispatch({
      ...session.state,
      type: 'delete-layer',
      payload: { layerId },
    });
    resetTransient();
    activeLayer = deleted.nextLayerId;
    selected = null;
    artworkTarget = null;
    artworkSelection = new Set();
    selection = new Set();
    tool = 'select';
    selectionHistory.set(history.past.at(-1), priorUI);
    selectionHistory.set(history.doc, selectionSnapshot(true));
    notify(
      deleted.replacedLastLayer ? 'Layer deleted · empty composition retained' : 'Layer deleted',
    );
    render();
    document.querySelector('.canvas-stage').focus({ preventScroll: true });
  } catch (error) {
    notify(error.message, true);
  }
}
function duplicateArtwork(layerId, selectionKey) {
  // A stale/forced click must not finish a draft, cancel a gesture, or duplicate
  // whichever layer happens to be active after the original button was rendered.
  if (
    deletionBusy() ||
    !artworkSelection.size ||
    (selectionKey === undefined
      ? artworkSelection.size !== 1
      : selectionKey !== JSON.stringify([...artworkSelection])) ||
    artworkTarget !== layerId ||
    activeLayer !== layerId
  )
    return;
  try {
    const priorUI = selectionSnapshot(true);
    const { copies: created } = session.dispatch({
      ...session.state,
      type: 'duplicate-artworks',
      payload: { layerIds: [...artworkSelection] },
    });
    const nextActive = created.find((copy) => copy.sourceLayerId === layerId).layerId;
    artworkSelection = new Set(created.map((copy) => copy.layerId));
    selectArtwork(nextActive, { preserve: true });
    tool = 'select';
    selectionHistory.set(history.past.at(-1), priorUI);
    selectionHistory.set(history.doc, selectionSnapshot(true));
    notify('Duplicated independent artwork in place');
    render();
    document.querySelector('.canvas-stage').focus({ preventScroll: true });
  } catch (error) {
    notify(error.message, true);
  }
}
function duplicateFocusedLayer(layerId) {
  if (deletionBusy()) return;
  try {
    const priorUI = selectionSnapshot(true);
    const created = session.dispatch({
      ...session.state,
      type: 'duplicate-layer',
      payload: { layerId },
    });
    activeLayer = created.layerId;
    artworkTarget = created.layerId;
    artworkSelection = new Set([created.layerId]);
    selectionHistory.set(history.past.at(-1), priorUI);
    selectionHistory.set(history.doc, selectionSnapshot(true));
    notify('Layer duplicated');
    render();
  } catch (error) {
    notify(error.message, true);
  }
}
function alignArtwork(button) {
  const targets = alignmentTargets();
  // Reject stale or forced controls before the generic click path can finish Pen.
  if (
    !app.contains(button) ||
    alignmentBusy() ||
    button.dataset.alignActive !== activeLayer ||
    button.dataset.alignReference !== alignmentPreferences.reference ||
    button.dataset.alignSelection !== JSON.stringify(targets.ids)
  )
    return;
  try {
    const priorUI = selectionSnapshot(true),
      oldPast = history.past.at(-1);
    const { changed } = session.dispatch({
      ...session.state,
      type: targets.scope === 'nodes' ? 'align-node-outputs' : 'align-artworks',
      payload: {
        ...(targets.scope === 'nodes'
          ? { layerId: activeLayer, nodeIds: targets.ids }
          : { layerIds: targets.ids }),
        action: button.dataset.alignArtwork,
        reference: alignmentPreferences.reference,
      },
    });
    if (history.past.at(-1) !== oldPast) {
      selectionHistory.set(history.past.at(-1), priorUI);
      selectionHistory.set(history.doc, selectionSnapshot(true));
    }
    notify(
      changed ? `Artwork aligned to ${alignmentPreferences.reference}` : 'Artwork already aligned',
    );
    render();
  } catch (error) {
    notify(error.message, true);
  }
}
function distributeArtwork(button) {
  const targets = alignmentTargets();
  if (targets.scope === 'nodes') return;
  if (
    !app.contains(button) ||
    alignmentBusy() ||
    button.dataset.distributeActive !== activeLayer ||
    (button.dataset.distributeGap !== undefined &&
      button.dataset.distributeSpacing !== String(alignmentPreferences.spacing ?? '')) ||
    button.dataset.distributeSelection !== JSON.stringify(targets.ids)
  )
    return;
  try {
    const priorUI = selectionSnapshot(true),
      oldPast = history.past.at(-1);
    const action = button.dataset.distributeArtwork ?? button.dataset.distributeGap;
    const { changed } = session.dispatch({
      ...session.state,
      type:
        button.dataset.distributeGap !== undefined
          ? 'distribute-artwork-gaps'
          : 'distribute-artworks',
      payload: {
        layerIds: targets.ids,
        action,
        ...(button.dataset.distributeGap !== undefined && alignmentPreferences.spacing !== null
          ? { spacing: alignmentPreferences.spacing }
          : {}),
      },
    });
    if (history.past.at(-1) !== oldPast) {
      selectionHistory.set(history.past.at(-1), priorUI);
      selectionHistory.set(history.doc, selectionSnapshot(true));
    }
    const subject = button.dataset.distributeGap !== undefined ? 'gaps' : 'centers';
    notify(
      changed
        ? `Artwork ${subject} distributed equally`
        : `Artwork ${subject} already equally spaced`,
    );
    render();
  } catch (error) {
    notify(error.message, true);
  }
}
function orderArtwork(layerId, direction) {
  // This guard runs before the generic click handler can auto-finish a Pen
  // draft. Resolve the exact ID afresh even for a stale/forced DOM event.
  if (deletionBusy()) return;
  const target = resolveArtworkOrderTarget(history.doc, layerId, direction);
  if (!target.supported) return;
  const priorUI = selectionSnapshot(true);
  session.dispatch({ ...session.state, type: 'order-layer', payload: { layerId, direction } });
  selectionHistory.set(history.past.at(-1), priorUI);
  selectionHistory.set(history.doc, selectionSnapshot(true));
  notify(`Layer moved ${direction} one position`);
  render();
}
function createArtwork(
  type,
  { at = [history.doc.width / 2, history.doc.height / 2], writingMode } = {},
) {
  try {
    const priorUI = selectionSnapshot();
    const created = session.dispatch({
      ...session.state,
      type: 'create-artwork',
      payload: { artworkType: type, at, ...(writingMode ? { writingMode } : {}) },
    });
    selectArtwork(created.layerId);
    selectionHistory.set(history.past.at(-1), priorUI);
    selectionHistory.set(history.doc, selectionSnapshot());
    notify(`Added independent ${type} artwork`);
    render();
    revealNode(created.nodeId);
    document.querySelector('.canvas-stage').focus({ preventScroll: true });
  } catch (error) {
    notify(error.message, true);
  }
}
// Pen previews stay detached from history.doc. Only Finish appends a layer.
function restorePenSelection(state) {
  ({ activeLayer, selected, artworkTarget } = state);
  artworkSelection = new Set(state.artworkSelection);
  selection = new Set(state.selection);
  pointSelection = state.pointSelection
    ? structuredClone(state.pointSelection)
    : { nodeId: null, ids: new Set() };
  pointFocus = state.pointFocus ? { ...state.pointFocus } : null;
}
function releasePenPointer() {
  if (drag?.type !== 'pen') return;
  const old = drag;
  drag = null;
  if (old.capture?.hasPointerCapture?.(old.pointerId))
    old.capture.releasePointerCapture(old.pointerId);
  pointerActive = false;
  snapMatches = [];
}
function discardPenDraft() {
  if (!penDraft) return;
  releasePenPointer();
  const prior = penDraft.priorUI;
  session.dispatch({
    ...session.state,
    type: 'cancel-gesture',
    payload: { gestureId: penDraft.token.gestureId },
  });
  penDraft = null;
  penId = null;
  restorePenSelection(prior);
}
function penCommand(type, payload = {}) {
  const result = session.dispatch({
    ...session.state,
    type,
    payload: { ...penDraft.token, ...payload },
  });
  if (!result.discarded && session.hasGesture) {
    penDraft.token = result;
    penDraft.layer = session
      .previewSnapshot(result.gestureId)
      .document.layers.find((l) => l.id === result.layerId);
  }
  return result;
}
const penSource = (draft = penDraft) =>
  draft?.layer.nodes.find((n) => n.id === (draft.token.nodeId ?? draft.layer.nodes[0].id));
function startPenContinuation(n, pointId, contourId) {
  const priorUI = selectionSnapshot(true),
    context = directPointContext(n);
  if (context?.locked) throw Error('Path route is locked');
  const result = session.dispatch({
    ...session.state,
    type: 'begin-gesture',
    payload: {
      target: {
        kind: 'pen',
        layerId: current().id,
        nodeId: n.id,
        endpointId: pointId,
        ...(contourId ? { contourId } : {}),
      },
    },
  });
  const layer = session
    .previewSnapshot(result.gestureId)
    .document.layers.find((l) => l.id === result.layerId);
  penDraft = {
    layer,
    priorUI,
    token: result,
    resuming: true,
    matrix: context?.matrix || [1, 0, 0, 1, 0, 0],
    closingPointId: result.closingPointId,
  };
  penId = result.nodeId;
  activeLayer = layer.id;
  selected = penId;
  selection = new Set([selected]);
  artworkTarget = null;
  artworkSelection = new Set();
  render();
  document.querySelector('.canvas-stage')?.focus({ preventScroll: true });
  notify('Continue path · click to add · Enter to finish · Ctrl+Z to cancel');
}
function finishPenDraft(closed = false, redraw = true) {
  // A key or tool change during a pressed point never commits its partial handles.
  if (drag?.type === 'pen') cancelInteraction();
  const draft = penDraft;
  if (!draft) return true;
  const curve = penSource(draft);
  if (!draft.resuming && curve.points.length < 2) {
    discardPenDraft();
    tool = 'select';
    notify('One-point draft discarded; a path needs two points');
  } else {
    try {
      const created = penCommand('commit-pen', { closed });
      penDraft = null;
      penId = null;
      tool = 'select';
      if (draft.resuming) {
        activeLayer = created.layerId;
        selected = created.nodeId;
        selection = new Set([selected]);
        artworkTarget = null;
        artworkSelection = new Set();
        if (
          directSelectionSource(history.doc, activeLayer, {
            sourceId: selected,
            fonts: resolveFonts(history.doc, activeFonts()),
          }).supported
        )
          tool = 'direct';
      } else selectArtwork(created.layerId);
      if (created.changed) {
        selectionHistory.set(history.past.at(-1), draft.priorUI);
        selectionHistory.set(history.doc, selectionSnapshot(true));
      }
      notify(
        draft.resuming
          ? created.changed
            ? 'Path extended'
            : 'Continuation ended without changes'
          : `Added independent ${closed ? 'closed' : 'open'} Curve artwork`,
      );
    } catch (error) {
      notify(error.message, true);
      if (redraw) render();
      return false;
    }
  }
  if (redraw) {
    render();
    document.querySelector('.canvas-stage').focus({ preventScroll: true });
  }
  return true;
}

async function confirmReplace(action = 'opening another document') {
  const accepted = await documents.guard(action);
  updateDocumentStatus();
  return accepted;
}
async function openCompositionImportDialog() {
  if (variableAction) throw Error('Finish or cancel variable font preparation first');
  if (documents.hasDraft()) throw Error('Finish or cancel the active edit first');
  const owner = session,
    revision = session.state.expectedRevision,
    generation = documents.generation;
  const opened = desktop
    ? await openFile()
    : await new Promise((resolve, reject) => {
        const input = document.createElement('input');
        input.type = 'file';
        input.accept = '.json,application/json';
        input.hidden = true;
        document.body.append(input);
        input.onchange = async () => {
          try {
            const file = input.files?.[0];
            if (file && file.size > MAX_PROJECT_BYTES) throw Error('Project exceeds 50 MB');
            resolve(file ? { name: file.name, contents: await file.text() } : null);
          } catch (error) {
            reject(error);
          } finally {
            input.remove();
          }
        };
        input.click();
      });
  if (!opened) return;
  let source;
  try {
    if (new TextEncoder().encode(opened.contents).byteLength > MAX_PROJECT_BYTES)
      throw Error('Project exceeds 50 MB');
    source = deserializeProject(opened.contents);
  } catch (error) {
    notify('Composition import rejected; current document preserved: ' + error.message, true);
    return;
  } finally {
    await documents.releaseUnused(opened.binding);
  }
  const dialog = document.createElement('dialog');
  dialog.innerHTML =
    '<form method="dialog"><h3>Project compositions</h3><label>Compositions to add <select data-composition-choice></select></label><output role="status"></output><footer><button value="cancel">Cancel</button><button value="import">Import</button></footer></form>';
  const select = dialog.querySelector('[data-composition-choice]');
  const status = dialog.querySelector('output');
  const allOption = document.createElement('option');
  allOption.value = '__all__';
  allOption.textContent = 'All Compositions (' + source.compositions.length + ')';
  select.append(allOption);
  for (const composition of source.compositions) {
    const option = document.createElement('option');
    option.value = composition.id;
    option.textContent = composition.name + ' - ' + composition.width + ' x ' + composition.height;
    select.append(option);
  }
  status.textContent =
    source.compositions.length +
    ' Compositions in ' +
    source.name +
    '; All adds the project set in one Undo step; choose one to add only that Composition.';
  dialog.addEventListener('close', () => {
    const value = dialog.returnValue;
    dialog.remove();
    if (value !== 'import' || !select.value) return;
    try {
      if (
        session !== owner ||
        session.state.expectedRevision !== revision ||
        documents.generation !== generation ||
        documents.hasDraft() ||
        variableAction
      )
        throw Error('Project changed while reading; import the file again');
      saveCompositionViewState();
      const imported = session.dispatch({
        ...session.state,
        type: 'import-project-compositions',
        payload: {
          sourceProject: source,
          compositionIds: select.value === '__all__' ? undefined : [select.value],
        },
      });
      resetTransient();
      for (const compositionId of imported.importedCompositionIds)
        compositionViewStates.forget(compositionId);
      compositionViewStates.prune(session.projectSnapshot().compositions);
      const graphScroll = restoreCompositionViewState();
      render(false, null, graphScroll);
      updateDocumentStatus();
      notify(select.value === '__all__' ? 'Compositions imported' : 'Composition imported');
    } catch (error) {
      notify('Composition import rejected; current document preserved: ' + error.message, true);
    }
  });
  document.body.append(dialog);
  dialog.showModal();
  select.focus();
}
function adoptSVGPart(text, name, request) {
  if (request.id !== svgPartRequest) return;
  if (
    request.generation !== documents.generation ||
    documents.hasDraft() ||
    readDocument() !== request.before
  )
    throw Error('Document changed while reading; add the SVG again');
  const source = readSVGSource(text);
  const result = commandChange(
    'add-svg-part',
    { layerId: request.layerId, source, name },
    'SVG node added; vector source is stored in this document',
    true,
    request.envelope,
  );
  if (!result) return;
  activeLayer = result.layerId;
  selected = result.nodeId;
  selection = new Set([selected]);
  artworkTarget = null;
  artworkSelection = new Set();
  pointSelection = { nodeId: null, ids: new Set() };
  pointFocus = null;
  tool = 'select';
  render();
  revealNode(selected);
}
async function importDesktopSVG() {
  const request = ++openRequest,
    before = readDocument(),
    generation = documents.generation,
    envelope = session.state;
  notify('Choose an SVG file…');
  const opened = await openSVGSource();
  if (request !== openRequest || generation !== documents.generation) return;
  if (!opened) {
    notify('SVG selection cancelled');
    return;
  }
  if (documents.hasDraft() || readDocument() !== before)
    throw Error('Document changed while reading; import the SVG again');
  const source = readSVGSource(opened.contents);
  const result = session.dispatch({ ...envelope, type: 'import-svg', payload: { source } });
  resetTransient();
  documents.replace();
  activeLayer = result.layerId;
  selected = result.nodeId;
  artworkTarget = null;
  artworkSelection = new Set();
  selection = new Set([selected]);
  notify(`SVG document imported: ${opened.name}`);
  render(true);
}
function updateDocumentStatus() {
  const dirty = documents.dirty,
    name = documents.binding?.path || history.doc.name;
  document.title = `${dirty ? '* ' : ''}${name} — Nect`;
  const status = document.querySelector('[data-document-status]');
  if (status) status.textContent = documents.saving ? 'Saving…' : dirty ? 'Unsaved' : 'Saved';
  recovery?.observe({
    generation: documents.generation,
    contents: readDocument(),
    name: history.doc.name || 'Untitled',
    dirty,
    revision: session.projectRevision,
  });
  updateRecoveryStatus();
}
function updateRecoveryStatus() {
  const el = document.querySelector('[data-recovery-status]');
  if (!el || !recovery) return;
  const state = recovery.state;
  const saved = state.savedRevision === null ? 'none' : `r${state.savedRevision}`;
  el.textContent = {
    ready: 'Recovery ready',
    pending: `Recovery queued r${state.requestedRevision} · saved ${saved}`,
    saving: `Saving recovery r${state.savingRevision} · saved ${saved}`,
    saved: `Recovery saved ${saved}`,
    failed: `Recovery failed r${state.failedRevision ?? state.requestedRevision} · Retry`,
  }[state.phase];
  el.title = state.error || (state.savedAt ? new Date(state.savedAt).toLocaleString() : '');
  el.classList.toggle('error', state.phase === 'failed');
}
async function showRecoveryCopies(startup = false) {
  const before = readDocument(),
    generation = documents.generation;
  const copies = await listRecovery();
  if (!copies.length) {
    if (!startup) notify('No recovery copies');
    return;
  }
  if (generation !== documents.generation || before !== readDocument() || documents.hasDraft())
    return;
  const dialog = document.createElement('dialog');
  dialog.dataset.recoveryDialog = '';
  dialog.innerHTML = `<form method="dialog"><h3>Recover editing</h3><select aria-label="Recovery copy">${copies.map((item, i) => `<option value="${i}">${esc(item.name)} · ${esc(new Date(item.savedAt).toLocaleString())}${item.previous ? ' · Previous copy' : ''}</option>`).join('')}</select><p>Recovery opens as an unsaved document.</p><button value="restore">Restore copy</button> <button value="later">Later</button></form>`;
  document.body.append(dialog);
  const chosen = await new Promise((resolve) => {
    dialog.addEventListener(
      'close',
      () => {
        resolve(
          dialog.returnValue === 'restore'
            ? copies[Number(dialog.querySelector('select').value)]
            : null,
        );
        dialog.remove();
      },
      { once: true },
    );
    dialog.showModal();
  });
  if (!chosen) return;
  if (generation !== documents.generation || before !== readDocument() || documents.hasDraft())
    throw Error('Document changed; choose the recovery copy again');
  if (!startup && !(await confirmReplace('restoring a recovery copy'))) return;
  const contents = await readRecovery(chosen.id),
    next = deserializeProject(contents);
  await preflightImages(next);
  if (generation !== documents.generation || before !== readDocument() || documents.hasDraft())
    throw Error('Document changed; current document preserved');
  await prepareOpenDocument(next);
  render(true);
  const protectedCopy = await recovery.preserve(chosen.id);
  notify(
    protectedCopy
      ? 'Recovery restored · Save As to keep a project file'
      : 'Recovery restored; recovery write failed',
    !protectedCopy,
  );
}
function replaceDocument(
  next,
  { binding = null, saved = false, prepared = false, activeCompositionId = null } = {},
) {
  sourceEpoch++;
  sourcePicker = null;
  sourceReturn = null;
  cancelVariableAction();
  if (!prepared) {
    variableBridge?.dispose();
    variableBridge = null;
  }
  resetTransient();
  compositionViewStates.clear();
  detailsState.clear();
  contextMenuScroll.clear();
  viewerState = createViewerState();
  const project = asCompositionProject(next);
  activeCompositionId ??= project.compositions[0].id;
  session = createCompositionSessionOwner(project, { ...sessionOptions, activeCompositionId });
  history = session.historyView;
  documents.replace({ binding, saved });
  zoom = 1;
  canvasFit = true;
  pan = [0, 0];
  graphZoom = 1;
  tool = 'select';
  selectedGuide = null;
  activeLayer = initialLayerId(history.doc);
  selected = current().nodes[0]?.id;
  artworkTarget = null;
  artworkSelection = new Set();
  selection = new Set();
}
function compositionViewDefaults(doc) {
  const layerId = initialLayerId(doc),
    layer = doc.layers.find((item) => item.id === layerId),
    selectedNode = layer?.nodes[0]?.id || null;
  return {
    activeLayer: layerId,
    selected: selectedNode,
    selection: selectedNode ? [selectedNode] : [],
    artworkTarget: null,
    artworkSelection: [],
    pointSelection: { nodeId: null, ids: [] },
    pointFocus: null,
    selectedGuide: null,
    zoom: 1,
    canvasFit: true,
    pan: [0, 0],
    graphZoom: 1,
    graphScroll: [0, 0],
    tool: 'select',
    guideIds: guideSettings(doc).guides.map((guide) => guide.id),
    viewerState: createViewerState(),
  };
}
function saveCompositionViewState() {
  const graph = app.querySelector('.graph-scroll');
  compositionViewStates.save(session.activeCompositionId, {
    activeLayer,
    selected,
    selection: [...selection],
    artworkTarget,
    artworkSelection: [...artworkSelection],
    pointSelection: { nodeId: pointSelection.nodeId, ids: [...pointSelection.ids] },
    pointFocus: pointFocus ? { ...pointFocus } : null,
    selectedGuide,
    zoom,
    canvasFit,
    pan: [...pan],
    graphZoom,
    graphScroll: graph ? [graph.scrollLeft, graph.scrollTop] : [0, 0],
    tool,
    viewerState,
  });
}
function restoreCompositionViewState() {
  const state = compositionViewStates.restore(
    session.activeCompositionId,
    history.doc,
    compositionViewDefaults(history.doc),
  );
  activeLayer = state.activeLayer;
  selected = state.selected;
  selection = new Set(state.selection);
  artworkTarget = state.artworkTarget;
  artworkSelection = new Set(state.artworkSelection);
  pointSelection = {
    nodeId: state.pointSelection.nodeId,
    ids: new Set(state.pointSelection.ids),
  };
  pointFocus = state.pointFocus ? { ...state.pointFocus } : null;
  selectedGuide = state.selectedGuide;
  zoom = state.zoom;
  canvasFit = state.canvasFit;
  pan = [...state.pan];
  graphZoom = state.graphZoom;
  tool = state.tool;
  viewerState = state.viewerState;
  return state.graphScroll;
}
function switchActiveComposition(compositionId) {
  if (variableAction) throw Error('Finish or cancel variable font preparation first');
  if (documents.hasDraft()) throw Error('Finish or cancel the active edit first');
  if (compositionId === session.activeCompositionId) return false;
  const project = session.projectSnapshot();
  if (!project.compositions.some((composition) => composition.id === compositionId))
    throw Error('Unknown active Composition ID');
  saveCompositionViewState();
  resetTransient();
  if (!session.switchComposition(compositionId)) return false;
  compositionViewStates.prune(project.compositions);
  const graphScroll = restoreCompositionViewState();
  render(false, null, graphScroll);
  updateDocumentStatus();
  return true;
}
function renderCompositionTablist() {
  const compositions = session.projectSnapshot().compositions,
    activeId = session.activeCompositionId,
    select = app.querySelector('[data-active-composition]')?.closest('label'),
    workspace = app.querySelector('.workspace');
  if (tabRenderSession !== session || tabRenderCompositionId !== activeId) {
    focusedCompositionTabId = activeId;
    tabRenderSession = session;
    tabRenderCompositionId = activeId;
  }
  if (compositions.length < 2) return;
  focusedCompositionTabId ??= activeId;
  if (!compositions.some((composition) => composition.id === focusedCompositionTabId))
    focusedCompositionTabId = activeId;
  const tablist = document.createElement('div');
  tablist.className = 'composition-tabs';
  tablist.setAttribute('role', 'tablist');
  tablist.setAttribute('aria-label', 'Compositions');
  tablist.setAttribute('aria-orientation', 'horizontal');
  for (const composition of compositions) {
    const tab = document.createElement('button');
    tab.type = 'button';
    tab.className = 'composition-tab';
    tab.id = `composition-tab-${composition.id}`;
    tab.dataset.activeComposition = composition.id;
    tab.setAttribute('role', 'tab');
    tab.setAttribute('aria-controls', 'composition-panel');
    tab.setAttribute('aria-selected', String(composition.id === activeId));
    tab.tabIndex = composition.id === focusedCompositionTabId ? 0 : -1;
    tab.title = composition.name;
    tab.textContent = composition.name;
    tablist.append(tab);
  }
  select?.replaceWith(tablist);
  if (workspace) {
    workspace.id = 'composition-panel';
    workspace.setAttribute('role', 'tabpanel');
    workspace.setAttribute('aria-labelledby', `composition-tab-${activeId}`);
  }
}
function focusCompositionTab(compositionId) {
  const tabs = [...app.querySelectorAll('[role="tab"][data-active-composition]')],
    tab = tabs.find((item) => item.dataset.activeComposition === compositionId);
  if (!tab) return;
  focusedCompositionTabId = compositionId;
  for (const item of tabs) item.tabIndex = item === tab ? 0 : -1;
  tab.focus();
}
function activateCompositionTab(compositionId) {
  try {
    switchActiveComposition(compositionId);
  } catch (error) {
    notify(error.message, true);
    render();
  }
  focusCompositionTab(session.activeCompositionId);
}
app.addEventListener('keydown', (event) => {
  const tab = event.target.closest?.('[role="tab"][data-active-composition]');
  if (!tab || event.altKey || event.ctrlKey || event.metaKey) return;
  const tabs = [...app.querySelectorAll('[role="tab"][data-active-composition]')],
    index = tabs.indexOf(tab);
  if (index < 0) return;
  let target = null;
  switch (event.key) {
    case 'ArrowRight':
      target = tabs[(index + 1) % tabs.length];
      break;
    case 'ArrowLeft':
      target = tabs[(index + tabs.length - 1) % tabs.length];
      break;
    case 'Home':
      target = tabs[0];
      break;
    case 'End':
      target = tabs.at(-1);
      break;
    case 'Enter':
    case ' ':
    case 'Space':
      event.preventDefault();
      activateCompositionTab(tab.dataset.activeComposition);
      return;
    default:
      return;
  }
  event.preventDefault();
  focusCompositionTab(target.dataset.activeComposition);
});
function addComposition() {
  if (variableAction) throw Error('Finish or cancel variable font preparation first');
  if (documents.hasDraft()) throw Error('Finish or cancel the active edit first');
  saveCompositionViewState();
  resetTransient();
  const compositionId = session.createComposition();
  compositionViewStates.forget(compositionId);
  compositionViewStates.prune(session.projectSnapshot().compositions);
  const graphScroll = restoreCompositionViewState();
  render(false, null, graphScroll);
  updateDocumentStatus();
}
function download(text, name, type) {
  const a = document.createElement('a'),
    url = URL.createObjectURL(new Blob([text], { type }));
  a.href = url;
  a.download = name;
  a.click();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
}
textTools = textToolGroup({
  root: app,
  active: () => tool === 'Text',
  available: (canvas = false) => {
    const field = document.activeElement;
    const pendingField = field?.matches?.('input,textarea') && field.value !== field.defaultValue;
    return (
      !navigationBusy() &&
      !documents.hasDraft() &&
      !variableAction &&
      !sourcePicker &&
      !canvasTextEdit &&
      !nameEdit &&
      !pendingField &&
      !document.querySelector('dialog[open]') &&
      (!canvas || !space)
    );
  },
  activate: () => {
    tool = 'Text';
    characterEdit = null;
    render();
    document.querySelector('[data-tool="Text"]')?.focus({ preventScroll: true });
  },
  create: (at, writingMode) => createArtwork('Text', { at, writingMode }),
  point: canvasPoint,
});
app.addEventListener('click', async (e) => {
  let b = e.target.closest('button');
  if (!b) return;
  const d = b.dataset;
  if (d.activeComposition !== undefined) {
    activateCompositionTab(d.activeComposition);
    return;
  }
  if (Object.hasOwn(d, 'sourceNav')) {
    sourceNavigationAction(b);
    return;
  }
  if (d.pointPanelMode || d.pointPanelPage) {
    const id = d.pointPanelMode || d.pointPanelPage;
    const view = pointPanelViews.get(id) || { all: false, page: 0 };
    pointPanelViews.set(
      id,
      d.pointPanelMode
        ? { all: !view.all, page: 0 }
        : { ...view, page: Math.max(0, Number(d.page)) },
    );
    b.blur();
    refreshPointPanels();
    return;
  }
  if (d.selectPanelPoint) {
    const n = current().nodes.find((n) => n.id === d.node);
    if (!n) return;
    selected = n.id;
    selection = new Set([n.id]);
    artworkTarget = null;
    artworkSelection = new Set();
    pointFocus = { nodeId: n.id, pointId: d.selectPanelPoint };
    b.blur();
    selectPointTarget(n, { pointId: d.selectPanelPoint, key: 'position' }, e);
    render();
    return;
  }
  if (d.anchorPreset !== undefined) {
    if (!b.disabled)
      commandChange(
        'reposition-transform-anchor',
        { nodeId: d.anchorNode, preset: Number(d.anchorPreset) },
        'Anchor repositioned; artwork preserved',
      );
    return;
  }
  if (d.arrangementApply || d.arrangementClear) {
    if (b.disabled) return;
    const nodeId = d.arrangementApply || d.arrangementClear,
      gap = Number(
        b.closest('.character-arrangement')?.querySelector('[data-arrangement-gap]')?.value,
      );
    if (!flushArrangementText(nodeId, b.closest('.text-controls'))) return;
    commandChange(
      d.arrangementClear ? 'clear-character-arrangement' : 'set-character-arrangement',
      { nodeId, ...(d.arrangementApply ? { mode: 'equal-ink-gap', gap } : {}) },
      d.arrangementClear ? 'Authored character positions restored' : 'Maintained ink gaps applied',
    );
    return;
  }
  if (d.action === 'align-panel') {
    const opening = !alignmentPreferences.open;
    if (opening) closeTransientPanels('align');
    alignmentPreferences.open = !alignmentPreferences.open;
    saveAlignmentPreferences();
    render();
    return;
  }
  if (d.distributeArtwork !== undefined || d.distributeGap !== undefined) {
    distributeArtwork(b);
    return;
  }
  if (d.alignArtwork !== undefined) {
    alignArtwork(b);
    return;
  }
  if (d.orderLayer !== undefined) {
    orderArtwork(d.orderLayer, d.orderDirection);
    return;
  }
  if (d.duplicateArtwork !== undefined) {
    if (app.contains(b)) duplicateArtwork(d.duplicateArtwork, d.duplicateSelection);
    return;
  }
  // Layer deletion must never auto-finish a Pen draft or cancel a live gesture.
  if (d.deleteLayer !== undefined) {
    deleteArtwork(d.deleteLayer);
    return;
  }
  if (
    /^(new|open|import-svg|add-svg-part|save|save-as)$/.test(d.action || '') &&
    documents.hasDraft()
  ) {
    notify('Finish or cancel the active edit before changing documents.', true);
    return;
  }
  if (d.penCancel !== undefined) {
    discardPenDraft();
    tool = 'select';
    notify('Draft path cancelled');
    render();
    document.querySelector('.canvas-stage').focus({ preventScroll: true });
    return;
  }
  if (d.penFinish !== undefined) {
    finishPenDraft();
    return;
  }
  if (penDraft && d.action === 'undo') {
    discardPenDraft();
    notify('Draft path cancelled');
    render();
    document.querySelector('.canvas-stage').focus({ preventScroll: true });
    return;
  }
  const viewAction = /^(zoom-|graph-|fit$|actual-size$|help$)/.test(d.action || '');
  if (penDraft && !viewAction && !b.closest('[inert]') && !finishPenDraft()) return;
  if (b.closest('[inert]')) return;
  const n = selectedNode();
  if (d.bypass) {
    cancelInteraction();
    const target = current().nodes.find((n) => n.id === d.bypass);
    commandChange(
      'set-node-bypassed',
      { nodeId: d.bypass, bypassed: !target?.bypassed },
      'Node bypass updated; settings and connections retained',
    );
    return;
  }
  if (d.styleTextRange || d.kernTextCursor) {
    const id = d.styleTextRange || d.kernTextCursor,
      el = b.closest('.text-controls')?.querySelector(`[data-text="${id}"]`),
      sourceNode = current().nodes.find((node) => node.id === id);
    const range =
      (el && textRanges.get(textRangeKey(el))) ||
      (el && { raw: el.value, start: el.selectionStart, end: el.selectionEnd });
    if (!el || !range || !sourceNode || current().locked) return;
    if (textDrafts.get(id)?.composition) {
      notify('Finish or cancel text composition first', true);
      return;
    }
    const draft = textDrafts.get(id);
    if (el.value !== sourceNode.text || draft?.state.changed)
      el.dispatchEvent(new Event('change', { bubbles: true }));
    try {
      const target = current().nodes.find((node) => node.id === id);
      if (range.raw !== target.text) throw Error('Text changed; select the range again');
      const ids = d.kernTextCursor
        ? characterPairInRange(
            target.text,
            target.characterPositions,
            range.start,
            range.end,
          ).split(':')
        : characterIdsInRange(target.text, target.characterPositions, range.start, range.end);
      if (d.kernTextCursor) {
        const layout = evaluate(current(), fonts, history.doc.imageAssets).textLayout(target.id);
        if (layout.reason) throw Error(layout.reason);
      }
      if (!ids.length) throw Error('Highlight one or more complete characters in Text first');
      artworkTarget = null;
      artworkSelection = new Set();
      selected = target.id;
      selection = new Set([target.id]);
      characterEdit = {
        node: target.id,
        layer: current().id,
        selected: new Set(ids),
        route: null,
        hover: null,
      };
      tool = 'select';
      render();
      document
        .querySelector(
          d.kernTextCursor ? '[data-character-kerning]' : '[data-character-style="font"]',
        )
        ?.focus({ preventScroll: true });
    } catch (error) {
      notify(error.message, true);
    }
    return;
  }
  if (d.characterKerningInherit !== undefined) {
    const context = characterContext(),
      pair = pairContext(context);
    if (pair)
      commandChange(
        'set-character-kerning',
        { nodeId: context.n.id, changes: { [pair.key]: null } },
        'Pair kerning inherited',
      );
    return;
  }
  if (d.characterStyleReset !== undefined || d.characterStyleInherit !== undefined) {
    const context = characterContext();
    if (!context?.typographySelected.length || context.typographyReason) return;
    const patch = d.characterStyleInherit
      ? { [d.characterStyleInherit]: null }
      : Object.fromEntries(CHARACTER_STYLE_KEYS.map((key) => [key, null]));
    commandChange(
      'set-character-style',
      { nodeId: selectedNode().id, characterIds: [...characterEdit.selected], patch },
      'Selected typography reset',
    );
    return;
  }
  if (d.characterMode) {
    cancelInteraction();
    artworkTarget = null;
    artworkSelection = new Set();
    selected = d.characterMode;
    selection = new Set([selected]);
    if (characterEdit?.node === d.characterMode) characterEdit = null;
    else if (!current().locked) {
      characterEdit = {
        node: d.characterMode,
        layer: current().id,
        selected: new Set(),
        route: null,
        hover: null,
      };
      tool = 'select';
    }
    render();
    document.querySelector('.canvas-stage').focus({ preventScroll: true });
    return;
  }
  if (d.editText) {
    artworkTarget = null;
    artworkSelection = new Set();
    selected = d.editText;
    selection = new Set([selected]);
    characterEdit = null;
    render();
    document
      .querySelector(`.inspector-body [data-text="${d.editText}"]`)
      ?.focus({ preventScroll: true });
    return;
  }
  if (d.characterShapeReset !== undefined) {
    const context = characterContext();
    if (!context?.selected.length || context.layout.reason) return;
    commandChange(
      'reset-character-adjustments',
      { nodeId: context.n.id, mode: 'transforms', characterIds: [...characterEdit.selected] },
      'Character shapes reset',
    );
    return;
  }
  if (d.characterReset) {
    commandChange(
      'reset-character-adjustments',
      {
        nodeId: selectedNode()?.id,
        mode: d.characterReset === 'all' ? 'all' : 'offsets',
        ...(d.characterReset === 'all' ? {} : { characterIds: [...characterEdit.selected] }),
      },
      d.characterReset === 'all' ? 'All character adjustments reset' : 'Character XY offsets reset',
    );
    return;
  }
  if (d.layoutCancel !== undefined) {
    layoutDraft = null;
    layoutPreview = false;
    render();
    return;
  }
  if (d.layoutApply !== undefined || d.layoutRemove !== undefined) {
    try {
      session.dispatch({
        ...session.state,
        type: 'set-layout-grid',
        payload: { config: d.layoutRemove !== undefined ? null : layoutConfig() },
      });
      layoutDraft = null;
      layoutPreview = false;
      selectedGuide = null;
      snapMatches = [];
      notify(
        d.layoutRemove !== undefined
          ? 'Layout guides removed; manual guides kept'
          : 'Layout guides generated',
      );
    } catch (e) {
      notify(e.message, true);
    }
    render();
    return;
  }
  if (d.layerGuide) {
    const l = history.doc.layers.find((x) => x.id === d.layerGuide);
    commandChange(
      'set-layer-state',
      { layerId: d.layerGuide, key: 'guide', value: !l.guide },
      !l.guide ? 'Guide layer: visible in editor, excluded from SVG' : 'Layer restored to artwork',
    );
    return;
  }
  if (d.addGuide) {
    const result = editGuides('create-guide', { axis: d.addGuide });
    if (result) {
      selectedGuide = result.guideId;
      render();
    }
    return;
  }
  if (d.selectGuide) {
    selectedGuide = d.selectGuide;
    render();
    return;
  }
  if (d.lockGuide || d.deleteGuide) {
    const guideId = d.lockGuide || d.deleteGuide,
      guide = guideSettings(history.doc).guides.find((g) => g.id === guideId);
    const result = d.lockGuide
      ? editGuides('set-guide', { guideId, key: 'locked', value: !guide?.locked })
      : editGuides('delete-guide', { guideId });
    if (result && d.deleteGuide && selectedGuide === guideId) {
      selectedGuide = null;
      render();
    }
    return;
  }
  if (d.action) {
    try {
      switch (d.action) {
        case 'undo': {
          const graphScroll = stepHistory();
          render(false, null, graphScroll);
          break;
        }
        case 'redo': {
          const graphScroll = stepHistory(true);
          render(false, null, graphScroll);
          break;
        }
        case 'save':
        case 'save-as': {
          try {
            const pending = documents.save(d.action === 'save-as');
            updateDocumentStatus();
            if (await pending) notify(desktop ? 'Project saved' : 'Project download started');
          } finally {
            updateDocumentStatus();
          }
          break;
        }
        case 'save-svg-export-preset':
        case 'save-png-export-preset': {
          const format = d.action === 'save-svg-export-preset' ? 'svg' : 'png';
          try {
            saveCurrentExportPreset(format);
          } catch (reason) {
            notify(reason?.message || 'Export preset could not be saved', true);
          }
          break;
        }
        case 'export':
        case 'export-png': {
          if (exportInFlight) break;
          exportInFlight = true;
          try {
            if (d.action === 'export') {
              const svg = exportSVG(history.doc, fonts, {
                unit: svgExportUnit,
                responsive: svgExportSizing === 'responsive',
                ...(svgExportPrecision === 'default'
                  ? {}
                  : { precision: Number(svgExportPrecision) }),
              });
              if (await saveFile(svg, 'nect-artwork.svg', 'svg'))
                notify('SVG exported · transparent artboard; guides excluded');
            } else {
              const size = pngDimensions(history.doc, pngExportSettings());
              const svg = exportSVG(history.doc, fonts);
              const ppi = pngExportMode === 'ppi' ? pngPpi : null,
                blob = await rasterizeSVG(svg, size, ppi);
              if (await saveFile(blob, 'nect-artwork.png', 'png'))
                notify(`PNG exported · ${size.width} × ${size.height} · transparent background`);
            }
          } finally {
            exportInFlight = false;
          }
          break;
        }
        case 'clear-layer-solo':
          commandChange('clear-layer-solo', {}, 'Solo cleared');
          break;
        case 'import-image':
          imagePick = {
            id: ++imageRequest,
            before: readDocument(),
            doc: history.doc,
            commandState: session.state,
          };
          document.querySelector('#image-file').value = '';
          document.querySelector('#image-file').click();
          break;
        case 'import-composition':
          try {
            await openCompositionImportDialog();
          } catch (error) {
            notify(
              `Composition import rejected; current document preserved: ${error.message}`,
              true,
            );
          }
          break;
        case 'add-svg-part':
          svgPartTarget(history.doc, current().id);
          svgPartPick = {
            id: ++svgPartRequest,
            before: readDocument(),
            generation: documents.generation,
            envelope: session.state,
            layerId: current().id,
          };
          if (desktop) {
            const request = svgPartPick;
            svgPartPick = null;
            notify('Choose an SVG file…');
            try {
              const opened = await openSVGSource();
              if (request.id !== svgPartRequest) break;
              if (opened) adoptSVGPart(opened.contents, opened.name, request);
              else notify('SVG selection cancelled');
            } catch (err) {
              notify(`SVG node rejected; current document preserved: ${err.message}`, true);
            }
          } else {
            document.querySelector('#svg-part-file').value = '';
            document.querySelector('#svg-part-file').click();
          }
          break;
        case 'import-svg':
          if (!(await confirmReplace('importing SVG'))) break;
          if (desktop) {
            try {
              await importDesktopSVG();
            } catch (err) {
              notify(`SVG import rejected; current document preserved: ${err.message}`, true);
            }
          } else {
            document.querySelector('#import-svg-file').value = '';
            document.querySelector('#import-svg-file').click();
          }
          break;
        case 'open':
          if (!(await confirmReplace())) break;
          if (desktop) {
            const request = ++openRequest,
              before = readDocument(),
              generation = documents.generation;
            const opened = await openFile();
            try {
              if (request !== openRequest || generation !== documents.generation) break;
              if (readDocument() !== before)
                throw Error('Document changed while reading; open the file again');
              if (opened !== null) {
                const next = deserializeProject(opened.contents);
                await preflightImages(next);
                if (
                  request !== openRequest ||
                  generation !== documents.generation ||
                  documents.hasDraft() ||
                  readDocument() !== before
                )
                  throw Error('Document changed while reading; open the file again');
                await prepareOpenDocument(next, { binding: opened.binding, saved: true });
                render(true);
                notify('Project restored');
              }
            } finally {
              // Accepted tokens stay active; every rejected/stale Open is retired.
              await documents.releaseUnused(opened?.binding);
            }
          } else document.querySelector('#open-file').click();
          break;
        case 'add-composition':
          addComposition();
          notify('Composition added');
          break;
        case 'new': {
          if (!(await confirmReplace('creating a new document'))) break;
          replaceDocument(blankDocument());
          selected = null;
          artworkTarget = null;
          artworkSelection = new Set();
          selection = new Set();
          tool = 'select';
          notify('New blank document · add artwork or graph nodes');
          render(true);
          document.querySelector('.canvas-stage').focus({ preventScroll: true });
          break;
        }
        case 'add-blank-layer': {
          // Changing layer scope must not retain an old layer's drag/wire target.
          cancelInteraction();
          const priorUI = selectionSnapshot();
          const created = session.dispatch({ ...session.state, type: 'create-blank-layer' });
          activeLayer = created.layerId;
          selected = null;
          artworkTarget = null;
          artworkSelection = new Set();
          selection = new Set();
          tool = 'select';
          selectionHistory.set(history.past.at(-1), priorUI);
          selectionHistory.set(history.doc, selectionSnapshot());
          notify('Added blank composition · Output is empty');
          render(true);
          document.querySelector('.canvas-stage').focus({ preventScroll: true });
          break;
        }
        case 'zoom-in':
        case 'zoom-out':
        case 'actual-size':
          if (!navigationBusy())
            zoomCanvas(
              d.action === 'actual-size' ? 1 : zoom * (d.action === 'zoom-in' ? 1.2 : 1 / 1.2),
            );
          break;
        case 'snap':
          editGuides('set-guide-setting', { key: 'snap', value: !guideSettings(history.doc).snap });
          break;
        case 'center-grid':
          editGuides('set-guide-setting', { key: 'gridAnchor', value: 'center' });
          break;
        case 'grid-panel':
          if (!gridOpen) closeTransientPanels('grid');
          gridOpen = !gridOpen;
          render();
          break;
        case 'layout-panel':
          if (!layoutOpen) closeTransientPanels('layout');
          layoutOpen = !layoutOpen;
          render();
          break;
        case 'guides-panel':
          if (!guidesOpen) closeTransientPanels('guides');
          guidesOpen = !guidesOpen;
          render();
          break;
        case 'fit':
          if (!navigationBusy()) fitArtboard();
          break;
        case 'find-node': {
          if (navigationBusy() || penDraft) break;
          const layerId = current().id;
          const panel = openSearchPanel({
            title: 'Find node',
            action: 'Go to node',
            selectedId: selected,
            items: current().nodes.map((n) => ({
              id: n.id,
              label: nodeDisplayName(n),
              detail: `${n.type} · ${n.id}`,
            })),
            onCommit(item) {
              if (!item || current().id !== layerId) return;
              const node = current().nodes.find((n) => n.id === item.id);
              if (!node) return;
              panel.close();
              selected = node.id;
              selection = new Set([node.id]);
              artworkTarget = null;
              artworkSelection.clear();
              render();
              const viewport = document.querySelector('.graph-scroll'),
                card = document.querySelector(`.node[data-id="${node.id}"]`);
              const scroll = [
                (node.position[0] + (card?.offsetWidth || node.width || 264) / 2) * graphZoom -
                  viewport.clientWidth / 2,
                (node.position[1] + (card?.offsetHeight || 100) / 2) * graphZoom -
                  viewport.clientHeight / 2,
              ].map((value) => Math.max(0, value));
              updateGraphBounds(scroll);
              viewport.scrollLeft = scroll[0];
              viewport.scrollTop = scroll[1];
              viewport.focus({ preventScroll: true });
              drawWires();
            },
          });
          break;
        }
        case 'graph-overview':
          if (!navigationBusy()) {
            graphOverviewOpen = !graphOverviewOpen;
            updateGraphOverview();
          }
          break;
        case 'graph-pan-up':
        case 'graph-pan-down':
        case 'graph-pan-left':
        case 'graph-pan-right':
          if (!navigationBusy()) {
            const viewport = document.querySelector('.graph-scroll');
            const amount = 80;
            const delta = {
              'graph-pan-up': [0, -amount],
              'graph-pan-down': [0, amount],
              'graph-pan-left': [-amount, 0],
              'graph-pan-right': [amount, 0],
            }[d.action];
            const scroll = [
              Math.max(0, viewport.scrollLeft + delta[0]),
              Math.max(0, viewport.scrollTop + delta[1]),
            ];
            updateGraphBounds(scroll);
            viewport.scrollTo(scroll[0], scroll[1]);
            updateGraphOverview(false);
            viewport.focus({ preventScroll: true });
          }
          break;
        case 'save-node-preset':
        case 'update-node-preset':
        case 'apply-node-preset':
          changeNodePreset(d.action);
          break;
        case 'graph-fit':
        case 'graph-fit-selection':
          if (!navigationBusy()) fitGraph(d.action === 'graph-fit-selection');
          break;
        case 'graph-zoom-in':
        case 'graph-zoom-out':
          if (!navigationBusy())
            zoomGraph(graphZoom * (d.action === 'graph-zoom-in' ? 1.2 : 1 / 1.2));
          break;
        case 'graph-reset':
          if (!navigationBusy()) {
            zoomGraph(1);
            document.querySelector('.graph-scroll').scrollTo(0, 0);
          }
          break;
        case 'help': {
          let dialog = document.createElement('dialog');
          dialog.innerHTML =
            '<h3>Nect SVG</h3><p>The editor starts with a blank document. New creates a blank document; + Blank composition appends an empty vector Output. Each layer owns an independent node composition. Toolbar shapes and Text append independent artwork. Canvas or Layers selects final artwork; graph titles select a source node. Drag the node-stage outline/marker for source edits. Blank canvas clears selection. Shift-drag locks horizontal, vertical or 45°; release Shift to move freely. Focus the artboard for Arrow: 1 unit, Shift+Arrow: 10; unsupported artwork shows its reason.</p><p>Pen creates independent Curve artwork without replacing existing layers. Click for corners, drag for Bézier handles, and click the first point to close. Enter, Escape or Finish path finishes an open curve. Cancel path discards the detached draft. One-point drafts are discarded on finish. Each finished path is one Undo. Add node → Curve and Use as output remain explicit graph operations.</p><p>Connect closed paths to Boolean A/B. Drag a path to Add input for C and later inputs. Save JSON retains all nodes and edits; SVG is the evaluated artwork.</p><p>Ctrl+Z / Ctrl+Shift+Z: Undo / Redo. Focus either view for +/− zoom, Ctrl/Cmd+1 actual size, Shift+1 fit. Artboard: plain wheel pans; Shift+wheel pans horizontally; Ctrl/Cmd+wheel zooms at the pointer. Space + drag or middle drag pans. Fit artboard centers the artboard; 100% is one document unit per CSS pixel.</p><p>Node graph: Ctrl/Cmd+D duplicates selected nodes beside the selection in one Undo. Middle drag, Ctrl/Cmd+Shift+drag or Space+drag pans; Alt or Ctrl/Cmd+wheel zooms (5–200%). Ctrl/Cmd+1 restores actual size while the graph is focused. Hold a dragged node near an edge to scroll; right/bottom grow automatically, left/top stop at the origin. Node edges and centers snap into alignment within 6 screen pixels; hold Ctrl/Cmd during the drag to bypass. Escape cancels a node drag. Fit all frames measured cards; Shift+2 fits selected cards. Reset view restores 100% at the origin. Fitting is limited to 5–200%.</p><button>Close</button>';
          dialog.querySelector('button').onclick = () => closeEditorDialog(dialog);
          showEditorDialog(dialog);
          break;
        }
      }
      // Pointer users can immediately hold Space and drag after clicking a view control.
      // Keyboard activation retains the button so Space keeps its native action.
      if (
        e.detail > 0 &&
        !navigationBusy() &&
        [
          'zoom-in',
          'zoom-out',
          'actual-size',
          'fit',
          'graph-zoom-in',
          'graph-zoom-out',
          'graph-fit',
          'graph-fit-selection',
          'graph-reset',
        ].includes(d.action)
      )
        document
          .querySelector(d.action.startsWith('graph-') ? '.graph-scroll' : '.canvas-stage')
          .focus({ preventScroll: true });
    } catch (err) {
      notify(err.message, true);
    }
    return;
  }
  if (d.loadFont || d.loadCharacterFont || d.loadVariableFont) {
    if (current().locked) {
      notify('Composition is locked', true);
      return;
    }
    fontRequest = {
      id: ++fontRequestId,
      layer: current().id,
      node: d.loadFont || d.loadCharacterFont || d.loadVariableFont,
      variable: !!d.loadVariableFont,
      ...(d.loadCharacterFont ? { characters: [...(characterEdit?.selected || [])] } : {}),
      before: readDocument(),
      commandState: session.state,
    };
    const input = document.querySelector('#font-file');
    input.dataset.fontRequest = String(fontRequest.id);
    input.value = '';
    input.click();
    return;
  }
  if (d.tool) {
    cancelInteraction();
    characterEdit = null;
    tool = d.tool;
    if (tool === 'pen') {
      if (!['Curve', 'Multi Path', 'Path Remap'].includes(selectedNode()?.type)) {
        const route = directSelectionSource(history.doc, current().id, {
          fonts: resolveFonts(history.doc, activeFonts()),
        });
        if (route.supported) {
          selected = route.nodeId;
          selection = new Set([selected]);
          artworkTarget = null;
          artworkSelection = new Set();
        }
      }
      penId = null;
      notify(
        'Click a selected source path segment to add a point; click blank canvas for a new path. Alt-click an anchor for handles.',
      );
      render();
    } else if (tool === 'direct') {
      artworkTarget = null;
      artworkSelection = new Set();
      const route = directSelectionSource(history.doc, current().id, {
        fonts: resolveFonts(history.doc, activeFonts()),
      });
      if (route.supported) {
        selected = route.nodeId;
        selection = new Set([selected]);
        if (pointSelection.nodeId !== selected)
          pointSelection = { nodeId: selected, ids: new Set() };
      }
      notify(route.supported ? 'Select anchors; Shift-click adds or removes points' : route.reason);
      render();
      document.querySelector('.canvas-stage')?.focus({ preventScroll: true });
    } else if (tool !== 'select') {
      createArtwork(tool);
      tool = 'select';
      render();
    } else render();
    return;
  }
  if (d.selectLayer) {
    if (
      !e.shiftKey &&
      lastNameClick?.layerId === d.selectLayer &&
      nameClicked(d.selectLayer, null, e)
    )
      return;
    resetTransient();
    selectArtwork(d.selectLayer, { toggle: e.shiftKey });
    render();
    if (!e.shiftKey) nameClicked(d.selectLayer, null, e);
    if (!nameEdit)
      (b.closest('[data-layer]')?.setAttribute('tabindex', '0'),
        b.closest('[data-layer]')?.focus({ preventScroll: true }));
    return;
  }
  if (d.visible || d.layerLock || d.layerSolo) {
    const layerId = d.visible || d.layerLock || d.layerSolo,
      key = d.visible ? 'visible' : d.layerSolo ? 'solo' : 'locked',
      l = history.doc.layers.find((x) => x.id === layerId);
    commandChange('set-layer-state', { layerId, key, value: !l[key] });
    return;
  }
  if (d.searchSource !== undefined) {
    const select = b.closest('.field').querySelector('select');
    if (select.disabled || current().locked) return;
    const panel = openSearchPanel({
      title: 'Connection nodes',
      action: 'Apply',
      selectedId: select.value,
      items: [...select.options].map((option) => ({
        id: option.value,
        label: option.textContent,
        detail: option.value,
        disabled: option.disabled,
      })),
      onCommit(item) {
        if (!item || !select.isConnected || current().locked) return;
        select.value = item.id;
        select.dispatchEvent(new window.Event('change', { bubbles: true }));
        panel.close();
      },
    });
    return;
  }
  if (d.connectMenu !== undefined) {
    let f = b.closest('.field'),
      nn = current().nodes.find((x) => x.id === f.dataset.node),
      q = parameter(nn, f.dataset.key);
    if (current().locked || q.locked) return;
    let dialog = document.createElement('dialog');
    dialog.innerHTML = `<h3>Connect ${esc(nn.name)} · ${esc(f.dataset.key)}</h3><p>Choose a ${q.type} source. Disconnect restores the saved value or expression.</p><select aria-label="Input source"><option value="">Disconnected</option>${current()
      .nodes.filter((x) => x.id !== nn.id)
      .flatMap((x) =>
        outputPorts(x)
          .filter((port) => socketCompatibleFor(x, nn, f.dataset.key, port.id))
          .map(
            (port) =>
              `<option value="${esc(port.id ? JSON.stringify([x.id, port.id]) : x.id)}" ${q.source === x.id && (q.sourcePort || 'output') === (port.id || 'output') ? 'selected' : ''}>${esc(x.name + (port.label ? ' ' + port.label : ''))} · ${esc(x.id)} · ${esc(port.id || 'output')}</option>`,
          ),
      )
      .join(
        '',
      )}</select><p><button data-confirm>Apply</button> <button data-cancel>Cancel</button></p>`;
    dialog.querySelector('[data-cancel]').onclick = () => closeEditorDialog(dialog);
    dialog.querySelector('[data-confirm]').onclick = () => {
      const source = dialog.querySelector('select').value;
      const [sourceId, sourcePort] = source.startsWith('[')
        ? JSON.parse(source)
        : [source, undefined];
      commandChange(
        source ? 'connect' : 'disconnect',
        { sourceId, sourcePort, nodeId: nn.id, key: f.dataset.key },
        'Connection updated',
      );
      closeEditorDialog(dialog);
    };
    showEditorDialog(dialog);
    searchConnectionOptions(dialog);
    return;
  }
  if (d.lock !== undefined || d.fx !== undefined) {
    let f = b.closest('.field'),
      nn = current().nodes.find((x) => x.id === f.dataset.node),
      q = parameter(nn, f.dataset.key);
    const payload = { nodeId: nn.id, key: f.dataset.key };
    if (d.lock !== undefined)
      commandChange('set-parameter-lock', { ...payload, locked: !q.locked }, 'Parameter updated');
    else {
      if (q.locked || current().locked) return;
      const dialog = document.createElement('dialog');
      dialog.innerHTML = `<h3>Expression · ${esc(nn.name)} · ${esc(f.dataset.key)}</h3><p>${q.type === 'Vector2' ? 'X; Y expressions: value reads Base X on the left, Base Y on the right.' : 'value reads this input’s authored base.'} Type a function, node name, or parameter prefix for suggestions. ↑/↓ select; Tab/Enter insert; Escape dismisses. Insert a parameter reference, then add arithmetic. Leave blank for direct values.</p><input aria-label="Parameter expression" autocomplete="off" spellcheck="false"><p><button data-insert-reference>Insert parameter reference…</button></p><p><button data-confirm>Apply</button> <button data-cancel>Cancel</button></p>`;
      const input = dialog.querySelector('input');
      input.value = q.expression || '';
      const completion = attachExpressionCompletion(
        input,
        () => expressionReferenceTargets(current(), nn.id),
        q.type,
      );
      dialog.querySelector('[data-insert-reference]').onclick = () => {
        completion.hide();
        const start = input.selectionStart ?? input.value.length,
          end = input.selectionEnd ?? start;
        const panel = openSearchPanel({
          title: 'Parameter references',
          action: 'Insert',
          items: expressionReferenceTargets(current(), nn.id).map((target) => ({
            id: target.expression,
            label: target.label,
            detail: target.nodeId + ' · ' + target.expression,
          })),
          onCommit(item) {
            if (!item) return;
            input.value = input.value.slice(0, start) + item.id + input.value.slice(end);
            panel.close();
            input.focus();
            input.setSelectionRange(start + item.id.length, start + item.id.length);
          },
        });
      };
      dialog.querySelector('[data-cancel]').onclick = () => closeEditorDialog(dialog);
      dialog.querySelector('[data-confirm]').onclick = () => {
        if (
          commandChange(
            'set-parameter-expression',
            { ...payload, expression: input.value },
            'Parameter updated',
          )
        )
          closeEditorDialog(dialog);
      };
      dialog.addEventListener('keydown', (event) => {
        if (event.defaultPrevented || event.isComposing || event.keyCode === 229) return;
        if (event.key === 'Escape') {
          event.preventDefault();
          closeEditorDialog(dialog);
        }
        if (event.key === 'Enter' && event.target === input) {
          event.preventDefault();
          dialog.querySelector('[data-confirm]').click();
        }
      });
      showEditorDialog(dialog);
      input.focus();
    }
    return;
  }
  if (d.preview)
    commandChange(
      'preview-node',
      { nodeId: d.preview, position: freePosition('Fill') },
      'Output connected',
    );
  if (d.collapse)
    commandChange(
      'set-node-settings',
      {
        nodeId: d.collapse,
        settings: { collapsed: !current().nodes.find((n) => n.id === d.collapse)?.collapsed },
      },
      'Node display updated',
    );
  if (d.delete) {
    const result = commandChange('delete-node', { nodeId: d.delete }, 'Node deleted', true);
    if (result && selected === d.delete) {
      selected = null;
      selection.delete(d.delete);
    }
    render();
  }
  if (d.compositionJump) {
    if (history.doc.layers.some((l) => l.id === d.compositionJump)) {
      activeLayer = d.compositionJump;
      selected = null;
      selection = new Set();
      artworkTarget = null;
      artworkSelection = new Set();
      render();
    }
    return;
  }
  if (d.referenceJump) {
    const ref = current().nodes.find((n) => n.id === d.referenceJump);
    try {
      artworkTarget = null;
      artworkSelection = new Set();
      selected = referenceTarget(current(), ref).id;
      selection = new Set([selected]);
      render();
      revealNode(selected);
    } catch (e) {
      notify(e.message, true);
    }
    return;
  }
  if (d.duplicate) duplicateGraphNodes(d.duplicate);
  if (d.applyOsText) {
    const el =
      document.querySelector(`.inspector-body [data-os-text="${d.applyOsText}"]`) ||
      document.querySelector(`[data-os-text="${d.applyOsText}"]`);
    if (el) void commitOsText(d.applyOsText, el.value);
    return;
  }
  if (d.loadOsAxes) {
    void loadOsAxes(d.loadOsAxes);
    return;
  }
  if (d.resetOsAxes) {
    const n = current().nodes.find((n) => n.id === d.resetOsAxes && n.osFont?.version >= 3);
    if (n && !osTextDrafts.has(n.id))
      void commitOsText(n.id, n.text, {
        axes: Object.fromEntries(n.osFont.axes.map((a) => [a.tag, a.defaultValue])),
      });
    return;
  }
  if (d.cancelOsText) {
    osTextDrafts.delete(d.cancelOsText);
    render();
    return;
  }
  if (d.clearOsText) {
    if (osTextDrafts.has(d.clearOsText)) {
      notify('Apply or cancel the Text edit first', true);
      return;
    }
    commandChange('clear-os-font-text', { nodeId: d.clearOsText }, 'Font replaced: DejaVu Sans');
    return;
  }
  if (d.refreshOsFonts !== undefined) {
    void refreshOsFontCatalog();
    return;
  }
  if (d.osTextFont) {
    void openOsOutlinePicker(d.osTextFont, 'edit');
    return;
  }
  if (d.osTextOutline) {
    void openOsOutlinePicker(d.osTextOutline);
    return;
  }
  if (d.bakeTextOutline) {
    if (osTextDrafts.has(d.bakeTextOutline)) {
      notify('Apply or cancel the Text edit first', true);
      return;
    }
    const result = commandChange(
      'bake-text-outline',
      { nodeId: d.bakeTextOutline },
      'Text converted to editable paths',
    );
    if (result) {
      characterEdit = null;
      pointSelection = { nodeId: null, ids: new Set() };
      render();
    }
    return;
  }
  if (d.pathUnit) {
    const n = current().nodes.find((n) => n.id === d.node),
      unit = n?.units?.find((unit) => unit.id === d.pathUnit);
    if (!unit) return;
    const ids = unitPointIds(n, unit),
      old = selectedPointIds(n),
      remove = e.shiftKey && ids.every((id) => old.includes(id));
    pointPanelViews.delete(n.id);
    pointFocus = null;
    pointSelection = { nodeId: n.id, ids: new Set(e.shiftKey ? old : []) };
    for (const id of ids) remove ? pointSelection.ids.delete(id) : pointSelection.ids.add(id);
    artworkTarget = null;
    artworkSelection = new Set();
    selected = n.id;
    selection = new Set([n.id]);
    render();
    return;
  }
  if (d.movePathUnit) {
    commandChange(
      'translate-path-units',
      { nodeId: d.node, unitIds: [d.movePathUnit], delta: d.delta.split(',').map(Number) },
      'Path unit moved',
    );
    return;
  }
  if (d.addPoint) commandChange('create-point', { nodeId: d.addPoint }, 'Point added');
  if (d.deletePoint)
    commandChange('delete-point', { nodeId: d.node, pointId: d.deletePoint }, 'Point removed');
  if (d.clearBatch) {
    remapBatch(d.clearBatch).selected.clear();
    render();
  }
  if (d.applyBatch)
    commandChange(
      'edit-remap',
      {
        nodeId: d.applyBatch,
        action: 'translate',
        pointIds: [...remapBatch(d.applyBatch).selected],
        delta: remapBatch(d.applyBatch).delta,
      },
      'Selected points moved as one edit',
    );
  if (d.rebindRemap)
    commandChange(
      'edit-remap',
      { nodeId: d.node, action: 'rebind', pointId: d.rebindRemap },
      'Retained edit explicitly rebound to the current point',
    );
  if (d.enableRemap)
    commandChange(
      'edit-remap',
      { nodeId: d.node, action: 'enable', pointId: d.enableRemap },
      'Point Value / Expression / Node Input controls enabled',
    );
  if (d.clearEdits)
    commandChange('edit-remap', { nodeId: d.clearEdits, action: 'clear' }, 'Vertex edits cleared');
});
function commitFont(bytes, name, request) {
  if (variableNode(request.node))
    throw Error(
      'Switch to a static font in Default font before loading a static font. Use Load variable TTF for variable fonts.',
    );
  if (fontRequest?.id !== request.id) return;
  if (readDocument() !== request.before || activeLayer !== request.layer)
    throw Error('Document changed while reading; load the font again');
  if (
    request.characters &&
    (characterEdit?.node !== request.node ||
      request.characters.join(',') !== [...(characterEdit?.selected || [])].join(','))
  )
    throw Error('Character selection changed while reading; load the font again');
  const result = session.dispatch({
    ...request.commandState,
    type: 'adopt-font',
    payload: {
      layerId: request.layer,
      nodeId: request.node,
      bytes,
      fileName: name,
      ...(request.characters ? { characterIds: request.characters } : {}),
    },
  });
  notify(`Loaded ${result.fontName} · saved JSON will include this font`);
  render();
}
let osOutlinePending = null;
async function refreshOsFontCatalog() {
  if (!desktop || osFontCatalogPending || typeof listOsOutlineFonts !== 'function') return;
  await readCurrentOsFontCatalog();
}
async function readCurrentOsFontCatalog() {
  const request = ++osFontCatalogRequest;
  osFontCatalogPending = true;
  render();
  const catalog = await readOsFontCatalog({
    listFamilies: listOsOutlineFonts,
    listFaces: typeof listDesktopFontFaces === 'function' ? listDesktopFontFaces : undefined,
  });
  if (request === osFontCatalogRequest) {
    osFontCatalog = catalog;
    osFontCatalogPending = false;
    render();
  }
  return catalog;
}
function stageOsText(el, composing = false) {
  const n = current().nodes.find((n) => n.id === el.dataset.osText && n.osFont);
  if (!n) return;
  if (el.value === n.text && !composing) {
    osTextDrafts.delete(n.id);
    return;
  }
  const draft = osTextDrafts.get(n.id) || {};
  Object.assign(draft, { value: el.value, composing, pending: false, request: null });
  osTextDrafts.set(n.id, draft);
  updateDocumentStatus();
}
async function prepareDesktopText(source, face, patch = {}) {
  const same =
    source.osFont?.version >= 3 &&
    ['family', 'name', 'weight', 'style', 'stretch'].every(
      (k) => source.osFont.face[k] === face[k],
    );
  if (!same && patch.axes === undefined && typeof listDesktopFontAxes === 'function') {
    const axes = await listDesktopFontAxes(face);
    if (axes.length)
      patch = { ...patch, axes: Object.fromEntries(axes.map((a) => [a.tag, a.baseValue])) };
  }
  return shapeDesktopText(desktopTextIntent(source, face, patch));
}
async function loadOsAxes(nodeId) {
  const n = current().nodes.find((n) => n.id === nodeId && n.osFont?.version === 2),
    owner = session,
    envelope = session.state,
    layerId = current().id;
  if (
    !n ||
    n.locked ||
    current().locked ||
    osTextDrafts.has(nodeId) ||
    typeof listDesktopFontAxes !== 'function'
  )
    return;
  try {
    const axes = await listDesktopFontAxes(n.osFont.face);
    if (
      session !== owner ||
      current().id !== layerId ||
      selected !== nodeId ||
      session.state.expectedRevision !== envelope.expectedRevision
    )
      throw Error('Document or selection changed; check axes again');
    if (!axes.length) {
      notify('This face has no variable axes');
      return;
    }
    await commitOsText(nodeId, n.text, {
      axes: Object.fromEntries(axes.map((a) => [a.tag, a.baseValue])),
    });
  } catch (error) {
    notify(error.message || String(error), true);
  }
}
async function commitOsText(nodeId, value, settings = {}) {
  const n = current().nodes.find((n) => n.id === nodeId && n.osFont);
  if (!n || !desktop || n.locked || current().locked) return;
  let draft = osTextDrafts.get(nodeId);
  if (draft?.pending || draft?.composing || composing) return;
  const settingsOnly = !draft && value === n.text && Object.keys(settings).length > 0;
  if (value === n.text && !Object.keys(settings).length) {
    osTextDrafts.delete(nodeId);
    updateDocumentStatus();
    return;
  }
  if (!draft) {
    draft = { value, composing: false };
    osTextDrafts.set(nodeId, draft);
  }
  const owner = session,
    envelope = session.state,
    layerId = current().id,
    ticket = {};
  draft.pending = true;
  draft.request = ticket;
  draft.value = value;
  try {
    const modern = n.osFont.version >= 2;
    const intent = modern
      ? desktopTextIntent(n, n.osFont.face, { text: value, ...settings })
      : osTextIntent({ ...n, text: value }, { allowEmpty: true });
    notify('Preparing OS Text…');
    const outline = modern
      ? await prepareDesktopText(n, n.osFont.face, { text: value, ...settings })
      : await outlineOsText({
          family: n.osFont.family,
          style: n.osFont.style,
          ...intent,
        });
    if (osTextDrafts.get(nodeId) !== draft || draft.request !== ticket) return;
    if (
      session !== owner ||
      current().id !== layerId ||
      selected !== nodeId ||
      session.state.expectedRevision !== envelope.expectedRevision
    )
      throw Error('Document or selection changed; apply the Text edit again');
    if (
      modern
        ? JSON.stringify(outline.face) !== JSON.stringify(n.osFont.face)
        : outline.family !== n.osFont.family || outline.style !== n.osFont.style
    )
      throw Error('Unmatched OS font response');
    const result = commandChange(
      'set-os-font-text',
      { nodeId, text: intent.text, outline, settings },
      'OS Text updated',
      false,
      envelope,
    );
    if (result) {
      osTextDrafts.delete(nodeId);
      render();
    }
  } catch (error) {
    if (osTextDrafts.get(nodeId) === draft && draft.request === ticket)
      notify(`Text preserved: ${error.message || error}`, true);
  } finally {
    if (osTextDrafts.get(nodeId) === draft && draft.request === ticket) {
      if (settingsOnly) osTextDrafts.delete(nodeId);
      else {
        draft.pending = false;
        draft.request = null;
      }
      render();
    }
  }
}
app.addEventListener('input', (event) => {
  if (event.target.dataset.osText)
    stageOsText(
      event.target,
      event.isComposing || osTextDrafts.get(event.target.dataset.osText)?.composing,
    );
});
app.addEventListener('change', (event) => {
  const el = event.target,
    nodeId = el.dataset.osWrap || el.dataset.osWrapWidth;
  if (!nodeId) return;
  const n = current().nodes.find((n) => n.id === nodeId && n.osFont?.version >= 2);
  if (!n || osTextDrafts.has(n.id)) return;
  const textWrap = el.dataset.osWrap
    ? el.value === 'basic'
      ? basicTextWrap(n.writingMode, wrapExtent(n.textWrap) ?? 320)
      : el.value === 'width'
        ? { width: wrapExtent(n.textWrap) ?? 320 }
        : null
    : withWrapExtent(n.textWrap, el.value.trim() ? Number(el.value) : NaN);
  void commitOsText(n.id, n.text, { textWrap });
});
app.addEventListener('change', (event) => {
  const el = event.target,
    key = el.dataset.osSetting;
  if (!key) return;
  const n = current().nodes.find((n) => n.id === el.dataset.osNode && n.osFont?.version >= 2);
  if (!n || osTextDrafts.has(n.id)) return;
  const value =
    el.type === 'checkbox'
      ? el.checked
      : ['alignment', 'writingMode'].includes(key)
        ? el.value
        : Number(el.value);
  void commitOsText(n.id, n.text, {
    [key]: value,
    ...(key === 'writingMode' && n.textWrap
      ? { textWrap: basicTextWrap(value, wrapExtent(n.textWrap)) }
      : {}),
  });
});
app.addEventListener('change', (event) => {
  const el = event.target,
    tag = el.dataset.osAxis;
  if (!tag) return;
  const n = current().nodes.find((n) => n.id === el.dataset.osNode && n.osFont?.version >= 3);
  if (!n || osTextDrafts.has(n.id)) return;
  if (!el.value.trim()) {
    notify('Enter an axis value', true);
    render();
    return;
  }
  const axes = Object.fromEntries(
    n.osFont.axes.map((a) => [a.tag, a.tag === tag ? Number(el.value) : a.value]),
  );
  void commitOsText(n.id, n.text, { axes });
});
app.addEventListener('compositionstart', (event) => {
  if (event.target.dataset.osText) stageOsText(event.target, true);
});
app.addEventListener('compositionend', (event) => {
  if (event.target.dataset.osText) stageOsText(event.target, false);
});
app.addEventListener('keydown', (event) => {
  const id = event.target.dataset.osText;
  if (!id || event.isComposing || event.keyCode === 229 || osTextDrafts.get(id)?.composing) return;
  if (event.key === 'Escape') {
    event.preventDefault();
    event.stopImmediatePropagation();
    osTextDrafts.delete(id);
    render();
  } else if (event.key === 'Enter' && (event.ctrlKey || event.metaKey)) {
    event.preventDefault();
    event.stopImmediatePropagation();
    void commitOsText(id, event.target.value);
  }
});
async function openOsOutlinePicker(nodeId, mode = 'bake') {
  if (!desktop || editorPicker || osOutlinePending || current().locked || deletionBusy()) return;
  if (osTextDrafts.has(nodeId)) {
    notify('Apply or cancel the Text edit first', true);
    return;
  }
  if (!flushArrangementText(nodeId)) return;
  const source = current().nodes.find((n) => n.id === nodeId);
  try {
    if (source?.type !== 'Text' || source.locked || source.bypassed)
      throw Error('Select an unlocked enabled Text');
    // The selected backend validates the complete source after a face is chosen.
  } catch (error) {
    notify(error.message, true);
    return;
  }
  const owner = session,
    envelope = session.state,
    layerId = current().id;
  const request = { preparing: false };
  osOutlinePending = request;
  const valid = () =>
    session === owner &&
    current().id === layerId &&
    selected === nodeId &&
    session.state.expectedRevision === envelope.expectedRevision;
  let picker;
  try {
    notify('Finding active OS fonts…');
    const catalog = await readCurrentOsFontCatalog();
    const families = catalog.families || [],
      faces = catalog.faces || [];
    if (osOutlinePending !== request || !valid())
      throw Error('Selection changed; reopen OS font outlines');
    const labels = ['Regular', 'Bold', 'Italic', 'Bold Italic'];
    const items = families
      .filter((f) => !faces.some((face) => face.family === f.family))
      .flatMap(({ family, styles }) =>
        styles.map((style) => ({
          id: `${family}:${style}`,
          family,
          style,
          label: `${family} · OS ${labels[style]}`,
          metadata: { family },
        })),
      );
    items.unshift(
      ...faces
        .filter((face) => !face.color)
        .map((face) => ({
          id: `dw:${face.family}:${face.name}:${face.weight}:${face.style}:${face.stretch}`,
          family: face.family,
          face,
          label: `${face.family} · ${face.name} · ${face.weight}`,
          metadata: {
            family: face.family,
            subfamily: face.name,
            weightClass: face.weight,
            italic: face.style !== 0,
          },
        })),
    );
    if (!items.length)
      throw Error(
        catalog.familiesError || catalog.facesError
          ? 'Font check failed; refresh fonts'
          : 'No OS outline fonts are active',
      );
    picker = editorPicker = openSearchPanel({
      title: 'OS font outlines',
      items,
      action: mode === 'edit' ? 'Use font for Text' : 'Convert to paths',
      fallbackFocus: () => document.querySelector('.canvas-stage')?.focus(),
      onCancel: () => {
        if (osOutlinePending === request) osOutlinePending = null;
        if (editorPicker === picker) editorPicker = null;
      },
      onCommit: async (item) => {
        if (request.preparing || picker.closed) return;
        request.preparing = true;
        picker.status('Preparing outlines; Escape cancels…', false);
        try {
          if (!valid()) throw Error('Document or selection changed; reopen OS font outlines');
          const outline = item.face
            ? await prepareDesktopText(source, item.face)
            : await outlineOsText({
                family: item.family,
                style: item.style,
                ...osTextIntent(source, { allowEmpty: mode === 'edit' }),
              });
          if (picker.closed || osOutlinePending !== request) return;
          if (picker.activeId !== item.id) throw Error('Font choice changed; convert again');
          if (
            item.face
              ? JSON.stringify(outline.face) !== JSON.stringify(item.face)
              : outline.family !== item.family || outline.style !== item.style
          )
            throw Error('Unmatched OS font response; no outlines were applied');
          if (!valid()) throw Error('Document or selection changed; OS outlines were not applied');
          const result = commandChange(
            mode === 'edit' ? 'set-os-font-text' : 'bake-os-text-outline',
            { nodeId, outline },
            mode === 'edit'
              ? `${item.label} selected`
              : `${item.label} converted to paths; no font data saved`,
            false,
            envelope,
          );
          if (result) {
            characterEdit = null;
            pointSelection = { nodeId: null, ids: new Set() };
            picker.close();
            editorPicker = null;
            osOutlinePending = null;
            render();
          } else
            picker.status('Conversion refused; document preserved. Close and repair the source.');
        } catch (error) {
          picker.status(error.message || String(error));
        } finally {
          request.preparing = false;
        }
      },
    });
  } catch (error) {
    if (osOutlinePending === request) osOutlinePending = null;
    notify(error.message || String(error), true);
  }
}
async function loadInstalledFont(id, nodeId, characters) {
  if (!id) return;
  if (current().locked) {
    notify('Composition is locked', true);
    return;
  }
  const entry = systemFonts.find(([key]) => key === id);
  if (!entry || entry[2]) {
    notify('Installed font unavailable', true);
    return;
  }
  const request = (fontRequest = {
    id: ++fontRequestId,
    layer: current().id,
    node: nodeId,
    ...(characters ? { characters } : {}),
    before: readDocument(),
    commandState: session.state,
  });
  notify(`Loading ${entry[1]}…`);
  render();
  try {
    const bytes = await readSystemFont(id);
    commitFont(bytes, entry[1], request);
  } catch (err) {
    if (fontRequest?.id === request.id)
      notify(`Font rejected; current document preserved: ${err.message || err}`, true);
  } finally {
    if (fontRequest?.id === request.id) {
      fontRequest = null;
      render();
    }
  }
}
app.addEventListener(
  'cancel',
  (e) => {
    if (e.target.id === 'font-file' && String(fontRequest?.id) === e.target.dataset.fontRequest) {
      fontRequest = null;
      render();
    }
  },
  true,
);
function textDraft(el) {
  const n = current().nodes.find((n) => n.id === el.dataset.text);
  if (!n || n.variation) return null;
  let draft = textDrafts.get(n.id);
  if (
    draft &&
    draft.envelope.expectedRevision !== session.state.expectedRevision &&
    el.value === n.text &&
    !draft.composition
  ) {
    session.dispatch({
      ...session.state,
      type: 'cancel-text-draft',
      payload: { draftId: draft.state.draftId },
    });
    textDrafts.delete(n.id);
    draft = null;
  }
  if (!draft) {
    const envelope = session.state;
    try {
      draft = {
        saved: n.text,
        envelope,
        state: session.dispatch({
          ...envelope,
          type: 'begin-text-draft',
          payload: { layerId: current().id, nodeId: n.id },
        }),
        capture: null,
        composition: null,
      };
    } catch (error) {
      notify(error.message, true);
      return null;
    }
    textDrafts.set(n.id, draft);
  }
  return draft;
}
function beginCanvasTextEdit(layerId, event) {
  if (!graphemeEditingAvailable) {
    notify('Text editing requires grapheme segmentation support in this runtime', true);
    return false;
  }
  const layer = history.doc.layers.find((l) => l.id === layerId);
  if (!layer || layer.locked || !layerShown(layer) || session.hasTextComposition) return false;
  const result = evaluate(layer, fonts, history.doc.imageAssets);
  const occurrences = characterOccurrences(result.path).filter((o) => !o.reason);
  const point = canvasPoint(event);
  const hits = occurrences.filter((o) => {
    try {
      return result.textLayout(o.source).characters.some((g) => hitCharacter(g, o.matrix, point));
    } catch {
      return false;
    }
  });
  const sources = [...new Set((hits.length ? hits : occurrences).map((o) => o.source))];
  if (sources.length !== 1) return false;
  const node = layer.nodes.find((n) => n.id === sources[0] && n.type === 'Text');
  if (!node || node.locked || node.bypassed) return false;
  activeLayer = layerId;
  artworkTarget = null;
  artworkSelection = new Set();
  selected = node.id;
  selection = new Set([node.id]);
  characterEdit = null;
  const occurrence = (hits.length ? hits : occurrences).find((o) => o.source === node.id);
  canvasTextEdit = { layerId, nodeId: node.id, route: occurrence.route, occurrence };
  render();
  const stage = app.querySelector('.canvas-stage');
  const editor = document.createElement('textarea');
  editor.className = 'canvas-text-editor';
  editor.dataset.text = node.id;
  editor.setAttribute('aria-label', 'Edit canvas text');
  editor.title = 'Ctrl+Enter to finish; Escape to cancel';
  editor.value = node.text;
  stage.append(editor);
  let anchor;
  const selectAt = (event, extend) => {
    const context = canvasTextContext();
    if (!context) return;
    const index = nearestTextCaret(context.geometry, canvasPoint(event));
    if (!extend) anchor = index;
    else anchor ??= editor.selectionStart;
    editor.setSelectionRange(
      Math.min(anchor, index),
      Math.max(anchor, index),
      index < anchor ? 'backward' : 'forward',
    );
    updateCanvas(true);
  };
  editor.onpointerdown = (event) => {
    if (event.button !== 0) return;
    event.preventDefault();
    event.stopPropagation();
    editor.focus({ preventScroll: true });
    selectAt(event, event.shiftKey);
    editor.setPointerCapture(event.pointerId);
  };
  editor.onpointermove = (event) => {
    if (!editor.hasPointerCapture(event.pointerId)) return;
    event.stopPropagation();
    selectAt(event, true);
  };
  editor.onpointerup = (event) => {
    if (editor.hasPointerCapture(event.pointerId)) editor.releasePointerCapture(event.pointerId);
    event.stopPropagation();
  };
  editor.onselect = editor.onkeyup = () => updateCanvas(true);
  editor.focus({ preventScroll: true });
  selectAt(event, false);
  positionCanvasTextEditor();
  return true;
}
function canvasTextContext(result = renderEvaluation) {
  // Geometry must read the displayed draft; current() deliberately owns committed authoring targets.
  if (!canvasTextEdit || result?.error || displayLayer().id !== canvasTextEdit.layerId) return null;
  const n = displayLayer().nodes.find((n) => n.id === canvasTextEdit.nodeId);
  if (!n) return null;
  const occurrence =
    characterOccurrences(result.path).find((o) => o.route === canvasTextEdit.route) ||
    canvasTextEdit.occurrence;
  const defaults = {
    ...result.textDefaults(n.id),
    position: result.objectResizeState(n.id).position,
  };
  return {
    n,
    defaults,
    geometry: textEditGeometry(n, result.textLayout(n.id), occurrence, fonts, defaults),
  };
}
document.addEventListener('selectionchange', () => {
  if (document.activeElement?.matches('.canvas-text-editor')) updateCanvas();
});
function canvasTextOverlay(result) {
  const context = canvasTextContext(result),
    input = app.querySelector('.canvas-text-editor');
  if (!context || !input) return '';
  const { selectionStart: start, selectionEnd: end } = input;
  const caretIndex = input.selectionDirection === 'backward' ? start : end;
  const caret =
    context.geometry.carets.filter((c) => c.index <= caretIndex).at(-1) ||
    context.geometry.carets[0];
  const selected = context.geometry.selections.filter((g) => g.end > start && g.start < end);
  return `<g class="canvas-text-selection" aria-hidden="true">${selected.map((g) => `<polygon points="${g.points.map((p) => p.join(',')).join(' ')}"/>`).join('')}${caret ? `<path class="canvas-text-caret" data-caret-index="${caret.index}" d="M${caret.points[0].join(' ')}L${caret.points[1].join(' ')}"/>` : ''}</g>`;
}
const editorFonts = new Map();
function positionCanvasTextEditor() {
  const editor = app.querySelector('.canvas-text-editor'),
    context = editor && canvasTextContext();
  if (!context) return;
  const svg = app.querySelector('.artboard'),
    ctm = svg.getScreenCTM();
  if (!ctm) return;
  const stage = app.querySelector('.canvas-stage').getBoundingClientRect();
  const points = [
    ...context.geometry.carets.flatMap((c) => c.points),
    ...context.geometry.selections.flatMap((g) => g.points),
  ].map((p) => new DOMPoint(...p).matrixTransform(ctm));
  if (!points.length) return;
  const left = Math.min(...points.map((p) => p.x)),
    top = Math.min(...points.map((p) => p.y));
  Object.assign(editor.style, {
    left: left - stage.left - 3 + 'px',
    top: top - stage.top - 3 + 'px',
    width: Math.max(12, Math.max(...points.map((p) => p.x)) - left + 6) + 'px',
    height: Math.max(12, Math.max(...points.map((p) => p.y)) - top + 6) + 'px',
    fontSize: context.defaults.size * canvasScale() + 'px',
    lineHeight:
      (context.n.leading?.mode === 'manual'
        ? context.n.leading.value
        : context.defaults.size * context.defaults.lineSpacing) *
        canvasScale() +
      'px',
    letterSpacing: context.defaults.tracking * canvasScale() + 'px',
    writingMode: context.n.writingMode || 'horizontal-tb',
  });
  // Same local face for the native IME caret. Outlines/caret geometry remain authoritative.
  const fontId = context.n.font;
  if (typeof FontFace === 'function') {
    const asset = history.doc.fontAssets?.[fontId];
    if (!editorFonts.has(fontId) || editorFonts.get(fontId).data !== asset?.data) {
      const old = editorFonts.get(fontId);
      if (old) document.fonts.delete(old.face);
      const family = 'NectEditor_' + fontId.replace(/[^a-zA-Z0-9_]/g, '_');
      const source = asset
        ? Uint8Array.from(atob(asset.data), (c) => c.charCodeAt(0)).buffer
        : 'url(/fonts/DejaVuSans.ttf)';
      const face = new FontFace(family, source);
      editorFonts.set(fontId, {
        family,
        face,
        data: asset?.data,
        ready: face
          .load()
          .then((loaded) => {
            if (editorFonts.get(fontId)?.face === loaded) document.fonts.add(loaded);
          })
          .catch(() => {}),
      });
    }
    editor.style.fontFamily = editorFonts.get(fontId).family;
  }
}
function compactTextInspector() {
  const inspector = app.querySelector('.inspector-body');
  if (selectedNode()?.type !== 'Text' || !inspector) return;
  const icons = {
    writingMode: ['T→', 'T↓'],
    textAlignment: null,
    crossAlignment: null,
    referenceBounds: ['□', '↔'],
  };
  for (const [key, glyphs] of Object.entries(icons)) {
    const attr = key.replace(/[A-Z]/g, (letter) => '-' + letter.toLowerCase());
    const select = inspector.querySelector(`select[data-${attr}]`);
    if (!select) continue;
    const field = select.closest('.field');
    field.classList.add('text-symbol-field');
    const label = field.querySelector('label');
    const axis =
      key === 'textAlignment'
        ? selectedNode().writingMode === 'vertical-rl'
          ? 'Y'
          : 'X'
        : key === 'crossAlignment'
          ? selectedNode().writingMode === 'vertical-rl'
            ? 'X'
            : 'Y'
          : null;
    label.innerHTML = `<span title="${esc(label.textContent)}">${axis || (key === 'writingMode' ? 'T' : '□')}</span>`;
    select.classList.add('visually-hidden');
    select.tabIndex = -1;
    select.setAttribute('aria-hidden', 'true');
    const row = document.createElement('div');
    row.className = 'text-symbol-buttons';
    row.setAttribute('role', 'group');
    row.setAttribute('aria-label', select.getAttribute('aria-label'));
    [...select.options].forEach((option, index) => {
      const button = document.createElement('button');
      button.type = 'button';
      button.dataset.textSymbol = key;
      button.dataset.textSymbolValue = option.value;
      button.setAttribute(
        'aria-label',
        `${select.getAttribute('aria-label')}: ${option.textContent}`,
      );
      button.setAttribute('aria-pressed', String(select.value === option.value));
      button.classList.toggle('active', select.value === option.value);
      button.title = button.getAttribute('aria-label');
      button.disabled = current().locked;
      button.innerHTML = glyphs
        ? glyphs[index]
        : option.value === 'origin'
          ? '⊥'
          : alignmentIcon(
              (axis === 'Y' ? ['top', 'center-y', 'bottom'] : ['left', 'center-x', 'right'])[
                ['start', 'center', 'end'].indexOf(option.value)
              ],
            );
      button.onclick = () => {
        select.value = option.value;
        select.dispatchEvent(new Event('change', { bubbles: true }));
      };
      row.append(button);
    });
    field.append(row);
  }
  const symbols = { position: '↔', size: 'T↕', letterSpacing: 'VA', lineSpacing: 'A↕' };
  for (const field of inspector.querySelectorAll('.field[data-node][data-key]')) {
    const symbol = symbols[field.dataset.key];
    if (!symbol) continue;
    const label = field.querySelector('label');
    if (!label) continue;
    label.title = label.textContent;
    label.innerHTML = `<span aria-hidden="true">${symbol}</span><span class="visually-hidden">${esc(label.title)}</span>`;
    field.classList.add('text-numeric-field');
  }
}
function stageTextInput(el, transaction) {
  const draft = textDraft(el);
  if (!draft) return false;
  try {
    draft.state = session.dispatch({
      ...draft.envelope,
      type: 'stage-text-draft',
      payload: {
        draftId: draft.state.draftId,
        expectedDraftRevision: draft.state.draftRevision,
        action: transaction?.composition ? 'composition-end' : 'edit',
        previousRaw: transaction?.previousRaw ?? draft.state.text,
        nextRaw: el.value,
        selectionStart: transaction?.selectionStart,
        selectionEnd: transaction?.selectionEnd,
        inputType: transaction?.inputType,
      },
    });
    textPreview = session.textDraftSnapshot(draft.state.draftId).document;
    textPreviewRevision = draft.envelope.expectedRevision;
    for (const field of app.querySelectorAll('textarea[data-text]'))
      if (field !== el && field.dataset.text === el.dataset.text) field.value = el.value;
    updateCanvas(true);
    return true;
  } catch (error) {
    notify(error.message, true);
    return false;
  }
}
app.addEventListener(
  'select',
  (e) => {
    const el = e.target;
    if (el.matches?.('textarea[data-text]'))
      textRanges.set(textRangeKey(el), {
        raw: el.value,
        start: el.selectionStart,
        end: el.selectionEnd,
      });
  },
  true,
);
app.addEventListener('pointerdown', (e) => {
  if (e.target.closest('[data-style-text-range], [data-kern-text-cursor]')) e.preventDefault();
});

// Pointer input only chooses a visual insertion slot; Session owns the edit.
installLayerReorder({
  app,
  getState: () => ({
    document: history.doc,
    envelope: session.state,
    busy: deletionBusy() || session.hasGesture,
  }),
  commit(payload, envelope) {
    const preview = structuredClone(history.doc);
    if (!moveArtworkLayerBefore(preview, payload.layerId, payload.beforeLayerId).moved) return;
    const priorUI = selectionSnapshot(true);
    session.dispatch({ ...envelope, type: 'reorder-layer', payload });
    selectionHistory.set(history.past.at(-1), priorUI);
    selectionHistory.set(history.doc, selectionSnapshot(true));
    lastNameClick = null;
    notify('Layer reordered');
    render();
    document
      .querySelector('.layers .layer[data-layer="' + payload.layerId + '"]')
      ?.focus({ preventScroll: true });
  },
  onError: (error) => notify(error.message, true),
});
// A refused row action must not blur/commit an inline name or another live edit.
app.addEventListener(
  'pointerdown',
  (e) => {
    if (e.target.closest('[data-order-layer]') && deletionBusy()) e.preventDefault();
  },
  true,
);
app.addEventListener('dblclick', (e) => {
  const title = e.target.closest('[data-rename-node], [data-select-layer]');
  if (title && !nameEdit)
    beginNameEdit(title.dataset.selectLayer || current().id, title.dataset.renameNode || null);
});
app.addEventListener('focusout', (e) => {
  if (e.target.matches('[data-rename-name]') && nameEdit) finishNameEdit(true, true);
  if (e.target.matches('.canvas-text-editor') && canvasTextEdit) {
    e.target.dispatchEvent(new Event('change', { bubbles: true }));
  }
});
app.addEventListener('beforeinput', (e) => {
  if (e.target.matches('textarea[data-text],textarea[data-os-text]')) {
    if (!graphemeEditingAvailable) {
      e.preventDefault();
      notify('Text editing requires grapheme segmentation support in this runtime', true);
      return;
    }
    const el = e.target;
    const range = graphemeDeletionRange(el.value, {
      start: el.selectionStart,
      end: el.selectionEnd,
      inputType: e.inputType,
      composing: composing || e.isComposing || !!textDrafts.get(el.dataset.text)?.composition,
    });
    // Extend only unsafe deletions; keep native input and its Undo history.
    if (
      !e.defaultPrevented &&
      range &&
      (el.selectionStart !== el.selectionEnd ||
        (range.start < el.selectionStart && el.selectionStart < range.end) ||
        [...el.value.slice(range.start, range.end)].length > 1)
    )
      el.setSelectionRange(range.start, range.end);
  }
  if (!e.target.matches('textarea[data-text]')) return;
  const el = e.target,
    draft = textDraft(el);
  if (!draft) return;
  draft.capture = {
    previousRaw: el.value,
    selectionStart: el.selectionStart,
    selectionEnd: el.selectionEnd,
    inputType: e.inputType,
    isComposing: e.isComposing,
  };
});
const nativeTextMovements = new WeakMap();
const nativeTextSelection = (el) => ({
  start: el.selectionStart,
  end: el.selectionEnd,
  selectionDirection: el.selectionDirection,
});
const nativeTextFocus = (range) =>
  range.selectionDirection === 'backward' ? range.start : range.end;
function settleNativeTextMovement(el) {
  const before = nativeTextMovements.get(el);
  nativeTextMovements.delete(el);
  if (
    !before ||
    !el.isConnected ||
    el.value !== before.raw ||
    composing ||
    textDrafts.get(el.dataset.text)?.composition
  )
    return;
  const after = nativeTextSelection(el),
    delta = nativeTextFocus(after) - nativeTextFocus(before);
  if (!delta) return;
  const correction = graphemeMovementCorrection(el.value, {
    ...after,
    logicalDirection: delta < 0 ? 'backward' : 'forward',
    extend: before.extend,
  });
  if (correction)
    el.setSelectionRange(correction.start, correction.end, correction.selectionDirection);
  if (el.matches('.canvas-text-editor')) updateCanvas(true);
}
app.addEventListener('keydown', (e) => {
  const el = e.target;
  if (!el.matches('textarea[data-text],textarea[data-os-text]')) return;
  settleNativeTextMovement(el);
  if (
    !graphemeEditingAvailable ||
    e.defaultPrevented ||
    e.isComposing ||
    e.keyCode === 229 ||
    composing ||
    textDrafts.get(el.dataset.text)?.composition ||
    e.ctrlKey ||
    e.metaKey ||
    e.altKey ||
    !['ArrowLeft', 'ArrowRight', 'ArrowUp', 'ArrowDown'].includes(e.key)
  )
    return;
  const before = { ...nativeTextSelection(el), raw: el.value, extend: e.shiftKey };
  nativeTextMovements.set(el, before);
  // Run after each native default action, including held-key repeats.
  setTimeout(() => {
    if (nativeTextMovements.get(el) === before) settleNativeTextMovement(el);
  }, 0);
});
app.addEventListener('keyup', (e) => {
  if (e.target.matches('textarea[data-text],textarea[data-os-text]'))
    settleNativeTextMovement(e.target);
});
app.addEventListener('input', (e) => {
  if (e.target.id === 'svg-export-preset-name') {
    svgExportPresetName = e.target.value;
    if (e.target.value.trim() !== (selectedSvgExportPreset || ''))
      clearExportPresetSelection('svg');
    updateExportPresetSaveLabel('svg');
    return;
  }
  if (e.target.id === 'png-export-preset-name') {
    pngExportPresetName = e.target.value;
    if (e.target.value.trim() !== (selectedPngExportPreset || ''))
      clearExportPresetSelection('png');
    updateExportPresetSaveLabel('png');
    return;
  }
  if (e.target.id === 'png-ppi') {
    clearExportPresetSelection('png');
    pngPpi = Number(e.target.value);
    updatePngExportPreview();
    return;
  }
  if (e.target.matches('[data-grid-opacity]')) {
    const value = e.target.value.trim() ? Number(e.target.value) / 100 : NaN;
    if (Number.isFinite(value) && value >= 0 && value <= 1) previewGridOpacity(value);
    return;
  }
  if (e.target.dataset.variableInstance) {
    void selectVariableInstance(e.target, true);
    return;
  }
  if (e.target.dataset.variableAxis) {
    const el = e.target,
      n = current().nodes.find((n) => n.id === el.dataset.variableNode);
    if (!n?.variation || !el.value.trim() || !Number.isFinite(Number(el.value))) return;
    void preparedChange(
      'set-variable-font',
      {
        nodeId: n.id,
        fontId: n.font,
        axes: { ...n.variation.axes, [el.dataset.variableAxis]: Number(el.value) },
      },
      undefined,
      session.state,
      true,
    );
    return;
  }
  if (e.target.matches('[data-align-spacing]')) {
    editAlignmentSpacing(e.target);
    return;
  }
  if (e.target.matches('[data-rename-name]')) {
    if (nameEdit) nameEdit.value = e.target.value;
    return;
  }
  if (!e.target.matches('textarea[data-text]')) return;
  const draft = textDraft(e.target);
  if (!draft || draft.composition || e.isComposing) return;
  stageTextInput(e.target, draft.capture || { inputType: e.inputType });
  draft.capture = null;
});
app.addEventListener('compositionstart', (e) => {
  if (e.target.dataset.text && variableNode(e.target.dataset.text)) {
    composing = true;
    return;
  }
  if (e.target.matches('[data-rename-name]')) {
    if (nameEdit) nameEdit.composing = true;
    return;
  }
  if (!e.target.matches('textarea[data-text]')) return;
  const el = e.target,
    draft = textDraft(el);
  if (!draft) return;
  draft.composition = {
    previousRaw: el.value,
    selectionStart: el.selectionStart,
    selectionEnd: el.selectionEnd,
  };
  try {
    draft.state = session.dispatch({
      ...draft.envelope,
      type: 'stage-text-draft',
      payload: {
        draftId: draft.state.draftId,
        expectedDraftRevision: draft.state.draftRevision,
        action: 'composition-start',
        ...draft.composition,
      },
    });
  } catch (error) {
    notify(error.message, true);
  }
});
app.addEventListener('compositionend', (e) => {
  if (e.target.dataset.text && variableNode(e.target.dataset.text)) {
    composing = false;
    return;
  }
  if (e.target.matches('[data-rename-name]')) {
    if (nameEdit) {
      nameEdit.value = e.target.value;
      nameEdit.composing = false;
      if (nameEdit.pendingCommit) finishNameEdit(true, true);
      else if (needsRender) scheduleRender();
    }
    return;
  }
  if (!e.target.matches('textarea[data-text]')) return;
  const draft = textDraft(e.target);
  if (!draft) return;
  const initial = draft.composition;
  draft.composition = null;
  stageTextInput(e.target, {
    ...initial,
    inputType: 'insertCompositionText',
    composition: true,
    isComposing: false,
  });
  draft.capture = null;
  if (draft.pendingCommit) {
    draft.pendingCommit = false;
    e.target.dispatchEvent(new Event('change', { bubbles: true }));
  }
});

app.addEventListener('change', async (e) => {
  let el = e.target,
    d = el.dataset;
  if (el.id === 'svg-export-preset' || el.id === 'png-export-preset') {
    const format = el.id === 'svg-export-preset' ? 'svg' : 'png';
    if (!el.value) {
      clearExportPresetSelection(format);
      if (format === 'svg') svgExportPresetName = '';
      else pngExportPresetName = '';
      const nameInput = app.querySelector('#' + format + '-export-preset-name');
      if (nameInput) nameInput.value = '';
      updateExportPresetSaveLabel(format);
      return;
    }
    try {
      applySavedExportPreset(format, el.value);
    } catch (reason) {
      clearExportPresetSelection(format);
      el.value = '';
      notify(reason?.message || 'Export preset could not be applied', true);
    }
    return;
  }
  if (d.artboardUnit !== undefined) {
    if (!ARTBOARD_UNITS.includes(el.value)) return;
    artboardUnit = el.value;
    updateArtboardUnitDisplay();
    return;
  }
  if (d.svgExportUnit !== undefined) {
    if (!ARTBOARD_UNITS.includes(el.value)) return;
    clearExportPresetSelection('svg');
    svgExportUnit = el.value;
    return;
  }
  if (d.svgExportSizing !== undefined) {
    if (!['fixed', 'responsive'].includes(el.value)) return;
    clearExportPresetSelection('svg');
    svgExportSizing = el.value;
    const units = app.querySelector('#svg-export-unit');
    if (units) units.hidden = svgExportSizing !== 'fixed';
    return;
  }
  if (d.svgExportPrecision !== undefined) {
    if (el.value !== 'default' && !SVG_EXPORT_PRECISIONS.includes(Number(el.value))) return;
    clearExportPresetSelection('svg');
    svgExportPrecision = el.value;
    return;
  }
  if (d.osText) {
    if (!e.isComposing) await commitOsText(d.osText, el.value);
    return;
  }
  if (d.arrangementGap) {
    const n = current().nodes.find((n) => n.id === d.arrangementGap);
    if (!n?.characterArrangement) return;
    if (!flushArrangementText(n.id, el.closest('.text-controls'))) return;
    commandChange(
      'set-character-arrangement',
      { nodeId: n.id, gap: Number(el.value), updateGap: true },
      'Maintained ink gap updated',
    );
    return;
  }
  if (d.alignReference !== undefined || d.alignSpacing !== undefined) {
    if (d.alignSpacing !== undefined) {
      editAlignmentSpacing(el, true);
      return;
    }
    if (d.alignReference !== undefined) {
      if (!['selection', 'artboard'].includes(el.value)) return;
      alignmentPreferences.reference = el.value;
    }
    saveAlignmentPreferences();
    render();
    return;
  }
  if (penDraft && el.closest('[inert], #add-node')) return;
  if (penDraft && !finishPenDraft()) return;
  if (d.renameName !== undefined) return;
  if (d.text && textDrafts.get(d.text)?.composition) {
    textDrafts.get(d.text).pendingCommit = true;
    return;
  }
  if (drag?.type === 'numeric' && drag.started && el === drag.capture) return;
  if (d.handlePolar) {
    try {
      if (el.disabled || !el.value.trim()) throw Error('Expected an editable finite handle value');
      // Display is rounded; Enter on an untouched field must not quantize geometry.
      if (el.value === el.defaultValue) return;
      const result = commitHandlePolar(
        session,
        {
          kind: 'curve-point',
          layerId: current().id,
          nodeId: d.node,
          pointId: d.point,
          key: d.handle,
        },
        d.handlePolar,
        Number(el.value),
      );
      const pt = current()
        .nodes.find((n) => n.id === d.node)
        .points.find((p) => p.id === d.point);
      el.value = String(Number(handlePolar(pt[d.handle].value)[d.handlePolar].toFixed(3)));
      el.defaultValue = el.value;
      if (result.changed) notify('Handle updated');
    } catch (error) {
      el.value = el.defaultValue;
      notify(error.message, true);
    }
    scheduleRender();
    return;
  }
  if (d.characterInstance !== undefined) {
    if (characterEdit) characterEdit.route = el.value || null;
    render();
    return;
  }
  if (d.characterSelection !== undefined) {
    if (characterEdit)
      characterEdit.selected = new Set([...el.selectedOptions].map((o) => o.value));
    render();
    return;
  }
  if (d.characterKerning !== undefined) {
    const context = characterContext(),
      pair = pairContext(context);
    if (!matchesPairInput(el, context, pair)) return;
    if (!el.value.trim()) {
      notify('Enter a value or choose Inherit pair', true);
      return;
    }
    commandChange(
      'set-character-kerning',
      { nodeId: context.n.id, changes: { [pair.key]: Number(el.value) } },
      'Pair kerning updated',
      true,
    );
    // A committed or rejected edit is no longer a draft. In particular, do not
    // let focus restoration carry a rejected path-overflow value into the render.
    const committed = selectedNode().characterPositions?.kerningPairs?.[pair.key];
    el.value = committed === undefined ? '' : String(committed);
    return;
  }
  if (d.characterStyle !== undefined) {
    const context = characterContext(),
      property = d.characterStyle;
    if (
      !context?.typographySelected.length ||
      context.typographyReason ||
      !CHARACTER_STYLE_KEYS.includes(property)
    )
      return;
    if (property === 'font' && el.value.startsWith('system-')) {
      await loadInstalledFont(el.value, context.n.id, [...characterEdit.selected]);
      return;
    }
    let value =
      property === 'font'
        ? el.value || null
        : Number(el.value) / (['width', 'height'].includes(property) ? 100 : 1);
    if (property !== 'font' && !el.value.trim()) {
      notify('Enter a value or choose Inherit', true);
      return;
    }
    commandChange(
      'set-character-style',
      {
        nodeId: selectedNode().id,
        characterIds: [...characterEdit.selected],
        patch: { [property]: value },
      },
      'Selected typography updated',
      true,
    );
    return;
  }
  if (d.characterTransform !== undefined) {
    const context = characterContext(),
      property = d.characterTransform,
      value = Number(el.value);
    if (
      !['rotation', 'scale'].includes(property) ||
      !el.value.trim() ||
      !Number.isFinite(value) ||
      (property === 'rotation' ? Math.abs(value) > 36000 : value < 0.01 || value > 100)
    ) {
      notify('Character rotation: -36000 to 36000 degrees; uniform scale: 0.01 to 100', true);
      return;
    }
    if (context?.selected.length && !context.layout.reason)
      commandChange(
        'set-character-transforms',
        {
          nodeId: context.n.id,
          characterIds: context.selected.map((g) => g.id),
          patch: { [property]: value },
        },
        'Character shapes updated',
        true,
      );
    return;
  }
  if (d.characterAxis !== undefined) {
    const context = characterContext(),
      value = Number(el.value),
      axis = Number(d.characterAxis);
    if (!el.value.trim() || !Number.isFinite(value) || Math.abs(value) > 100000) {
      notify('Character offset must be between -100000 and 100000 px', true);
      return;
    }
    if (context?.selected.length)
      commandChange(
        'set-character-offsets',
        {
          nodeId: context.n.id,
          changes: Object.fromEntries(
            context.selected.map((g) => [g.id, g.offset.map((v, i) => (i === axis ? value : v))]),
          ),
        },
        'Character positions updated',
        true,
      );
    return;
  }
  if (d.batchPoint) {
    const selected = remapBatch(d.node).selected;
    if (el.checked) selected.add(d.batchPoint);
    else selected.delete(d.batchPoint);
    scheduleRender();
    return;
  }
  if (d.batchAxis !== undefined) {
    const value = Number(el.value);
    if (Number.isFinite(value)) remapBatch(d.node).delta[Number(d.batchAxis)] = value;
    else notify('Expected a finite batch delta', true);
    return;
  }
  if (el.id === 'png-export-mode') {
    if (!['scale', 'ppi'].includes(el.value)) return;
    clearExportPresetSelection('png');
    pngExportMode = el.value;
    render();
    return;
  }
  if (el.id === 'png-ppi') {
    clearExportPresetSelection('png');
    pngPpi = Number(el.value);
    updatePngExportPreview();
    return;
  }
  if (el.id === 'png-scale') {
    clearExportPresetSelection('png');
    pngScale = Number(el.value);
    render();
    return;
  }
  if (el.id === 'image-file') {
    const file = el.files[0];
    if (!file) {
      imagePick = null;
      return;
    }
    const request = imagePick || {
      id: ++imageRequest,
      before: readDocument(),
      doc: history.doc,
      commandState: session.state,
    };
    imagePick = null;
    try {
      const asset = await prepareImage(file);
      if (request.id !== imageRequest) return;
      if (history.doc !== request.doc || readDocument() !== request.before)
        throw Error('Document changed while reading; import the image again');
      const created = session.dispatch({
        ...request.commandState,
        type: 'adopt-image',
        payload: { name: asset.name, png: asset.data },
      });
      activeLayer = created.layerId;
      selected = created.nodeId;
      artworkTarget = null;
      artworkSelection = new Set();
      selection = new Set([selected]);
      notify(`Imported ${asset.name} · embedded PNG; JSON keeps the editable image`);
      render(true);
    } catch (err) {
      notify(`Image rejected; current document preserved: ${err.message}`, true);
    }
    return;
  }
  if (d.font && el.value.startsWith('system-')) {
    await loadInstalledFont(el.value, d.font);
    return;
  }
  if (el.id === 'font-file') {
    const file = el.files[0],
      request = fontRequest;
    if (!request || (el.dataset.fontRequest && el.dataset.fontRequest !== String(request.id)))
      return;
    if (!file) {
      fontRequest = null;
      render();
      return;
    }
    try {
      if (file.size > MAX_FONT_BYTES) throw Error('Font exceeds 20 MB');
      if (!/\.(ttf|otf)$/i.test(file.name)) throw Error('Choose a static .ttf or .otf font');
      const bytes = await file.arrayBuffer();
      if (request.variable) {
        if (
          fontRequest?.id !== request.id ||
          readDocument() !== request.before ||
          activeLayer !== request.layer
        )
          throw Error('Document changed while reading; load the font again');
        const asset = makeVariableFontAsset(bytes, file.name, history.doc.fontAssets);
        await preparedChange(
          'set-variable-font',
          { layerId: request.layer, nodeId: request.node, fontId: asset.id, axes: {}, asset },
          'Variable font loaded · embedded once in saved JSON',
          request.commandState,
        );
      } else commitFont(bytes, file.name, request);
    } catch (err) {
      notify(`Font rejected; current document preserved: ${err.message}`, true);
    }
    if (fontRequest?.id === request.id) fontRequest = null;
    render();
    return;
  }
  if (el.id === 'svg-part-file') {
    const file = el.files[0],
      request = svgPartPick;
    svgPartPick = null;
    if (!file || !request) return;
    try {
      if (file.size > 10_000_000) throw Error('SVG exceeds 10 MB');
      adoptSVGPart(await file.text(), file.name, request);
    } catch (err) {
      notify(`SVG node rejected; current document preserved: ${err.message}`, true);
    }
    return;
  }
  if (el.id === 'open-file' || el.id === 'import-svg-file') {
    let file = el.files[0];
    if (!file) return;
    try {
      const request = ++openRequest,
        before = readDocument(),
        generation = documents.generation,
        envelope = session.state;
      if (file.size > (el.id === 'open-file' ? MAX_PROJECT_BYTES : 10_000_000))
        throw Error('File exceeds the supported size limit');
      const text = await file.text(),
        source = el.id === 'import-svg-file' ? readSVGSource(text) : null,
        next = source ? null : deserializeProject(text);
      if (next) await preflightImages(next);
      if (request !== openRequest || generation !== documents.generation) return;
      if (documents.hasDraft() || readDocument() !== before)
        throw Error('Document changed while reading; open the file again');
      if (el.id === 'open-file') await prepareOpenDocument(next, { saved: true });
      else {
        // Import is an undoable authored replacement; it has no source-file binding.
        resetTransient();
        const result = session.dispatch({ ...envelope, type: 'import-svg', payload: { source } });
        documents.replace();
        activeLayer = result.layerId;
        selected = result.nodeId;
        artworkTarget = null;
        artworkSelection = new Set();
      }
      notify(el.id === 'import-svg-file' ? 'SVG document imported' : 'Project restored');
      render(true);
    } catch (err) {
      notify(
        `${el.id === 'import-svg-file' ? 'SVG import' : 'Open'} rejected; current document preserved: ${err.message}`,
        true,
      );
    }
    return;
  }
  if (d.gridOpacity !== undefined) {
    const value = el.value.trim() ? Number(el.value) / 100 : NaN;
    gridOpacityDraft = null;
    if (value !== guideSettings(history.doc).gridOpacity)
      editGuides('set-guide-setting', { key: 'gridOpacity', value });
    return;
  }
  if (d.layoutField || d.layoutPreview !== undefined) {
    if (d.layoutField)
      layoutDraft = {
        ...layoutConfig(),
        [d.layoutField]: el.value.trim() ? Number(el.value) : NaN,
      };
    else layoutPreview = el.checked;
    if (pointerActive) scheduleRender();
    else render();
    return;
  }
  if (d.guideSetting || d.guidePosition) {
    if (d.guideSetting)
      editGuides('set-guide-setting', {
        key: d.guideSetting,
        value:
          d.guideSetting === 'gridSize' ? (el.value.trim() ? Number(el.value) : NaN) : el.checked,
      });
    else
      editGuides('set-guide', {
        guideId: d.guidePosition,
        key: 'position',
        value: el.value.trim() ? Number(el.value) : NaN,
      });
    return;
  }
  if (el.id === 'add-node') {
    if (el.value) addNode(el.value);
    return;
  }
  if (d.operation) {
    commandChange('set-operation', { nodeId: d.operation, operation: el.value });
    return;
  }
  if (d.pathSource !== undefined) {
    const field = el.closest('.field');
    commandChange(el.value ? 'connect' : 'disconnect', {
      sourceId: el.value,
      nodeId: field.dataset.node,
      key: field.dataset.key,
    });
    return;
  }
  if (d.text && variableNode(d.text)) {
    if (composing || e.isComposing) return;
    await preparedChange('set-text-content', { nodeId: d.text, text: el.value }, 'Text updated');
    return;
  }
  if (d.variableInstance) {
    await selectVariableInstance(el);
    return;
  }
  if (d.variableAxis) {
    const n = current().nodes.find((n) => n.id === d.variableNode);
    if (!n?.variation || !el.value.trim() || !Number.isFinite(Number(el.value))) {
      notify('Enter a finite axis value', true);
      return;
    }
    await preparedChange(
      'set-variable-font',
      {
        nodeId: n.id,
        fontId: n.font,
        axes: { ...n.variation.axes, [d.variableAxis]: Number(el.value) },
      },
      'Variable axis updated',
    );
    return;
  }
  if (d.text) {
    const draft = textDrafts.get(d.text);
    try {
      if (draft) {
        if (draft.state.text !== el.value && !stageTextInput(el, draft.capture)) return;
        session.dispatch({
          ...draft.envelope,
          type: 'commit-text-draft',
          payload: {
            draftId: draft.state.draftId,
            expectedDraftRevision: draft.state.draftRevision,
          },
        });
      } else if (
        current()
          .nodes.find((n) => n.id === d.text)
          ?.text.replace(/\r\n?/g, '\n') !== el.value
      )
        session.dispatch({
          ...session.state,
          type: 'set-text-content',
          payload: {
            layerId: current().id,
            nodeId: d.text,
            text: el.value,
          },
        });
      textDrafts.delete(d.text);
      textPreview = null;
      if (el.classList.contains('canvas-text-editor')) canvasTextEdit = null;
      notify('Updated');
    } catch (error) {
      notify(error.message, true);
    }
    scheduleRender();
    return;
  }
  const settingEntry = [
    ['font', 'font'],
    ['referenceBounds', 'referenceBounds'],
    ['crossAlignment', 'crossAlignment'],
    ['textAlignment', 'alignment'],
    ['textKerning', 'kerning'],
    ['textLigatures', 'fontFeatures'],
    ['writingMode', 'writingMode'],
  ].find(([key]) => d[key]);
  const textSettingId =
    d.textPath ||
    d.textPathMode ||
    d.textPathStart ||
    d.textPathReversed ||
    d.textWrap ||
    d.textWrapWidth ||
    d.textLeading ||
    d.textLeadingValue ||
    (settingEntry && d[settingEntry[0]]);
  if (textSettingId) {
    try {
      const n = current().nodes.find((n) => n.id === textSettingId && n.type === 'Text');
      if (!n) throw Error('Select a Text node');
      let settings;
      if (d.textPath || d.textPathMode || d.textPathStart || d.textPathReversed) {
        let attachment = n.pathAttachment ? { ...n.pathAttachment } : null;
        if (d.textPath) {
          if (!el.value) attachment = null;
          else {
            const [nodeId, contourId] = JSON.parse(el.value);
            attachment = {
              nodeId,
              contourId,
              startMode: 'normalized',
              start: { start: 0, center: 0.5, end: 1 }[n.alignment || 'start'],
              reversed: false,
            };
          }
        } else {
          if (!attachment) throw Error('Select a path contour first');
          if (d.textPathMode) {
            const length = evaluate(current(), fonts, history.doc.imageAssets).textLayout(n.id)
              .attachment.length;
            attachment.start *= el.value === 'normalized' ? 1 / length : length;
            attachment.startMode = el.value;
          }
          if (d.textPathStart) {
            if (!el.value.trim()) throw Error('Enter a finite start anchor');
            attachment.start = Number(el.value);
          }
          if (d.textPathReversed) attachment.reversed = el.checked;
        }
        settings = { pathAttachment: attachment };
      } else if (d.textWrap || d.textWrapWidth) {
        let wrap = null;
        if (d.textWrap) {
          if (!['off', 'width', 'basic'].includes(el.value)) throw Error('Invalid wrap mode');
          if (el.value === 'width') wrap = { width: wrapExtent(n.textWrap) ?? 320 };
          if (el.value === 'basic')
            wrap = basicTextWrap(n.writingMode, wrapExtent(n.textWrap) ?? 320);
        } else {
          if (!el.value.trim()) throw Error('Enter a finite wrap width');
          wrap = withWrapExtent(n.textWrap, Number(el.value));
        }
        settings = { textWrap: wrap };
      } else if (d.textLeading || d.textLeadingValue) {
        let leading;
        if (d.textLeading) {
          if (el.value === 'legacy') leading = null;
          else if (el.value === 'auto') leading = { mode: 'auto' };
          else if (el.value === 'manual')
            leading = { mode: 'manual', value: initialManualLeading(n) };
          else throw Error('Invalid leading mode');
        } else {
          if (n.leading?.mode !== 'manual' || !el.value.trim())
            throw Error('Enter a manual leading distance');
          leading = { mode: 'manual', value: Number(el.value) };
        }
        settings = { leading };
      } else if (d.textLigatures) {
        if (!['', 'on', 'off'].includes(el.value)) throw Error('Invalid ligature setting');
        settings = { fontFeatures: el.value ? { liga: el.value === 'on' } : null };
      } else
        settings = {
          [settingEntry[1]]: settingEntry[1] === 'kerning' ? el.checked : el.value,
          ...(settingEntry[1] === 'writingMode' && n.textWrap
            ? { textWrap: basicTextWrap(el.value, wrapExtent(n.textWrap)) }
            : {}),
        };
      commandChange('set-text-settings', { nodeId: n.id, settings }, undefined, true);
    } catch (error) {
      notify(error.message, true);
      scheduleRender();
    }
    return;
  }
  if (
    d.base !== undefined ||
    d.value !== undefined ||
    d.expression !== undefined ||
    d.color !== undefined
  ) {
    const field = el.closest('.field'),
      n = current().nodes.find((node) => node.id === field?.dataset.node),
      q = n && parameter(n, field.dataset.key);
    if (!q) return;
    const payload = { nodeId: n.id, key: field.dataset.key };
    try {
      if (d.expression !== undefined) {
        commandChange(
          'set-parameter-expression',
          { ...payload, expression: el.value },
          undefined,
          true,
        );
      } else {
        let value;
        if (d.base !== undefined && !el.value.trim()) throw Error('Expected a finite number');
        if (d.color !== undefined) {
          value = { ...(q.value || { hex: current().fill, alpha: 1 }) };
          if (d.color === 'alpha') {
            if (!el.value.trim()) throw Error('Alpha must be from 0 to 1');
            value.alpha = Number(el.value);
          } else value.hex = el.value;
        } else {
          value =
            q.type === 'Boolean'
              ? el.checked
              : Number(el.value) / Number(el.dataset.valueFactor || 1);
          if (
            q.type !== 'Boolean' &&
            !el.value.trim() &&
            (['Linear Gradient Fill', 'Radial Gradient Fill', 'Closed Path Offset'].includes(
              n.type,
            ) ||
              (n.type === 'Stroke' && field.dataset.key === 'width'))
          )
            throw Error('Expected a finite number');
        }
        if (
          n.osFont?.version >= 2 &&
          ['size', 'letterSpacing', 'lineSpacing'].includes(field.dataset.key) &&
          d.base === undefined
        ) {
          if (osTextDrafts.has(n.id)) throw Error('Apply or cancel the Text edit first');
          await commitOsText(n.id, n.text, {
            [field.dataset.key === 'letterSpacing' ? 'tracking' : field.dataset.key]: value,
          });
          return;
        }
        commandChange(
          d.base !== undefined
            ? 'set-parameter-base'
            : isTransform(n)
              ? 'set-transform-control'
              : 'set-parameter',
          {
            ...payload,
            value,
            ...(q.type === 'Vector2' ? { axis: Number(d.axis) } : {}),
          },
          undefined,
          true,
        );
      }
    } catch (error) {
      notify(error.message, true);
      scheduleRender();
    }
    return;
  }
  if (d.strokePreset) {
    const settings = {
      solid: { strokeDash: [] },
      dashed: { strokeDash: [12, 6], strokeCap: 'butt' },
      dotted: { strokeDash: [0, 8], strokeCap: 'round' },
    }[el.value];
    if (settings)
      commandChange('set-node-settings', { nodeId: d.strokePreset, settings }, undefined, true);
    return;
  }
  if (
    d.compositionTarget ||
    d.referenceTarget ||
    d.vectorPositionHandle ||
    d.strokeCap ||
    d.strokeJoin ||
    d.strokeDash ||
    d.positionReference ||
    d.layerName !== undefined ||
    d.dimension
  ) {
    try {
      if (d.compositionTarget) {
        commandChange(
          'set-composition-reference',
          { nodeId: d.compositionTarget, target: el.value ? { compositionId: el.value } : null },
          undefined,
          true,
        );
      } else if (d.referenceTarget) {
        const target = el.value ? JSON.parse(el.value) : null;
        commandChange(
          'set-reference',
          {
            nodeId: d.referenceTarget,
            target: target ? { nodeId: target[0], portId: target[1] } : null,
          },
          undefined,
          true,
        );
      } else if (d.layerName !== undefined)
        commandChange('rename', { name: el.value }, undefined, true);
      else if (d.dimension)
        commandChange(
          'set-document-settings',
          {
            key: d.dimension,
            value: displayLengthToDocumentUnits(el.value, d.dimensionUnit || 'px'),
          },
          undefined,
          true,
        );
      else {
        const nodeId =
            d.vectorPositionHandle ||
            d.strokeCap ||
            d.strokeJoin ||
            d.strokeDash ||
            d.positionReference,
          key = d.vectorPositionHandle
            ? 'canvasPositionHandle'
            : d.strokeCap
              ? 'strokeCap'
              : d.strokeJoin
                ? 'strokeJoin'
                : d.strokeDash
                  ? 'strokeDash'
                  : 'positionReference',
          value = d.vectorPositionHandle
            ? el.checked
            : d.strokeDash
              ? parseStrokeDash(el.value)
              : el.value;
        commandChange('set-node-settings', { nodeId, settings: { [key]: value } }, undefined, true);
      }
    } catch (error) {
      notify(error.message, true);
      scheduleRender();
    }
    return;
  }
  if (d.handleType)
    commandChange(
      'set-point-mode',
      { nodeId: d.node, pointId: d.handleType, mode: el.value },
      undefined,
      true,
    );
  else if (d.remap)
    commandChange(
      'edit-remap',
      {
        nodeId: d.node,
        action: 'set',
        pointId: d.remap,
        component: d.component,
        axis: Number(d.axis),
        value: Number(el.value),
      },
      undefined,
      true,
    );
});
function canvasPoint(event) {
  let svg = document.querySelector('.artboard'),
    pt = new DOMPoint(event.clientX, event.clientY);
  let r = pt.matrixTransform(svg.getScreenCTM().inverse());
  return [r.x, r.y];
}
let space = false;
const editingControl = (target) =>
  target.closest?.('input,textarea,select,[contenteditable]:not([contenteditable="false"])');
function navigationExcluded(target) {
  return editingControl(target) || target.closest?.('.character-floating,.guides-panel,dialog');
}
function navigationBusy() {
  return !!(
    drag ||
    wiring ||
    composing ||
    nameEdit?.composing ||
    [...textDrafts.values()].some((d) => d.composition)
  );
}
app.addEventListener(
  'compositionstart',
  () => {
    composing = true;
    space = false;
    updateNavigation();
  },
  true,
);
app.addEventListener(
  'compositionend',
  () => {
    composing = false;
    updateLayerDeletionControls();
    if (needsRender) scheduleRender();
  },
  true,
);
// Graph navigation stays in screen pixels; document positions remain world coordinates.
let graphFrame = null;
function stopGraphDrag() {
  if (graphFrame !== null) window.cancelAnimationFrame(graphFrame);
  graphFrame = null;
  if (drag?.capture?.hasPointerCapture?.(drag.pointerId))
    drag.capture.releasePointerCapture(drag.pointerId);
}
function captureGraphPointer(e) {
  drag.pointerId = e.pointerId;
  drag.capture = document.querySelector('.graph-scroll');
  drag.capture.focus({ preventScroll: true });
  drag.capture.setPointerCapture?.(e.pointerId);
}
function captureWirePointer(e) {
  const viewport = document.querySelector('.graph-scroll');
  try {
    viewport.setPointerCapture?.(e.pointerId);
    if (viewport.hasPointerCapture?.(e.pointerId)) wiring.capture = viewport;
  } catch {
    /* Pointer may already have ended before capture is delivered. */
  }
}
function releaseWirePointer(wire) {
  if (wire?.capture?.hasPointerCapture?.(wire.pointerId))
    wire.capture.releasePointerCapture(wire.pointerId);
}
function restoreMarqueeSelection(state) {
  restorePenSelection(state);
  characterEdit = state.characterEdit ? structuredClone(state.characterEdit) : null;
  selectedGuide = state.selectedGuide;
}
function moveGraphMarquee(e) {
  const d = drag;
  if (!d.started && Math.hypot(e.clientX - d.start[0], e.clientY - d.start[1]) < 4) return;
  d.started = true;
  const viewport = document.querySelector('.graph-scroll'),
    bounds = viewport.getBoundingClientRect();
  const end = graphPoint(
    Math.max(bounds.left, Math.min(bounds.left + viewport.clientWidth, e.clientX)),
    Math.max(bounds.top, Math.min(bounds.top + viewport.clientHeight, e.clientY)),
  );
  const min = d.startWorld.map((value, i) => Math.min(value, end[i]));
  const max = d.startWorld.map((value, i) => Math.max(value, end[i]));
  let overlay = document.querySelector('.graph-marquee');
  if (!overlay) {
    overlay = document.createElement('div');
    overlay.className = 'graph-marquee';
    overlay.setAttribute('aria-hidden', 'true');
    document.querySelector('.graph').append(overlay);
  }
  Object.assign(overlay.style, {
    left: min[0] + 'px',
    top: min[1] + 'px',
    width: max[0] - min[0] + 'px',
    height: max[1] - min[1] + 'px',
  });
  const hits = [...document.querySelectorAll('.graph .node')]
    .filter((card) => {
      const r = card.getBoundingClientRect(),
        low = graphPoint(r.left, r.top),
        high = graphPoint(r.right, r.bottom);
      return high[0] >= min[0] && low[0] <= max[0] && high[1] >= min[1] && low[1] <= max[1];
    })
    .map((card) => card.dataset.id);
  selection = new Set([...(d.additive ? d.beforeUI.selection : []), ...hits]);
  selected = selection.has(d.beforeUI.selected)
    ? d.beforeUI.selected
    : [...selection].at(-1) || null;
  artworkTarget = null;
  artworkSelection = new Set();
  characterEdit = null;
  selectedGuide = null;
  for (const card of document.querySelectorAll('.graph .node'))
    card.classList.toggle('selected', selection.has(card.dataset.id));
  updateNavigation();
}
function graphPoint(x, y) {
  const r = document.querySelector('.graph').getBoundingClientRect();
  return [(x - r.x) / graphZoom, (y - r.y) / graphZoom];
}
function beginGraphGesture(target) {
  try {
    return session.dispatch({ ...session.state, type: 'begin-gesture', payload: { target } });
  } catch (error) {
    notify(error.message, true);
    return null;
  }
}
function updateGraphGesture(update) {
  try {
    drag.gesture = session.dispatch({
      ...session.state,
      type: 'update-gesture',
      payload: { ...drag.gesture, update },
    });
    drag.preview = session.previewSnapshot(drag.gesture.gestureId).document;
    return drag.preview.layers.find((layer) => layer.id === current().id);
  } catch (error) {
    notify(error.message, true);
    cancelInteraction();
    return null;
  }
}
function updateCanvasGesture(target, update) {
  if (!drag.gesture) {
    drag.reservingGesture = true;
    drag.gesture = beginGraphGesture(target);
    delete drag.reservingGesture;
    if (!drag.gesture) {
      cancelInteraction();
      return false;
    }
  }
  return !!updateGraphGesture(update);
}
function moveGraphNodes() {
  if (drag?.type !== 'node' || current().locked) return;
  const point = graphPoint(...drag.pointer);
  const { delta, guides } = snapGraphDirection(
    drag.boxes.filter((box) => drag.originals.some((node) => node.id === box.id)),
    drag.boxes,
    point.map((value, axis) => value - drag.startWorld[axis]),
    graphZoom,
    drag.snapBypass,
    6,
    { state: drag, shift: drag.shiftKey },
  );
  const preview = updateGraphGesture({ delta });
  if (!preview) return;
  for (const original of drag.originals) {
    const n = preview.nodes.find((x) => x.id === original.id);
    const el = document.querySelector(`.node[data-id="${n.id}"]`);
    el.style.left = n.position[0] + 'px';
    el.style.top = n.position[1] + 'px';
  }
  updateGraphBounds();
  drawWires();
  const overlay = document.querySelector('.graph-snap-guides');
  overlay.innerHTML = guides
    .map((guide) => {
      if (guide.kind === 'direction')
        return `<line data-snap-kind="direction" x1="${guide.start[0]}" y1="${guide.start[1]}" x2="${guide.end[0]}" y2="${guide.end[1]}"/>`;
      const { axis, coordinate, start, end } = guide;
      return axis === 0
        ? `<line x1="${coordinate}" y1="${start - 12 / graphZoom}" x2="${coordinate}" y2="${end + 12 / graphZoom}"/>`
        : `<line x1="${start - 12 / graphZoom}" y1="${coordinate}" x2="${end + 12 / graphZoom}" y2="${coordinate}"/>`;
    })
    .join('');
}
function edgeVelocity(value, low, high) {
  const band = Math.min(64, (high - low) / 3);
  if (band <= 0) return 0;
  if (value < low + band) return -720 * Math.min(1, (low + band - value) / band) ** 2;
  if (value > high - band) return 720 * Math.min(1, (value - high + band) / band) ** 2;
  return 0;
}
function graphAutoPan(time) {
  graphFrame = null;
  const motion = drag?.type === 'node' ? drag : wiring;
  if (!motion) return;
  if (current().locked) {
    cancelInteraction();
    return;
  }
  const dt = Math.min(0.05, Math.max(0, (time - (motion.frameTime ?? time)) / 1000));
  motion.frameTime = time;
  const viewport = document.querySelector('.graph-scroll'),
    r = viewport.getBoundingClientRect();
  const vx = edgeVelocity(motion.pointer[0], r.left, r.left + viewport.clientWidth),
    vy = edgeVelocity(motion.pointer[1], r.top, r.top + viewport.clientHeight);
  // Grow ahead of the pointer, including when the node itself is beyond the viewport.
  const graph = document.querySelector('.graph');
  if (vx > 0)
    graph.style.width =
      Math.max(graph.offsetWidth, (viewport.scrollLeft + viewport.clientWidth + 160) / graphZoom) +
      'px';
  if (vy > 0)
    graph.style.height =
      Math.max(graph.offsetHeight, (viewport.scrollTop + viewport.clientHeight + 160) / graphZoom) +
      'px';
  const left = viewport.scrollLeft,
    top = viewport.scrollTop;
  viewport.scrollLeft += vx * dt;
  viewport.scrollTop += vy * dt;
  // Pointer moves update synchronously; a frame needs another preview only after
  // actual scrolling, including browsers clamping motion at the top/left edge.
  if (viewport.scrollLeft !== left || viewport.scrollTop !== top) {
    if (wiring) drawWires();
    else moveGraphNodes();
  }
  if (wiring || drag?.type === 'node') graphFrame = requestAnimationFrame(graphAutoPan);
}
function cancelInteraction() {
  if (drag?.type === 'pen') {
    let discarded;
    try {
      discarded = penCommand('cancel-pen-point').discarded;
    } catch (error) {
      // A stale baseline can always release its exact token without restoring over new work.
      discardPenDraft();
      notify(error.message, true);
      render();
      return;
    }
    releasePenPointer();
    if (discarded) {
      const prior = penDraft.priorUI;
      penDraft = null;
      penId = null;
      restorePenSelection(prior);
    }
    notify(
      penDraft
        ? 'Point cancelled; draft retained. Finish or Cancel path.'
        : 'First point cancelled',
    );
    render();
    document.querySelector('.canvas-stage').focus({ preventScroll: true });
    return;
  }
  if (!drag && !wiring) return;
  if (drag?.type === 'graph-overview') {
    const old = drag;
    drag = null;
    pointerActive = false;
    const viewport = document.querySelector('.graph-scroll');
    viewport.scrollLeft = old.scroll[0];
    viewport.scrollTop = old.scroll[1];
    if (old.capture.hasPointerCapture?.(old.pointerId))
      old.capture.releasePointerCapture(old.pointerId);
    updateNavigation();
    return;
  }
  const old = drag;
  const wire = wiring;
  drag = null;
  wiring = null;
  releaseWirePointer(wire);
  if (old?.type === 'graph-marquee') restoreMarqueeSelection(old.beforeUI);
  if (graphFrame !== null) window.cancelAnimationFrame(graphFrame);
  graphFrame = null;
  if (old?.capture?.hasPointerCapture?.(old.pointerId))
    old.capture.releasePointerCapture(old.pointerId);
  if (old?.gesture)
    session.dispatch({
      ...session.state,
      type: 'cancel-gesture',
      payload: { gestureId: old.gesture.gestureId },
    });
  if (old?.localNumeric && old.started) {
    old.capture.value = old.originalInput;
    setLocalNumeric(old.capture, Number(old.originalInput || 0));
  }
  // Do not let focus restoration retain the cancelled scrub preview as a typed draft.
  if (old?.type === 'numeric' && old.started)
    old.capture.value = old.originalInput ?? String(old.original);
  pointerActive = false;
  snapMatches = [];
  wiring = null;
  render();
}
function zoomGraph(next, clientX, clientY) {
  const viewport = document.querySelector('.graph-scroll'),
    r = viewport.getBoundingClientRect();
  next = Math.max(GRAPH_LIMITS[0], Math.min(GRAPH_LIMITS[1], next));
  const anchor = [
    clientX === undefined ? viewport.clientWidth / 2 : clientX - r.left,
    clientY === undefined ? viewport.clientHeight / 2 : clientY - r.top,
  ];
  const scroll = anchoredScroll(
    [viewport.scrollLeft, viewport.scrollTop],
    anchor,
    next / graphZoom,
  );
  graphZoom = next;
  document.querySelector('.graph').style.zoom = next;
  updateGraphBounds(scroll);
  viewport.scrollLeft = scroll[0];
  viewport.scrollTop = scroll[1];
  updateNavigation();
  drawWires();
}
// Overview is transient view state. Coordinates include the graph's zoomed 16px margin.
function graphOverviewGeometry(remeasure = true) {
  const viewport = document.querySelector('.graph-scroll');
  if (!viewport || !viewport.clientWidth || !viewport.clientHeight) return null;
  const view = {
    x: viewport.scrollLeft / graphZoom,
    y: viewport.scrollTop / graphZoom,
    width: viewport.clientWidth / graphZoom,
    height: viewport.clientHeight / graphZoom,
  };
  if (remeasure || !graphOverviewNodes)
    graphOverviewNodes = [...document.querySelectorAll('.graph .node')].map((el) => ({
      x: el.offsetLeft + 16,
      y: el.offsetTop + 16,
      width: el.offsetWidth,
      height: el.offsetHeight,
    }));
  const nodes = graphOverviewNodes;
  const bounds = unionBounds([{ x: 0, y: 0, width: 1, height: 1 }, view, ...nodes]);
  // Preserve aspect ratio in world units while keeping the small window a fixed 2:1 shape.
  const width = Math.max(bounds.width + 64, (bounds.height + 64) * 2);
  const height = width / 2;
  return { nodes, viewport: view, bounds: { x: -32, y: -32, width, height } };
}
function updateGraphOverview(remeasure = true) {
  const panel = document.querySelector('.graph-overview');
  const toggle = document.querySelector('[data-action="graph-overview"]');
  if (!panel || !toggle) return;
  panel.hidden = !graphOverviewOpen;
  toggle.setAttribute('aria-expanded', String(graphOverviewOpen));
  if (!graphOverviewOpen) return;
  const viewport = document.querySelector('.graph-scroll');
  panel.style.width =
    Math.max(0, Math.min(240, viewport.clientWidth - 48, (viewport.clientHeight - 16) * 2)) + 'px';
  const geometry = graphOverviewGeometry(remeasure);
  const map = panel.querySelector('svg');
  if (!geometry) {
    map.innerHTML = '';
    return;
  }
  const bounds = drag?.type === 'graph-overview' ? drag.bounds : geometry.bounds;
  map.setAttribute('viewBox', `${bounds.x} ${bounds.y} ${bounds.width} ${bounds.height}`);
  const rect = (r, cls) =>
    `<rect class="${cls}" x="${r.x}" y="${r.y}" width="${r.width}" height="${r.height}" vector-effect="non-scaling-stroke"/>`;
  if (remeasure || !map.querySelector('.overview-viewport'))
    map.innerHTML =
      geometry.nodes.map((r) => rect(r, 'overview-node')).join('') +
      rect(geometry.viewport, 'overview-viewport');
  else {
    const frame = map.querySelector('.overview-viewport');
    for (const [key, value] of Object.entries(geometry.viewport)) frame.setAttribute(key, value);
  }
}
function observeGraphOverview() {
  if (!window.ResizeObserver) return;
  graphOverviewObserver ||= new window.ResizeObserver(() => updateGraphOverview());
  for (const element of document.querySelectorAll('.graph-scroll,.graph .node'))
    graphOverviewObserver.observe(element);
}
function graphOverviewPoint(event, map, bounds) {
  const rect = map.getBoundingClientRect();
  return [
    bounds.x + ((event.clientX - rect.left) / Math.max(1, rect.width)) * bounds.width,
    bounds.y + ((event.clientY - rect.top) / Math.max(1, rect.height)) * bounds.height,
  ];
}
function moveGraphOverview(event) {
  const point = graphOverviewPoint(event, drag.capture, drag.bounds);
  const scroll = point.map((value, axis) => Math.max(0, (value - drag.offset[axis]) * graphZoom));
  updateGraphBounds(scroll);
  const viewport = document.querySelector('.graph-scroll');
  viewport.scrollLeft = scroll[0];
  viewport.scrollTop = scroll[1];
  updateGraphOverview(false);
}
app.addEventListener('keydown', (event) => {
  if (!event.target.closest('.graph-overview-map') || event.isComposing || composing) return;
  if (event.key === 'Escape' && !drag) {
    event.preventDefault();
    event.stopPropagation();
    graphOverviewOpen = false;
    updateGraphOverview();
    document.querySelector('[data-action="graph-overview"]').focus({ preventScroll: true });
  }
  const axis = { ArrowLeft: [-80, 0], ArrowRight: [80, 0], ArrowUp: [0, -80], ArrowDown: [0, 80] }[
    event.key
  ];
  if (!axis) return;
  event.preventDefault();
  event.stopPropagation();
  if (navigationBusy() || penDraft || nameEdit) return;
  const viewport = document.querySelector('.graph-scroll');
  const scroll = [
    Math.max(0, viewport.scrollLeft + axis[0]),
    Math.max(0, viewport.scrollTop + axis[1]),
  ];
  updateGraphBounds(scroll);
  viewport.scrollLeft = scroll[0];
  viewport.scrollTop = scroll[1];
  updateGraphOverview(false);
});
function fitGraph(selectedOnly = false) {
  const viewport = document.querySelector('.graph-scroll');
  const cards = [...document.querySelectorAll('.graph .node')].filter(
    (el) => !selectedOnly || selection.has(el.dataset.id),
  );
  // Port sockets protrude beyond the card edge; reserve their visible extent too.
  const bounds = unionBounds(
    cards.map((el) => ({
      x: el.offsetLeft - 8,
      y: el.offsetTop - 8,
      width: el.offsetWidth + 16,
      height: el.offsetHeight + 16,
    })),
  );
  if (!bounds) return;
  const fit = fitScale(bounds, [viewport.clientWidth, viewport.clientHeight], 32, GRAPH_LIMITS);
  graphZoom = fit.scale;
  document.querySelector('.graph').style.zoom = graphZoom;
  // CSS zoom scales the graph margin as well as the cards.
  const scroll = [
    (bounds.x + bounds.width / 2 + 16) * graphZoom - viewport.clientWidth / 2,
    (bounds.y + bounds.height / 2 + 16) * graphZoom - viewport.clientHeight / 2,
  ];
  updateGraphBounds(scroll);
  viewport.scrollLeft = Math.max(0, scroll[0]);
  viewport.scrollTop = Math.max(0, scroll[1]);
  updateNavigation();
  drawWires();
  if (fit.limited) notify('Fit limited to 5%: some nodes may remain outside the view');
}
app.addEventListener(
  'wheel',
  (e) => {
    const viewport = e.target.closest('.canvas-stage,.graph-scroll');
    if (!viewport) return;
    // Do not move the view beneath a live edit, including native graph scrolling.
    if (navigationBusy()) {
      e.preventDefault();
      return;
    }
    if (navigationExcluded(e.target)) return;
    const delta = wheelPixels(e, [viewport.clientWidth, viewport.clientHeight]);
    if (e.ctrlKey || e.metaKey || e.altKey) {
      e.preventDefault();
      const factor = Math.exp(-Math.max(-1000, Math.min(1000, delta[1] || delta[0])) * 0.002);
      if (viewport.matches('.canvas-stage')) zoomCanvas(zoom * factor, e.clientX, e.clientY);
      else zoomGraph(graphZoom * factor, e.clientX, e.clientY);
    } else if (viewport.matches('.canvas-stage')) {
      e.preventDefault();
      canvasFit = false;
      pan = pan.map((value, axis) => value - delta[axis]);
      fitCanvas();
    }
  },
  { passive: false },
);
document.addEventListener('visibilitychange', () => {
  if (document.hidden && drag?.type === 'graph-overview') cancelInteraction();
});
window.addEventListener('blur', () => {
  lastNameClick = null;
  space = false;
  cancelInteraction();
  updateNavigation();
});
app.addEventListener('lostpointercapture', (e) => {
  if (drag?.pointerId === e.pointerId || wiring?.pointerId === e.pointerId) cancelInteraction();
});

app.addEventListener('pointerdown', (e) => {
  if (drag) return;
  const overview = e.target.closest('.graph-overview-map');
  if (overview) {
    e.preventDefault();
    if (
      e.button !== 0 ||
      navigationBusy() ||
      penDraft ||
      nameEdit ||
      document.querySelector('dialog[open]')
    )
      return;
    const viewport = document.querySelector('.graph-scroll');
    const geometry = graphOverviewGeometry();
    if (!geometry) return;
    const point = graphOverviewPoint(e, overview, geometry.bounds);
    const v = geometry.viewport;
    const inside =
      point[0] >= v.x && point[0] <= v.x + v.width && point[1] >= v.y && point[1] <= v.y + v.height;
    drag = {
      type: 'graph-overview',
      capture: overview,
      pointerId: e.pointerId,
      bounds: geometry.bounds,
      scroll: [viewport.scrollLeft, viewport.scrollTop],
      offset: inside ? [point[0] - v.x, point[1] - v.y] : [v.width / 2, v.height / 2],
    };
    overview.focus({ preventScroll: true });
    try {
      overview.setPointerCapture?.(e.pointerId);
    } catch {
      drag = null;
      return;
    }
    moveGraphOverview(e);
    return;
  }
  if (penDraft && e.target.closest('[inert]')) return;
  const viewport = e.target.closest('.graph-scroll');
  const control = editingControl(e.target);
  if (control && e.target.closest('.canvas-text-editor')) return;
  const numeric = e.target.closest('input[type="number"][data-scrub-step]');
  if (numeric && e.button === 0 && !numeric.disabled && !numeric.readOnly) {
    const field = numeric.closest('.field[data-node]'),
      n = field && current().nodes.find((n) => n.id === field.dataset.node),
      q = n && parameter(n, field.dataset.key),
      textTarget = !q && textNumericTarget(numeric),
      otherTarget = !q && !textTarget && numericControlTarget(numeric),
      rect = numeric.getBoundingClientRect();
    // Leave the native spinner hit area alone. A click elsewhere still focuses/selects text.
    if (
      (!current().locked || otherTarget?.localNumeric) &&
      ((q && !q.locked && (numeric.dataset.base !== undefined || (!q.source && !q.expression))) ||
        textTarget ||
        otherTarget) &&
      (!rect.width || e.clientX < rect.right - 18)
    ) {
      drag = {
        type: 'numeric',
        ...(textTarget ||
          otherTarget || {
            nodeId: n.id,
            key: field.dataset.key,
            axis: Number(numeric.dataset.axis || 0),
            alpha: numeric.dataset.color === 'alpha',
            factor: Number(numeric.dataset.valueFactor || 1),
            authoredBase: numeric.dataset.base !== undefined,
          }),
        layerId: current().id,
        startX: e.clientX,
        pointerId: e.pointerId,
        capture: numeric,
        started: false,
      };
      return;
    }
  }

  if (
    viewport &&
    !control &&
    !navigationBusy() &&
    (e.button === 1 || (e.button === 0 && (space || (e.shiftKey && (e.ctrlKey || e.metaKey)))))
  ) {
    drag = {
      type: 'graph-pan',
      start: [e.clientX, e.clientY],
      scroll: [viewport.scrollLeft, viewport.scrollTop],
    };
    viewport.focus({ preventScroll: true });
    captureGraphPointer(e);
    updateNavigation();
    e.preventDefault();
    return;
  }
  if (
    viewport &&
    e.button === 0 &&
    !control &&
    !e.target.closest('.node') &&
    !navigationBusy() &&
    !penDraft
  ) {
    viewport.focus({ preventScroll: true });
    drag = {
      type: 'graph-marquee',
      start: [e.clientX, e.clientY],
      startWorld: graphPoint(e.clientX, e.clientY),
      additive: e.shiftKey,
      beforeUI: selectionSnapshot(true),
      started: false,
    };
    captureGraphPointer(e);
    e.preventDefault();
    return;
  }
  const stage = e.target.closest('.canvas-stage');
  // Floating fields/buttons are UI, never canvas hit targets or navigation gestures.
  if (stage && e.target.closest('.character-floating')) return;
  if (stage && !control && !navigationBusy() && (e.button === 1 || (e.button === 0 && space))) {
    // View navigation takes priority over guides, shapes and Pen, including after zoom.
    canvasFit = false;
    drag = {
      type: 'pan',
      start: [e.clientX, e.clientY],
      original: [...pan],
      pointerId: e.pointerId,
      capture: stage,
    };
    stage.focus({ preventScroll: true });
    stage.setPointerCapture?.(e.pointerId);
    updateNavigation();
    e.preventDefault();
    return;
  }
  if (
    stage &&
    characterEdit &&
    !control &&
    !e.target.closest('.character-floating, .canvas-text-editor, [contenteditable]') &&
    e.button === 0
  ) {
    e.preventDefault();
    if (current().locked || !layerShown(current())) return;
    if ([...textDrafts.values()].some((draft) => draft.composition)) {
      notify('Finish or cancel text composition before selecting characters');
      return;
    }
    // Blur the original control before any structural render so its change commits its draft.
    stage.focus({ preventScroll: true });
    const result = evaluate(current(), fonts, history.doc.imageAssets),
      context = characterContext(result),
      pos = canvasPoint(e);
    if (!context) return;
    const hits = context.editable.flatMap((occurrence, instance) =>
      context.layout.characters
        .filter((g) => hitCharacter(g, occurrence.matrix, pos))
        .map((glyph) => ({ occurrence, glyph, instance })),
    );
    if (!hits.length) {
      characterEdit.selected.clear();
      render();
      document.querySelector('.canvas-stage').focus({ preventScroll: true });
      return;
    }
    const already = hits.filter(
      (h) => h.occurrence.route === characterEdit.route && characterEdit.selected.has(h.glyph.id),
    );
    if (hits.length > 1 && (e.shiftKey || already.length !== 1)) {
      selectOverlappingCharacters(hits, e.shiftKey);
      return;
    }
    const hit = hits.length === 1 ? hits[0] : already[0];
    // A Shift-click still toggles membership. Delay removing a selected character
    // until release, so Shift-drag can move the current selection instead.
    const toggleOnClick = e.shiftKey && characterEdit.selected.has(hit.glyph.id);
    characterEdit.route = hit.occurrence.route;
    if (!toggleOnClick) chooseCharacter(hit, e.shiftKey);
    render();
    const capture = document.querySelector('.canvas-stage');
    capture.focus({ preventScroll: true });
    if (!characterEdit.selected.size) return;
    const next = characterContext(),
      snapGeometry = collectCharacterSnapGeometry({
        characters: next.layout.characters,
        selectedIds: next.selected.map((g) => g.id),
        occurrence: hit.occurrence,
        lines: next.layout.lines,
        writingMode: next.n.writingMode,
      });
    drag = {
      type: 'character',
      toggleOnClick: toggleOnClick ? hit : null,
      nodeId: next.n.id,
      route: hit.occurrence.route,
      matrix: hit.occurrence.matrix,
      start: pos,
      startClient: [e.clientX, e.clientY],
      originals: next.selected.map((g) => ({ id: g.id, offset: [...g.offset] })),
      anchors: snapGeometry.sources,
      snapCandidates: snapGeometry.candidates,
      before: cloneDocument(history.doc),
      pointerId: e.pointerId,
      capture,
    };
    capture.setPointerCapture?.(e.pointerId);
    return;
  }
  if (e.button !== 0) {
    if (viewport && control && e.button === 1) e.preventDefault();
    return;
  }
  if (e.target.closest('.output')) {
    const socket = e.target.closest('.output');
    let id = socket.dataset.source,
      port = socket.dataset.sourcePort;
    if (current().locked) return;
    document.querySelector('.graph-scroll').focus({ preventScroll: true });
    wiring = {
      source: id,
      port,
      pointer: [e.clientX, e.clientY],
      pointerId: e.pointerId,
      envelope: session.state,
      layerId: current().id,
    };
    document.querySelectorAll('.graph .field').forEach((f) =>
      f.classList.toggle(
        'compatible',
        socketCompatibleFor(
          current().nodes.find((n) => n.id === id),
          current().nodes.find((n) => n.id === f.dataset.node),
          f.dataset.key,
          port,
        ) &&
          f.dataset.node !== id &&
          !parameter(
            current().nodes.find((n) => n.id === f.dataset.node),
            f.dataset.key,
          )?.locked,
      ),
    );
    updateGraphBounds();
    drawWires();
    captureWirePointer(e);
    e.preventDefault();
    graphFrame = requestAnimationFrame(graphAutoPan);
    return;
  }
  if (e.target.closest('.input')) {
    let f = e.target.closest('.field');
    const q = parameter(
      current().nodes.find((n) => n.id === f.dataset.node),
      f.dataset.key,
    );
    if (current().locked || q.locked) return;
    document.querySelector('.graph-scroll').focus({ preventScroll: true });
    wiring = {
      disconnect: true,
      envelope: session.state,
      layerId: current().id,
      node: f.dataset.node,
      key: f.dataset.key,
      pointer: [e.clientX, e.clientY],
      pointerId: e.pointerId,
    };
    drawWires();
    captureWirePointer(e);
    graphFrame = requestAnimationFrame(graphAutoPan);
    e.preventDefault();
    return;
  }
  if (e.target.closest('.splitter')) {
    drag = { type: 'split', startY: e.clientY, height: graphHeight };
    e.preventDefault();
    return;
  }
  let head = e.target.closest('.node-head');
  if (head && !e.target.closest('button') && !control) {
    artworkTarget = null;
    artworkSelection = new Set();
    selectedGuide = null;
    let id = head.closest('.node').dataset.id;
    if (e.shiftKey) {
      if (selection.has(id)) selection.delete(id);
      else selection.add(id);
      selected = selection.has(id) ? id : [...selection][0] || null;
      render();
      return;
    }
    if (!selection.has(id)) selection = new Set([id]);
    selected = id;
    if (current().locked) {
      render();
      return;
    }
    // Commit a focused field before taking the gesture rollback snapshot.
    document.querySelector('.graph-scroll').focus({ preventScroll: true });
    const gesture = beginGraphGesture({
      kind: 'graph-translate',
      layerId: current().id,
      nodeIds: [...selection],
    });
    if (!gesture) return;
    drag = {
      type: 'node',
      snapBypass: e.ctrlKey || e.metaKey,
      shiftKey: e.shiftKey,
      boxes: current().nodes.map((node) => {
        const el = document.querySelector(`.node[data-id="${node.id}"]`);
        return {
          id: node.id,
          x: node.position[0],
          y: node.position[1],
          width: el.offsetWidth || node.width || 264,
          height: el.offsetHeight || 260,
        };
      }),
      nameTarget: e.target.closest('[data-rename-node]') ? id : null,
      started: false,
      pointer: [e.clientX, e.clientY],
      startWorld: graphPoint(e.clientX, e.clientY),
      id,
      start: [e.clientX, e.clientY],
      originals: [...selection].map((id) => ({
        id,
        position: [...current().nodes.find((n) => n.id === id).position],
      })),
      gesture,
    };
    captureGraphPointer(e);
    document
      .querySelectorAll('.node')
      .forEach((x) => x.classList.toggle('selected', selection.has(x.dataset.id)));
    e.preventDefault();
    return;
  }
  const resized = e.target.closest('.node-resize')?.closest('.node');
  if (resized) {
    if (current().locked) return;
    document.querySelector('.graph-scroll').focus({ preventScroll: true });
    const gesture = beginGraphGesture({
      kind: 'graph-width',
      layerId: current().id,
      nodeId: resized.dataset.id,
    });
    if (!gesture) return;
    drag = {
      type: 'resize',
      id: resized.dataset.id,
      startX: e.clientX,
      width: resized.offsetWidth,
      gesture,
    };
    captureGraphPointer(e);
    e.preventDefault();
    return;
  }
  if (!stage || wiring) return;
  if (
    tool === 'direct' &&
    e.button === 0 &&
    !e.target.closest('[data-point],[data-path-unit-move]')
  ) {
    const doc = displayDocument(),
      point = canvasPoint(e),
      hitLayer = e.target.closest('[data-layer]')?.dataset.layer;
    const layers = doc.layers
      .filter((l) => layerIsVisible(history.doc, l) && !l.guide && (!hitLayer || l.id === hitLayer))
      .reverse();
    let picked = null,
      reason = 'Select an editable path';
    for (const layer of layers) {
      const route = directSelectionSource(doc, layer.id, {
        fonts: resolveFonts(doc, activeFonts()),
      });
      if (!route.supported) {
        reason = route.reason;
        continue;
      }
      const focused = pointSelection.nodeId === route.nodeId ? [...pointSelection.ids] : [];
      const hit = hitPointControl(route.path, point, canvasScale(), focused),
        segment = !hit && hitPathSegment(route.path, point, canvasScale());
      if (hit?.ambiguous || segment?.ambiguous) {
        notify('Overlapping points or segments: select the source in the graph', true);
        e.preventDefault();
        return;
      }
      if (hit || segment || hitLayer) {
        picked = { ...route, layer };
        break;
      }
    }
    if (!picked) {
      pointSelection = { nodeId: null, ids: new Set() };
      pointFocus = null;
      notify(reason, true);
      updateCanvas(true);
      e.preventDefault();
      return;
    }
    activeLayer = picked.layerId;
    selected = picked.nodeId;
    selection = new Set([selected]);
    artworkTarget = null;
    artworkSelection = new Set();
    if (pointSelection.nodeId !== selected) pointSelection = { nodeId: selected, ids: new Set() };
    updateCanvas(true);
    queueMicrotask(() => {
      if (!drag) render();
    });
  }
  pendingCanvasTextClick = null;
  if (
    tool === 'select' &&
    !control &&
    e.button === 0 &&
    !e.shiftKey &&
    !e.ctrlKey &&
    !e.metaKey &&
    !e.altKey
  ) {
    const layerId =
      e.target.closest('[data-node-stage-layer]')?.dataset.nodeStageLayer ||
      e.target.closest('[data-layer]')?.dataset.layer;
    if (layerId)
      pendingCanvasTextClick = {
        layerId,
        sessionId: session.id,
        revision: session.state.expectedRevision,
        pointerId: e.pointerId,
        x: e.clientX,
        y: e.clientY,
        moved: false,
      };
    else lastCanvasTextClick = null;
  }
  const guideId = e.target.dataset.guide;
  if (guideId && tool !== 'pen') {
    selectedGuide = guideId;
    const a = guideSettings(history.doc),
      g = a.guides.find((g) => g.id === guideId);
    if (a.locked || g.locked) {
      notify('Guide is locked');
      render();
      return;
    }
    drag = {
      type: 'guide',
      id: guideId,
      axis: g.axis === 'x' ? 0 : 1,
      original: g.position,
      start: canvasPoint(e),
      before: cloneDocument(history.doc),
    };
    e.preventDefault();
    updateCanvas();
    return;
  }
  selectedGuide = null;
  const dedicated = e.target.closest(
    '[data-object-resize],[data-transform-handle],[data-position-handle],[data-point],[data-path-unit-move],[data-node-stage-move]',
  );
  if (tool === 'select' && !dedicated) {
    if (document.querySelector('dialog[open]')) return;
    const layerId = e.target.closest('[data-layer]')?.dataset.layer;
    stage.focus({ preventScroll: true });
    if (!layerId) {
      artworkTarget = null;
      artworkSelection = new Set();
      selected = null;
      selection = new Set();
      render();
      document.querySelector('.canvas-stage').focus({ preventScroll: true });
      return;
    }
    const toggleOnClick = e.shiftKey && artworkTarget && artworkSelection.has(layerId);
    selectArtwork(layerId, { toggle: e.shiftKey && !toggleOnClick, preserve: true });
    const resolved = artworkMove();
    render(false, resolved);
    const capture = document.querySelector('.canvas-stage');
    capture.focus({ preventScroll: true });
    if (!resolved.supported) {
      if (toggleOnClick) {
        selectArtwork(layerId, { toggle: true });
        render();
        document.querySelector('.canvas-stage').focus({ preventScroll: true });
      } else notify(resolved.reason, true);
      return;
    }
    const target = resolved.target,
      bounds = [...artworkSelection].map((id) =>
        pathBounds(
          evaluate(
            history.doc.layers.find((l) => l.id === id),
            fonts,
            history.doc.imageAssets,
          ).path,
        ),
      ),
      b = [0, 1].map((axis) => {
        const min = Math.min(...bounds.map((b) => b[axis][0])),
          max = Math.max(...bounds.map((b) => b[axis][2]));
        return [min, (min + max) / 2, max];
      });
    drag = {
      type: 'artwork',
      target,
      selectionBefore: selectionSnapshot(),
      toggleOnClick: toggleOnClick ? layerId : null,
      start: canvasPoint(e),
      started: false,
      anchors: b.map((axis) => [...axis]),
      before: cloneDocument(history.doc),
    };
    captureArtworkPointer(e, capture);
    e.preventDefault();
    return;
  }
  let penAltAnchor = false;
  if (tool === 'pen' && !penDraft && selectedNode() && !artworkTarget) {
    const n = selectedNode();
    // Only the explicitly selected source stage is an editable target. Never
    // infer an owner from the painted final output or a generated point ID.
    if (selection.size === 1 && selection.has(n.id) && outputType(n) === 'Path') {
      if (document.querySelector('dialog[open]') || composing) return;
      try {
        stage.focus({ preventScroll: true });
        const path =
            directPointContext(n)?.path ||
            evaluate(current(), fonts, history.doc.imageAssets).run(n.id),
          pos = canvasPoint(e),
          anchor = hitPointControl(path, pos, canvasScale()),
          segment = !anchor && hitPathSegment(path, pos, canvasScale());
        if (anchor || segment) {
          e.preventDefault();
          if (!layerShown(current()) || current().locked || n.locked || n.bypassed)
            throw Error('Show and unlock the source path before editing');
          if (!['Curve', 'Multi Path'].includes(n.type))
            throw Error(
              'Generated path: select the original Curve or Multi Path. Local point insertion is not supported here.',
            );
          if (anchor?.ambiguous || segment?.ambiguous)
            throw Error(
              'Overlapping path targets: point insertion is ambiguous. Choose an unambiguous segment.',
            );
          stage.focus({ preventScroll: true });
          if (anchor) {
            if (e.altKey) penAltAnchor = true;
            else {
              const contour =
                  n.type === 'Multi Path'
                    ? n.contours.find((c) => c.points.includes(anchor.pointId))
                    : null,
                ids = contour?.points || n.points.map((p) => p.id),
                open = contour ? !contour.closed : !n.params.closed.value;
              if (!e.shiftKey && open && [ids[0], ids.at(-1)].includes(anchor.pointId)) {
                startPenContinuation(n, anchor.pointId, contour?.id);
                e.preventDefault();
                return;
              }
              pointFocus = { nodeId: n.id, pointId: anchor.pointId };
              selectPointTarget(n, anchor, e);
              return;
            }
          } else {
            if (e.altKey || e.shiftKey)
              throw Error(
                'Click a segment without Alt or Shift to add a point; Alt edits anchor handles.',
              );
            if (insertionRemapDependents(current(), n.id).length)
              throw Error(
                'Point insertion is unavailable because this source feeds retained Path Remap edits: topology changes can suspend or alter them. No edits were rebound.',
              );
            drag = {
              type: 'path-insert',
              payload: {
                layerId: current().id,
                nodeId: n.id,
                contourId: segment.contourId,
                startPointId: segment.startPointId,
                endPointId: segment.endPointId,
                t: segment.t,
              },
              envelope: { ...session.state },
              startClient: [e.clientX, e.clientY],
              position: segment.position,
              cancelled: false,
            };
            captureArtworkPointer(e, stage);
            updateCanvas(true);
            notify(
              'Release to add one point. Drag or Escape cancels. Downstream point-count effects may change.',
            );
            return;
          }
        }
      } catch (error) {
        notify(error.message, true);
        return;
      }
    }
  }
  if (tool === 'pen' && !penAltAnchor) {
    let pos = canvasPoint(e);
    // A new independent layer must include the old active layer as a snap target.
    if (!penDraft)
      snapLayerBounds = collectSnapLayerBounds(
        history.doc,
        canvasLayers().map((l) => [l, evaluate(l, fonts, history.doc.imageAssets)]),
        null,
        pathBounds,
      );
    const previous = penDraft && penSource().points.find((p) => p.id === penDraft.token.pointId);
    const penOrigin =
      previous &&
      (penDraft.matrix
        ? transformCharacterPoint(penDraft.matrix, previous.position.value)
        : previous.position.value);
    if (penOrigin) {
      const delta = pos.map((v, i) => v - penOrigin[i]);
      const constraint = constrainDirectionDrag(
        {},
        delta,
        delta.map((v) => v * canvasScale()),
        e.shiftKey,
      );
      pos = snapped(
        [[penOrigin[0]], [penOrigin[1]]],
        constraint.delta,
        e,
        null,
        constraint.direction,
        penOrigin,
        [0, 0],
      ).map((v, i) => v + penOrigin[i]);
    } else pos = snapped([[pos[0]], [pos[1]]], [0, 0], e).map((v, i) => v + pos[i]);
    if (!pos.every(Number.isFinite)) return;
    if (
      penDraft &&
      e.target.dataset.point === (penDraft.closingPointId || penSource().points[0]?.id) &&
      !e.target.dataset.handle
    ) {
      // The clicked vertex is replaced on finish. Cancel native pointer focus
      // so the newly focused artboard keeps the following keyboard action.
      e.preventDefault();
      finishPenDraft(true);
      return;
    }
    if (
      penDraft?.resuming &&
      e.target.dataset.point === penDraft.token.pointId &&
      !e.target.dataset.handle
    ) {
      e.preventDefault();
      return;
    }
    try {
      const authored = penDraft?.matrix ? inverseAffinePoint(penDraft.matrix, pos) : pos;
      let token;
      if (!penDraft) {
        const priorUI = selectionSnapshot(),
          result = session.dispatch({
            ...session.state,
            type: 'begin-gesture',
            payload: { target: { kind: 'pen' }, position: pos },
          }),
          layer = session
            .previewSnapshot(result.gestureId)
            .document.layers.find((l) => l.id === result.layerId);
        penDraft = { layer, priorUI, token: result };
        token = result;
        penId = layer.nodes[0].id;
        activeLayer = layer.id;
        artworkTarget = null;
        artworkSelection = new Set();
        selected = penId;
        selection = new Set([selected]);
      } else {
        token = penCommand('append-pen-point', { position: authored });
      }
      drag = {
        type: 'pen',
        pointId: token.pointId,
        nodeId: penId,
        start: canvasPoint(e),
        anchor: pos,
        directionOrigin: pos,
        startClient: [e.clientX, e.clientY],
        pointerId: e.pointerId,
      };
      render();
      const capture = document.querySelector('.canvas-stage');
      drag.capture = capture;
      capture.focus({ preventScroll: true });
      capture.setPointerCapture?.(e.pointerId);
      e.preventDefault();
      if (penSource().points.length === 1) revealNode(penId);
      notify('Draft path · Enter / Escape: finish · Cancel path: discard');
    } catch (error) {
      notify(error.message, true);
    }
    return;
  }
  if (!layerShown(current())) {
    notify('Show the active layer before editing it on the canvas', true);
    return;
  }
  if (current().locked) {
    notify('Composition is locked', true);
    return;
  }
  let pos = canvasPoint(e),
    n = selectedNode();
  const pathUnit = e.target.closest('[data-path-unit-move]')?.dataset.pathUnitMove;
  if (pathUnit && n?.type === 'Multi Path') {
    drag = {
      type: 'path-unit',
      matrix: directPointContext(n)?.matrix,
      nodeId: n.id,
      unitId: pathUnit,
      start: pos,
      before: cloneDocument(history.doc),
      started: false,
    };
    captureArtworkPointer(e, stage);
    e.preventDefault();
    return;
  }
  const resizeHandle = e.target.closest('[data-object-resize]');
  if (resizeHandle) {
    e.preventDefault();
    stage.focus({ preventScroll: true });
    n = selectedNode();
    if (resizeHandle.dataset.objectNode !== n?.id) return;
    const handles = objectResizeContext(n, evaluate(current(), fonts, history.doc.imageAssets)),
      handle = handles[Number(resizeHandle.dataset.objectResize)],
      q = n.params[handle?.key];
    if (!handle || !q || q.locked || q.source || q.expression) {
      notify('Dimension is locked or driven by an input', true);
      return;
    }
    drag = {
      type: 'object-resize',
      nodeId: n.id,
      handle,
      key: handle.key,
      original: structuredClone(q.value),
      start: canvasPoint(e),
      startClient: [e.clientX, e.clientY],
      before: cloneDocument(history.doc),
      pointerId: e.pointerId,
      capture: stage,
      started: false,
    };
    stage.setPointerCapture?.(e.pointerId);
    return;
  }
  const transformHandle = e.target.closest('[data-transform-handle]');
  if (transformHandle && n?.type === 'Transform') {
    e.preventDefault();
    if (
      selection.size !== 1 ||
      !selection.has(n.id) ||
      transformHandle.dataset.transformNode !== n.id
    )
      return;
    // Commit any focused numeric draft before capturing the gesture state.
    stage.focus({ preventScroll: true });
    n = selectedNode();
    const key = transformHandle.dataset.transformHandle,
      q = n?.params[key];
    if (
      !['anchor', 'scale', 'rotation'].includes(key) ||
      !q ||
      q.locked ||
      q.source ||
      q.expression
    ) {
      notify(`${key} is locked or driven by an input`, true);
      return;
    }
    try {
      const context = transformContext(n, evaluate(current(), fonts, history.doc.imageAssets));
      if (!context) return;
      if (key === 'anchor') {
        const position = n.params.position;
        if (position && (position.locked || position.source || position.expression))
          throw Error('Anchor and Position must be unlocked and undriven');
        anchorFromOutputDelta(context.state, [0, 0]);
      }
      const start = canvasPoint(e);
      drag = {
        type: 'transform',
        key,
        nodeId: n.id,
        geometry: context.geometry,
        start,
        startClient: [e.clientX, e.clientY],
        original: q.value,
        angle: rotationHandleAngle(context.geometry.pivot, start),
        angleDelta: 0,
        before: cloneDocument(history.doc),
        pointerId: e.pointerId,
        capture: stage,
        started: false,
      };
      stage.setPointerCapture?.(e.pointerId);
    } catch (err) {
      notify(err.message, true);
    }
    return;
  }
  const controller = positionController(n);
  if (controller) {
    // A dedicated point target never turns a click elsewhere into a controller move.
    if (e.target.closest('[data-position-handle]')?.dataset.positionHandle !== n.id) return;
    const q = n.params[controller.key];
    if (q.locked || q.source || q.expression) {
      notify(`${controller.label} is locked or driven by an input`, true);
      return;
    }
    stage.focus({ preventScroll: true });
    n = selectedNode();
    drag = {
      type: 'position-controller',
      key: controller.key,
      nodeId: n.id,
      start: canvasPoint(e),
      startClient: [e.clientX, e.clientY],
      original: [...n.params[controller.key].value],
      before: cloneDocument(history.doc),
      pointerId: e.pointerId,
      capture: stage,
      started: false,
    };
    stage.setPointerCapture?.(e.pointerId);
    e.preventDefault();
    return;
  }
  stage.focus({ preventScroll: true });
  n = selectedNode();
  const directContext = directPointContext(n);
  if (directContext && (!directContext.supported || directContext.locked)) {
    notify(directContext.reason || 'Source path is locked', true);
    return;
  }
  let pointTarget = e.target.dataset.point
    ? { pointId: e.target.dataset.point, key: e.target.dataset.handle || 'position' }
    : null;
  if (
    ['Curve', 'Multi Path', 'Path Remap'].includes(n?.type) &&
    document.querySelector('.artboard')?.getScreenCTM
  ) {
    const path =
      directContext?.path || evaluate(current(), fonts, history.doc.imageAssets).run(n.id);
    pointTarget = hitPointControl(
      path,
      pos,
      canvasScale(),
      [pointFocus, pointHover].filter((t) => t?.nodeId === n.id).map((t) => t.pointId),
    );
    if (pointTarget?.ambiguous) {
      notify('Overlapping points: choose a point in the graph.', true);
      return;
    }
  }
  if (pointTarget) {
    pointFocus = { nodeId: n.id, pointId: pointTarget.pointId };
    if (!selectPointTarget(n, pointTarget, e)) return;
  }
  if (pointTarget?.key === 'position' && e.altKey) {
    drag = {
      type: 'point-handles',
      matrix: directContext?.matrix,
      nodeId: n.id,
      pointId: pointTarget.pointId,
      start: pos,
      before: cloneDocument(history.doc),
      started: false,
    };
    captureArtworkPointer(e, stage);
    e.preventDefault();
    if (
      updateCanvasGesture(
        {
          kind: 'point-handles',
          layerId: current().id,
          nodeId: n.id,
          pointId: pointTarget.pointId,
        },
        {},
      )
    )
      updateCanvas(true);
    return;
  }
  if (pointTarget && n?.type === 'Path Remap') {
    const input = evaluate(current(), fonts, history.doc.imageAssets).run(n.params.path.source),
      pointId = pointTarget.pointId,
      key = pointTarget.key,
      existing = n.edits.find((e) => e.pointId === pointId);
    if (existing && input.unstable && existing.fingerprint !== input.fingerprint) {
      notify(
        'This edit is suspended. Use Rebind retained edit to this point before dragging.',
        true,
      );
      return;
    }
    if (
      existing?.params?.[key] &&
      (existing.params[key].locked ||
        existing.params[key].source ||
        existing.params[key].expression)
    ) {
      notify(`${key} is locked or driven`, true);
      return;
    }
    const before = cloneDocument(history.doc);
    drag = {
      pointIds: key === 'position' ? selectedPointIds(n) : undefined,
      type: 'remap',
      matrix: directContext?.matrix,
      independent: pointTarget.key !== 'position' && e.altKey,
      nodeId: n.id,
      pointId,
      key,
      start: pos,
      original: clone(existing?.params?.[key]?.value ?? existing?.[key] ?? [0, 0]),
      anchor: (() => {
        const p = (
          directContext?.path || evaluate(current(), fonts, history.doc.imageAssets).run(n.id)
        ).contours
          .flatMap((c) => c.points)
          .find((p) => p.id === pointId);
        return key === 'position' ? p.position : p.position.map((v, i) => v + p[key][i]);
      })(),
      before,
    };
    captureArtworkPointer(e, stage);
    const contour = input.contours.find((c) => c.points.some((p) => p.id === pointId));
    for (const key of [`contour-${contour.id}`, `remap-${pointId}`]) {
      const detail = document.querySelector(
        `.node[data-id="${n.id}"] details[data-detail="${key}"]`,
      );
      if (detail) detail.open = true;
    }
    return;
  }
  if (pointTarget && ['Curve', 'Multi Path'].includes(n?.type)) {
    let pt = n.points.find((p) => p.id === pointTarget.pointId),
      key = pointTarget.key,
      q = pt[key];
    if (q.locked || q.source || q.expression) {
      notify(`${key} is locked or driven by an input`, true);
      return;
    }
    drag = {
      pointIds: key === 'position' ? selectedPointIds(n) : undefined,
      type: 'point',
      matrix: directContext?.matrix,
      independent: pointTarget.key !== 'position' && e.altKey,
      nodeId: n.id,
      pointId: pt.id,
      key,
      start: pos,
      original: clone(q.value),
      anchor: (() => {
        const p = (
          directContext?.path || evaluate(current(), fonts, history.doc.imageAssets).run(n.id)
        ).contours
          .flatMap((c) => c.points)
          .find((p) => p.id === pt.id);
        return key === 'position' ? p.position : p.position.map((v, i) => v + p[key][i]);
      })(),
      before: cloneDocument(history.doc),
    };
    captureArtworkPointer(e, stage);
    return;
  }
  const stageMove = e.target.closest('[data-node-stage-move]');
  if (
    stageMove?.dataset.nodeStageMove !== selected ||
    stageMove?.dataset.nodeStageLayer !== current().id
  )
    return;
  const generators = current().nodes.filter((n) => selection.has(n.id));
  if (generators.some((n) => n.pathAttachment)) {
    notify(
      'Move the source path, change the start anchor, or use a downstream Transform for attached Text',
      true,
    );
    return;
  }
  if (
    generators.length &&
    !generators.some((n) => n.bypassed) &&
    ((generators.length === 1 && generators[0].type === 'Transform') ||
      generators.every((n) =>
        [
          'Circle',
          'Ellipse',
          'Rectangle',
          'Polygon',
          'Star',
          'Text',
          'Curve',
          'Multi Path',
          'Image',
          'Composition Reference',
        ].includes(n.type),
      ))
  ) {
    const targets = generators.map((n) => {
      const keys = ['Curve', 'Multi Path'].includes(n.type)
        ? n.points.map((p) => `${p.id}.position`)
        : [
            n.type === 'Transform'
              ? 'offset'
              : ['Text', 'Image', 'Composition Reference'].includes(n.type)
                ? 'position'
                : 'center',
          ];
      return { nodeId: n.id, keys, original: keys.map((k) => clone(parameter(n, k).value)) };
    });
    if (
      targets.some((t) =>
        t.keys.some((k) => {
          const q = parameter(
            current().nodes.find((n) => n.id === t.nodeId),
            k,
          );
          return q.locked || q.source || q.expression;
        }),
      )
    ) {
      notify('A selected position is locked or driven by an input', true);
      return;
    }
    const bounds = generators.map((g) =>
      pathBounds(evaluate(current(), fonts, history.doc.imageAssets).run(g.id)),
    );
    const anchors = [0, 1].map((axis) => {
      const low = Math.min(...bounds.map((b) => b[axis][0])),
        high = Math.max(...bounds.map((b) => b[axis][2]));
      return [low, (low + high) / 2, high];
    });
    drag = { type: 'shape', targets, anchors, start: pos, before: cloneDocument(history.doc) };
    captureArtworkPointer(e, stage);
  } else notify('Select generators, or one Transform to move its whole group');
});
function movePointer(e) {
  if (!drag && penSegmentHover && !e.target.closest?.('.canvas-stage')) {
    penSegmentHover = null;
    updateCanvas();
  }
  if (!drag && !wiring && pointHover && !e.target.closest?.('.canvas-stage')) {
    pointHover = null;
    updateCanvas();
  }
  if (
    !drag &&
    !wiring &&
    ['Curve', 'Multi Path', 'Path Remap'].includes(selectedNode()?.type) &&
    e.target.closest?.('.canvas-stage')
  ) {
    const n = selectedNode(),
      path = renderEvaluation?.run(n.id);
    const hit = hitPointControl(
      path,
      canvasPoint(e),
      canvasScale(),
      [pointFocus, pointHover].filter((t) => t?.nodeId === n.id).map((t) => t.pointId),
    );
    if (tool === 'pen' && !penDraft && !artworkTarget && selection.size === 1) {
      const segment = !hit && hitPathSegment(path, canvasPoint(e), canvasScale());
      const nextSegment =
        segment && !segment.ambiguous && ['Curve', 'Multi Path'].includes(n.type)
          ? { ...segment, nodeId: n.id, revision: session.state.expectedRevision }
          : null;
      if (penSegmentHover || nextSegment) {
        penSegmentHover = nextSegment;
        updateCanvas();
      }
    } else penSegmentHover = null;
    const next = hit && !hit.ambiguous ? { nodeId: n.id, pointId: hit.pointId } : null;
    if (pointHover?.nodeId !== next?.nodeId || pointHover?.pointId !== next?.pointId) {
      pointHover = next;
      updateCanvas();
    }
  }
  if (characterEdit && !drag && !wiring) {
    const target = e.target.closest?.('[data-character]');
    const route = target?.closest('[data-character-route]')?.dataset.characterRoute;
    characterEdit.hover = target ? `${route}:${target.dataset.character}` : null;
    document
      .querySelectorAll('.character-hit.hovered')
      .forEach((el) => el.classList.remove('hovered'));
    target?.classList.add('hovered');
  }
  if (wiring) {
    if (wiring.pointerId !== undefined && wiring.pointerId !== e.pointerId) return;
    wiring.pointer = [e.clientX, e.clientY];
    drawWires();
    return;
  }
  if (!drag || (drag.pointerId !== undefined && drag.pointerId !== e.pointerId)) return;
  if (drag.type === 'path-insert') {
    if (Math.hypot(e.clientX - drag.startClient[0], e.clientY - drag.startClient[1]) >= 4) {
      drag.cancelled = true;
      updateCanvas(true);
    }
    return;
  }
  if (drag.type === 'graph-marquee') {
    moveGraphMarquee(e);
    return;
  }
  if (directionDrag())
    drag.lastPointer = {
      clientX: e.clientX,
      clientY: e.clientY,
      pointerId: e.pointerId,
      altKey: e.altKey,
      shiftKey: e.shiftKey,
    };
  if (drag.type === 'artwork') {
    const resolved = artworkMove();
    if (!resolved.supported || !sameArtworkMove(resolved.target, drag.target)) {
      cancelInteraction();
      notify(resolved.reason || 'Artwork target changed', true);
      return;
    }
    if (
      !drag.started &&
      Math.hypot(e.clientX - drag.startClient[0], e.clientY - drag.startClient[1]) < 3
    )
      return;
    const pos = canvasPoint(e),
      delta = artworkDelta(
        drag.anchors,
        pos.map((v, i) => v - drag.start[i]),
        e,
      );
    if (!delta.every(Number.isFinite)) return;
    try {
      if (
        !updateCanvasGesture(
          { kind: 'artwork-translate', layerIds: [...artworkSelection] },
          { delta },
        )
      )
        return;
    } catch (error) {
      cancelInteraction();
      notify(error.message, true);
      return;
    }
    drag.started = true;
    drag.lastDelta = delta;
    updateCanvas();
    return;
  }
  if (['transform', 'object-resize'].includes(drag.type)) {
    const n = selectedNode(),
      q = n?.params[drag.key];
    if (
      n?.id !== drag.nodeId ||
      n?.bypassed ||
      selection.size !== 1 ||
      current().locked ||
      !layerShown(current()) ||
      !q ||
      q.locked ||
      q.source ||
      q.expression
    ) {
      cancelInteraction();
      return;
    }
    if (
      !drag.started &&
      Math.hypot(e.clientX - drag.startClient[0], e.clientY - drag.startClient[1]) < 3
    )
      return;
    const pos = canvasPoint(e);
    if (drag.key === 'anchor') {
      if (
        !updateCanvasGesture(
          { kind: 'transform-anchor', layerId: current().id, nodeId: drag.nodeId },
          {
            delta: artworkDelta(
              [[drag.geometry.pivot[0]], [drag.geometry.pivot[1]]],
              pos.map((v, i) => v - drag.start[i]),
              e,
            ),
          },
        )
      )
        return;
      drag.started = true;
      updateCanvas();
      return;
    }
    let next;
    if (drag.type === 'object-resize')
      next = resizedObjectValue(
        drag.original,
        drag.handle,
        pos.map((v, i) => v - drag.start[i]),
      );
    else if (drag.key === 'scale')
      next = scaleFromHandle(
        drag.original,
        drag.geometry,
        pos.map((v, i) => v - drag.start[i]),
      );
    else {
      const angle = rotationHandleAngle(drag.geometry.pivot, pos, 4 / canvasScale());
      if (angle === null) return;
      if (drag.angle !== null) drag.angleDelta += rotationHandleDelta(drag.angle, angle);
      drag.angle = angle;
      next = rotationFromHandle(drag.original, drag.angleDelta, e.shiftKey);
    }
    if (!(Array.isArray(next) ? next.every(Number.isFinite) : Number.isFinite(next))) return;
    if (
      !updateCanvasGesture(
        { kind: 'parameter', layerId: current().id, nodeId: drag.nodeId, key: drag.key },
        { value: next },
      )
    )
      return;
    drag.started = true;
    updateCanvas();
    return;
  }
  if (drag.type === 'position-controller') {
    const n = selectedNode(),
      q = n?.params[drag.key];
    if (
      n?.id !== drag.nodeId ||
      positionController(n)?.key !== drag.key ||
      current().locked ||
      !layerShown(current()) ||
      !q ||
      q.locked ||
      q.source ||
      q.expression
    ) {
      cancelInteraction();
      return;
    }
    if (
      !drag.started &&
      Math.hypot(e.clientX - drag.startClient[0], e.clientY - drag.startClient[1]) < 3
    )
      return;
    const pos = canvasPoint(e),
      delta = artworkDelta(
        [[drag.original[0]], [drag.original[1]]],
        pos.map((v, i) => v - drag.start[i]),
        e,
      ),
      next = drag.original.map((v, i) => v + delta[i]);
    if (!next.every(Number.isFinite)) return;
    if (
      !updateCanvasGesture(
        { kind: 'parameter', layerId: current().id, nodeId: drag.nodeId, key: drag.key },
        { value: next },
      )
    )
      return;
    drag.started = true;
    updateCanvas();
    return;
  }
  if (drag.type === 'character') {
    if (
      current().locked ||
      !layerShown(current()) ||
      characterEdit?.node !== drag.nodeId ||
      selected !== drag.nodeId ||
      characterEdit?.route !== drag.route ||
      characterEdit.selected.size !== drag.originals.length ||
      drag.originals.some((g) => !characterEdit.selected.has(g.id)) ||
      !characterOccurrences(renderEvaluation?.path).some(
        (o) =>
          o.source === drag.nodeId &&
          o.route === drag.route &&
          !o.reason &&
          !o.suspended &&
          o.matrix.every((v, i) => v === drag.matrix[i]),
      )
    ) {
      cancelInteraction();
      return;
    }
    if (
      Math.hypot(e.clientX - drag.startClient[0], e.clientY - drag.startClient[1]) < 3 &&
      !drag.started
    )
      return;
    try {
      const pos = canvasPoint(e),
        displayed = artworkDelta(
          drag.anchors,
          pos.map((v, i) => v - drag.start[i]),
          e,
        ),
        local = inverseCharacterDelta(drag.matrix, displayed);
      const controlledAxis = maintainedAxis(current().nodes.find((n) => n.id === drag.nodeId));
      if (controlledAxis !== null && (!e.shiftKey || Math.abs(local[controlledAxis]) > 1e-8)) {
        cancelInteraction();
        notify(
          'Maintained ink gaps control spacing; use cross-axis Shift-drag or disable to edit.',
          true,
        );
        return;
      }
      if (controlledAxis !== null) local[controlledAxis] = 0;
      if (
        !updateCanvasGesture(
          {
            kind: 'character-translate',
            layerId: current().id,
            nodeId: drag.nodeId,
            ids: drag.originals.map((g) => g.id),
          },
          { delta: local },
        )
      )
        return;
      drag.started = true;
      updateCanvas();
    } catch (error) {
      cancelInteraction();
      notify(error.message, true);
    }
    return;
  }
  if (drag.type === 'numeric') {
    const d = drag,
      dx = e.clientX - d.startX;
    if (!d.started && Math.abs(dx) < 4) return;
    if (
      current().id !== d.layerId ||
      (current().locked && !d.localNumeric) ||
      !d.capture.isConnected ||
      d.capture.disabled ||
      d.capture.readOnly
    ) {
      cancelInteraction();
      return;
    }
    if (d.commandTarget || d.localNumeric) {
      moveOtherNumeric(d, e, dx);
      return;
    }
    if (d.textKind) {
      moveTextNumeric(d, e, dx);
      return;
    }
    // Finish any typed draft before taking the drag snapshot. It remains its own edit.
    if (!d.started && d.capture.value !== d.capture.defaultValue) {
      const text = d.capture.value,
        expected = Number(text) / (d.factor || 1);
      d.capture.dispatchEvent(new window.Event('change', { bubbles: true }));
      const node = current().nodes.find((n) => n.id === d.nodeId),
        q = parameter(node, d.key),
        actual = d.alpha ? q.value.alpha : q.type === 'Vector2' ? q.value[d.axis] : q.value;
      if (!text.trim() || !Number.isFinite(expected) || actual !== expected) {
        cancelInteraction();
        return;
      }
    }
    const n = current().nodes.find((n) => n.id === d.nodeId),
      q = n && parameter(n, d.key);
    if (!q || current().locked || q.locked || (!d.authoredBase && (q.source || q.expression))) {
      cancelInteraction();
      return;
    }
    if (!d.started) {
      d.original = d.alpha ? q.value.alpha : q.type === 'Vector2' ? q.value[d.axis] : q.value;
      if (isTransform(n) && !n.params[d.key] && ['position', 'scaleWidth'].includes(d.key)) {
        const evaluated = renderEvaluation.transformValues(n.id)[d.key];
        d.original = q.type === 'Vector2' ? evaluated[d.axis] : evaluated;
      }
      d.gesture = beginGraphGesture({
        kind: 'parameter',
        layerId: d.layerId,
        nodeId: d.nodeId,
        key: d.key,
        transformControl: isTransform(n),
        authoredBase: d.authoredBase,
        ...(q.type === 'Vector2' ? { axis: d.axis } : {}),
        ...(d.alpha ? { alpha: true } : {}),
      });
      if (!d.gesture) {
        cancelInteraction();
        return;
      }
      d.started = true;
      d.capture.setPointerCapture?.(e.pointerId);
      d.capture.classList.add('scrubbing');
    }
    e.preventDefault();
    const [min, max] = d.alpha ? [0, 1] : numericLimits(n, d.key),
      step = Number(d.capture.dataset.scrubStep),
      // Subtract the activation threshold so a click with slight jitter never jumps.
      distance = Math.sign(dx) * Math.max(0, Math.abs(dx) - 4),
      value =
        Math.trunc(distance) === 0
          ? d.original
          : Math.max(
              min,
              Math.min(
                max,
                Number(
                  (d.original + (Math.trunc(distance) * step) / (d.factor || 1)).toPrecision(15),
                ),
              ),
            );
    if (!Number.isFinite(value)) return;
    try {
      d.gesture = session.dispatch({
        ...session.state,
        type: 'update-gesture',
        payload: { ...d.gesture, update: { value } },
      });
      d.preview = session.previewSnapshot(d.gesture.gestureId).document;
    } catch (error) {
      notify(error.message, true);
      return;
    }
    d.capture.value = String(value * (d.factor || 1));
    updateCanvas();
    return;
  }
  if (drag.type === 'graph-overview') {
    moveGraphOverview(e);
    return;
  }
  if (drag.type === 'graph-pan') {
    const viewport = document.querySelector('.graph-scroll');
    viewport.scrollLeft = drag.scroll[0] - e.clientX + drag.start[0];
    viewport.scrollTop = drag.scroll[1] - e.clientY + drag.start[1];
    return;
  }
  if (drag.type === 'resize') {
    const width = Math.max(220, Math.min(550, drag.width + (e.clientX - drag.startX) / graphZoom));
    const preview = updateGraphGesture({ width });
    if (!preview) return;
    document.querySelector(`.node[data-id="${drag.id}"]`).style.width =
      (preview.nodes.find((node) => node.id === drag.id).width ?? 264) + 'px';
    updateGraphBounds();
    drawWires();
    return;
  }
  if (drag.type === 'split') {
    graphHeight = Math.max(
      18,
      Math.min(
        75,
        drag.height +
          ((drag.startY - e.clientY) / document.querySelector('.middle').clientHeight) * 100,
      ),
    );
    document.querySelector('.graph-panel').style.height = graphHeight + '%';
    fitCanvas();
    return;
  }
  if (drag.type === 'pan') {
    pan = drag.original.map(
      (v, i) => v + [e.clientX - drag.start[0], e.clientY - drag.start[1]][i],
    );
    fitCanvas();
    return;
  }
  if (drag.type === 'node') {
    if (!drag.started && Math.hypot(e.clientX - drag.start[0], e.clientY - drag.start[1]) < 4)
      return;
    if (!drag.started) {
      drag.started = true;
      lastNameClick = null;
      graphFrame = requestAnimationFrame(graphAutoPan);
    }
    drag.pointer = [e.clientX, e.clientY];
    drag.snapBypass = e.ctrlKey || e.metaKey;
    drag.shiftKey = e.shiftKey;
    moveGraphNodes();
    return;
  }
  let pos = canvasPoint(e),
    delta = pos.map((v, i) => v - drag.start[i]),
    n = current().nodes.find((x) => x.id === drag.nodeId);
  if (drag.type === 'path-unit') {
    if (
      !drag.started &&
      Math.hypot(e.clientX - drag.startClient[0], e.clientY - drag.startClient[1]) < 3
    )
      return;
    drag.started = true;
    delta = artworkDelta([[drag.start[0]], [drag.start[1]]], delta, e);
    if (
      updateCanvasGesture(
        {
          kind: 'path-units-translate',
          layerId: current().id,
          nodeId: drag.nodeId,
          unitIds: [drag.unitId],
        },
        { delta: drag.matrix ? inverseAffineDelta(drag.matrix, delta) : delta },
      )
    )
      updateCanvas();
    return;
  }
  if (drag.type === 'point-handles') {
    if (
      !drag.started &&
      Math.hypot(e.clientX - drag.startClient[0], e.clientY - drag.startClient[1]) < 3
    )
      return;
    drag.started = true;
    delta = artworkDelta([[drag.start[0]], [drag.start[1]]], delta, e);
    if (
      updateCanvasGesture(
        {
          kind: 'point-handles',
          layerId: current().id,
          nodeId: drag.nodeId,
          pointId: drag.pointId,
        },
        { delta: drag.matrix ? inverseAffineDelta(drag.matrix, delta) : delta },
      )
    )
      updateCanvas();
    return;
  }
  if (drag.type === 'guide') {
    const sources = [[], []];
    sources[drag.axis] = [drag.original];
    delta = snapped(sources, delta, e, drag.id);
    const value = Math.max(-100000, Math.min(100000, drag.original + delta[drag.axis]));
    if (!updateCanvasGesture({ kind: 'guide-position', guideId: drag.id }, { value })) return;
    updateCanvas();
    return;
  }
  const anchor = drag.anchor || drag.start;
  if (artworkDrag()) {
    const targets =
      drag.type === 'shape'
        ? drag.targets
        : [
            {
              nodeId: drag.nodeId,
              keys: (drag.pointIds || [drag.pointId]).map(
                (id) => `${drag.type === 'remap' ? 'remap.' : ''}${id}.${drag.key}`,
              ),
            },
          ];
    const editable = targets.every((target) => {
      const node = current().nodes.find((n) => n.id === target.nodeId);
      return (
        node &&
        !node.pathAttachment &&
        target.keys.every((key) => {
          const q = parameter(node, key);
          if (drag.type === 'remap' && !q) return true;
          return q && !q.locked && !q.source && !q.expression;
        })
      );
    });
    if (current().locked || !layerShown(current()) || !editable) {
      cancelInteraction();
      return;
    }
    if (
      e.shiftKey &&
      !drag.started &&
      Math.hypot(e.clientX - drag.startClient[0], e.clientY - drag.startClient[1]) < 3
    )
      return;
    delta = artworkDelta(drag.anchors || [[anchor[0]], [anchor[1]]], delta, e);
    drag.started = true;
    drag.lastDelta = delta;
  } else if (drag.type === 'pen') delta = artworkDelta([[anchor[0]], [anchor[1]]], delta, e);
  else delta = snapped(drag.anchors || [[anchor[0]], [anchor[1]]], delta, e);
  if (['remap', 'shape', 'point'].includes(drag.type)) {
    const target =
      drag.type === 'shape'
        ? {
            kind: 'nodes-translate',
            layerId: current().id,
            nodeIds: drag.targets.map((t) => t.nodeId),
          }
        : {
            kind: drag.type === 'remap' ? 'remap-point' : 'curve-point',
            layerId: current().id,
            nodeId: drag.nodeId,
            pointId: drag.pointId,
            ...(drag.pointIds?.length > 1 ? { pointIds: drag.pointIds } : {}),
            key: drag.key,
          };
    if (drag.type === 'point' && drag.key !== 'position') drag.independent ||= e.altKey;
    const localDelta = drag.matrix ? inverseAffineDelta(drag.matrix, delta) : delta;
    if (
      !updateCanvasGesture(target, {
        delta: localDelta,
        ...(drag.type === 'point' && drag.key !== 'position'
          ? { independent: !!drag.independent }
          : {}),
      })
    )
      return;
  } else if (drag.type === 'pen') {
    try {
      penCommand('update-pen-point', {
        delta: penDraft.matrix ? inverseAffineDelta(penDraft.matrix, delta) : delta,
      });
    } catch (error) {
      notify(error.message, true);
    }
  }
  updateCanvas();
}
document.addEventListener('pointermove', movePointer);
document.addEventListener('pointermove', (e) => {
  const pending = pendingCanvasTextClick;
  if (
    pending?.pointerId === e.pointerId &&
    Math.hypot(e.clientX - pending.x, e.clientY - pending.y) >= 3
  ) {
    pending.moved = true;
    lastCanvasTextClick = null;
  }
});
document.addEventListener('pointercancel', (e) => {
  pendingCanvasTextClick = null;
  lastCanvasTextClick = null;
  lastNameClick = null;
  const pointerId = wiring?.pointerId ?? drag?.pointerId;
  if (pointerId !== undefined && pointerId !== e.pointerId) return;
  cancelInteraction();
});
document.addEventListener('pointerup', (e) => {
  if (drag?.pointerId !== undefined && drag.pointerId !== e.pointerId) return;
  if (drag?.type === 'numeric' && drag.started) {
    const n = current().nodes.find((n) => n.id === drag.nodeId),
      q = !drag.textKind && n && parameter(n, drag.key);
    if (
      current().id !== drag.layerId ||
      (current().locked && !drag.localNumeric) ||
      !drag.capture.isConnected ||
      drag.capture.disabled ||
      drag.capture.readOnly ||
      (drag.commandTarget || drag.localNumeric
        ? false
        : drag.textKind
          ? !textNumericState(drag)
          : !q || q.locked || (!drag.authoredBase && (q.source || q.expression)))
    ) {
      cancelInteraction();
      return;
    }
  }
  let artworkResolution = null;
  if (drag?.type === 'artwork') {
    artworkResolution = artworkMove();
    if (!artworkResolution.supported || !sameArtworkMove(artworkResolution.target, drag.target)) {
      cancelInteraction();
      notify(artworkResolution.reason || 'Artwork target changed', true);
      return;
    }
  }
  if (wiring) {
    if (wiring.pointerId !== undefined && wiring.pointerId !== e.pointerId) return;
    let w = wiring;
    wiring = null;
    const drop = w.capture
      ? document.elementFromPoint?.(e.clientX, e.clientY) || e.target
      : e.target;
    releaseWirePointer(w);
    if (graphFrame !== null) window.cancelAnimationFrame(graphFrame);
    graphFrame = null;
    document.querySelectorAll('.compatible').forEach((x) => x.classList.remove('compatible'));
    updateGraphBounds();
    drawWires();
    let f = drop?.closest('.graph .field');
    if (w.disconnect) {
      if (!f)
        commandChange(
          'disconnect',
          { layerId: w.layerId, nodeId: w.node, key: w.key },
          'Disconnected; fallback restored',
          false,
          w.envelope,
        );
    } else if (f)
      commandChange(
        'connect',
        {
          layerId: w.layerId,
          sourceId: w.source,
          sourcePort: w.port,
          nodeId: f.dataset.node,
          key: f.dataset.key,
        },
        'Connected',
        false,
        w.envelope,
      );
    return;
  }
  if (drag) {
    let d = drag;
    drag = null;
    if (graphFrame !== null) window.cancelAnimationFrame(graphFrame);
    graphFrame = null;
    if (d.capture?.hasPointerCapture?.(d.pointerId)) d.capture.releasePointerCapture(d.pointerId);
    if (d.type === 'path-insert') {
      if (
        !d.cancelled &&
        Math.hypot(e.clientX - d.startClient[0], e.clientY - d.startClient[1]) < 4 &&
        current().id === d.payload.layerId &&
        selectedNode()?.id === d.payload.nodeId
      ) {
        const result = commandChange(
          'insert-path-point',
          d.payload,
          'Added one source-path point; downstream point-count effects may change',
          false,
          d.envelope,
        );
        if (result?.changed) {
          const n = selectedNode();
          pointSelection = { nodeId: n.id, ids: new Set() };
          pointFocus = { nodeId: n.id, pointId: result.pointId };
          selectPointTarget(n, { pointId: result.pointId, key: 'position' }, { shiftKey: false });
        }
      } else {
        notify('Point insertion cancelled');
        render();
      }
      return;
    }
    if (d.type === 'graph-overview') {
      updateNavigation();
      return;
    }
    if (d.type === 'graph-marquee') {
      if (!d.started && !d.additive) {
        selected = null;
        selection = new Set();
        artworkTarget = null;
        artworkSelection = new Set();
        characterEdit = null;
        selectedGuide = null;
      }
      render();
      return;
    }
    snapMatches = [];
    if (d.gesture) {
      try {
        const result = session.dispatch({
          ...session.state,
          type: 'commit-gesture',
          payload: d.gesture,
        });
        if (result.changed && d.type === 'artwork') {
          selectionHistory.set(history.past.at(-1), d.selectionBefore);
          selectionHistory.set(history.doc, selectionSnapshot());
        }
      } catch (error) {
        session.dispatch({
          ...session.state,
          type: 'cancel-gesture',
          payload: { gestureId: d.gesture.gestureId },
        });
        notify(error.message, true);
      }
      if (
        d.type === 'node' &&
        !d.started &&
        d.nameTarget &&
        nameClicked(current().id, d.nameTarget, e)
      )
        return;
      if (d.type === 'character' && d.toggleOnClick && !d.started)
        chooseCharacter(d.toggleOnClick, true);
      if (d.type === 'artwork' && d.toggleOnClick && !d.started)
        selectArtwork(d.toggleOnClick, { toggle: true });
      render(
        false,
        d.type === 'artwork' && !(d.toggleOnClick && !d.started) ? artworkResolution : null,
      );
      return;
    }
    if (d.type === 'pen') {
      try {
        penCommand('accept-pen-point');
      } catch (error) {
        discardPenDraft();
        notify(error.message, true);
        render();
        return;
      }
      notify('Draft path · Enter / Escape: finish · Cancel path: discard');
      render();
      document.querySelector('.canvas-stage').focus({ preventScroll: true });
      return;
    }
    if (d.type === 'numeric' && !d.started) return;
    if (d.type === 'node' && !d.started && d.nameTarget) {
      if (nameClicked(current().id, d.nameTarget, e)) return;
    }
    if (d.type === 'character' && d.toggleOnClick && !d.started)
      chooseCharacter(d.toggleOnClick, true);
    const toggledArtwork = d.type === 'artwork' && d.toggleOnClick && !d.started;
    if (toggledArtwork) selectArtwork(d.toggleOnClick, { toggle: true });
    render(false, d.type === 'artwork' && !toggledArtwork ? artworkResolution : null);
  }
});
// A second press may become a drag. Only completed stationary clicks may enter
// text editing, after the normal move gesture has released its ownership.
document.addEventListener('pointerup', (e) => {
  const pending = pendingCanvasTextClick;
  if (!pending || pending.pointerId !== e.pointerId) return;
  pendingCanvasTextClick = null;
  if (
    pending.moved ||
    Math.hypot(e.clientX - pending.x, e.clientY - pending.y) >= 3 ||
    pending.sessionId !== session.id ||
    pending.revision !== session.state.expectedRevision ||
    tool !== 'select' ||
    composing ||
    session.hasTextComposition
  ) {
    lastCanvasTextClick = null;
    return;
  }
  const prior = lastCanvasTextClick,
    now = performance.now();
  lastCanvasTextClick = { layerId: pending.layerId, time: now, x: e.clientX, y: e.clientY };
  if (
    prior?.layerId === pending.layerId &&
    now - prior.time < 500 &&
    Math.hypot(e.clientX - prior.x, e.clientY - prior.y) < 5 &&
    beginCanvasTextEdit(pending.layerId, e)
  ) {
    lastCanvasTextClick = null;
    e.preventDefault();
  }
});
function duplicateGraphNodes(nodeId) {
  if (deletionBusy() || variableAction) return;
  const ids = selection.has(nodeId) && selection.size > 1 ? [...selection] : [nodeId];
  if (!ids.length || ids.some((id) => !current().nodes.some((n) => n.id === id))) return;
  const measurements = {};
  for (const el of document.querySelectorAll('.graph .node'))
    measurements[el.dataset.id] = { width: el.offsetWidth, height: el.offsetHeight };
  const result = commandChange(
    'duplicate-nodes',
    { nodeIds: ids, offset: adjacentNodeOffset(current().nodes, ids, measurements) },
    'Nodes duplicated',
    true,
  );
  if (result) {
    artworkTarget = null;
    artworkSelection = new Set();
    selected = result.nodeIds[0];
    selection = new Set(result.nodeIds);
    render();
    document.querySelector('.graph-scroll')?.focus({ preventScroll: true });
    revealNode(selected);
  }
}
function nodePresetControls() {
  const presets = history.doc.nodePresets || [];
  const chosen = presets.find((p) => p.id === selectedNodePreset);
  const blocked = current().locked || deletionBusy() || variableAction || osTextDrafts.size;
  return `<div class="node-preset-controls" role="group" aria-label="Node presets">
    <select id="node-preset" aria-label="Node preset"><option value="">Presets</option>${presets.map((p) => `<option value="${esc(p.id)}" ${p === chosen ? 'selected' : ''}>${esc(p.name)}</option>`).join('')}</select>
    <input id="node-preset-name" aria-label="Preset name" placeholder="Preset name" maxlength="100" value="${esc(chosen?.name || '')}">
    <button data-action="save-node-preset" ${blocked || !selection.size ? 'disabled' : ''}>Save preset</button>
    <button data-action="apply-node-preset" ${blocked || !chosen ? 'disabled' : ''}>Apply preset</button>
    <button data-action="update-node-preset" ${blocked || !chosen || !selection.size ? 'disabled' : ''}>Update preset</button>
  </div>`;
}
document.addEventListener('change', (event) => {
  if (event.target.id !== 'node-preset') return;
  selectedNodePreset = event.target.value || null;
  const chosen = history.doc.nodePresets?.find((p) => p.id === selectedNodePreset);
  const input = document.querySelector('#node-preset-name');
  if (input) input.value = chosen?.name || '';
  const blocked = current().locked || deletionBusy() || variableAction || osTextDrafts.size;
  for (const type of ['apply', 'update']) {
    const button = document.querySelector(`[data-action="${type}-node-preset"]`);
    if (button) button.disabled = !!(blocked || !chosen || (type === 'update' && !selection.size));
  }
});
function changeNodePreset(type) {
  if (deletionBusy() || variableAction || osTextDrafts.size) return;
  const payload =
    type === 'apply-node-preset'
      ? { presetId: selectedNodePreset }
      : {
          nodeIds: [...selection],
          name: document.querySelector('#node-preset-name').value,
          ...(type === 'update-node-preset' ? { presetId: selectedNodePreset } : {}),
        };
  const result = commandChange(
    type,
    payload,
    type === 'apply-node-preset'
      ? 'Preset applied'
      : type === 'update-node-preset'
        ? 'Preset updated'
        : 'Preset saved',
    true,
  );
  if (!result) return;
  selectedNodePreset = result.presetId;
  if (result.nodeIds) {
    artworkTarget = null;
    artworkSelection = new Set();
    pointSelection = { nodeId: null, ids: new Set() };
    selected = result.nodeIds[0];
    selection = new Set(result.nodeIds);
  }
  render();
  if (result.nodeIds) revealNode(selected);
}
function copyGraphNodes() {
  if (deletionBusy() || variableAction) return;
  if (osTextDrafts.size) {
    notify('Apply or cancel the Text edit before copying nodes', true);
    return;
  }
  try {
    const result = session.dispatch({
      ...session.state,
      type: 'copy-nodes',
      payload: { layerId: current().id, nodeIds: [...selection] },
    });
    notify(
      `${result.copiedCount} nodes copied · this Document only${result.externalInputCount ? ` · ${result.externalInputCount} external input(s) need the original Composition` : ''}${result.skippedOutputs ? ` · ${result.skippedOutputs} Output skipped` : ''}`,
    );
  } catch (error) {
    notify(error.message, true);
  }
}
function pasteGraphNodes() {
  if (deletionBusy() || variableAction) return;
  if (osTextDrafts.size) {
    notify('Apply or cancel the Text edit before pasting nodes', true);
    return;
  }
  const result = commandChange('paste-nodes', {}, 'Nodes pasted', true);
  if (!result) return;
  artworkTarget = null;
  artworkSelection = new Set();
  pointSelection = { nodeId: null, ids: new Set() };
  selected = result.nodeIds[0];
  selection = new Set(result.nodeIds);
  render();
  notify(
    `${result.nodeIds.length} nodes pasted${result.externalInputCount ? ' · external inputs still use original nodes' : ''}`,
  );
  document.querySelector('.graph-scroll')?.focus({ preventScroll: true });
  revealNode(selected);
}
function closeNodeContextMenu(restore = false) {
  if (!nodeContextMenu) return;
  const focus = nodeContextMenu.dataset.restoreFocus || '.graph-scroll';
  nodeContextMenu.remove();
  nodeContextMenu = null;
  contextMenuScroll.clear();
  if (restore) document.querySelector(focus)?.focus({ preventScroll: true });
}
function openNodeContextMenu(nodeId, x, y) {
  closeNodeContextMenu();
  if (deletionBusy() || variableAction) {
    notify('Finish or cancel the current edit before using node actions', true);
    return;
  }
  const n = current().nodes.find((n) => n.id === nodeId);
  if (!n) return;
  if (!selection.has(nodeId)) selection = new Set([nodeId]);
  selected = nodeId;
  artworkTarget = null;
  artworkSelection = new Set();
  selectedGuide = null;
  render();
  const owner = session,
    revision = session.state.expectedRevision,
    layerId = current().id;
  const nodes = current().nodes.filter((n) => selection.has(n.id));
  const deletable = nodes.filter(
    (n) => !n.locked && n.id !== current().output && !['Output', 'Image Output'].includes(n.type),
  );
  const output = current().nodes.find((n) => n.id === current().output);
  const locked = current().locked ? 'Composition is locked' : '';
  const outputReason =
    locked ||
    (['Output', 'Image Output'].includes(n.type) || n.id === current().output
      ? 'This is already an Output node'
      : !['Path', 'PaintedPath', 'Image'].includes(outputType(n))
        ? 'This node has no artwork output'
        : !output ||
            (!socketCompatibleFor(n, output, 'path') &&
              !(n.type === 'Mask' && output.type === 'Image Output'))
          ? 'Incompatible with this composition’s Output'
          : output.locked || output.params.path.locked
            ? 'Output input is locked'
            : '');
  const duplicateReason =
    locked ||
    (current().nodes.length + nodes.length > 2000
      ? 'Too many nodes'
      : nodes.some(
            (n) => n.type === 'Path Remap' && n.edits.length && selection.has(n.params.path.source),
          )
        ? 'Duplicate an edited Path Remap separately from its source'
        : '');
  const actions = [
    ['output', 'Use as output', outputReason, 'Connect only this node to the composition Output'],
    [
      'duplicate',
      nodes.length > 1 ? `Duplicate ${nodes.length} nodes` : 'Duplicate',
      duplicateReason,
      'Duplicate the selected nodes (one Undo)',
    ],
    [
      'delete',
      deletable.length > 1 ? `Delete ${deletable.length} nodes` : 'Delete',
      locked || (!deletable.length ? 'Locked and Output nodes cannot be deleted' : ''),
      nodes.length !== deletable.length
        ? 'Delete selected nodes; Output nodes are retained; locked nodes are retained'
        : 'Delete the selected nodes (one Undo)',
    ],
    ['copy', 'Copy nodes · Ctrl+C', '', 'Copy selected source/effect nodes in this Document'],
    ['paste', 'Paste nodes · Ctrl+V', locked, 'Paste copied nodes into this Composition'],
  ];
  const menu = document.createElement('div');
  menu.className = 'node-context-menu';
  menu.setAttribute('role', 'menu');
  menu.setAttribute('aria-label', `Node actions: ${nodeDisplayName(n)}`);
  menu.tabIndex = -1;
  menu.innerHTML = actions
    .map(
      ([id, label, reason, hint]) =>
        `<button type="button" role="menuitem" data-node-context-action="${id}" ${reason ? 'disabled' : ''} title="${esc(reason || hint)}">${esc(label)}${reason ? `<small>${esc(reason)}</small>` : ''}</button>`,
    )
    .join('');
  menu.addEventListener('click', (event) => {
    if (nodeContextMenu !== menu) return;
    const button = event.target.closest('button');
    if (!button || button.disabled) return;
    const action = button.dataset.nodeContextAction;
    closeNodeContextMenu(true);
    if (
      session !== owner ||
      session.state.expectedRevision !== revision ||
      current().id !== layerId ||
      deletionBusy() ||
      variableAction
    )
      return;
    if (action === 'copy') copyGraphNodes();
    else if (action === 'paste') pasteGraphNodes();
    else if (action === 'output')
      commandChange('preview-node', { nodeId, position: freePosition('Fill') }, 'Output connected');
    if (action === 'duplicate') duplicateGraphNodes(nodeId);
    if (action === 'delete') {
      const result = commandChange(
        'delete-nodes',
        { nodeIds: nodes.map((n) => n.id) },
        'Selected nodes deleted',
        true,
      );
      if (result) {
        selection = new Set(result.retainedNodeIds);
        selected = result.retainedNodeIds[0] || null;
        pointSelection = { nodeId: null, ids: new Set() };
        render();
      }
    }
    document.querySelector('.graph-scroll')?.focus({ preventScroll: true });
  });
  placeContextMenu(menu, x, y);
}
function placeContextMenu(menu, x, y, focus = '.graph-scroll') {
  menu.dataset.restoreFocus = focus;
  document.body.append(menu);
  nodeContextMenu = menu;
  const rect = menu.getBoundingClientRect();
  menu.style.left = `${Math.max(4, Math.min(x, window.innerWidth - rect.width - 4))}px`;
  menu.style.top = `${Math.max(4, Math.min(y, window.innerHeight - rect.height - 4))}px`;
  (menu.querySelector('button:not(:disabled)') || menu).focus({ preventScroll: true });
  // render() restores these scroll owners synchronously; their queued scroll
  // events must not dismiss the menu that was just opened at those positions.
  contextMenuScroll = new Map(
    [
      document,
      document.documentElement,
      document.body,
      ...app.querySelectorAll('.graph-scroll,.inspector,.layers,.guides-panel,.guide-list'),
    ].map((el) => {
      const owner = el === document ? document.scrollingElement || document.documentElement : el;
      return [el, [owner.scrollLeft || 0, owner.scrollTop || 0]];
    }),
  );
}
function openArtworkContextMenu(layerId, x, y) {
  closeNodeContextMenu();
  if (deletionBusy() || variableAction) return;
  const layer = history.doc.layers.find((layer) => layer.id === layerId);
  if (!layer) return;
  // Unlike a plain click, an intentionally empty Layer remains an exact target.
  if (!artworkTarget || !artworkSelection.has(layerId)) artworkSelection = new Set([layerId]);
  activeLayer = artworkTarget = layerId;
  selected = null;
  selection = new Set();
  selectedGuide = null;
  characterEdit = null;
  pointSelection = { nodeId: null, ids: new Set() };
  render();
  const owner = session,
    revision = session.state.expectedRevision,
    ids = [...artworkSelection],
    selectionKey = JSON.stringify(ids),
    layers = history.doc.layers.filter((layer) => artworkSelection.has(layer.id)),
    locked = layers.some((layer) => layer.locked) ? 'Unlock selected layers first' : '',
    duplication =
      ids.length > 1
        ? resolveArtworkDuplicateSelection(history.doc, artworkSelection, { fonts })
        : {
            supported: history.doc.layers.length < 100,
            reason: 'Duplicate requires fewer than 100 layers.',
          };
  const menu = document.createElement('div');
  menu.className = 'node-context-menu artwork-context-menu';
  menu.setAttribute('role', 'menu');
  menu.setAttribute('aria-label', `Artwork actions: ${layer.name}`);
  menu.tabIndex = -1;
  menu.innerHTML = [
    [
      'duplicate',
      ids.length > 1 ? `Duplicate ${ids.length} layers` : 'Duplicate layer',
      locked || (!duplication.supported ? duplication.reason : ''),
    ],
    ['delete', ids.length > 1 ? `Delete ${ids.length} layers` : 'Delete layer', locked],
  ]
    .map(
      ([action, label, reason]) =>
        `<button type="button" role="menuitem" data-artwork-context-action="${action}" ${reason ? 'disabled' : ''} title="${esc(reason || label)}">${esc(label)}${reason ? `<small>${esc(reason)}</small>` : ''}</button>`,
    )
    .join('');
  menu.addEventListener('click', (event) => {
    if (nodeContextMenu !== menu) return;
    const button = event.target.closest('button');
    if (!button || button.disabled) return;
    const action = button.dataset.artworkContextAction;
    closeNodeContextMenu(true);
    if (
      session !== owner ||
      session.state.expectedRevision !== revision ||
      artworkTarget !== layerId ||
      JSON.stringify([...artworkSelection]) !== selectionKey ||
      deletionBusy() ||
      variableAction
    )
      return;
    if (action === 'duplicate') {
      if (ids.length === 1) duplicateFocusedLayer(layerId);
      else duplicateArtwork(layerId, selectionKey);
    } else if (action === 'delete') deleteArtworkSelection(ids);
  });
  placeContextMenu(menu, x, y, '.canvas-stage');
}
function deleteArtworkSelection(layerIds) {
  if (deletionBusy() || variableAction) return;
  const priorUI = selectionSnapshot(true);
  const result = commandChange('delete-layers', { layerIds }, 'Layers deleted', true);
  if (!result) return;
  resetTransient();
  activeLayer = result.nextLayerId;
  selected = artworkTarget = null;
  selection = new Set();
  artworkSelection = new Set();
  tool = 'select';
  selectionHistory.set(history.past.at(-1), priorUI);
  selectionHistory.set(history.doc, selectionSnapshot(true));
  render();
  document.querySelector('.canvas-stage')?.focus({ preventScroll: true });
}
function openGraphBackgroundMenu(x, y) {
  closeNodeContextMenu();
  if (deletionBusy() || variableAction) return;
  const owner = session,
    revision = session.state.expectedRevision,
    layerId = current().id;
  const menu = document.createElement('div');
  menu.className = 'node-context-menu';
  menu.setAttribute('role', 'menu');
  menu.setAttribute('aria-label', 'Graph actions');
  menu.tabIndex = -1;
  menu.innerHTML = `<button type="button" role="menuitem" data-node-context-action="paste" ${current().locked ? 'disabled' : ''}>Paste nodes · Ctrl+V</button>`;
  menu.addEventListener('click', (event) => {
    if (nodeContextMenu !== menu || !event.target.closest('button:not(:disabled)')) return;
    closeNodeContextMenu(true);
    if (
      session === owner &&
      session.state.expectedRevision === revision &&
      current().id === layerId
    )
      pasteGraphNodes();
  });
  placeContextMenu(menu, x, y);
}
// One document-wide guard also covers portals, menus, dialogs and body margins.
// Text fields keep the platform's ordinary selection/Cut/Copy/Paste menu.
document.addEventListener(
  'contextmenu',
  (event) => {
    if (!editingControl(event.target)) event.preventDefault();
    if (!app.contains(event.target) || editingControl(event.target)) closeNodeContextMenu();
  },
  true,
);
document.addEventListener(
  'pointerdown',
  (event) => {
    if (nodeContextMenu && !nodeContextMenu.contains(event.target)) closeNodeContextMenu();
  },
  true,
);
document.addEventListener(
  'keydown',
  (event) => {
    if (!nodeContextMenu) return;
    if (event.key === 'Escape' || event.key === 'Tab') {
      event.preventDefault();
      event.stopImmediatePropagation();
      closeNodeContextMenu(true);
      return;
    }
    if (!nodeContextMenu.contains(event.target)) return;
    // A menu owns its keyboard input; do not trigger graph/global shortcuts.
    event.stopImmediatePropagation();
    const buttons = [...nodeContextMenu.querySelectorAll('button:not(:disabled)')];
    const index = buttons.indexOf(document.activeElement);
    if (['ArrowDown', 'ArrowUp', 'Home', 'End'].includes(event.key)) {
      event.preventDefault();
      const next =
        event.key === 'Home'
          ? 0
          : event.key === 'End'
            ? buttons.length - 1
            : (index + (event.key === 'ArrowDown' ? 1 : -1) + buttons.length) % buttons.length;
      buttons[next]?.focus();
    } else if (['Enter', ' '].includes(event.key)) {
      event.preventDefault();
      if (!event.repeat) buttons[index]?.click();
    }
  },
  true,
);
window.addEventListener('resize', () => closeNodeContextMenu());
document.addEventListener(
  'scroll',
  (event) => {
    if (!nodeContextMenu || nodeContextMenu.contains(event.target)) return;
    const prior = contextMenuScroll.get(event.target);
    const owner =
      event.target === document
        ? document.scrollingElement || document.documentElement
        : event.target;
    if (prior && prior[0] === (owner.scrollLeft || 0) && prior[1] === (owner.scrollTop || 0))
      return;
    closeNodeContextMenu();
  },
  true,
);
app.addEventListener('contextmenu', (e) => {
  if (editingControl(e.target)) return;
  closeNodeContextMenu();
  const input = e.target.closest('.input');
  if (input) {
    if (!deletionBusy() && !variableAction) {
      const f = input.closest('.field');
      commandChange('disconnect', { nodeId: f.dataset.node, key: f.dataset.key }, 'Disconnected');
    }
    return;
  }
  const card = e.target.closest('.graph .node');
  if (card) return openNodeContextMenu(card.dataset.id, e.clientX, e.clientY);
  const layer = e.target.closest(
    '.layer[data-layer],.artboard [data-layer],[data-artwork-selection]',
  );
  if (layer)
    return openArtworkContextMenu(
      layer.dataset.layer || layer.dataset.artworkSelection,
      e.clientX,
      e.clientY,
    );
  if (e.target.closest('.graph-scroll')) openGraphBackgroundMenu(e.clientX, e.clientY);
});
app.addEventListener(
  'toggle',
  () =>
    requestAnimationFrame(() => {
      updateGraphBounds();
      drawWires();
    }),
  true,
);
// Own selection and clipboard shortcuts only inside the graph.
document.addEventListener('keydown', (event) => {
  const key = event.key.toLowerCase();
  if (
    event.defaultPrevented ||
    !(event.ctrlKey || event.metaKey) ||
    event.shiftKey ||
    event.altKey ||
    !['a', 'c', 'v'].includes(key) ||
    navigationExcluded(event.target) ||
    !event.target.closest?.('.graph-scroll,.graph-toolbar') ||
    event.isComposing ||
    event.keyCode === 229 ||
    composing
  )
    return;
  // Keep ordinary text Copy, including a native selected text range.
  if (key === 'c' && window.getSelection()?.toString()) return;
  event.preventDefault();
  event.stopImmediatePropagation();
  if (event.repeat) return;
  if (key === 'a') {
    if (deletionBusy() || variableAction) return;
    selection = new Set(current().nodes.map((n) => n.id));
    if (!selection.has(selected)) selected = current().nodes[0]?.id || null;
    artworkTarget = null;
    artworkSelection = new Set();
    characterEdit = null;
    selectedGuide = null;
    pointSelection = { nodeId: null, ids: new Set() };
    window.getSelection()?.removeAllRanges();
    render();
    document.querySelector('.graph-scroll')?.focus({ preventScroll: true });
  } else if (key === 'c') copyGraphNodes();
  else pasteGraphNodes();
});
document.addEventListener('keydown', (event) => {
  if (
    event.defaultPrevented ||
    !(event.ctrlKey || event.metaKey) ||
    event.shiftKey ||
    event.altKey ||
    event.key.toLowerCase() !== 'd' ||
    navigationExcluded(event.target) ||
    !event.target.closest?.('.graph-scroll,.graph-toolbar')
  )
    return;
  event.preventDefault();
  event.stopImmediatePropagation();
  if (
    event.repeat ||
    event.isComposing ||
    event.keyCode === 229 ||
    deletionBusy() ||
    variableAction
  )
    return;
  if (selected && selection.has(selected)) duplicateGraphNodes(selected);
});
document.addEventListener('keydown', (e) => {
  if (navigationExcluded(e.target) || e.isComposing || e.keyCode === 229 || composing) return;
  const viewport = e.target.closest('.canvas-stage,.canvas-toolbar,.graph-scroll,.graph-toolbar');
  if (!viewport) return;
  const graph = viewport.matches('.graph-scroll,.graph-toolbar');
  const modifier = e.ctrlKey || e.metaKey;
  const fit = !modifier && e.shiftKey && (e.code === 'Digit1' || e.key === '1' || e.key === '!');
  const fitSelection =
    graph && !modifier && e.shiftKey && (e.code === 'Digit2' || e.key === '2' || e.key === '@');
  const actual = modifier && !e.shiftKey && e.key === '1';
  const zoomStep = !modifier && !e.altKey && ['+', '=', '-', '_'].includes(e.key);
  if (!fit && !fitSelection && !actual && !zoomStep) return;
  e.preventDefault();
  e.stopImmediatePropagation();
  if (navigationBusy()) return;
  if (fit) graph ? fitGraph() : fitArtboard();
  else if (fitSelection) fitGraph(true);
  else if (graph) zoomGraph(actual ? 1 : graphZoom * (['+', '='].includes(e.key) ? 1.2 : 1 / 1.2));
  else zoomCanvas(actual ? 1 : zoom * (['+', '='].includes(e.key) ? 1.2 : 1 / 1.2));
});
for (const type of ['keydown', 'keyup'])
  document.addEventListener(type, (e) => {
    if (drag?.type !== 'node' || !['Control', 'Meta'].includes(e.key)) return;
    drag.snapBypass = e.ctrlKey || e.metaKey;
    if (drag.started) moveGraphNodes();
  });
document.addEventListener('keydown', (e) => {
  const numericExpressionInput = e.target.matches?.(
    'input[type="number"], input[data-numeric-expression]',
  );
  if (
    numericExpressionInput &&
    !e.isComposing &&
    e.key.length === 1 &&
    /[+\-*\/%().]/.test(e.key) &&
    !e.ctrlKey &&
    !e.metaKey &&
    !e.altKey
  ) {
    // Native number controls reject operators before the draft can reach the
    // shared change/command path. Keep the control's number semantics until a
    // draft actually needs expression syntax.
    e.preventDefault();
    const value = e.target.value;
    e.target.dataset.numericExpression = 'true';
    e.target.type = 'text';
    e.target.inputMode = 'decimal';
    e.target.value = `${value}${e.key}`;
    e.target.setSelectionRange(e.target.value.length, e.target.value.length);
  }
  if (numericExpressionInput && e.key === 'Escape' && !e.isComposing && e.keyCode !== 229) {
    e.preventDefault();
    e.target.value = e.target.defaultValue;
    e.target.type = 'number';
    delete e.target.dataset.numericExpression;
    return;
  }
  if (numericExpressionInput && e.key === 'Enter' && !e.isComposing && e.keyCode !== 229) {
    const input = e.target;
    const source = input.value.trim();
    if (source && /[+\-*\/%()]/.test(source)) {
      e.preventDefault();
      try {
        input.value = String(expression(source, undefined, false, Number(input.defaultValue)));
        input.type = 'number';
        delete input.dataset.numericExpression;
        input.dispatchEvent(new window.Event('change', { bubbles: true }));
      } catch (error) {
        input.value = input.defaultValue;
        input.type = 'number';
        delete input.dataset.numericExpression;
        notify('Invalid calculation', true);
      }
      return;
    }
    if (input.dataset.handlePolar) {
      e.preventDefault();
      input.dispatchEvent(new window.Event('change', { bubbles: true }));
      return;
    }
  }
  if (e.target.matches?.('[data-rename-name]')) {
    if (e.isComposing || e.keyCode === 229 || nameEdit?.composing) return;
    if (e.key === 'Enter' || e.key === 'Escape') {
      e.preventDefault();
      if (nameEdit) nameEdit.value = e.target.value;
      finishNameEdit(e.key === 'Enter');
    }
    return;
  }
  if (e.key === 'F2' && artworkTarget && e.target.matches?.('.canvas-stage')) {
    e.preventDefault();
    if (artworkSelection.size > 1) notify('Select one artwork to rename.', true);
    else beginNameEdit(artworkTarget);
    return;
  }
  if (e.key === 'F2' || (e.key === 'Enter' && e.target.matches?.('[data-rename-node]'))) {
    const title = e.target.closest?.('[data-rename-node], [data-select-layer]');
    if (title) {
      e.preventDefault();
      beginNameEdit(title.dataset.selectLayer || current().id, title.dataset.renameNode || null);
      return;
    }
  }
  if (directionDrag() && e.key === 'Shift') {
    if (drag.lastPointer) movePointer({ ...drag.lastPointer, shiftKey: true, altKey: e.altKey });
    return;
  }
  if (drag?.type === 'node' && e.key === 'Shift') {
    drag.shiftKey = true;
    moveGraphNodes();
    return;
  }
  if (drag?.type === 'path-insert' && e.key === 'Escape') {
    e.preventDefault();
    cancelInteraction();
    return;
  }
  if (drag?.type === 'numeric' && e.key === 'Escape') {
    e.preventDefault();
    cancelInteraction();
    return;
  }
  if (drag?.type === 'numeric' && drag.started) {
    e.preventDefault();
    return;
  }
  if (e.target.matches?.('[data-base]') && !e.isComposing && e.keyCode !== 229) {
    if (e.key === 'Escape') {
      e.preventDefault();
      e.target.value = e.target.defaultValue;
      e.target.blur();
      return;
    }
    if (e.key === 'Enter') {
      e.preventDefault();
      e.target.dispatchEvent(new window.Event('change', { bubbles: true }));
      return;
    }
  }
  if (
    ['pan', 'graph-pan', 'graph-marquee', 'graph-overview'].includes(drag?.type) &&
    e.key === 'Escape'
  ) {
    // Navigation is view-only: stop at the current offset without undo or selection changes.
    e.preventDefault();
    cancelInteraction();
    return;
  }
  if (
    [
      'character',
      'artwork',
      'position-controller',
      'shape',
      'point',
      'remap',
      'transform',
      'object-resize',
      'point-handles',
      'path-unit',
    ].includes(drag?.type) &&
    e.key === 'Escape'
  ) {
    e.preventDefault();
    cancelInteraction();
    return;
  }
  if (
    (artworkDrag() && e.key === 'Delete') ||
    ['transform', 'object-resize', 'point-handles', 'path-unit'].includes(drag?.type)
  ) {
    // A pending direct manipulation owns its document snapshot. Do not let
    // Delete interleave a second document action. Transform handles also block
    // history/tool shortcuts; artwork tools keep their existing cancel-and-switch behavior.
    e.preventDefault();
    e.stopImmediatePropagation();
    return;
  }
  if (e.key === 'Escape' && e.target.dataset?.arrangementGap) {
    const n = current().nodes.find((n) => n.id === e.target.dataset.arrangementGap);
    e.target.value = String(n?.characterArrangement?.gap ?? 10);
    e.preventDefault();
    return;
  }
  if (e.key === 'Escape' && e.target.dataset?.textWrapWidth) {
    const n = current().nodes.find((n) => n.id === e.target.dataset.textWrapWidth);
    e.target.value = String(wrapExtent(n.textWrap));
    e.preventDefault();
    return;
  }
  if (e.key === 'Escape' && e.target.dataset?.characterKerning !== undefined) {
    const pair = pairContext();
    e.target.value = pair?.manual ? String(pair.value) : '';
    e.preventDefault();
    return;
  }
  if (e.key === 'Escape' && e.target.dataset?.textLeadingValue) {
    const n = current().nodes.find((n) => n.id === e.target.dataset.textLeadingValue);
    e.target.value = String(n.leading.value);
    e.preventDefault();
    return;
  }
  if (
    e.key === 'Escape' &&
    e.target.matches?.('[data-character-transform], [data-character-axis], [data-character-style]')
  ) {
    const input = e.target,
      context = characterContext();
    if (context) {
      const property = input.dataset.characterTransform || input.dataset.characterStyle,
        axis = Number(input.dataset.characterAxis);
      const values = (
        input.dataset.characterStyle ? context.typographySelected : context.selected
      ).map((g) => (property ? g[property] : g.offset[axis]));
      input.value = values.some((v) => v !== values[0])
        ? ''
        : String(
            (values[0] ?? (property === 'scale' ? 1 : 0)) *
              (['width', 'height'].includes(input.dataset.characterStyle) ? 100 : 1),
          );
      if (input.dataset.characterStyle === 'font')
        input.value = values.some((v) => v !== values[0]) ? '__mixed' : values[0];
    }
    e.preventDefault();
    return;
  }
  if (
    e.target.matches?.('textarea[data-text]') &&
    !e.isComposing &&
    !composing &&
    !textDrafts.get(e.target.dataset.text)?.composition
  ) {
    if (e.key === 'Escape') {
      const draft = textDrafts.get(e.target.dataset.text);
      if (draft)
        session.dispatch({
          ...session.state,
          type: 'cancel-text-draft',
          payload: { draftId: draft.state.draftId },
        });
      textDrafts.delete(e.target.dataset.text);
      textPreview = null;
      canvasTextEdit = null;
      e.target.value = current().nodes.find((n) => n.id === e.target.dataset.text)?.text || '';
      e.preventDefault();
      e.stopImmediatePropagation();
      render();
      return;
    }
    if (e.key === 'Enter' && (e.ctrlKey || e.metaKey)) {
      e.preventDefault();
      e.target.dispatchEvent(new Event('change', { bubbles: true }));
      document.querySelector('.canvas-stage').focus({ preventScroll: true });
      return;
    }
  }
  if (
    editingControl(e.target) ||
    e.target.closest('dialog') ||
    e.isComposing ||
    e.keyCode === 229 ||
    composing
  )
    return;
  if (
    penDraft &&
    (e.key === 'Escape' || (e.key === 'Enter' && !e.target.closest('button,[role="button"],a')))
  ) {
    e.preventDefault();
    finishPenDraft();
    return;
  }
  if (penDraft && (e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 'z') {
    e.preventDefault();
    if (e.shiftKey) {
      notify('Finish or cancel the draft path before Redo');
      return;
    }
    discardPenDraft();
    notify('Draft path cancelled');
    render();
    document.querySelector('.canvas-stage').focus({ preventScroll: true });
    return;
  }
  if (['Delete', 'Backspace'].includes(e.key) && e.target.matches?.('.canvas-stage') && e.repeat) {
    e.preventDefault();
    return;
  }
  if (
    ['Delete', 'Backspace'].includes(e.key) &&
    e.target.matches?.('.canvas-stage') &&
    pointSelection.ids.size
  ) {
    // Point selection owns both delete keys, including rejected/generated
    // targets. Never let these keys cascade into whole-node/artwork deletion.
    e.preventDefault();
    if (e.repeat || e.ctrlKey || e.metaKey || e.altKey || deletionBusy()) return;
    const n = current().nodes.find((node) => node.id === pointSelection.nodeId);
    if (!n || !['Curve', 'Multi Path'].includes(n.type)) {
      notify('Select an editable source path to delete points', true);
      return;
    }
    commandChange(
      'delete-point',
      { nodeId: n.id, pointIds: [...pointSelection.ids] },
      'Selected points removed',
    );
    document.querySelector('.canvas-stage')?.focus({ preventScroll: true });
    return;
  }
  if (
    tool === 'direct' &&
    ['Delete', 'Backspace'].includes(e.key) &&
    !e.target.closest('.graph-scroll')
  ) {
    e.preventDefault();
    notify('Select points to delete');
    return;
  }
  if (
    e.target.matches?.('.canvas-stage') &&
    pointSelection.ids.size &&
    ['ArrowLeft', 'ArrowRight', 'ArrowUp', 'ArrowDown'].includes(e.key)
  ) {
    e.preventDefault();
    if (drag || penDraft || e.ctrlKey || e.metaKey || e.altKey || deletionBusy()) return;
    const n = current().nodes.find((n) => n.id === pointSelection.nodeId),
      context = directPointContext(n);
    if (!n || (context && (!context.supported || context.locked))) {
      notify(context?.reason || 'Select an editable source path', true);
      return;
    }
    const step = e.shiftKey ? 10 : 1,
      world = [
        e.key === 'ArrowLeft' ? -step : e.key === 'ArrowRight' ? step : 0,
        e.key === 'ArrowUp' ? -step : e.key === 'ArrowDown' ? step : 0,
      ];
    try {
      const delta = context?.matrix ? inverseAffineDelta(context.matrix, world) : world;
      commandChange(
        'translate-points',
        { nodeId: n.id, pointIds: [...pointSelection.ids], delta },
        'Selected points moved',
      );
      document.querySelector('.canvas-stage')?.focus({ preventScroll: true });
    } catch (error) {
      notify(error.message, true);
    }
    return;
  }
  if (
    tool === 'direct' &&
    e.target.matches?.('.canvas-stage') &&
    ['ArrowLeft', 'ArrowRight', 'ArrowUp', 'ArrowDown'].includes(e.key)
  ) {
    e.preventDefault();
    return;
  }
  if (['Delete', 'Backspace'].includes(e.key) && deletionBusy()) {
    e.preventDefault();
    return;
  }
  if (
    characterEdit &&
    e.target.closest('.canvas-stage') &&
    ['ArrowLeft', 'ArrowRight', 'ArrowUp', 'ArrowDown'].includes(e.key)
  ) {
    e.preventDefault();
    if (drag) return;
    const context = characterContext();
    if (!context?.occurrence || !context.selected.length) return;
    try {
      const step = e.shiftKey ? 10 : 1,
        delta = inverseCharacterDelta(context.occurrence.matrix, [
          e.key === 'ArrowLeft' ? -step : e.key === 'ArrowRight' ? step : 0,
          e.key === 'ArrowUp' ? -step : e.key === 'ArrowDown' ? step : 0,
        ]);
      commandChange(
        'set-character-offsets',
        {
          nodeId: context.n.id,
          changes: Object.fromEntries(
            context.selected.map((g) => [g.id, g.offset.map((v, i) => v + delta[i])]),
          ),
        },
        'Character positions updated',
      );
      document.querySelector('.canvas-stage').focus({ preventScroll: true });
    } catch (err) {
      notify(err.message, true);
    }
    return;
  }
  if (
    e.target.matches?.('.canvas-stage') &&
    ['ArrowLeft', 'ArrowRight', 'ArrowUp', 'ArrowDown'].includes(e.key) &&
    !e.ctrlKey &&
    !e.metaKey &&
    !e.altKey &&
    !characterEdit &&
    !document.querySelector('dialog[open]') &&
    !navigationBusy()
  ) {
    if (!artworkTarget) return;
    e.preventDefault();
    const resolved = artworkMove();
    if (!resolved.supported) {
      notify(resolved.reason, true);
      return;
    }
    const step = e.shiftKey ? 10 : 1,
      delta = [
        e.key === 'ArrowLeft' ? -step : e.key === 'ArrowRight' ? step : 0,
        e.key === 'ArrowUp' ? -step : e.key === 'ArrowDown' ? step : 0,
      ];
    // No asynchronous work or document mutation intervenes before this write.
    // The next preview/key/commit still resolves independently from live state.
    const priorUI = selectionSnapshot();
    const oldPast = history.past.at(-1);
    commandChange(
      'translate-artworks',
      { layerIds: [...artworkSelection], delta },
      'Artwork position updated',
    );
    if (history.past.at(-1) !== oldPast) {
      selectionHistory.set(history.past.at(-1), priorUI);
      selectionHistory.set(history.doc, selectionSnapshot());
    }
    document.querySelector('.canvas-stage').focus({ preventScroll: true });
    return;
  }
  if (e.code === 'Space' && !e.target.closest('button,[role="button"]')) {
    // A busy surface must not fall through to the browser's Space-to-scroll action.
    if (e.target.closest('.canvas-stage,.graph-scroll')) e.preventDefault();
    if (!navigationBusy()) space = true;
    updateNavigation();
  }
  if (e.key === 'Escape') {
    characterEdit = null;
    cancelInteraction();
    layoutDraft = null;
    layoutPreview = false;
    drag = null;
    snapMatches = [];
    selectedGuide = null;
    wiring = null;
    tool = 'select';
    penId = null;
    render();
  }
  if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 'z') {
    e.preventDefault();
    const graphScroll = stepHistory(e.shiftKey);
    render(false, null, graphScroll);
  }
  if (e.key === 'Delete' && selectedGuide) {
    const result = editGuides('delete-guide', { guideId: selectedGuide });
    if (result) {
      selectedGuide = null;
      render();
    }
    return;
  }
  if (e.key === 'Delete' && artworkTarget) {
    // Whole-artwork deletion is explicit, focused and independent of move support.
    // Clearing selection after success prevents a held/repeated Delete cascading.
    if (e.target.matches?.('.canvas-stage') && !e.ctrlKey && !e.metaKey && !e.altKey) {
      e.preventDefault();
      if (artworkSelection.size > 1)
        notify('Select one artwork to delete, or use an explicit Delete layer row action.', true);
      else if (!e.repeat) deleteArtwork(artworkTarget);
    }
    return;
  }
  if (['Delete', 'Backspace'].includes(e.key) && e.target.closest?.('.graph-scroll')) {
    e.preventDefault();
    if (e.repeat || current().locked || deletionBusy()) return;
    const nodeIds = current()
      .nodes.filter((n) => selection.has(n.id) && !['Output', 'Image Output'].includes(n.type))
      .map((n) => n.id);
    if (!nodeIds.length) {
      notify('Select a node other than Output to delete');
      return;
    }
    if (
      nodeIds.length &&
      commandChange('delete-nodes', { nodeIds }, 'Selected nodes deleted', true)
    ) {
      selection.clear();
      selected = null;
      pointSelection = { nodeId: null, ids: new Set() };
      render();
    }
    return;
  }
  if (e.key === 'Delete' && !['Output', 'Image Output'].includes(selectedNode()?.type))
    document.querySelector(`[data-delete="${selected}"]`)?.click();
});
document.addEventListener('keydown', (e) => {
  if (
    editingControl(e.target) ||
    e.target.closest('dialog') ||
    e.isComposing ||
    e.keyCode === 229 ||
    composing ||
    e.repeat
  )
    return;
  if (e.ctrlKey || e.metaKey) {
    if (e.key.toLowerCase() === 's') {
      e.preventDefault();
      document.querySelector(`[data-action="${e.shiftKey ? 'save-as' : 'save'}"]`)?.click();
    }
    if (e.key.toLowerCase() === 'o') {
      e.preventDefault();
      document.querySelector('[data-action="open"]')?.click();
    }
    return;
  }
  if (e.altKey) return;
  const keys = { a: 'direct', v: 'select', p: 'pen', c: 'Circle', r: 'Rectangle', t: 'Text' };
  if (keys[e.key.toLowerCase()])
    document.querySelector(`[data-tool="${keys[e.key.toLowerCase()]}"]`)?.click();
});
app.addEventListener('keydown', (e) => {
  if (e.target.matches('.splitter') && ['ArrowUp', 'ArrowDown'].includes(e.key)) {
    e.preventDefault();
    graphHeight = Math.max(18, Math.min(75, graphHeight + (e.key === 'ArrowUp' ? 5 : -5)));
    document.querySelector('.graph-panel').style.height = graphHeight + '%';
    fitCanvas();
  }
});
document.addEventListener('keyup', (e) => {
  if (directionDrag() && e.key === 'Shift') {
    drag.axis = null;
    drag.direction = null;
    if (drag.lastPointer) movePointer({ ...drag.lastPointer, shiftKey: false, altKey: e.altKey });
  }
  if (drag?.type === 'node' && e.key === 'Shift') {
    drag.shiftKey = false;
    drag.direction = null;
    moveGraphNodes();
  }
  if (e.code === 'Space') {
    space = false;
    updateNavigation();
  }
});
window.addEventListener('resize', fitCanvas);
window.addEventListener('beforeunload', (e) => {
  if (!desktop && (documents.hasDraft() || documents.dirty || documents.saving)) {
    e.preventDefault();
    e.returnValue = '';
  }
});
render();
registerClose(async () => {
  try {
    const canClose = await documents.guard('closing Nect');
    updateDocumentStatus();
    if (canClose) await recovery?.flush();
    return canClose;
  } catch (error) {
    notify(error.message, true);
    return false;
  }
}).catch((error) => notify(`Close protection unavailable: ${error.message}`, true));
new window.MutationObserver(updateDocumentStatus).observe(app, { childList: true });
if (desktop && typeof listOsOutlineFonts === 'function') void refreshOsFontCatalog();
updateDocumentStatus();
if (desktop)
  showRecoveryCopies(true).catch((error) => {
    recovery.report(recovery.current, { phase: 'failed', error: error.message || String(error) });
  });
if (desktop)
  listSystemFonts()
    .then((items) => {
      systemFonts = items;
      systemFontStatus = items.length ? '' : 'No Windows fonts found. Use Load TTF / OTF.';
      render();
    })
    .catch(() => {
      systemFontStatus = 'Windows font discovery unavailable. Use Load TTF / OTF.';
      render();
    });
fetch('/fonts/DejaVuSans.ttf')
  .then((r) => {
    if (!r.ok) throw Error('Bundled font unavailable');
    return r.arrayBuffer();
  })
  .then((b) => {
    bundledFont = parseFont(b);
    render();
  })
  .catch((e) => notify(e.message, true));
// Refresh actual disabled state after gesture handlers have established/released ownership.
app.addEventListener('pointerdown', updateLayerDeletionControls);
// Bubble after text/rename handlers have cleared their own composition state.
app.addEventListener('compositionend', updateLayerDeletionControls);
document.addEventListener('pointerup', updateLayerDeletionControls);
document.addEventListener('pointercancel', updateLayerDeletionControls);
// Explicit test/debug API; same model and mutations as the UI.
window.nect = {
  get recoveryState() {
    return recovery && { ...recovery.state };
  },
  get gestureState() {
    return session.gestureState;
  },
  previewSnapshot: (gestureId) => session.previewSnapshot(gestureId),
  get commandState() {
    return session.state;
  },
  dispatch(command) {
    const result = session.dispatch(command);
    if (!['begin-text-draft', 'stage-text-draft', 'cancel-text-draft'].includes(command.type))
      render();
    return result;
  },
  snapshot: () => session.snapshot(),
  get document() {
    return session.snapshot();
  },
  get fileState() {
    return {
      dirty: documents.dirty,
      revision: documents.revision,
      savedRevision: documents.savedRevision,
      binding: documents.binding && { ...documents.binding },
      saving: documents.saving,
    };
  },
  evaluate: () => {
    const doc = session.snapshot();
    return evaluate(
      doc.layers.find((l) => l.id === activeLayer) || doc.layers[0],
      fonts,
      doc.imageAssets,
    );
  },
  get activeCompositionId() {
    return session.activeCompositionId;
  },
  get compositions() {
    return session.projectSnapshot().compositions.map(({ id, name, width, height }) => ({
      id,
      name,
      width,
      height,
    }));
  },
  projectSnapshot: () => session.projectSnapshot(),
  load(text) {
    const project = deserializeProject(text),
      activeDocument = activeCompositionDocument(project, project.compositions[0].id);
    if (hasVariableFonts(activeDocument))
      return prepareOpenDocument(project).then(() => render(true));
    replaceDocument(project);
    render(true);
  },
  export: (options) => exportSVG(history.doc, fonts, options),
};

app.addEventListener('keydown', (event) => {
  if (event.key === 'Escape' && event.target.matches('[data-grid-opacity]')) cancelGridOpacity();
});
document.addEventListener('pointercancel', cancelGridOpacity);

app.addEventListener('click', async (event) => {
  const button = event.target.closest('[data-recovery-open], [data-recovery-retry]');
  if (!button || !recovery) return;
  try {
    if (button.hasAttribute('data-recovery-open')) await showRecoveryCopies();
    else if (recovery.state.phase === 'failed') await recovery.retry();
  } catch (error) {
    notify(error.message || String(error), true);
  }
});

// Keep the command surface compact without changing command ownership.
const commandMenuObserver = new MutationObserver(() => {
  ensureCommandMenus(app, document);
});
commandMenuObserver.observe(app, { childList: true });
app.append(document.createComment('command-menu-init'));
document.addEventListener('keydown', (event) => {
  if (!(event.ctrlKey || event.metaKey) || event.altKey || event.isComposing || event.repeat)
    return;
  if (event.target.closest?.('input, textarea, select, [contenteditable="true"], dialog')) return;
  const row = event.target.closest?.('[data-layer]');
  if (!row || !app.contains(row)) return;
  const layerId = row.dataset.layer;
  if (event.key.toLowerCase() === 'c') {
    layerClipboard = [layerId];
    event.preventDefault();
    notify('Layer copied');
  } else if (event.key.toLowerCase() === 'v' && layerClipboard?.length) {
    event.preventDefault();
    const sourceId = layerClipboard.find((id) =>
      history.doc.layers.some((layer) => layer.id === id),
    );
    if (!sourceId) return;
    duplicateFocusedLayer(sourceId);
  } else if (event.key.toLowerCase() === 'd') {
    event.preventDefault();
    duplicateFocusedLayer(layerId);
  }
});
render();
document.addEventListener('click', (event) => {
  if (event.target.closest('.command-menu')) return;
  document.querySelectorAll('.command-menu[open]').forEach((menu) => (menu.open = false));
  if (event.target.closest('.canvas-toolbar,.guides-panel,.alignment-panel')) return;
  if (alignmentPreferences.open || gridOpen || layoutOpen || guidesOpen) {
    closeTransientPanels();
    render();
  }
});
document.addEventListener('keydown', (event) => {
  if (event.key !== 'Escape') return;
  const open = [...document.querySelectorAll('.command-menu[open]')];
  if (!open.length && !alignmentPreferences.open && !gridOpen && !layoutOpen && !guidesOpen) return;
  event.preventDefault();
  open.forEach((menu) => (menu.open = false));
  if (open.length) open[0].querySelector('summary')?.focus();
  if (alignmentPreferences.open || gridOpen || layoutOpen || guidesOpen) {
    closeTransientPanels();
    render();
  }
});

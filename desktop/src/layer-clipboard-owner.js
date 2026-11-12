import {
  artworkReferencesInLayers,
  ARTWORK_REFERENCE_SCHEMA_VERSION,
  COMPONENT_COMPOSITION_REFERENCE_SCHEMA_VERSION,
  COMPOSITION_PROPERTY_REFERENCE_SCHEMA_VERSION,
  QUALIFIED_COLOR_REFERENCE_SCHEMA_VERSION,
  nodesHaveComponentCompositionReferenceSyntax,
  nodesHaveCompositionPropertyReferenceSyntax,
  nodesHaveQualifiedColorReferenceSyntax,
} from './curve-point-reference.js';

// The token contains no caller-supplied Layer. Its resolver remains private to
// the owning clipboard and refuses retired or temporarily unavailable copies.
const capabilities = new WeakMap();
function freezeSnapshot(value) {
  if (value && typeof value === 'object' && !Object.isFrozen(value)) {
    for (const child of Object.values(value)) freezeSnapshot(child);
    Object.freeze(value);
  }
  return value;
}
export function resolveLayerClipboardToken(token, owner, compositionId) {
  const resolve = token && capabilities.get(token);
  if (!resolve) throw Error('Copied Layer is unavailable; copy it again');
  return resolve(owner, compositionId);
}

export function createLayerClipboardOwner({ currentGeneration = null } = {}) {
  const noRestore = Symbol('unavailable Layer history');
  let copied = null;
  const generationMatches = (record) =>
    !currentGeneration || currentGeneration() === record.generation;
  const available = (record) =>
    record.restoreToken === null &&
    generationMatches(record) &&
    record.owner.hasLayer(record.compositionId, record.layerId) &&
    record.owner.hasLayerClipboardResources(record.compositionId, record.resources);
  return {
    get hasCopy() {
      return copied !== null;
    },
    copy({ owner, generation, compositionId, layerId }) {
      const previous = copied,
        revision = owner.projectRevision,
        envelope = owner.state;
      const assertCurrent = () => {
        if (
          (currentGeneration && currentGeneration() !== generation) ||
          owner.activeCompositionId !== compositionId ||
          owner.projectRevision !== revision ||
          copied !== previous ||
          owner.state.sessionId !== envelope.sessionId ||
          owner.state.expectedRevision !== envelope.expectedRevision
        )
          throw Error('Project changed; copy the Layer again');
      };
      assertCurrent();
      const doc = owner.historyView.doc,
        source = doc.layers.find((layer) => layer.id === layerId);
      if (!source) throw Error('Layer is unavailable');
      if (source.locked) throw Error('Unlock this Layer before copying it');
      const artworkReferences = artworkReferencesInLayers([source]);
      if (doc.schemaVersion < ARTWORK_REFERENCE_SCHEMA_VERSION && artworkReferences.length)
        throw Error('Copying an inactive Artwork reference is not supported');
      if (artworkReferences.some((ref) => ref.layerId === layerId))
        throw Error('Copying a self-referencing Artwork is not supported');
      if (
        (doc.schemaVersion >= COMPONENT_COMPOSITION_REFERENCE_SCHEMA_VERSION &&
          nodesHaveComponentCompositionReferenceSyntax(source.nodes)) ||
        (doc.schemaVersion >= COMPOSITION_PROPERTY_REFERENCE_SCHEMA_VERSION &&
          nodesHaveCompositionPropertyReferenceSyntax(source.nodes)) ||
        (doc.schemaVersion >= QUALIFIED_COLOR_REFERENCE_SCHEMA_VERSION &&
          nodesHaveQualifiedColorReferenceSyntax(source.nodes))
      )
        throw Error('Copying this Layer reference is not supported');
      const fontIds = new Set(
          source.nodes
            .filter((item) => item.type === 'Text')
            .flatMap((item) => [
              item.font,
              ...Object.values(item.characterPositions?.styles || {}).map((style) => style.font),
            ])
            .filter((id) => id && id !== 'default'),
        ),
        imageIds = new Set(
          source.nodes.filter((item) => item.type === 'Image').map((item) => item.asset),
        ),
        slotIds = new Set(
          source.nodes.flatMap((item) =>
            Object.values(item.params || {})
              .filter((q) => q?.type === 'Color' && q.colorSlotId !== undefined)
              .map((q) => q.colorSlotId),
          ),
        ),
        layerIds = new Set(
          source.nodes.map((item) => item.compositionReference?.compositionId).filter(Boolean),
        );
      const resources = {
        fonts: [...fontIds].map((id) => [id, doc.fontAssets?.[id]?.data]),
        images: [...imageIds].map((id) => [
          id,
          doc.imageAssets?.[id]?.data,
          doc.imageAssets?.[id]?.width,
          doc.imageAssets?.[id]?.height,
        ]),
        slotIds: [...slotIds],
        layerIds: [...layerIds],
        sourceLayerId: layerId,
        artworkReferences: artworkReferences
          .filter((ref) => ref.layerId !== layerId)
          .map((ref) => ({
            ...ref,
            ownerType: doc.layers
              .find((layer) => layer.id === ref.layerId)
              ?.nodes.find((item) => item.id === ref.nodeId)?.type,
          })),
      };
      if (!owner.hasLayerClipboardResources(compositionId, resources))
        throw Error('A copied Layer resource is unavailable');
      const capture = Object.freeze({
        layerId,
        snapshot: freezeSnapshot(structuredClone(source)),
        schemaVersion: doc.schemaVersion,
      });
      assertCurrent();
      const record = {
        owner,
        generation,
        compositionId,
        layerId,
        restoreToken: null,
        resources,
        capture,
        token: Object.freeze({}),
      };
      capabilities.set(record.token, (requestedOwner, requestedCompositionId) => {
        if (
          copied !== record ||
          requestedOwner !== record.owner ||
          requestedCompositionId !== record.compositionId ||
          !available(record)
        )
          throw Error('Copied Layer is unavailable; restore it or copy again');
        return capture;
      });
      copied = record;
    },
    clear() {
      copied = null;
    },
    observe({ owner, generation, operation, history }) {
      if (!copied) return;
      if (copied.owner !== owner || copied.generation !== generation) {
        copied = null;
        return;
      }
      const exists =
        owner.hasLayer(copied.compositionId, copied.layerId) &&
        owner.hasLayerClipboardResources(copied.compositionId, copied.resources);
      if (copied.restoreToken !== null) {
        if (exists && history.entryToken === copied.restoreToken) copied.restoreToken = null;
      } else if (!exists) {
        copied.restoreToken =
          (operation === 'undo' ? history.redoToken : history.undoToken) ?? noRestore;
      }
    },
    source({ owner, generation, compositionId }) {
      if (!copied) return null;
      if (copied.owner !== owner || copied.generation !== generation) {
        copied = null;
        return null;
      }
      if (copied.compositionId !== compositionId || !available(copied)) return null;
      return copied.layerId;
    },
    token(context) {
      return this.source(context) ? copied.token : null;
    },
  };
}

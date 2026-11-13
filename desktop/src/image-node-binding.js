export function evaluateImage(_operation, read, context) {
  const asset = context.imageAssetId();
  if (!Object.hasOwn(context.imageAssets, asset)) throw Error('Missing image asset');
  const size = read('size');
  if (size.some((value) => value <= 0 || value > 100000))
    throw Error('Image size must be positive and at most 100000');
  return { kind: 'image', asset, position: read('position'), size };
}

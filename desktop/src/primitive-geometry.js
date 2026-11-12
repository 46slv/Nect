import { fromPaper, getPaperScope } from './paper-runtime.js';

export function primitiveGeometry(type, nodeId, read) {
  const scope = getPaperScope(),
    center = read('center');
  let item;
  if (type === 'Circle')
    item = new scope.Path.Circle({
      center,
      radius: Math.max(0, read('radius')),
      insert: false,
    });
  else {
    let size = read('size').map(Math.abs),
      rectangle = new scope.Rectangle(center[0] - size[0] / 2, center[1] - size[1] / 2, ...size);
    item =
      type === 'Ellipse'
        ? new scope.Path.Ellipse({ rectangle, insert: false })
        : new scope.Path.Rectangle({ rectangle, insert: false });
  }
  const result = fromPaper(item, nodeId);
  item.remove();
  return result;
}

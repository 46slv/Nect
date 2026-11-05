export function signature(path) {
  return JSON.stringify(
    path.contours.map((c) => [
      c.closed,
      c.points.map((p) => [p.position, p.handleA, p.handleB]),
      ...(c.group === undefined ? [] : [c.group]),
      ...(c.repeaterOpacity === undefined ? [] : [c.repeaterOpacity]),
      ...(c.repeaterFill === undefined ? [] : [c.repeaterFill]),
      ...(c.repeaterStroke === undefined ? [] : [c.repeaterStroke]),
    ]),
  );
}

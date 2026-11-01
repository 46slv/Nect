// Discovery status only. Native preparation remains the exact-face authority.
export async function readOsFontCatalog({ listFamilies, listFaces }) {
  const readers = [listFamilies, listFaces];
  const results = await Promise.allSettled(
    readers.map((read) =>
      typeof read === 'function' ? Promise.resolve().then(read) : Promise.resolve(null),
    ),
  );
  return {
    families: results[0].status === 'fulfilled' ? results[0].value : null,
    faces: results[1].status === 'fulfilled' ? results[1].value : null,
    familiesError: results[0].status === 'rejected',
    facesError: results[1].status === 'rejected',
  };
}
// The normal picker reports native discovery separately from file candidates.
// A successful empty catalog is not a failed or still-pending discovery.
export function osFontDiscoveryStatus({
  desktop = true,
  pending = false,
  faces = null,
  facesError = false,
} = {}) {
  if (!desktop) return '';
  if (pending) return 'Checking native fonts…';
  if (facesError) return 'Native font discovery failed';
  if (faces === null) return 'Native fonts not checked';
  if (!faces.length) return 'No native font faces found';
  return `${faces.length} native font ${faces.length === 1 ? 'face' : 'faces'}`;
}
export function osFontAvailability(
  ref,
  {
    desktop = true,
    pending = false,
    families = null,
    faces = null,
    familiesError = false,
    facesError = false,
  } = {},
) {
  if (!desktop) return 'Saved outlines · Windows required to edit';
  if (pending) return 'Checking font…';
  const modern = ref.version >= 2,
    rows = modern ? faces : families;
  if (modern ? facesError : familiesError) return 'Font check failed · Refresh fonts';
  if (rows === null) return 'Font availability not checked';
  const active = modern
    ? rows.some((face) =>
        ['family', 'name', 'weight', 'style', 'stretch', 'format', 'color'].every(
          (key) => face[key] === ref.face[key],
        ),
      )
    : rows.some((family) => family.family === ref.family && family.styles.includes(ref.style));
  return active ? '' : 'Font unavailable · saved outlines retained';
}

# Procedural production fixtures

`radial-ornament.nect` is original synthetic artwork authored in a live Nect
desktop through `scripts/create_radial_demo.py`. It contains five retained
primitives, one point correction, two Repeaters, multiple paint entries, a
cross-operation property link and a neutral Group. The 85 exported SVG paint
layers are derived from those sources, not 85 independently stored shapes.

Open the native file, drill into Radial ornament, and edit Petal's radius or
its North point. Change the Repeater angle to revise both the petals and the
linked outer rays. The repeat rule is fixed step, so arbitrary count changes do
not automatically close the circle. Reorder a Repeater before paint to inspect
the compound-path alternative. Undo restores the previous procedural state.

The `.svg` is the exported appearance reference; use `.nect` for editing.
No external media, fonts or imported third-party artwork are included.

// Small, authored workspace symbols. The button owns the accessible name;
// SVGs are presentation only and never depend on an installed symbol font.
const paths = {
  direct: '<path d="M5 3v14l3.6-3.8 2.7 5.6 2-1-2.7-5.4 5.2-.6z"/><rect x="17" y="3" width="3" height="3"/>',
  select: '<path fill="currentColor" d="M5 3v14l3.6-3.8 2.7 5.6 2-1-2.7-5.4 5.2-.6z"/>',
  pen: '<path d="M12 3 5 16l4 3 10-7-4-1-2-4z"/><path d="m5 16 5-5"/><circle cx="11" cy="10" r="1"/>',
  Circle: '<circle cx="12" cy="12" r="8"/>',
  Rectangle: '<rect x="4" y="4" width="16" height="16"/>',
  Polygon: '<path d="m12 3 7.8 4.5v9L12 21l-7.8-4.5v-9z"/>',
  Star: '<path d="m12 3 2.7 5.7 6.3.8-4.6 4.4 1.1 6.2L12 17.2l-5.5 2.9 1.1-6.2L3 9.5l6.3-.8z"/>',
  Text: '<path d="M4 5h16M12 5v14m-4 0h8"/>',
  assets: '<rect x="3.5" y="4.5" width="17" height="15" rx="1"/><circle cx="9" cy="9" r="1.5"/><path d="m5 17 4.5-4 2.8 2.3 2.5-2.7L19 17"/>',
  colors: '<path d="M12 3.5 A8.5 8.5 0 0 1 19.36 7.75 L14.17 10.75 A2.5 2.5 0 0 0 12 9.5 Z" fill="#dc5656" stroke="none"/><path d="M19.36 7.75 A8.5 8.5 0 0 1 19.36 16.25 L14.17 13.25 A2.5 2.5 0 0 0 14.17 10.75 Z" fill="#e59a39" stroke="none"/><path d="M19.36 16.25 A8.5 8.5 0 0 1 12 20.5 L12 14.5 A2.5 2.5 0 0 0 14.17 13.25 Z" fill="#d2bb45" stroke="none"/><path d="M12 20.5 A8.5 8.5 0 0 1 4.64 16.25 L9.83 13.25 A2.5 2.5 0 0 0 12 14.5 Z" fill="#58a676" stroke="none"/><path d="M4.64 16.25 A8.5 8.5 0 0 1 4.64 7.75 L9.83 10.75 A2.5 2.5 0 0 0 9.83 13.25 Z" fill="#5492ca" stroke="none"/><path d="M4.64 7.75 A8.5 8.5 0 0 1 12 3.5 L12 9.5 A2.5 2.5 0 0 0 9.83 10.75 Z" fill="#a579bd" stroke="none"/><circle cx="12" cy="12" r="8.5"/><circle cx="12" cy="12" r="2.5"/><path d="M12 3.5v6m0 5v6m-7.36-12.75 5.19 3m4.34 2.5 5.19 3M4.64 16.25l5.19-3m4.34-2.5 5.19-3"/>',
  layers: '<path d="m12 3 9 5-9 5-9-5zM3 12l9 5 9-5M3 16l9 5 9-5"/>',
  align: '<path d="M4 3v18M20 3v18M7 8h10M7 16h10"/><path d="m9 6-2 2 2 2m6-4 2 2-2 2M9 14l-2 2 2 2m6-4 2 2-2 2"/>',
  inspector: '<path d="M4 6h3m4 0h9M4 12h9m4 0h3M4 18h3m4 0h9"/><circle cx="9" cy="6" r="2"/><circle cx="15" cy="12" r="2"/><circle cx="9" cy="18" r="2"/>',
};

export function workspaceIcon(name) {
  const shape = paths[name];
  if (!shape) throw new Error(`Unknown workspace icon: ${name}`);
  return `<svg class="workspace-icon" viewBox="0 0 24 24" aria-hidden="true" focusable="false" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">${shape}</svg>`;
}

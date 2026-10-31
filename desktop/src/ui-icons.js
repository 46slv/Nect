// Lucide source and modifications are documented in public/licenses/LUCIDE-NOTICE.txt.
// Button labels own semantics; SVGs are presentation only.
const paths = {
  direct:
    '<path d="M12.586 12.586 19 19"/><path fill="Canvas" d="M3.688 3.037a.497.497 0 0 0-.651.651l6.5 15.999a.501.501 0 0 0 .947-.062l1.569-6.083a2 2 0 0 1 1.448-1.479l6.124-1.579a.5.5 0 0 0 .063-.947z"/><rect x="1" y="1" width="5.5" height="5.5" rx=".5" fill="Canvas" stroke-width="1.7"/>',
  select:
    '<path d="M12.586 12.586 19 19"/><path fill="currentColor" d="M3.688 3.037a.497.497 0 0 0-.651.651l6.5 15.999a.501.501 0 0 0 .947-.062l1.569-6.083a2 2 0 0 1 1.448-1.479l6.124-1.579a.5.5 0 0 0 .063-.947z"/>',
  pen: '<path d="m18 13-.654-4.736a4 4 0 00-2.756-3.266l-9.163-2.9a2 2 0 00-2.017.493l-.82.819a2 2 0 00-.492 2.017l2.9 9.163a4 4 0 003.266 2.756L13 18"/><path d="M18.648 12.352a1.205 1.205 0 011.704 0l1.296 1.296a1.205 1.205 0 010 1.704l-6.296 6.296a1.205 1.205 0 01-1.704 0l-1.296-1.296a1.205 1.205 0 010-1.704z"/><path d="m3 3 6.586 6.586"/><circle cx="11" cy="11" r="2"/>',
  Circle: '<circle cx="12" cy="12" r="10"/>',
  Rectangle: '<rect width="18" height="18" x="3" y="3" rx="2"/>',
  Polygon:
    '<path d="M21 16V8a2 2 0 0 0-1-1.73l-7-4a2 2 0 0 0-2 0l-7 4A2 2 0 0 0 3 8v8a2 2 0 0 0 1 1.73l7 4a2 2 0 0 0 2 0l7-4A2 2 0 0 0 21 16z"/>',
  Star: '<path d="M11.525 2.295a.53.53 0 0 1 .95 0l2.31 4.679a2.123 2.123 0 0 0 1.595 1.16l5.166.756a.53.53 0 0 1 .294.904l-3.736 3.638a2.123 2.123 0 0 0-.611 1.878l.882 5.14a.53.53 0 0 1-.771.56l-4.618-2.428a2.122 2.122 0 0 0-1.973 0L6.396 21.01a.53.53 0 0 1-.77-.56l.881-5.139a2.122 2.122 0 0 0-.611-1.879L2.16 9.795a.53.53 0 0 1 .294-.906l5.165-.755a2.122 2.122 0 0 0 1.597-1.16z"/>',
  Text: '<path d="M12 4v16"/><path d="M4 7V5a1 1 0 0 1 1-1h14a1 1 0 0 1 1 1v2"/><path d="M9 20h6"/>',
  'Text-vertical':
    '<g transform="translate(0 2) scale(.72)"><path d="M12 4v16"/><path d="M4 7V5a1 1 0 0 1 1-1h14a1 1 0 0 1 1 1v2"/><path d="M9 20h6"/></g><path d="M20 5v14m-3-3 3 3 3-3"/>',
  assets:
    '<path d="M8.3 10a.7.7 0 0 1-.626-1.079L11.4 3a.7.7 0 0 1 1.198-.043L16.3 8.9a.7.7 0 0 1-.572 1.1Z"/><rect x="3" y="14" width="7" height="7" rx="1"/><circle cx="17.5" cy="17.5" r="3.5"/>',
  undo: '<path d="M9 14 4 9l5-5"/><path d="M4 9h10.5a5.5 5.5 0 0 1 5.5 5.5a5.5 5.5 0 0 1-5.5 5.5H11"/>',
  redo: '<path d="m15 14 5-5-5-5"/><path d="M20 9H9.5A5.5 5.5 0 0 0 4 14.5A5.5 5.5 0 0 0 9.5 20H13"/>',
  expression:
    '<rect width="18" height="18" x="3" y="3" rx="2" ry="2"/><path d="M9 17c2 0 2.8-1 2.8-2.8V10c0-2 1-3.3 3.2-3"/><path d="M9 11.2h5.7"/>',
  colors:
    '<path d="M12 3.5 A8.5 8.5 0 0 1 19.36 7.75 L14.17 10.75 A2.5 2.5 0 0 0 12 9.5 Z" fill="#dc5656" stroke="none"/><path d="M19.36 7.75 A8.5 8.5 0 0 1 19.36 16.25 L14.17 13.25 A2.5 2.5 0 0 0 14.17 10.75 Z" fill="#e59a39" stroke="none"/><path d="M19.36 16.25 A8.5 8.5 0 0 1 12 20.5 L12 14.5 A2.5 2.5 0 0 0 14.17 13.25 Z" fill="#d2bb45" stroke="none"/><path d="M12 20.5 A8.5 8.5 0 0 1 4.64 16.25 L9.83 13.25 A2.5 2.5 0 0 0 12 14.5 Z" fill="#58a676" stroke="none"/><path d="M4.64 16.25 A8.5 8.5 0 0 1 4.64 7.75 L9.83 10.75 A2.5 2.5 0 0 0 9.83 13.25 Z" fill="#5492ca" stroke="none"/><path d="M4.64 7.75 A8.5 8.5 0 0 1 12 3.5 L12 9.5 A2.5 2.5 0 0 0 9.83 10.75 Z" fill="#a579bd" stroke="none"/><circle cx="12" cy="12" r="8.5"/><circle cx="12" cy="12" r="2.5"/><path d="M12 3.5v6m0 5v6m-7.36-12.75 5.19 3m4.34 2.5 5.19 3M4.64 16.25l5.19-3m4.34-2.5 5.19-3"/>',
  layers:
    '<path d="M13 13.74a2 2 0 0 1-2 0L2.5 8.87a1 1 0 0 1 0-1.74L11 2.26a2 2 0 0 1 2 0l8.5 4.87a1 1 0 0 1 0 1.74z"/><path d="m20 14.285 1.5.845a1 1 0 0 1 0 1.74L13 21.74a2 2 0 0 1-2 0l-8.5-4.87a1 1 0 0 1 0-1.74l1.5-.845"/>',
  align:
    '<rect width="6" height="14" x="2" y="5" rx="2"/><rect width="6" height="10" x="16" y="7" rx="2"/><path d="M12 2v20"/>',
  inspector:
    '<path d="M10 5H3"/><path d="M12 19H3"/><path d="M14 3v4"/><path d="M16 17v4"/><path d="M21 12h-9"/><path d="M21 19h-5"/><path d="M21 5h-7"/><path d="M8 10v4"/><path d="M8 12H3"/>',
  eye: '<path d="M2.062 12.348a1 1 0 0 1 0-.696 10.75 10.75 0 0 1 19.876 0 1 1 0 0 1 0 .696 10.75 10.75 0 0 1-19.876 0"/><circle cx="12" cy="12" r="3"/>',
  'eye-off':
    '<path d="M10.733 5.076a10.744 10.744 0 0 1 11.205 6.575 1 1 0 0 1 0 .696 10.747 10.747 0 0 1-1.444 2.49"/><path d="M14.084 14.158a3 3 0 0 1-4.242-4.242"/><path d="M17.479 17.499a10.75 10.75 0 0 1-15.417-5.151 1 1 0 0 1 0-.696 10.75 10.75 0 0 1 4.446-5.143"/><path d="m2 2 20 20"/>',
};

export function workspaceIcon(name) {
  const shape = paths[name];
  if (!shape) throw new Error(`Unknown workspace icon: ${name}`);
  return `<svg class="workspace-icon" data-icon="${name}" viewBox="0 0 24 24" aria-hidden="true" focusable="false" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">${shape}</svg>`;
}

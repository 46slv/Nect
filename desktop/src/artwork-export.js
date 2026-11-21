// Shared artwork output. The caller owns lifecycle, snapshot selection, and
// destination permission; this operation never chooses or changes a document.
export function createArtworkExporter({ exportSVG, pngDimensions, rasterizeSVG }) {
  return async function exportArtwork({ document, fonts, format, svgOptions, pngOptions }, write) {
    if (format === 'svg') {
      const contents = exportSVG(document, fonts, svgOptions);
      return { written: Boolean(await write(contents, 'nect-artwork.svg', 'svg')) };
    }
    if (format === 'png') {
      const size = pngDimensions(document, pngOptions);
      const svg = exportSVG(document, fonts);
      const ppi = pngOptions.mode === 'ppi' ? pngOptions.ppi : null;
      const contents = await rasterizeSVG(svg, size, ppi);
      return { written: Boolean(await write(contents, 'nect-artwork.png', 'png')), size };
    }
    throw new TypeError('Unsupported artwork export format');
  };
}

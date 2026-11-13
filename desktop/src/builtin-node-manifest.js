import { snapshotNodeDeclaration } from './node-foundation.js';
import { mathDeclaration } from './math-node-declaration.js';
import { evaluateMath } from './math-node-binding.js';
import { separateVectorDeclaration } from './separate-vector-declaration.js';
import { evaluateSeparateVector, projectSeparateVector } from './separate-vector-binding.js';
import { vectorMathDeclaration } from './vector-math-declaration.js';
import { evaluateVectorMath } from './vector-math-binding.js';
import { reversePathDeclaration } from './reverse-path-declaration.js';
import { evaluateReversePath } from './reverse-path-binding.js';
import { joinGeometryDeclaration } from './join-geometry-declaration.js';
import { evaluateJoinGeometry } from './join-geometry-binding.js';
import { combineVectorDeclaration } from './combine-vector-declaration.js';
import { evaluateCombineVector } from './combine-vector-binding.js';
import {
  valueDeclaration,
  vectorDeclaration,
  expressionDeclaration,
} from './value-source-declarations.js';
import { evaluateValueSource } from './value-source-binding.js';
import { toggleDeclaration } from './toggle-node-declaration.js';
import { nullPositionDeclaration } from './null-position-declaration.js';
import { evaluateNullPosition } from './null-position-binding.js';
import { colorDeclaration } from './color-node-declaration.js';
import { simplifyPathDeclaration } from './simplify-path-declaration.js';
import { evaluateSimplifyPath } from './simplify-path-binding.js';
import { expandStrokeDeclaration } from './expand-stroke-declaration.js';
import { evaluateExpandStroke } from './expand-stroke-binding.js';
import { puckerBloatDeclaration } from './pucker-bloat-declaration.js';
import { evaluatePuckerBloat } from './pucker-bloat-binding.js';
import { wavePointsDeclaration } from './wave-points-declaration.js';
import { evaluateWavePoints } from './wave-points-binding.js';
import { twistPointsDeclaration } from './twist-points-declaration.js';
import { evaluateTwistPoints } from './twist-points-binding.js';
import { bendPointsDeclaration } from './bend-points-declaration.js';
import { evaluateBendPoints } from './bend-points-binding.js';
import { taperDeclaration } from './taper-node-declaration.js';
import { evaluateTaper } from './taper-node-binding.js';
import { autoHandlesDeclaration } from './auto-handles-declaration.js';
import { evaluateAutoHandles } from './auto-handles-binding.js';
import { polarTransformDeclaration } from './polar-transform-declaration.js';
import { evaluatePolarTransform } from './polar-transform-binding.js';
import { smoothPathDeclaration } from './smooth-path-declaration.js';
import { evaluateSmoothPath } from './smooth-path-binding.js';
import { roughenDeclaration } from './roughen-node-declaration.js';
import { evaluateRoughen } from './roughen-node-binding.js';
import { mirrorDeclaration } from './mirror-node-declaration.js';
import { evaluateMirror } from './mirror-node-binding.js';
import { cornerRoundDeclaration } from './corner-round-declaration.js';
import { evaluateCornerRound } from './corner-round-binding.js';
import { textToPathDeclaration } from './text-to-path-declaration.js';
import { evaluateTextToPath } from './text-to-path-binding.js';
import { setClosureDeclaration } from './set-closure-declaration.js';
import { evaluateSetClosure } from './set-closure-binding.js';
import { vectorTransformDeclaration } from './vector-transform-declaration.js';
import { evaluateVectorTransform } from './vector-transform-binding.js';
import { outputDeclaration } from './output-node-declaration.js';
import { evaluateOutput } from './output-node-binding.js';
import { polygonDeclaration } from './polygon-node-declaration.js';
import { evaluatePolygon } from './polygon-node-binding.js';
import { starDeclaration } from './star-node-declaration.js';
import { evaluateStar } from './star-node-binding.js';
import { circleDeclaration } from './circle-node-declaration.js';
import { evaluateCircle } from './circle-node-binding.js';
import { ellipseDeclaration } from './ellipse-node-declaration.js';
import { evaluateEllipse } from './ellipse-node-binding.js';
import { rectangleDeclaration } from './rectangle-node-declaration.js';
import { evaluateRectangle } from './rectangle-node-binding.js';
import { closedOffsetDeclaration } from './closed-offset-node-declaration.js';
import { evaluateClosedOffset } from './closed-offset-node-binding.js';
import { subdividePathDeclaration } from './subdivide-path-declaration.js';
import { evaluateSubdividePath } from './subdivide-path-binding.js';
import { resamplePathDeclaration } from './resample-path-declaration.js';
import { evaluateResamplePath } from './resample-path-binding.js';
import { trimPathDeclaration } from './trim-path-declaration.js';
import { evaluateTrimPath } from './trim-path-binding.js';
import { setPathStartDeclaration } from './set-path-start-declaration.js';
import { evaluateSetPathStart } from './set-path-start-binding.js';
import { dashPathDeclaration } from './dash-path-declaration.js';
import { evaluateDashPath } from './dash-path-binding.js';
import { convertToPathDeclaration } from './convert-to-path-declaration.js';
import { evaluateConvertToPath } from './convert-to-path-binding.js';
import { fillDeclaration } from './fill-node-declaration.js';
import { evaluateFill } from './fill-node-binding.js';
import { linearGradientDeclaration } from './linear-gradient-fill-declaration.js';
import { evaluateLinearGradient } from './linear-gradient-fill-binding.js';
import { radialGradientDeclaration } from './radial-gradient-fill-declaration.js';
import { evaluateRadialGradient } from './radial-gradient-fill-binding.js';
import { angularGradientDeclaration } from './angular-gradient-fill-declaration.js';
import { evaluateAngularGradient } from './angular-gradient-fill-binding.js';
import { strokeDeclaration } from './stroke-node-declaration.js';
import { evaluateStroke } from './stroke-node-binding.js';
import { keyerDeclaration } from './keyer-node-declaration.js';
import { evaluateKeyer } from './keyer-node-binding.js';

// One trusted row per built-in: behavior, placement and exposure are captured at startup.
const entry = (declaration, binding, definitionAfter, catalogAfter, exposure = 'public') =>
  Object.freeze({
    declaration: snapshotNodeDeclaration(declaration),
    binding: Object.freeze({ ...binding }),
    placement: Object.freeze({ definitionAfter, catalogAfter }),
    exposure,
  });
export const builtinManifest = Object.freeze([
  entry(mathDeclaration, { evaluate: evaluateMath }, 'Value', 'Value'),
  entry(
    separateVectorDeclaration,
    { evaluate: evaluateSeparateVector, project: projectSeparateVector },
    'Vector Math',
    'Vector Transform',
  ),
  entry(vectorMathDeclaration, { evaluate: evaluateVectorMath }, 'Math', 'Vector'),
  entry(reversePathDeclaration, { evaluate: evaluateReversePath }, 'Auto Handles', 'Set Closure'),
  entry(joinGeometryDeclaration, { evaluate: evaluateJoinGeometry }, 'Boolean', 'Boolean'),
  entry(
    combineVectorDeclaration,
    { evaluate: evaluateCombineVector },
    'Separate Vector',
    'Separate Vector',
  ),
  entry(valueDeclaration, { evaluate: evaluateValueSource }, 'Instance on Points', 'Line Screen'),
  entry(vectorDeclaration, { evaluate: evaluateValueSource }, 'Color', 'Math'),
  entry(
    expressionDeclaration,
    { evaluate: evaluateValueSource },
    'Composition Reference',
    'Reference',
  ),
  entry(toggleDeclaration, { evaluate: evaluateValueSource }, 'Combine Vector', 'Combine Vector'),
  entry(nullPositionDeclaration, { evaluate: evaluateNullPosition }, 'Vector', 'Toggle'),
  entry(colorDeclaration, { evaluate: evaluateValueSource }, 'Toggle', 'Luminance Instances'),
  entry(simplifyPathDeclaration, { evaluate: evaluateSimplifyPath }, 'Dash Path', 'Smooth Path'),
  entry(
    expandStrokeDeclaration,
    { evaluate: evaluateExpandStroke },
    'Simplify Path',
    'Text to Path',
  ),
  entry(puckerBloatDeclaration, { evaluate: evaluatePuckerBloat }, 'Wave Points', 'Wave Points'),
  entry(wavePointsDeclaration, { evaluate: evaluateWavePoints }, 'Reverse Path', 'Twist Points'),
  entry(twistPointsDeclaration, { evaluate: evaluateTwistPoints }, 'Bend Points', 'Bend Points'),
  entry(bendPointsDeclaration, { evaluate: evaluateBendPoints }, 'Taper', 'Taper'),
  entry(taperDeclaration, { evaluate: evaluateTaper }, 'Set Closure', 'Mirror'),
  entry(
    autoHandlesDeclaration,
    { evaluate: evaluateAutoHandles },
    'Polar Transform',
    'Resample Path',
  ),
  entry(
    polarTransformDeclaration,
    { evaluate: evaluatePolarTransform },
    'Twist Points',
    'Angular Gradient Fill',
  ),
  entry(
    smoothPathDeclaration,
    { evaluate: evaluateSmoothPath },
    'Vector Transform',
    'Corner Round',
  ),
  entry(roughenDeclaration, { evaluate: evaluateRoughen }, 'Subdivide Path', 'Pucker & Bloat'),
  entry(mirrorDeclaration, { evaluate: evaluateMirror }, 'Corner Round', 'Path Remap'),
  entry(
    cornerRoundDeclaration,
    { evaluate: evaluateCornerRound },
    'Resample Path',
    'Closed Path Offset',
  ),
  entry(textToPathDeclaration, { evaluate: evaluateTextToPath }, 'Text', 'Convert to Path'),
  entry(
    setClosureDeclaration,
    { evaluate: evaluateSetClosure },
    'Set Path Start',
    'Set Path Start',
  ),
  entry(
    vectorTransformDeclaration,
    { evaluate: evaluateVectorTransform },
    'Transform',
    'Vector Math',
  ),
  entry(outputDeclaration, { evaluate: evaluateOutput }, 'Stroke', null, 'private'),
  entry(circleDeclaration, { evaluate: evaluateCircle }, 'Mask', 'Mask'),
  entry(ellipseDeclaration, { evaluate: evaluateEllipse }, 'Circle', 'Circle'),
  entry(rectangleDeclaration, { evaluate: evaluateRectangle }, 'Ellipse', 'Ellipse'),
  entry(polygonDeclaration, { evaluate: evaluatePolygon }, 'Rectangle', 'Rectangle'),
  entry(starDeclaration, { evaluate: evaluateStar }, 'Polygon', 'Polygon'),
  entry(
    closedOffsetDeclaration,
    { evaluate: evaluateClosedOffset },
    'Convert to Path',
    'Expand Stroke',
  ),
  entry(
    subdividePathDeclaration,
    { evaluate: evaluateSubdividePath },
    'Pucker & Bloat',
    'Reverse Path',
  ),
  entry(resamplePathDeclaration, { evaluate: evaluateResamplePath }, 'Roughen', 'Subdivide Path'),
  entry(trimPathDeclaration, { evaluate: evaluateTrimPath }, 'Expand Stroke', 'Simplify Path'),
  entry(setPathStartDeclaration, { evaluate: evaluateSetPathStart }, 'Trim Path', 'Trim Path'),
  entry(dashPathDeclaration, { evaluate: evaluateDashPath }, 'Smooth Path', 'Auto Handles'),
  entry(convertToPathDeclaration, { evaluate: evaluateConvertToPath }, 'Join Geometry', 'Text'),
  entry(linearGradientDeclaration, { evaluate: evaluateLinearGradient }, 'Fill', 'Stroke'),
  entry(
    radialGradientDeclaration,
    { evaluate: evaluateRadialGradient },
    'Linear Gradient Fill',
    'Linear Gradient Fill',
  ),
  entry(
    angularGradientDeclaration,
    { evaluate: evaluateAngularGradient },
    'Radial Gradient Fill',
    Object.freeze({ before: 'Luminance to Points' }),
  ),
  entry(fillDeclaration, { evaluate: evaluateFill }, 'Expression', 'Color'),
  entry(strokeDeclaration, { evaluate: evaluateStroke }, 'Line Screen', 'Fill'),
  entry(keyerDeclaration, { evaluate: evaluateKeyer }, 'Image Output', 'Luminance to Points'),
]);

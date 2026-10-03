"""Independent closed-schema gate for native 0.79 nonseparable mode IDs."""
import copy
import json
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
OLD = ['normal','multiply','screen','overlay','darken','lighten','color-dodge','color-burn','hard-light','soft-light','difference','exclusion']
NEW = ['hue','saturation','color','luminosity']
previous = json.loads((ROOT/'schemas/native-v0.78.schema.json').read_text())
current = json.loads((ROOT/'schemas/native-v0.79.schema.json').read_text())
assert previous['$defs']['compositing']['properties']['blend']['enum'] == OLD
assert current['$defs']['compositing']['properties']['blend']['enum'] == OLD + NEW
assert current['properties']['version']['const'] == '0.79'
assert current['$id'] == 'urn:nect:native:0.79'
normalized = copy.deepcopy(current)
for key in ('$id','title','$comment'):
    normalized[key] = previous[key]
normalized['properties']['version']['const'] = '0.78'
normalized['$defs']['compositing']['properties']['blend']['enum'] = OLD
assert normalized == previous, '0.79 changed unrelated schema semantics'
for version in range(11,79):
    schema = json.loads((ROOT/f'schemas/native-v0.{version}.schema.json').read_text())
    assert schema['$defs']['compositing']['properties']['blend']['enum'] == OLD
print('PASS 0.79 closed enum and all 68 historical compositing schemas unchanged')

ARITHMETIC = ['linear-burn','linear-dodge','linear-light','vivid-light','pin-light','hard-mix','subtract','divide','darker-color','lighter-color']
latest = json.loads((ROOT/'schemas/native-v0.80.schema.json').read_text())
assert latest['$defs']['compositing']['properties']['blend']['enum'] == OLD + NEW + ARITHMETIC
assert latest['properties']['version']['const'] == '0.80'
assert latest['$id'] == 'urn:nect:native:0.80'
normalized = copy.deepcopy(latest)
for key in ('$id','title','$comment'):
    normalized[key] = current[key]
normalized['properties']['version']['const'] = '0.79'
normalized['$defs']['compositing']['properties']['blend']['enum'] = OLD + NEW
assert normalized == current, '0.80 changed unrelated schema semantics'
print('PASS 0.80 closed enum; 0.79 HSL and older schemas preserved')

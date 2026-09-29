"""Black-box CLI tests with hand-specified expectations."""
import json
import re
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
from pathlib import Path

exe = str(Path(sys.argv[1]).resolve())
checks = 0

def run(mode, value=None):
    return subprocess.run([exe, mode], input=None if value is None else json.dumps(value),
                          text=True, capture_output=True, timeout=10)

def check(value, message):
    global checks
    if not value:
        raise AssertionError(message)
    checks += 1

def remove_migrated_asset_defaults(document):
    check(document.pop("raster_assets") == [], "legacy migration starts with no raster assets")
    for obj in document['objects']:
        if obj['kind'] == 'group':
            check(obj.pop('stack') == [], 'legacy Group migration starts with no effects')
    for composition in document['compositions']:
        check(composition.pop('guides') == [], 'legacy migration supplies no Composition Guides')
        for artboard in composition['artboards']:
            check('layout' not in artboard, 'legacy migration leaves Artboard layout absent')
    return document


def remove_migrated_compositing_defaults(document):
    remove_migrated_asset_defaults(document)
    for obj in document['objects']:
        check(obj.pop('visible') is True, 'legacy artwork remains visible')
        check(obj.pop('compositing') == dict(version=1,opacity=dict(literal=1),blend='normal',isolated=False,mask=None), 'legacy appearance stays neutral')
    return document


def remove_migrated_anchor_defaults(document):
    remove_migrated_compositing_defaults(document)
    for obj in document['objects']:
        check(obj.pop('anchor') == [{'literal':0},{'literal':0}], 'legacy anchor defaults to local origin without changing the affine matrix')
        check(obj.pop('transform_parent') is None, 'legacy transform follows structure')
    return document


sample = json.loads(run('--demo').stdout)
check(run('--validate', sample).returncode == 0, 'demo validates in new process')
check(json.loads(run('--validate',sample).stdout)['native_version']==sample['version'],'validation reports the current writer version')

future = dict(sample, version='999')
check(run('--validate', future).returncode == 2, 'future schema rejected')

unknown = dict(sample, secret_extension={})
check('UNKNOWN_FIELD' in run('--validate', unknown).stderr, 'unknown field rejected')

native_026 = json.loads(json.dumps(sample));native_026['version'] = '0.26'
check(run('--validate', native_026).returncode == 0, 'native 0.26 literal-only document remains readable')
old_driver = json.loads(json.dumps(sample));old_driver['version'] = '0.26'
next(obj for obj in old_driver['objects'] if obj['id'] == 'path-A')['visibility_driver'] = dict(
    link=dict(object='path-B', point='', field='object.visible'))
check('UNKNOWN_FIELD' in run('--validate', old_driver).stderr,
      'native 0.26 refuses the v0.27 visibility driver field')

malformed_driver = json.loads(json.dumps(sample))
next(obj for obj in malformed_driver['objects'] if obj['id'] == 'path-A')['visibility_driver'] = dict(
    link=dict(object='path-B', point='', field='object.visible'), unexpected=True)
check('UNKNOWN_FIELD' in run('--validate', malformed_driver).stderr,
      'native 0.32 rejects unknown visibility driver wrapper fields')
malformed_visibility_ref = json.loads(json.dumps(sample))
next(obj for obj in malformed_visibility_ref['objects'] if obj['id'] == 'path-A')['visibility_driver'] = dict(
    link=dict(object='path-B', point='', field='composite.opacity'))
check('TYPE_MISMATCH' in run('--validate', malformed_visibility_ref).stderr,
      'native 0.32 rejects a visibility driver Ref with the wrong field')

visibility_schema = json.loads((Path(__file__).parent.parent / 'schemas/native-v0.27.schema.json').read_text(encoding='utf-8'))
visibility_driver = visibility_schema['$defs']['visibility_driver']
visibility_ref = visibility_schema['$defs']['visibility_ref']
check(visibility_schema['properties']['version']['const'] == '0.27' and
      visibility_driver['additionalProperties'] is False and visibility_driver['required'] == ['link'] and
      visibility_driver['properties']['link']['$ref'] == '#/$defs/visibility_ref' and
      visibility_ref['additionalProperties'] is False and visibility_ref['required'] == ['object', 'point', 'field'] and
      visibility_ref['properties']['point']['const'] == '' and visibility_ref['properties']['field']['const'] == 'object.visible' and
      all('visibility_driver' in visibility_schema['$defs'][kind]['properties'] for kind in ('group','path','text_object','image_object')),
      'native 0.27 schema describes the closed same-field visibility Ref on every Object kind')
operation_enabled_schema = json.loads((Path(__file__).parent.parent / 'schemas/native-v0.28.schema.json').read_text(encoding='utf-8'))
operation_enabled_driver = operation_enabled_schema['$defs']['operation_enabled_driver']
operation_enabled_ref = operation_enabled_schema['$defs']['operation_enabled_ref']
check(operation_enabled_schema['properties']['version']['const'] == '0.28' and
      operation_enabled_driver['additionalProperties'] is False and operation_enabled_driver['required'] == ['link'] and
      operation_enabled_ref['additionalProperties'] is False and
      operation_enabled_ref['properties']['field']['pattern'] == '^op\\.[A-Za-z0-9_-]+\\.enabled$',
      'native 0.28 schema describes only the closed same-field operation enabled Ref')
gradient_enabled_schema = json.loads((Path(__file__).parent.parent / 'schemas/native-v0.29.schema.json').read_text(encoding='utf-8'))
gradient_definition = gradient_enabled_schema['$defs']['gradient']
gradient_enabled_driver = gradient_enabled_schema['$defs']['gradient_enabled_driver']
gradient_enabled_ref = gradient_enabled_schema['$defs']['gradient_enabled_ref']
check(gradient_enabled_schema['properties']['version']['const'] == '0.29' and
      gradient_definition['additionalProperties'] is False and
      gradient_definition['properties']['enabled_driver']['$ref'] == '#/$defs/gradient_enabled_driver' and
      gradient_enabled_driver['additionalProperties'] is False and gradient_enabled_driver['required'] == ['link'] and
      gradient_enabled_ref['additionalProperties'] is False and gradient_enabled_ref['properties']['point']['const'] == '' and
      gradient_enabled_ref['properties']['field']['pattern'] == '^op\\.[A-Za-z0-9_-]+\\.gradient\\.[A-Za-z0-9_-]+\\.enabled$',
      'native 0.29 schema adds only the closed same-field Gradient enabled Ref')
isolation_schema = json.loads((Path(__file__).parent.parent / 'schemas/native-v0.30.schema.json').read_text(encoding='utf-8'))
compositing_definition = isolation_schema['$defs']['compositing']
isolation_driver = isolation_schema['$defs']['composite_isolated_driver']
isolation_ref = isolation_schema['$defs']['composite_isolated_ref']
legacy_compositing = gradient_enabled_schema['$defs']['compositing']
check(isolation_schema['properties']['version']['const'] == '0.30' and
      compositing_definition['additionalProperties'] is False and
      compositing_definition['properties']['isolated_driver']['$ref'] == '#/$defs/composite_isolated_driver' and
      'isolated_driver' not in compositing_definition['required'] and
      isolation_driver['additionalProperties'] is False and isolation_driver['required'] == ['link'] and
      isolation_ref['additionalProperties'] is False and isolation_ref['required'] == ['object', 'point', 'field'] and
      isolation_ref['properties']['point']['const'] == '' and
      isolation_ref['properties']['field']['const'] == 'composite.isolated' and
      'isolated_driver' not in legacy_compositing['properties'],
      'native 0.30 schema adds only the optional closed same-field Composite isolation Ref')
mask_schema = json.loads((Path(__file__).parent.parent / 'schemas/native-v0.31.schema.json').read_text(encoding='utf-8'))
mask_definition = mask_schema['$defs']['geometry_mask']
mask_driver = mask_schema['$defs']['mask_enabled_driver']
mask_ref = mask_schema['$defs']['mask_enabled_ref']
legacy_mask = isolation_schema['$defs']['geometry_mask']
check(mask_schema['properties']['version']['const'] == '0.31' and
      mask_definition['additionalProperties'] is False and
      mask_definition['properties']['enabled_driver']['$ref'] == '#/$defs/mask_enabled_driver' and
      'enabled_driver' not in mask_definition['required'] and
      mask_driver['additionalProperties'] is False and mask_driver['required'] == ['link'] and
      mask_driver['properties']['link']['$ref'] == '#/$defs/mask_enabled_ref' and
      mask_ref['additionalProperties'] is False and mask_ref['required'] == ['object', 'point', 'field'] and
      mask_ref['properties']['point']['const'] == '' and
      mask_ref['properties']['field']['pattern'] == '^mask\\.[A-Za-z0-9_-]+\\.enabled$' and
      'enabled_driver' not in legacy_mask['properties'],
      'native 0.31 schema adds only an optional closed qualified GeometryMask enabled Ref')
point_edit_schema = json.loads((Path(__file__).parent.parent / 'schemas/native-v0.32.schema.json').read_text(encoding='utf-8'))
point_edit_definition = point_edit_schema['$defs']['point_edit']
point_edit_driver = point_edit_schema['$defs']['point_edit_enabled_driver']
point_edit_ref = point_edit_schema['$defs']['point_edit_enabled_ref']
legacy_point_edit = mask_schema['$defs']['point_edit']
check(point_edit_schema['properties']['version']['const'] == '0.32' and
      point_edit_definition['additionalProperties'] is False and
      point_edit_definition['properties']['enabled_driver']['$ref'] == '#/$defs/point_edit_enabled_driver' and
      'enabled_driver' not in point_edit_definition['required'] and
      point_edit_driver['additionalProperties'] is False and point_edit_driver['required'] == ['link'] and
      point_edit_driver['properties']['link']['$ref'] == '#/$defs/point_edit_enabled_ref' and
      point_edit_ref['additionalProperties'] is False and point_edit_ref['required'] == ['object', 'point', 'field'] and
      point_edit_ref['properties']['point']['const'] == '' and
      point_edit_ref['properties']['field']['pattern'] == '^point_edit\\.[A-Za-z0-9_-]+\\.enabled$' and
      'enabled_driver' not in legacy_point_edit['properties'],
      'native 0.32 schema adds only an optional closed same-field Point Edit enabled Ref')
artboard_size_schema = json.loads((Path(__file__).parent.parent / 'schemas/native-v0.33.schema.json').read_text(encoding='utf-8'))
artboard_size_definition = artboard_size_schema['$defs']['artboard']
artboard_size_driver = artboard_size_schema['$defs']['artboard_size_driver']
artboard_size_ref = artboard_size_schema['$defs']['artboard_size_ref']
artboard_size_expression = artboard_size_schema['$defs']['expression']
legacy_artboard = point_edit_schema['$defs']['artboard']
check(artboard_size_schema['properties']['version']['const'] == '0.33' and
      artboard_size_definition['additionalProperties'] is False and
      artboard_size_definition['properties']['width_driver']['$ref'] == '#/$defs/artboard_size_driver' and
      artboard_size_definition['properties']['height_driver']['$ref'] == '#/$defs/artboard_size_driver' and
      'width_driver' not in legacy_artboard['properties'] and 'height_driver' not in legacy_artboard['properties'] and
      len(artboard_size_driver['oneOf']) == 2 and
      artboard_size_driver['oneOf'][0]['properties']['link']['$ref'] == '#/$defs/artboard_size_ref' and
      artboard_size_driver['oneOf'][1]['properties']['expression']['$ref'] == '#/$defs/expression' and
      artboard_size_ref['additionalProperties'] is False and artboard_size_ref['required'] == ['object', 'point', 'field'] and
      artboard_size_ref['properties']['point']['const'] == '' and
      artboard_size_ref['properties']['field']['enum'] == ['artboard.width', 'artboard.height'] and
      artboard_size_expression['additionalProperties'] is False and
      artboard_size_expression['properties']['version']['const'] == 1,
      'native 0.33 schema adds only optional closed Artboard link or expression size drivers')
guide_expression_schema = json.loads((Path(__file__).parent.parent / 'schemas/native-v0.34.schema.json').read_text(encoding='utf-8'))
guide_expression_definition = guide_expression_schema['$defs']['guide']
check(guide_expression_schema['properties']['version']['const'] == '0.34' and
      guide_expression_definition['additionalProperties'] is False and
      guide_expression_definition['properties']['position_driver']['additionalProperties'] is False and
      guide_expression_definition['properties']['position_expression']['$ref'] == '#/$defs/expression' and
      guide_expression_definition['not']['required'] == ['position_driver', 'position_expression'] and
      guide_expression_schema['$defs']['expression']['additionalProperties'] is False,
      'native 0.34 schema adds a closed Guide expression and excludes simultaneous link and expression sources')
margin_left_schema = json.loads((Path(__file__).parent.parent / 'schemas/native-v0.35.schema.json').read_text(encoding='utf-8'))
margin_definition = margin_left_schema['$defs']['margin']
margin_driver = margin_left_schema['$defs']['margin_left_driver']
check(margin_left_schema['properties']['version']['const'] == '0.35' and
      margin_definition['additionalProperties'] is False and
      margin_definition['properties']['left_driver']['$ref'] == '#/$defs/margin_left_driver' and
      'left_driver' not in guide_expression_schema['$defs']['margin']['properties'] and
      margin_driver['additionalProperties'] is False and margin_driver['required'] == ['link'] and
      margin_driver['properties']['link']['$ref'] == '#/$defs/artboard_size_ref',
      'native 0.35 schema adds only an optional closed Artboard size Ref for Margin left')
grid_x_schema = json.loads((Path(__file__).parent.parent / 'schemas/native-v0.36.schema.json').read_text(encoding='utf-8'))
grid_x_definition = grid_x_schema['$defs']['grid']
grid_x_driver = grid_x_schema['$defs']['grid_bounds_x_driver']
legacy_grid = margin_left_schema['$defs']['grid']
check(grid_x_schema['properties']['version']['const'] == '0.36' and
      grid_x_definition['additionalProperties'] is False and
      grid_x_definition['properties']['bounds_x_driver']['$ref'] == '#/$defs/grid_bounds_x_driver' and
      'bounds_x_driver' not in grid_x_definition['required'] and
      'bounds_x_driver' not in legacy_grid['properties'] and
      grid_x_driver['additionalProperties'] is False and grid_x_driver['required'] == ['link'] and
      grid_x_driver['properties']['link']['$ref'] == '#/$defs/artboard_size_ref',
      'native 0.36 schema adds only an optional closed Artboard size Ref for Grid bounds x')
grid_x_expression_schema = json.loads((Path(__file__).parent.parent / 'schemas/native-v0.37.schema.json').read_text(encoding='utf-8'))
grid_x_expression_definition = grid_x_expression_schema['$defs']['grid']
check(grid_x_expression_schema['properties']['version']['const'] == '0.37' and
      grid_x_expression_definition['additionalProperties'] is False and
      grid_x_expression_definition['properties']['bounds_x_driver']['$ref'] == '#/$defs/grid_bounds_x_driver' and
      grid_x_expression_definition['properties']['bounds_x_expression']['$ref'] == '#/$defs/expression' and
      grid_x_expression_definition['not']['required'] == ['bounds_x_driver', 'bounds_x_expression'] and
      'bounds_x_expression' not in grid_x_schema['$defs']['grid']['required'],
      'native 0.37 schema adds only an optional closed Grid x expression mutually exclusive with its link')
margin_left_expression_schema = json.loads((Path(__file__).parent.parent / 'schemas/native-v0.38.schema.json').read_text(encoding='utf-8'))
margin_left_expression_definition = margin_left_expression_schema['$defs']['margin']
check(margin_left_expression_schema['properties']['version']['const'] == '0.38' and
      margin_left_expression_definition['additionalProperties'] is False and
      margin_left_expression_definition['properties']['left_driver']['$ref'] == '#/$defs/margin_left_driver' and
      margin_left_expression_definition['properties']['left_expression']['$ref'] == '#/$defs/expression' and
      margin_left_expression_definition['not']['required'] == ['left_driver', 'left_expression'] and
      'left_expression' not in margin_left_expression_definition['required'] and
      margin_left_expression_schema['$defs']['grid']['properties']['bounds_x_expression']['$ref'] == '#/$defs/expression',
      'native 0.38 schema adds an optional closed Margin left expression mutually exclusive with its link')
grid_y_schema = json.loads((Path(__file__).parent.parent / 'schemas/native-v0.39.schema.json').read_text(encoding='utf-8'))
grid_y_definition = grid_y_schema['$defs']['grid']
grid_y_driver = grid_y_schema['$defs']['grid_bounds_y_driver']
check(grid_y_schema['properties']['version']['const'] == '0.39' and
      grid_y_definition['additionalProperties'] is False and
      grid_y_definition['properties']['bounds_y_driver']['$ref'] == '#/$defs/grid_bounds_y_driver' and
      'bounds_y_driver' not in grid_y_definition['required'] and
      grid_y_driver['additionalProperties'] is False and grid_y_driver['required'] == ['link'] and
      grid_y_driver['properties']['link']['$ref'] == '#/$defs/artboard_size_ref',
      'native 0.39 schema adds only an optional closed Artboard size Ref for Grid bounds y')
margin_top_link_schema = json.loads((Path(__file__).parent.parent / 'schemas/native-v0.41.schema.json').read_text(encoding='utf-8'))
margin_top_driver = margin_top_link_schema['$defs']['margin_top_driver']
margin_top_expression_schema = json.loads((Path(__file__).parent.parent / 'schemas/native-v0.42.schema.json').read_text(encoding='utf-8'))
margin_top_expression_definition = margin_top_expression_schema['$defs']['margin']
legacy_margin_schema = json.loads((Path(__file__).parent.parent / 'schemas/native-v0.40.schema.json').read_text(encoding='utf-8'))
check(margin_top_link_schema['properties']['version']['const'] == '0.41' and
      margin_top_link_schema['$defs']['margin']['properties']['top_driver']['$ref'] == '#/$defs/margin_top_driver' and
      'top_driver' not in margin_top_link_schema['$defs']['margin']['required'] and
      'top_driver' not in legacy_margin_schema['$defs']['margin']['properties'] and
      margin_top_driver['additionalProperties'] is False and margin_top_driver['required'] == ['link'] and
      margin_top_driver['properties']['link']['$ref'] == '#/$defs/artboard_size_ref',
      'native 0.41 schema remains the optional closed Artboard-size link format for Margin top')
check(margin_top_expression_schema['properties']['version']['const'] == '0.42' and
      margin_top_expression_definition['properties']['top_expression']['$ref'] == '#/$defs/expression' and
      'top_expression' not in margin_top_expression_definition['required'] and
      margin_top_expression_definition['not']['anyOf'] == [
          {'required':['left_driver','left_expression']}, {'required':['top_driver','top_expression']}],
      'native 0.42 schema adds an optional Margin top expression mutually exclusive with its link')
margin_right_schema = json.loads((Path(__file__).parent.parent / 'schemas/native-v0.43.schema.json').read_text(encoding='utf-8'))
margin_right_definition = margin_right_schema['$defs']['margin']
margin_right_driver = margin_right_schema['$defs']['margin_right_driver']
check(margin_right_schema['properties']['version']['const'] == '0.43' and
      margin_right_definition['additionalProperties'] is False and
      margin_right_definition['properties']['right_driver']['$ref'] == '#/$defs/margin_right_driver' and
      'right_driver' not in margin_right_definition['required'] and
      margin_right_driver['additionalProperties'] is False and margin_right_driver['required'] == ['link'] and
      margin_right_driver['properties']['link']['$ref'] == '#/$defs/artboard_size_ref',
      'native 0.43 schema adds only an optional closed Artboard-size link for Margin right')
margin_right_expression_schema = json.loads((Path(__file__).parent.parent / 'schemas/native-v0.44.schema.json').read_text(encoding='utf-8'))
margin_right_expression_definition = margin_right_expression_schema['$defs']['margin']
check(margin_right_expression_schema['properties']['version']['const'] == '0.44' and
      margin_right_expression_definition['additionalProperties'] is False and
      margin_right_expression_definition['properties']['right_expression']['$ref'] == '#/$defs/expression' and
      'right_expression' not in margin_right_expression_definition['required'] and
      margin_right_expression_definition['not']['anyOf'] == [
          {'required':['left_driver','left_expression']}, {'required':['top_driver','top_expression']},
          {'required':['right_driver','right_expression']}],
      'native 0.44 schema adds an optional closed Margin right expression mutually exclusive with its link')
margin_bottom_schema = json.loads((Path(__file__).parent.parent / 'schemas/native-v0.45.schema.json').read_text(encoding='utf-8'))
margin_bottom_definition = margin_bottom_schema['$defs']['margin']
margin_bottom_driver = margin_bottom_schema['$defs']['margin_bottom_driver']
check(margin_bottom_schema['properties']['version']['const'] == '0.45' and
      margin_bottom_definition['additionalProperties'] is False and
      margin_bottom_definition['properties']['bottom_driver']['$ref'] == '#/$defs/margin_bottom_driver' and
      'bottom_driver' not in margin_bottom_definition['required'] and
      'bottom_driver' not in margin_right_expression_schema['$defs']['margin']['properties'] and
      margin_bottom_driver['additionalProperties'] is False and margin_bottom_driver['required'] == ['link'] and
      margin_bottom_driver['properties']['link']['$ref'] == '#/$defs/artboard_size_ref',
      'native 0.45 schema adds only an optional closed Artboard-size link for Margin bottom')
margin_bottom_expression_schema = json.loads((Path(__file__).parent.parent / 'schemas/native-v0.47.schema.json').read_text(encoding='utf-8'))
margin_bottom_expression_definition = margin_bottom_expression_schema['$defs']['margin']
check(margin_bottom_expression_schema['properties']['version']['const'] == '0.47' and
      margin_bottom_expression_definition['additionalProperties'] is False and
      margin_bottom_expression_definition['properties']['bottom_expression']['$ref'] == '#/$defs/expression' and
      'bottom_expression' not in margin_bottom_expression_definition['required'] and
      {'required':['bottom_driver','bottom_expression']} in margin_bottom_expression_definition['not']['anyOf'] and
      margin_bottom_expression_schema['$defs']['expression']['additionalProperties'] is False and
      margin_bottom_expression_schema['$defs']['expression']['properties']['version']['const'] == 1,
      'native 0.47 schema adds a closed bottom expression mutually exclusive with its optional link')
grid_width_schema = json.loads((Path(__file__).parent.parent / 'schemas/native-v0.48.schema.json').read_text(encoding='utf-8'))
grid_width_definition = grid_width_schema['$defs']['grid']
grid_width_driver = grid_width_schema['$defs']['grid_bounds_width_driver']
check(grid_width_schema['properties']['version']['const'] == '0.48' and
      grid_width_definition['additionalProperties'] is False and
      grid_width_definition['properties']['bounds_width_driver']['$ref'] == '#/$defs/grid_bounds_width_driver' and
      'bounds_width_driver' not in grid_width_definition['required'] and
      grid_width_definition['properties']['bounds_width_expression']['$ref'] == '#/$defs/expression' and
      'bounds_width_expression' not in grid_width_definition['required'] and
      {'required':['bounds_width_driver','bounds_width_expression']} in grid_width_definition['not']['anyOf'] and
      grid_width_driver['additionalProperties'] is False and grid_width_driver['required'] == ['link'] and
      grid_width_driver['properties']['link']['$ref'] == '#/$defs/artboard_size_ref' and
      grid_width_schema['$defs']['expression']['additionalProperties'] is False and
      grid_width_schema['$defs']['expression']['properties']['version']['const'] == 1,
      'native 0.48 schema adds a closed optional Grid width expression beside its mutually exclusive link')
grid_width_v047_schema = json.loads((Path(__file__).parent.parent / 'schemas/native-v0.47.schema.json').read_text(encoding='utf-8'))
check(grid_width_v047_schema['properties']['version']['const'] == '0.47' and
      'bounds_width_expression' not in grid_width_v047_schema['$defs']['grid']['properties'] and
      grid_width_v047_schema['$defs']['grid']['properties']['bounds_width_driver']['$ref'] == '#/$defs/grid_bounds_width_driver',
      'native 0.47 keeps the prior optional Grid width link schema without the new expression field')
grid_height_v049_schema = json.loads((Path(__file__).parent.parent / 'schemas/native-v0.49.schema.json').read_text(encoding='utf-8'))
grid_height_v048_schema = json.loads((Path(__file__).parent.parent / 'schemas/native-v0.48.schema.json').read_text(encoding='utf-8'))
grid_height_definition = grid_height_v049_schema['$defs']['grid']
grid_height_driver = grid_height_v049_schema['$defs']['grid_bounds_height_driver']
check(grid_height_v049_schema['properties']['version']['const'] == '0.49' and
      grid_height_definition['additionalProperties'] is False and
      grid_height_definition['properties']['bounds_height_driver']['$ref'] == '#/$defs/grid_bounds_height_driver' and
      'bounds_height_driver' not in grid_height_definition['required'] and
      'bounds_height_driver' not in grid_height_v048_schema['$defs']['grid']['properties'] and
      grid_height_driver['additionalProperties'] is False and grid_height_driver['required'] == ['link'] and
      grid_height_driver['properties']['link']['$ref'] == '#/$defs/artboard_size_ref',
      'native 0.49 adds only a closed optional Grid height Artboard-size link; native 0.48 remains without that field')
grid_height_expression_schema = json.loads((Path(__file__).parent.parent / 'schemas/native-v0.50.schema.json').read_text(encoding='utf-8'))
grid_height_expression_definition = grid_height_expression_schema['$defs']['grid']
check(grid_height_expression_schema['properties']['version']['const'] == '0.50' and
      grid_height_expression_definition['additionalProperties'] is False and
      grid_height_expression_definition['properties']['bounds_height_driver']['$ref'] == '#/$defs/grid_bounds_height_driver' and
      grid_height_expression_definition['properties']['bounds_height_expression']['$ref'] == '#/$defs/expression' and
      'bounds_height_expression' not in grid_height_expression_definition['required'] and
      {'required':['bounds_height_driver','bounds_height_expression']} in grid_height_expression_definition['not']['anyOf'] and
      'bounds_height_expression' not in grid_height_definition['properties'] and
      grid_height_expression_schema['$defs']['expression']['additionalProperties'] is False and
      grid_height_expression_schema['$defs']['expression']['properties']['version']['const'] == 1,
      'native 0.50 adds only a closed optional Grid height expression mutually exclusive with its link; native 0.49 stays unchanged')

literal_029 = json.loads(json.dumps(sample));literal_029['version'] = '0.29'
check(run('--validate', literal_029).returncode == 0,
      'native 0.29 remains readable with literal-only Composite isolation')
false_029 = json.loads(json.dumps(sample));false_029['version'] = '0.29'
next(obj for obj in false_029['objects'] if obj['id'] == 'path-B')['compositing']['isolated_driver'] = dict(
    link=dict(object='path-A', point='', field='composite.isolated'))
check('UNKNOWN_FIELD' in run('--validate', false_029).stderr,
      'native 0.29 rejects a falsely versioned Composite isolation driver')
malformed_isolation = json.loads(json.dumps(sample))
next(obj for obj in malformed_isolation['objects'] if obj['id'] == 'path-B')['compositing']['isolated_driver'] = dict(
    link=dict(object='path-A', point='', field='composite.isolated'), unexpected=True)
check('UNKNOWN_FIELD' in run('--validate', malformed_isolation).stderr,
      'native 0.30 rejects unknown Composite isolation driver wrapper fields')

wrong_color = dict(sample, color_space='cmyk')
check('UNSUPPORTED_COLOR_OR_UNIT' in run('--validate', wrong_color).stderr, 'CMYK not pretended supported')

svg = run('--svg', sample)
root = ET.fromstring(svg.stdout)
paths = root.findall('.//{http://www.w3.org/2000/svg}path')
check(len(paths) == 2, 'independent XML parser finds both paths')
check(paths[0].attrib['d'].startswith('M 100 150 C '), 'exact first anchor')
check(root.attrib['viewBox'] == '0 0 640 480', 'explicit artboard extent')

with tempfile.TemporaryDirectory() as tmp:
    source = Path(tmp) / 'drawing.nect.json'
    source.write_text(json.dumps(sample), encoding='utf-8')
    ref = dict(object='path-A', point='point-A1', field='x')
    grid_expression = 'ref("process-margin-upstream","","artboard.width") + 10'
    requests = [
        dict(op='capabilities'),
        dict(op='apply', expected_revision=0, commands=[dict(type='set', ref=ref, value=180)]),
        dict(op='inspect'),
        dict(op='undo', expected_revision=1),
        dict(op='inspect'),
        dict(op='execute_shell', command='not-allowed'),
        dict(op='inspect'),
    ]
    p = subprocess.run([exe, '--serve', str(source)],
        input='\n'.join(json.dumps(r) for r in requests)+'\n',
        text=True, capture_output=True, timeout=10)

    check(p.returncode == 0, 'serve process exits normally')
    responses = [json.loads(x) for x in p.stdout.splitlines()]
    check(len(responses)==7, 'one response per request')
    check(responses[0]['result']['mcp'] is False, 'JSON transport does not pretend to be MCP')
    check(responses[2]['result']['objects'][0]['contours'][0]['points'][0]['x']['literal']==180,
          'mutation visible through API')
    check(responses[4]['result']['objects'][0]['contours'][0]['points'][0]['x']['literal']==100,
          'undo visible through API')
    check(responses[5]['error']['code']=='UNSUPPORTED_OPERATION', 'no universal shell')
    check(responses[6]['result']==responses[4]['result'], 'rejected request has no mutation')

    saved = responses[2]['result']
    check(run('--validate', saved).returncode==0, 'edited document reloads in fresh process')
    check('M 180 150' in run('--svg', saved).stdout, 'fresh process export uses persisted edit')

with tempfile.TemporaryDirectory() as tmp:
    source = Path(tmp) / 'visibility.nect.json'
    source.write_text(json.dumps(sample), encoding='utf-8')
    target = dict(object='path-A',point='',field='object.visible')
    driver = dict(object='path-B',point='',field='object.visible')
    requests = [
        dict(op='apply',expected_revision=0,commands=[dict(type='link_object_visibility',target=target,source=driver,replace_driver=False)]),
        dict(op='get',ref=target),
        dict(op='apply',expected_revision=1,commands=[dict(type='set_visibility',object='path-B',visible=False)]),
        dict(op='get',ref=target),
        dict(op='apply',expected_revision=2,commands=[dict(type='set_visibility',object='path-A',visible=True)]),
        dict(op='apply',expected_revision=2,commands=[dict(type='unlink_object_visibility',target=target)]),
        dict(op='apply',expected_revision=3,commands=[dict(type='set_visibility',object='path-B',visible=True)]),
        dict(op='get',ref=target),
    ]
    process = subprocess.run([exe,'--serve',str(source)],input='\n'.join(map(json.dumps,requests))+'\n',
        capture_output=True,text=True,encoding='utf-8',timeout=10)
    replies = [json.loads(line) for line in process.stdout.splitlines()]
    check(process.returncode == 0 and len(replies) == len(requests) and replies[0]['ok'],
          'Object visibility JSON-lines process accepts the dedicated link command')
    check(replies[1]['result']['authored'] == dict(literal=True,driver=dict(link=driver)) and
          replies[1]['result']['evaluated'] is True and replies[1]['result']['link'] is True,
          'JSON-lines get separates authored Object visibility from its link')
    check(replies[2]['ok'] and replies[3]['result']['evaluated'] is False and
          replies[4]['error']['code'] == 'DRIVEN_PROPERTY' and replies[4]['revision'] == 2,
          'JSON-lines source edits propagate while direct edits of the driven target fail atomically')
    check(replies[5]['ok'] and replies[6]['ok'] and
          replies[7]['result']['authored'] == dict(literal=False,driver=None) and replies[7]['result']['evaluated'] is False,
          'JSON-lines unlink freezes evaluated visibility after the former source changes')

with tempfile.TemporaryDirectory() as tmp:
    source = Path(tmp) / 'point-edit-enabled.nect.json'
    source.write_text(json.dumps(sample), encoding='utf-8')
    enabled_ref = dict(object='process-circle', point='', field='point_edit.enabled')
    generated_point = dict(object='process-circle', point='process-circle-source-east', field='x')
    primitive = dict(id='process-circle-source', type='nect.shape.circle', version=1,
        parameters=dict(center_x=dict(literal=50), center_y=dict(literal=40), radius=dict(literal=100)))
    requests = [
        dict(op='apply', expected_revision=0, commands=[dict(type='create_primitive', composition=sample['compositions'][0]['id'],
            parent='', id='process-circle', name='Point Edit Circle', source=primitive)]),
        dict(op='get', ref=enabled_ref),
        dict(op='resolve_name', name='Point Edit Circle', point='', field='point_edit.enabled'),
        dict(op='properties'),
        dict(op='apply', expected_revision=1, commands=[dict(type='set', ref=generated_point, value=75)]),
        dict(op='get', ref=enabled_ref),
        dict(op='resolve_name', name='Point Edit Circle', point='', field='point_edit.enabled'),
        dict(op='properties'),
        dict(op='apply', expected_revision=2, commands=[dict(type='enable_point_edit', object='process-circle', enabled=False)]),
        dict(op='get', ref=enabled_ref),
        dict(op='get', ref=generated_point),
        dict(op='inspect'),
        dict(op='apply', expected_revision=3, commands=[dict(type='set', ref=enabled_ref, value=0)]),
        dict(op='apply', expected_revision=3, commands=[dict(type='clear_point_edit', object='process-circle')]),
        dict(op='get', ref=enabled_ref),
        dict(op='properties'),
    ]
    process = subprocess.run([exe, '--serve', str(source)], input='\n'.join(map(json.dumps, requests))+'\n',
        capture_output=True, text=True, encoding='utf-8', timeout=20)
    replies = [json.loads(line) for line in process.stdout.splitlines()]
    check(process.returncode == 0 and len(replies) == len(requests) and replies[0]['ok'],
          'Procedural Path creation succeeds before Point Edit discovery')
    check(replies[1]['error']['code'] == 'NO_POINT_EDIT' and replies[2]['error']['code'] == 'NO_POINT_EDIT' and
          not any(value['ref'] == enabled_ref for value in replies[3]['result']),
          'Undrafted Point Edit is absent from get, unique-name resolution and properties')
    enabled_read = replies[5]['result']
    discovered = next(value for value in replies[7]['result'] if value['ref'] == enabled_ref)
    check(enabled_read['type'] == 'bool' and enabled_read['unit'] == 'boolean' and
          enabled_read['space'] == 'local' and enabled_read['origin'] == 'authored' and
          enabled_read['authored'] == dict(literal=True, driver=None) and enabled_read['evaluated'] is True and
          enabled_read['link'] is False and enabled_read['expression'] is False and
          replies[6]['result'] == enabled_ref and discovered == enabled_read,
          'Point Edit get, properties and unique-name resolution expose the authored typed boolean')
    check(replies[8]['ok'] and replies[9]['result']['authored'] == dict(literal=False, driver=None) and
          replies[9]['result']['evaluated'] is False and replies[10]['result']['evaluated'] == 150,
          'Disabling Point Edit reads false and restores generated geometry while retaining the override')
    check(replies[12]['error']['code'] == 'MISSING_REFERENCE' and replies[12]['revision'] == 3 and
          replies[13]['ok'] and replies[14]['error']['code'] == 'NO_POINT_EDIT' and
          not any(value['ref'] == enabled_ref for value in replies[15]['result']),
          'Generic Scalar edit cannot change the boolean, and clearing the instance removes discovery')
    saved = replies[11]['result']
    source.write_text(json.dumps(saved), encoding='utf-8')
    before = source.read_bytes()
    cold = subprocess.run([exe, '--serve', str(source)], input=json.dumps(dict(op='get', ref=enabled_ref))+'\n'+
        json.dumps(dict(op='get', ref=generated_point))+'\n'+json.dumps(dict(op='inspect'))+'\n',
        capture_output=True, text=True, encoding='utf-8', timeout=20)
    cold_replies = [json.loads(line) for line in cold.stdout.splitlines()]
    check(cold.returncode == 0 and len(cold_replies) == 3 and all(item['ok'] for item in cold_replies) and
          cold_replies[0]['result']['authored'] == dict(literal=False, driver=None) and
          cold_replies[1]['result']['evaluated'] == 150 and
          next(obj for obj in cold_replies[2]['result']['objects'] if obj['id'] == 'process-circle')['point_edit']['id'] ==
              'process-circle-source-point-edit' and source.read_bytes() == before,
          'Distinct native 0.27 cold open retains Point Edit identity, bypass, generator fallback and exact bytes')

with tempfile.TemporaryDirectory() as tmp:
    path = Path(tmp) / 'point-edit-enabled-link.nect'
    path.write_text(json.dumps(sample), encoding='utf-8')
    target_object='process-pe-target';source_object='process-pe-source'
    target_generator='process-pe-target-generator';source_generator='process-pe-source-generator'
    target_ref=dict(object=target_object,point='',field=f'point_edit.{target_generator}-point-edit.enabled')
    source_ref=dict(object=source_object,point='',field=f'point_edit.{source_generator}-point-edit.enabled')
    target_point=dict(object=target_object,point=f'{target_generator}-east',field='x')
    circle=lambda id: dict(id=id,type='nect.shape.circle',version=1,
        parameters=dict(center_x=dict(literal=50),center_y=dict(literal=40),radius=dict(literal=100)))
    requests=[
        dict(op='apply',expected_revision=0,commands=[
            dict(type='create_primitive',composition=sample['compositions'][0]['id'],parent='',id=target_object,
                 name='Point Edit target',source=circle(target_generator)),
            dict(type='create_primitive',composition=sample['compositions'][0]['id'],parent='',id=source_object,
                 name='Point Edit source',source=circle(source_generator)),
            dict(type='set',ref=target_point,value=260),
            dict(type='set',ref=dict(object=source_object,point=f'{source_generator}-east',field='x'),value=360),
            dict(type='enable_point_edit',object=target_object,enabled=False)]),
        dict(op='apply',expected_revision=1,commands=[dict(type='link_point_edit_enabled',target=target_ref,source=source_ref,replace_driver=False)]),
        dict(op='get',ref=target_ref),dict(op='properties'),dict(op='history'),
        dict(op='apply',expected_revision=2,commands=[
            dict(type='set',ref=dict(object=target_object,point='',field='generator.radius'),value=120),
            dict(type='enable_point_edit',object=target_object,enabled=True)]),
        dict(op='history'),dict(op='get',ref=target_point),
        dict(op='apply',expected_revision=2,commands=[dict(type='enable_point_edit',object=source_object,enabled=False)]),
        dict(op='get',ref=target_ref),dict(op='get',ref=target_point),
        dict(op='apply',expected_revision=3,commands=[dict(type='enable_point_edit',object=source_object,enabled=True)]),
        dict(op='get',ref=target_ref),
        dict(op='apply',expected_revision=4,commands=[dict(type='enable_point_edit',object=source_object,enabled=False)]),
        dict(op='inspect'),
        dict(op='apply',expected_revision=5,commands=[dict(type='clear_point_edit',object=source_object)]),
        dict(op='apply',expected_revision=5,commands=[dict(type='clear_point_edit',object=source_object),
            dict(type='set',ref=dict(object=source_object,point=f'{source_generator}-east',field='x'),value=420)]),
        dict(op='apply',expected_revision=5,commands=[dict(type='convert_to_path',object=source_object)]),
        dict(op='apply',expected_revision=5,commands=[dict(type='delete_objects',objects=[source_object])]),
        dict(op='apply',expected_revision=5,commands=[dict(type='unlink_point_edit_enabled',target=target_ref)]),
        dict(op='get',ref=target_ref),
        dict(op='apply',expected_revision=6,commands=[dict(type='enable_point_edit',object=source_object,enabled=True)]),
        dict(op='get',ref=target_ref),
    ]
    process=subprocess.run([exe,'--serve',str(path)],input='\n'.join(map(json.dumps,requests))+'\n',
        capture_output=True,text=True,encoding='utf-8',timeout=25)
    replies=[json.loads(line) for line in process.stdout.splitlines()]
    names=['setup','link','get_linked','properties','history_before_bad_batch','bad_batch','history_after_bad_batch',
        'point_before_source_bypass','source_bypass','target_bypassed','point_bypassed','source_restore','target_restored',
        'source_bypass_again','linked_native','clear_source','clear_recreate','convert_source','delete_source','unlink',
        'frozen','source_restore_after_unlink','still_frozen']
    reply=dict(zip(names,replies))
    check(process.returncode==0 and len(replies)==len(requests) and all('ok' in value for value in replies),
        'Point Edit enabled links return one typed response per JSON-lines process request')
    linked=reply['get_linked']['result']
    check(reply['setup']['ok'] and reply['link']['ok'] and linked['authored']==dict(literal=False,driver=dict(link=source_ref)) and
        linked['evaluated'] is True and linked['link'] is True and linked['expression'] is False and
        reply['get_linked']['result']==next(item for item in reply['properties']['result'] if item['ref']==target_ref),
        'JSON-lines get and properties retain the authored false bit and exact instance-qualified driver')
    check(not reply['bad_batch']['ok'] and reply['bad_batch']['error']['code']=='DRIVEN_PROPERTY' and
        reply['bad_batch']['revision']==2 and reply['history_before_bad_batch']['result']==reply['history_after_bad_batch']['result'] and
        reply['point_before_source_bypass']['result']['evaluated']==260 and
        reply['point_bypassed']['result']['evaluated']==150 and
        reply['target_bypassed']['result']['authored']==dict(literal=False,driver=dict(link=source_ref)) and
        reply['target_bypassed']['result']['evaluated'] is False,
        'An invalid later batch command is atomic; a false source uses generator fallback and retains the saved override')
    check(reply['source_restore']['ok'] and reply['target_restored']['result']['evaluated'] is True and
        reply['target_restored']['result']['authored']['literal'] is False and
        reply['source_bypass_again']['ok'] and reply['clear_source']['error']['code']=='POINT_EDIT_IN_USE' and
        reply['clear_recreate']['error']['code']=='POINT_EDIT_IN_USE' and
        reply['convert_source']['error']['code']=='POINT_EDIT_IN_USE' and
        reply['delete_source']['error']['code']=='POINT_EDIT_IN_USE' and
        all(reply[name]['revision']==5 for name in ('clear_source','clear_recreate','convert_source','delete_source')),
        'Same-ID source toggles propagate while clear, clear/recreate, conversion and deletion guard surviving dependents')
    native=reply['linked_native']['result']
    native_target=next(obj for obj in native['objects'] if obj['id']==target_object)['point_edit']
    check(native['version']=='0.50' and native_target['enabled'] is False and
        native_target['enabled_driver']==dict(link=source_ref) and run('--validate',native).returncode==0,
        'Native 0.36 persists only the optional closed qualified driver beside the authored false literal')
    old=native.copy();old['version']='0.31'
    check('UNSUPPORTED_POINT_EDIT_ENABLED_DRIVER' in run('--validate',old).stderr,
        'Native 0.31 rejects a falsely versioned Point Edit enabled driver')
    malformed=json.loads(json.dumps(native));next(obj for obj in malformed['objects'] if obj['id']==target_object)['point_edit']['enabled_driver']['extra']=True
    check('UNKNOWN_FIELD' in run('--validate',malformed).stderr,
        'Native rejects unknown Point Edit enabled driver wrapper fields')
    wrong_ref=json.loads(json.dumps(native));next(obj for obj in wrong_ref['objects'] if obj['id']==target_object)['point_edit']['enabled_driver']['link']['field']='point_edit.enabled'
    check('INVALID_POINT_EDIT_REF' in run('--validate',wrong_ref).stderr,
        'Native rejects an unqualified Point Edit slot as a persistent source Ref')
    path.write_text(json.dumps(native),encoding='utf-8');before=path.read_bytes()
    cold=subprocess.run([exe,'--serve',str(path)],input='\n'.join(map(json.dumps,[
        dict(op='get',ref=target_ref),dict(op='properties'),dict(op='inspect')]))+'\n',
        capture_output=True,text=True,encoding='utf-8',timeout=20)
    cold_replies=[json.loads(line) for line in cold.stdout.splitlines()]
    cold_doc=cold_replies[2]['result']
    cold_expected=reply['target_bypassed']['result']
    check(cold.returncode==0 and all(value['ok'] for value in cold_replies) and
        cold_replies[0]['result']==cold_expected and
        next(value for value in cold_replies[1]['result'] if value['ref']==target_ref)==cold_expected and
        next(obj for obj in cold_doc['objects'] if obj['id']==target_object)['point_edit']==native_target and
        path.read_bytes()==before,
        'A distinct JSON-lines process cold-opens native 0.36 with matching typed state and byte-stable authorship')
    check(reply['unlink']['ok'] and reply['frozen']['result']['authored']==dict(literal=False,driver=None) and
        reply['frozen']['result']['evaluated'] is False and reply['source_restore_after_unlink']['ok'] and
        reply['still_frozen']['result']['authored']==dict(literal=False,driver=None) and
        reply['still_frozen']['result']['evaluated'] is False,
        'JSON-lines unlink freezes the evaluated bit against later source changes')

with tempfile.TemporaryDirectory() as tmp:
    source = Path(tmp) / 'mask-enabled.nect.json'
    source.write_text(json.dumps(sample), encoding='utf-8')
    mask_ref = dict(object='path-A', point='', field='mask.enabled')
    mask = dict(id='process-mask', source='path-B', version=1, enabled=True, fill_rule='nonzero')
    requests = [
        dict(op='apply', expected_revision=0, commands=[dict(type='set_mask', object='path-A', mask=mask)]),
        dict(op='get', ref=mask_ref),
        dict(op='resolve_name', name='Curve A', point='', field='mask.enabled'),
        dict(op='properties'),
        dict(op='apply', expected_revision=1, commands=[dict(type='set_mask', object='path-A', mask=dict(mask, enabled=False))]),
        dict(op='get', ref=mask_ref),
        dict(op='apply', expected_revision=2, commands=[
            dict(type='set_mask', object='path-A', mask=mask),
            dict(type='set', ref=mask_ref, value=0)]),
        dict(op='get', ref=mask_ref),
        dict(op='undo', expected_revision=2),
        dict(op='get', ref=mask_ref),
        dict(op='apply', expected_revision=3, commands=[dict(type='set_mask', object='path-A', mask=None)]),
        dict(op='get', ref=mask_ref),
        dict(op='properties'),
        dict(op='apply', expected_revision=4, commands=[dict(type='set_mask', object='path-A', mask=dict(mask, id='process-mask-replacement'))]),
        dict(op='inspect'),
    ]
    process = subprocess.run([exe, '--serve', str(source)], input='\n'.join(map(json.dumps, requests))+'\n',
        capture_output=True, text=True, encoding='utf-8', timeout=20)
    replies = [json.loads(line) for line in process.stdout.splitlines()]
    check(process.returncode == 0 and len(replies) == len(requests) and replies[0]['ok'],
          'Mask enabled JSON-lines process creates and reads the optional owner-slot property')
    initial_mask = replies[1]['result']
    discovered = next(value for value in replies[3]['result'] if value['ref'] == mask_ref)
    check(initial_mask['type'] == 'bool' and initial_mask['unit'] == 'boolean' and
          initial_mask['authored'] == dict(literal=True, driver=None) and initial_mask['evaluated'] is True and
          initial_mask['link'] is False and initial_mask['expression'] is False and
          replies[2]['result'] == mask_ref and discovered == initial_mask,
          'JSON-lines get, properties and unique name resolution report the exact authored boolean')
    check(replies[4]['ok'] and replies[5]['result']['authored'] == dict(literal=False, driver=None) and
          not replies[6]['ok'] and replies[6]['error']['code'] == 'MISSING_REFERENCE' and replies[6]['revision'] == 2 and
          replies[7]['result']['authored'] == dict(literal=False, driver=None),
          'SetMask changes the read while generic Scalar mutation rejects without committing a batch prefix')
    check(replies[8]['ok'] and replies[9]['result']['authored'] == dict(literal=True, driver=None) and
          replies[10]['ok'] and replies[11]['error']['code'] == 'MISSING_MASK' and
          not any(value['ref'] == mask_ref for value in replies[12]['result']),
          'Undo restores the literal, and removal makes get fail with MISSING_MASK and removes discovery')
    check(replies[13]['ok'] and replies[14]['result']['version'] == '0.50' and
          next(obj for obj in replies[14]['result']['objects'] if obj['id'] == 'path-A')['compositing']['mask']['id'] == 'process-mask-replacement',
          'Replacement mask retains its native identity under the owner-slot Ref')
    saved = replies[14]['result']
    source.write_text(json.dumps(saved), encoding='utf-8')
    before = source.read_bytes()
    cold = subprocess.run([exe, '--serve', str(source)], input=json.dumps(dict(op='get', ref=mask_ref))+'\n'+
        json.dumps(dict(op='properties'))+'\n'+json.dumps(dict(op='inspect'))+'\n',
        capture_output=True, text=True, encoding='utf-8', timeout=20)
    cold_replies = [json.loads(line) for line in cold.stdout.splitlines()]
    cold_get = cold_replies[0]['result']
    check(cold.returncode == 0 and len(cold_replies) == 3 and all(item['ok'] for item in cold_replies) and
          cold_get['authored'] == dict(literal=True, driver=None) and cold_get['evaluated'] is True and
          next(value for value in cold_replies[1]['result'] if value['ref'] == mask_ref) == cold_get and
          source.read_bytes() == before,
          'Distinct JSON-lines cold open preserves mask ID, authored literal, typed read and exact native bytes')

with tempfile.TemporaryDirectory() as tmp:
    source = Path(tmp) / 'mask-enabled-link.nect.json'
    source.write_text(json.dumps(sample), encoding='utf-8')
    target_ref = dict(object='path-A', point='', field='mask.process-target.enabled')
    source_ref = dict(object='path-B', point='', field='mask.process-source.enabled')
    legacy_ref = dict(object='path-A', point='', field='mask.enabled')
    target_mask = dict(id='process-target', source='path-B', version=1, enabled=False, fill_rule='evenodd')
    source_mask = dict(id='process-source', source='path-A', version=1, enabled=True, fill_rule='nonzero')
    requests = [
        dict(op='apply', expected_revision=0, commands=[
            dict(type='set_mask', object='path-A', mask=target_mask),
            dict(type='set_mask', object='path-B', mask=source_mask)]),
        dict(op='apply', expected_revision=1, commands=[dict(type='link_mask_enabled',
            target=target_ref, source=source_ref, replace_driver=False)]),
        dict(op='get', ref=target_ref),
        dict(op='get', ref=legacy_ref),
        dict(op='properties'),
        dict(op='inspect'),
        dict(op='apply', expected_revision=2, commands=[dict(type='link_mask_enabled',
            target=legacy_ref, source=source_ref, replace_driver=False)]),
        dict(op='apply', expected_revision=2, commands=[dict(type='set_mask', object='path-B',
            mask=dict(source_mask, enabled=False))]),
        dict(op='get', ref=target_ref),
        dict(op='compositing_plan', composition=sample['compositions'][0]['id']),
        dict(op='apply', expected_revision=3, commands=[dict(type='set_mask', object='path-B', mask=source_mask)]),
        dict(op='apply', expected_revision=4, commands=[dict(type='unlink_mask_enabled', target=target_ref)]),
        dict(op='get', ref=target_ref),
        dict(op='undo', expected_revision=5),
        dict(op='get', ref=target_ref),
        dict(op='redo', expected_revision=6),
        dict(op='get', ref=target_ref),
        dict(op='inspect'),
    ]
    process = subprocess.run([exe, '--serve', str(source)], input='\n'.join(map(json.dumps, requests))+'\n',
        capture_output=True, text=True, encoding='utf-8', timeout=20)
    replies = [json.loads(line) for line in process.stdout.splitlines()]
    check(process.returncode == 0 and len(replies) == len(requests) and replies[0]['ok'] and replies[1]['ok'],
          'Mask enabled-link commands execute through the JSON-lines Session')
    linked = replies[2]['result']
    legacy = replies[3]['result']
    listed = next(value for value in replies[4]['result'] if value['ref'] == target_ref)
    check(linked['authored'] == dict(literal=False, driver=dict(link=source_ref)) and linked['evaluated'] is True and
          linked['link'] is True and listed == linked and
          legacy['authored'] == dict(literal=False, driver=None) and legacy['evaluated'] is False and
          replies[4]['result'] and replies[6]['error']['code'] == 'INVALID_MASK_REF' and replies[6]['revision'] == 2,
          'Qualified get/properties expose exact linked state while legacy owner-slot reads stay literal-only')
    linked_native = replies[5]['result']
    check(linked_native['version'] == '0.50' and
          next(obj for obj in linked_native['objects'] if obj['id'] == 'path-A')['compositing']['mask']['enabled_driver'] == dict(link=source_ref),
          'Native 0.36 inspect preserves the exact mask driver beside its authored literal')
    changed = replies[7]['result']['changed_ids']
    disabled = replies[8]['result']
    plan = next(node for node in replies[9]['result']['roots'] if node['object'] == 'path-A')
    check({'path-A', 'path-B'}.issubset(set(changed)) and disabled['authored'] == dict(literal=False, driver=dict(link=source_ref)) and
          disabled['evaluated'] is False and plan['mask'] is None,
          'A source bypass edit propagates to its linked mask consumer while retaining target identity and literal')
    frozen = replies[12]['result']
    restored = replies[14]['result']
    redone = replies[16]['result']
    final_doc = replies[17]['result']
    check(frozen['authored'] == dict(literal=True, driver=None) and frozen['evaluated'] is True and
          replies[13]['ok'] and restored['authored'] == dict(literal=False, driver=dict(link=source_ref)) and restored['evaluated'] is True and
          replies[15]['ok'] and redone == frozen,
          'Unlink freezes evaluated bool and Undo/Redo restore the exact Session link state')
    with tempfile.TemporaryDirectory() as cold_tmp:
        cold_path = Path(cold_tmp) / 'linked-native.nect.json'
        cold_path.write_text(json.dumps(linked_native), encoding='utf-8')
        before = cold_path.read_bytes()
        cold = subprocess.run([exe, '--serve', str(cold_path)], input='\n'.join(map(json.dumps, [
            dict(op='get', ref=target_ref), dict(op='properties'), dict(op='inspect')]))+'\n',
            capture_output=True, text=True, encoding='utf-8', timeout=20)
        cold_replies = [json.loads(line) for line in cold.stdout.splitlines()]
        check(cold.returncode == 0 and len(cold_replies) == 3 and all(item['ok'] for item in cold_replies) and
              cold_replies[0]['result'] == linked and
              next(value for value in cold_replies[1]['result'] if value['ref'] == target_ref) == linked and
              cold_replies[2]['result'] == linked_native and cold_path.read_bytes() == before,
              'Distinct JSON-lines process cold-opens native 0.36 with exact linked reads and unchanged bytes')
    old_version = json.loads(json.dumps(linked_native)); old_version['version'] = '0.30'
    check('UNSUPPORTED_MASK_ENABLED_DRIVER' in run('--validate', old_version).stderr,
          'Native 0.30 rejects a version-lied mask enabled driver')
    malformed = json.loads(json.dumps(linked_native))
    next(obj for obj in malformed['objects'] if obj['id'] == 'path-A')['compositing']['mask']['enabled_driver'] = dict(
        link=source_ref, extra=True)
    check('UNKNOWN_FIELD' in run('--validate', malformed).stderr,
          'Native 0.36 rejects an unknown enabled driver wrapper field')
    wrong_field = json.loads(json.dumps(linked_native))
    next(obj for obj in wrong_field['objects'] if obj['id'] == 'path-A')['compositing']['mask']['enabled_driver'] = dict(
        link=dict(object='path-B', point='', field='mask.enabled'))
    check('INVALID_MASK_REF' in run('--validate', wrong_field).stderr,
          'Native 0.36 rejects the unqualified owner-slot Ref as a driver')

guide_document = json.loads(json.dumps(sample))
guide_composition = guide_document['compositions'][0]
guide_composition['guides'] = []
guide_source = dict(object='process-guide-source', point='', field='guide.position')
guide_target = dict(object='process-guide-target', point='', field='guide.position')
with tempfile.TemporaryDirectory() as tmp:
    source_path = Path(tmp) / 'guide-source.nect.json'
    source_path.write_text(json.dumps(guide_document), encoding='utf-8')
    requests = [
        dict(op='apply', expected_revision=0, commands=[
            dict(type='add_guide', composition=guide_composition['id'],
                 guide=dict(id=guide_source['object'], name='Guide source', axis='x', position=100)),
            dict(type='add_guide', composition=guide_composition['id'],
                 guide=dict(id=guide_target['object'], name='Guide target', axis='x', position=240))]),
        dict(op='apply', expected_revision=1, commands=[dict(type='link_guide_position',
            target=guide_target, source=guide_source, replace_driver=False)]),
        dict(op='get', ref=guide_target),
        dict(op='properties'),
        dict(op='apply', expected_revision=2, commands=[dict(type='set_guide_position_expression',
            target=guide_target, expression=dict(source='ref("process-guide-source","","guide.position") + 20', version=1),
            replace_driver=True)]),
        dict(op='get', ref=guide_target),
        dict(op='apply', expected_revision=3, commands=[dict(type='set', ref=guide_target, value=5)]),
        dict(op='inspect'),
    ]
    proc = subprocess.run([exe, '--serve', str(source_path)], input='\n'.join(map(json.dumps, requests))+'\n',
                          capture_output=True, text=True, encoding='utf-8', timeout=10)
    check(proc.returncode == 0, 'Guide JSON-lines process exits normally')
    replies = [json.loads(line) for line in proc.stdout.splitlines()]
    check(len(replies) == len(requests) and replies[0]['ok'] and replies[1]['ok'] and replies[4]['ok'],
          'Guide add, link and expression commands share the revisioned Session JSON-lines path')
    guide_get = replies[2]['result']
    check(guide_get['type'] == 'number' and guide_get['unit'] == 'du' and guide_get['space'] == 'composition' and
          guide_get['authored'] == dict(literal=240, driver=guide_source, source_kind='link', expression=None) and
          guide_get['evaluated'] == 100 and guide_get['link'] is True and guide_get['expression'] is True,
          f'Guide JSON-lines get exposes the typed authored literal and evaluated link value: {guide_get!r}')
    guide_properties = replies[3]['result']
    check(next(value for value in guide_properties if value['ref'] == guide_target)['evaluated'] == 100,
          'Guide JSON-lines properties returns the evaluated Guide view')
    expression_get = replies[5]['result']
    check(expression_get['authored'] == dict(literal=240, driver=None, source_kind='expression',
          expression=dict(source='ref("process-guide-source","","guide.position") + 20', version=1)) and
          expression_get['evaluated'] == 120 and expression_get['expression'] is True,
          'Guide JSON-lines command installs the exact typed expression without changing its authored literal')
    check(not replies[6]['ok'] and replies[6]['error']['code'] == 'TYPE_MISMATCH' and replies[6]['revision'] == 3,
          'Generic Scalar set rejects Guide.position without advancing revision')
    linked_native = replies[7]['result']
    check(linked_native['version'] == '0.50' and
          next(value for value in linked_native['compositions'][0]['guides'] if value['id'] == guide_target['object'])['position_expression'] ==
              dict(source='ref("process-guide-source","","guide.position") + 20', version=1),
          'Native 0.36 inspect preserves the exact Guide expression and authored target literal')

    cold_path = Path(tmp) / 'guide-cold.nect.json'
    cold_path.write_text(json.dumps(linked_native), encoding='utf-8')
    cold_requests = [dict(op='get', ref=guide_target),
        dict(op='apply', expected_revision=0, commands=[dict(type='unlink_guide_position', target=guide_target)]),
        dict(op='get', ref=guide_target)]
    cold = subprocess.run([exe, '--serve', str(cold_path)], input='\n'.join(map(json.dumps, cold_requests))+'\n',
                          capture_output=True, text=True, encoding='utf-8', timeout=10)
    cold_replies = [json.loads(line) for line in cold.stdout.splitlines()]
    check(cold.returncode == 0 and cold_replies[0]['result']['evaluated'] == 120 and cold_replies[1]['ok'] and
          cold_replies[2]['result']['authored'] == dict(literal=120, driver=None, source_kind='literal', expression=None) and
          cold_replies[2]['result']['evaluated'] == 120,
          'A distinct JSON-lines process reopens the Guide expression and explicit unlink freezes its evaluated value')

with tempfile.TemporaryDirectory() as tmp:
    source_path = Path(tmp) / 'margin-left.nect'
    source_path.write_text(json.dumps(sample), encoding='utf-8')
    composition = sample['compositions'][0]
    target = composition['artboards'][0]
    width, height = target['width'], target['height']
    target_ref = dict(object=target['id'], point='', field='margin.left')
    grid_ref = dict(object='process-margin-grid', point='', field='grid.bounds.x')
    source_ref = dict(object='process-margin-source', point='', field='artboard.width')
    source_height_ref = dict(object='process-margin-height-source', point='', field='artboard.height')
    layout = dict(margin=dict(left=40, top=20, right=40, bottom=20),
        grid=dict(id='process-margin-grid',bounds=dict(x=40,y=20,width=width-80,height=height-40),
            columns=2,rows=1,column_gutter=20,row_gutter=0))
    source_board = dict(id='process-margin-source',name='Margin source',x=0,y=0,width=40,height=100,
        parent_size=dict(artboard='process-margin-upstream',width=True,height=False))
    source_height_board = dict(id='process-margin-height-source',name='Margin height source',x=0,y=0,width=100,height=30)
    upstream_board = dict(id='process-margin-upstream',name='Source size',x=0,y=0,width=40,height=100)
    margin_expression='ref("process-margin-source","","artboard.width") + ref("process-margin-height-source","","artboard.height")'
    requests = [
        dict(op='apply',expected_revision=0,commands=[
            dict(type='add_artboard',composition=composition['id'],artboard=source_board,index=1),
            dict(type='add_artboard',composition=composition['id'],artboard=source_height_board,index=2),
            dict(type='add_artboard',composition=composition['id'],artboard=upstream_board,index=3),
            dict(type='set_artboard_layout',composition=composition['id'],artboard_id=target['id'],layout=layout)]),
        dict(op='apply',expected_revision=1,commands=[
            dict(type='link_margin_left',target=target_ref,source=source_ref,replace_driver=False),
            dict(type='link_grid_bounds_x',target=grid_ref,source=source_ref,replace_driver=False)]),
        dict(op='get',ref=target_ref),dict(op='get',ref=grid_ref),
        dict(op='apply',expected_revision=2,commands=[dict(type='update_artboard',composition=composition['id'],
            artboard=dict(upstream_board,width=60))]),
        dict(op='get',ref=target_ref),dict(op='get',ref=grid_ref),dict(op='properties'),dict(op='inspect'),
        dict(op='apply',expected_revision=3,commands=[dict(type='set_grid_bounds_x_expression',target=grid_ref,
            expression=dict(source=grid_expression,version=1),replace_driver=True)]),
        dict(op='get',ref=grid_ref),dict(op='properties'),dict(op='inspect'),
        dict(op='apply',expected_revision=4,commands=[dict(type='set_margin_left_expression',target=target_ref,
            expression=dict(source=margin_expression,version=1),replace_driver=True)]),
        dict(op='get',ref=target_ref),dict(op='properties'),dict(op='inspect'),
        dict(op='apply',expected_revision=5,commands=[
            dict(type='unlink_margin_left',target=target_ref),dict(type='unlink_grid_bounds_x',target=grid_ref)]),
        dict(op='get',ref=target_ref),dict(op='get',ref=grid_ref),
    ]
    process = subprocess.run([exe,'--serve',str(source_path)],input='\n'.join(map(json.dumps,requests))+'\n',
        capture_output=True,text=True,encoding='utf-8',timeout=20)
    replies = [json.loads(line) for line in process.stdout.splitlines()]
    check(process.returncode==0 and len(replies)==len(requests) and all(item['ok'] for item in replies),
        'Margin left JSON-lines link, upstream edit, typed reads, inspect and unlink succeed in the Session process')
    expected_source = dict(object='process-margin-source',point='',field='artboard.width')
    first,first_grid=replies[2]['result'],replies[3]['result']
    changed,changed_grid,listed = replies[5]['result'],replies[6]['result'],replies[7]['result']
    check(first['authored']==dict(literal=40,driver=expected_source,source_kind='link') and first['evaluated']==40 and
        first['link'] is True and first_grid['authored']==dict(literal=40,driver=expected_source,source_kind='link') and
        first_grid['evaluated']==40 and changed['authored']==first['authored'] and changed['evaluated']==60 and
        changed_grid['authored']==first_grid['authored'] and changed_grid['evaluated']==60 and
        next(item for item in listed if item['ref']==target_ref)==changed and
        next(item for item in listed if item['ref']==grid_ref)==changed_grid,
        'JSON-lines get and properties preserve Margin and Grid x literals/sources while reporting updated evaluated values')
    grid_expression_native=replies[12]['result']
    target_layout=next(board for board in grid_expression_native['compositions'][0]['artboards'] if board['id']==target['id'])['layout']
    expressed_grid=replies[10]['result']
    check(expressed_grid['authored']==dict(literal=40,driver=None,source_kind='expression',
          expression=dict(source=grid_expression,version=1)) and expressed_grid['evaluated']==70 and
          next(item for item in replies[11]['result'] if item['ref']==grid_ref)==expressed_grid and
          grid_expression_native['version']=='0.50' and target_layout['margin']['left_driver']==dict(link=expected_source) and
          target_layout['grid']['bounds_x_expression']==dict(source=grid_expression,version=1) and
          'bounds_x_driver' not in target_layout['grid'],
          'JSON-lines carries Grid expression commands, typed reads and native 0.43 Grid authorship')
    expressed_margin=replies[14]['result']
    linked_native=replies[16]['result']
    target_layout=next(board for board in linked_native['compositions'][0]['artboards'] if board['id']==target['id'])['layout']
    check(expressed_margin['authored']==dict(literal=40,driver=None,source_kind='expression',
          expression=dict(source=margin_expression,version=1)) and expressed_margin['evaluated']==90 and
          next(item for item in replies[15]['result'] if item['ref']==target_ref)==expressed_margin and
          linked_native['version']=='0.50' and target_layout['margin']['left_expression']==dict(source=margin_expression,version=1) and
          'left_driver' not in target_layout['margin'] and
          target_layout['grid']['bounds_x_expression']==dict(source=grid_expression,version=1),
          'JSON-lines carries the Margin expression command, typed properties and exact native 0.43 source')
    cold_path=Path(tmp)/'margin-left-cold.nect';cold_path.write_text(json.dumps(linked_native),encoding='utf-8')
    cold_bytes=cold_path.read_bytes()
    cold=subprocess.run([exe,'--serve',str(cold_path)],input=json.dumps(dict(op='get',ref=target_ref))+'\n'+
        json.dumps(dict(op='get',ref=grid_ref))+'\n'+json.dumps(dict(op='inspect'))+'\n',
        capture_output=True,text=True,encoding='utf-8',timeout=10)
    cold_replies=[json.loads(line) for line in cold.stdout.splitlines()]
    check(cold.returncode==0 and len(cold_replies)==3 and all(item['ok'] for item in cold_replies) and
        cold_replies[0]['result']==expressed_margin and cold_replies[1]['result']==expressed_grid and
        cold_replies[2]['result']==linked_native and cold_path.read_bytes()==cold_bytes,
        'Distinct JSON-lines process cold-opens exact 0.43 Margin and Grid expressions with unchanged authored/evaluated state')
    lied=json.loads(json.dumps(linked_native));lied['version']='0.35'
    check('INVALID_LAYOUT' in run('--validate',lied).stderr,
        'Native 0.35 version-lie cannot admit the new Grid bounds x driver')
    expression_lie=json.loads(json.dumps(linked_native));expression_lie['version']='0.36'
    check('INVALID_LAYOUT' in run('--validate',expression_lie).stderr,
        'Native 0.36 version-lie cannot admit the new Grid bounds x expression')
    margin_expression_lie=json.loads(json.dumps(linked_native));margin_expression_lie['version']='0.37'
    check('INVALID_LAYOUT' in run('--validate',margin_expression_lie).stderr,
        'Native 0.37 version-lie cannot admit the new Margin left expression')
    unlinked,unlinked_grid=replies[18]['result'],replies[19]['result']
    check(unlinked['authored']==dict(literal=90,driver=None,source_kind='literal') and unlinked['evaluated']==90 and
        unlinked_grid['authored']==dict(literal=70,driver=None,source_kind='literal') and unlinked_grid['evaluated']==70,
        'JSON-lines unlink freezes evaluated Margin expression and Grid expression values into their authored literals')

with tempfile.TemporaryDirectory() as tmp:
    source_path = Path(tmp) / 'margin-top.nect'
    source_path.write_text(json.dumps(sample), encoding='utf-8')
    composition = sample['compositions'][0]
    target = composition['artboards'][0]
    target_ref = dict(object=target['id'],point='',field='margin.top')
    source_ref = dict(object='process-margin-top-source',point='',field='artboard.height')
    alternate_ref = dict(object='process-margin-top-alternate',point='',field='artboard.height')
    expression = f'ref("{source_ref["object"]}","","artboard.height") + 10'
    upstream = dict(id='process-margin-top-upstream',name='Top upstream',x=0,y=0,width=100,height=50)
    source_board = dict(id=source_ref['object'],name='Top source',x=0,y=0,width=100,height=50,
        parent_size=dict(artboard=upstream['id'],width=False,height=True))
    alternate_board = dict(id=alternate_ref['object'],name='Alternate top source',x=0,y=0,width=100,height=55)
    layout = dict(margin=dict(left=20,top=40,right=20,bottom=40),grid=dict(id='process-margin-top-grid',
        bounds=dict(x=20,y=40,width=target['width']-40,height=target['height']-80),columns=1,rows=1,
        column_gutter=0,row_gutter=0))
    requests = [
        dict(op='apply',expected_revision=0,commands=[
            dict(type='add_artboard',composition=composition['id'],artboard=source_board,index=1),
            dict(type='add_artboard',composition=composition['id'],artboard=alternate_board,index=2),
            dict(type='add_artboard',composition=composition['id'],artboard=upstream,index=3),
            dict(type='set_artboard_layout',composition=composition['id'],artboard_id=target['id'],layout=layout)]),
        dict(op='apply',expected_revision=1,commands=[dict(type='link_margin_top',target=target_ref,
            source=source_ref,replace_driver=False)]),
        dict(op='apply',expected_revision=2,commands=[dict(type='set_margin_top_expression',target=target_ref,
            expression=dict(source=expression,version=1),replace_driver=False)]),
        dict(op='apply',expected_revision=2,commands=[dict(type='set_margin_top_expression',target=target_ref,
            expression=dict(source=expression,version=1),replace_driver=True)]),
        dict(op='get',ref=target_ref),dict(op='properties'),dict(op='inspect'),
        dict(op='apply',expected_revision=3,commands=[dict(type='set_artboard_layout',composition=composition['id'],
            artboard_id=target['id'],layout=dict(margin=dict(left=20,top=40,right=20,bottom=40,
                top_driver=dict(link=alternate_ref)),grid=layout['grid']))]),
        dict(op='apply',expected_revision=3,commands=[dict(type='update_artboard',composition=composition['id'],
            artboard=dict(target,layout=dict(margin=dict(left=20,top=40,right=20,bottom=40,
                top_driver=dict(link=alternate_ref)),grid=layout['grid'])))]),
        dict(op='apply',expected_revision=3,commands=[dict(type='add_artboard',composition=composition['id'],
            artboard=dict(id='process-margin-top-smuggled',name='Smuggled',x=0,y=0,width=100,height=100,
                layout=dict(margin=dict(left=10,top=10,right=10,bottom=10,top_driver=dict(link=source_ref)))),index=4)]),
        dict(op='apply',expected_revision=3,commands=[dict(type='update_artboard',composition=composition['id'],
            artboard=dict(upstream,height=60))]),
        dict(op='get',ref=target_ref),dict(op='properties'),
        dict(op='apply',expected_revision=4,commands=[dict(type='unlink_margin_top',target=target_ref)]),
        dict(op='get',ref=target_ref),dict(op='inspect'),
    ]
    process = subprocess.run([exe,'--serve',str(source_path)],input='\n'.join(map(json.dumps,requests))+'\n',
        capture_output=True,text=True,encoding='utf-8',timeout=20)
    replies = [json.loads(line) for line in process.stdout.splitlines()]
    check(process.returncode==0 and len(replies)==len(requests) and replies[0]['ok'] and replies[1]['ok'] and
        not replies[2]['ok'] and replies[2]['error']['code']=='DRIVEN_MARGIN_TOP' and replies[2]['revision']==2 and
        replies[3]['ok'] and all(replies[index]['ok'] for index in (0,1,3,4,5,6)) and
        all(not replies[index]['ok'] and replies[index]['error']['code']=='MARGIN_DRIVER_SMUGGLING' and
            replies[index]['revision']==3 for index in (7,8,9)),
        'Margin top JSON-lines expression command requires explicit replacement and rejects full-layout source smuggling: '+
        repr(dict(returncode=process.returncode,count=len(replies),states=[
            (reply.get('ok'),reply.get('revision'),reply.get('error')) for reply in replies])))
    linked = replies[4]['result']
    linked_native = replies[6]['result']
    updated = replies[11]['result']
    check(linked['authored']==dict(literal=40,driver=None,source_kind='expression',
        expression=dict(source=expression,version=1)) and linked['evaluated']==60 and
        linked['link'] is True and linked['expression'] is True and
        next(item for item in replies[5]['result'] if item['ref']==target_ref)==linked and
        linked_native['version']=='0.50' and
        next(board for board in linked_native['compositions'][0]['artboards'] if board['id']==target['id'])[
            'layout']['margin']['top_expression']==dict(source=expression,version=1) and
        'top_driver' not in next(board for board in linked_native['compositions'][0]['artboards'] if board['id']==target['id'])[
            'layout']['margin'] and
        updated['authored']==linked['authored'] and updated['evaluated']==70 and
        next(item for item in replies[12]['result'] if item['ref']==target_ref)==updated,
        'JSON-lines get, properties and native 0.43 inspect preserve the exact Margin top expression across upstream size changes')
    cold_path=Path(tmp)/'margin-top-cold.nect';cold_path.write_text(json.dumps(linked_native),encoding='utf-8')
    cold=subprocess.run([exe,'--serve',str(cold_path)],input=json.dumps(dict(op='get',ref=target_ref))+'\n'+
        json.dumps(dict(op='inspect'))+'\n',capture_output=True,text=True,encoding='utf-8',timeout=10)
    cold_replies=[json.loads(line) for line in cold.stdout.splitlines()]
    check(cold.returncode==0 and len(cold_replies)==2 and all(reply['ok'] for reply in cold_replies) and
        cold_replies[0]['result']==linked and cold_replies[1]['result']==linked_native,
        'A distinct JSON-lines process cold-opens the exact Margin top expression and native bytes')
    lied=json.loads(json.dumps(linked_native));lied['version']='0.41'
    check('INVALID_LAYOUT' in run('--validate',lied).stderr,
        'Native 0.41 version-lie cannot admit the new Margin top expression')
    literal_native=json.loads(json.dumps(replies[15]['result']));literal_native['version']='0.41'
    check(run('--validate',literal_native).returncode==0,
        'Native 0.41 literal-only Margin top remains readable after the format bump')

with tempfile.TemporaryDirectory() as tmp:
    source_path = Path(tmp) / 'margin-right.nect'
    source_path.write_text(json.dumps(sample), encoding='utf-8')
    composition = sample['compositions'][0]
    target = composition['artboards'][0]
    target_ref = dict(object=target['id'],point='',field='margin.right')
    source_ref = dict(object='process-margin-right-source',point='',field='artboard.width')
    alternate_ref = dict(object='process-margin-right-alternate',point='',field='artboard.height')
    source_board = dict(id=source_ref['object'],name='Right source',x=0,y=0,width=45,height=100)
    alternate_board = dict(id=alternate_ref['object'],name='Alternate right source',x=0,y=0,width=100,height=55)
    layout = dict(margin=dict(left=10,top=10,right=25,bottom=10))
    requests = [
        dict(op='apply',expected_revision=0,commands=[
            dict(type='add_artboard',composition=composition['id'],artboard=source_board,index=1),
            dict(type='add_artboard',composition=composition['id'],artboard=alternate_board,index=2),
            dict(type='set_artboard_layout',composition=composition['id'],artboard_id=target['id'],layout=layout)]),
        dict(op='apply',expected_revision=1,commands=[dict(type='link_margin_right',target=target_ref,
            source=source_ref,replace_driver=False)]),
        dict(op='apply',expected_revision=2,commands=[dict(type='link_margin_right',target=target_ref,
            source=source_ref,replace_driver=False)]),
        dict(op='apply',expected_revision=2,commands=[dict(type='link_margin_right',target=target_ref,
            source=alternate_ref,replace_driver=False)]),
        dict(op='apply',expected_revision=2,commands=[dict(type='set_artboard_layout',composition=composition['id'],
            artboard_id=target['id'],layout=dict(margin=dict(left=10,top=10,right=26,bottom=10)))]),
        dict(op='apply',expected_revision=2,commands=[dict(type='update_artboard',composition=composition['id'],
            artboard=dict(target,layout=dict(margin=dict(left=10,top=10,right=25,bottom=10,
                right_driver=dict(link=alternate_ref)))))]),
        dict(op='apply',expected_revision=2,commands=[dict(type='add_artboard',composition=composition['id'],
            artboard=dict(id='process-margin-right-smuggled',name='Smuggled right source',x=0,y=0,width=100,height=100,
                layout=dict(margin=dict(left=10,top=10,right=25,bottom=10,right_driver=dict(link=source_ref)))),index=3)]),
        dict(op='apply',expected_revision=2,commands=[dict(type='update_artboard',composition=composition['id'],
            artboard=dict(source_board,width=60))]),
        dict(op='get',ref=target_ref),dict(op='properties'),dict(op='inspect'),
        dict(op='apply',expected_revision=3,commands=[dict(type='unlink_margin_right',target=target_ref)]),
        dict(op='get',ref=target_ref),dict(op='inspect'),
    ]
    process = subprocess.run([exe,'--serve',str(source_path)],input='\n'.join(map(json.dumps,requests))+'\n',
        capture_output=True,text=True,encoding='utf-8',timeout=20)
    replies = [json.loads(line) for line in process.stdout.splitlines()]
    check(process.returncode==0 and len(replies)==len(requests) and replies[0]['ok'] and replies[1]['ok'] and
        replies[2]['ok'] and replies[2]['revision']==2 and not replies[3]['ok'] and
        replies[3]['error']['code']=='DRIVEN_MARGIN_RIGHT' and replies[3]['revision']==2 and
        not replies[4]['ok'] and replies[4]['error']['code']=='DRIVEN_MARGIN_RIGHT' and replies[4]['revision']==2 and
        not replies[5]['ok'] and replies[5]['error']['code']=='MARGIN_DRIVER_SMUGGLING' and replies[5]['revision']==2 and
        not replies[6]['ok'] and replies[6]['error']['code']=='MARGIN_DRIVER_SMUGGLING' and replies[6]['revision']==2 and
        replies[7]['ok'] and all(replies[index]['ok'] for index in (8,9,10,11,12,13)),
        'Margin right JSON-lines commands are idempotent, require explicit replacement and reject direct/full-layout/AddArtboard source injection: '+
        repr([(reply.get('ok'),reply.get('revision'),reply.get('error')) for reply in replies]))
    linked = replies[8]['result']
    linked_native = replies[10]['result']
    check(linked['authored']==dict(literal=25,driver=source_ref,source_kind='link') and linked['evaluated']==60 and
        linked['link'] is True and linked['expression'] is True and
        next(item for item in replies[9]['result'] if item['ref']==target_ref)==linked and
        linked_native['version']=='0.50' and
        next(board for board in linked_native['compositions'][0]['artboards'] if board['id']==target['id'])[
            'layout']['margin']['right_driver']==dict(link=source_ref) and
        next(item for item in replies[9]['result'] if item['ref']==target_ref)['evaluated']==60,
        'JSON-lines get, properties and native inspect retain Margin right source and evaluated value')
    cold_path = Path(tmp) / 'margin-right-cold.nect'
    cold_path.write_text(json.dumps(linked_native),encoding='utf-8')
    cold = subprocess.run([exe,'--serve',str(cold_path)],input=json.dumps(dict(op='get',ref=target_ref))+'\n'+
        json.dumps(dict(op='inspect'))+'\n',capture_output=True,text=True,encoding='utf-8',timeout=10)
    cold_replies = [json.loads(line) for line in cold.stdout.splitlines()]
    check(cold.returncode==0 and len(cold_replies)==2 and all(reply['ok'] for reply in cold_replies) and
        cold_replies[0]['result']==linked and cold_replies[1]['result']==linked_native,
        'A distinct JSON-lines process cold-opens the exact Margin right Ref and native bytes')
    frozen = replies[12]['result']
    check(frozen['authored']==dict(literal=60,driver=None,source_kind='literal') and frozen['evaluated']==60 and
        replies[13]['result']['version']=='0.50' and 'right_driver' not in next(
            board for board in replies[13]['result']['compositions'][0]['artboards'] if board['id']==target['id'])[
                'layout']['margin'],
        'JSON-lines unlink freezes the evaluated Margin right into its literal and removes the driver')
    lied = json.loads(json.dumps(linked_native));lied['version']='0.42'
    check('INVALID_LAYOUT' in run('--validate',lied).stderr,
        'Native 0.42 version-lie cannot admit a Margin right source')
    legacy_literal = json.loads(json.dumps(replies[13]['result']));legacy_literal['version']='0.42'
    check(run('--validate',legacy_literal).returncode==0,
        'Native 0.42 literal-only Margin right remains readable')

with tempfile.TemporaryDirectory() as tmp:
    base = json.loads(json.dumps(replies[13]['result']))
    target = next(board for board in base['compositions'][0]['artboards'] if board['id']==target['id'])
    target_ref = dict(object=target['id'],point='',field='margin.right')
    source_ref = dict(object='process-margin-right-source',point='',field='artboard.width')
    expression = dict(source='ref("process-margin-right-source","","artboard.width") + 10',version=1)
    smuggled = dict(id='process-margin-right-expression-smuggled',name='Smuggled expression',x=0,y=0,width=100,height=100,
        layout=dict(margin=dict(left=10,top=10,right=25,bottom=10,right_expression=expression)))
    requests = [
        dict(op='apply',expected_revision=0,commands=[dict(type='set_margin_right_expression',target=target_ref,
            expression=expression,replace_driver=False)]),
        dict(op='apply',expected_revision=1,commands=[dict(type='add_artboard',composition=base['compositions'][0]['id'],
            artboard=smuggled,index=len(base['compositions'][0]['artboards'])+1)]),
        dict(op='get',ref=target_ref),dict(op='properties'),dict(op='inspect'),
        dict(op='apply',expected_revision=1,commands=[dict(type='set_margin_right_expression',target=target_ref,
            expression=expression,replace_driver=False)]),
        dict(op='apply',expected_revision=1,commands=[dict(type='unlink_margin_right',target=target_ref)]),
        dict(op='get',ref=target_ref),dict(op='inspect'),
    ]
    expression_path = Path(tmp) / 'margin-right-expression.nect.json'
    expression_path.write_text(json.dumps(base),encoding='utf-8')
    process = subprocess.run([exe,'--serve',str(expression_path)],input='\n'.join(map(json.dumps,requests))+'\n',
        capture_output=True,text=True,encoding='utf-8',timeout=20)
    replies = [json.loads(line) for line in process.stdout.splitlines()]
    check(process.returncode==0 and len(replies)==len(requests) and replies[0]['ok'] and
        not replies[1]['ok'] and replies[1]['error']['code']=='MARGIN_DRIVER_SMUGGLING' and replies[1]['revision']==1 and
        replies[5]['ok'] and replies[5]['revision']==1 and not replies[5]['result']['changed_ids'] and
        replies[6]['ok'] and replies[6]['revision']==2,
        'Margin right expression JSON-lines command is idempotent and AddArtboard expression payload reaches the core smuggling guard: '+
        repr([(reply.get('ok'),reply.get('revision'),reply.get('error')) for reply in replies]))
    expressed = replies[2]['result']
    expression_native = replies[4]['result']
    check(expressed['authored']==dict(literal=60,driver=None,source_kind='expression',expression=expression) and
        expressed['evaluated']==70 and expressed['link'] is True and expressed['expression'] is True and
        next(item for item in replies[3]['result'] if item['ref']==target_ref)==expressed and
        expression_native['version']=='0.50' and
        next(board for board in expression_native['compositions'][0]['artboards'] if board['id']==target['id'])[
            'layout']['margin']['right_expression']==expression and
        run('--validate',expression_native).returncode==0,
        'JSON-lines get, properties and native 0.50 inspect retain the exact Margin right expression and evaluated du')
    lied = json.loads(json.dumps(expression_native));lied['version']='0.43'
    check('INVALID_LAYOUT' in run('--validate',lied).stderr,
        'Native 0.43 version-lie cannot admit a Margin right expression')
    cold_path = Path(tmp) / 'margin-right-expression-cold.nect'
    cold_path.write_text(json.dumps(expression_native),encoding='utf-8')
    cold = subprocess.run([exe,'--serve',str(cold_path)],input=json.dumps(dict(op='get',ref=target_ref))+'\n'+
        json.dumps(dict(op='inspect'))+'\n',capture_output=True,text=True,encoding='utf-8',timeout=10)
    cold_replies = [json.loads(line) for line in cold.stdout.splitlines()]
    check(cold.returncode==0 and len(cold_replies)==2 and all(reply['ok'] for reply in cold_replies) and
        cold_replies[0]['result']==expressed and cold_replies[1]['result']==expression_native,
        'A distinct JSON-lines process cold-opens the exact Margin right expression source and native bytes')
    frozen = replies[7]['result']
    check(frozen['authored']==dict(literal=70,driver=None,source_kind='literal') and frozen['evaluated']==70 and
        replies[8]['result']['version']=='0.50' and 'right_expression' not in next(
            board for board in replies[8]['result']['compositions'][0]['artboards'] if board['id']==target['id'])[
                'layout']['margin'],
        'JSON-lines unlink freezes evaluated Margin right and removes the expression source')

# The next Artboard layout source is a vertical trailing inset. In particular,
# AddArtboard JSON must parse the optional source so the core smuggling guard owns
# the rejection and the process boundary cannot silently skip it.
with tempfile.TemporaryDirectory() as tmp:
    source_path = Path(tmp) / 'margin-bottom.nect'
    source_path.write_text(json.dumps(sample), encoding='utf-8')
    composition = sample['compositions'][0]
    target = composition['artboards'][0]
    target_ref = dict(object=target['id'],point='',field='margin.bottom')
    source_ref = dict(object='process-margin-bottom-source',point='',field='artboard.height')
    alternate_ref = dict(object='process-margin-bottom-alternate',point='',field='artboard.width')
    upstream_id = 'process-margin-bottom-upstream'
    upstream = dict(id=upstream_id,name='Bottom upstream',x=0,y=0,width=50,height=50)
    source_board = dict(id=source_ref['object'],name='Bottom source',x=0,y=0,width=100,height=50,
        parent_size=dict(artboard=upstream_id,width=False,height=True))
    alternate = dict(id=alternate_ref['object'],name='Alternate source',x=0,y=0,width=100,height=60)
    layout = dict(margin=dict(left=40,top=40,right=40,bottom=40))
    smuggled = dict(id='process-margin-bottom-smuggled',name='Smuggled bottom source',x=0,y=0,width=100,height=100,
        layout=dict(margin=dict(left=10,top=10,right=10,bottom=10,bottom_driver=dict(link=source_ref))))
    requests = [
        dict(op='apply',expected_revision=0,commands=[
            dict(type='add_artboard',composition=composition['id'],artboard=upstream,index=1),
            dict(type='add_artboard',composition=composition['id'],artboard=source_board,index=2),
            dict(type='add_artboard',composition=composition['id'],artboard=alternate,index=3),
            dict(type='update_artboard',composition=composition['id'],artboard=dict(target,width=960,height=640)),
            dict(type='set_artboard_layout',composition=composition['id'],artboard_id=target['id'],layout=layout)]),
        dict(op='apply',expected_revision=1,commands=[dict(type='link_margin_bottom',target=target_ref,
            source=source_ref,replace_driver=False)]),
        dict(op='apply',expected_revision=2,commands=[dict(type='link_margin_bottom',target=target_ref,
            source=source_ref,replace_driver=False)]),
        dict(op='apply',expected_revision=2,commands=[dict(type='link_margin_bottom',target=target_ref,
            source=alternate_ref,replace_driver=False)]),
        dict(op='apply',expected_revision=2,commands=[dict(type='set_artboard_layout',composition=composition['id'],
            artboard_id=target['id'],layout=dict(margin=dict(left=40,top=40,right=40,bottom=41)))]),
        dict(op='apply',expected_revision=2,commands=[dict(type='update_artboard',composition=composition['id'],
            artboard=dict(target,width=960,height=640,layout=dict(margin=dict(left=40,top=40,right=40,bottom=40,
                bottom_driver=dict(link=alternate_ref)))))]),
        dict(op='apply',expected_revision=2,commands=[dict(type='add_artboard',composition=composition['id'],
            artboard=smuggled,index=4)]),
        dict(op='apply',expected_revision=2,commands=[dict(type='update_artboard',composition=composition['id'],
            artboard=dict(upstream,height=60))]),
        dict(op='get',ref=target_ref),dict(op='properties'),dict(op='inspect'),
        dict(op='apply',expected_revision=3,commands=[dict(type='update_artboard',composition=composition['id'],
            artboard=dict(upstream,height=1000))]),
        dict(op='apply',expected_revision=3,commands=[dict(type='delete_artboard',composition=composition['id'],
            artboard=source_ref['object'])]),
        dict(op='apply',expected_revision=3,commands=[dict(type='unlink_margin_bottom',target=target_ref)]),
        dict(op='get',ref=target_ref),dict(op='inspect'),
    ]
    process = subprocess.run([exe,'--serve',str(source_path)],input='\n'.join(map(json.dumps,requests))+'\n',
        capture_output=True,text=True,encoding='utf-8',timeout=20)
    replies = [json.loads(line) for line in process.stdout.splitlines()]
    check(process.returncode==0 and len(replies)==len(requests) and replies[0]['ok'] and replies[1]['ok'] and
        replies[2]['ok'] and replies[2]['revision']==2 and not replies[3]['ok'] and
        replies[3]['error']['code']=='DRIVEN_MARGIN_BOTTOM' and replies[3]['revision']==2 and
        not replies[4]['ok'] and replies[4]['error']['code']=='DRIVEN_MARGIN_BOTTOM' and replies[4]['revision']==2 and
        not replies[5]['ok'] and replies[5]['error']['code']=='MARGIN_DRIVER_SMUGGLING' and replies[5]['revision']==2 and
        not replies[6]['ok'] and replies[6]['error']['code']=='MARGIN_DRIVER_SMUGGLING' and replies[6]['revision']==2 and
        replies[7]['ok'] and not replies[11]['ok'] and replies[11]['error']['code']=='INVALID_LAYOUT' and
        replies[11]['revision']==3 and not replies[12]['ok'] and replies[12]['error']['code']=='ARTBOARD_IN_USE' and
        replies[12]['revision']==3 and all(replies[index]['ok'] for index in (8,9,10,13,14,15)),
        'Margin bottom process commands are idempotent and reject direct edits, replacement and AddArtboard smuggling: '+
        repr([(reply.get('ok'),reply.get('revision'),reply.get('error')) for reply in replies]))
    linked = replies[8]['result']
    linked_native = replies[10]['result']
    check(linked['authored']==dict(literal=40,driver=source_ref,source_kind='link') and linked['evaluated']==60 and
        linked['link'] is True and linked['expression'] is True and
        next(item for item in replies[9]['result'] if item['ref']==target_ref)==linked and
        linked_native['version']=='0.50' and
        next(board for board in linked_native['compositions'][0]['artboards'] if board['id']==target['id'])[
            'layout']['margin']['bottom_driver']==dict(link=source_ref),
        'JSON-lines get, properties and native 0.50 inspect preserve Margin bottom source and evaluated value')
    cold_path = Path(tmp) / 'margin-bottom-cold.nect'
    cold_path.write_text(json.dumps(linked_native),encoding='utf-8')
    before = cold_path.read_bytes()
    cold = subprocess.run([exe,'--serve',str(cold_path)],input=json.dumps(dict(op='get',ref=target_ref))+'\n'+
        json.dumps(dict(op='inspect'))+'\n',capture_output=True,text=True,encoding='utf-8',timeout=10)
    cold_replies = [json.loads(line) for line in cold.stdout.splitlines()]
    check(cold.returncode==0 and len(cold_replies)==2 and all(reply['ok'] for reply in cold_replies) and
        cold_replies[0]['result']==linked and cold_replies[1]['result']==linked_native and cold_path.read_bytes()==before,
        'A distinct JSON-lines process cold-opens the exact Margin bottom Ref and unchanged native bytes')
    frozen = replies[14]['result']
    check(frozen['authored']==dict(literal=60,driver=None,source_kind='literal') and frozen['evaluated']==60 and
        replies[15]['result']['version']=='0.50' and 'bottom_driver' not in next(
            board for board in replies[15]['result']['compositions'][0]['artboards'] if board['id']==target['id'])[
                'layout']['margin'],
        'JSON-lines unlink freezes evaluated Margin bottom into its authored literal')
    lied = json.loads(json.dumps(linked_native));lied['version']='0.44'
    check('INVALID_LAYOUT' in run('--validate',lied).stderr,
        'Native 0.44 version-lie cannot admit a Margin bottom source')
    literal_native = json.loads(json.dumps(replies[15]['result']));literal_native['version']='0.44'
    check(run('--validate',literal_native).returncode==0,
        'Native 0.44 literal-only Margin bottom remains readable')
    malformed = json.loads(json.dumps(linked_native))
    next(board for board in malformed['compositions'][0]['artboards'] if board['id']==target['id'])[
        'layout']['margin']['bottom_driver']['extra'] = True
    check('INVALID_LAYOUT' in run('--validate',malformed).stderr,
        'Native 0.47 rejects an unknown Margin bottom driver wrapper field')

with tempfile.TemporaryDirectory() as tmp:
    source_path = Path(tmp) / 'grid-bounds-y.nect.json'
    source_path.write_text(json.dumps(sample), encoding='utf-8')
    composition = sample['compositions'][0]
    target = composition['artboards'][0]
    target_ref = dict(object='process-grid-y-grid',point='',field='grid.bounds.y')
    source_ref = dict(object='process-grid-y-source',point='',field='artboard.width')
    source_board = dict(id=source_ref['object'],name='Grid y source',x=0,y=0,width=50,height=100)
    layout = dict(margin=dict(left=20,top=20,right=20,bottom=20),grid=dict(id=target_ref['object'],
        bounds=dict(x=20,y=40,width=560,height=400),columns=2,rows=1,column_gutter=20,row_gutter=0))
    requests = [
        dict(op='apply',expected_revision=0,commands=[
            dict(type='add_artboard',composition=composition['id'],artboard=source_board,index=1),
            dict(type='set_artboard_layout',composition=composition['id'],artboard_id=target['id'],layout=layout)]),
        dict(op='apply',expected_revision=1,commands=[dict(type='link_grid_bounds_y',target=target_ref,
            source=source_ref,replace_driver=False)]),
        dict(op='get',ref=target_ref),dict(op='properties'),dict(op='inspect'),
        dict(op='apply',expected_revision=2,commands=[dict(type='update_artboard',composition=composition['id'],
            artboard=dict(source_board,width=70))]),
        dict(op='get',ref=target_ref),dict(op='inspect'),
        dict(op='apply',expected_revision=3,commands=[dict(type='update_artboard',composition=composition['id'],
            artboard=dict(source_board,width=81))]),
        dict(op='apply',expected_revision=3,commands=[dict(type='update_artboard',composition=composition['id'],
            artboard=dict(target,height=469))]),
    ]
    process = subprocess.run([exe,'--serve',str(source_path)],input='\n'.join(map(json.dumps,requests))+'\n',
        capture_output=True,text=True,encoding='utf-8',timeout=15)
    replies = [json.loads(line) for line in process.stdout.splitlines()]
    check(process.returncode==0 and len(replies)==len(requests) and replies[0]['ok'] and replies[1]['ok'],
        'Grid y JSON-lines process accepts layout setup and the dedicated stable-Ref link command')
    linked = replies[2]['result']
    linked_native = replies[4]['result']
    updated = replies[6]['result']
    check(linked['authored']==dict(literal=40,driver=source_ref,source_kind='link') and linked['evaluated']==50 and
        next(item for item in replies[3]['result'] if item['ref']==target_ref)==linked and
        linked_native['version']=='0.50' and
        next(board for board in linked_native['compositions'][0]['artboards'] if board['id']==target['id'])['layout']['grid'][
            'bounds_y_driver']==dict(link=source_ref) and updated['authored']==linked['authored'] and updated['evaluated']==70,
        'JSON-lines get, properties and native 0.43 inspect preserve Grid y authorship while reporting upstream evaluation')
    check(not replies[8]['ok'] and replies[8]['error']['code']=='INVALID_LAYOUT' and replies[8]['revision']==3 and
        not replies[9]['ok'] and replies[9]['error']['code']=='INVALID_LAYOUT' and replies[9]['revision']==3,
        'JSON-lines rejects upstream y plus Grid height and target-height violations atomically')
    cold_path=Path(tmp)/'grid-bounds-y-cold.nect.json'
    cold_path.write_text(json.dumps(replies[7]['result']),encoding='utf-8')
    cold_requests=[dict(op='get',ref=target_ref),
        dict(op='apply',expected_revision=0,commands=[dict(type='unlink_grid_bounds_y',target=target_ref)]),
        dict(op='get',ref=target_ref),dict(op='inspect')]
    cold=subprocess.run([exe,'--serve',str(cold_path)],input='\n'.join(map(json.dumps,cold_requests))+'\n',
        capture_output=True,text=True,encoding='utf-8',timeout=10)
    cold_replies=[json.loads(line) for line in cold.stdout.splitlines()]
    cold_evidence=dict(returncode=cold.returncode,count=len(cold_replies),ok=[reply.get('ok') for reply in cold_replies],
        first_matches=bool(cold_replies and cold_replies[0].get('result')==updated),
        unlink=cold_replies[1].get('error') if len(cold_replies)>1 else None,
        frozen=cold_replies[2].get('result') if len(cold_replies)>2 else None,
        native_version=cold_replies[3].get('result',{}).get('version') if len(cold_replies)>3 else None)
    check(cold.returncode==0 and len(cold_replies)==4 and all(reply['ok'] for reply in cold_replies) and
        cold_replies[0]['result']==updated and cold_replies[2]['result']['authored']==
            dict(literal=70,driver=None,source_kind='literal') and cold_replies[2]['result']['evaluated']==70 and
        cold_replies[2]['result']['link'] is True and cold_replies[2]['result']['expression'] is True and
        cold_replies[3]['result']['version']=='0.50',
        'A separate JSON-lines process cold-opens Grid y and Unlink freezes its evaluated value: '+repr(cold_evidence))
    lied=json.loads(json.dumps(replies[7]['result']));lied['version']='0.38'
    check('INVALID_LAYOUT' in run('--validate',lied).stderr,
        'Native 0.38 version-lie cannot admit a Grid bounds y source')

with tempfile.TemporaryDirectory() as tmp:
    source_path=Path(tmp)/'grid-bounds-width.nect.json'
    source_path.write_text(json.dumps(sample),encoding='utf-8')
    composition=sample['compositions'][0]
    target=composition['artboards'][0]
    target_ref=dict(object='process-grid-width-grid',point='',field='grid.bounds.width')
    source_ref=dict(object='process-grid-width-source',point='',field='artboard.width')
    width_expression='ref("process-grid-width-source","","artboard.width") + 10'
    source_board=dict(id=source_ref['object'],name='Grid width source',x=0,y=0,width=40,height=100)
    layout=dict(margin=dict(left=20,top=20,right=20,bottom=20),grid=dict(id=target_ref['object'],
        bounds=dict(x=20,y=20,width=400,height=300),columns=2,rows=1,column_gutter=20,row_gutter=0))
    requests=[
        dict(op='apply',expected_revision=0,commands=[
            dict(type='add_artboard',composition=composition['id'],artboard=source_board,index=1),
            dict(type='set_artboard_layout',composition=composition['id'],artboard_id=target['id'],layout=layout)]),
        dict(op='apply',expected_revision=1,commands=[dict(type='link_grid_bounds_width',target=target_ref,
            source=source_ref,replace_driver=False)]),
        dict(op='get',ref=target_ref),dict(op='properties'),dict(op='inspect'),
        dict(op='apply',expected_revision=2,commands=[dict(type='update_artboard',composition=composition['id'],
            artboard=dict(source_board,width=70))]),
        dict(op='get',ref=target_ref),
        dict(op='apply',expected_revision=3,commands=[dict(type='set_grid_bounds_width_expression',target=target_ref,
            expression=dict(source=width_expression,version=1),replace_driver=True)]),
        dict(op='get',ref=target_ref),
        dict(op='apply',expected_revision=4,commands=[dict(type='update_artboard',composition=composition['id'],
            artboard=dict(source_board,width=940))]),
        dict(op='apply',expected_revision=4,commands=[dict(type='update_artboard',composition=composition['id'],
            artboard=dict(source_board,width=10))]),dict(op='inspect')]
    process=subprocess.run([exe,'--serve',str(source_path)],input='\n'.join(map(json.dumps,requests))+'\n',
        capture_output=True,text=True,encoding='utf-8',timeout=15)
    replies=[json.loads(line) for line in process.stdout.splitlines()]
    check(process.returncode==0 and len(replies)==len(requests) and replies[0]['ok'] and replies[1]['ok'],
        'Grid width JSON-lines process accepts setup and its dedicated stable-Ref link command')
    linked=replies[2]['result'];native=replies[4]['result'];updated=replies[6]['result'];expressed=replies[8]['result']
    check(linked['authored']==dict(literal=400,driver=source_ref,source_kind='link') and linked['evaluated']==40 and
        next(item for item in replies[3]['result'] if item['ref']==target_ref)==linked and native['version']=='0.50' and
        next(board for board in native['compositions'][0]['artboards'] if board['id']==target['id'])['layout']['grid'][
            'bounds_width_driver']==dict(link=source_ref) and updated['authored']==linked['authored'] and
        updated['evaluated']==70 and replies[7]['ok'] and expressed['authored']==
            dict(literal=400,driver=None,source_kind='expression',expression=dict(source=width_expression,version=1)) and
        expressed['evaluated']==80 and replies[11]['result']['version']=='0.50' and
        next(board for board in replies[11]['result']['compositions'][0]['artboards'] if board['id']==target['id'])['layout']['grid'][
            'bounds_width_expression']==dict(source=width_expression,version=1),
        'JSON-lines get, properties and native inspect preserve Grid width literal and exact expression evaluation')
    check(not replies[9]['ok'] and replies[9]['error']['code']=='INVALID_LAYOUT' and
        not replies[10]['ok'] and replies[10]['error']['code']=='INVALID_LAYOUT' and replies[10]['revision']==4,
        'JSON-lines rejects width containment and zero-cell changes atomically at the committed revision')
    cold_path=Path(tmp)/'grid-bounds-width-cold.nect.json'
    cold_path.write_text(json.dumps(replies[11]['result']),encoding='utf-8')
    cold_requests=[dict(op='get',ref=target_ref),
        dict(op='apply',expected_revision=0,commands=[dict(type='unlink_grid_bounds_width',target=target_ref)]),
        dict(op='get',ref=target_ref),dict(op='inspect')]
    cold=subprocess.run([exe,'--serve',str(cold_path)],input='\n'.join(map(json.dumps,cold_requests))+'\n',
        capture_output=True,text=True,encoding='utf-8',timeout=10)
    cold_replies=[json.loads(line) for line in cold.stdout.splitlines()]
    check(cold.returncode==0 and len(cold_replies)==4 and all(reply['ok'] for reply in cold_replies) and
        cold_replies[0]['result']==expressed and cold_replies[2]['result']['authored']==
            dict(literal=80,driver=None,source_kind='literal') and cold_replies[2]['result']['evaluated']==80 and
        cold_replies[3]['result']['version']=='0.50',
        'A distinct JSON-lines process cold-opens Grid width expression and Unlink freezes the evaluated value')
    lied=json.loads(json.dumps(replies[11]['result']));lied['version']='0.47'
    check('INVALID_LAYOUT' in run('--validate',lied).stderr,
        'Native 0.47 version-lie cannot admit a Grid width expression')

with tempfile.TemporaryDirectory() as tmp:
    source_path=Path(tmp)/'grid-bounds-height.nect.json'
    height_document=json.loads(json.dumps(sample))
    composition=height_document['compositions'][0]
    target=composition['artboards'][0]
    target.update(width=960,height=640)
    target_ref=dict(object='process-grid-height-grid',point='',field='grid.bounds.height')
    source_ref=dict(object='process-grid-height-source',point='',field='artboard.height')
    source_board=dict(id=source_ref['object'],name='Grid height source',x=0,y=0,width=100,height=500)
    layout=dict(grid=dict(id=target_ref['object'],bounds=dict(x=40,y=40,width=880,height=500),
        columns=2,rows=2,column_gutter=0,row_gutter=20))
    source_path.write_text(json.dumps(height_document),encoding='utf-8')
    requests=[
        dict(op='apply',expected_revision=0,commands=[
            dict(type='update_artboard',composition=composition['id'],artboard=target),
            dict(type='add_artboard',composition=composition['id'],artboard=source_board,index=1),
            dict(type='set_artboard_layout',composition=composition['id'],artboard_id=target['id'],layout=layout)]),
        dict(op='apply',expected_revision=1,commands=[dict(type='link_grid_bounds_height',target=target_ref,
            source=source_ref,replace_driver=False)]),
        dict(op='get',ref=target_ref),dict(op='properties'),dict(op='inspect'),
        dict(op='apply',expected_revision=2,commands=[dict(type='update_artboard',composition=composition['id'],
            artboard=dict(source_board,height=520))]),
        dict(op='get',ref=target_ref),
        dict(op='apply',expected_revision=3,commands=[dict(type='update_artboard',composition=composition['id'],
            artboard=dict(source_board,height=620))]),dict(op='inspect')]
    process=subprocess.run([exe,'--serve',str(source_path)],input='\n'.join(map(json.dumps,requests))+'\n',
        capture_output=True,text=True,encoding='utf-8',timeout=15)
    replies=[json.loads(line) for line in process.stdout.splitlines()]
    check(process.returncode==0 and len(replies)==len(requests) and all(reply['ok'] for reply in replies[:7]),
        'Grid height JSON-lines process accepts setup, typed link and a valid source edit')
    linked=replies[2]['result'];native=replies[4]['result'];updated=replies[6]['result']
    check(linked['authored']==dict(literal=500,driver=source_ref,source_kind='link') and linked['evaluated']==500 and
        next(item for item in replies[3]['result'] if item['ref']==target_ref)==linked and native['version']=='0.50' and
        next(board for board in native['compositions'][0]['artboards'] if board['id']==target['id'])['layout']['grid'][
            'bounds_height_driver']==dict(link=source_ref) and updated['authored']==linked['authored'] and
        updated['evaluated']==520 and updated['link'] is True and updated['expression'] is True and
        next(board for board in replies[8]['result']['compositions'][0]['artboards'] if board['id']==target['id'])[
            'layout']['grid']['bounds']['height']==500,
        'JSON-lines get, properties and native inspect preserve Grid height literal and exact evaluated Artboard source')
    check(not replies[7]['ok'] and replies[7]['error']['code']=='INVALID_LAYOUT' and replies[7]['revision']==3 and
        replies[8]['revision']==3,
        'JSON-lines rejects an upstream Grid bottom-edge violation atomically at the committed revision')
    cold_path=Path(tmp)/'grid-bounds-height-cold.nect.json'
    cold_path.write_text(json.dumps(replies[8]['result']),encoding='utf-8')
    cold=subprocess.run([exe,'--serve',str(cold_path)],input=json.dumps(dict(op='get',ref=target_ref))+'\n'+
        json.dumps(dict(op='apply',expected_revision=0,commands=[dict(type='unlink_grid_bounds_height',target=target_ref)]))+'\n'+
        json.dumps(dict(op='get',ref=target_ref))+'\n'+json.dumps(dict(op='inspect'))+'\n',
        capture_output=True,text=True,encoding='utf-8',timeout=10)
    cold_replies=[json.loads(line) for line in cold.stdout.splitlines()]
    check(cold.returncode==0 and len(cold_replies)==4 and all(reply['ok'] for reply in cold_replies) and
        cold_replies[0]['result']==updated and cold_replies[2]['result']['authored']==
            dict(literal=520,driver=None,source_kind='literal') and cold_replies[2]['result']['evaluated']==520 and
        cold_replies[2]['result']['link'] is True and cold_replies[2]['result']['expression'] is True and
        cold_replies[3]['result']['version']=='0.50',
        'A distinct JSON-lines cold process preserves the Grid height link and Unlink freezes its evaluated value')
    lied=json.loads(json.dumps(replies[8]['result']));lied['version']='0.48'
    check('INVALID_LAYOUT' in run('--validate',lied).stderr,
        'Native 0.48 version-lie cannot admit a Grid height driver')
    old=json.loads(json.dumps(replies[8]['result']));old['version']='0.48'
    next(board for board in old['compositions'][0]['artboards'] if board['id']==target['id'])['layout']['grid'].pop(
        'bounds_height_driver')
    check(run('--validate',old).returncode==0,
        'Native 0.48 remains readable when the optional Grid height driver is absent')

with tempfile.TemporaryDirectory() as tmp:
    source_path=Path(tmp)/'grid-bounds-height-expression.nect.json'
    document=json.loads(json.dumps(sample))
    composition=document['compositions'][0]
    target=composition['artboards'][0]
    target.update(width=960,height=640)
    target_ref=dict(object='process-grid-height-expression-grid',point='',field='grid.bounds.height')
    source_ref=dict(object='process-grid-height-expression-source',point='',field='artboard.height')
    expression='ref("process-grid-height-expression-source","","artboard.height") + 10'
    source_board=dict(id=source_ref['object'],name='Grid height expression source',x=0,y=0,width=100,height=490)
    layout=dict(grid=dict(id=target_ref['object'],bounds=dict(x=40,y=40,width=880,height=500),
        columns=2,rows=2,column_gutter=0,row_gutter=20))
    source_path.write_text(json.dumps(document),encoding='utf-8')
    requests=[
        dict(op='apply',expected_revision=0,commands=[
            dict(type='update_artboard',composition=composition['id'],artboard=target),
            dict(type='add_artboard',composition=composition['id'],artboard=source_board,index=1),
            dict(type='set_artboard_layout',composition=composition['id'],artboard_id=target['id'],layout=layout)]),
        dict(op='apply',expected_revision=1,commands=[dict(type='set_grid_bounds_height_expression',target=target_ref,
            expression=dict(source=expression,version=1),replace_driver=False)]),
        dict(op='get',ref=target_ref),dict(op='properties'),dict(op='inspect'),
        dict(op='apply',expected_revision=2,commands=[dict(type='update_artboard',composition=composition['id'],
            artboard=dict(source_board,height=510))]),dict(op='get',ref=target_ref),dict(op='inspect'),
        dict(op='apply',expected_revision=3,commands=[dict(type='update_artboard',composition=composition['id'],
            artboard=dict(source_board,height=610))]),
        dict(op='apply',expected_revision=3,commands=[dict(type='update_artboard',composition=composition['id'],
            artboard=dict(source_board,height=10))])]
    process=subprocess.run([exe,'--serve',str(source_path)],input='\n'.join(map(json.dumps,requests))+'\n',
        capture_output=True,text=True,encoding='utf-8',timeout=15)
    replies=[json.loads(line) for line in process.stdout.splitlines()]
    check(process.returncode==0 and len(replies)==len(requests) and all(replies[index]['ok'] for index in range(8)),
        'Grid height expression JSON-lines process accepts setup, dedicated expression command and a valid source edit')
    expressed=replies[2]['result'];native=replies[7]['result']
    check(expressed['authored']==dict(literal=500,driver=None,source_kind='expression',
          expression=dict(source=expression,version=1)) and expressed['evaluated']==500 and
        next(item for item in replies[3]['result'] if item['ref']==target_ref)==expressed and
        replies[6]['result']['authored']==expressed['authored'] and replies[6]['result']['evaluated']==520 and
        native['version']=='0.50' and
        next(board for board in native['compositions'][0]['artboards'] if board['id']==target['id'])[
            'layout']['grid']['bounds_height_expression']==dict(source=expression,version=1),
        'JSON-lines get, properties and native inspect preserve Grid height literal, exact expression and evaluated height')
    check(not replies[8]['ok'] and replies[8]['error']['code']=='INVALID_LAYOUT' and
        not replies[9]['ok'] and replies[9]['error']['code']=='INVALID_LAYOUT' and
        replies[8]['revision']==3 and replies[9]['revision']==3,
        'JSON-lines rejects height containment and zero-row-cell source edits atomically')
    cold_path=Path(tmp)/'grid-bounds-height-expression-cold.nect.json'
    cold_path.write_text(json.dumps(native),encoding='utf-8')
    cold_requests=[dict(op='get',ref=target_ref),
        dict(op='apply',expected_revision=0,commands=[dict(type='unlink_grid_bounds_height',target=target_ref)]),
        dict(op='get',ref=target_ref),dict(op='inspect')]
    cold=subprocess.run([exe,'--serve',str(cold_path)],input='\n'.join(map(json.dumps,cold_requests))+'\n',
        capture_output=True,text=True,encoding='utf-8',timeout=10)
    cold_replies=[json.loads(line) for line in cold.stdout.splitlines()]
    check(cold.returncode==0 and len(cold_replies)==4 and all(reply['ok'] for reply in cold_replies) and
        cold_replies[0]['result']==replies[6]['result'] and
        cold_replies[2]['result']['authored']==dict(literal=520,driver=None,source_kind='literal') and
        cold_replies[2]['result']['evaluated']==520 and cold_replies[3]['result']['version']=='0.50',
        'A distinct JSON-lines process cold-opens Grid height expression and Unlink freezes its evaluated value')
    lied=json.loads(json.dumps(native));lied['version']='0.49'
    check('INVALID_LAYOUT' in run('--validate',lied).stderr,
        'Native 0.49 version-lie cannot admit a Grid height expression')

duplicate = subprocess.run(
    [exe, '--serve'], input='{"op":"inspect","op":"apply"}\n',
    text=True, capture_output=True, timeout=10)
check(json.loads(duplicate.stdout)['error']['code']=='DUPLICATE_KEY', 'duplicate JSON keys rejected')

duplicate_escaped = subprocess.run(
    [exe, '--serve'], input=r'{"op":"inspect","o\u0070":"apply"}'+'\n',
    text=True, capture_output=True, timeout=10)
check(json.loads(duplicate_escaped.stdout)['error']['code']=='DUPLICATE_KEY',
      'escaped duplicate keys rejected')

print(f'PASS {checks} process checks')

# Real pre-migration bytes: linked freeform geometry authored by the 0.1 binary.
legacy = json.loads((Path(__file__).parent / 'fixtures/native-v0.1-linked.nect').read_text(encoding='utf-8'))
check(legacy['version'] == '0.1', 'fixture is the historical format')
with tempfile.TemporaryDirectory() as tmp:
    path = Path(tmp) / 'legacy.nect'
    path.write_text(json.dumps(legacy), encoding='utf-8')
    requests = [dict(op='inspect'), dict(op='apply', expected_revision=0, commands=[
        dict(type='set', ref=dict(object='path-A', point='point-A1', field='x'), value=321)]),
        dict(op='get', ref=dict(object='path-B', point='point-B1', field='x')),dict(op='inspect')]
    result = subprocess.run([exe,'--serve',str(path)], input='\n'.join(map(json.dumps,requests))+'\n',
        capture_output=True,text=True,timeout=10)
    replies = [json.loads(line) for line in result.stdout.splitlines()]
    check(all(r['ok'] for r in replies), 'legacy migration and edit succeed')
    migrated = replies[0]['result']
    projected = json.loads(json.dumps(migrated))
    projected['version'] = '0.1'
    remove_migrated_anchor_defaults(projected)
    check(projected.pop('named_colors')==[], 'legacy migration does not invent named colors')
    for obj in projected['objects']:
        if obj['kind'] != 'path':
            continue
        paint = obj.pop('stack')[0]
        check(paint['id'] == obj.pop('legacy_stroke'), 'legacy address refers to stable migrated stroke')
        obj['fill'] = 'none'
        obj['stroke'] = dict(rgba=[paint['parameters'][k] for k in ('r','g','b','a')], width=paint['parameters']['width'])
    check(projected == legacy, 'migration preserves every authored legacy value and reference')
    check(replies[2]['result']['evaluated'] == 341, 'legacy stable binding still evaluates after editing')
    saved = replies[3]['result']
    path.write_text(json.dumps(saved), encoding='utf-8')
    reopened = subprocess.run([exe,'--serve',str(path)],input='{"op":"inspect"}\n',
        capture_output=True,text=True,timeout=10)
    check(json.loads(reopened.stdout)['result'] == saved, 'upgraded native bytes reopen without authored drift')
    check(run('--validate',saved).returncode == 0,'upgraded file validates in fresh process')

    primitive = json.loads((Path(__file__).parent / 'fixtures/native-v0.2-primitive.nect').read_text(encoding='utf-8'))
    # Force an ID collision with the default migration paint name, and retain a
    # legacy stroke dependency; migration must allocate, not steal an old ID.
    primitive['objects'][1]['contours'][0]['id'] = 'path-A-stroke'
    primitive['objects'][2]['stroke']['width']['binding'] = dict(
        source=dict(object='path-A',point='',field='stroke.width'),scale=2,offset=1,mode='copy_local_value')
    path.write_text(json.dumps(primitive),encoding='utf-8')
    migrated_run = subprocess.run([exe,'--serve',str(path)],input='{"op":"inspect"}\n'+
        '{"op":"get","ref":{"object":"path-B","point":"","field":"stroke.width"}}\n',
        capture_output=True,text=True,timeout=10)
    result = [json.loads(line) for line in migrated_run.stdout.splitlines()]
    check(all(x['ok'] for x in result),'retained primitive file migrates')
    new = result[0]['result']
    old_circle = next(x for x in primitive['objects'] if x['id']=='circle')
    new_circle = next(x for x in new['objects'] if x['id']=='circle')
    check(new_circle['source']==old_circle['source'] and new_circle['point_edit']==old_circle['point_edit'],
        '0.2 migration preserves generator and corrections verbatim')
    a = next(x for x in new['objects'] if x['id']=='path-A')
    check(a['legacy_stroke']!='path-A-stroke' and a['contours'][0]['id']=='path-A-stroke',
        'new paint identity avoids all existing document identities')
    check(result[1]['result']['evaluated']==5,'legacy stroke binding survives stack migration')
    check(run('--validate',new).returncode==0,'migrated source, correction and stack reopen')
# Real 0.3 production scene must retain its complete procedural state in 0.4.
ornament = Path(__file__).parent.parent / 'examples/radial-ornament.nect'
old = json.loads(ornament.read_text(encoding='utf-8'))
check(old['version']=='0.3','production fixture remains historical 0.3')
migrated_run = subprocess.run([exe,'--serve',str(ornament)],input='{"op":"inspect"}\n',
    capture_output=True,text=True,timeout=10)
new=json.loads(migrated_run.stdout)['result'];remove_migrated_anchor_defaults(new);new['version']='0.3'
check(new.pop('named_colors')==[], '0.3 migration starts with no named colors')
check(new==old,'0.3 migration retains all paint, repeat, binding and correction state')
check(run('--svg',old).stdout==(ornament.with_suffix('.svg')).read_text(encoding='utf-8'),
    'solid 0.3 scene exports identical SVG after migration')
with tempfile.TemporaryDirectory() as tmp:
    operation = dict(id='cold-posterize', type='nect.group.posterize', version=1,
                     enabled=True, parameters=dict(levels=dict(literal=3)),
                     composite='below', fill_rule='nonzero')
    commands = [dict(type='add_operation', object='ornament', index=0, operation=operation)]
    authored = subprocess.run([exe, '--serve', str(ornament)], input='\n'.join(map(json.dumps, [
        dict(op='apply', expected_revision=0, commands=commands), dict(op='inspect')]))+'\n',
        capture_output=True, text=True, encoding='utf-8', timeout=10)
    replies = [json.loads(line) for line in authored.stdout.splitlines()]
    check(authored.returncode == 0 and len(replies) == 2 and replies[0]['ok'] and replies[1]['ok'],
          'Group Posterize authors through the real JSON-lines process')
    group_native = replies[1]['result']
    check(group_native['version'] == '0.50' and
          next(obj for obj in group_native['objects'] if obj['id'] == 'ornament')['stack'] == [operation],
          'Current writer preserves Group Posterize identity, version, level, and order')
    cold_path = Path(tmp) / 'group-posterize.nect'
    cold_path.write_text(json.dumps(group_native), encoding='utf-8')
    reopened = subprocess.run([exe, '--serve', str(cold_path)], input='{"op":"inspect"}\n',
        capture_output=True, text=True, encoding='utf-8', timeout=10)
    check(reopened.returncode == 0 and json.loads(reopened.stdout)['result'] == group_native,
          'Cold process reopens exact editable Group Posterize native state')
    check('UNSUPPORTED_SVG_EFFECT' in run('--svg', group_native).stderr,
          'Cold Group Posterize refuses a silently lossy SVG derivative')
gradient_path=ornament.with_name('gradient-ornament.nect')
old=json.loads(gradient_path.read_text(encoding='utf-8'))
check(old['version']=='0.4','gradient fixture remains historical 0.4')
upgraded=subprocess.run([exe,'--serve',str(gradient_path)],input='{"op":"inspect"}\n',capture_output=True,text=True,timeout=10)
new=json.loads(upgraded.stdout)['result'];remove_migrated_anchor_defaults(new);new['version']='0.4'
check(new.pop('named_colors')==[], '0.4 migration starts with no named colors')
check(new==old,'0.4 migration preserves gradients and their linked stable stops')
check(run('--svg',old).stdout==gradient_path.with_suffix('.svg').read_text(encoding='utf-8'),
    'gradient 0.4 scene exports identical SVG after frame migration')
frames=json.loads(json.dumps(sample));frames['version']='0.5'
frames.pop('named_colors')
remove_migrated_anchor_defaults(frames)
composition=frames['compositions'][0]
composition['artboards'].append(dict(id='requested-crop',name='Crop',x=100,y=50,width=300,height=250))
requested=subprocess.run([exe,'--svg',composition['id'],'requested-crop'],input=json.dumps(frames),capture_output=True,text=True,timeout=10)
check(requested.returncode==0 and ET.fromstring(requested.stdout).attrib['viewBox']=='100 50 300 250',
    'CLI can export a non-first frame by stable Composition/Artboard IDs')
frames_path=ornament.with_name('artboard-studies.nect')
old=json.loads(frames_path.read_text(encoding='utf-8'))
check(old['version']=='0.5','frame fixture remains historical 0.5')
upgraded=subprocess.run([exe,'--serve',str(frames_path)],input='{"op":"inspect"}\n',capture_output=True,text=True,encoding='utf-8',timeout=10)
new=json.loads(upgraded.stdout)['result'];remove_migrated_anchor_defaults(new);new['version']='0.5'
check(new.pop('named_colors')==[], '0.5 migration starts with no named colors')
check(new==old,'0.5 migration preserves ordered frames, inheritance and all authored artwork')
text_path=ornament.with_name('typography-poster.nect')
old=json.loads(text_path.read_text(encoding='utf-8'))
check(old['version']=='0.6','Text fixture remains historical 0.6')
upgraded=subprocess.run([exe,'--serve',str(text_path)],input='{"op":"inspect"}\n',capture_output=True,text=True,encoding='utf-8',timeout=10)
new=json.loads(upgraded.stdout)['result'];remove_migrated_anchor_defaults(new);new['version']='0.6'
check(new.pop('named_colors')==[], '0.6 migration starts with no named colors')
check(new==old,'0.6 migration preserves all editable Text and shape inputs')
color_path=ornament.with_name('named-color-poster.nect')
old=json.loads(color_path.read_text(encoding='utf-8'))
check(old['version']=='0.7','named-color fixture remains historical 0.7')
upgraded=subprocess.run([exe,'--serve',str(color_path)],input='{"op":"inspect"}\n',capture_output=True,text=True,encoding='utf-8',timeout=10)
new=json.loads(upgraded.stdout)['result'];check(new['version']=='0.50','current writer uses native 0.50')
remove_migrated_anchor_defaults(new);new['version']='0.7';check(new==old,'0.7 migration preserves named colors, links, Text and authored geometry')
polystar_path=ornament.with_name('polystar-field.nect')
old=json.loads(polystar_path.read_text(encoding='utf-8'))
check(old['version']=='0.8','Polystar fixture remains historical 0.8')
upgraded=subprocess.run([exe,'--serve',str(polystar_path)],input='{"op":"inspect"}\n'+
    '{"op":"properties"}\n',capture_output=True,text=True,encoding='utf-8',timeout=10)
replies=[json.loads(line) for line in upgraded.stdout.splitlines()]
new=replies[0]['result'];remove_migrated_anchor_defaults(new);new['version']='0.8'
check(new==old,'0.8 migration preserves linked count, angular correction, all paints and text')
# Catch the documented field vocabulary falling behind real numeric properties.
# This checks that specific schema boundary; the native codec remains the validator.
schema=json.loads((polystar_path.parent.parent/'schemas/native-v0.17.schema.json').read_text())
field_rules=schema['$defs']['ref']['properties']['field']['anyOf']
for property_ in replies[1]['result']:
    if property_['type']!='number': continue
    name=property_['ref']['field']
    # Typed Artboard and Guide views are surfaced by `properties()`, but their
    # Refs are not members of the native scalar Ref vocabulary.
    if name.startswith('artboard.') or name=='guide.position': continue
    check(any(name in rule.get('enum',[]) or ('pattern' in rule and re.fullmatch(rule['pattern'],name)) for rule in field_rules),
          'schema accepts emitted numeric field '+name)
old_schema=json.loads((polystar_path.parent.parent/'schemas/native-v0.16.schema.json').read_text())
content_schema=schema['$defs']['text_source']['properties']['content_driver']
content_driver=schema['$defs']['content_driver']
content_ref=schema['$defs']['content_ref']
check(schema['properties']['version']['const']=='0.17' and 'content_driver' not in old_schema['$defs']['text_source']['properties'] and
      content_schema['$ref']=='#/$defs/content_driver' and content_driver['additionalProperties'] is False and
      content_driver['required']==['link'] and content_ref['properties']['point']['const']=='' and
      content_ref['properties']['field']['const']=='text.content',
      'Native 0.17 schema adds only the closed same-type Text content Ref driver')
current_schema=json.loads((polystar_path.parent.parent/'schemas/native-v0.18.schema.json').read_text())
family_schema=current_schema['$defs']['text_source']['properties']['family_driver']
family_driver=current_schema['$defs']['family_driver'];family_ref=current_schema['$defs']['family_ref']
check(current_schema['properties']['version']['const']=='0.18' and
      'family_driver' not in schema['$defs']['text_source']['properties'] and
      family_schema['$ref']=='#/$defs/family_driver' and family_driver['additionalProperties'] is False and
      family_driver['required']==['link'] and family_ref['properties']['point']['const']=='' and
      family_ref['properties']['field']['const']=='text.family',
      'Native 0.18 adds only the closed same-type Text family Ref driver')
direction_schema=json.loads((polystar_path.parent.parent/'schemas/native-v0.20.schema.json').read_text())
direction_field=direction_schema['$defs']['text_source']['properties']['direction_driver']
direction_driver=direction_schema['$defs']['direction_driver'];direction_ref=direction_schema['$defs']['direction_ref']
check(direction_schema['properties']['version']['const']=='0.20' and
      'direction_driver' not in current_schema['$defs']['text_source']['properties'] and
      direction_field['$ref']=='#/$defs/direction_driver' and direction_driver['additionalProperties'] is False and
      direction_driver['required']==['link'] and direction_ref['properties']['point']['const']=='' and
      direction_ref['properties']['field']['const']=='text.direction',
      'Native 0.20 schema retains the closed same-type Text direction Ref driver')
previous_schema=json.loads((polystar_path.parent.parent/'schemas/native-v0.19.schema.json').read_text())
layout_field=direction_schema['$defs']['text_source']['properties']['layout_driver']
layout_driver=direction_schema['$defs']['layout_driver'];layout_ref=direction_schema['$defs']['layout_ref']
check('layout_driver' not in previous_schema['$defs']['text_source']['properties'] and
      layout_field['$ref']=='#/$defs/layout_driver' and layout_driver['additionalProperties'] is False and
      layout_driver['required']==['link'] and layout_ref['properties']['point']['const']=='' and
      layout_ref['properties']['field']['const']=='text.layout',
      'Native 0.20 adds only the closed same-type Text layout Ref driver')
# Native expressions remain authored and are forbidden in all earlier versions.
expression_doc=json.loads(json.dumps(sample))
target=next(o for o in expression_doc['objects'] if o['id']=='path-B')
formula='ref("path-A","point-A1","x") * 2 + 3'
target['transform'][4]['expression']={'source':formula,'version':1}
check(run('--validate',expression_doc).returncode==0,'expression document validates')
for version in ('0.8','0.9'):
    legacy_expr=json.loads(json.dumps(expression_doc));legacy_expr['version']=version
    if version=='0.8': remove_migrated_anchor_defaults(legacy_expr)
    else: remove_migrated_compositing_defaults(legacy_expr)
    check('UNKNOWN_FIELD' in run('--validate',legacy_expr).stderr,'legacy '+version+' rejects expression source')
for invalid in ({'source':'1','version':2},{'source':'1','version':1,'javascript':True}):
    bad=json.loads(json.dumps(expression_doc));next(o for o in bad['objects'] if o['id']=='path-B')['transform'][4]['expression']=invalid
    check(run('--validate',bad).returncode==2,'unknown formula semantic/field rejects')
with tempfile.TemporaryDirectory() as tmp:
    path=Path(tmp)/'expression.nect';path.write_text(json.dumps(expression_doc),encoding='utf-8')
    ref={'object':'path-B','point':'','field':'transform.tx'}
    commands=[{'op':'inspect'},{'op':'get','ref':ref},{'op':'expression_language'},
              {'op':'apply','expected_revision':0,'commands':[{'type':'set_expression','targets':[ref],
                 'expression':{'source':formula+' + 7','version':1},'replace_binding':False}]},
              {'op':'get','ref':ref},{'op':'undo','expected_revision':1},{'op':'inspect'}]
    proc=subprocess.run([exe,'--serve',str(path)],input='\n'.join(json.dumps(c) for c in commands)+'\n',capture_output=True,text=True,encoding='utf-8',timeout=10)
    replies=[json.loads(line) for line in proc.stdout.splitlines()]
    check(all(r['ok'] for r in replies),'formula discovery/apply/readback/undo succeeds')
    check(replies[0]['result']==expression_doc and replies[-1]['result']==expression_doc,'native formula source and literals survive exact roundtrip and Undo')
    check(replies[1]['result']['evaluated']==203 and replies[4]['result']['evaluated']==210,'formula command uses the shared evaluator')
    check(replies[2]['result']['trigonometry']=='degrees','formula language is discoverable')
# Native0.10 remains an exact authored source after default compositing migration.
expression_path=ornament.with_name('phase-form.nect')
old=json.loads(expression_path.read_text(encoding='utf-8'));check(old['version']=='0.10','expression fixture stays historical0.10')
upgraded=subprocess.run([exe,'--serve',str(expression_path)],input='{"op":"inspect"}\n',capture_output=True,text=True,encoding='utf-8',timeout=10)
new=json.loads(upgraded.stdout)['result'];remove_migrated_compositing_defaults(new);new['version']='0.10'
check(new==old,'0.10 migration retains exact formula source and all authored inputs')
with tempfile.TemporaryDirectory() as tmp:
    path=Path(tmp)/'masked.nect';path.write_text(json.dumps(sample),encoding='utf-8')
    comp=sample['compositions'][0]
    cmds=[dict(type='mask_objects',composition=comp['id'],parent='',members=comp['roots'],id='mask-group',mask_id='geometry-mask',name='Mask',top=True),
          dict(type='set',ref=dict(object='mask-group',point='',field='composite.opacity'),value=.5),
          dict(type='set_compositing',object='mask-group',blend='multiply',isolated=False)]
    requests=[dict(op='apply',expected_revision=0,commands=cmds),dict(op='inspect'),dict(op='compositing_plan',composition=comp['id']),
              dict(op='export_svg',composition=comp['id'],artboard=comp['artboards'][0]['id']),dict(op='undo',expected_revision=1),dict(op='inspect')]
    proc=subprocess.run([exe,'--serve',str(path)],input='\n'.join(json.dumps(r) for r in requests)+'\n',capture_output=True,text=True,encoding='utf-8',timeout=10)
    replies=[json.loads(line) for line in proc.stdout.splitlines()];check(all(r['ok'] for r in replies),'mask authoring/plan/SVG/Undo succeeds')
    native=replies[1]['result'];group=next(o for o in native['objects'] if o['id']=='mask-group')
    check(group['compositing']['mask']['source']==comp['roots'][-1] and group['compositing']['opacity']==dict(literal=.5),'mask uses final paint order and ordinary opacity')
    plan=replies[2]['result'];check(plan['backdrop']=='transparent' and plan['roots'][0]['isolated'],'mask/blend aggregate is explicitly isolated')
    svg_root=ET.fromstring(replies[3]['result']);ns={'s':'http://www.w3.org/2000/svg'}
    clip=svg_root.find('.//s:clipPath',ns);check(clip.attrib['id']=='geometry-mask' and clip.attrib['clipPathUnits']=='userSpaceOnUse','SVG clip uses stable identity and Composition coordinates')
    group_svg=svg_root.find("s:g[@id='mask-group']",ns);check(group_svg.attrib['opacity']=='0.5' and 'mix-blend-mode:multiply' in group_svg.attrib['style'],'SVG retains aggregate opacity and declared blend')
    check(svg_root.find(".//s:g[@id='"+comp['roots'][-1]+"']",ns) is None,'hidden mask source not exported as artwork')
    check(replies[-1]['result']==sample,'one Undo restores all masked-group inputs')
    check(run('--validate',native).returncode==0,'masked native reopens in a fresh process')
    native['version']='0.10';check('UNKNOWN_FIELD' in run('--validate',native).stderr,'old format rejects new compositing fields')
# Native0.11 compositing inputs stay exact; Offset is a new, strict0.12 type.
mask_path=ornament.with_name('colour-cut.nect')
old=json.loads(mask_path.read_text(encoding='utf-8'));check(old['version']=='0.11','mask fixture stays historical0.11')
upgraded=subprocess.run([exe,'--serve',str(mask_path)],input='{"op":"inspect"}\n',capture_output=True,text=True,encoding='utf-8',timeout=10)
new=json.loads(upgraded.stdout)['result'];remove_migrated_asset_defaults(new);new['version']='0.11'
check(new==old,'0.11 migration retains all exact masks, blends, references and geometry')
with tempfile.TemporaryDirectory() as tmp:
    path=Path(tmp)/'offset.nect';path.write_text(json.dumps(sample),encoding='utf-8')
    primitive=dict(id='offset-source',type='nect.shape.rectangle',version=1,
        parameters={k:dict(literal=v) for k,v in dict(center_x=50,center_y=30,width=100,height=60).items()})
    offset=dict(id='offset-op',type='nect.shape.offset',version=1,enabled=True,composite='below',fill_rule='nonzero',line_join='miter',
        parameters=dict(amount=dict(literal=10),miter_limit=dict(literal=4)))
    commands=[dict(type='create_primitive',composition=comp['id'],parent='',id='offset-box',name='Offset box',source=primitive),
              dict(type='add_operation',object='offset-box',operation=offset,index=1)]
    reference=dict(object='offset-box',point='',field='op.offset-op.amount')
    requests=[dict(op='operator_types'),dict(op='apply',expected_revision=0,commands=commands),dict(op='inspect'),
              dict(op='export_svg',composition=comp['id'],artboard=comp['artboards'][0]['id']),
              dict(op='get',ref=reference),dict(op='apply',expected_revision=1,commands=[dict(type='operation_options',object='offset-box',operation='offset-op',composite='below',fill_rule='evenodd',line_join='round')]),
              dict(op='inspect'),dict(op='undo',expected_revision=2),dict(op='inspect'),dict(op='undo',expected_revision=3),dict(op='inspect')]
    proc=subprocess.run([exe,'--serve',str(path)],input='\n'.join(json.dumps(r) for r in requests)+'\n',capture_output=True,text=True,encoding='utf-8',timeout=10)
    replies=[json.loads(line) for line in proc.stdout.splitlines()];check(all(r['ok'] for r in replies),'Offset discovery/commands/native/SVG/options/Undo succeed')
    definition=next(v for v in replies[0]['result'] if v['type']=='nect.shape.offset')
    check(definition['template']==dict(offset,id='new-operation') and definition['curve_flattening_tolerance_du']==.1,'Offset template and approximation disclosed')
    native=replies[2]['result'];obj=next(o for o in native['objects'] if o['id']=='offset-box')
    check(obj['source']==primitive and obj['stack'][-1]==offset,'native retains source and exact Offset settings')
    check(replies[4]['result']['evaluated']==10 and next(p for p in definition['parameters'] if p['name']=='amount')['unit']=='du','Offset amount is an ordinary length property')
    check(next(o for o in replies[6]['result']['objects'] if o['id']=='offset-box')['stack'][-1]['line_join']=='round','join options share commands')
    check(replies[8]['result']==native and replies[-1]['result']==sample,'each Offset change undoes exactly once')
    root=ET.fromstring(replies[3]['result']);node=root.find(".//{http://www.w3.org/2000/svg}g[@id='offset-box']")
    svg_path=node.find('.//{http://www.w3.org/2000/svg}path')
    numbers=[float(v) for v in re.findall(r'-?\d+(?:\.\d+)?(?:[eE][+-]?\d+)?',svg_path.attrib['d'])]
    check(min(numbers[0::2])==-10 and max(numbers[0::2])==110 and min(numbers[1::2])==-10 and max(numbers[1::2])==70,'SVG exports expanded prior Stroke geometry')
    check(run('--validate',native).returncode==0,'Offset native reopens in a new process')
    native['version']='0.11';remove_migrated_asset_defaults(native);check('UNKNOWN_FIELD' in run('--validate',native).stderr,'old format rejects Offset line-join field')
    next(o for o in native['objects'] if o['id']=='offset-box')['stack'][-1].pop('line_join')
    check('UNSUPPORTED_OPERATOR' in run('--validate',native).stderr,'old format rejects Offset type even without its new field')

# The JSON-lines process shares the typed Text weight commands and projections.
with tempfile.TemporaryDirectory() as tmp:
    path=Path(tmp)/'text-weight.nect';path.write_text(json.dumps(sample),encoding='utf-8')
    composition_id=sample['compositions'][0]['id']
    def text_source(source_id,content,weight):
        return dict(id=source_id,version=1,content=content,family='Yu Gothic',locale='ja-JP',layout='auto',
            direction='horizontal',alignment='start',weight=weight,italic=False,
            parameters={name:dict(literal=value) for name,value in dict(origin_x=0,origin_y=0,font_size=48,
                frame_width=400,frame_height=200,tracking=0,line_spacing=0).items()})
    source_a=text_source('weight-a-source','Weight A',700);source_b=text_source('weight-b-source','Weight B',400)
    ref_a=dict(object='weight-a',point='',field='text.weight');ref_b=dict(object='weight-b',point='',field='text.weight')
    requests=[
        dict(op='apply',expected_revision=0,commands=[dict(type='create_text',composition=composition_id,parent='',id='weight-a',name='Weight A',source=source_a),
            dict(type='create_text',composition=composition_id,parent='',id='weight-b',name='Weight B',source=source_b)]),
        dict(op='resolve_name',name='Weight A',point='',field='text.weight'),dict(op='properties'),
        dict(op='apply',expected_revision=1,commands=[dict(type='link_text_weight',target=ref_b,source=ref_a,replace_driver=False)]),
        dict(op='get',ref=ref_b),dict(op='apply',expected_revision=2,commands=[dict(type='update_text',object='weight-a',source=dict(source_a,weight=300))]),
        dict(op='get',ref=ref_b),dict(op='apply',expected_revision=3,commands=[dict(type='update_text',object='weight-b',source=dict(source_b,weight=450))]),
        dict(op='inspect'),dict(op='text_layout',object='weight-b'),
        dict(op='apply',expected_revision=3,commands=[dict(type='unlink_text_weight',target=ref_b)]),
        dict(op='undo',expected_revision=4),dict(op='inspect'),dict(op='get',ref=ref_b)]
    proc=subprocess.run([exe,'--serve',str(path)],input='\n'.join(map(json.dumps,requests))+'\n',
        capture_output=True,text=True,encoding='utf-8',timeout=20)
    replies=[json.loads(line) for line in proc.stdout.splitlines()]
    check(len(replies)==len(requests),'Text weight process returns one response for every request')
    check(all(replies[i]['ok'] for i in (0,1,2,3,4,5,6,8,9,10,11,12,13)),
        'Text weight API create/discovery/link/update/layout/unlink/Undo succeeds')
    check(replies[1]['result']==ref_a,'JSON-lines resolve_name returns the stable Text weight Ref')
    metadata=next(value for value in replies[2]['result'] if value['ref']==ref_b)
    check(metadata['type']=='integer' and metadata['unit']=='unitless' and metadata['range']==dict(min=1,max=999) and
        metadata['authored']==dict(literal=400,driver=None),'Text weight properties report the authored integer contract')
    check(replies[4]['result']['evaluated']==700 and replies[4]['result']['authored']==dict(literal=400,driver=dict(link=ref_a)) and
        replies[6]['result']['evaluated']==300,'JSON-lines get preserves the literal while the stable source changes')
    check(not replies[7]['ok'] and replies[7]['error']['code']=='DRIVEN_PROPERTY' and replies[7]['revision']==3,
        'A JSON-lines driven literal edit fails without advancing revision')
    source_objects={obj['id']:obj for obj in replies[8]['result']['objects']}
    check(source_objects['weight-b']['text']['weight']==400 and source_objects['weight-b']['text']['weight_driver']==dict(link=ref_a),
        'Rejected process command preserves the authored source and link')
    check(replies[9]['result']['weight']==300,'Text layout consumes the evaluated linked weight')
    check(replies[12]['result']['version']=='0.50' and replies[12]['result']==replies[8]['result'],
        'Undo restores the pre-unlink native state exactly')
    check(replies[13]['result']['evaluated']==300 and replies[13]['result']['authored']['literal']==400,
        'Undo restores the stable driver and its evaluated integer through a fresh request')
    check(run('--validate',replies[12]['result']).returncode==0,'Native 0.23 Text weight document validates in a fresh process')
    old_weight=json.loads(json.dumps(replies[12]['result']));old_weight['version']='0.15'
    check(run('--validate',old_weight).returncode==2 and 'UNSUPPORTED_TEXT_WEIGHT_DRIVER' in run('--validate',old_weight).stderr,
        'Native 0.15 rejects the new Text weight driver instead of dropping it')

# The first string-valued Text source address uses its own link-only command and native field.
with tempfile.TemporaryDirectory() as tmp:
    path=Path(tmp)/'text-content.nect';path.write_text(json.dumps(sample),encoding='utf-8')
    composition_id=sample['compositions'][0]['id']
    def content_source(source_id,content):
        return dict(id=source_id,version=1,content=content,family='Yu Gothic',locale='ja-JP',layout='auto',
            direction='horizontal',alignment='start',weight=400,italic=False,
            parameters={name:dict(literal=value) for name,value in dict(origin_x=0,origin_y=0,font_size=48,
                frame_width=400,frame_height=200,tracking=0,line_spacing=0).items()})
    source_a=content_source('content-a-source','Source A');source_a['family']='Source family'
    source_b=content_source('content-b-source','Manual B');source_b['family']='Manual family'
    ref_a=dict(object='content-a',point='',field='text.content');ref_b=dict(object='content-b',point='',field='text.content')
    family_ref_a=dict(object='content-a',point='',field='text.family');family_ref_b=dict(object='content-b',point='',field='text.family')
    requests=[
        dict(op='apply',expected_revision=0,commands=[dict(type='create_text',composition=composition_id,parent='',id='content-a',name='Content A',source=source_a),
            dict(type='create_text',composition=composition_id,parent='',id='content-b',name='Content B',source=source_b)]),
        dict(op='get',ref=ref_b),
        dict(op='apply',expected_revision=1,commands=[dict(type='link_text_content',target=ref_b,source=ref_a,replace_driver=False)]),
        dict(op='properties'),dict(op='apply',expected_revision=2,commands=[dict(type='update_text',object='content-a',source=dict(source_a,content='Revised A'))]),
        dict(op='get',ref=ref_b),dict(op='apply',expected_revision=3,commands=[dict(type='update_text',object='content-b',source=dict(source_b,content='Blocked B'))]),
        dict(op='inspect'),dict(op='apply',expected_revision=3,commands=[dict(type='unlink_text_content',target=ref_b)]),
        dict(op='get',ref=ref_b),dict(op='undo',expected_revision=4),dict(op='get',ref=ref_b),dict(op='inspect'),
        dict(op='apply',expected_revision=5,commands=[dict(type='link_text_family',target=family_ref_b,source=family_ref_a,replace_driver=False)]),
        dict(op='apply',expected_revision=6,commands=[dict(type='update_text',object='content-a',source=dict(source_a,content='Revised A',family='Revised source family'))]),
        dict(op='get',ref=family_ref_b),dict(op='properties'),
        dict(op='apply',expected_revision=7,commands=[dict(type='update_text',object='content-b',source=dict(source_b,family='Blocked family'))]),
        dict(op='inspect'),dict(op='apply',expected_revision=7,commands=[dict(type='unlink_text_family',target=family_ref_b)]),
        dict(op='undo',expected_revision=8),dict(op='get',ref=family_ref_b),dict(op='inspect')]
    requests.extend([dict(op='text_layout',object='content-b'),
        dict(op='apply',expected_revision=9,commands=[dict(type='unlink_text_family',target=family_ref_b)]),
        dict(op='get',ref=family_ref_b),dict(op='text_layout',object='content-b')])
    proc=subprocess.run([exe,'--serve',str(path)],input='\n'.join(map(json.dumps,requests))+'\n',
        capture_output=True,text=True,encoding='utf-8',timeout=20)
    replies=[json.loads(line) for line in proc.stdout.splitlines()]
    check(len(replies)==len(requests) and all(replies[i]['ok'] for i in (0,1,2,3,4,5,7,8,9,10,11,12)),
        'Text content API create/discovery/link/update/unlink/Undo requests succeed')
    check(replies[1]['result']['authored']==dict(literal='Manual B',driver=None) and replies[1]['result']['link'] is True,
        'JSON-lines Text content exposes a typed string literal and link capability')
    metadata=next(value for value in replies[3]['result'] if value['ref']==ref_b)
    check(metadata['type']=='string' and metadata['authored']==dict(literal='Manual B',driver=dict(link=ref_a)) and
        metadata['evaluated']=='Source A' and replies[5]['result']['evaluated']=='Revised A',
        'JSON-lines properties and get retain the authored string as the stable source changes')
    check(not replies[6]['ok'] and replies[6]['error']['code']=='DRIVEN_PROPERTY' and replies[6]['revision']==3,
        'JSON-lines UpdateText cannot change a driven content literal')
    source_objects={obj['id']:obj for obj in replies[7]['result']['objects']}
    check(source_objects['content-b']['text']['content']=='Manual B' and
        source_objects['content-b']['text']['content_driver']==dict(link=ref_a),
        'Rejected process command preserves the literal and content driver')
    check(replies[9]['result']['authored']==dict(literal='Revised A',driver=None) and
        replies[11]['result']['evaluated']=='Revised A' and replies[11]['result']['authored']['driver']==dict(link=ref_a),
        'Unlink freezes content and Undo restores its link and current evaluation')
    check(all(replies[i]['ok'] for i in (13,14,15,16,18,19,20,21,22,23,24,25,26)),
        'Text family API link, source update, discovery, unlink and Undo requests succeed')
    check(replies[15]['result']['authored']==dict(literal='Manual family',driver=dict(link=family_ref_a)) and
        replies[15]['result']['evaluated']=='Revised source family' and
        any(item['ref']==family_ref_b and item['link'] is True and item['evaluated']=='Revised source family' for item in replies[16]['result']),
        'JSON-lines family reads preserve the authored literal and evaluate the linked source')
    check(not replies[17]['ok'] and replies[17]['error']['code']=='DRIVEN_PROPERTY' and replies[17]['revision']==7,
        'JSON-lines UpdateText cannot change a driven font-family literal')
    linked_layout=replies[23]['result'];frozen_layout=replies[26]['result']
    layout_fields=('x','y','width','height','overflow','glyph_count','warnings','used_fonts')
    check(replies[25]['result']['authored']==dict(literal='Revised source family',driver=None) and
        linked_layout['glyph_count']>0 and linked_layout['used_fonts'] and
        all(linked_layout[field]==frozen_layout[field] for field in layout_fields),
        'Text layout consumes the linked family, and unlink freezes identical geometry, warnings and used fonts')
    native=replies[12]['result'];check(native['version']=='0.50' and run('--validate',native).returncode==0,
        'Native 0.23 content link validates in a separate CLI process')
    family_native=replies[22]['result'];check(family_native['version']=='0.50' and run('--validate',family_native).returncode==0,
        'Native 0.23 family link validates in a separate CLI process')
    path.write_text(json.dumps(family_native,ensure_ascii=False),encoding='utf-8');before=path.read_bytes()
    cold=subprocess.run([exe,'--serve',str(path)],input=json.dumps(dict(op='get',ref=ref_b))+'\n'+
        json.dumps(dict(op='get',ref=family_ref_b))+'\n'+json.dumps(dict(op='inspect'))+'\n',
        capture_output=True,text=True,encoding='utf-8',timeout=20)
    cold_replies=[json.loads(line) for line in cold.stdout.splitlines()]
    check(all(reply['ok'] for reply in cold_replies) and cold_replies[0]['result']['evaluated']=='Revised A' and
        cold_replies[0]['result']['authored']['driver']==dict(link=ref_a) and
        cold_replies[1]['result']['evaluated']=='Revised source family' and
        cold_replies[1]['result']['authored']['driver']==dict(link=family_ref_a) and path.read_bytes()==before,
        'A separate JSON-lines cold open preserves both Text links and evaluated values without changing native bytes')

# Text writing direction is a closed enum with its own same-field link and projection.
with tempfile.TemporaryDirectory() as tmp:
    path=Path(tmp)/'text-direction.nect';path.write_text(json.dumps(sample),encoding='utf-8')
    composition_id=sample['compositions'][0]['id']
    def direction_source(source_id,direction):
        return dict(id=source_id,version=1,content='Nect Direction',family='Yu Gothic',locale='ja-JP',layout='auto',
            direction=direction,alignment='start',weight=400,italic=False,
            parameters={name:dict(literal=value) for name,value in dict(origin_x=0,origin_y=0,font_size=48,
                frame_width=400,frame_height=200,tracking=0,line_spacing=0).items()})
    source_a=direction_source('direction-a-source','horizontal');source_b=direction_source('direction-b-source','vertical')
    ref_a=dict(object='direction-a',point='',field='text.direction');ref_b=dict(object='direction-b',point='',field='text.direction')
    steps=[
        ('create',dict(op='apply',expected_revision=0,commands=[dict(type='create_text',composition=composition_id,parent='',id='direction-a',name='Direction A',source=source_a),
            dict(type='create_text',composition=composition_id,parent='',id='direction-b',name='Direction B',source=source_b)])),
        ('resolve',dict(op='resolve_name',name='Direction B',point='',field='text.direction')),
        ('literal',dict(op='get',ref=ref_b)),
        ('properties_literal',dict(op='properties')),
        ('link',dict(op='apply',expected_revision=1,commands=[dict(type='link_text_direction',target=ref_b,source=ref_a,replace_driver=False)])),
        ('properties_linked',dict(op='properties')),
        ('horizontal_linked',dict(op='get',ref=ref_b)),
        ('horizontal_layout',dict(op='text_layout',object='direction-b')),
        ('horizontal_source_layout',dict(op='text_layout',object='direction-a')),
        ('source_update',dict(op='apply',expected_revision=2,commands=[dict(type='update_text',object='direction-a',source=dict(source_a,direction='vertical'))])),
        ('vertical_linked',dict(op='get',ref=ref_b)),
        ('vertical_layout',dict(op='text_layout',object='direction-b')),
        ('rejected_driven_edit',dict(op='apply',expected_revision=3,commands=[dict(type='update_text',object='direction-b',source=dict(source_b,direction='horizontal'))])),
        ('inspect_after_rejection',dict(op='inspect')),
        ('unlink',dict(op='apply',expected_revision=3,commands=[dict(type='unlink_text_direction',target=ref_b)])),
        ('frozen',dict(op='get',ref=ref_b)),
        ('frozen_layout',dict(op='text_layout',object='direction-b')),
        ('undo',dict(op='undo',expected_revision=4)),
        ('restored_link',dict(op='get',ref=ref_b)),
        ('source_edit',dict(op='apply',expected_revision=5,commands=[dict(type='update_text',object='direction-a',source=dict(source_a,direction='horizontal'))])),
        ('linked_horizontal',dict(op='get',ref=ref_b)),
        ('native',dict(op='inspect'))]
    requests=[request for _,request in steps]
    proc=subprocess.run([exe,'--serve',str(path)],input='\n'.join(map(json.dumps,requests))+'\n',
        capture_output=True,text=True,encoding='utf-8',timeout=20)
    replies=[json.loads(line) for line in proc.stdout.splitlines()]
    check(len(replies)==len(requests), 'JSON-lines returns one response per Text direction request')
    reply={name:replies[index] for index,(name,_) in enumerate(steps)}
    check(all(reply[name]['ok'] for name,_ in steps if name!='rejected_driven_edit'),
        'Text direction API create/discovery/link/update/layout/unlink/Undo requests succeed')
    check(reply['resolve']['result']==ref_b,'JSON-lines resolve_name returns the stable Text direction Ref')
    check(reply['literal']['result']['type']=='enum' and reply['literal']['result']['choices']==['horizontal','vertical'] and
        reply['literal']['result']['authored']==dict(literal='vertical',driver=None) and reply['literal']['result']['link'] is True,
        'JSON-lines Text direction read exposes its exact enum domain and authored literal')
    literal_metadata=next(value for value in reply['properties_literal']['result'] if value['ref']==ref_b)
    linked_metadata=next(value for value in reply['properties_linked']['result'] if value['ref']==ref_b)
    check(literal_metadata['authored']==dict(literal='vertical',driver=None) and literal_metadata['evaluated']=='vertical' and
        linked_metadata['authored']==dict(literal='vertical',driver=dict(link=ref_a)) and linked_metadata['evaluated']=='horizontal' and
        linked_metadata['type']=='enum' and linked_metadata['choices']==['horizontal','vertical'],
        'JSON-lines properties expose the literal before linking and preserve it while following its same-type source')
    check(reply['horizontal_linked']['result']['authored']==dict(literal='vertical',driver=dict(link=ref_a)) and
        reply['horizontal_linked']['result']['evaluated']=='horizontal' and reply['vertical_linked']['result']['evaluated']=='vertical',
        'JSON-lines get follows the source direction while preserving the target literal')
    layout_fields=('x','y','width','height','overflow','glyph_count','warnings','used_fonts')
    horizontal_layout=reply['horizontal_layout']['result'];source_layout=reply['horizontal_source_layout']['result']
    vertical_linked_layout=reply['vertical_layout']['result'];vertical_frozen_layout=reply['frozen_layout']['result']
    check(all(horizontal_layout[field]==source_layout[field] for field in layout_fields) and
        all(vertical_linked_layout[field]==vertical_frozen_layout[field] for field in layout_fields),
        'Text layout consumes the evaluated direction and unlink freezes identical geometry, warnings and used fonts')
    check(not reply['rejected_driven_edit']['ok'] and reply['rejected_driven_edit']['error']['code']=='DRIVEN_PROPERTY' and
        reply['rejected_driven_edit']['revision']==3 and
        next(obj for obj in reply['inspect_after_rejection']['result']['objects'] if obj['id']=='direction-b')['text']['direction_driver']==dict(link=ref_a),
        'A driven Text direction literal edit rejects without changing revision or the authored Ref')
    check(reply['frozen']['result']['authored']==dict(literal='vertical',driver=None) and
        reply['restored_link']['result']['authored']==dict(literal='vertical',driver=dict(link=ref_a)) and
        reply['restored_link']['result']['evaluated']=='vertical' and reply['linked_horizontal']['result']['evaluated']=='horizontal',
        'Unlink freezes Text direction, Undo restores its link, and later source edits still propagate')
    native=reply['native']['result'];check(native['version']=='0.50' and run('--validate',native).returncode==0,
        'Native 0.23 Text direction link validates in a separate CLI process')
    path.write_text(json.dumps(native),encoding='utf-8');before=path.read_bytes()
    cold=subprocess.run([exe,'--serve',str(path)],input=json.dumps(dict(op='get',ref=ref_b))+'\n'+
        json.dumps(dict(op='get',ref=ref_a))+'\n'+json.dumps(dict(op='inspect'))+'\n',
        capture_output=True,text=True,encoding='utf-8',timeout=20)
    cold_replies=[json.loads(line) for line in cold.stdout.splitlines()]
    cold_target=cold_replies[0]['result'];cold_source=cold_replies[1]['result']
    check(all(reply['ok'] for reply in cold_replies) and cold_target['authored']==dict(literal='vertical',driver=dict(link=ref_a)) and
        cold_target['evaluated']=='horizontal' and cold_source['evaluated']=='horizontal' and path.read_bytes()==before,
        'A separate JSON-lines cold open preserves direction literals, Ref and evaluation without changing native bytes')

# Text sizing layout links keep the target's frame dimensions and cold-open as authored state.
with tempfile.TemporaryDirectory() as tmp:
    path=Path(tmp)/'text-layout.nect';path.write_text(json.dumps(sample),encoding='utf-8')
    composition_id=sample['compositions'][0]['id']
    def layout_text(source_id,content,layout,frame_width=96,frame_height=48):
        return dict(id=source_id,version=1,content=content,family='Yu Gothic',locale='ja-JP',layout=layout,
            direction='horizontal',alignment='start',weight=400,italic=False,
            parameters={name:dict(literal=value) for name,value in dict(origin_x=0,origin_y=0,font_size=28,
                frame_width=frame_width,frame_height=frame_height,tracking=0,line_spacing=0).items()})
    source_a=layout_text('layout-a-source','Source sizing','auto')
    source_b=layout_text('layout-b-source','Target words that wrap within a fixed frame','frame')
    source_auto=layout_text('layout-auto-source','Target words that wrap within a fixed frame','auto')
    source_frame=layout_text('layout-frame-source','Target words that wrap within a fixed frame','frame')
    ref_a=dict(object='layout-a',point='',field='text.layout');ref_b=dict(object='layout-b',point='',field='text.layout')
    steps=[
        ('create',dict(op='apply',expected_revision=0,commands=[
            dict(type='create_text',composition=composition_id,parent='',id='layout-a',name='Layout A',source=source_a),
            dict(type='create_text',composition=composition_id,parent='',id='layout-b',name='Layout B',source=source_b),
            dict(type='create_text',composition=composition_id,parent='',id='layout-auto-twin',name='Auto twin',source=source_auto),
            dict(type='create_text',composition=composition_id,parent='',id='layout-frame-twin',name='Frame twin',source=source_frame)])),
        ('resolve',dict(op='resolve_name',name='Layout B',point='',field='text.layout')),
        ('literal',dict(op='get',ref=ref_b)),
        ('properties_literal',dict(op='properties')),
        ('link',dict(op='apply',expected_revision=1,commands=[dict(type='link_text_layout',target=ref_b,source=ref_a,replace_driver=False)])),
        ('properties_linked',dict(op='properties')),
        ('auto_linked',dict(op='get',ref=ref_b)),
        ('auto_layout',dict(op='text_layout',object='layout-b')),
        ('auto_twin_layout',dict(op='text_layout',object='layout-auto-twin')),
        ('frame_update',dict(op='apply',expected_revision=2,commands=[dict(type='update_text',object='layout-a',source=dict(source_a,layout='frame'))])),
        ('frame_linked',dict(op='get',ref=ref_b)),
        ('frame_layout',dict(op='text_layout',object='layout-b')),
        ('frame_twin_layout',dict(op='text_layout',object='layout-frame-twin')),
        ('rejected_driven_edit',dict(op='apply',expected_revision=3,commands=[dict(type='update_text',object='layout-b',source=dict(source_b,layout='auto'))])),
        ('inspect_after_rejection',dict(op='inspect')),
        ('unlink',dict(op='apply',expected_revision=3,commands=[dict(type='unlink_text_layout',target=ref_b)])),
        ('frozen',dict(op='get',ref=ref_b)),
        ('undo',dict(op='undo',expected_revision=4)),
        ('restored_link',dict(op='get',ref=ref_b)),
        ('source_update',dict(op='apply',expected_revision=5,commands=[dict(type='update_text',object='layout-a',source=dict(source_a,layout='auto'))])),
        ('linked_auto',dict(op='get',ref=ref_b)),
        ('native',dict(op='inspect'))]
    requests=[request for _,request in steps]
    proc=subprocess.run([exe,'--serve',str(path)],input='\n'.join(map(json.dumps,requests))+'\n',
        capture_output=True,text=True,encoding='utf-8',timeout=20)
    replies=[json.loads(line) for line in proc.stdout.splitlines()]
    check(len(replies)==len(requests),'JSON-lines returns one response per Text layout request')
    reply={name:replies[index] for index,(name,_) in enumerate(steps)}
    check(all(reply[name]['ok'] for name,_ in steps if name!='rejected_driven_edit'),
        'Text layout API create/discovery/link/update/unlink/Undo requests succeed')
    check(reply['resolve']['result']==ref_b,'JSON-lines resolve_name returns the stable Text layout Ref')
    check(reply['literal']['result']['type']=='enum' and reply['literal']['result']['choices']==['auto','frame'] and
        reply['literal']['result']['authored']==dict(literal='frame',driver=None) and reply['literal']['result']['link'] is True,
        'JSON-lines Text layout read exposes its exact enum domain and authored literal')
    literal_metadata=next(value for value in reply['properties_literal']['result'] if value['ref']==ref_b)
    linked_metadata=next(value for value in reply['properties_linked']['result'] if value['ref']==ref_b)
    check(literal_metadata['authored']==dict(literal='frame',driver=None) and literal_metadata['evaluated']=='frame' and
        linked_metadata['authored']==dict(literal='frame',driver=dict(link=ref_a)) and linked_metadata['evaluated']=='auto' and
        linked_metadata['type']=='enum' and linked_metadata['choices']==['auto','frame'],
        'JSON-lines properties preserve the Text layout literal while reporting its evaluated choice')
    check(reply['auto_linked']['result']['authored']==dict(literal='frame',driver=dict(link=ref_a)) and
        reply['auto_linked']['result']['evaluated']=='auto' and reply['frame_linked']['result']['evaluated']=='frame',
        'JSON-lines get follows Text layout source edits while preserving the target literal')
    layout_fields=('x','y','width','height','overflow','glyph_count','warnings','used_fonts')
    check(all(reply['auto_layout']['result'][field]==reply['auto_twin_layout']['result'][field] for field in layout_fields) and
        all(reply['frame_layout']['result'][field]==reply['frame_twin_layout']['result'][field] for field in layout_fields),
        'Text layout uses the evaluated enum and the target-owned frame geometry')
    width_ref=dict(object='layout-b',point='',field='text.frame_width')
    height_ref=dict(object='layout-b',point='',field='text.frame_height')
    check(not reply['rejected_driven_edit']['ok'] and reply['rejected_driven_edit']['error']['code']=='DRIVEN_PROPERTY' and
        reply['rejected_driven_edit']['revision']==3 and
        next(obj for obj in reply['inspect_after_rejection']['result']['objects'] if obj['id']=='layout-b')['text']['layout_driver']==dict(link=ref_a),
        'A driven Text layout literal edit rejects without changing revision or the authored Ref')
    check(reply['frozen']['result']['authored']==dict(literal='frame',driver=None) and
        reply['restored_link']['result']['authored']==dict(literal='frame',driver=dict(link=ref_a)) and
        reply['restored_link']['result']['evaluated']=='frame' and reply['linked_auto']['result']['evaluated']=='auto',
        'Unlink freezes Text layout, Undo restores its link, and later source edits still propagate')
    native=reply['native']['result'];check(native['version']=='0.50' and run('--validate',native).returncode==0,
        'Native 0.23 Text layout link validates in a separate CLI process')
    target_native=next(obj for obj in native['objects'] if obj['id']=='layout-b')['text']
    check(target_native['layout']=='frame' and target_native['layout_driver']==dict(link=ref_a) and
        target_native['parameters']['frame_width']['literal']==96 and target_native['parameters']['frame_height']['literal']==48,
        'Native layout save retains the target literal, Ref and target-owned frame dimensions')
    literal_native=json.loads(json.dumps(native));literal_native['version']='0.19'
    for obj in literal_native['objects']:
        if obj.get('kind')=='text':obj['text'].pop('layout_driver',None)
    check(run('--validate',literal_native).returncode==0,'Native 0.19 Text layout literals migrate as literal-only state')
    old_driver=json.loads(json.dumps(native));old_driver['version']='0.19'
    check('UNSUPPORTED_TEXT_LAYOUT_DRIVER' in run('--validate',old_driver).stderr,
        'Native 0.19 rejects a Text layout driver field')
    path.write_text(json.dumps(native),encoding='utf-8');before=path.read_bytes()
    cold=subprocess.run([exe,'--serve',str(path)],input=json.dumps(dict(op='get',ref=ref_b))+'\n'+
        json.dumps(dict(op='get',ref=width_ref))+'\n'+json.dumps(dict(op='get',ref=height_ref))+'\n'+json.dumps(dict(op='inspect'))+'\n',
        capture_output=True,text=True,encoding='utf-8',timeout=20)
    cold_replies=[json.loads(line) for line in cold.stdout.splitlines()]
    cold_target=cold_replies[0]['result'];cold_doc=cold_replies[3]['result']
    check(all(value['ok'] for value in cold_replies) and cold_target['authored']==dict(literal='frame',driver=dict(link=ref_a)) and
        cold_target['evaluated']=='auto' and cold_replies[1]['result']['evaluated']==96 and cold_replies[2]['result']['evaluated']==48 and
        next(obj for obj in cold_doc['objects'] if obj['id']=='layout-b')['text']['layout_driver']==dict(link=ref_a) and path.read_bytes()==before,
        'A separate JSON-lines cold open preserves layout, Ref and target frame dimensions without changing native bytes')

# Text alignment uses its own same-field enum link and native 0.21 driver.
alignment_schema=json.loads((polystar_path.parent.parent/'schemas/native-v0.21.schema.json').read_text())
alignment_field=alignment_schema['$defs']['text_source']['properties']['alignment_driver']
alignment_driver=alignment_schema['$defs']['alignment_driver'];alignment_ref=alignment_schema['$defs']['alignment_ref']
previous_alignment_schema=json.loads((polystar_path.parent.parent/'schemas/native-v0.20.schema.json').read_text())
check(alignment_schema['properties']['version']['const']=='0.21' and
      'alignment_driver' not in previous_alignment_schema['$defs']['text_source']['properties'] and
      alignment_field['$ref']=='#/$defs/alignment_driver' and alignment_driver['additionalProperties'] is False and
      alignment_driver['required']==['link'] and alignment_ref['properties']['point']['const']=='' and
      alignment_ref['properties']['field']['const']=='text.alignment',
      'Native 0.21 adds only the closed same-field Text alignment Ref driver')
with tempfile.TemporaryDirectory() as tmp:
    path=Path(tmp)/'text-alignment.nect';path.write_text(json.dumps(sample),encoding='utf-8')
    composition_id=sample['compositions'][0]['id']
    def alignment_text(source_id,content,alignment):
        return dict(id=source_id,version=1,content=content,family='Yu Gothic',locale='ja-JP',layout='frame',
            direction='horizontal',alignment=alignment,weight=400,italic=False,
            parameters={name:dict(literal=value) for name,value in dict(origin_x=0,origin_y=0,font_size=28,
                frame_width=240,frame_height=80,tracking=0,line_spacing=0).items()})
    source_a=alignment_text('alignment-a-source','Alignment text','start')
    source_b=alignment_text('alignment-b-source','Alignment text','end')
    ref_a=dict(object='alignment-a',point='',field='text.alignment')
    ref_b=dict(object='alignment-b',point='',field='text.alignment')
    requests=[
        ('create',dict(op='apply',expected_revision=0,commands=[
            dict(type='create_text',composition=composition_id,parent='',id='alignment-a',name='Alignment A',source=source_a),
            dict(type='create_text',composition=composition_id,parent='',id='alignment-b',name='Alignment B',source=source_b)])),
        ('resolve',dict(op='resolve_name',name='Alignment B',point='',field='text.alignment')),
        ('literal',dict(op='get',ref=ref_b)),('properties',dict(op='properties')),
        ('link',dict(op='apply',expected_revision=1,commands=[dict(type='link_text_alignment',target=ref_b,source=ref_a,replace_driver=False)])),
        ('linked_start',dict(op='get',ref=ref_b)),
        ('source_center',dict(op='apply',expected_revision=2,commands=[dict(type='update_text',object='alignment-a',source=dict(source_a,alignment='center'))])),
        ('linked_center',dict(op='get',ref=ref_b)),
        ('rejected_driven_edit',dict(op='apply',expected_revision=3,commands=[dict(type='update_text',object='alignment-b',source=dict(source_b,alignment='start'))])),
        ('unlink',dict(op='apply',expected_revision=3,commands=[dict(type='unlink_text_alignment',target=ref_b)])),
        ('frozen_center',dict(op='get',ref=ref_b)),('undo',dict(op='undo',expected_revision=4)),
        ('restored_link',dict(op='get',ref=ref_b)),
        ('source_end',dict(op='apply',expected_revision=5,commands=[dict(type='update_text',object='alignment-a',source=dict(source_a,alignment='end'))])),
        ('native',dict(op='inspect'))]
    proc=subprocess.run([exe,'--serve',str(path)],input='\n'.join(json.dumps(request) for _,request in requests)+'\n',
        capture_output=True,text=True,encoding='utf-8',timeout=20)
    replies=[json.loads(line) for line in proc.stdout.splitlines()]
    check(len(replies)==len(requests),'Text alignment JSON-lines returns one response per request')
    reply={name:replies[index] for index,(name,_) in enumerate(requests)}
    check(all(reply[name]['ok'] for name,_ in requests if name!='rejected_driven_edit'),
        'Text alignment API create/discovery/link/update/unlink/Undo requests succeed')
    check(reply['resolve']['result']==ref_b,'resolve_name returns the stable Text alignment Ref')
    check(reply['literal']['result']['choices']==['start','center','end'] and
        reply['literal']['result']['authored']==dict(literal='end',driver=None) and reply['literal']['result']['evaluated']=='end',
        'Text alignment get exposes its closed enum and authored literal')
    metadata=next(value for value in reply['properties']['result'] if value['ref']==ref_b)
    linked_metadata=next(value for value in reply['native']['result']['objects'] if value['id']=='alignment-b')['text']
    check(metadata['type']=='enum' and metadata['authored']==dict(literal='end',driver=None) and metadata['evaluated']=='end' and
        reply['linked_start']['result']['authored']==dict(literal='end',driver=dict(link=ref_a)) and
        reply['linked_start']['result']['evaluated']=='start' and reply['linked_center']['result']['evaluated']=='center',
        'Text alignment properties retain target authorship while following same-field source edits')
    check(not reply['rejected_driven_edit']['ok'] and reply['rejected_driven_edit']['error']['code']=='DRIVEN_PROPERTY' and
        reply['rejected_driven_edit']['revision']==3 and reply['frozen_center']['result']['authored']==dict(literal='center',driver=None) and
        reply['restored_link']['result']['authored']==dict(literal='end',driver=dict(link=ref_a)) and
        reply['restored_link']['result']['evaluated']=='center',
        'A driven alignment edit rejects atomically; unlink freezes and Undo restores its driver')
    native=reply['native']['result'];check(native['version']=='0.50' and run('--validate',native).returncode==0,
        'Native 0.23 Text alignment link validates in a separate CLI process')
    check(linked_metadata['alignment']=='end' and linked_metadata['alignment_driver']==dict(link=ref_a),
        'Native 0.23 retains alignment literal separately from its stable Ref')
    legacy=json.loads(json.dumps(native));legacy['version']='0.20'
    target=next(obj for obj in legacy['objects'] if obj['id']=='alignment-b')['text'];target.pop('alignment_driver')
    check(run('--validate',legacy).returncode==0 and
        'UNSUPPORTED_TEXT_ALIGNMENT_DRIVER' in run('--validate',dict(native,version='0.20')).stderr,
        'Native 0.20 preserves literal alignment and rejects the 0.21 driver field')
    malformed=json.loads(json.dumps(native));next(obj for obj in malformed['objects'] if obj['id']=='alignment-b')['text']['alignment_driver']={'other':ref_a}
    check('INVALID_TEXT_ALIGNMENT_DRIVER' in run('--validate',malformed).stderr,
        'Malformed native Text alignment drivers reject instead of being dropped')
    path.write_text(json.dumps(native),encoding='utf-8');before=path.read_bytes()
    cold=subprocess.run([exe,'--serve',str(path)],input=json.dumps(dict(op='get',ref=ref_b))+'\n'+json.dumps(dict(op='inspect'))+'\n',
        capture_output=True,text=True,encoding='utf-8',timeout=20)
    cold_replies=[json.loads(line) for line in cold.stdout.splitlines()]
    cold_target=cold_replies[0]['result'];cold_doc=cold_replies[1]['result']
    check(all(value['ok'] for value in cold_replies) and cold_target['authored']==dict(literal='end',driver=dict(link=ref_a)) and
        cold_target['evaluated']=='end' and next(obj for obj in cold_doc['objects'] if obj['id']=='alignment-b')['text']['alignment_driver']==dict(link=ref_a) and
        path.read_bytes()==before,
        'A separate JSON-lines cold open preserves Text alignment source, Ref, evaluation and exact native bytes')

# Text locale links preserve their unnormalized literal and use the same platform layout path.
locale_schema=json.loads((polystar_path.parent.parent/'schemas/native-v0.22.schema.json').read_text())
locale_field=locale_schema['$defs']['text_source']['properties']['locale_driver']
locale_driver=locale_schema['$defs']['locale_driver'];locale_ref=locale_schema['$defs']['locale_ref']
previous_locale_schema=json.loads((polystar_path.parent.parent/'schemas/native-v0.21.schema.json').read_text())
check(locale_schema['properties']['version']['const']=='0.22' and
      'locale_driver' not in previous_locale_schema['$defs']['text_source']['properties'] and
      locale_field['$ref']=='#/$defs/locale_driver' and locale_driver['additionalProperties'] is False and
      locale_driver['required']==['link'] and locale_ref['properties']['point']['const']=='' and
      locale_ref['properties']['field']['const']=='text.locale',
      'Native 0.22 adds only the closed same-field Text locale Ref driver')
with tempfile.TemporaryDirectory() as tmp:
    path=Path(tmp)/'text-locale.nect';path.write_text(json.dumps(sample),encoding='utf-8')
    composition_id=sample['compositions'][0]['id']
    def locale_text(source_id,content,locale):
        return dict(id=source_id,version=1,content=content,family='Yu Gothic',locale=locale,layout='frame',
            direction='horizontal',alignment='start',weight=400,italic=False,
            parameters={name:dict(literal=value) for name,value in dict(origin_x=0,origin_y=0,font_size=28,
                frame_width=180,frame_height=72,tracking=0,line_spacing=0).items()})
    source_a=locale_text('locale-a-source','Source locale','ar-SA')
    source_b=locale_text('locale-b-source','سلام','ja-JP')
    twin_arabic=locale_text('locale-arabic-twin-source','سلام','ar-SA')
    twin_japanese=locale_text('locale-japanese-twin-source','سلام','ja-JP')
    ref_a=dict(object='locale-a',point='',field='text.locale')
    ref_b=dict(object='locale-b',point='',field='text.locale')
    requests=[
        ('create',dict(op='apply',expected_revision=0,commands=[
            dict(type='create_text',composition=composition_id,parent='',id='locale-a',name='Locale A',source=source_a),
            dict(type='create_text',composition=composition_id,parent='',id='locale-b',name='Locale B',source=source_b),
            dict(type='create_text',composition=composition_id,parent='',id='locale-arabic-twin',name='Arabic twin',source=twin_arabic),
            dict(type='create_text',composition=composition_id,parent='',id='locale-japanese-twin',name='Japanese twin',source=twin_japanese)])),
        ('resolve',dict(op='resolve_name',name='Locale B',point='',field='text.locale')),
        ('literal',dict(op='get',ref=ref_b)),('properties',dict(op='properties')),
        ('link',dict(op='apply',expected_revision=1,commands=[dict(type='link_text_locale',target=ref_b,source=ref_a,replace_driver=False)])),
        ('linked',dict(op='get',ref=ref_b)),('linked_layout',dict(op='text_layout',object='locale-b')),
        ('arabic_twin_layout',dict(op='text_layout',object='locale-arabic-twin')),
        ('source_ja',dict(op='apply',expected_revision=2,commands=[dict(type='update_text',object='locale-a',source=dict(source_a,locale='ja-JP'))])),
        ('linked_ja',dict(op='get',ref=ref_b)),('linked_ja_layout',dict(op='text_layout',object='locale-b')),
        ('japanese_twin_layout',dict(op='text_layout',object='locale-japanese-twin')),
        ('rejected_edit',dict(op='apply',expected_revision=3,commands=[dict(type='update_text',object='locale-b',source=dict(source_b,locale='fr-FR'))])),
        ('unlink',dict(op='apply',expected_revision=3,commands=[dict(type='unlink_text_locale',target=ref_b)])),
        ('frozen',dict(op='get',ref=ref_b)),('undo',dict(op='undo',expected_revision=4)),
        ('restored',dict(op='get',ref=ref_b)),('native',dict(op='inspect'))]
    proc=subprocess.run([exe,'--serve',str(path)],input='\n'.join(json.dumps(request) for _,request in requests)+'\n',
        capture_output=True,text=True,encoding='utf-8',timeout=20)
    replies=[json.loads(line) for line in proc.stdout.splitlines()]
    check(len(replies)==len(requests),'Text locale JSON-lines returns one response per request')
    reply={name:replies[index] for index,(name,_) in enumerate(requests)}
    check(all(reply[name]['ok'] for name,_ in requests if name!='rejected_edit'),
        'Text locale API create/discovery/link/update/unlink/Undo requests succeed')
    check(reply['resolve']['result']==ref_b,'resolve_name returns the stable Text locale Ref')
    check(reply['literal']['result']['type']=='string' and
        reply['literal']['result']['authored']==dict(literal='ja-JP',driver=None) and
        reply['literal']['result']['evaluated']=='ja-JP' and reply['literal']['result']['link'] is True,
        'Text locale get exposes its literal, evaluated value and link capability')
    metadata=next(value for value in reply['properties']['result'] if value['ref']==ref_b)
    check(metadata==reply['literal']['result'] and
        reply['linked']['result']['authored']==dict(literal='ja-JP',driver=dict(link=ref_a)) and
        reply['linked']['result']['evaluated']=='ar-SA' and
        reply['linked_ja']['result']['authored']==dict(literal='ja-JP',driver=dict(link=ref_a)) and
        reply['linked_ja']['result']['evaluated']=='ja-JP',
        'Text locale discovery retains authored bytes while evaluating the source locale dynamically')
    def same_layout(left,right):
        return all(left[key]==right[key] for key in ('x','y','width','height','overflow','glyph_count','warnings','used_fonts'))
    check(same_layout(reply['linked_layout']['result'],reply['arabic_twin_layout']['result']) and
        same_layout(reply['linked_ja_layout']['result'],reply['japanese_twin_layout']['result']),
        'Linked Arabic Text layout, used fonts and warnings equal same-document literal twins')
    check(not reply['rejected_edit']['ok'] and reply['rejected_edit']['error']['code']=='DRIVEN_PROPERTY' and
        reply['rejected_edit']['revision']==3 and reply['frozen']['result']['authored']==dict(literal='ja-JP',driver=None) and
        reply['restored']['result']['authored']==dict(literal='ja-JP',driver=dict(link=ref_a)) and
        reply['restored']['result']['evaluated']=='ja-JP',
        'Driven Text locale edit rejects atomically; unlink freezes and Undo restores its driver')
    native=reply['native']['result'];check(native['version']=='0.50' and run('--validate',native).returncode==0,
        'Native 0.23 Text locale link validates in a separate CLI process')
    linked_metadata=next(obj for obj in native['objects'] if obj['id']=='locale-b')['text']
    check(linked_metadata['locale']=='ja-JP' and linked_metadata['locale_driver']==dict(link=ref_a),
        'Native 0.23 retains the target locale literal separately from its stable Ref')
    legacy=json.loads(json.dumps(native));legacy['version']='0.21'
    legacy_target=next(obj for obj in legacy['objects'] if obj['id']=='locale-b')['text'];legacy_target.pop('locale_driver')
    check(run('--validate',legacy).returncode==0 and
        'UNSUPPORTED_TEXT_LOCALE_DRIVER' in run('--validate',dict(native,version='0.21')).stderr,
        'Native 0.21 preserves locale literals and rejects a falsely versioned driver')
    malformed=json.loads(json.dumps(native));next(obj for obj in malformed['objects'] if obj['id']=='locale-b')['text']['locale_driver']={'other':ref_a}
    check('INVALID_TEXT_LOCALE_DRIVER' in run('--validate',malformed).stderr,
        'Malformed native Text locale drivers reject instead of being dropped')
    path.write_text(json.dumps(native),encoding='utf-8');before=path.read_bytes()
    cold=subprocess.run([exe,'--serve',str(path)],input=json.dumps(dict(op='get',ref=ref_b))+'\n'+
        json.dumps(dict(op='text_layout',object='locale-b'))+'\n'+json.dumps(dict(op='text_layout',object='locale-japanese-twin'))+'\n'+
        json.dumps(dict(op='inspect'))+'\n',capture_output=True,text=True,encoding='utf-8',timeout=20)
    cold_replies=[json.loads(line) for line in cold.stdout.splitlines()]
    cold_target=cold_replies[0]['result'];cold_doc=cold_replies[3]['result']
    check(all(value['ok'] for value in cold_replies) and cold_target['authored']==dict(literal='ja-JP',driver=dict(link=ref_a)) and
        cold_target['evaluated']=='ja-JP' and same_layout(json.loads(cold.stdout.splitlines()[1])['result'],
        json.loads(cold.stdout.splitlines()[2])['result']) and
        next(obj for obj in cold_doc['objects'] if obj['id']=='locale-b')['text']['locale_driver']==dict(link=ref_a) and
        path.read_bytes()==before,
        'A separate JSON-lines cold open preserves locale source, Ref, evaluation, layout and exact native bytes')

# Fill rule uses a stable same-field operation Ref through the process Session.
with tempfile.TemporaryDirectory() as tmp:
    path=Path(tmp)/'fill-rule.nect';path.write_text(json.dumps(sample),encoding='utf-8')
    objects={obj['id']:obj for obj in sample['objects']}
    source_name=objects['path-A']['name'];target_name=objects['path-B']['name']
    source_ref=dict(object='path-A',point='',field='op.source-fill.fill_rule')
    target_ref=dict(object='path-B',point='',field='op.target-fill.fill_rule')
    def fill_operation(id_,rule):
        return dict(id=id_,type='nect.paint.fill',version=1,enabled=True,
            parameters={key:dict(literal=value) for key,value in dict(r=0,g=0,b=0,a=1).items()},
            composite='below',fill_rule=rule)
    steps=[
        ('add',dict(op='apply',expected_revision=0,commands=[
            dict(type='add_operation',object='path-A',index=1,operation=fill_operation('source-fill','evenodd')),
            dict(type='add_operation',object='path-B',index=1,operation=fill_operation('target-fill','nonzero'))])),
        ('resolve',dict(op='resolve_name',name=target_name,point='',field=target_ref['field'])),
        ('literal',dict(op='get',ref=target_ref)),
        ('properties_literal',dict(op='properties')),
        ('link',dict(op='apply',expected_revision=1,commands=[dict(type='link_fill_rule',target=target_ref,source=source_ref,replace_driver=False)])),
        ('linked',dict(op='get',ref=target_ref)),
        ('properties_linked',dict(op='properties')),
        ('source_edit',dict(op='apply',expected_revision=2,commands=[dict(type='operation_options',object='path-A',operation='source-fill',composite='below',fill_rule='nonzero')])),
        ('updated',dict(op='get',ref=target_ref)),
        ('render_plan',dict(op='render_plan',object='path-B')),
        ('svg',dict(op='export_svg',composition=sample['compositions'][0]['id'],artboard=sample['compositions'][0]['artboards'][0]['id'])),
        ('bad_batch',dict(op='apply',expected_revision=3,commands=[
            dict(type='operation_options',object='path-B',operation='target-fill',composite='above',fill_rule='nonzero'),
            dict(type='link_fill_rule',target=target_ref,source=target_ref,replace_driver=False)])),
        ('driven_edit',dict(op='apply',expected_revision=3,commands=[dict(type='operation_options',object='path-B',operation='target-fill',composite='below',fill_rule='evenodd')])),
        ('unlink',dict(op='apply',expected_revision=3,commands=[dict(type='unlink_fill_rule',target=target_ref)])),
        ('frozen',dict(op='get',ref=target_ref)),
        ('undo',dict(op='undo',expected_revision=4)),
        ('restored',dict(op='get',ref=target_ref)),
        ('source_again',dict(op='apply',expected_revision=5,commands=[dict(type='operation_options',object='path-A',operation='source-fill',composite='below',fill_rule='evenodd')])),
        ('follows',dict(op='get',ref=target_ref)),
        ('native',dict(op='inspect')),
    ]
    proc=subprocess.run([exe,'--serve',str(path)],input='\n'.join(json.dumps(request) for _,request in steps)+'\n',
        capture_output=True,text=True,encoding='utf-8',timeout=20)
    replies=[json.loads(line) for line in proc.stdout.splitlines()]
    check(proc.returncode==0 and len(replies)==len(steps),'Fill JSON-lines process returns one response per request')
    reply={name:replies[index] for index,(name,_) in enumerate(steps)}
    check(reply['add']['ok'] and reply['link']['ok'] and reply['source_edit']['ok'] and reply['unlink']['ok'] and reply['undo']['ok'] and reply['source_again']['ok'],
        'Fill JSON-lines add/link/update/unlink/Undo commands use the Session path')
    check(reply['resolve']['result']==target_ref,'Fill resolve_name returns the stable operation Ref')
    check(reply['literal']['result']['type']=='enum' and reply['literal']['result']['choices']==['nonzero','evenodd'] and
        reply['literal']['result']['authored']==dict(literal='nonzero',driver=None) and reply['literal']['result']['evaluated']=='nonzero',
        'Fill get exposes the exact enum domain and authored literal')
    literal_metadata=next(value for value in reply['properties_literal']['result'] if value['ref']==target_ref)
    linked_metadata=next(value for value in reply['properties_linked']['result'] if value['ref']==target_ref)
    check(literal_metadata['authored']==dict(literal='nonzero',driver=None) and
        linked_metadata['authored']==dict(literal='nonzero',driver=dict(link=source_ref)) and
        linked_metadata['evaluated']=='evenodd' and linked_metadata['link'] is True and linked_metadata['expression'] is False,
        'Fill properties retain the target literal and report the same-field evaluated source')
    check(reply['updated']['result']['authored']==dict(literal='nonzero',driver=dict(link=source_ref)) and
        reply['updated']['result']['evaluated']=='nonzero' and
        next(layer for layer in reply['render_plan']['result']['paint_layers'] if layer['operation']=='target-fill')['fill_rule']=='nonzero' and
        'fill-rule="nonzero"' in reply['svg']['result'],
        'Source edits flow through Fill get, render_plan and SVG')
    check(not reply['bad_batch']['ok'] and reply['bad_batch']['error']['code']=='DEPENDENCY_CYCLE' and reply['bad_batch']['revision']==3 and
        not reply['driven_edit']['ok'] and reply['driven_edit']['error']['code']=='DRIVEN_PROPERTY' and reply['driven_edit']['revision']==3,
        'Invalid second batch command and implicit driven edit reject without advancing revision')
    check(reply['frozen']['result']['authored']==dict(literal='nonzero',driver=None) and
        reply['restored']['result']['authored']==dict(literal='nonzero',driver=dict(link=source_ref)) and
        reply['follows']['result']['evaluated']=='evenodd',
        'Fill unlink freezes the choice and Undo restores the live dependency')
    native=reply['native']['result'];check(native['version']=='0.50' and run('--validate',native).returncode==0,
        'Native 0.36 Fill driver validates in a separate CLI process')
    native_target=next(obj for obj in native['objects'] if obj['id']=='path-B')['stack'][-1]
    check(native_target['fill_rule']=='nonzero' and native_target['fill_rule_driver']==dict(link=source_ref),
        'Native Fill keeps the authored literal beside its stable driver')
    false_version=json.loads(json.dumps(native));false_version['version']='0.25'
    check('UNSUPPORTED_FILL_RULE_DRIVER' in run('--validate',false_version).stderr,
        'Native 0.25 rejects a falsely versioned Fill driver')
    malformed=json.loads(json.dumps(native));next(obj for obj in malformed['objects'] if obj['id']=='path-B')['stack'][-1]['fill_rule_driver']={'other':source_ref}
    check('INVALID_FILL_RULE_DRIVER' in run('--validate',malformed).stderr,
        'Malformed native Fill driver rejects instead of being dropped')
    path.write_text(json.dumps(native),encoding='utf-8');before=path.read_bytes()
    cold=subprocess.run([exe,'--serve',str(path)],input=json.dumps(dict(op='get',ref=target_ref))+'\n'+
        json.dumps(dict(op='properties'))+'\n'+json.dumps(dict(op='inspect'))+'\n',
        capture_output=True,text=True,encoding='utf-8',timeout=20)
    cold_replies=[json.loads(line) for line in cold.stdout.splitlines()]
    cold_get=cold_replies[0]['result'];cold_doc=cold_replies[2]['result']
    check(all(value['ok'] for value in cold_replies) and cold_get['authored']==dict(literal='nonzero',driver=dict(link=source_ref)) and
        cold_get['evaluated']=='evenodd' and next(obj for obj in cold_doc['objects'] if obj['id']=='path-B')['stack'][-1]['fill_rule_driver']==dict(link=source_ref) and
        path.read_bytes()==before,
        'A distinct JSON-lines cold open preserves Fill literal, stable Ref, evaluated value and exact native bytes')

# Operation enabled links drive every paint consumer through the shared Session.
with tempfile.TemporaryDirectory() as tmp:
    path=Path(tmp)/'operation-enabled.nect';path.write_text(json.dumps(sample),encoding='utf-8')
    source_ref=dict(object='path-A',point='',field='op.path-A-stroke.enabled')
    target_ref=dict(object='path-B',point='',field='op.path-B-stroke.enabled')
    target_name=next(obj['name'] for obj in sample['objects'] if obj['id']=='path-B')
    steps=[
        ('initial',dict(op='apply',expected_revision=0,commands=[
            dict(type='enable_operation',object='path-A',operation='path-A-stroke',enabled=False),
            dict(type='link_operation_enabled',target=target_ref,source=source_ref,replace_driver=False)])),
        ('resolve',dict(op='resolve_name',name=target_name,point='',field=target_ref['field'])),
        ('linked',dict(op='get',ref=target_ref)),
        ('properties',dict(op='properties')),
        ('render_plan_disabled',dict(op='render_plan',object='path-B')),
        ('svg_disabled',dict(op='export_svg',composition=sample['compositions'][0]['id'],artboard=sample['compositions'][0]['artboards'][0]['id'])),
        ('source_enable',dict(op='apply',expected_revision=1,commands=[dict(type='enable_operation',object='path-A',operation='path-A-stroke',enabled=True)])),
        ('linked_enabled',dict(op='get',ref=target_ref)),
        ('render_plan_enabled',dict(op='render_plan',object='path-B')),
        ('svg_enabled',dict(op='export_svg',composition=sample['compositions'][0]['id'],artboard=sample['compositions'][0]['artboards'][0]['id'])),
        ('driven_edit',dict(op='apply',expected_revision=2,commands=[dict(type='enable_operation',object='path-B',operation='path-B-stroke',enabled=False)])),
        ('native',dict(op='inspect')),
    ]
    proc=subprocess.run([exe,'--serve',str(path)],input='\n'.join(json.dumps(request) for _,request in steps)+'\n',
        capture_output=True,text=True,encoding='utf-8',timeout=20)
    replies=[json.loads(line) for line in proc.stdout.splitlines()]
    check(proc.returncode==0 and len(replies)==len(steps),'Operation enabled JSON-lines process returns one response per request')
    reply={name:replies[index] for index,(name,_) in enumerate(steps)}
    check(reply['initial']['ok'] and reply['source_enable']['ok'] and reply['resolve']['result']==target_ref,
        'Operation enabled links use stable name resolution and shared Session commands')
    linked=reply['linked']['result']
    metadata=next(value for value in reply['properties']['result'] if value['ref']==target_ref)
    check(linked['type']=='bool' and linked['unit']=='boolean' and
        linked['authored']==dict(literal=True,driver=dict(link=source_ref)) and linked['evaluated'] is False and
        linked['link'] is True and linked['expression'] is False and metadata==linked,
        'Typed get and properties retain the literal, exact source Ref, and evaluated boolean')
    check(reply['render_plan_disabled']['result']['paint_layers']==[] and
        len(ET.fromstring(reply['svg_disabled']['result']).findall('.//{http://www.w3.org/2000/svg}path'))==0,
        'A disabled driver bypasses the target shape paint in the render plan and SVG')
    updated=reply['linked_enabled']['result']
    check(updated['authored']==dict(literal=True,driver=dict(link=source_ref)) and updated['evaluated'] is True and
        [layer['operation'] for layer in reply['render_plan_enabled']['result']['paint_layers']]==['path-B-stroke'] and
        len(ET.fromstring(reply['svg_enabled']['result']).findall('.//{http://www.w3.org/2000/svg}path'))==2,
        'The source boolean updates operation evaluation, paint planning, and SVG projection together')
    check(not reply['driven_edit']['ok'] and reply['driven_edit']['error']['code']=='DRIVEN_PROPERTY' and
        reply['driven_edit']['revision']==2,
        'EnableOperation refuses a linked enabled target without changing its revision')
    native=reply['native']['result']
    check(native['version']=='0.50' and
        next(obj for obj in native['objects'] if obj['id']=='path-B')['stack'][0]['enabled_driver']==dict(link=source_ref) and
        run('--validate',native).returncode==0,
        'Native 0.36 preserves and validates the authored operation enabled source Ref')
    old=json.loads(json.dumps(native));old['version']='0.27'
    check('UNSUPPORTED_OPERATION_ENABLED_DRIVER' in run('--validate',old).stderr,
        'Native 0.27 rejects operation enabled driver smuggling')
    malformed=json.loads(json.dumps(native));next(obj for obj in malformed['objects'] if obj['id']=='path-B')['stack'][0]['enabled_driver']={'link':source_ref,'extra':True}
    check('UNKNOWN_FIELD' in run('--validate',malformed).stderr,
        'Native 0.36 rejects unknown operation enabled driver wrapper fields')
    path.write_text(json.dumps(native),encoding='utf-8');before=path.read_bytes()
    cold=subprocess.run([exe,'--serve',str(path)],input=json.dumps(dict(op='get',ref=target_ref))+'\n'+
        json.dumps(dict(op='properties'))+'\n'+json.dumps(dict(op='inspect'))+'\n',
        capture_output=True,text=True,encoding='utf-8',timeout=20)
    cold_replies=[json.loads(line) for line in cold.stdout.splitlines()]
    cold_get=cold_replies[0]['result'];cold_doc=cold_replies[2]['result']
    check(all(value['ok'] for value in cold_replies) and cold_get==updated and
        next(value for value in cold_replies[1]['result'] if value['ref']==target_ref)==cold_get and
        next(obj for obj in cold_doc['objects'] if obj['id']=='path-B')['stack'][0]['enabled_driver']==dict(link=source_ref) and
        path.read_bytes()==before,
        'A distinct JSON-lines cold open preserves the typed link, evaluated value, exact Ref and native bytes')

# Gradient bypass links are native v0.29 properties distinct from operation enabled.
def gradient(id,enabled):
    return dict(id=id,type='linear',version=1,enabled=enabled,
        start_x=dict(literal=0),start_y=dict(literal=0),end_x=dict(literal=200),end_y=dict(literal=0),
        stops=[dict(id=id+'-start',offset=dict(literal=0),rgba=[dict(literal=1),dict(literal=0),dict(literal=0),dict(literal=1)]),
               dict(id=id+'-end',offset=dict(literal=1),rgba=[dict(literal=0),dict(literal=0),dict(literal=1),dict(literal=1)])])

with tempfile.TemporaryDirectory() as tmp:
    path=Path(tmp)/'gradient-enabled.nect';path.write_text(json.dumps(sample),encoding='utf-8')
    source_ref=dict(object='path-A',point='',field='op.path-A-stroke.gradient.source-gradient.enabled')
    target_ref=dict(object='path-B',point='',field='op.path-B-stroke.gradient.target-gradient.enabled')
    target_stop=dict(object='path-B',point='',field='op.path-B-stroke.gradient.target-gradient.stop.target-gradient-start.color')
    steps=[
        ('setup',dict(op='apply',expected_revision=0,commands=[
            dict(type='set_gradient',object='path-A',operation='path-A-stroke',gradient=gradient('source-gradient',True)),
            dict(type='set_gradient',object='path-B',operation='path-B-stroke',gradient=gradient('target-gradient',False)),
            dict(type='enable_operation',object='path-A',operation='path-A-stroke',enabled=False)])),
        ('link',dict(op='apply',expected_revision=1,commands=[dict(type='link_gradient_enabled',target=target_ref,source=source_ref,replace_driver=False)])),
        ('get',dict(op='get',ref=target_ref)),('properties',dict(op='properties')),
        ('render_active',dict(op='render_plan',object='path-B')),('source_render',dict(op='render_plan',object='path-A')),
        ('colors_active',dict(op='used_colors')),
        ('svg_active',dict(op='export_svg',composition=sample['compositions'][0]['id'],artboard=sample['compositions'][0]['artboards'][0]['id'])),
        ('direct_toggle',dict(op='apply',expected_revision=2,commands=[
            dict(type='set_gradient',object='path-B',operation='path-B-stroke',gradient=gradient('target-gradient',True))])),
        ('same_id_edit',dict(op='apply',expected_revision=2,commands=[dict(type='set_gradient',object='path-B',operation='path-B-stroke',gradient=dict(
            gradient('target-gradient',False),end_x=dict(literal=280)))])),
        ('preserved',dict(op='get',ref=target_ref)),
        ('source_disable',dict(op='apply',expected_revision=3,commands=[dict(type='set_gradient',object='path-A',operation='path-A-stroke',gradient=gradient('source-gradient',False))])),
        ('get_disabled',dict(op='get',ref=target_ref)),('render_disabled',dict(op='render_plan',object='path-B')),
        ('colors_disabled',dict(op='used_colors')),
        ('svg_disabled',dict(op='export_svg',composition=sample['compositions'][0]['id'],artboard=sample['compositions'][0]['artboards'][0]['id'])),
        ('source_enable',dict(op='apply',expected_revision=4,commands=[dict(type='set_gradient',object='path-A',operation='path-A-stroke',gradient=gradient('source-gradient',True))])),
        ('get_enabled',dict(op='get',ref=target_ref)),
        ('delete_source',dict(op='apply',expected_revision=5,commands=[dict(type='delete_objects',objects=['path-A'])])),
        ('native',dict(op='inspect')),
    ]
    proc=subprocess.run([exe,'--serve',str(path)],input='\n'.join(json.dumps(request) for _,request in steps)+'\n',
        capture_output=True,text=True,encoding='utf-8',timeout=25)
    replies=[json.loads(line) for line in proc.stdout.splitlines()]
    check(proc.returncode==0 and len(replies)==len(steps),'Gradient enabled JSON-lines commands return one response per request')
    reply={name:replies[index] for index,(name,_) in enumerate(steps)}
    check(reply['setup']['ok'] and reply['link']['ok'] and reply['get']['result']['authored']==dict(literal=False,driver=dict(link=source_ref)) and
        reply['get']['result']['evaluated'] is True and next(value for value in reply['properties']['result'] if value['ref']==target_ref)==reply['get']['result'],
        'JSON-lines link and typed properties preserve the false target literal and exact source Ref')
    active_layer=next(layer for layer in reply['render_active']['result']['paint_layers'] if layer['operation']=='path-B-stroke')
    active_uses=[ref for color in reply['colors_active']['result']['colors'] for ref in color['uses']]
    check(active_layer.get('gradient') is not None and reply['source_render']['result']['paint_layers']==[] and target_stop in active_uses and
        len(ET.fromstring(reply['svg_active']['result']).findall('.//{http://www.w3.org/2000/svg}linearGradient'))==1,
        'Evaluated target bypass reaches paint planning, used-color discovery and SVG even when the source Stroke is disabled')
    check(not reply['direct_toggle']['ok'] and reply['direct_toggle']['error']['code']=='DRIVEN_PROPERTY' and reply['direct_toggle']['revision']==2 and
        reply['same_id_edit']['ok'] and reply['preserved']['result']['authored']==dict(literal=False,driver=dict(link=source_ref)) and
        reply['preserved']['result']['evaluated'] is True,
        'Driven Paint toggle is refused while same-ID geometry replacement preserves the authored driver')
    disabled_uses=[ref for color in reply['colors_disabled']['result']['colors'] for ref in color['uses']]
    check(reply['source_disable']['ok'] and reply['get_disabled']['result']['evaluated'] is False and
        next(layer for layer in reply['render_disabled']['result']['paint_layers'] if layer['operation']=='path-B-stroke').get('gradient') is None and
        target_stop not in disabled_uses and not ET.fromstring(reply['svg_disabled']['result']).findall('.//{http://www.w3.org/2000/svg}linearGradient') and
        reply['get_enabled']['result']['evaluated'] is True,
        'Source toggles update target shape, used-color and SVG projections without changing the authored false target')
    check(not reply['delete_source']['ok'] and reply['delete_source']['error']['code']=='MISSING_REFERENCE' and
        reply['delete_source']['revision']==5,
        'Deleting a source object fails while a surviving Gradient target depends on it')
    native=reply['native']['result'];target_object=next(obj for obj in native['objects'] if obj['id']=='path-B')
    native_gradient=target_object['stack'][0]['gradient']
    check(native['version']=='0.50' and native_gradient['enabled'] is False and native_gradient['enabled_driver']==dict(link=source_ref) and
        run('--validate',native).returncode==0,
        'Native 0.36 persists the target literal and exact closed same-field driver')
    false_version=json.loads(json.dumps(native));false_version['version']='0.28'
    check('UNSUPPORTED_GRADIENT_ENABLED_DRIVER' in run('--validate',false_version).stderr,
        'Native 0.28 rejects a falsely versioned Gradient enabled driver')
    literal_legacy=json.loads(json.dumps(native));literal_legacy['version']='0.28'
    del next(obj for obj in literal_legacy['objects'] if obj['id']=='path-B')['stack'][0]['gradient']['enabled_driver']
    check(run('--validate',literal_legacy).returncode==0,
        'Native 0.28 continues to read literal-only Gradient state')
    malformed=json.loads(json.dumps(native));next(obj for obj in malformed['objects'] if obj['id']=='path-B')['stack'][0]['gradient']['enabled_driver']['extra']=True
    check('UNKNOWN_FIELD' in run('--validate',malformed).stderr,
        'Native rejects unknown Gradient enabled driver wrapper fields')
    wrong_ref=json.loads(json.dumps(native));next(obj for obj in wrong_ref['objects'] if obj['id']=='path-B')['stack'][0]['gradient']['enabled_driver']['link']['field']='op.path-A-stroke.enabled'
    check(run('--validate',wrong_ref).returncode==2,
        'Native validation rejects a Gradient enabled Ref with the wrong typed field')
    path.write_text(json.dumps(native),encoding='utf-8');before=path.read_bytes()
    cold=subprocess.run([exe,'--serve',str(path)],input=json.dumps(dict(op='get',ref=target_ref))+'\n'+
        json.dumps(dict(op='properties'))+'\n'+json.dumps(dict(op='inspect'))+'\n',capture_output=True,text=True,encoding='utf-8',timeout=20)
    cold_replies=[json.loads(line) for line in cold.stdout.splitlines()]
    cold_doc=cold_replies[2]['result']
    check(cold.returncode==0 and all(item['ok'] for item in cold_replies) and cold_replies[0]['result']==reply['get_enabled']['result'] and
        next(value for value in cold_replies[1]['result'] if value['ref']==target_ref)==cold_replies[0]['result'] and
        next(obj for obj in cold_doc['objects'] if obj['id']=='path-B')['stack'][0]['gradient']['enabled_driver']==dict(link=source_ref) and
        path.read_bytes()==before,
        'A distinct JSON-lines process cold-opens the link with equal typed state and exact unchanged native bytes')
with tempfile.TemporaryDirectory() as tmp:
    path=Path(tmp)/'composite-isolation.nect';path.write_text(json.dumps(sample),encoding='utf-8')
    source_ref=dict(object='path-A',point='',field='composite.isolated')
    target_ref=dict(object='path-B',point='',field='composite.isolated')
    composition=sample['compositions'][0]
    steps=[
        ('link',dict(op='apply',expected_revision=0,commands=[
            dict(type='set_compositing',object='path-A',blend='normal',isolated=True),
            dict(type='link_composite_isolated',target=target_ref,source=source_ref,replace_driver=False)])),
        ('linked',dict(op='get',ref=target_ref)),('properties',dict(op='properties')),
        ('direct_edit',dict(op='apply',expected_revision=1,commands=[dict(type='set_compositing',object='path-B',blend='normal',isolated=True)])),
        ('blend',dict(op='apply',expected_revision=1,commands=[dict(type='set_compositing',object='path-B',blend='multiply',isolated=False)])),
        ('source_false',dict(op='apply',expected_revision=2,commands=[dict(type='set_compositing',object='path-A',blend='normal',isolated=False)])),
        ('false_value',dict(op='get',ref=target_ref)),('false_plan',dict(op='compositing_plan',composition=composition['id'])),
        ('normal_blend',dict(op='apply',expected_revision=3,commands=[dict(type='set_compositing',object='path-B',blend='normal',isolated=False)])),
        ('source_true',dict(op='apply',expected_revision=4,commands=[dict(type='set_compositing',object='path-A',blend='normal',isolated=True)])),
        ('native',dict(op='inspect')),
        ('bad_batch',dict(op='apply',expected_revision=5,commands=[
            dict(type='set_compositing',object='path-B',blend='screen',isolated=False),
            dict(type='link_composite_isolated',target=target_ref,source=target_ref,replace_driver=False)])),
        ('delete_source',dict(op='apply',expected_revision=5,commands=[dict(type='delete_objects',objects=['path-A'])])),
        ('unlink',dict(op='apply',expected_revision=5,commands=[dict(type='unlink_composite_isolated',target=target_ref)])),
        ('frozen',dict(op='get',ref=target_ref)),
        ('source_again',dict(op='apply',expected_revision=6,commands=[dict(type='set_compositing',object='path-A',blend='normal',isolated=False)])),
        ('still_frozen',dict(op='get',ref=target_ref)),
    ]
    proc=subprocess.run([exe,'--serve',str(path)],input='\n'.join(json.dumps(request) for _,request in steps)+'\n',
        capture_output=True,text=True,encoding='utf-8',timeout=20)
    replies=[json.loads(line) for line in proc.stdout.splitlines()]
    check(proc.returncode==0 and len(replies)==len(steps),'Composite isolation JSON-lines process returns one response per request')
    reply={name:replies[index] for index,(name,_) in enumerate(steps)}
    check(reply['link']['ok'] and reply['blend']['ok'] and reply['source_false']['ok'] and
        reply['normal_blend']['ok'] and reply['source_true']['ok'] and reply['unlink']['ok'] and reply['source_again']['ok'],
        'Composite isolation link, blend preservation, source edits and unlink use Session commands')
    check(reply['linked']['result']['authored']==dict(literal=False,driver=dict(link=source_ref)) and
        reply['linked']['result']['evaluated'] is True and reply['linked']['result']['link'] is True and
        next(value for value in reply['properties']['result'] if value['ref']==target_ref)==reply['linked']['result'],
        'JSON-lines get and properties preserve target literal while exposing evaluated source state')
    target_plan=next(node for node in reply['false_plan']['result']['roots'] if node['object']=='path-B')
    check(reply['false_value']['result']['evaluated'] is False and target_plan['isolated'] is True,
        'A false linked authored value does not cancel isolation required by a non-normal blend')
    check(not reply['direct_edit']['ok'] and reply['direct_edit']['error']['code']=='DRIVEN_PROPERTY' and
        not reply['bad_batch']['ok'] and reply['bad_batch']['error']['code']=='DEPENDENCY_CYCLE' and reply['bad_batch']['revision']==5 and
        not reply['delete_source']['ok'] and reply['delete_source']['error']['code']=='MISSING_REFERENCE' and reply['delete_source']['revision']==5,
        'Driven literal, invalid later command and referenced-source deletion reject atomically')
    native=reply['native']['result'];native_target=next(obj for obj in native['objects'] if obj['id']=='path-B')['compositing']
    check(native['version']=='0.50' and native_target['isolated'] is False and
        native_target['isolated_driver']==dict(link=source_ref) and run('--validate',native).returncode==0,
        'Native 0.36 keeps the authored literal and exact driver separately')
    false_version=json.loads(json.dumps(native));false_version['version']='0.29'
    check('UNKNOWN_FIELD' in run('--validate',false_version).stderr,'Native 0.29 rejects a driver carried by a false version')
    path.write_text(json.dumps(native),encoding='utf-8');before=path.read_bytes()
    cold=subprocess.run([exe,'--serve',str(path)],input='\n'.join(json.dumps(request) for request in [
        dict(op='get',ref=target_ref),dict(op='properties'),dict(op='inspect')])+'\n',
        capture_output=True,text=True,encoding='utf-8',timeout=20)
    cold_replies=[json.loads(line) for line in cold.stdout.splitlines()];cold_doc=cold_replies[2]['result']
    check(cold.returncode==0 and all(value['ok'] for value in cold_replies) and
        cold_replies[0]['result']==reply['linked']['result'] and
        next(value for value in cold_replies[1]['result'] if value['ref']==target_ref)==cold_replies[0]['result'] and
        next(obj for obj in cold_doc['objects'] if obj['id']=='path-B')['compositing']==native_target and path.read_bytes()==before,
        'A distinct JSON-lines cold open preserves the typed state and exact native bytes')
    check(reply['frozen']['result']['authored']==dict(literal=True,driver=None) and
        reply['still_frozen']['result']['authored']==dict(literal=True,driver=None) and reply['still_frozen']['result']['evaluated'] is True,
        'Unlink freezes the current value independently of later source edits')
print(f'PASS {checks} process and native migration checks')

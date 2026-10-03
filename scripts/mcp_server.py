"""MCP 2025-06-18 stdio adapter forwarding to one live Nect desktop Session.

Run: python scripts/mcp_server.py --endpoint <desktop pipe/socket>
The local JSON-lines Session API itself is not MCP. This process implements
initialize, initialized, ping, tools/list, tools/call and read-only resources;
it never loads a document.
"""
import argparse
import json
import sys
from session_client import call, LIMIT

IDENTITY = {'session_id': {'type': 'string'}, 'document_id': {'type': 'string'}}
PROTOCOL_VERSION = '2025-06-18'
SERVER_INFO = {'name': 'nect-desktop', 'version': '0.1.0'}
SERVER_CAPABILITIES = {'tools': {'listChanged': False}, 'resources': {}}
SESSION_RESOURCE_URI = 'nect://session'
CAPABILITIES_RESOURCE_URI = 'nect://capabilities'
RESOURCES = [
    {'uri': SESSION_RESOURCE_URI, 'name': 'Nect desktop Session',
     'description': 'Live desktop Session identity, revision, save status and persistence receipts.',
     'mimeType': 'application/json'},
    {'uri': CAPABILITIES_RESOURCE_URI, 'name': 'Nect MCP capabilities',
     'description': 'MCP protocol and semantic capabilities derived from the active server definitions.',
     'mimeType': 'application/json'},
]
TOOLS = [
    {'name': 'nect_session', 'description': 'Read live desktop identity, revision and persistence receipts: pending/writing/native saved/recovery revisions and explicit errors.',
     'inputSchema': {'type': 'object', 'properties': {}, 'additionalProperties': False},
     'annotations': {'readOnlyHint': True, 'openWorldHint': False}},
    {'name': 'nect_command',
     'description': ('Inspect/evaluate/export_svg or edit the desktop-owned Session. request.op: '
                     'inspect, properties, get, resolve_name, evaluate, expression_language, compositing_types, compositing_plan, export_plan, compatibility_plan, export_svg, artboards, operator_types, gradient_types, render_plan, conversion_plan, definitions, definition, macros, macro, presets, preset, text_defaults, text_fonts, text_layout, apply, undo, redo, history, restore_history. '
                     'history lists stable state IDs and bounded retained memory estimates for this live Session only. '
                     'restore_history takes state_id and expected_revision, atomically returns to a retained state in one revision, '
                     'and keeps future states available until a new edit replaces the redo branch. '
                     'apply requires expected_revision and commands. Commands include create_path, '
                     'add_point, remove_point, close_contour, set, link, unlink, rename, '
                     'reorder_points, reorder_objects, group_contiguous, delete_objects, create_primitive, '
                     'enable_point_edit, clear_point_edit, convert_to_path, add_operation, remove_operation, reorder_operations, '
                     'enable_operation, operation_options, link_fill_rule, unlink_fill_rule, link_gradient_enabled, unlink_gradient_enabled, link_mask_enabled, unlink_mask_enabled, link_composite_isolated, unlink_composite_isolated, set_gradient, create_preset, create_preset_from_stack, rename_preset, update_preset, delete_preset, apply_preset and import_apply_preset. operator_types and gradient_types return exact templates. '
                     'Document-local PresetDefinition v1 retains its exact Offset then Repeater format. Preset schema v2 captures the complete supported ordered Path/Text processing sequence as tagged builtin/Macro entries; Macro entries retain same-Document Definition ID, pinned revision, enabled state and literal PublicParam overrides without copying graph internals. create_preset_from_stack returns captured_source_operations and tagged captured_source_entries with exact stable source entry IDs. presets lists definitions; preset {id} inspects one stable ID. Capture refuses link/expression-backed builtin fields with PRESET_NONPORTABLE_SOURCE and exact source Refs. Apply preflights every entry, creates fresh processing-entry IDs and Macro instance IDs in one Undo, and reports processing_entry_ids. import_apply_preset accepts a built-in literal Preset definition plus caller-supplied asset_id/accepted_revision context, a fresh definition_id, object and operation_id_prefix; it creates the document-local DefinitionID and applies in one Session revision/Undo. Its applied_library_presets receipt does not attest which Library file was read. Macro entries are refused by the portable workspace Library slice. Editing a definition affects later applies only, not existing stacks. Native 0.66 writes Preset v2; older native versions reject a v2 version lie. '
                     'Macro v1 stores document-local typed definitions with pinned revisions and one ordered mixed processing stack; only the validated Offset@1 -> Repeater@1 graph is supported. macros lists definitions; macro {id} reads one stable ID. create_macro_definition, rename_macro_definition, update_macro_definition (append a pinned revision), delete_macro_definition, instantiate_macro {object,definition,instance,revision,index}, set_macro_override/reset_macro_override, update_macro_instance {object,instance,revision} (explicit preflighted migration), and detach_macro_instance {object,instance,operation_id_prefix} are atomic Session commands. The workspace Macro Library stores canonical graph bytes and all retained revisions under a separate AssetID/accepted asset revision. import_apply_macro {definition,definition_id,object,instance,pinned_revision,index,asset_id,accepted_revision,overrides?} copies every retained graph revision into a fresh Document DefinitionID and applies a fresh processing instance in one generic apply batch/Undo; the stable PublicParamID numeric overrides are optional, and caller-supplied asset receipt metadata is not filesystem attestation. The public amount Ref field is exactly macro.offset.amount, with the Macro instance ID in Ref.point; properties/get expose the ordinary numeric value and unit. inspect retains Macro stack tags and pinned revision. '
                     'nect.shape.offset expands/contracts closed simple regions in object-local units; amount and miter_limit are normal properties. '
                     'operation_options accepts line_join miter/round/bevel for Offset; its composite must remain below. '
                     'Fill rule uses a same-field stable Ref via link_fill_rule {target:Fill op fill_rule Ref,source:Fill op fill_rule Ref,replace_driver:bool}; unlink_fill_rule freezes the evaluated nonzero/evenodd choice. The literal remains authored and Fill paint geometry, Canvas and SVG use the evaluated choice. Offset fill rules remain literal. '
                     'Offset also deforms earlier paint geometry, preserving paint bases; open or intersecting contours reject explicitly. '
                     'primitive_types returns Circle/Rectangle/Polygon/Star source templates for create_primitive. '
                     'Polygon/Star generator.points is a linkable integer (Polygon 3..256, Star 2..256); rotation uses degrees. '
                     'Radius/outer_radius/inner_radius and center_x/center_y use local du. '
                     'Generated point IDs preserve reduced angular phase and outer/inner role across count changes. '
                     'Count changes reject if a corrected or referenced point disappears; they never retarget by list index. '
                     'clear_point_edit {object} explicitly removes all point overrides and their bindings in one undoable edit. '
                     'Definition/Instance v1: create_definition {id,name,root} gives a stable Definition ID for an existing same-Composition Object subtree; definitions lists and definition {id} reads definitions. rename_definition {definition,name} changes metadata without changing the ID. create_instance {composition,parent,id,definition,name} places a stable Instance; its own transform, visibility, opacity/blend and order apply at the placement. Source-root affine and visibility are excluded, while root content/compositing and descendant authored content/visibility remain live. set_instance_override {instance,target:Ref,value} supports Scalar composite.opacity and Text text.font_size plus descendant transform.tx/transform.ty and Rectangle generator.width/generator.height; the last four are R04 Template projection fields. Source Fill/Color continues to propagate and has no local Fill/Color override. reset_instance_override restores one source value. Native 0.82 adds set_instance_visibility_override {instance,source,visible:boolean} for one stable descendant SourceItemID and reset_instance_visibility_override {instance,source}; these change occurrence-local visibility only, retain source literals/links/expressions, and respect ancestor suppression. The Definition root is not an eligible source item. detach_instance {instance,id_prefix} materializes the current source and overrides as independent Objects in one Undo. delete_definition refuses while any Instance uses it. Nested Instances/external dependencies reject. A Composition render caps the combined authored and transient projected Object count at 10000 and returns INSTANCE_RENDER_LIMIT when exceeded; Definitions are capped at 10000 per Document. '
                     'Collections are non-owning ordered Object ID sets: collections and collection {id} read stable membership; create_collection {id,name,members}, rename_collection {collection,name}, set_collection_members {collection,members}, and delete_collection {collection} use the same Session revision and Undo authority without moving artwork. '
                     'Anchor refs transform.anchor_x/y are local du; changing them preserves artwork. center_anchor {object} uses evaluated geometric bounds excluding stroke width. '
                     'set_position {object,x,y} sets the anchor position in effective-parent coordinates; transform_around_anchor {object,rotation,scale_x,scale_y} applies degree rotation and local-axis scale factors once about it. '
                     'These edit the canonical affine Scalars, reject changed driven fields, and do not create independent TRS properties. '
                     'set_transform_parent {object,parent:id|null,preserve_world:bool} replaces structural transform inheritance within the same Composition; null returns to the structural parent. '
                     'Keep-world parenting rejects singular new parents or driven fields that must change. Structure still owns ordering, grouping and selection. '
                     'transforms returns local/world matrices, effective parent, local anchor, parent-space position and world anchor for every object. '
                     'Batch commands: edit_properties {targets:[Ref],value:number,relative:bool}; link_properties {targets:[Ref],source:Ref,relative:bool}; unlink_properties {targets:[Ref]}. '
                     'Targets are 1..1000 unique compatible scalar refs. Absolute edits assign each target, relative edits add to each initial value once; driven edits reject until explicitly unlinked. '
                     'Relative links preserve each initial difference and unlink freezes each initial evaluated value; all target values come from one snapshot. '
                     'set_visibility {object,visible} edits the authored literal and rejects a driven Object visibility until explicitly unlinked. '
                     'link_object_visibility {target:object.visible Ref,source:object.visible Ref,replace_driver:bool} and unlink_object_visibility {target:object.visible Ref} provide same-Composition links across Path, Text, Image and Group; unlink freezes the evaluated own value. get/properties report authored literal, stable driver Ref and evaluated own visibility separately. Scalar batches cannot mutate this boolean. '
                     'set_compositing {object,blend,isolated,profile?} sets an exact registry blend/isolation; optional profile must equal the fixed nect.srgb8-premultiplied/v1 production profile or returns UNSUPPORTED_BLEND_PROFILE. Hue, Saturation, Color and Luminosity require native 0.79; they use shared W3C nonseparable binary64 arithmetic with final represented-half-up rounding. Ten deterministic arithmetic/whole-color modes require native 0.80 and explicitly refuse SVG as UNSUPPORTED_SVG_BLEND; their native/PNG paths preserve authored state. Existing modes retain Qt raster arithmetic. 16/32-bpc and alternate color spaces are unavailable. composite.opacity is a normal [0,1] Scalar. '
                     'link_composite_isolated {target:composite.isolated Ref,source:composite.isolated Ref,replace_driver:bool} and unlink_composite_isolated {target:composite.isolated Ref} provide same-Composition links between Objects; unlink freezes the evaluated authored value. The isolated literal remains authored, get/properties expose its driver and evaluated value, and SetCompositing preserves a driver when changing blend without changing that literal. Scene isolation still aggregates evaluated authored isolation with non-neutral opacity/blend, enabled masks and Group effects. '
                     'compositing_types lists the single immutable 26-mode registry, per-mode descriptors/arithmetic, fixed supported profile, unavailable profile cells, SVG CSS representation/reader requirements, and unverified AE status; compositing_plan {composition} reports ordered resolved scopes. '
                     'set_mask {object,mask:null|{id,source,version:1,enabled,fill_rule:"nonzero"|"evenodd",mode?:"geometry"|"alpha"|"luma",invert?:bool,mask_color_space?:"srgb"}} edits the canonical mask. Missing mode/invert/profile means Geometry/false/sRGB. Geometry uses final Path/Text geometry; Alpha uses isolated source RGBA from Path/Text/Image/Group in Composition space, includes source opacity and internal Group content, ignores only source-root visibility and blend, and applies optional invert. Luma uses unpremultiplied 8-bit sRGB RGB with coefficients 0.2125/0.7154/0.0721 multiplied by source alpha before optional invert; mask_color_space must be srgb. SVG explicitly refuses enabled Alpha/Luma masks without a lossless projection. '
                     'link_mask_enabled {target:Ref{object,point:"",field:"mask.<stable mask ID>.enabled"},source:same-field Ref,replace_driver:bool} and unlink_mask_enabled {target:qualified mask Ref} link one exact mask instance to another in the same Composition; unlink freezes the evaluated bool. mask.enabled remains a literal-only owner-slot read. Same-ID set_mask edits preserve the enabled driver, while driven toggle/removal/replacement needs explicit unlink. Qualified get/properties expose literal, driver and evaluated bypass; Canvas/SVG use the evaluated bit while retaining the mask and source geometry. '
                     'mask_objects {composition,parent,members:[ordered contiguous sibling IDs],id,mask_id,name,top:bool} wraps members in a masked Group and hides the topmost (last) or bottommost (first) source; a driven source must be unlinked first. '
                     'ungroup {composition,parent,group} removes a visible neutral static Group, preserving child IDs/order/world geometry in one Undo; compositing, dynamic Group transforms, removed-Group references and changing dependencies reject atomically. '
                     'put_inside {composition,parent,group,members:[ordered IDs]} moves contiguous siblings immediately preceding group into its children before existing content, preserving world transforms; target effects intentionally apply. '
                     'Normal Groups pass through; opacity/blend/mask/nonneutral scopes isolate then apply to the aggregate. Full AE blend parity remains unsupported. '
                     'set_expression {targets:[Ref],expression:{source:string,version:1},replace_binding:bool} assigns a bounded pure expression to compatible scalars. '
                     'Use expression_language for limits/functions; ref("object-id","point-id-or-empty","field") uses stable IDs. '
                     'Typing numbers cannot replace a formula. Unlink freezes its result; link commands explicitly replace it. Existing binding replacement needs replace_binding:true. '
                     'Formula errors/cycles/units/ranges reject the whole command; expressions persist in native 0.13 and SVG contains evaluated values only. '
                     'distribute_objects {objects:[id],axis:x/y,reference?:selection|artboard:id|grid:id|key_object:id,spacing?:du} makes equal geometric gaps among 3..1000 non-overlapping objects. Spatial order is independent of selection order. Selection reference keeps the outer objects fixed; key_object requires explicit nonnegative spacing. Artboard/Grid references place objects across evaluated bounds with n+1 gaps; an inherited Template Grid resolves by the target assignment Grid ID without materializing a local layout. A null-suppressed Grid is unavailable and rejects atomically. '
                     'align_objects {objects:[id],axis:x/y,alignment:min/center/max,reference:selection|key_object:id|artboard:id|grid:id|guide:id,guide_artboard?:id} aligns geometric bounds excluding stroke in one Composition. For an Artboard-local or inherited Template Guide, supply its target guide_artboard separately from the stable Guide ID; omitted/null scope retains global Guide semantics. Scoped Guides must be present, enabled and axis-matching. Artboard/Grid references use evaluated bounds; inherited Template Grids resolve by target assignment Grid ID without local materialization. The legacy artboard:null/id alias remains supported without reference. Rejects structural ancestor/descendant overlap and driven/unpreservable changes atomically. '
                     'transform_objects {objects:[id],rotation:number,scale_x:number,scale_y:number,pivot:null/[x,y]} applies world-axis scale then clockwise degree rotation about one common Composition-space pivot. Null uses geometric selection bounds center excluding stroke. Selected ancestors/followers transform once; changed driven matrices or singular external parents reject atomically. Zero/negative scale is allowed; retained sources and IDs remain. '
                     'translate_objects {objects:[id],dx:number,dy:number} translates selected world matrices once, including selected ancestors/followers, in one Composition. '
                     'duplicate_objects {objects:[id],prefix:unused ID prefix of 1..48 characters} makes independent in-place copies, once per selected Group closure, in one Composition. '
                     'Internal bindings/expressions/masks/Transform Parents follow copied IDs; outgoing references, Named Colors and image assets stay shared. Existing inbound references and Collection membership stay unchanged. '
                     'Copies follow each selected sibling run in paint order. apply returns created_ids for new objects; inspect reads their hierarchy and fresh nested IDs. Use translate_objects explicitly to move copies. '
                     'stroke_style {object,operation,line_cap:butt/round/square,line_join:miter/round/bevel,miter_limit:number} explicitly promotes a Stroke to behavior v2 and retains source geometry. Miter limit [1,1000] then becomes a linkable op.OP.miter_limit Scalar; changing a driven limit rejects. Existing v1 strokes stay butt/miter/4. '
                     'set_gradient edits or replaces one authored Gradient; preserve its IDs, Scalar bindings and same-ID enabled driver. Link or unlink its bypass only with link_gradient_enabled {target:Gradient enabled Ref,source:Gradient enabled Ref,replace_driver:bool} or unlink_gradient_enabled {target:Gradient enabled Ref}. Links are same-field within one Composition; unlink freezes the evaluated bool. Gradient.enabled stays the authored literal, and shape/Canvas/used-color/SVG consumers use the evaluated bypass separately from operation enabled. '
                     'Gradient numeric refs are op.OP_ID.gradient.GRADIENT_ID.start_x/start_y/end_x/end_y or stop.STOP_ID.offset/r/g/b/a. '
                     'Artboard commands: add_artboard {composition,artboard,index}, update_artboard {composition,artboard}, '
                     'delete_artboard/detach_artboard_parent {composition,artboard:id}, reorder_artboards {composition,order:[ids]}. '
                     'Artboard fields are id,name,x,y,width,height and optional parent_size:{artboard:id,width:bool,height:bool}. '
                     'ArtboardTemplate 0.77 has eight atomic commands: create_artboard_template {composition,id,name,source_artboard,definition:null|id}, rename_artboard_template {composition,template,name}, delete_artboard_template {composition,template}, assign_artboard_template {composition,artboard,template,content_instance:null|fresh-id}, set_artboard_template_override {composition,artboard,field,value}, reset_artboard_template_override {composition,artboard,field}, detach_artboard_template {composition,artboard,id_prefix}, and duplicate_template_artboard {composition,artboard,id_prefix,x,y,index}. Templates and source Artboards/Definitions resolve by stable ID within one Composition; names do not retarget. A content_instance ID must be fresh and creates an ordinary Definition Instance at the target Artboard origin. Template frame axes and Margin/Grid families override independently; set fields are frame.width/frame.height, layout.margin and layout.grid. For a local absent family send null; Grid values require the target assignment grid_id and bounds/count/gutters. Reset accepts one of those four fields and restores live inheritance. Detach materializes evaluated frame/layout and clears ownership with a fresh ID prefix. Duplicate requires an assigned Template and copies its authored frame/layout, Guide authoring and descendant Instance overrides to the exact x/y/index, allocating fresh Artboard, Content Instance, Guide and Grid IDs while preserving source references in one Undo. Assigned sources cannot be deleted. Native writer 0.77 persists the Template relation and authored overrides. '
                     'Artboard Guides have six atomic commands: add_artboard_guide/update_artboard_guide {composition,artboard,guide:{id,name,axis:x|y,position,enabled}}, delete_artboard_guide {composition,artboard,guide_id}, set_artboard_guide_override {composition,artboard,guide_id,field:position|enabled,value}, reset_artboard_guide_override {composition,artboard,guide_id,field:position|enabled}, and detach_artboard_guide {composition,artboard,guide_id,new_guide_id}. New authored Guide IDs are document-unique; inherited Guide Refs use {object:target-artboard-id,point:source-guide-id,field:artboard.guide.position}. Source Guide name, axis, and unoverridden enabled state continue to inherit. Detached Guides suppress only their source occurrence. Guide links/expressions and generic Set are unsupported. New local Guides and Guide overrides are persisted by native writer 0.77; earlier versions reject these fields. '
                     'Grid column gutter targets the stable Grid Ref {object:grid_id,point:"",field:"grid.column_gutter"}. set_grid_column_gutter_expression {target,expression:{source,version:1},replace_driver:bool} accepts bounded du expressions using same-Composition distinct-Artboard width/height refs; an active link or expression requires replace_driver:true to replace. link_grid_column_gutter and unlink_grid_column_gutter use the same target, with unlink freezing the evaluated value. get/properties return the authored literal, source and evaluated gutter. '
                     'Grid columns targets the stable Grid Ref {object:grid_id,point:"",field:"grid.columns"}. link_grid_columns {target,source,replace_driver:bool} and set_grid_columns_expression {target,expression:{source,version:1},replace_driver:bool} are mutually exclusive sources; expressions accept unitless constants and empty-point grid.columns Refs from distinct Artboards in the same Composition. Evaluation requires an exact finite integer from 1 to 1000 and rejects cycles/depth overflow. Replacing a different link/expression requires replace_driver:true; unlink_grid_columns freezes the evaluated count. get/properties expose the authored literal, exact source text/Ref, source kind, evaluated integer and expression capability. '
                     'Grid rows targets the stable Grid Ref {object:grid_id,point:"",field:"grid.rows"}. link_grid_rows {target,source,replace_driver:bool} and set_grid_rows_expression {target,expression:{source,version:1},replace_driver:bool} are mutually exclusive; expressions accept unitless constants and empty-point grid.rows Refs from distinct Artboards in the same Composition, with exact finite integer results from 1 to 1000 and cycle/depth protection. Replacing a different link/expression requires replace_driver:true; unlink_grid_rows freezes the evaluated count. get/properties expose the authored literal, exact source text/Ref, source kind, evaluated count and expression capability. Canvas rows, vertical Snap and cell-height validation use the evaluated count. '
                     'Size inheritance is same-composition; artboards readback returns authored/evaluated frames in export order. '
                     'Frame movement changes crops only; reorder changes order only; neither moves artwork. '
                     'Text: text_defaults returns a source template; text_fonts lists installed families. '
                     'create_text {composition,parent,id,name,source}; update_text {object,source} preserves source ID and existing Scalars. '
                     'Text source includes content,family,locale,weight,italic,layout auto/frame,direction horizontal/vertical,alignment start/center/end. '
                     'Native 0.78 adds ordered unique font_features records {feature_tag,parameter,scope:"whole_text"} and additional_axis_values {axis_tag:number}; Text source behavior version remains 1. Tags are exact case-sensitive four-byte printable ASCII, including spaces; feature parameters are uint32 [0,4294967295], and additional-axis values are finite doubles retained without clamping. '
                     'Use add_text_font_feature {object,feature:{feature_tag,parameter,scope:"whole_text"}}, update_text_font_feature {object,feature_tag,parameter}, remove_text_font_feature {object,feature_tag}, set_text_additional_axis {object,axis_tag,value}, and remove_text_additional_axis {object,axis_tag} through apply. Existing text.weight and text.italic exclusively own wght/ital; setting or removing those reserved additional axes returns TEXT_AXIS_CONFLICT. '
                     'create_text may supply initial font intent; update_text must preserve both font collections unchanged and returns USE_TYPED_COMMAND if either differs. Authored font intent in inspect/native is not proof of actual shaping. '
                     'properties/get/resolve_name expose content/family/locale as string and layout/direction/alignment as enum. Content, family and locale support same-field stable Ref links; direction, layout and alignment support typed links. Their literals remain authored and evaluated values drive layout/render, with expressions unsupported. update_text edits only unlinked literals. '
                     'Text weight is an integer [1,999]; link_text_weight {target:Text weight Ref,source:Text weight Ref,replace_driver:bool} links one Text weight to another. set_text_weight_expression {target:Text weight Ref,expression:{source,version:1},replace_driver:bool} accepts unitless constants and same-Document Text weight refs with exact integer results. unlink_text_weight {target:Text weight Ref} freezes its evaluated value. It is not a Scalar target. '
                     'Text content uses link_text_content {target:Text content Ref,source:Text content Ref,replace_driver:bool} and unlink_text_content {target:Text content Ref}; unlink freezes evaluated UTF-8 content, and update_text cannot change the driven literal. '
                     'Text family uses link_text_family {target:Text family Ref,source:Text family Ref,replace_driver:bool} and unlink_text_family {target:Text family Ref}; unlink freezes the evaluated font family, and update_text cannot change the driven literal. '
                     'Text locale uses link_text_locale {target:Text locale Ref,source:Text locale Ref,replace_driver:bool} and unlink_text_locale {target:Text locale Ref}; unlink freezes the evaluated locale, and update_text cannot change the driven literal. Locale values retain the existing nonempty UTF-8, 128-byte validation and platform shaping/fallback behavior; expressions and normalization are unsupported. '
                     'Text direction uses link_text_direction {target:Text direction Ref,source:Text direction Ref,replace_driver:bool} and unlink_text_direction {target:Text direction Ref}; its closed domain is horizontal/vertical. '
                     'Text layout uses link_text_layout {target:Text layout Ref,source:Text layout Ref,replace_driver:bool} and unlink_text_layout {target:Text layout Ref}; its closed domain is auto/frame. The target keeps its own frame_width/frame_height Scalars when the evaluated layout is frame. '
                     'Text alignment uses link_text_alignment {target:Text alignment Ref,source:Text alignment Ref,replace_driver:bool} and unlink_text_alignment {target:Text alignment Ref}; its closed domain is start/center/end, and unlink freezes the evaluated choice. '
                     'Numeric text.* refs: origin_x,origin_y,font_size,frame_width,frame_height,tracking,line_spacing (0=font default). '
                     'text_layout {object} reports bounds, overflow, fonts and warnings; font_request is the evaluated request, while font_runs records actual backend glyph runs and separately known resolved axes/ranges, unavailable metadata and unsupported intent. Feature availability alone does not prove a glyph effect. These derived receipts are not native authored state. Linux text_layout and Text projection remain unsupported with TEXT_PLATFORM_UNSUPPORTED; no Windows DirectWrite result is implied by authored-state success. export_plan {composition,artboard} discloses outlined SVG text; native text stays editable. set_artboard_background {composition,artboard,value} authors an optional literal sRGB/straight ColorValue (null means none). Template background override/reset and detach retain exact values; native0.81, no generic Color links. PNG includes authored underlay beneath completed transparent artwork; transparent/white is an additional matte choice. Analysis excludes underlay. compatibility_plan {target_profile,composition,artboard?,options?} is read-only: svg/1.1+css-compositing classifies source semantics and bounded planned local bakes; options accepts raster_scale in (0,16]. Bakes are not executed. ai/30.8 and pdf/x-4:2008 report policy_qualification_required and unavailable encoders without guessing item semantics. Multi-Artboard compositions require explicit artboard. '
                     'Typed colors: color_properties or properties lists aggregate color refs and four ordinary numeric channels; get supports both types. '
                     'used_colors inventories enabled paint inputs grouped by exact RGBA; equal values do not imply links. '
                     'create_named_color {color:{id,name,space:"srgb",profile:"srgb",alpha:"straight",rgba:[four Scalars]}}; '
                     'rename_named_color {color:id,name}; delete_named_color {color:id} rejects while referenced. '
                     'set_color {ref,value:{space:"srgb",profile:"srgb",alpha:"straight",rgba:[four numbers]}} rejects driven channels. '
                     'link_color {target,source} links all four channels; unlink_color {ref} freezes the resolved value. '
                     'Aggregate refs use point:"", field:"color" for a named color owner ID, op.OP.color for paint, or op.OP.gradient.GRAD.stop.STOP.color for a gradient stop. '
                     'Raster Images: assets lists metadata and placements (no byte payload); image.width/height are ordinary du Scalars. '
                     'create_image {composition,parent,id,name,source:{asset,width:Scalar,height:Scalar}} reuses an accepted asset. '
                     'delete_raster_asset {asset} rejects while placed. Use nect_image for local file import, check, reload, relink or embed. '
                     'Use properties to discover stable refs and units. All mutations are atomic and undoable.'),
     'inputSchema': {'type': 'object', 'properties': dict(IDENTITY, request={'type': 'object'}),
                     'required': ['session_id', 'document_id', 'request'], 'additionalProperties': False},
     'annotations': {'readOnlyHint': False, 'destructiveHint': True, 'openWorldHint': False}},
    {'name': 'nect_file',
     'description': ('Create/open/save a native document or protect recovery in the live desktop. '
                     'new/open/open_recovery rotate session identity; read returned identity before subsequent edits. '
                     'open protects outgoing work. open_recovery opens an unnamed copy without overwriting its source. '
                     'Committed edits live-save about once per second in the background. recover explicitly flushes committed protection. '
                     'save preserves previous file backups; external native changes reject with FILE_CHANGED, use another Save As path or reopen. '
                     'Read nect_session.persistence for verified per-destination revisions; queued data is not saved.'),
     'inputSchema': {'type': 'object', 'properties': dict(IDENTITY,
         op={'type': 'string', 'enum': ['new', 'open', 'open_recovery', 'save', 'recover']},
         expected_revision={'type': 'integer', 'minimum': 0}, path={'type': 'string'}),
         'required': ['session_id', 'document_id', 'op', 'expected_revision'], 'additionalProperties': False},
     'annotations': {'readOnlyHint': False, 'destructiveHint': True, 'openWorldHint': False}},
    {'name': 'nect_export_flattened_svg',
     'description': 'Explicit appearance-only SVG derivative with one embedded PNG from committed Canvas artwork. All paths, text and layers become one raster image in the derivative; native data and History remain unchanged. Requires an absolute .svg path and pixels-per-document-unit scale (0,16]. Authored Artboard background is preserved on transparent matte. Atomic replacement, native/linked source protection and active-gesture refusal. This is not editable vector SVG or AI export.',
     'inputSchema': {'type':'object','properties':dict(IDENTITY,
         op={'type':'string','enum':['export_flattened_svg']},expected_revision={'type':'integer','minimum':0},
         path={'type':'string'},composition={'type':'string'},artboard={'type':'string'},
         scale={'type':'number','exclusiveMinimum':0,'maximum':16}),
         'required':['session_id','document_id','op','expected_revision','path','composition','artboard','scale'],'additionalProperties':False},
     'annotations':{'readOnlyHint':False,'destructiveHint':True,'openWorldHint':False}},
    {'name': 'nect_export_png',
     'description': 'Export committed artwork from one Artboard to an absolute local .png path. Shared Canvas compositing, no UI overlays. Atomic replacement; no native/history changes. Explicit scale in pixels per document unit (0 < scale <= 16), transparent or white background, 8-bit sRGB, maximum 8192 per axis / 16 MP. Active gestures and native/linked-source destinations reject.',
     'inputSchema': {'type':'object','properties':dict(IDENTITY,
         op={'type':'string','enum':['export_png']},expected_revision={'type':'integer','minimum':0},
         path={'type':'string'},composition={'type':'string'},artboard={'type':'string'},
         scale={'type':'number'},background={'type':'string','enum':['transparent','white']}),
         'required':['session_id','document_id','op','expected_revision','path','composition','artboard','scale','background'],'additionalProperties':False},
     'annotations':{'readOnlyHint':False,'destructiveHint':True,'openWorldHint':False}},
    {'name': 'nect_analyze_regions',
      'description': 'Read-only analysis of committed Canvas output on one Artboard. Requires the exact live session/document identity and expected revision; active gestures reject. Uses a transparent backdrop and the shared Canvas renderer at scale 0 < scale <= 16, then finds deterministic 4-connected components where the 8-bit output alpha byte is >= threshold. Also returns foreground 4-neighbor alpha edge pixels as maximal row-major horizontal runs, including hole boundaries; clockwise outer contour cycles on integer pixel-corner coordinates; and maximal runs of at least three threshold-foreground pixel centers in four directions. Line candidates require exact one-pixel thinness: horizontal pixels have no foreground above/below, vertical pixels no foreground left/right, and diagonal pixels no foreground in any orthogonal neighbor. Runs cannot extend by one more qualifying pixel. Directions are horizontal, vertical, down-diagonal and up-diagonal; endpoints are inclusive integer output pixel centers, sorted by direction then start y/x. Thick strokes, gaps and subpixel positions are not inferred. At diagonal vertex crossings, contour tracing takes the right turn before straight or left; counterclockwise inner/hole cycles are omitted. Returns integer pixel area and bounds, edge pixel count, image dimensions, sRGB/premultiplied alpha domain, and source revision. Optional include_color_groups=true groups threshold-foreground pixels by exact straight sRGB RGB8 output bytes, with area, bounds and maximal row-major horizontal runs (maximum 256 groups and 20,000 total color runs). Optional include_color_components=true requires include_color_groups=true and splits those exact group runs into 4-connected components, sorted by RGB then first row-major pixel; each has a stable typed ID, result-local component_index, area, bounds and lossless runs, with at most 10,000 total components. Optional intersect_color_component_index selects a component from this same result and intersects its runs with mask_boolean; it requires both color options and returns a snapshot-identified derived candidate with area and maximal row-major runs. Limits: 4,000,000 output pixels, 10,000 regions, 100,000 edge runs, 200,000 directed boundary edges, 10,000 line candidates and 20,000 Boolean-mask runs. No files, Session history, or authored state are changed. The response declares analysis_behavior_version=1 and a deterministic analysis_id bound to document/composition/artboard, source revision, scale, threshold, domains and exact rendered pixels. Region, contour, line, color, morphology, erosion, Boolean-mask and intersection children carry typed snapshot IDs, not authored or cross-revision IDs. Optional intersect_color_component_id selects the exact component in the current snapshot; supplying it together with the legacy index selector rejects. Stale or mismatched IDs reject without a partial result.',
     'inputSchema': {'type':'object','properties':dict(IDENTITY,
         op={'type':'string','enum':['analyze_regions']},expected_revision={'type':'integer','minimum':0},
         composition={'type':'string'},artboard={'type':'string'},scale={'type':'number','exclusiveMinimum':0,'maximum':16},
          threshold={'type':'integer','minimum':1,'maximum':255},include_color_groups={'type':'boolean'},
          include_color_components={'type':'boolean'},intersect_color_component_index={'type':'integer','minimum':0},
          intersect_color_component_id={'type':'string','minLength':1}),
         'required':['session_id','document_id','op','expected_revision','composition','artboard','scale','threshold'],'additionalProperties':False},
     'annotations':{'readOnlyHint':True,'destructiveHint':False,'openWorldHint':False}},
    {'name': 'nect_adopt_analysis_line',
     'description': 'Adopt one exact current thin-line analysis candidate as an editable two-anchor open Path in one Undo. Re-analyzes the exact Session/revision, scale and threshold; accepts stable analysis_id and line_id, never supplied geometry. Pixel-center endpoints map through Artboard origin and scale with no fitting or thickness reconstruction. Source artwork remains unchanged. Stale/forged identities and active gestures reject.',
     'inputSchema': {'type':'object','properties':dict(IDENTITY,
         op={'type':'string','enum':['adopt_analysis_line']},expected_revision={'type':'integer','minimum':0},
         composition={'type':'string'},artboard={'type':'string'},scale={'type':'number','exclusiveMinimum':0,'maximum':16},
         threshold={'type':'integer','minimum':1,'maximum':255},analysis_id={'type':'string'},line_id={'type':'string'},name={'type':'string'}),
         'required':['session_id','document_id','op','expected_revision','composition','artboard','scale','threshold','analysis_id','line_id','name'],'additionalProperties':False},
     'annotations':{'readOnlyHint':False,'destructiveHint':False,'openWorldHint':False}},
    {'name': 'nect_adopt_analysis_contour',
     'description': ('Create one editable Path from an existing analyze_regions outer contour in one Undo. '
                     'Requires the exact live Session, revision, analysis_id, contour_id and original analysis parameters. '
                     'Rechecks the current rendered snapshot; stale or mismatched IDs reject without mutation. '
                     'Inserts at the Composition root, preserving source artwork. Pixel corners map to Artboard origin '
                     'plus pixel/scale, with fresh authored point IDs and straight anchors. Holes are omitted; '
                     'no smoothing, curve fitting or appearance preservation is claimed. At most 10000 anchors.'),
     'inputSchema': {'type':'object','properties':dict(IDENTITY,
         op={'type':'string','enum':['adopt_analysis_contour']},expected_revision={'type':'integer','minimum':0},
         composition={'type':'string'},artboard={'type':'string'},scale={'type':'number','exclusiveMinimum':0,'maximum':16},
         threshold={'type':'integer','minimum':1,'maximum':255},analysis_id={'type':'string','minLength':1},
         contour_id={'type':'string','minLength':1},name={'type':'string'}),
         'required':['session_id','document_id','op','expected_revision','composition','artboard','scale','threshold',
                     'analysis_id','contour_id','name'],'additionalProperties':False},
     'annotations':{'readOnlyHint':False,'destructiveHint':False,'openWorldHint':False}},
    {'name': 'nect_analyze_dataset',
     'description': ('Read-only typed analysis.dataset/v1 from the live desktop Session. Operators are '
                     'nect.analysis.image.regions@1 (input domain artboard.rgba8_srgb_premultiplied; parameters '
                     'artboard_id, scale, threshold and optional image-region selectors), '
                     'nect.analysis.vector.geometry@1 (document.path_geometry; parameters object_id and optional '
                     'contour_id), and nect.analysis.document.structure@1 (document.composition_structure; empty '
                     'parameters). Results identify the exact document, Composition and source revision, coordinate '
                     'domain, typed records, warnings and hard limits. Child IDs are stable for one analysis snapshot; '
                     'source Object, Contour and Point IDs remain provenance. Uses the exact live Session and never '
                     'creates or adopts a second Document. Stale revisions, unsupported operators/versions/domains, '
                     'invalid parameters, dangling relations, non-finite geometry and resource-limit violations reject '
                     'without a partial result or authored/history mutation.'),
     'inputSchema': {'type':'object','properties':dict(IDENTITY,
         op={'type':'string','enum':['analysis_dataset']},expected_revision={'type':'integer','minimum':0},
         operator_type_id={'type':'string','enum':['nect.analysis.image.regions','nect.analysis.vector.geometry','nect.analysis.document.structure']},
         operator_version={'type':'integer','minimum':1},
         input_domain={'type':'string','enum':['artboard.rgba8_srgb_premultiplied','document.path_geometry','document.composition_structure']},
         composition_id={'type':'string'},parameters={'type':'object'}),
         'required':['session_id','document_id','op','expected_revision','operator_type_id','operator_version','input_domain','composition_id','parameters'],
         'additionalProperties':False},
     'annotations':{'readOnlyHint':True,'destructiveHint':False,'openWorldHint':False}},
    {'name': 'nect_import_svg',
     'description': 'Import a bounded local static SVG as editable path/Group artwork in one Undo. Supports M/L/H/V/C/S/Q/T/A/Z, groups, rect/circle/ellipse/line/polyline/polygon, solid paints and affine transforms. Shapes/arcs become paths; elliptical portions use cubic approximation (spans at most45 degrees). CSS stylesheets, text/images, masks, external content and unknown semantics reject atomically. The source viewport maps coordinates but is not imported as a crop or Artboard. Original file unchanged. Requires absolute local path, fresh 1..40-character identifier prefix, current identity/revision; 1 MiB,128 nodes,10000 points.',
     'inputSchema': {'type': 'object', 'properties': dict(IDENTITY,
         op={'type': 'string', 'enum': ['import_svg']}, expected_revision={'type': 'integer', 'minimum': 0},
         path={'type': 'string'}, composition={'type': 'string'}, prefix={'type': 'string'}, name={'type': 'string'},
         x={'type': 'number'}, y={'type': 'number'}),
         'required': ['session_id','document_id','op','expected_revision','path','composition','prefix','name','x','y'], 'additionalProperties': False},
     'annotations': {'readOnlyHint': False, 'destructiveHint': False, 'openWorldHint': False}},
    {'name': 'nect_image',
     'description': ('Import PNG/JPEG as a Linked or Embedded asset with one Image placement, or manage an existing asset. '
                     'import_image requires path (absolute local Windows drive path), explicit mode linked/embedded, composition,parent (empty for root), '
                     'asset (new stable ID), id (new Image ID), name, x,y; placement starts at one du per oriented source pixel. '
                     'asset operation requires asset ID and action status/check/reload/relink/embed; relink also requires path. '
                     'status reads the last observation without filesystem access; check compares linked source bytes without accepting pixels or changing revision. '
                     'reload and relink atomically replace accepted bytes for ALL placements, keeping display dimensions and transforms. '
                     'embed keeps accepted cached bytes and clears the link; no file read. All authored changes are one Undo. '
                     'Native open never fetches links; current/changed/missing/unreadable are explicit check observations. '
                     'PNG/JPEG 8-bit RGB/gray/palette only, JPEG EXIF orientation and bounded ICC to sRGB. '
                     'Limits: 8 MiB and 16MP per source,8192 per axis;24 MiB and32MP per document;128 assets. '
                     'Unsupported CMYK,high-bit-depth,animation,PNG eXIf and unsupported color metadata reject without changes.'),
     'inputSchema': {'type':'object','properties':dict(IDENTITY,
         op={'type':'string','enum':['import_image','asset']},expected_revision={'type':'integer','minimum':0},
         path={'type':'string'},mode={'type':'string','enum':['linked','embedded']},composition={'type':'string'},parent={'type':'string'},
         asset={'type':'string'},id={'type':'string'},name={'type':'string'},x={'type':'number'},y={'type':'number'},
         action={'type':'string','enum':['status','check','reload','relink','embed']}),
         'required':['session_id','document_id','op','expected_revision','asset'],'additionalProperties':False},
     'annotations':{'readOnlyHint':False,'destructiveHint':False,'openWorldHint':False}},
]


class ProtocolError(Exception):
    def __init__(self, code, message, data=None):
        self.code, self.message, self.data = code, message, data


def capabilities_resource():
    tool_names = [tool['name'] for tool in TOOLS]
    capability_groups = []
    semantic_operations = []
    unsupported_groups = []
    for tool in TOOLS:
        description = tool['description']
        request_schema = tool.get('inputSchema', {}).get('properties', {}).get('request')
        purpose = description.partition(' request.op:')[0].strip()
        capability_groups.append({'name': tool['name'], 'tools': [tool['name']],
                                  'description': purpose})
        if request_schema is not None:
            marker = 'request.op: '
            if marker in description:
                operation_text = description.split(marker, 1)[1].partition('. ')[0]
                semantic_operations = [operation.strip() for operation in operation_text.split(',')
                                       if operation.strip()]
        for sentence in description.split('. '):
            if 'unsupported' in sentence.casefold():
                unsupported_groups.append({'tool': tool['name'], 'statement': sentence.strip()})
    return {
        'protocolVersion': PROTOCOL_VERSION,
        'serverInfo': SERVER_INFO,
        'capabilities': SERVER_CAPABILITIES,
        'toolNames': tool_names,
        'capabilityGroups': capability_groups,
        'semanticRequestOperations': semantic_operations,
        'unsupportedCapabilityGroups': unsupported_groups,
    }


def read_resource(endpoint, uri):
    if uri == SESSION_RESOURCE_URI:
        try:
            # Always query the desktop-owned Session. Never retain a sidecar snapshot.
            payload = call(endpoint, {'op': 'hello'})
        except (OSError, ValueError, TimeoutError):
            raise ProtocolError(-32603, 'Unable to read the live Session resource', {'uri': uri})
    else:
        payload = capabilities_resource()
    return {'contents': [{'uri': uri, 'mimeType': 'application/json',
                          'text': json.dumps(payload, ensure_ascii=False, allow_nan=False)}]}


def unique(pairs):
    out = {}
    for key, value in pairs:
        if key in out:
            raise ValueError('Duplicate JSON key')
        out[key] = value
    return out


def run(endpoint):
    initialized = False
    ready = False
    for line in iter(lambda: sys.stdin.buffer.readline(LIMIT + 1), b''):
        request_id = None
        notification = False
        try:
            if len(line) > LIMIT:
                raise ProtocolError(-32700, 'Message exceeds 64 MiB')
            try:
                message = json.loads(line, object_pairs_hook=unique,
                    parse_constant=lambda _: (_ for _ in ()).throw(ValueError('Non-finite JSON number')))
            except (ValueError, UnicodeError):
                raise ProtocolError(-32700, 'Invalid JSON')
            if not isinstance(message, dict) or message.get('jsonrpc') != '2.0' or not isinstance(message.get('method'), str):
                raise ProtocolError(-32600, 'JSON-RPC 2.0 request object required')
            notification = 'id' not in message
            request_id = message.get('id')
            if not notification and (isinstance(request_id, bool) or not isinstance(request_id, (str, int))):
                raise ProtocolError(-32600, 'Request ID must be a string or integer')
            method = message['method']
            params = message.get('params', {})
            if not isinstance(params, dict):
                raise ProtocolError(-32602, 'Named parameters required')
            if method == 'notifications/initialized' and initialized:
                ready = True
                continue
            if notification:
                continue
            if method == 'ping':
                result = {}
            elif method == 'initialize':
                if initialized:
                    raise ProtocolError(-32600, 'Already initialized')
                if not isinstance(params.get('protocolVersion'), str) or not isinstance(params.get('capabilities'), dict) or not isinstance(params.get('clientInfo'), dict):
                    raise ProtocolError(-32602, 'Initialization requires protocolVersion, capabilities and clientInfo')
                initialized = True
                result = {'protocolVersion': PROTOCOL_VERSION, 'capabilities': SERVER_CAPABILITIES,
                          'serverInfo': SERVER_INFO,
                          'instructions': 'Read nect_session first. Keep identity and expected revision explicit. Never blindly retry a failed transport mutation.'}
            elif not ready:
                raise ProtocolError(-32002, 'Initialize and send notifications/initialized first')
            elif method == 'tools/list':
                if params:
                    raise ProtocolError(-32602, 'No pagination cursor is supported')
                result = {'tools': TOOLS}
            elif method == 'resources/list':
                if params:
                    raise ProtocolError(-32602, 'No pagination cursor is supported')
                result = {'resources': RESOURCES}
            elif method == 'resources/read':
                if set(params) != {'uri'}:
                    raise ProtocolError(-32602, 'Resource read requires only a uri')
                uri = params['uri']
                if not isinstance(uri, str):
                    raise ProtocolError(-32602, 'Resource URI must be a string')
                if uri not in {resource['uri'] for resource in RESOURCES}:
                    raise ProtocolError(-32002, 'Resource not found', {'uri': uri})
                result = read_resource(endpoint, uri)
            elif method == 'tools/call':
                name = params.get('name')
                arguments = params.get('arguments', {})
                tool = next((t for t in TOOLS if t['name'] == name), None)
                if tool is None:
                    raise ProtocolError(-32602, 'Unknown tool')
                if not isinstance(arguments, dict):
                    raise ProtocolError(-32602, 'Tool arguments must be an object')
                schema = tool['inputSchema']
                if set(arguments) - set(schema['properties']) or set(schema.get('required', [])) - set(arguments):
                    raise ProtocolError(-32602, 'Missing or unknown tool arguments')
                for key, value in arguments.items():
                    rule = schema['properties'][key]
                    expected = {'string': str, 'object': dict, 'integer': int, 'number': (int, float), 'boolean': bool}[rule['type']]
                    if (not isinstance(value, expected) or
                        (isinstance(value, bool) and rule['type'] != 'boolean') or
                        ('enum' in rule and value not in rule['enum']) or
                        ('minimum' in rule and value < rule['minimum']) or
                        ('maximum' in rule and value > rule['maximum']) or
                        ('exclusiveMinimum' in rule and value <= rule['exclusiveMinimum'])):
                        raise ProtocolError(-32602, 'Invalid argument: ' + key)
                envelope = {'op': 'hello'} if name == 'nect_session' else dict(arguments)
                if name == 'nect_command':
                    envelope['op'] = 'core'
                try:
                    reply = call(endpoint, envelope)
                except (OSError, ValueError, TimeoutError) as error:
                    reply = {'ok': False, 'error': {'code': 'TRANSPORT_ERROR', 'message': str(error)}}
                result = {'content': [{'type': 'text', 'text': json.dumps(reply, ensure_ascii=False)}],
                          'structuredContent': reply, 'isError': not reply.get('ok', False)}
            else:
                raise ProtocolError(-32601, 'Method not found')
            response = {'jsonrpc': '2.0', 'id': request_id, 'result': result}
        except ProtocolError as error:
            if notification:
                continue
            body = {'code': error.code, 'message': error.message}
            if error.data is not None:
                body['data'] = error.data
            response = {'jsonrpc': '2.0', 'id': request_id, 'error': body}
        sys.stdout.buffer.write(json.dumps(response, ensure_ascii=False, allow_nan=False).encode('utf-8') + b'\n')
        sys.stdout.buffer.flush()
        if len(line) > LIMIT:
            return


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--endpoint', required=True)
    run(parser.parse_args().endpoint)

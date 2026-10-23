#include "canvas.hpp"
#include "nect/io.hpp"

#include <QApplication>
#include <QImage>
#include <QMouseEvent>
#include <QTest>
#include <QWheelEvent>
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace nect;
using nect::desktop::Canvas;
namespace {
int checks=0;
void check(bool ok,const std::string& message){if(!ok)throw std::runtime_error(message);++checks;}
Object rectangle(Id id,double x,double y,double width,double height,QColor color) {
    Object object;object.id=id;object.name=id;Contour contour;contour.id=id+"-contour";contour.closed=true;
    for(const auto& xy:std::vector<Vec2>{{x,y},{x+width,y},{x+width,y+height},{x,y+height}}) {
        Point point;point.id=id+"-p"+std::to_string(contour.points.size());point.x.literal=xy.x;point.y.literal=xy.y;
        contour.points.push_back(point);
    }
    object.contours.push_back(contour);
    auto fill=default_operation(id+"-fill","nect.paint.fill");
    fill.parameters.at("r").literal=color.redF();fill.parameters.at("g").literal=color.greenF();
    fill.parameters.at("b").literal=color.blueF();fill.parameters.at("a").literal=color.alphaF();
    object.stack.push_back(fill);return object;
}
Document document(bool paper=true) {
    auto result=empty_document("document","composition","artboard");
    result.compositions[0].artboards[0].width=640;result.compositions[0].artboards[0].height=480;
    if(paper){result.objects.emplace("paper",rectangle("paper",0,0,640,480,Qt::white));result.compositions[0].roots.push_back("paper");}
    return result;
}
void add(Document& d,Object object) {d.compositions[0].roots.push_back(object.id);d.objects.emplace(object.id,std::move(object));}
void group(Document& d,const Id& id,std::vector<Id> children) {
    Object object;object.id=id;object.name=id;object.kind=Kind::group;object.children=std::move(children);
    auto& roots=d.compositions[0].roots;
    for(const auto& child:object.children)roots.erase(std::remove(roots.begin(),roots.end(),child),roots.end());
    add(d,std::move(object));
}
struct Fixture {
    Session session;Canvas canvas;QString last_error;int commits=0;
    explicit Fixture(Document d):session(std::move(d)),canvas(session) {
        canvas.error=[this](QString error){last_error=error;};canvas.document_changed=[this]{++commits;};
        canvas.resize(740,580);canvas.show();canvas.setFocus();QApplication::processEvents();
        canvas.fit_artboard();canvas.set_selection({});canvas.set_show_mask_outline(false);QApplication::processEvents();
        check(std::abs(canvas.zoom()-1)<1e-8,"Unit-scale pixel fixture");
    }
    QPoint screen(double x,double y) const {return {qRound(canvas.width()/2.0+(x-320)*canvas.zoom()),qRound(canvas.height()/2.0+(y-240)*canvas.zoom())};}
    QImage image() {QApplication::processEvents();return canvas.grab().toImage();}
    QColor pixel(const QImage& image,double x,double y)const {
        const auto p=screen(x,y);return image.pixelColor(qRound(p.x()*image.devicePixelRatio()),qRound(p.y()*image.devicePixelRatio()));
    }
    void color(double x,double y,QColor expected,const std::string& reason,int tolerance=2) {
        const auto actual=pixel(image(),x,y);
        check(std::abs(actual.red()-expected.red())<=tolerance&&std::abs(actual.green()-expected.green())<=tolerance&&std::abs(actual.blue()-expected.blue())<=tolerance,
            reason+": expected "+expected.name().toStdString()+", got "+actual.name().toStdString());
    }
    void apply(std::vector<Command> commands){session.apply(commands,session.revision());canvas.refresh();QApplication::processEvents();}
    void click(double x,double y){QTest::mouseClick(&canvas,Qt::LeftButton,Qt::NoModifier,screen(x,y),1);QApplication::processEvents();}
    void no_error(){check(last_error.isEmpty(),"Unexpected render error: "+last_error.toStdString());}
};

void group_opacity_is_applied_once() {
    auto d=document();add(d,rectangle("a",100,100,160,160,Qt::red));add(d,rectangle("b",180,120,160,160,Qt::red));
    group(d,"group",{"a","b"});d.objects.at("group").compositing.opacity.literal=.5;Fixture f(d);
    f.color(140,150,QColor(255,128,128),"Single child receives Group opacity");
    f.color(220,170,QColor(255,128,128),"Overlapping opaque children receive Group opacity only once");
    f.no_error();
}
void group_posterize_uses_independent_postcomposite_pixel_oracle() {
    auto d=document(false);
    add(d,rectangle("red",100,100,180,180,QColor(255,0,0,128)));
    add(d,rectangle("blue",180,120,180,180,QColor(0,0,255,128)));
    group(d,"group",{"red","blue"});
    const auto original=Canvas::render_artboard(d,"composition","artboard",1,true);
    const auto transparent_original=Canvas::render_artboard(d,"composition","artboard",1,false);
    const auto baseline_overlap=original.pixelColor(220,180);
    const auto baseline_red=original.pixelColor(140,180);
    const auto baseline_blue=original.pixelColor(300,180);
    check(std::abs(baseline_overlap.red()-127)<=3&&std::abs(baseline_overlap.green()-63)<=3&&std::abs(baseline_overlap.blue()-191)<=3,
        "No-effect overlap is the independently expected red-over-blue source-over pixel");
    const auto straight_baseline=transparent_original.pixelColor(220,180);
    check(std::abs(straight_baseline.red()-85)<=3&&straight_baseline.green()==0&&std::abs(straight_baseline.blue()-170)<=3&&
        straight_baseline.alpha()==192,
        "Transparent baseline independently retains the purple straight overlap and exact three-quarter alpha");
    check(baseline_red==QColor(255,127,127)&&baseline_blue==QColor(127,127,255),
        "Single-child transparent colors composite over the separate white export background");

    Session session(d);session.apply({AddOperation{"group",default_operation("posterize","nect.group.posterize"),0}},0);
    const auto actual=Canvas::render_artboard(session.document(),"composition","artboard",1,true);
    const auto overlap=actual.pixelColor(220,180);
    const auto transparent_actual=Canvas::render_artboard(session.document(),"composition","artboard",1,false).pixelColor(220,180);
    // Independent arithmetic: source-over gives A=192, premultiplied RGB=(64,0,128).
    // At L=2, straight R=64/192 quantizes to 0 and B=128/192 quantizes to 1;
    // over white, the preserved 63/255 transparency yields (63,63,255).
    check(std::abs(overlap.red()-63)<=3&&std::abs(overlap.green()-63)<=3&&std::abs(overlap.blue()-255)<=3,
        "Group Posterize quantizes the completed overlap and preserves its 75% alpha");
    check(transparent_actual.red()==0&&transparent_actual.green()==0&&transparent_actual.blue()==255&&transparent_actual.alpha()==192,
        "Posterized transparent overlap is opaque blue RGB at the original alpha byte");
    const auto red=actual.pixelColor(140,180),blue=actual.pixelColor(300,180);
    check(red==QColor(255,127,127)&&blue==QColor(127,127,255),
        "Single-child red and blue interiors remain unchanged at two levels");
    check(overlap!=baseline_overlap,
        "Negative oracle: child-local Posterize would retain the baseline purple overlap");

    session.apply({EnableOperation{"group","posterize",false}},session.revision());
    const auto bypass=Canvas::render_artboard(session.document(),"composition","artboard",1,true);
    check(bypass==original,"Bypass returns the original overlap and all rendered pixels exactly");
    session.apply({EnableOperation{"group","posterize",true}},session.revision());
    session.apply({Set{operation_ref("group","posterize","levels"),3}},session.revision());
    const auto three_levels=Canvas::render_artboard(session.document(),"composition","artboard",1,true);
    check(three_levels.pixelColor(220,180)!=overlap,"Changing levels through Session changes the derived overlap only");
    check(three_levels.pixelColor(140,180)==red&&three_levels.pixelColor(300,180)==blue,
        "Changing levels leaves nonoverlapping child interiors unchanged");
    const auto before_remove=session.document();const auto before_remove_revision=session.revision();
    session.apply({RemoveOperation{"group","posterize"}},session.revision());
    check(session.revision()==before_remove_revision+1&&Canvas::render_artboard(session.document(),"composition","artboard",1,true)==original,
        "Removing Group Posterize is one edit and restores the exact original pixel result");
    session.undo(session.revision());
    check(session.document()==before_remove&&Canvas::render_artboard(session.document(),"composition","artboard",1,true)==three_levels,
        "Undo restores the exact authored Group stack and pixel result");
}
void pass_through_and_isolation_have_distinct_backdrops() {
    auto d=document();add(d,rectangle("blue",80,80,300,260,Qt::blue));add(d,rectangle("red",100,100,200,180,Qt::red));
    d.objects.at("red").compositing.blend="multiply";group(d,"group",{"red"});Fixture f(d);
    f.color(160,160,Qt::black,"Neutral Group passes a child's Multiply into the Composition backdrop");
    f.apply({SetCompositing{"group","normal",true}});
    f.color(160,160,Qt::red,"Explicit isolation gives the child a transparent local backdrop");
    f.no_error();
}
void blend_alpha_and_transparent_root() {
    auto d=document();add(d,rectangle("blue",80,80,300,260,Qt::blue));add(d,rectangle("red",100,100,200,180,Qt::red));
    d.objects.at("red").compositing.blend="multiply";d.objects.at("red").compositing.opacity.literal=.5;Fixture f(d);
    f.color(160,160,QColor(0,0,128),"Multiply respects aggregate alpha");
    f.apply({SetCompositing{"red","screen",false}});f.color(160,160,QColor(128,0,255),"Screen respects aggregate alpha");f.no_error();
    auto first=document(false);add(first,rectangle("red",100,100,200,180,Qt::red));first.objects.at("red").compositing.blend="screen";
    Fixture transparent(first);transparent.color(160,160,Qt::red,"First Screen layer blends against transparent artwork, not the UI paper");transparent.no_error();
}
double separable_blend(const std::string& mode,double backdrop,double source) {
    // Independent channel oracles from W3C Compositing and Blending Level 1,
    // section 10.1: https://www.w3.org/TR/compositing-1/#blendingseparable
    // Work in straight sRGB components; do not use Qt's composition functions.
    if(mode=="normal")return source;
    if(mode=="multiply")return backdrop*source;
    if(mode=="screen")return backdrop+source-backdrop*source;
    if(mode=="overlay")return backdrop<=.5?2*backdrop*source:1-2*(1-backdrop)*(1-source);
    if(mode=="darken")return std::min(backdrop,source);
    if(mode=="lighten")return std::max(backdrop,source);
    if(mode=="color-dodge")return backdrop==0?0:source==1?1:std::min(1.0,backdrop/(1-source));
    if(mode=="color-burn")return backdrop==1?1:source==0?0:1-std::min(1.0,(1-backdrop)/source);
    if(mode=="hard-light")return source<=.5?2*backdrop*source:1-2*(1-backdrop)*(1-source);
    if(mode=="soft-light") {
        if(source<=.5)return backdrop-(1-2*source)*backdrop*(1-backdrop);
        const auto curve=backdrop<=.25?((16*backdrop-12)*backdrop+4)*backdrop:std::sqrt(backdrop);
        return backdrop+(2*source-1)*(curve-backdrop);
    }
    if(mode=="difference")return std::abs(backdrop-source);
    if(mode=="exclusion")return backdrop+source-2*backdrop*source;
    throw std::runtime_error("Missing independent blend oracle: "+mode);
}
void all_supported_blends_match_independent_channel_formulas() {
    const QColor backdrop(51,102,204),source(204,179,77);
    auto d=document();add(d,rectangle("backdrop",80,80,320,280,backdrop));
    add(d,rectangle("source-a",100,100,200,200,source));add(d,rectangle("source-b",140,120,200,200,source));
    group(d,"aggregate",{"source-a","source-b"});Fixture f(d);
    for(const auto* mode:{"normal","multiply","screen","overlay","darken","lighten","color-dodge","color-burn","hard-light","soft-light","difference","exclusion"}) {
        for(const double alpha:{1.0,.5}) {
            f.apply({SetCompositing{"aggregate",mode,false},Set{{"aggregate","","composite.opacity"},alpha}});
            const auto channel=[&](int b,int s) {
                const auto cb=b/255.0,cs=s/255.0;
                // Opaque backdrop: source-over is (1-a)*Cb + a*B(Cb,Cs).
                return qRound(255*((1-alpha)*cb+alpha*separable_blend(mode,cb,cs)));
            };
            const QColor expected(channel(backdrop.red(),source.red()),channel(backdrop.green(),source.green()),channel(backdrop.blue(),source.blue()));
            f.color(180,180,expected,std::string(mode)+" mixed-channel Group blend at opacity "+std::to_string(alpha),3);
        }
    }
    f.no_error();
}
void standard_blends_compose_two_transparent_layers() {
    const QColor backdrop(51,102,204,128),source(204,179,77,128);
    auto d=document(false);
    add(d,rectangle("backdrop",80,80,200,200,backdrop));
    add(d,rectangle("source",120,80,200,200,source));
    group(d,"aggregate",{"source"});
    Session session(d);
    constexpr double as=128.0/255.0,ab=128.0/255.0;
    const double ao=as+ab*(1-as);
    const auto expected_channel=[&](const std::string& mode,int b,int s) {
        const double cb=b/255.0,cs=s/255.0;
        // W3C §6 source-over with a separable blend and straight sRGB colors.
        const double premultiplied=as*(1-ab)*cs+as*ab*separable_blend(mode,cb,cs)+(1-as)*ab*cb;
        return static_cast<int>(std::lround(255*premultiplied/ao));
    };
    const auto same_layer_byte=[&](const QColor& observed,const QColor& authored) {
        // Premultiplied RGBA8 can change a straight RGB byte by one on readback.
        return observed.alpha()==authored.alpha()&&
            std::abs(observed.red()-authored.red())<=1&&
            std::abs(observed.green()-authored.green())<=1&&
            std::abs(observed.blue()-authored.blue())<=1;
    };
    for(const auto* mode:{"normal","multiply","screen","overlay","darken","lighten","color-dodge","color-burn",
        "hard-light","soft-light","difference","exclusion"}) {
        session.apply({SetCompositing{"aggregate",mode,false}},session.revision());
        const auto image=Canvas::render_artboard(session.document(),"composition","artboard",1,false);
        const auto overlap=image.pixelColor(180,180),backdrop_only=image.pixelColor(90,180);
        const auto source_only=image.pixelColor(300,180),empty=image.pixelColor(20,20);
        const QColor expected(expected_channel(mode,backdrop.red(),source.red()),
            expected_channel(mode,backdrop.green(),source.green()),
            expected_channel(mode,backdrop.blue(),source.blue()));
        check(overlap.alpha()==static_cast<int>(std::lround(255*ao))&&
            std::abs(overlap.red()-expected.red())<=3&&
            std::abs(overlap.green()-expected.green())<=3&&
            std::abs(overlap.blue()-expected.blue())<=3,
            std::string(mode)+" obeys independent W3C source-over and blend RGB/alpha with both layers transparent");
        check(same_layer_byte(backdrop_only,backdrop)&&same_layer_byte(source_only,source)&&empty.alpha()==0,
            std::string(mode)+" retains each isolated layer outside overlap and never blends against UI paper: backdrop="+
            std::to_string(backdrop_only.red())+"/"+std::to_string(backdrop_only.green())+"/"+
            std::to_string(backdrop_only.blue())+"/"+std::to_string(backdrop_only.alpha())+" source="+
            std::to_string(source_only.red())+"/"+std::to_string(source_only.green())+"/"+
            std::to_string(source_only.blue())+"/"+std::to_string(source_only.alpha())+" empty="+
            std::to_string(empty.alpha()));
    }
}
void open_mask_hole_and_fill_rule() {
    auto d=document();add(d,rectangle("target",80,80,300,280,Qt::green));
    auto source=rectangle("source",100,100,220,220,Qt::black);
    source.contours[0].closed=false;
    auto inner=rectangle("inner",160,160,100,100,Qt::black);source.contours.push_back(inner.contours[0]);
    auto visibility_driver=rectangle("visibility-driver",500,20,1,1,Qt::black);visibility_driver.visible=false;add(d,std::move(visibility_driver));
    source.visible=true;source.visibility_driver=Ref{"visibility-driver","","object.visible"};
    source.stack[0].parameters.at("a").literal=0;source.compositing.opacity.literal=0;add(d,std::move(source));
    d.objects.at("target").compositing.mask=GeometryMask{"mask","source",1,true,"evenodd"};Fixture f(d);
    check(object_visibility_state(f.session.document(),{"source","","object.visible"}).literal&&
        !object_visibility_state(f.session.document(),{"source","","object.visible"}).evaluated,
        "Mask source has authored true visibility linked to an evaluated false source");
    f.color(120,120,Qt::green,"An open mask contour closes for fill");f.color(210,210,Qt::white,"Even-odd mask preserves a same-winding hole");
    f.color(90,150,Qt::white,"Mask clears the full aggregate outside coverage");
    f.apply({SetMask{"target",GeometryMask{"mask","source",1,true,"nonzero"}}});
    f.color(210,210,Qt::green,"Nonzero rule fills same-winding nested contours");f.no_error();
}
void linked_visibility_controls_canvas_pixels() {
    auto d=document(false);
    auto hidden=rectangle("hidden-driver",20,20,20,20,Qt::blue);hidden.visible=false;add(d,std::move(hidden));
    auto target=rectangle("linked-target",100,100,200,200,Qt::red);target.visibility_driver=Ref{"hidden-driver","","object.visible"};add(d,std::move(target));
    Fixture f(d);f.color(180,180,QColor(250,250,250),"Canvas omits artwork whose visibility driver evaluates false");f.no_error();
}
void expression_visibility_projects_own_value_to_canvas() {
    auto d=document(false);
    auto member=rectangle("visibility-member",20,20,20,20,Qt::blue);add(d,std::move(member));
    group(d,"hidden-ancestor",{"visibility-member"});d.objects.at("hidden-ancestor").visible=false;
    auto target=rectangle("expression-target",100,100,200,200,Qt::red);
    target.visibility_expression=Expression{"ref(\"visibility-member\",\"\",\"object.visible\")",1};add(d,std::move(target));
    Fixture f(d);
    const auto own=evaluate_object_visibilities(f.session.document());
    check(!own.at("hidden-ancestor")&&own.at("visibility-member")&&own.at("expression-target"),
        "Object visibility expression reads the member's authored own value under a hidden ancestor");
    f.color(180,180,Qt::red,"Canvas keeps an expression target visible when its same-Composition own source is true");
    f.apply({SetObjectVisibilityExpression{{"expression-target","","object.visible"},
        {"!ref(\"visibility-member\",\"\",\"object.visible\")",1},true}});
    f.color(180,180,QColor(250,250,250),"Canvas applies negated Object visibility expression to target artwork");
    f.apply({SetVisibility{"visibility-member",false}});
    f.color(180,180,Qt::red,"Canvas re-evaluates a negated Object visibility expression after its source edit");
    f.no_error();
}
void linked_fill_rule_projects_to_canvas_and_svg() {
    auto d=document();auto target=rectangle("target",80,80,300,280,Qt::green);
    auto inner=rectangle("inner",160,160,100,100,Qt::green);target.contours.push_back(inner.contours.front());add(d,std::move(target));
    auto source=rectangle("source",420,80,120,120,Qt::black);source.visible=false;
    source.stack.front().fill_rule="evenodd";add(d,std::move(source));
    Fixture f(d);const Ref target_ref=operation_ref("target","target-fill","fill_rule");
    const Ref source_ref=operation_ref("source","source-fill","fill_rule");
    auto expected=d;expected.objects.at("target").stack.front().fill_rule="evenodd";
    f.apply({LinkFillRule{target_ref,source_ref}});
    check(fill_rule_property(f.session.document(),target_ref).literal=="nonzero"&&
        fill_rule_property(f.session.document(),target_ref).evaluated=="evenodd",
        "Linked Fill keeps target-authored nonzero while evaluating source even-odd");
    f.color(210,210,Qt::white,"Linked even-odd rule cuts a same-winding Canvas hole");
    f.color(120,120,Qt::green,"Linked rule preserves target Fill outside its hole");
    check(export_svg(f.session.document(),"composition","artboard")==export_svg(expected,"composition","artboard"),
        "SVG projects the same evaluated Fill rule as an unlinked even-odd twin");
    f.apply({OperationOptions{"source","source-fill","below","nonzero"}});
    expected.objects.at("target").stack.front().fill_rule="nonzero";
    f.color(210,210,Qt::green,"Changing the Fill source updates the Canvas hole");
    check(fill_rule_property(f.session.document(),target_ref).literal=="nonzero"&&
        fill_rule_property(f.session.document(),target_ref).evaluated=="nonzero"&&
        export_svg(f.session.document(),"composition","artboard")==export_svg(expected,"composition","artboard"),
        "Updated source rule drives both target evaluation and SVG projection");
    f.no_error();
}
void linked_mask_enabled_projects_to_canvas_and_svg() {
    auto d=document();add(d,rectangle("target",80,80,300,280,Qt::green));
    auto mask_source=rectangle("mask-source",100,100,120,120,Qt::black);mask_source.visible=false;add(d,std::move(mask_source));
    auto enabled_owner=rectangle("enabled-owner",420,20,40,40,Qt::black);enabled_owner.visible=false;
    enabled_owner.compositing.mask=GeometryMask{"source-mask","driver-geometry",1,false,"nonzero"};add(d,std::move(enabled_owner));
    auto driver_geometry=rectangle("driver-geometry",420,80,40,40,Qt::black);driver_geometry.visible=false;add(d,std::move(driver_geometry));
    d.objects.at("target").compositing.mask=GeometryMask{"target-mask","mask-source",1,false,"nonzero"};
    Fixture f(d);
    const auto target_ref=geometry_mask_enabled_ref("target","target-mask");
    const auto source_ref=geometry_mask_enabled_ref("enabled-owner","source-mask");
    const Expression expression{" ! ref ( \"enabled-owner\" , \"\" , \"mask.source-mask.enabled\" ) ",1};
    auto expected_enabled=d;expected_enabled.objects.at("target").compositing.mask->enabled=true;
    f.apply({SetMaskEnabledExpression{target_ref,expression,false}});
    const auto linked=geometry_mask_enabled_state(f.session.document(),target_ref);
    check(!linked.literal&&!linked.driver&&linked.expression==expression&&linked.evaluated,
        "A false authored target expression negates the false mask on a hidden source owner");
    f.color(130,130,Qt::green,"Evaluated true mask expression retains clipping inside source geometry");
    f.color(250,250,Qt::white,"Evaluated true mask expression clips outside source geometry");
    check(export_svg(f.session.document(),"composition","artboard")==export_svg(expected_enabled,"composition","artboard"),
        "SVG uses the evaluated expression bit like an authored enabled-mask twin");
    auto source_mask=*f.session.document().objects.at("enabled-owner").compositing.mask;source_mask.enabled=true;
    auto expected_bypassed=d;expected_bypassed.objects.at("enabled-owner").compositing.mask=source_mask;
    f.apply({SetMask{"enabled-owner",source_mask}});expected_bypassed.objects.at("target").compositing.mask->enabled=false;
    const auto bypassed=geometry_mask_enabled_state(f.session.document(),target_ref);
    const auto values=evaluate(f.session.document());
    const auto evaluated_scene=evaluate_scene(f.session.document(),"composition",values,evaluate_transforms(f.session.document(),values));
    check(!bypassed.literal&&!bypassed.driver&&bypassed.expression==expression&&!bypassed.evaluated&&
        !evaluated_scene.roots[1].mask,
        "A true source bypasses target clipping without erasing its authored mask or expression");
    f.color(250,250,Qt::green,"Evaluated false mask expression bypasses clipping on Canvas");
    check(export_svg(f.session.document(),"composition","artboard")==export_svg(expected_bypassed,"composition","artboard"),
        "SVG uses the evaluated false expression bit while retaining target mask authored state");
    f.no_error();
}
void alpha_mask_path_and_group_pixel_oracle() {
    auto d=document(false);
    add(d,rectangle("target-content",0,0,30,10,Qt::red));group(d,"target",{"target-content"});
    add(d,rectangle("zero",0,0,10,10,QColor(0,0,0,0)));
    add(d,rectangle("half",10,0,10,10,QColor(0,0,0,128)));
    add(d,rectangle("one",20,0,10,10,QColor(0,0,0,255)));
    group(d,"alpha-source",{"zero","half","one"});
    d.objects.at("alpha-source").visible=false;
    d.objects.at("target").compositing.mask=GeometryMask{"alpha-mask","alpha-source",1,true,
        "nonzero",std::nullopt,std::nullopt,"alpha",false};
    const auto pixels=Canvas::render_artboard(d,"composition","artboard",1,false);
    check(pixels.pixelColor(5,5).alpha()==0&&pixels.pixelColor(15,5).alpha()==128&&pixels.pixelColor(25,5).alpha()==255,
        "Hidden Alpha Group source yields exact zero, half and full RGBA coverage through visible Path children");

    auto hidden_child=d;hidden_child.objects.at("half").visible=false;
    const auto hidden_pixels=Canvas::render_artboard(hidden_child,"composition","artboard",1,false);
    check(hidden_pixels.pixelColor(15,5).alpha()==0&&hidden_pixels.pixelColor(25,5).alpha()==255,
        "Alpha source ignores only root visibility and respects hidden descendant visibility");

    auto opacity=d;opacity.objects.at("alpha-source").compositing.opacity.literal=.5;
    const auto opacity_pixels=Canvas::render_artboard(opacity,"composition","artboard",1,false);
    const int half_alpha=opacity_pixels.pixelColor(15,5).alpha(),full_alpha=opacity_pixels.pixelColor(25,5).alpha();
    check(half_alpha==64&&full_alpha==127,
        "Source Group opacity scales the isolated alpha projection before target opacity");
}
void luma_mask_srgb_pixel_oracle() {
    auto d=document(false);
    add(d,rectangle("target-content",0,0,80,10,QColor(0,0,0,255)));group(d,"target",{"target-content"});
    const std::array<QColor,6> colors{{QColor(255,0,0),QColor(54,54,54),QColor(0,255,0),
        QColor(0,0,255),QColor(255,255,255,0),QColor(255,255,255)}};
    const std::array<int,6> reference_expected{{54,54,182,18,0,128}};
    std::array<int,6> expected=reference_expected;
    std::array<int,6> source_alpha{{255,255,255,255,0,0}};
    const std::array<int,6> x_samples{{15,25,35,45,55,65}};
    std::vector<Id> children;
    for(std::size_t i=0;i<colors.size();++i) {
        const auto id="luma-source-"+std::to_string(i);
        auto source=rectangle(id,static_cast<double>(i*10),0,10,10,colors[i]);
        if(i==5)source.compositing.opacity.literal=.5;
        add(d,std::move(source));children.push_back(id);
    }
    group(d,"luma-source",children);d.objects.at("luma-source").visible=false;
    d.objects.at("luma-source").transform[4].literal=10;
    d.objects.at("target").compositing.mask=GeometryMask{"luma-mask","luma-source",1,true,
        "nonzero",std::nullopt,std::nullopt,"luma",false};

    auto source_only=d;source_only.objects.at("luma-source").visible=true;
    source_only.objects.at("target").visible=false;
    const auto source_projection=Canvas::render_artboard(source_only,"composition","artboard",1,false);
    source_alpha[5]=source_projection.pixelColor(x_samples[5],5).alpha();
    check(source_alpha[5]==127&&std::abs(source_alpha[5]-reference_expected[5])<=1,
        "Half-opacity white projects alpha 127, within one byte of the independent 128 reference");
    expected[5]=static_cast<int>(std::lround((0.2125*colors[5].red()+0.7154*colors[5].green()+
        0.0721*colors[5].blue())*source_alpha[5]/255.0));
    check(expected[5]==127&&std::abs(expected[5]-reference_expected[5])<=1,
        "Independent SVG coefficients derive the half-white mask byte from the observed source alpha");
    for(std::size_t i=0;i<colors.size();++i) {
        const auto pixel=source_projection.pixelColor(x_samples[i],5);
        const int numeric=static_cast<int>(std::lround((0.2125*colors[i].red()+0.7154*colors[i].green()+
            0.0721*colors[i].blue())*source_alpha[i]/255.0));
        check(numeric==expected[i],"Independent SVG luminance oracle has the fixed expected 8-bit value");
        const auto detail="fixture="+std::to_string(i)+" sample="+std::to_string(x_samples[i])+
            " rgba="+std::to_string(pixel.red())+"/"+std::to_string(pixel.green())+"/"+
            std::to_string(pixel.blue())+"/"+std::to_string(pixel.alpha());
        check(pixel.alpha()==source_alpha[i]&&
            (source_alpha[i]==0||pixel.red()==colors[i].red()),
            "Isolated RGBA fixture exposes the expected straight color and source alpha bytes: "+detail);
    }
    const auto pixels=Canvas::render_artboard(d,"composition","artboard",1,false);
    for(std::size_t i=0;i<expected.size();++i)
        check(pixels.pixelColor(x_samples[i],5).alpha()==expected[i],
            "Luma mask output alpha matches the independent sRGB red/gray/green/blue/transparent/half-alpha oracle");

    d.objects.at("target").compositing.mask->invert=true;
    const auto inverted=Canvas::render_artboard(d,"composition","artboard",1,false);
    for(std::size_t i=0;i<expected.size();++i)
        check(inverted.pixelColor(x_samples[i],5).alpha()==255-expected[i],
            "Luma inversion is applied after luminance and alpha multiplication");
    check(inverted.pixelColor(75,5).alpha()==255,
        "Inverted Luma fills target coverage outside the source projection");
}
void luma_source_internal_mask_and_effect() {
    auto d=document(false);
    add(d,rectangle("target-content",100,100,40,20,Qt::red));group(d,"target",{"target-content"});
    add(d,rectangle("gray",100,100,40,20,QColor(54,54,54)));
    group(d,"luma-source",{"gray"});d.objects.at("luma-source").visible=false;
    add(d,rectangle("matte",100,100,20,20,Qt::black));d.objects.at("matte").visible=false;
    d.objects.at("target").compositing.mask=GeometryMask{"outer-luma","luma-source",1,true,
        "nonzero",std::nullopt,std::nullopt,"luma",false};
    const auto unmasked=Canvas::render_artboard(d,"composition","artboard",1,false);
    check(unmasked.pixelColor(110,110).alpha()==54&&unmasked.pixelColor(130,110).alpha()==54,
        "Independent gray fixture produces 54/255 Luma coverage across the source Group");

    d.objects.at("luma-source").compositing.mask=GeometryMask{"inner-alpha","matte",1,true,
        "nonzero",std::nullopt,std::nullopt,"alpha",false};
    const auto clipped=Canvas::render_artboard(d,"composition","artboard",1,false);
    check(clipped.pixelColor(110,110).alpha()==54&&clipped.pixelColor(130,110).alpha()==0,
        "The source Group's internal Alpha mask clips its Luma projection before outer extraction");

    Session session(d);session.apply({AddOperation{"luma-source",default_operation("posterize","nect.group.posterize"),0}},0);
    const auto effected=Canvas::render_artboard(session.document(),"composition","artboard",1,false);
    check(effected.pixelColor(110,110).alpha()==0&&effected.pixelColor(130,110).alpha()==0,
        "Two-level Group Posterize turns gray 54 to black before Luma extraction without changing authored source");
}
void alpha_mask_image_source_pixel_oracle() {
    auto d=document(false);
    add(d,rectangle("target-content",90,90,30,30,Qt::red));group(d,"target",{"target-content"});
    const RasterPixels source_pixels{3,1,{0,0,0,0, 0,0,255,128, 255,0,0,255}};
    auto payload=make_raster(encode_raster_png(source_pixels));
    d.raster_assets.emplace("alpha-raster",RasterAsset{"alpha-raster","Alpha source","embedded","",payload});
    Object image;image.id="alpha-image";image.name="Alpha image";image.kind=Kind::image;
    image.image=ImageSource{"alpha-raster",Scalar{3,{}},Scalar{1,{}}};add(d,std::move(image));
    const Affine image_world{1,0,0,1,100,105};
    for(std::size_t i=0;i<6;++i)d.objects.at("alpha-image").transform[i].literal=image_world[i];
    group(d,"nested-image-group",{"alpha-image"});group(d,"alpha-image-source",{"nested-image-group"});
    d.objects.at("alpha-image-source").visible=false;
    d.objects.at("target").compositing.mask=GeometryMask{"image-alpha-mask","alpha-image-source",1,true,
        "nonzero",std::nullopt,std::nullopt,"alpha",false};
    const auto pixels=Canvas::render_artboard(d,"composition","artboard",1,false);
    const int zero_alpha=pixels.pixelColor(100,105).alpha(),half_alpha=pixels.pixelColor(101,105).alpha(),full_alpha=pixels.pixelColor(102,105).alpha();
    check(zero_alpha==0&&half_alpha==128&&full_alpha==255,
        "Nested Group/Image source projects its decoded RGBA alpha instead of its RGB color: "+
        std::to_string(zero_alpha)+"/"+std::to_string(half_alpha)+"/"+std::to_string(full_alpha));
}
void alpha_mask_invert_uses_target_bounds_and_source_world() {
    auto d=document(false);
    add(d,rectangle("target-content",100,100,100,100,Qt::red));group(d,"target",{"target-content"});
    add(d,rectangle("source-shape",0,0,30,30,Qt::black));group(d,"source-root",{"source-shape"});
    const Affine translated{1,0,0,1,120,120};
    for(std::size_t i=0;i<6;++i)d.objects.at("source-root").transform[i].literal=translated[i];
    d.objects.at("source-root").visible=false;
    d.objects.at("target").compositing.mask=GeometryMask{"invert-alpha-mask","source-root",1,true,
        "nonzero",std::nullopt,std::nullopt,"alpha",true};
    const auto inverted=Canvas::render_artboard(d,"composition","artboard",1,false);
    check(inverted.pixelColor(110,110).alpha()==255&&inverted.pixelColor(130,130).alpha()==0&&
        inverted.pixelColor(90,90).alpha()==0,
        "Inverted source transform reveals target outside source bounds and clears source coverage");

    d.objects.at("source-root").compositing.opacity.literal=0;
    d.objects.at("target").compositing.mask->invert=false;
    const auto zero_source=Canvas::render_artboard(d,"composition","artboard",1,false);
    check(zero_source.pixelColor(110,110).alpha()==0,
        "Zero-opacity hidden Alpha root produces an all-zero non-inverted mask");
    d.objects.at("target").compositing.mask->invert=true;
    const auto inverted_zero=Canvas::render_artboard(d,"composition","artboard",1,false);
    check(inverted_zero.pixelColor(110,110).alpha()==255&&inverted_zero.pixelColor(90,90).alpha()==0,
        "Inverting a zero-opacity source preserves target coverage and stays within target content bounds");
}
void repeated_mask_uses_external_world_transform() {
    auto d=document();add(d,rectangle("target",80,80,300,240,Qt::green));auto source=rectangle("source",100,100,40,80,Qt::black);
    source.visible=false;source.transform_parent="driver";
    auto repeat=default_operation("repeat","nect.shape.repeater");repeat.parameters.at("copies").literal=2;repeat.parameters.at("position_x").literal=100;
    source.stack.push_back(repeat);add(d,std::move(source));group(d,"driver",{});
    d.objects.at("driver").transform[4].literal=50;d.objects.at("driver").transform[5].literal=20;
    d.objects.at("target").compositing.mask=GeometryMask{"mask","source"};Fixture f(d);
    f.color(160,150,Qt::green,"Mask projects original geometry through explicit Transform Parent");
    f.color(260,150,Qt::green,"Mask includes final repeated geometry");f.color(220,150,Qt::white,"Repeated mask leaves its gap transparent");f.no_error();
}
void hidden_sources_do_not_hit_but_keep_direct_controls() {
    auto d=document();add(d,rectangle("target",80,80,300,280,Qt::green));auto source=rectangle("source",120,120,160,160,Qt::black);source.visible=false;add(d,std::move(source));
    d.objects.at("target").compositing.mask=GeometryMask{"mask","source"};Fixture f(d);
    f.click(180,180);check(f.canvas.selected_object=="target","Normal hit skips the hidden mask source above the target");
    f.canvas.set_selection({});f.click(90,180);check(f.canvas.selected_object=="paper","Clipped artwork does not hit through any geometry fallback");
    f.canvas.set_selection("source","source-p0");const auto before=f.session.document();
    const auto start=f.screen(120,120),end=f.screen(140,120);
    QTest::mousePress(&f.canvas,Qt::LeftButton,Qt::NoModifier,start,1);QTest::mouseMove(&f.canvas,end,1);
    QTest::mouseRelease(&f.canvas,Qt::LeftButton,Qt::NoModifier,end,1);QApplication::processEvents();
    check(f.session.revision()==1&&f.commits==1,"Tree-selected hidden source has a real one-gesture point edit");
    check(evaluate(f.session.document()).at({"source","source-p0","x"})==140&&!f.session.document().objects.at("source").visible,"Hidden source drag edits geometry without changing visibility");
    f.session.undo(f.session.revision());check(f.session.document()==before,"Hidden source edit is one complete Undo");f.canvas.refresh();
    f.apply({SetMask{"target",std::nullopt},SetVisibility{"target",false}});f.canvas.set_selection({});f.click(180,180);
    check(f.canvas.selected_object=="paper","Normal hit skips an explicitly hidden leaf");f.no_error();
}
void mask_outline_is_separate_from_inherited_selection() {
    auto d=document();add(d,rectangle("target",150,150,100,100,Qt::green));auto source=rectangle("source",70,70,260,260,Qt::black);source.visible=false;add(d,std::move(source));
    group(d,"group",{"target","source"});d.objects.at("group").compositing.mask=GeometryMask{"mask","source"};Fixture f(d);
    f.canvas.set_selection("group");f.canvas.set_show_mask_outline(false);const auto hidden=f.image();
    for(const int x:{100,160,220,280})check(f.pixel(hidden,x,70)==QColor(Qt::white),"Selecting a Group does not reveal its hidden source as a normal accent outline");
    f.canvas.set_show_mask_outline(true);const auto shown=f.image();int changed=0;
    for(int x=90;x<310;++x)for(int y=68;y<=72;++y)if(f.pixel(hidden,x,y)!=f.pixel(shown,x,y))++changed;
    check(changed>50,"Show mask outline draws a separate faint outline for the selected target");
    f.canvas.set_show_mask_outline(false);f.canvas.set_selection("source","source-p0");
    check(f.pixel(f.image(),70,70)!=QColor(Qt::white),"Direct source selection keeps editable point overlays with mask outline disabled");f.no_error();
}
void same_pixels(const QImage& actual,const QImage& reference,const std::string& reason) {
    check(actual.size()==reference.size()&&actual.devicePixelRatio()==reference.devicePixelRatio(),reason+" image dimensions");
    int maximum=0;
    for(int y=0;y<actual.height();++y)for(int x=0;x<actual.width();++x) {
        const auto a=actual.pixelColor(x,y),b=reference.pixelColor(x,y);
        maximum=std::max({maximum,std::abs(a.red()-b.red()),std::abs(a.green()-b.green()),std::abs(a.blue()-b.blue())});
    }
    check(maximum<=3,reason+": maximum channel difference "+std::to_string(maximum));
}
void shift_view(Fixture& fixture) {
    auto& canvas=fixture.canvas;
    const auto send=[&](QEvent::Type type,QPointF position,Qt::MouseButton button,Qt::MouseButtons buttons) {
        QMouseEvent event(type,position,QPointF(canvas.mapToGlobal(position.toPoint()))+position-position.toPoint(),button,buttons,Qt::NoModifier);
        QApplication::sendEvent(&canvas,&event);
    };
    send(QEvent::MouseButtonPress,{370.25,290.5},Qt::MiddleButton,Qt::MiddleButton);
    send(QEvent::MouseMove,{248.6,350.9},Qt::NoButton,Qt::MiddleButton);
    send(QEvent::MouseButtonRelease,{248.6,350.9},Qt::MiddleButton,Qt::NoButton);
    const QPointF position(410.3,330.8);
    QWheelEvent wheel(position,QPointF(canvas.mapToGlobal(position.toPoint()))+position-position.toPoint(),{},QPoint(0,90),Qt::NoButton,Qt::NoModifier,Qt::ScrollUpdate,false);
    QApplication::sendEvent(&canvas,&wheel);QApplication::processEvents();
}
void cropped_unmasked_scope_preserves_stroke_gradient_and_repeater() {
    auto d=document();auto path=rectangle("stroke",100,100,110,150,Qt::black);path.contours[0].closed=false;
    path.contours[0].points.resize(3);
    path.contours[0].points[1].x.literal=180;path.contours[0].points[1].y.literal=110;
    path.contours[0].points[2].x.literal=120;path.contours[0].points[2].y.literal=125;
    auto stroke=default_operation("wide-stroke","nect.paint.stroke");stroke.parameters.at("width").literal=24;
    Gradient gradient;gradient.id="stroke-gradient";gradient.start_x.literal=100;gradient.start_y.literal=100;
    gradient.end_x.literal=200;gradient.end_y.literal=160;
    GradientStop a;a.id="stop-a";a.rgba[0].literal=1;GradientStop b;b.id="stop-b";b.offset.literal=1;b.rgba[2].literal=1;
    gradient.stops={a,b};stroke.gradient=gradient;
    auto repeat=default_operation("stroke-repeat","nect.shape.repeater");repeat.parameters.at("copies").literal=3;
    repeat.parameters.at("position_x").literal=90;repeat.parameters.at("position_y").literal=25;
    repeat.parameters.at("scale_x").literal=1.2;repeat.parameters.at("scale_y").literal=.8;repeat.parameters.at("rotation").literal=17;
    path.stack={stroke,repeat};const Affine matrix{1.2,.18,-.25,.8,40,20};
    for(std::size_t i=0;i<6;++i)path.transform[i].literal=matrix[i];add(d,std::move(path));group(d,"group",{"stroke"});
    Fixture f(d);
    for(int view=0;view<3;++view) {
        if(view)shift_view(f);
        f.apply({SetCompositing{"group","normal",false}});const auto reference=f.image();
        f.apply({SetCompositing{"group","normal",true}});
        same_pixels(f.image(),reference,"Cropped isolation preserves transformed wide miter strokes, gradients and Repeaters at view "+std::to_string(view));
    }
    f.no_error();
}
void cropped_mask_scope_preserves_world_alignment() {
    auto d=document();add(d,rectangle("target",-1000,-1000,3000,3000,Qt::green));
    auto source=rectangle("source",80,60,260,200,Qt::green);source.visible=false;source.transform_parent="driver";add(d,std::move(source));
    group(d,"group",{"target","source"});d.objects.at("group").compositing.mask=GeometryMask{"mask","source"};
    group(d,"driver",{});const Affine matrix{.9,.3,-.1,1.2,77,-40};
    for(std::size_t i=0;i<6;++i)d.objects.at("driver").transform[i].literal=matrix[i];Fixture f(d);
    for(int view=0;view<3;++view) {
        if(view)shift_view(f);
        const auto clipped=f.image();
        f.apply({SetMask{"group",std::nullopt},SetVisibility{"target",false},SetVisibility{"source",true}});
        same_pixels(clipped,f.image(),"Cropped world-space mask equals its directly painted source at view "+std::to_string(view));
        f.apply({SetVisibility{"target",true},SetVisibility{"source",false},SetMask{"group",GeometryMask{"mask","source"}}});
    }
    f.no_error();
}
void path_deform_mask_and_paint_share_group_geometry_world() {
    auto d=document();add(d,rectangle("target",0,0,640,480,Qt::green));
    auto source=rectangle("deform-source",10,10,40,30,Qt::white);source.visible=false;
    source.transform[4].literal=100;source.transform[5].literal=100;add(d,source);
    group(d,"deform-group",{"deform-source"});
    const Affine transform{2,0,0,1.5,90,70};
    for(std::size_t i=0;i<6;++i)d.objects.at("deform-group").transform[i].literal=transform[i];
    Object guide;guide.id="deform-guide";guide.name="Guide";guide.visible=false;
    Point first;first.id="deform-guide-a";Point last;last.id="deform-guide-b";last.x.literal=640;
    guide.contours={{"deform-guide-contour",false,{first,last}}};add(d,guide);
    GroupPathFollow relation;relation.id="pixel-deform-relation";relation.path="deform-guide";relation.contour="deform-guide-contour";
    relation.mode="deform";relation.items={{"deform-source",{0,0,true}}};d.objects.at("deform-group").path_follow=relation;
    d.objects.at("target").compositing.mask=GeometryMask{"deform-mask","deform-source"};Fixture f(d);
    f.color(120,120,Qt::green,"Deformed Geometry mask consumes the leaf-to-Group coordinates and Group world once");
    f.color(160,120,Qt::white,"Deformed mask excludes outside derived geometry");
    auto mask=*f.session.document().objects.at("target").compositing.mask;mask.mode="alpha";
    f.apply({SetMask{"target",mask}});f.color(120,120,Qt::green,"Alpha mask uses the same canonical deformed paint projection");
    mask.mode="luma";f.apply({SetMask{"target",mask}});
    f.color(120,120,Qt::green,"White deformed Luma paint matches the Geometry/Alpha oracle");
    f.color(310,290,Qt::white,"Structural child world is not applied a second time to deformed paint");f.no_error();
}
void render_limits_remain_visible() {
    auto d=document(false);add(d,rectangle("target",100,100,100,100,Qt::red));Id child="target";
    for(int i=0;i<17;++i){const auto id="group-"+std::to_string(i);group(d,id,{child});d.objects.at(id).compositing.isolated=true;child=id;}
    Fixture f(d);check(f.last_error.startsWith("RENDER_LIMIT:"),"Excessive isolation reports an explicit renderer limit");
    const auto first=f.image();const auto second=f.image();const auto sample=[&](const QImage& image){return image.pixelColor(qRound(15*image.devicePixelRatio()),qRound(50*image.devicePixelRatio()));};
    check(sample(first)==sample(second)&&sample(first).red()>sample(first).green()*1.5,"The rendering failure badge remains visible on subsequent paints");
}
}
int main(int argc,char** argv) {
    // Pixel and interaction correctness only; no compositor/presentation timing claims.
    if(qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))qputenv("QT_QPA_PLATFORM","offscreen");
    QApplication application(argc,argv);
    try {
        group_opacity_is_applied_once();group_posterize_uses_independent_postcomposite_pixel_oracle();pass_through_and_isolation_have_distinct_backdrops();blend_alpha_and_transparent_root();
        all_supported_blends_match_independent_channel_formulas();standard_blends_compose_two_transparent_layers();
        open_mask_hole_and_fill_rule();alpha_mask_path_and_group_pixel_oracle();luma_mask_srgb_pixel_oracle();
        luma_source_internal_mask_and_effect();
        alpha_mask_image_source_pixel_oracle();alpha_mask_invert_uses_target_bounds_and_source_world();
        linked_visibility_controls_canvas_pixels();expression_visibility_projects_own_value_to_canvas();linked_fill_rule_projects_to_canvas_and_svg();
        linked_mask_enabled_projects_to_canvas_and_svg();repeated_mask_uses_external_world_transform();hidden_sources_do_not_hit_but_keep_direct_controls();
        mask_outline_is_separate_from_inherited_selection();cropped_unmasked_scope_preserves_stroke_gradient_and_repeater();
        cropped_mask_scope_preserves_world_alignment();path_deform_mask_and_paint_share_group_geometry_world();render_limits_remain_visible();
        std::cout<<"PASS "<<checks<<" compositing Canvas pixel and interaction checks\n";return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL after "<<checks<<" checks: "<<error.what()<<'\n';return 1;}
}

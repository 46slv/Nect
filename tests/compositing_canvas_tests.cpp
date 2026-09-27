#include "canvas.hpp"
#include "nect/io.hpp"

#include <QApplication>
#include <QImage>
#include <QMouseEvent>
#include <QTest>
#include <QWheelEvent>
#include <algorithm>
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
void open_mask_hole_and_fill_rule() {
    auto d=document();add(d,rectangle("target",80,80,300,280,Qt::green));
    auto source=rectangle("source",100,100,220,220,Qt::black);
    source.contours[0].closed=false;
    auto inner=rectangle("inner",160,160,100,100,Qt::black);source.contours.push_back(inner.contours[0]);
    source.visible=false;source.stack[0].parameters.at("a").literal=0;source.compositing.opacity.literal=0;add(d,std::move(source));
    d.objects.at("target").compositing.mask=GeometryMask{"mask","source",1,true,"evenodd"};Fixture f(d);
    f.color(120,120,Qt::green,"An open mask contour closes for fill");f.color(210,210,Qt::white,"Even-odd mask preserves a same-winding hole");
    f.color(90,150,Qt::white,"Mask clears the full aggregate outside coverage");
    f.apply({SetMask{"target",GeometryMask{"mask","source",1,true,"nonzero"}}});
    f.color(210,210,Qt::green,"Nonzero rule fills same-winding nested contours");f.no_error();
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
        all_supported_blends_match_independent_channel_formulas();
        open_mask_hole_and_fill_rule();linked_fill_rule_projects_to_canvas_and_svg();repeated_mask_uses_external_world_transform();hidden_sources_do_not_hit_but_keep_direct_controls();
        mask_outline_is_separate_from_inherited_selection();cropped_unmasked_scope_preserves_stroke_gradient_and_repeater();
        cropped_mask_scope_preserves_world_alignment();render_limits_remain_visible();
        std::cout<<"PASS "<<checks<<" compositing Canvas pixel and interaction checks\n";return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL after "<<checks<<" checks: "<<error.what()<<'\n';return 1;}
}

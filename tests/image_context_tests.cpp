#include "window.hpp"
#include "visual_style.hpp"
#include "nect/io.hpp"
#include <QAction>
#include <QApplication>
#include <QDockWidget>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QPointer>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>
#include <iostream>
using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
void events(){QApplication::processEvents();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QTest::qWait(20);}
template<class T>T* named(QObject& scope,const char* name){for(auto* p:scope.findChildren<T*>(name))if(p->isVisible())return p;throw std::runtime_error(std::string("Missing visible control: ")+name);}
bool same(const Session& a,const Session& b){return a.document()==b.document()&&a.preview_document()==b.preview_document()&&encode(a.document())==encode(b.document())&&a.history()==b.history()&&a.revision()==b.revision()&&a.gesture_generation()==b.gesture_generation()&&a.gesture_active()==b.gesture_active();}
std::vector<unsigned char> png(unsigned char r,unsigned char g,unsigned char b,unsigned width=40,unsigned height=20){RasterPixels p{width,height,{}};for(unsigned i=0;i<width*height;++i)p.rgba.insert(p.rgba.end(),{r,g,b,255});return encode_raster_png(p);}
void write(const QString& path,const std::vector<unsigned char>& bytes){QFile f(path);check(f.open(QIODevice::WriteOnly),"Open owned PNG");check(f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size())==qsizetype(bytes.size()),"Write complete owned PNG");}
Document fixture(const QString& path,const std::vector<unsigned char>& bytes){
    Session s(empty_document("image-context","comp","art"));
    auto other=default_primitive("other-source","nect.shape.rectangle");other.parameters.at("center_x").literal=520;other.parameters.at("center_y").literal=440;other.parameters.at("width").literal=30;other.parameters.at("height").literal=24;
    auto blue=default_operation("other-fill","nect.paint.fill");blue.parameters.at("b").literal=1;
    s.apply({AddRasterAsset{{"asset","Shared reference","linked",path.toStdString(),make_raster(bytes)}},CreateImage{"comp","","image","Reference image",{"asset",{120},{60}}},Set{{"image","","transform.tx"},60},Set{{"image","","transform.ty"},80},CreateImage{"comp","","copy","Shared second placement",{"asset",{80},{50}}},Set{{"copy","","transform.tx"},400},Set{{"copy","","transform.ty"},300},CreatePrimitive{"comp","","other","Other artwork",other},AddOperation{"other",blue,0}},0);
    auto d=s.document();d.compositions.front().artboards.front().width=640;d.compositions.front().artboards.front().height=480;auto art=d.compositions.front().artboards.front();art.id="other-art";art.name="Other Artboard";art.x=700;d.compositions.front().artboards.push_back(art);return d;
}
void evidence(Window& w,const QString& suffix){const auto path=qEnvironmentVariable("NECT_IMAGE_CONTEXT_EVIDENCE");if(!path.isEmpty())check(w.grab().save(path+suffix),"Window evidence saved");}
void select(Window& w){auto* dock=named<QDockWidget>(w,"structure");dock->show();dock->raise();events();auto* tree=dock->findChild<QTreeWidget*>();check(tree,"Structure tree");tree->expandAll();QTreeWidgetItem* row=nullptr;for(QTreeWidgetItemIterator it(tree);*it;++it)if((*it)->data(0,Qt::UserRole).toString()=="image"&&(*it)->data(0,Qt::UserRole+1).toString().isEmpty())row=*it;check(row,"Exact Image row");tree->scrollToItem(row);events();QTest::mouseClick(tree->viewport(),Qt::LeftButton,Qt::NoModifier,tree->visualItemRect(row).center());events();check(w.canvas->selected_object=="image","Exact Image selected");dock=named<QDockWidget>(w,"properties");dock->show();dock->raise();events();}
void reveal(Window& w,QWidget* c){auto* area=named<QScrollArea>(w,"inspector-scroll");check(c->isVisible()&&c->isEnabled(),"Actual enabled Image control");area->verticalScrollBar()->setValue(c->mapTo(area->widget(),QPoint()).y()-area->viewport()->height()/2);events();std::cout<<"Reach "<<c->objectName().toStdString()<<" viewport="<<area->viewport()->width()<<" content="<<area->widget()->width()<<" min="<<area->widget()->minimumSizeHint().width()<<" hmax="<<area->horizontalScrollBar()->maximum()<<std::endl;
    const bool fits=area->horizontalScrollBar()->maximum()==0&&area->viewport()->rect().contains(QRect(c->mapTo(area->viewport(),QPoint()),c->size()))&&c->visibleRegion().contains(c->rect());
    if(!fits){for(auto* x:area->widget()->findChildren<QWidget*>())if(x->minimumSizeHint().width()>area->viewport()->width()-60)std::cout<<"Minimum owner "<<x->metaObject()->className()<<" "<<x->objectName().toStdString()<<" min="<<x->minimumSizeHint().width()<<std::endl;evidence(w,".failure.png");}check(fits,"Image control reachable with vertical scrolling only");
}
QLineEdit* field(Window& w,const char* name){const auto ref=QJsonDocument(QJsonObject{{"object","image"},{"point",""},{"field",name}}).toJson(QJsonDocument::Compact);for(auto* p:w.findChildren<QLineEdit*>())if(p->isVisible()&&p->property("nect-reference").toByteArray()==ref)return p;throw std::runtime_error("Missing Image field");}
void click(Window& w,const char* name){events();auto* c=named<QPushButton>(w,name);reveal(w,c);QTest::mouseClick(c,Qt::LeftButton);events();}
void history(Window& w,const QString& text){for(auto* a:w.findChildren<QAction*>())if(a->text()==text){a->trigger();events();return;}throw std::runtime_error("Missing history action");}
void canonical(Window& w,Session& expected){check(same(w.host.session,expected),"Action equals complete canonical Session/native/history/preview");history(w,"Undo");expected.undo(expected.revision());check(same(w.host.session,expected),"Undo full equality");history(w,"Redo");expected.redo(expected.revision());check(same(w.host.session,expected),"Redo full equality");}
void number(Window& w,Session& expected,const char* name,const char* text,double value){auto* c=field(w,name);reveal(w,c);QTest::mouseClick(c,Qt::LeftButton);QTest::keyClick(c,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(c,text);check(same(w.host.session,expected),"Image numeric draft fully Session neutral");QTest::keyClick(c,Qt::Key_Return);events();expected.apply({EditProperties{{{"image","",name}},value,false}},expected.revision());canonical(w,expected);check(evaluate(w.host.session.document()).at({"image","",name})==value,"Evaluated Image dimension");}
void painted(Window& w,const Session& expected,const QColor& color,const QString& suffix){const auto image=Canvas::render_artboard(w.host.session.document(),"comp","art",1,false);check(image==Canvas::render_artboard(expected.document(),"comp","art",1,false),"Paint equals independent canonical state");check(image.pixelColor(65,85)==color&&image.pixelColor(405,305)==color&&image.pixelColor(520,440)==QColor(Qt::blue)&&image.pixelColor(20,20).alpha()==0,"Both shared placements paint accepted pixels; unrelated artwork retained");events();const auto canvas=w.canvas->grab().toImage();const auto sample=[&](double x,double y){const QPoint p(qRound(w.canvas->width()/2.0+(x-320)*w.canvas->zoom()),qRound(w.canvas->height()/2.0+(y-240)*w.canvas->zoom()));return canvas.pixelColor(qRound(p.x()*canvas.devicePixelRatio()),qRound(p.y()*canvas.devicePixelRatio()));};check(sample(90,105)==color&&sample(430,325)==color&&sample(520,440)==QColor(Qt::blue),"Actual Canvas paints accepted/shared/other pixels");const auto path=qEnvironmentVariable("NECT_IMAGE_CONTEXT_EVIDENCE");if(!path.isEmpty())check(image.save(path+suffix+".artwork.png"),"Artwork evidence saved");evidence(w,suffix+".png");}
void draft_fit(const QString& scratch) {
    const auto path=scratch+"/draft-fit-reference.png";const auto original=png(200,100,40);write(path,original);
    const auto initial=fixture(path,original);Window w(scratch+"/draft-fit-recovery");w.host.session=Session(initial);
    w.host.session_id="image-draft-fit-session";w.host.edited();w.show();events();w.canvas->fit_artboard();events();
    Session expected(initial);select(w);const auto canvas_width=w.canvas->width(),dock_width=named<QDockWidget>(w,"properties")->width();
    auto* fit=named<QPushButton>(w,"image-fit-width");reveal(w,fit);auto* width=field(w,"image.width");reveal(w,width);
    auto* area=named<QScrollArea>(w,"inspector-scroll");
    check(area->viewport()->rect().contains(QRect(fit->mapTo(area->viewport(),QPoint()),fit->size())),"Fit and Width simultaneously reachable in standard pane");
    QTest::mouseClick(width,Qt::LeftButton);QTest::keyClick(width,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(width,"160");
    check(width->hasFocus()&&width->isModified()&&same(w.host.session,expected),"Focused Image draft remains complete Session neutral before first action click");
    const QPointer<QLineEdit> pending_width=width;
    QTest::mousePress(fit,Qt::LeftButton);QTest::mouseRelease(fit,Qt::LeftButton,Qt::NoModifier,QPoint(-4,-4));events();
    check(pending_width&&pending_width->isModified()&&pending_width->text()=="160"&&same(w.host.session,expected),"Cancelled Fit pointer press preserves pending dimension draft and complete Session");
    evidence(w,".draft-fit-before.png");QTest::mouseClick(fit,Qt::LeftButton);events();
    std::cout<<"First Fit click width="<<w.host.session.document().objects.at("image").image->width.literal
             <<" height="<<w.host.session.document().objects.at("image").image->height.literal<<" revision="<<w.host.session.revision()<<std::endl;
    evidence(w,".draft-fit-after.png");
    expected.apply({Set{{"image","","image.width"},640},Set{{"image","","image.height"},320}},expected.revision());
    check(same(w.host.session,expected)&&w.host.session.can_undo()==expected.can_undo()&&w.host.session.can_redo()==expected.can_redo(),
        "First direct Fit click supersedes unfinished dimensions in one canonical active-Artboard/source-aspect transaction with full Session/history equality");
    auto protected_source=initial;protected_source.objects.at("image").image->width.literal=640;protected_source.objects.at("image").image->height.literal=320;
    check(w.host.session.document()==protected_source&&encode(w.host.session.document())==encode(protected_source),
        "Draft-to-Fit changes only intended Image dimensions; complete shared asset/other placement/transforms/paint/Artboards retained");
    painted(w,expected,QColor(200,100,40),".draft-fit");
    history(w,"Undo");expected.undo(expected.revision());check(same(w.host.session,expected)&&w.host.session.document()==initial,"Fit Undo restores full initial source without a transient discarded-draft entry");
    history(w,"Redo");expected.redo(expected.revision());check(same(w.host.session,expected),"Fit Redo full equality");
    check(w.canvas->width()==canvas_width&&named<QDockWidget>(w,"properties")->width()==dock_width,"Draft-to-Fit preserves standard pane and Canvas widths");
    const auto native=scratch+"/draft-fit.nect";w.host.save(native);check(same(w.host.session,expected),"Draft-to-Fit native save full Session neutral");w.host.changed={};w.hide();
    Window cold(scratch+"/draft-fit-cold");cold.host.open(native);cold.show();events();cold.canvas->fit_artboard();events();select(cold);Session reopened(expected.document());
    check(same(cold.host.session,reopened),"Fresh native Window restores full independent fitted Image source");
    reveal(cold,field(cold,"image.width"));check(field(cold,"image.width")->text().toDouble()==640&&field(cold,"image.height")->text().toDouble()==320,
        "Cold source controls read canonical fitted dimensions");painted(cold,reopened,QColor(200,100,40),".draft-fit-cold");cold.host.changed={};cold.hide();
}
void draft_fit_guards(const QString& scratch) {
    const auto path=scratch+"/guard-reference.png";const auto original=png(200,100,40);write(path,original);
    Window resumed(scratch+"/cancelled-draft-recovery");resumed.host.session=Session(fixture(path,original));resumed.host.session_id="image-cancelled-fit-session";
    resumed.host.edited();resumed.show();events();select(resumed);Session pending=resumed.host.session;
    auto* fit=named<QPushButton>(resumed,"image-fit-width");reveal(resumed,fit);auto* width=field(resumed,"image.width");reveal(resumed,width);
    QTest::mouseClick(width,Qt::LeftButton);QTest::keyClick(width,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(width,"160");
    QTest::mousePress(fit,Qt::LeftButton);QTest::mouseRelease(fit,Qt::LeftButton,Qt::NoModifier,QPoint(-4,-4));events();
    check(same(resumed.host.session,pending)&&width->isModified()&&width->text()=="160","Cancelled Fit keeps a real pending source draft");
    QTest::mouseClick(width,Qt::LeftButton);QTest::keyClick(width,Qt::Key_Return);events();
    pending.apply({EditProperties{{{"image","","image.width"}},160,false}},pending.revision());
    canonical(resumed,pending);check(evaluate(resumed.host.session.document()).at({"image","","image.width"})==160,
        "Retained cancelled-click draft commits through ordinary canonical Enter and complete UndoRedo");resumed.host.changed={};resumed.hide();
    Window w(scratch+"/guard-recovery");w.host.session=Session(fixture(path,original));w.host.session_id="image-fit-guard-session";
    w.host.edited();w.show();events();select(w);
    auto* stale_fit=named<QPushButton>(w,"image-fit-width");
    w.host.session.apply({Set{{"copy","","image.height"},65}},w.host.session.revision());const Session external=w.host.session;
    w.host.edited();stale_fit->click();check(same(w.host.session,external),"Old Fit callback rejects genuine external revision and preserves complete incoming source/history");events();
    w.host.session.begin_gesture(w.host.session.revision());w.host.edited();events();
    auto* preview_fit=named<QPushButton>(w,"image-fit-width");const Session preview=w.host.session;
    preview_fit->click();check(same(w.host.session,preview),"Fit preserves another owner's active gesture/full preview/native/history");
    w.host.session.cancel_gesture();const Session cancelled=w.host.session;
    preview_fit->click();check(same(w.host.session,cancelled),"Preview-born Fit stays ineligible after cancellation at the same revision/generation");
    w.host.edited();events();Session expected=w.host.session;click(w,"image-fit-width");
    expected.apply({Set{{"image","","image.width"},640},Set{{"image","","image.height"},320}},expected.revision());
    canonical(w,expected);w.host.changed={};w.hide();
    auto driven=fixture(path,original);Session driven_source(driven);
    driven_source.apply({Link{{"image","","image.width"},{{"copy","","image.width"},1,0,"copy_local_value"}}},driven_source.revision());
    Window linked(scratch+"/driven-recovery");linked.host.session=driven_source;linked.host.session_id="image-fit-driven-session";
    linked.host.edited();linked.show();events();select(linked);click(linked,"image-fit-width");
    check(same(linked.host.session,driven_source),"Driven dimension Fit refuses atomically without partial height/source/history changes");linked.host.changed={};linked.hide();
}
}
int main(int argc,char** argv){QApplication app(argc,argv);app.setStyle("Fusion");app.setStyleSheet(application_style_sheet());QTemporaryDir scratch(qEnvironmentVariable("NECT_IMAGE_CONTEXT_SCRATCH",QDir::tempPath())+"/image-context-XXXXXX");QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,scratch.path());app.setOrganizationName("NectTest");app.setApplicationName("ImageContext");
    if(argc>1&&std::string(argv[1])=="--draft-fit")try{check(scratch.isValid(),"Owned draft-fit scratch");draft_fit(scratch.path());draft_fit_guards(scratch.path());std::cout<<"PASS "<<checks<<" Image direct draft-to-Fit click/cancelled press/genuine context guards/full source/History/native; physical input NOT_RUN\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
    try{check(scratch.isValid(),"Owned scratch");const auto path=scratch.filePath("shared-reference-v01.png");const auto original=png(200,100,40),replacement=png(30,190,220,30,30);write(path,original);Window w(scratch.filePath("recovery"));w.host.session=Session(fixture(path,original));w.host.session_id="image-context-session";w.host.edited();w.show();events();w.canvas->fit_artboard();events();Session expected=w.host.session;select(w);check(same(w.host.session,expected),"Selection fully Session neutral");const auto canvas_width=w.canvas->width(),dock_width=named<QDockWidget>(w,"properties")->width();
        number(w,expected,"image.width","160",160);number(w,expected,"image.height","90",90);click(w,"image-fit-width");expected.apply({Set{{"image","","image.width"},640},Set{{"image","","image.height"},320}},expected.revision());canonical(w,expected);painted(w,expected,QColor(200,100,40),".fit");
        write(path,replacement);click(w,"image-check-link");check(same(w.host.session,expected),"Check changed link preserves complete Session and accepted bytes");check(named<QLabel>(w,"image-link-status")->text().startsWith("changed"),"Changed file observation visible");painted(w,expected,QColor(200,100,40),".changed");
        click(w,"image-reload");auto asset=expected.document().raster_assets.at("asset");asset.payload=make_raster(replacement);expected.apply({ReplaceRasterAsset{asset}},expected.revision());canonical(w,expected);check(expected.document().objects.at("image").image->width.literal==640&&expected.document().objects.at("image").image->height.literal==320&&expected.document().objects.at("copy").image->width.literal==80&&expected.document().objects.at("copy").image->height.literal==50,"Reload changes aspect/pixels and preserves both authored display sizes");painted(w,expected,QColor(30,190,220),".reloaded");
        const auto linked_saved=expected.document();const auto linked=scratch.filePath("linked.nect");w.host.save(linked);check(same(w.host.session,expected),"Linked native save fully Session neutral");const auto linked_fixture=scratch.filePath("linked-readback.nect");check(QFile::copy(linked,linked_fixture),"Freeze owned linked native readback before later autosave");check(QFile::remove(path),"Remove only owned link");click(w,"image-check-link");check(same(w.host.session,expected),"Missing link check preserves accepted pixels/full Session");check(named<QLabel>(w,"image-link-status")->text().startsWith("missing"),"Missing link observation visible");painted(w,expected,QColor(30,190,220),".missing");
        click(w,"image-embed");asset.mode="embedded";asset.locator.clear();expected.apply({ReplaceRasterAsset{asset}},expected.revision());canonical(w,expected);painted(w,expected,QColor(30,190,220),".embedded");check(w.canvas->width()==canvas_width&&named<QDockWidget>(w,"properties")->width()==dock_width,"Standard pane and Canvas widths retained");const auto native=scratch.filePath("embedded.nect");w.host.save(native);check(same(w.host.session,expected),"Embedded native save fully Session neutral");w.host.changed={};w.hide();
        Window cold(scratch.filePath("cold"));cold.host.open(linked_fixture);cold.show();events();cold.canvas->fit_artboard();events();select(cold);check(cold.host.session.document().objects==expected.document().objects,"Linked fresh Window preserves complete objects/IDs/display sizes");check(cold.host.session.document()==linked_saved&&encode(cold.host.session.document())==encode(linked_saved),"Linked fresh native readback full document including missing link/accepted payload");Session reopened=cold.host.session;check(cold.host.asset_status("asset").value("state").toString()=="unchecked","Cold linked state avoids implicit filesystem acceptance");reveal(cold,field(cold,"image.width"));check(field(cold,"image.width")->text().toDouble()==640&&field(cold,"image.height")->text().toDouble()==320,"Fresh dimension controls read retained source");click(cold,"image-check-link");check(same(cold.host.session,reopened),"Fresh missing-link check fully Session neutral");painted(cold,reopened,QColor(30,190,220),".cold-linked");cold.host.changed={};cold.hide();
        Window embedded(scratch.filePath("embedded-cold"));embedded.host.open(native);embedded.show();events();embedded.canvas->fit_artboard();events();select(embedded);check(embedded.host.session.document()==expected.document()&&encode(embedded.host.session.document())==encode(expected.document()),"Embedded fresh native full document/payload/order readback");Session embedded_expected=embedded.host.session;painted(embedded,embedded_expected,QColor(30,190,220),".cold-embedded");embedded.host.changed={};embedded.hide();std::cout<<"PASS "<<checks<<" standard-pane Image accepted-pixel authoring; physical input/Relink chooser NOT_RUN\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}

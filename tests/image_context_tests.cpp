#include "window.hpp"
#include "visual_style.hpp"
#include "nect/io.hpp"
#include <QAction>
#include <QApplication>
#include <QDockWidget>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QPointer>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QTreeWidget>
#include <iostream>
#include <functional>
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
bool same_asset_session(const Session& a,const Session& b){return same(a,b)&&a.can_undo()==b.can_undo()&&a.can_redo()==b.can_redo();}
void assets_modal(const QString& scratch,const std::string& scenario) {
    const auto path=scratch+"/assets-reference.png";const auto original=png(200,100,40);write(path,original);
    const auto initial=fixture(path,original);Window w(scratch+"/assets-recovery");w.host.session=Session(initial);
    w.host.session_id="assets-modal-session";w.host.edited();w.show();events();w.canvas->fit_artboard();events();select(w);
    Session expected(initial);QPushButton* button=nullptr;
    if(scenario=="place"){w.canvas->set_active_artboard("comp","other-art",false);events();select(w);check(w.canvas->active_artboard()=="other-art","Place starts on the exact nonzero-origin Artboard");}
    for(auto* b:w.findChildren<QPushButton*>())if(b->isVisible()&&b->text()=="Image Assets…")button=b;
    check(button,"Exact owned Properties Image Assets button");reveal(w,button);
    auto* width=field(w,"image.width");reveal(w,width);
    QTest::mouseClick(width,Qt::LeftButton);QTest::keyClick(width,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(width,"160");
    check(width->hasFocus()&&width->isModified()&&same_asset_session(w.host.session,expected),"Assets pending Width full Session neutral");
    // Reach the existing library using only the pane's vertical scroll; retain focus and draft.
    const auto suffix=".assets-"+QString::fromStdString(scenario);
    reveal(w,button);evidence(w,suffix+"-before.png");
    expected.apply({EditProperties{{{"image","","image.width"}},160,false}},expected.revision());
    bool opened=false,entry=false,selected=false,neutral=false;std::string failure;
    QTimer::singleShot(0,&w,[&]{
        auto* dialog=w.findChild<QDialog*>("image-assets-dialog");if(!dialog)return;
        try {
            opened=dialog->isVisible();entry=same_asset_session(w.host.session,expected);
            auto* list=dialog->findChild<QListWidget*>("image-assets-list");
            selected=list&&list->currentItem()&&list->currentItem()->data(Qt::UserRole).toString()=="asset";
            const auto prefix=qEnvironmentVariable("NECT_IMAGE_CONTEXT_EVIDENCE");
            check(prefix.isEmpty()||dialog->grab().save(prefix+suffix+"-dialog.png"),"Actual Assets dialog saved");
            std::cout<<"Assets opened="<<opened<<" entryFullSession="<<entry<<" revision="<<w.host.session.revision()<<std::endl;
            auto* place=dialog->findChild<QPushButton*>("assets-place");check(place,"Actual Place selected button");
            if(scenario!="close"&&scenario!="place") {
                if(scenario=="revision")w.host.session.apply({Set{{"copy","","image.height"},65}},w.host.session.revision());
                else if(scenario=="session")w.host.session_id="incoming-assets-session";
                else if(scenario=="document") {auto d=w.host.session.document();d.id="incoming-assets-document";w.host.session=Session(d);w.host.session.apply({Set{{"copy","","image.height"},65}},0);}
                else {w.host.session.begin_gesture(w.host.session.revision());w.host.session.update_gesture({Set{{"copy","","image.height"},65}});if(scenario=="generation")w.host.session.cancel_gesture();}
                const Session incoming=w.host.session;QTest::mouseClick(place,Qt::LeftButton);
                neutral=same_asset_session(w.host.session,incoming);expected=incoming;
                const auto code=scenario=="session"||scenario=="document"?"SESSION_CONFLICT":"REVISION_CONFLICT";
                check(w.statusBar()->currentMessage().startsWith(code),"Assets incoming conflict is explicitly reported");
            } else if(scenario=="place") {
                write(path,png(30,190,220,30,30));
                for(int n=0;n<2;++n) {
                    QTest::mouseClick(place,Qt::LeftButton);const auto id=w.canvas->selected_object;
                    check(!id.empty()&&!expected.document().objects.contains(id),"Place emits a fresh stable placement ID");
                    expected.apply({CreateImage{"comp","",id,"Shared reference",{"asset",{40},{20}}},Set{{id,"","transform.tx"},700},Set{{id,"","transform.ty"},0}},expected.revision());
                    check(same_asset_session(w.host.session,expected),"Repeated Place exact independent source/full History/revision equality");
                    check(list->currentItem()&&list->currentItem()->data(Qt::UserRole).toString()=="asset","Place refresh retains selected shared asset identity");
                }
            }
        }catch(const std::exception& e){failure=e.what();}
        QPushButton* close=nullptr;for(auto* b:dialog->findChildren<QPushButton*>())if(b->text()=="Close")close=b;
        if(close)QTest::mouseClick(close,Qt::LeftButton);else{failure="Missing actual Close button";dialog->reject();}
    });
    QTest::mouseClick(button,Qt::LeftButton);events();evidence(w,suffix+"-after.png");
    check(failure.empty(),failure.c_str());check(opened&&entry&&selected,"First pointer click enters actual Assets modal after ordinary Width commit");
    if(scenario!="close"&&scenario!="place") {check(neutral,"Assets Place refuses incoming context with complete source/preview/History/generation/eligibility neutral");w.host.changed={};w.hide();return;}
    check(same_asset_session(w.host.session,expected),"Assets Close retains only scalar and explicit placement transactions");
    w.canvas->set_active_artboard("comp","art");events();painted(w,expected,QColor(200,100,40),suffix);
    const int steps=scenario=="place"?3:1;
    for(int n=0;n<steps;++n){history(w,"Undo");expected.undo(expected.revision());check(same_asset_session(w.host.session,expected),"Assets separate Undo full equality");}
    check(w.host.session.document()==initial,"Assets all Undo restores complete initial placements/asset/source/artwork/Artboards");
    for(int n=0;n<steps;++n){history(w,"Redo");expected.redo(expected.revision());check(same_asset_session(w.host.session,expected),"Assets separate Redo full equality");}
    const auto native=scratch+"/assets.nect";w.host.save(native);check(same_asset_session(w.host.session,expected),"Assets save full Session neutral");w.host.changed={};w.hide();
    Window cold(scratch+"/assets-cold");cold.host.open(native);cold.show();events();cold.canvas->fit_artboard();events();select(cold);
    Session reopened(expected.document());check(same_asset_session(cold.host.session,reopened),"Assets cold native restores all accepted shared source/placements");
    painted(cold,reopened,QColor(200,100,40),suffix+"-cold");cold.host.changed={};cold.hide();
}
struct RelinkObservation { bool opened=false,scalar=false,saved=false; };
RelinkObservation relink_dialog(Window& w,QPushButton* button,const Session& modal_expected,const QString& path,bool cancel,const QString& suffix,const std::function<void()>& incoming={}) {
    RelinkObservation result;
    QTimer::singleShot(0,&w,[&]{
        auto* dialog=w.findChild<QFileDialog*>();if(!dialog)return;
        result.opened=dialog->isVisible()&&dialog->windowTitle()=="Relink Image";
        result.scalar=same_asset_session(w.host.session,modal_expected);
        const auto prefix=qEnvironmentVariable("NECT_IMAGE_CONTEXT_EVIDENCE");
        result.saved=prefix.isEmpty()||dialog->grab().save(prefix+suffix+".chooser.png");
        std::cout<<"Relink chooser opened="<<result.opened<<" modalFullSession="<<result.scalar<<" revision="<<w.host.session.revision()<<std::endl;
        if(incoming)incoming();
        if(cancel)dialog->reject();else{dialog->selectFile(path);QMetaObject::invokeMethod(dialog,"accept",Qt::QueuedConnection);}
    });
    QTest::mouseClick(button,Qt::LeftButton);events();return result;
}
void relink_guard(const QString& scratch,const std::string& scenario) {
    const auto path=scratch+"/guard-original.png",chosen=scratch+"/guard-chosen.png";
    const auto original=png(200,100,40),replacement=png(30,190,220,30,30);write(path,original);write(chosen,replacement);
    Window w(scratch+"/modal-guard-recovery");w.host.session=Session(fixture(path,original));w.host.session_id="modal-guard-session";
    w.host.edited();w.show();events();select(w);Session expected=w.host.session;
    auto* button=named<QPushButton>(w,"image-relink");reveal(w,button);auto* width=field(w,"image.width");reveal(w,width);
    QTest::mouseClick(width,Qt::LeftButton);QTest::keyClick(width,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(width,"160");
    expected.apply({EditProperties{{{"image","","image.width"}},160,false}},expected.revision());
    const auto result=relink_dialog(w,button,expected,chosen,false,".guard-"+QString::fromStdString(scenario),[&]{
        if(scenario=="revision")w.host.session.apply({Set{{"copy","","image.height"},65}},w.host.session.revision());
        else if(scenario=="session")w.host.session_id="incoming-session";
        else if(scenario=="document"){
            auto d=w.host.session.document();d.id="incoming-document";w.host.session=Session(d);
            w.host.session.apply({Set{{"copy","","image.height"},65}},w.host.session.revision());
        }else{
            w.host.session.begin_gesture(w.host.session.revision());
            w.host.session.update_gesture({Set{{"copy","","image.height"},65}});
            if(scenario=="generation")w.host.session.cancel_gesture();
        }
        expected=w.host.session;w.host.edited();
    });
    check(result.opened&&result.scalar&&result.saved,"Guard starts in owned modal with exact committed scalar state");
    check(same_asset_session(w.host.session,expected),"Modal Relink refuses incoming revision/Session/document/preview/generation with full incoming Session preserved");
    check(w.host.session.document().raster_assets.at("asset").locator==path.toStdString()&&w.host.session.document().raster_assets.at("asset").payload->bytes()==original,"Modal conflict preserves complete original accepted asset");
    if(scenario=="session")check(w.host.session_id=="incoming-session","Incoming Session identity preserved");
    evidence(w,".guard-"+QString::fromStdString(scenario)+"-after.png");w.host.changed={};w.hide();
}
void relink_preflight(const QString& scratch) {
    const auto path=scratch+"/preflight.png",chosen=scratch+"/preflight-chosen.png";const auto original=png(200,100,40);write(path,original);write(chosen,png(30,190,220));
    for(const bool driven:{false,true}) {
        Session expected(fixture(path,original));if(driven)expected.apply({Link{{"image","","image.width"},{{"copy","","image.width"},1,0,"copy_local_value"}}},expected.revision());
        Window w(scratch+"/preflight-recovery");w.host.session=expected;w.host.session_id="relink-preflight";w.host.edited();w.show();events();select(w);
        auto* button=named<QPushButton>(w,"image-relink");reveal(w,button);auto* width=field(w,"image.width");reveal(w,width);
        QTest::mouseClick(width,Qt::LeftButton);QTest::keyClick(width,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(width,driven?"160":"-3");
        const auto refused=relink_dialog(w,button,expected,chosen,true,".preflight");
        check(!refused.opened&&same_asset_session(w.host.session,expected),"Invalid or driven dimension refuses before chooser and retains complete source/history");
        w.host.changed={};w.hide();
    }
    Window w(scratch+"/cancelled-pointer-recovery");w.host.session=Session(fixture(path,original));w.host.session_id="relink-pointer-session";w.host.edited();w.show();events();select(w);Session expected=w.host.session;
    auto* button=named<QPushButton>(w,"image-relink");reveal(w,button);auto* width=field(w,"image.width");reveal(w,width);
    QTest::mouseClick(width,Qt::LeftButton);QTest::keyClick(width,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(width,"160");const QPointer<QLineEdit> pending=width;
    QTest::mousePress(button,Qt::LeftButton);QTest::mouseRelease(button,Qt::LeftButton,Qt::NoModifier,QPoint(-4,-4));events();
    check(pending&&pending->isModified()&&pending->text()=="160"&&same_asset_session(w.host.session,expected),"Cancelled Relink pointer press preserves pending ordinary dimension and full Session");
    QTest::mouseClick(pending,Qt::LeftButton);QTest::keyClick(pending,Qt::Key_Return);events();expected.apply({EditProperties{{{"image","","image.width"}},160,false}},expected.revision());canonical(w,expected);
    button=named<QPushButton>(w,"image-relink");w.host.session.apply({Set{{"copy","","image.height"},65}},w.host.session.revision());expected=w.host.session;
    const auto stale=relink_dialog(w,button,expected,chosen,true,".stale");check(!stale.opened&&same_asset_session(w.host.session,expected),"Old Relink callback refuses stale revision before opening chooser");
    w.host.edited();events();w.host.session.begin_gesture(w.host.session.revision());w.host.edited();events();button=named<QPushButton>(w,"image-relink");w.host.session.cancel_gesture();expected=w.host.session;
    const auto preview_born=relink_dialog(w,button,expected,chosen,true,".preview-born");check(!preview_born.opened&&same_asset_session(w.host.session,expected),"Preview-born Relink stays ineligible after cancellation at same revision/generation");
    w.host.changed={};w.hide();
}
void draft_relink(const QString& scratch) {
    const auto original=png(200,100,40),replacement=png(30,190,220,30,30);
    for(const bool cancel:{true,false}) {
        const QString tag=cancel?"relink-cancel":"relink-accept";
        const auto path=scratch+"/"+tag+"-original.png",chosen=scratch+"/"+tag+"-chosen.png";
        write(path,original);write(chosen,replacement);const auto initial=fixture(path,original);
        Window w(scratch+"/"+tag+"-recovery");w.host.session=Session(initial);w.host.session_id="image-relink-session";
        w.host.edited();w.show();events();w.canvas->fit_artboard();events();select(w);Session expected(initial);
        auto* button=named<QPushButton>(w,"image-relink");reveal(w,button);auto* width=field(w,"image.width");reveal(w,width);
        auto* area=named<QScrollArea>(w,"inspector-scroll");
        check(area->viewport()->rect().contains(QRect(button->mapTo(area->viewport(),QPoint()),button->size())),"Relink and Width simultaneously reachable in standard pane");
        QTest::mouseClick(width,Qt::LeftButton);QTest::keyClick(width,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(width,"160");
        check(width->hasFocus()&&width->isModified()&&same_asset_session(w.host.session,expected),"Pending Width before Relink complete Session neutral");
        evidence(w,"."+tag+"-before.png");expected.apply({EditProperties{{{"image","","image.width"}},160,false}},expected.revision());
        const auto result=relink_dialog(w,button,expected,chosen,cancel,"."+tag);
        check(result.opened&&result.saved,"First pointer click opens actual owned Qt Relink chooser; evidence saved");
        check(result.scalar,"Chooser entry follows exact ordinary dimension commit with full Session equality");
        auto accepted=expected.document().raster_assets.at("asset");
        if(!cancel){accepted.locator=chosen.toStdString();accepted.payload=make_raster(replacement);expected.apply({ReplaceRasterAsset{accepted}},expected.revision());}
        std::cout<<"Relink "<<tag.toStdString()<<" width="<<w.host.session.document().objects.at("image").image->width.literal
                 <<" revision="<<w.host.session.revision()<<" expected="<<expected.revision()<<" fullSession="<<same_asset_session(w.host.session,expected)<<std::endl;
        evidence(w,"."+tag+"-after.png");
        check(same_asset_session(w.host.session,expected),"Relink cancel keeps scalar only; accept adds exact selected shared asset with full Session equality");
        auto protected_source=initial;protected_source.objects.at("image").image->width.literal=160;protected_source.raster_assets.at("asset")=accepted;
        check(w.host.session.document()==protected_source&&encode(w.host.session.document())==encode(protected_source),"Relink retains full identity/per-placement source/transforms/paint/Artboards");
        painted(w,expected,cancel?QColor(200,100,40):QColor(30,190,220),"."+tag);
        history(w,"Undo");expected.undo(expected.revision());check(same_asset_session(w.host.session,expected),"Relink first Undo complete canonical equality");
        if(!cancel){check(w.host.session.document().raster_assets.at("asset")==initial.raster_assets.at("asset"),"Relink Undo restores original asset while retaining scalar");history(w,"Undo");expected.undo(expected.revision());}
        check(same_asset_session(w.host.session,expected)&&w.host.session.document()==initial,"All relevant Undo restores full original source");
        history(w,"Redo");expected.redo(expected.revision());if(!cancel){history(w,"Redo");expected.redo(expected.revision());}
        check(same_asset_session(w.host.session,expected),"Relink Redo full canonical equality");
        const auto native=scratch+"/"+tag+".nect";w.host.save(native);check(same_asset_session(w.host.session,expected),"Relink native save complete Session neutral");w.host.changed={};w.hide();
        Window cold(scratch+"/"+tag+"-cold");cold.host.open(native);cold.show();events();cold.canvas->fit_artboard();events();select(cold);Session reopened(expected.document());
        check(same_asset_session(cold.host.session,reopened),"Relink native fresh Window full source and accepted pixels");
        reveal(cold,field(cold,"image.width"));check(field(cold,"image.width")->text().toDouble()==160&&field(cold,"image.height")->text().toDouble()==60,"Relink cold controls read per-placement dimensions");
        painted(cold,reopened,cancel?QColor(200,100,40):QColor(30,190,220),"."+tag+"-cold");cold.host.changed={};cold.hide();
    }
}
void draft_check_link(const QString& scratch) {
    const auto path=scratch+"/check-reference.png";
    const auto original=png(200,100,40),replacement=png(30,190,220,30,30);write(path,original);
    const auto initial=fixture(path,original);Window w(scratch+"/check-recovery");w.host.session=Session(initial);
    w.host.session_id="image-draft-check-session";w.host.edited();w.show();events();w.canvas->fit_artboard();events();
    Session expected(initial);select(w);const auto canvas_width=w.canvas->width(),dock_width=named<QDockWidget>(w,"properties")->width();
    auto* action=named<QPushButton>(w,"image-check-link");reveal(w,action);auto* width=field(w,"image.width");reveal(w,width);
    auto* area=named<QScrollArea>(w,"inspector-scroll");
    check(area->viewport()->rect().contains(QRect(action->mapTo(area->viewport(),QPoint()),action->size())),"Check link and Width simultaneously reachable");
    write(path,replacement);QTest::mouseClick(width,Qt::LeftButton);QTest::keyClick(width,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(width,"160");
    check(width->hasFocus()&&width->isModified()&&same_asset_session(w.host.session,expected),"Pending Width before Check remains complete Session neutral");
    evidence(w,".draft-check-before.png");QTest::mouseClick(action,Qt::LeftButton);events();
    expected.apply({EditProperties{{{"image","","image.width"}},160,false}},expected.revision());
    const auto observed=w.host.asset_status("asset");const auto visible=named<QLabel>(w,"image-link-status")->text();
    std::cout<<"First Check width="<<w.host.session.document().objects.at("image").image->width.literal<<" revision="<<w.host.session.revision()
             <<" fullSession="<<same_asset_session(w.host.session,expected)<<" state="<<observed.value("state").toString().toStdString()
             <<" visible="<<visible.toStdString()<<std::endl;evidence(w,".draft-check-after.png");
    check(same_asset_session(w.host.session,expected),"First Check commits only ordinary Width; complete independent source/native/History/preview/eligibility");
    auto protected_source=initial;protected_source.objects.at("image").image->width.literal=160;
    check(w.host.session.document()==protected_source&&encode(w.host.session.document())==encode(protected_source),"Check retains accepted/shared asset, locator, other placement, transforms, paint and Artboards");
    painted(w,expected,QColor(200,100,40),".draft-check");
    check(observed.value("state").toString()=="changed"&&visible.startsWith("changed"),"First Check observes changed owned file in current visible Properties");
    check(observed.value("accepted_sha256").toString()==QString::fromStdString(make_raster(original)->sha256())&&
          observed.value("observed_sha256").toString()==QString::fromStdString(make_raster(replacement)->sha256())&&!observed.value("checked_at").toString().isEmpty(),"Observation binds original accepted hash and changed file hash");
    history(w,"Undo");expected.undo(expected.revision());check(same_asset_session(w.host.session,expected)&&w.host.session.document()==initial,"Check adds no Undo entry beyond ordinary Width");
    history(w,"Redo");expected.redo(expected.revision());check(same_asset_session(w.host.session,expected),"Ordinary Width Redo complete equality");
    check(named<QLabel>(w,"image-link-status")->text().startsWith("changed"),"Observation visible after Properties rebuild");
    write(path,original);click(w,"image-check-link");check(same_asset_session(w.host.session,expected),"Current file check complete Session neutral");
    check(w.host.asset_status("asset").value("state").toString()=="current"&&named<QLabel>(w,"image-link-status")->text().startsWith("current"),"Current accepted file observation visible");
    check(QFile::remove(path),"Remove only owned Check fixture");click(w,"image-check-link");check(same_asset_session(w.host.session,expected),"Missing file check complete Session neutral");
    check(w.host.asset_status("asset").value("state").toString()=="missing"&&named<QLabel>(w,"image-link-status")->text().startsWith("missing"),"Missing file observation visible");
    w.host.session.begin_gesture(w.host.session.revision());w.host.session.update_gesture({Set{{"copy","","image.height"},65}});w.host.edited();events();
    const Session preview=w.host.session;click(w,"image-check-link");
    check(same_asset_session(w.host.session,preview),"Check presentation preserves external active preview/source/History/generation/eligibility");
    w.host.session.cancel_gesture();expected=w.host.session;w.host.edited();events();
    auto* stale=named<QPushButton>(w,"image-check-link");const auto missing=w.host.asset_status("asset");write(path,original);
    w.host.session_id="other-check-session";stale->click();events();
    check(same_asset_session(w.host.session,expected)&&w.host.asset_status("asset")==missing,"Old Check callback refuses another Session without observing or changing source");
    w.host.session_id="image-draft-check-session";w.host.edited();events();
    check(w.canvas->width()==canvas_width&&named<QDockWidget>(w,"properties")->width()==dock_width,"Check preserves standard pane and Canvas widths");
    const auto native=scratch+"/checked.nect";w.host.save(native);check(same_asset_session(w.host.session,expected),"Check native save full Session neutral");w.host.changed={};w.hide();
    Window cold(scratch+"/check-cold");cold.host.open(native);cold.show();events();cold.canvas->fit_artboard();events();select(cold);Session reopened(expected.document());
    check(same_asset_session(cold.host.session,reopened),"Fresh native Window restores full accepted source with ordinary Width");
    check(cold.host.asset_status("asset").value("state").toString()=="unchecked"&&named<QLabel>(cold,"image-link-status")->text().startsWith("unchecked"),"Cold native starts unchecked without implicit filesystem acceptance");
    reveal(cold,field(cold,"image.width"));check(field(cold,"image.width")->text().toDouble()==160&&field(cold,"image.height")->text().toDouble()==60,"Cold controls read ordinary dimension source");
    painted(cold,reopened,QColor(200,100,40),".draft-check-cold");cold.host.changed={};cold.hide();
}
void draft_asset_action(const QString& scratch,bool embed=false) {
    const QString tag=embed?"draft-embed":"draft-reload";
    const auto path=scratch+"/"+tag+"-reference.png";
    const auto original=png(200,100,40),replacement=png(30,190,220,30,30);write(path,original);
    const auto initial=fixture(path,original);Window w(scratch+"/"+tag+"-recovery");w.host.session=Session(initial);
    w.host.session_id="image-draft-reload-session";w.host.edited();w.show();events();w.canvas->fit_artboard();events();
    Session expected(initial);select(w);auto* reload=named<QPushButton>(w,embed?"image-embed":"image-reload");reveal(w,reload);
    auto* width=field(w,"image.width");reveal(w,width);
    auto* area=named<QScrollArea>(w,"inspector-scroll");
    check(area->viewport()->rect().contains(QRect(reload->mapTo(area->viewport(),QPoint()),reload->size())),"Asset action and Width simultaneously reachable");
    write(path,replacement);QTest::mouseClick(width,Qt::LeftButton);QTest::keyClick(width,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(width,"160");
    check(width->hasFocus()&&width->isModified()&&same_asset_session(w.host.session,expected),"Draft-to-asset action remains full Session neutral before click");
    const QPointer<QLineEdit> pending_width=width;
    QTest::mousePress(reload,Qt::LeftButton);QTest::mouseRelease(reload,Qt::LeftButton,Qt::NoModifier,QPoint(-4,-4));events();
    check(pending_width&&pending_width->isModified()&&pending_width->text()=="160"&&same_asset_session(w.host.session,expected),"Cancelled asset action press preserves complete Session and pending Width");
    evidence(w,"."+tag+"-before.png");QTest::mouseClick(reload,Qt::LeftButton);events();
    std::cout<<"First "<<tag.toStdString()<<" width="<<w.host.session.document().objects.at("image").image->width.literal
             <<" payload="<<w.host.session.document().raster_assets.at("asset").payload->width()<<"x"
             <<w.host.session.document().raster_assets.at("asset").payload->height()<<" mode="<<w.host.session.document().raster_assets.at("asset").mode<<" revision="<<w.host.session.revision()<<std::endl;
    evidence(w,"."+tag+"-after.png");
    expected.apply({EditProperties{{{"image","","image.width"}},160,false}},expected.revision());
    auto accepted=expected.document().raster_assets.at("asset");
    if(embed){accepted.mode="embedded";accepted.locator.clear();}else accepted.payload=make_raster(replacement);
    expected.apply({ReplaceRasterAsset{accepted}},expected.revision());
    check(same_asset_session(w.host.session,expected),
        "First asset action click commits ordinary pending Width then exact accepted asset with complete independent Session/History");
    auto protected_source=initial;protected_source.objects.at("image").image->width.literal=160;protected_source.raster_assets.at("asset")=accepted;
    check(w.host.session.document()==protected_source&&encode(w.host.session.document())==encode(protected_source),
        "Asset action preserves complete per-placement source/transforms/paint/Artboards except pending Width and shared accepted payload");
    const QColor accepted_color=embed?QColor(200,100,40):QColor(30,190,220);
    painted(w,expected,accepted_color,"."+tag);
    history(w,"Undo");expected.undo(expected.revision());check(same_asset_session(w.host.session,expected),"Asset action Undo restores original asset while retaining committed Width");
    history(w,"Undo");expected.undo(expected.revision());check(same_asset_session(w.host.session,expected)&&w.host.session.document()==initial,"Next Undo restores exact initial source");
    history(w,"Redo");expected.redo(expected.revision());history(w,"Redo");expected.redo(expected.revision());check(same_asset_session(w.host.session,expected),"Both Redos restore full accepted source/history");
    const auto native=scratch+"/"+tag+".nect";w.host.save(native);check(same_asset_session(w.host.session,expected),"Asset action native save full Session neutral");w.host.changed={};w.hide();
    Window cold(scratch+"/"+tag+"-cold");cold.host.open(native);cold.show();events();cold.canvas->fit_artboard();events();select(cold);Session reopened(expected.document());
    check(same_asset_session(cold.host.session,reopened),"Fresh native Window restores complete accepted asset source");
    reveal(cold,field(cold,"image.width"));check(field(cold,"image.width")->text().toDouble()==160&&field(cold,"image.height")->text().toDouble()==60,
        "Asset action cold controls retain committed per-placement dimensions");painted(cold,reopened,accepted_color,"."+tag+"-cold");cold.host.changed={};cold.hide();
}
void draft_asset_guards(const QString& scratch,bool embed=false) {
    const char* action=embed?"image-embed":"image-reload";
    const auto path=scratch+"/reload-guards.png";const auto original=png(200,100,40);write(path,original);
    Window w(scratch+"/reload-guards-recovery");w.host.session=Session(fixture(path,original));w.host.session_id="reload-guards-session";
    w.host.edited();w.show();events();select(w);
    auto* reload=named<QPushButton>(w,action);reveal(w,reload);auto* width=field(w,"image.width");reveal(w,width);
    QTest::mouseClick(width,Qt::LeftButton);QTest::keyClick(width,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(width,"160");Session expected=w.host.session;
    QTest::mousePress(reload,Qt::LeftButton);QTest::mouseRelease(reload,Qt::LeftButton,Qt::NoModifier,QPoint(-4,-4));events();
    check(same_asset_session(w.host.session,expected)&&width->isModified(),"Cancelled asset action retains pending ordinary draft");
    QTest::mouseClick(width,Qt::LeftButton);QTest::keyClick(width,Qt::Key_Return);events();expected.apply({EditProperties{{{"image","","image.width"}},160,false}},expected.revision());
    canonical(w,expected);check(w.host.session.can_undo()==expected.can_undo()&&w.host.session.can_redo()==expected.can_redo(),"Cancelled asset action resumed Enter retains full Undo eligibility");
    auto* stale=named<QPushButton>(w,action);w.host.session.apply({Set{{"copy","","image.height"},65}},w.host.session.revision());Session external=w.host.session;
    stale->click();check(same_asset_session(w.host.session,external),"Old asset action rejects external revision without adopting it");w.host.edited();events();
    stale=named<QPushButton>(w,action);w.host.session_id="different-session";stale->click();check(same_asset_session(w.host.session,external),"Asset action refuses another Session");w.host.session_id="reload-guards-session";
    w.host.session.begin_gesture(w.host.session.revision());Session active=w.host.session;stale->click();check(same_asset_session(w.host.session,active),"Asset action refuses active external gesture and preserves preview");
    w.host.edited();events();auto* preview_born=named<QPushButton>(w,action);w.host.session.cancel_gesture();Session cancelled=w.host.session;
    preview_born->click();check(same_asset_session(w.host.session,cancelled),"Preview-born asset action remains ineligible after cancellation at same revision/generation");w.host.edited();events();
    reload=named<QPushButton>(w,action);reveal(w,reload);width=field(w,"image.width");reveal(w,width);
    QTest::mouseClick(width,Qt::LeftButton);QTest::keyClick(width,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(width,"-3");Session refused=w.host.session;
    QTest::mouseClick(reload,Qt::LeftButton);events();check(same_asset_session(w.host.session,refused),"Invalid dimension refuses asset acceptance with complete Session unchanged");
    w.host.edited();events();reload=named<QPushButton>(w,action);reveal(w,reload);width=field(w,"image.width");reveal(w,width);
    QTest::mouseClick(width,Qt::LeftButton);QTest::keyClick(width,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(width,"+=40");expected=w.host.session;
    check(QFile::remove(path),"Remove only owned link");QTest::mouseClick(reload,Qt::LeftButton);events();
    expected.apply({EditProperties{{{"image","","image.width"}},40,true}},expected.revision());
    if(embed){auto asset=expected.document().raster_assets.at("asset");asset.mode="embedded";asset.locator.clear();expected.apply({ReplaceRasterAsset{asset}},expected.revision());}
    canonical(w,expected);check(w.host.session.document().raster_assets.at("asset").payload->sha256()==make_raster(original)->sha256(),
        "Missing-link asset action keeps accepted bytes and the independently committed relative dimension");w.host.changed={};w.hide();
}
void draft_asset_scalar_forms(const QString& scratch,bool embed=false) {
    const auto path=scratch+"/asset-scalar-forms.png";const auto original=png(200,100,40),replacement=png(30,190,220,30,30);write(path,original);
    const auto initial=fixture(path,original);write(path,replacement);
    for(const auto& text:{"120","=120 + 40"}) {
        Window w(scratch+"/scalar-form-recovery");w.host.session=Session(initial);w.host.session_id="asset-scalar-forms";
        w.host.edited();w.show();events();select(w);auto* button=named<QPushButton>(w,embed?"image-embed":"image-reload");reveal(w,button);
        auto* width=field(w,"image.width");reveal(w,width);QTest::mouseClick(width,Qt::LeftButton);QTest::keyClick(width,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(width,text);
        Session expected(initial);QTest::mouseClick(button,Qt::LeftButton);events();
        if(text[0]=='=')expected.apply({SetExpression{{{"image","","image.width"}},{"120 + 40",1},false}},expected.revision());
        else expected.apply({EditProperties{{{"image","","image.width"}},120,false}},expected.revision());
        auto asset=expected.document().raster_assets.at("asset");if(embed){asset.mode="embedded";asset.locator.clear();}else asset.payload=make_raster(replacement);
        expected.apply({ReplaceRasterAsset{asset}},expected.revision());
        std::cout<<"Scalar draft "<<text<<" actualRevision="<<w.host.session.revision()<<" expectedRevision="<<expected.revision()
                 <<" fullDocument="<<(w.host.session.document()==expected.document())<<std::endl;
        check(same_asset_session(w.host.session,expected),"Asset action preserves exact expression source or no-op dimension with independent full Session");
        canonical(w,expected);w.host.changed={};w.hide();
    }
    Session driven(initial);driven.apply({Link{{"image","","image.width"},{{"copy","","image.width"},1,0,"copy_local_value"}}},driven.revision());
    Window w(scratch+"/driven-asset-recovery");w.host.session=driven;w.host.session_id="driven-asset-session";w.host.edited();w.show();events();select(w);
    auto* button=named<QPushButton>(w,embed?"image-embed":"image-reload");reveal(w,button);auto* width=field(w,"image.width");reveal(w,width);
    QTest::mouseClick(width,Qt::LeftButton);QTest::keyClick(width,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(width,"160");QTest::mouseClick(button,Qt::LeftButton);events();
    check(same_asset_session(w.host.session,driven),"Driven dimension refusal preserves complete binding/source/accepted asset/History atomically");w.host.changed={};w.hide();
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
int main(int argc,char** argv){if(argc>1&&(std::string(argv[1])=="--draft-relink"||std::string(argv[1]).starts_with("--relink-")))QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);QApplication app(argc,argv);app.setStyle("Fusion");app.setStyleSheet(application_style_sheet());QTemporaryDir scratch(qEnvironmentVariable("NECT_IMAGE_CONTEXT_SCRATCH",QDir::tempPath())+"/image-context-XXXXXX");QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,scratch.path());app.setOrganizationName("NectTest");app.setApplicationName("ImageContext");
    if(argc>1&&std::string(argv[1])=="--assets-modal")try{check(scratch.isValid()&&argc==3,"Owned Assets exact scenario");assets_modal(scratch.path(),argv[2]);std::cout<<"PASS "<<checks<<" owned Qt Assets modal full Session/native; physical input NOT_RUN\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
    if(argc>1&&std::string(argv[1]).starts_with("--relink-"))try{check(scratch.isValid(),"Owned Relink guard scratch");if(std::string(argv[1])=="--relink-preflight")relink_preflight(scratch.path());else{check(argc==3,"Exact modal guard scenario");relink_guard(scratch.path(),argv[2]);}std::cout<<"PASS "<<checks<<" Image Relink modal/preflight guard full Session; native OS chooser/physical input NOT_RUN\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
    if(argc>1&&std::string(argv[1])=="--draft-relink")try{check(scratch.isValid(),"Owned Relink scratch");draft_relink(scratch.path());std::cout<<"PASS "<<checks<<" Image first-click owned Qt Relink chooser/cancel/accept/full Session/native; native OS chooser/physical input NOT_RUN\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
    if(argc>1&&std::string(argv[1])=="--draft-check")try{check(scratch.isValid(),"Owned draft-check scratch");draft_check_link(scratch.path());std::cout<<"PASS "<<checks<<" first Check link/ordinary draft/visible observation/full accepted source/History/native; physical input NOT_RUN\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
    if(argc>1&&std::string(argv[1])=="--draft-reload")try{check(scratch.isValid(),"Owned draft-reload scratch");draft_asset_action(scratch.path());draft_asset_guards(scratch.path());draft_asset_scalar_forms(scratch.path());std::cout<<"PASS "<<checks<<" Image direct draft-to-Reload/cancel/context/scalar guards/full accepted source/History/native; physical input NOT_RUN\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
    if(argc>1&&std::string(argv[1])=="--draft-embed")try{check(scratch.isValid(),"Owned draft-embed scratch");draft_asset_action(scratch.path(),true);draft_asset_guards(scratch.path(),true);draft_asset_scalar_forms(scratch.path(),true);std::cout<<"PASS "<<checks<<" Image direct draft-to-Embed/cancel/context/scalar guards/full accepted source/History/native; physical input NOT_RUN\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
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

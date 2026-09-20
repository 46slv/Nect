#include "window.hpp"
#include <QAction>
#include <QApplication>
#include <QFile>
#include <QFileDialog>
#include <QDialog>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <iostream>

using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);++checks;}
template<class F>void rejects(const char* code,F action){try{action();}catch(const Error& e){check(e.code==code,std::string("Expected ")+code+", got "+e.code);return;}throw std::runtime_error(std::string("Missing rejection: ")+code);}
void write(const QString& path,const std::vector<unsigned char>& bytes){QFile f(path);check(f.open(QIODevice::WriteOnly),"Open owned fixture");check(f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size())==qsizetype(bytes.size()),"Write complete fixture");}
std::vector<unsigned char> png(unsigned char r,unsigned char g,unsigned char b,unsigned char alpha=255) {
    RasterPixels p{4,3,{}};for(int i=0;i<12;++i)p.rgba.insert(p.rgba.end(),{r,g,b,alpha});return encode_raster_png(p);
}
QJsonObject dispatch(Host& host,QJsonObject operation,bool ok=true) {
    operation["session_id"]=host.session_id;operation["document_id"]=QString::fromStdString(host.session.document().id);
    if(!operation.contains("expected_revision"))operation["expected_revision"]=qint64(host.session.revision());
    const auto result=QJsonDocument::fromJson(host.dispatch(QJsonDocument(operation).toJson(QJsonDocument::Compact))).object();
    check(result.value("ok").toBool()==ok,"Host request: "+QJsonDocument(result).toJson(QJsonDocument::Compact).left(500).toStdString());return result;
}
void lifecycle(const QString& directory) {
    Host host(directory+"/recovery");const auto comp=host.session.document().compositions[0].id;
    const auto original=png(200,100,40),replacement=png(30,190,220);const auto path=directory+"/source.png";write(path,original);
    auto imported=dispatch(host,{{"op","import_image"},{"path",path},{"mode","linked"},{"composition",QString::fromStdString(comp)},
        {"parent",""},{"asset","asset"},{"id","image"},{"name","Synthetic source"},{"x",100},{"y",80}});
    check(host.session.revision()==1&&host.session.document().raster_assets.at("asset").payload->bytes()==original,"File admission retains exact original once");
    check(host.asset_status("asset")["state"]=="current","Import observes current link");
    host.session.apply({CreateImage{comp,"","copy","Copy",{"asset",{80},{60}}},Set{{"image","","image.width"},160},Set{{"image","","image.height"},120}},1);host.edited();
    const auto before=host.session.document();const auto rev=host.session.revision();write(path,replacement);
    check(host.check_asset("asset")["state"]=="changed"&&host.session.revision()==rev&&host.session.document()==before,"Checking external change never accepts pixels");
    const auto loaded=dispatch(host,{{"op","asset"},{"asset","asset"},{"action","reload"}});
    const auto ids=loaded["result"].toObject()["changed_ids"].toArray();check(ids.contains("image")&&ids.contains("copy"),"Reload reports all shared placements");
    check(host.session.document().raster_assets.at("asset").payload->bytes()==replacement,"Reload accepts new bytes");
    check(host.session.document().objects==before.objects,"Reload preserves every placement property and identity");
    host.session.undo(host.session.revision());check(host.session.document()==before,"One Undo restores exact linked asset");host.session.redo(host.session.revision());
    const auto accepted=host.session.document();write(path,{'b','a','d'});
    check(host.check_asset("asset")["state"]=="changed","Malformed replacement is still detected");
    dispatch(host,{{"op","asset"},{"asset","asset"},{"action","reload"}},false);check(host.session.document()==accepted,"Malformed Reload is atomic");
    check(QFile::remove(path),"Remove owned linked fixture");check(host.check_asset("asset")["state"]=="missing","Missing link is explicit");
    rejects("ASSET_MISSING",[&]{host.update_asset("asset","reload",{},host.session.revision());});check(host.session.document()==accepted,"Missing Reload keeps accepted pixels");
    const auto relocated=directory+"/relocated.png";write(relocated,original);
    dispatch(host,{{"op","asset"},{"asset","asset"},{"action","relink"},{"path",relocated}});
    check(host.session.document().raster_assets.at("asset").locator==relocated.toStdString(),"Relink records new absolute locator");
    const auto relinked=host.session.document();check(QFile::remove(relocated),"Remove relocated fixture");
    dispatch(host,{{"op","asset"},{"asset","asset"},{"action","embed"}});
    check(host.session.document().raster_assets.at("asset").mode=="embedded"&&host.session.document().raster_assets.at("asset").payload->bytes()==original,"Embed uses accepted bytes even after file disappears");
    host.session.undo(host.session.revision());host.edited();check(host.session.document()==relinked,"Embed Undo restores link and original bytes");
    const auto native=directory+"/images.nect";host.save(native);host.recover();const auto saved=encode(host.session.document());
    const auto recovery=host.persistence()["recovery_file"].toString();
    host.open(native);check(encode(host.session.document())==saved&&host.asset_status("asset")["state"]=="unchecked","Reopen has exact accepted pixels and no filesystem observation");
    check(host.check_asset("asset")["state"]=="missing","Explicit check after reopen finds missing source");
    host.open_recovery(recovery);check(encode(host.session.document())==saved&&host.file_path.isEmpty(),"Recovery snapshot is self-contained even for missing links");
    auto jpeg=QImage(8,5,QImage::Format_RGB32);jpeg.fill(QColor(30,160,230));const auto jpeg_path=directory+"/source.jpg";
    check(jpeg.save(jpeg_path,"JPEG",95),"Create independent Qt JPEG fixture");
    host.import_image(jpeg_path,"embedded",comp,"","jpeg-asset","jpeg-image","JPEG",0,0,host.session.revision());
    check(host.session.document().raster_assets.at("jpeg-asset").payload->mime()=="image/jpeg","JPEG import through the same Host pipeline");
    const auto state=host.session.document();rejects("REVISION_CONFLICT",[&]{host.import_image("C:/missing.png","embedded",comp,"","no","no","No",0,0,999);});
    rejects("INVALID_ASSET_LOCATOR",[&]{host.import_image("https://example.invalid/image.png","embedded",comp,"","no","no","No",0,0,host.session.revision());});
    check(host.session.document()==state,"Stale and unsupported location imports never mutate state");
    host.flush();
}
template<class T>T* visible(Window& w,const QString& name) {
    QApplication::processEvents();for(auto* item:w.findChildren<T*>())if(item->isVisible()&&item->objectName()==name)return item;
    throw std::runtime_error("Missing Image control: "+name.toStdString());
}
void gui(const QString& directory) {
    Window window(directory+"/ui-recovery");window.show();QApplication::processEvents();const auto path=directory+"/ui.png";const auto bytes=png(200,100,40);write(path,bytes);
    // Exercise the actual Add action and file dialog; only the file chooser is automated.
    QTimer::singleShot(0,&window,[&]{auto* dialog=window.findChild<QFileDialog*>();check(dialog,"Import opens file chooser");dialog->selectFile(path);QMetaObject::invokeMethod(dialog,"accept",Qt::QueuedConnection);});
    auto* import=window.findChild<QAction*>("import-linked-image");check(import,"Linked Image menu exists");import->trigger();QApplication::processEvents();
    auto& s=window.host.session;check(s.document().objects.size()==1,"GUI import creates an Image");const auto object=window.canvas->selected_object,asset=s.document().objects.at(object).image->asset;
    check(s.document().raster_assets.at(asset).payload->bytes()==bytes,"GUI retains exact source bytes");
    const Ref width{object,"","image.width"};QLineEdit* input=nullptr;
    const auto ref=QJsonDocument(QJsonObject{{"object",QString::fromStdString(object)},{"point",""},{"field","image.width"}}).toJson(QJsonDocument::Compact);
    for(auto* item:window.findChildren<QLineEdit*>())if(item->isVisible()&&item->property("nect-reference").toByteArray()==ref)input=item;
    check(input,"Image dimensions use ordinary property controls");window.findChild<QScrollArea*>()->ensureWidgetVisible(input);input->setFocus();input->selectAll();QTest::keyClicks(input,"160");QTest::keyClick(input,Qt::Key_Return);QApplication::processEvents();
    check(evaluate(s.document()).at(width)==160,"GUI resize reaches Session");
    write(path,png(20,220,80));QTest::mouseClick(visible<QPushButton>(window,"image-check-link"),Qt::LeftButton);QApplication::processEvents();
    check(visible<QLabel>(window,"image-link-status")->text().startsWith("changed"),"Inspector shows changed link");
    const auto before=s.document();QTest::mouseClick(visible<QPushButton>(window,"image-reload"),Qt::LeftButton);QApplication::processEvents();
    check(s.document().raster_assets.at(asset).payload->bytes()!=bytes&&evaluate(s.document()).at(width)==160,"GUI Reload preserves resized placement");
    s.undo(s.revision());window.host.edited();check(s.document()==before,"GUI reload is one Undo");
    const auto relocated=directory+"/ui-relocated.png";write(relocated,png(40,80,210));
    auto* relink=visible<QPushButton>(window,"image-relink");
    QTimer::singleShot(0,&window,[&]{auto* dialog=window.findChild<QFileDialog*>();check(dialog,"Relink opens file chooser");dialog->selectFile(relocated);QMetaObject::invokeMethod(dialog,"accept",Qt::QueuedConnection);});
    QTest::mouseClick(relink,Qt::LeftButton);QApplication::processEvents();
    check(s.document().raster_assets.at(asset).locator==relocated.toStdString()&&evaluate(s.document()).at(width)==160,"GUI Relink keeps display size");
    QTest::mouseClick(visible<QPushButton>(window,"image-embed"),Qt::LeftButton);QApplication::processEvents();
    check(s.document().raster_assets.at(asset).mode=="embedded","GUI Embed shares command state");
    check(!window.findChild<QPushButton*>("stack-add")||!window.findChild<QPushButton*>("stack-add")->isVisible(),"Image does not offer vector stack controls");
    QTimer::singleShot(0,&window,[&]{auto* dialog=window.findChild<QDialog*>("image-assets-dialog");check(dialog,"Asset library opens");dialog->findChild<QPushButton*>("assets-place")->click();dialog->accept();});
    window.findChild<QAction*>("image-assets")->trigger();QApplication::processEvents();
    check(s.document().objects.size()==2&&s.document().raster_assets.size()==1,"Asset library places a shared source without duplicating bytes");
    window.close();QApplication::processEvents();
}
void pixels() {
    auto d=empty_document("d","c","a");d.compositions[0].artboards[0].width=640;d.compositions[0].artboards[0].height=480;
    d.raster_assets.emplace("asset",RasterAsset{"asset","Solid","embedded","",make_raster(png(200,100,40))});
    Session s(d);s.apply({CreateImage{"c","","image","Image",{"asset",{160},{100}}},Set{{"image","","transform.tx"},100},Set{{"image","","transform.ty"},100}},0);
    Canvas canvas(s);QString error;canvas.error=[&](const QString& e){error=e;};canvas.resize(740,580);canvas.show();QApplication::processEvents();canvas.fit_artboard();canvas.set_selection({});canvas.set_show_mask_outline(false);
    const auto screen=[&](double x,double y){return QPoint(qRound(canvas.width()/2.0+(x-320)*canvas.zoom()),qRound(canvas.height()/2.0+(y-240)*canvas.zoom()));};
    const auto color=[&](double x,double y,QColor expected){QApplication::processEvents();const auto image=canvas.grab().toImage();const auto p=screen(x,y);const auto actual=image.pixelColor(qRound(p.x()*image.devicePixelRatio()),qRound(p.y()*image.devicePixelRatio()));check(std::abs(actual.red()-expected.red())<=2&&std::abs(actual.green()-expected.green())<=2&&std::abs(actual.blue()-expected.blue())<=2,"Canvas pixel expected "+expected.name().toStdString()+", got "+actual.name().toStdString());};
    color(150,140,QColor(200,100,40));QTest::mouseClick(&canvas,Qt::LeftButton,Qt::NoModifier,screen(150,140));check(canvas.selected_object=="image","Image interior is selectable");
    const auto before_move=s.document();QTest::mousePress(&canvas,Qt::LeftButton,Qt::NoModifier,screen(150,140));QTest::mouseMove(&canvas,screen(170,150));QTest::mouseRelease(&canvas,Qt::LeftButton,Qt::NoModifier,screen(170,150));
    check(evaluate(s.document()).at({"image","","transform.tx"})==120&&evaluate(s.document()).at({"image","","transform.ty"})==110,"Canvas drag moves the Image using normal transforms");
    s.undo(s.revision());check(s.document()==before_move,"Image drag is one exact Undo");canvas.refresh();canvas.set_selection({});
    auto circle=default_primitive("source","nect.shape.circle");circle.parameters.at("center_x").literal=180;circle.parameters.at("center_y").literal=150;circle.parameters.at("radius").literal=35;
    s.apply({CreatePrimitive{"c","","mask","Mask",circle},SetVisibility{"mask",false},SetMask{"image",GeometryMask{"clip","mask"}}},s.revision());canvas.refresh();
    color(180,150,QColor(200,100,40));color(110,110,QColor(250,250,250));
    auto paper=default_primitive("paper-source","nect.shape.rectangle");paper.parameters.at("center_x").literal=320;paper.parameters.at("center_y").literal=240;paper.parameters.at("width").literal=640;paper.parameters.at("height").literal=480;
    auto fill=default_operation("paper-fill","nect.paint.fill");fill.parameters.at("b").literal=200.0/255;
    s.apply({CreatePrimitive{"c","","paper","Backdrop",paper},AddOperation{"paper",fill,0},ReorderObjects{"c","",{"paper","image","mask"}},SetCompositing{"image","multiply",false},Set{{"image","","composite.opacity"},.5}},s.revision());canvas.refresh();
    color(180,150,QColor(0,0,116));color(110,110,QColor(0,0,200));check(error.isEmpty(),"Image mask/blend renders without error");
    s.apply({SetCompositing{"image","screen",false}},s.revision());canvas.refresh();color(180,150,QColor(100,50,204));
    canvas.close();
}
}
int main(int argc,char** argv) {
    qputenv("QT_QPA_PLATFORM","offscreen");QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);QApplication app(argc,argv);
    try{QTemporaryDir directory;check(directory.isValid(),"Owned scratch");lifecycle(directory.path());gui(directory.path());pixels();std::cout<<"PASS "<<checks<<" desktop Image lifecycle/UI/pixel checks\n";return 0;}
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}

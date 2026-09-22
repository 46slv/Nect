#include "canvas.hpp"
#include "window.hpp"
#include <QTimer>
#include <QAction>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QStatusBar>
#include "host.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QColorSpace>
#include <iostream>
#include <cmath>
using namespace nect;
using namespace nect::desktop;
void check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
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

int main(int argc,char** argv) {
 QApplication app(argc,argv);
 try {
  auto d=document(false);add(d,rectangle("a",100,100,160,160,Qt::red));add(d,rectangle("b",180,120,160,160,Qt::red));
  group(d,"group",{"a","b"});d.objects.at("group").compositing.opacity.literal=.5;
  auto& board=d.compositions[0].artboards[0];board.x=100;board.y=100;board.width=300;board.height=240;
  auto image=Canvas::render_artboard(d,"composition","artboard",2,false);
  check(image.size()==QSize(600,480),"Explicit crop/resolution");
  check(std::abs(image.pixelColor(240,140).alpha()-128)<=1,"Group alpha applied once at overlap");
  check(image.pixelColor(80,100).red()==255,"Straight red color preserved");
  check(image.pixelColor(590,470).alpha()==0,"No paper or overlays");
  auto white=Canvas::render_artboard(d,"composition","artboard",1,true);
  check(white.pixelColor(299,239)==QColor(Qt::white),"Explicit white backdrop");
  check(white.pixelColor(120,70).green()==127||white.pixelColor(120,70).green()==128,"White applied after composition");
  check(image.colorSpace()==QColorSpace(QColorSpace::SRgb),"Output declares sRGB");
  QTemporaryDir dir;check(dir.isValid(),"Scratch");Host host(dir.path()+"/recovery");host.session=Session(d);
  const auto before=encode(host.session.document());const auto path=dir.path()+"/out.png";
  const auto result=host.export_png(path,"composition","artboard",2,false,0);
  check(result["width"].toInt()==600,"Host result dimensions");
  QImage decoded(path);check(decoded.convertToFormat(image.format())==image,"PNG round trip preserves pixels");
  check(encode(host.session.document())==before&&host.session.revision()==0,"Export leaves native state/revision unchanged");
  QFile output(path);check(output.open(QIODevice::ReadOnly),"Read exported file");const auto bytes=output.readAll();output.close();
  for(double scale:{0.,-1.,17.,16.}) {
   bool rejected=false;try{host.export_png(path,"composition","artboard",scale,false,0);}catch(const Error&){rejected=true;}
   check(rejected,"Invalid scale/oversized output rejects");
   check(output.open(QIODevice::ReadOnly),"Reopen old output");check(output.readAll()==bytes,"Failure preserves existing output");output.close();
  }
  bool rejected=false;try{host.export_png(path,"composition","missing",1,false,0);}catch(const Error&){rejected=true;}check(rejected,"Unknown artboard rejects");
  host.file_path=path;rejected=false;try{host.export_png(path,"composition","artboard",1,false,0);}catch(const Error& e){rejected=e.code=="EXPORT_TARGET";}check(rejected,"Native destination protected");host.file_path.clear();
  QJsonObject request{{"op","export_png"},{"session_id",host.session_id},{"document_id",QString::fromStdString(d.id)},
   {"expected_revision",0},{"path",path},{"composition","composition"},{"artboard","artboard"},{"scale",1},{"background","transparent"}};
  auto reply=QJsonDocument::fromJson(host.dispatch(QJsonDocument(request).toJson())).object();check(reply["ok"].toBool(),"Production envelope export");
  request["expected_revision"]=1;reply=QJsonDocument::fromJson(host.dispatch(QJsonDocument(request).toJson())).object();check(!reply["ok"].toBool(),"Stale revision rejects");
  host.session.begin_gesture(0);rejected=false;try{host.export_png(path,"composition","artboard",1,false,0);}catch(const Error& e){rejected=e.code=="GESTURE_ACTIVE";}check(rejected,"Gesture draft not exported");host.session.cancel_gesture();
  rejected=false;try{host.export_png(dir.path()+"/missing/out.png","composition","artboard",1,false,0);}catch(const Error& e){rejected=e.code=="IO_ERROR";}check(rejected,"Write failure is explicit");
  host.import_image(path,"linked","composition","","exported-asset","exported-image","Reimport",0,0,0);
  check(host.session.document().raster_assets.at("exported-asset").payload->width()==300,"Export reimports through production WIC parser");
  rejected=false;try{host.export_png(path,"composition","artboard",1,false,1);}catch(const Error& e){rejected=e.code=="EXPORT_TARGET";}check(rejected,"Linked source protected");
  Window window(dir.path()+"/ui-recovery");window.host.session=Session(d);window.refresh();
  auto* export_action=window.findChild<QAction*>("export-png");check(export_action,"Discoverable export action");
  bool valid_dimensions=false,invalid_disabled=false;
  const auto ui_path=dir.path()+"/ui.png";
  QTimer::singleShot(0,&window,[&]{
   auto* dialog=window.findChild<QDialog*>("png-export-dialog");
   auto* scale=dialog->findChild<QDoubleSpinBox*>("png-scale");auto* buttons=dialog->findChild<QDialogButtonBox*>();
   dialog->findChild<QLineEdit*>("png-path")->setText(ui_path);
   scale->setValue(16);invalid_disabled=!buttons->button(QDialogButtonBox::Save)->isEnabled();
   scale->setValue(.5);valid_dimensions=dialog->findChild<QLabel*>("png-dimensions")->text().contains("150")&&buttons->button(QDialogButtonBox::Save)->isEnabled();
   dialog->findChild<QComboBox*>("png-background")->setCurrentIndex(1);buttons->button(QDialogButtonBox::Save)->click();
  });
  export_action->trigger();check(valid_dimensions&&invalid_disabled,"Live size and preflight output limit");
  QImage ui_output(ui_path);check(ui_output.size()==QSize(150,120)&&ui_output.pixelColor(149,119)==QColor(Qt::white),"Settings drive real export");
  bool remembered=false;
  QTimer::singleShot(0,&window,[&]{
   auto* dialog=window.findChild<QDialog*>("png-export-dialog");
   remembered=dialog->findChild<QDoubleSpinBox*>("png-scale")->value()==.5&&dialog->findChild<QComboBox*>("png-background")->currentIndex()==1&&dialog->findChild<QLineEdit*>("png-path")->text()==ui_path;
   dialog->findChild<QDoubleSpinBox*>("png-scale")->setValue(2);dialog->reject();
  });
  export_action->trigger();check(remembered&&window.host.session.revision()==0,"Successful settings remembered; cancel does not edit");
  const auto stale_path=dir.path()+"/stale.png";
  QTimer::singleShot(0,&window,[&]{
   auto* dialog=window.findChild<QDialog*>("png-export-dialog");
   dialog->findChild<QLineEdit*>("png-path")->setText(stale_path);
   window.host.session.apply({Rename{"a","Changed during export"}},0);
   dialog->accept();
  });
  export_action->trigger();check(!QFile::exists(stale_path)&&window.statusBar()->currentMessage().startsWith("REVISION_CONFLICT"),"Settings revision frozen during concurrent API changes");
  std::cout<<"PNG export crop/alpha/compositing/encoding/integrity/failure contracts passed\n";
 } catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}

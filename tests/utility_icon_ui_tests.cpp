#include "window.hpp"
#include "nect/io.hpp"
#include <QAction>
#include <QApplication>
#include <QDir>
#include <QImage>
#include <QPainter>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const char*why){if(!ok)throw std::runtime_error(why);++checks;}
void events(){QApplication::processEvents();}
void check_rendered_glyph(const QImage& image,const QIcon& icon) {
 const auto dpr=image.devicePixelRatio();
 const auto center=image.copy(QRect(qRound((18-10)*dpr),qRound((16-10)*dpr),qRound(20*dpr),qRound(20*dpr)));
 const auto mask=icon.pixmap(QSize(20,20),dpr,QIcon::Normal,QIcon::Off).toImage();
 check(mask.size()==center.size(),"rendered glyph mask matches physical capture dimensions");
 int background_total=0,background_count=0;
 for(int y=0;y<mask.height();++y)for(int x=0;x<mask.width();++x)if(qAlpha(mask.pixel(x,y))<=8){background_total+=qGray(center.pixel(x,y));++background_count;}
 check(background_count>0,"transparent glyph mask supplies captured background");
 const double background=double(background_total)/background_count;int solid=0,visible=0;
 for(int y=0;y<mask.height();++y)for(int x=0;x<mask.width();++x)if(qAlpha(mask.pixel(x,y))>=192){++solid;visible+=qGray(center.pixel(x,y))>=background+15;}
 check(solid>=20*dpr*dpr&&visible>=solid*0.60,"actual captured glyph is visible against its background, not a blank capture");
}
void check_icon_pixels(const QIcon& icon) {
 for(const auto mode:{QIcon::Normal,QIcon::Active,QIcon::Disabled,QIcon::Selected})for(const auto state:{QIcon::Off,QIcon::On})for(const qreal dpr:{1.0,2.0,3.0}){
  const auto pixmap=icon.pixmap(QSize(20,20),dpr,mode,state);const auto pixels=pixmap.toImage();
  check(pixmap.size()==QSize(qRound(20*dpr),qRound(20*dpr))&&qAbs(pixmap.devicePixelRatio()-dpr)<0.01,"icon mode/state has exact physical size and DPR");
  int ink=0,transparent=0;for(int y=0;y<pixels.height();++y)for(int x=0;x<pixels.width();++x){const auto alpha=qAlpha(pixels.pixel(x,y));ink+=alpha>128;transparent+=alpha==0;}
  check(ink>10*dpr*dpr&&transparent>10*dpr*dpr,"each mode/state/DPR contains real transparent-backed glyph pixels");
 }
}
QImage capture(Window&w,QToolButton*b){events();QTest::qWait(180);const auto whole=w.grab().toImage();const auto dpr=whole.devicePixelRatio();const auto point=b->mapTo(&w,QPoint(0,0));return whole.copy(QRect(qRound(point.x()*dpr),qRound(point.y()*dpr),qRound(b->width()*dpr),qRound(b->height()*dpr)));}
}
int main(int argc,char**argv){qputenv("QT_QPA_PLATFORM","offscreen");QApplication app(argc,argv);try{
 QTemporaryDir dir;check(dir.isValid(),"owned scratch available");QSettings settings(dir.filePath("settings.ini"),QSettings::IniFormat);Window w(dir.path(),std::make_unique<FolderLibrary>(settings));w.show();w.activateWindow();events();
 const auto before=encode(w.host.session.document());const auto revision=w.host.session.revision();const auto history=w.host.session.history();
 auto* guides=w.findChild<QToolButton*>("utility-show-guides");auto* grid=w.findChild<QToolButton*>("utility-show-grid");auto* snap=w.findChild<QToolButton*>("utility-snap");auto* action=w.findChild<QAction*>("canvas-snap");auto* scroll=w.findChild<QScrollArea*>("canvas-utility-scroll");
 check(guides&&grid&&snap&&action&&scroll,"all existing controls available");
 const std::vector<QToolButton*> buttons{guides,grid,snap};const QStringList names{"Guide","Grid","Snap"};std::vector<std::vector<QImage>> images;
 for(std::size_t i=0;i<buttons.size();++i){auto*b=buttons[i];check(!b->icon().isNull(),"binary utility has an icon");check_icon_pixels(b->icon());check(b->toolButtonStyle()==Qt::ToolButtonIconOnly,"binary utility is icon-only");check(b->isCheckable(),"icon itself remains checkable");check(b->accessibleName().contains(names.at(int(i)),Qt::CaseInsensitive)&&b->toolTip().contains(names.at(int(i)),Qt::CaseInsensitive),"name available for tooltip and focus accessibility");
  auto set_checked=[&](bool value){if(b==snap)action->setChecked(value);else b->setChecked(value);events();check(b->isChecked()==value,"checked state reaches button");if(b==snap)check(action->isChecked()==value&&w.canvas->snap_enabled()==value,"Snap capture uses authoritative action and Canvas state");};
  scroll->ensureWidgetVisible(b);events();const auto geometry=b->geometry();std::vector<QImage> states;
  w.canvas->setFocus();QTest::mouseMove(w.canvas,QPoint(5,5));set_checked(false);states.push_back(capture(w,b));
  set_checked(true);states.push_back(capture(w,b));check(states[0]!=states[1],"ON and OFF render distinctly without hover");
  set_checked(false);QTest::mouseMove(b,b->rect().center());states.push_back(capture(w,b));check(b->underMouse(),"pointer enters icon control");check(states[0]!=states[2],"hover feedback is visible");
  QTest::mouseMove(w.canvas,QPoint(5,5));b->setFocus(Qt::TabFocusReason);states.push_back(capture(w,b));check(b->hasFocus(),"keyboard focus reaches icon");check(states[0]!=states[3],"focus feedback is visible");
  w.canvas->setFocus();if(b==snap)action->setEnabled(false);else b->setEnabled(false);states.push_back(capture(w,b));check(states[0]!=states[4],"disabled feedback is visible");b->click();check(!b->isChecked(),"disabled activation does not toggle");if(b==snap)action->setEnabled(true);else b->setEnabled(true);
  check(b->geometry()==geometry,"all states preserve hit geometry");check(!b->icon().isNull(),"action/state changes retain icon");
  QTest::mouseClick(b,Qt::LeftButton);events();check(b->isChecked(),"icon pointer click toggles state");b->setFocus();QTest::keyClick(b,Qt::Key_Space);events();check(!b->isChecked(),"icon keyboard activation toggles state");check(b->geometry()==geometry,"pointer and keyboard preserve geometry");for(const auto&state:states)check_rendered_glyph(state,b->icon());images.push_back(std::move(states));
 }
 check(guides->icon().pixmap(20,20).toImage()!=snap->icon().pixmap(20,20).toImage()&&guides->icon().pixmap(20,20).toImage()!=grid->icon().pixmap(20,20).toImage()&&grid->icon().pixmap(20,20).toImage()!=snap->icon().pixmap(20,20).toImage(),"three controls have distinct icon identities");
 guides->setChecked(false);grid->setChecked(true);action->setChecked(true);events();check(!w.canvas->show_guides()&&w.canvas->show_grid()&&w.canvas->snap_enabled()&&snap->isChecked(),"visibility and Snap remain independent");action->trigger();events();check(!w.canvas->snap_enabled()&&!snap->isChecked(),"menu action synchronizes icon button");snap->click();events();check(action->isChecked()&&w.canvas->snap_enabled(),"icon synchronizes existing menu action");
 w.canvas->set_show_guides(true);w.canvas->set_show_grid(false);w.canvas->set_snap_enabled(false);events();check(guides->isChecked()&&!grid->isChecked()&&!snap->isChecked()&&!action->isChecked(),"Canvas owner readback synchronizes buttons and action");w.refresh();events();for(auto*b:buttons)check(!b->icon().isNull()&&b->toolButtonStyle()==Qt::ToolButtonIconOnly,"refresh preserves icon-only presentation");
 w.resize(1000,650);events();auto* bar=scroll->horizontalScrollBar();check(scroll->viewport()->height()>=30,"narrow strip retains control height");check(scroll->widget()->width()<=scroll->viewport()->width()||bar->maximum()>0,"overflow exposes horizontal scrolling");for(const char*name:{"utility-show-guides","utility-show-grid","utility-snap","utility-fit","utility-setup","utility-shortcut-help"}){auto*b=w.findChild<QToolButton*>(name);check(b,"utility control preserved");scroll->ensureWidgetVisible(b);events();check(b->visibleRegion().contains(b->rect().center()),"every utility control remains reachable at narrow width");}
 for(const char*name:{"utility-fit","utility-setup","utility-shortcut-help"})check(w.findChild<QToolButton*>(name)->toolButtonStyle()==Qt::ToolButtonTextOnly,"nonbinary controls retain their presentation");
 check(encode(w.host.session.document())==before&&w.host.session.revision()==revision&&w.host.session.history()==history,"all view-only interactions preserve exact native bytes revision and History");
 const auto scale=w.devicePixelRatioF();if(qEnvironmentVariable("QT_SCALE_FACTOR")=="2")check(scale>=1.9,"high-DPI process actually renders at scaled DPR");
 const auto output=qEnvironmentVariable("NECT_TEST_ARTIFACT_DIR");if(!output.isEmpty()){QDir().mkpath(output);QImage sheet(qRound(430*scale),qRound(235*scale),QImage::Format_ARGB32_Premultiplied);sheet.setDevicePixelRatio(scale);sheet.fill(QColor("#1e252b"));QPainter p(&sheet);p.setPen(QColor("#dbe4ee"));const QStringList labels{"OFF","ON","Hover","Focus","Disabled"};for(int j=0;j<5;++j)p.drawText(QPointF(90+j*65,22),labels[j]);for(int i=0;i<3;++i){p.drawText(QPointF(8,60+i*65),names[i]);for(int j=0;j<5;++j)p.drawImage(QPointF(90+j*65,38+i*65),images[i][j]);}p.end();check(sheet.save(output+"/utility-icons-"+QString::number(scale,'g',3)+"x.png"),"state contact sheet saved");}
 std::cout<<"PASS "<<checks<<" utility icon/state/geometry checks at DPR "<<scale<<'\n';return 0;
 }catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}

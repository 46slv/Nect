#include "window.hpp"
#include <QApplication>
#include <QClipboard>
#include <QCheckBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QTemporaryDir>
#include <QTest>
#include <iostream>
using namespace nect;
using namespace nect::desktop;
namespace {
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
template<class T>T* named(Window& w,const QString& label){QApplication::processEvents();for(auto* p:w.findChildren<T*>())if(p->isVisible()&&p->accessibleName()==label)return p;throw std::runtime_error("Missing widget "+label.toStdString());}
void numeric(Window& w,const QString& label,const char* source){auto* p=named<QLineEdit>(w,label);w.findChild<QScrollArea*>()->ensureWidgetVisible(p);p->setFocus();p->selectAll();QTest::keyClicks(p,source);QTest::keyClick(p,Qt::Key_Return);QApplication::processEvents();}
void controls(){
    QTemporaryDir tmp;Window w(tmp.path());w.host.session=Session(empty_document("doc","comp","board"));auto& s=w.host.session;
    auto source=default_primitive("source","nect.shape.rectangle");source.parameters.at("width").literal=80;source.parameters.at("height").literal=40;
    s.apply({CreatePrimitive{"comp","","rect","Rectangle",source}},s.revision());w.host.edited();w.show();QApplication::processEvents();w.canvas->set_selection("rect");
    const Ref width{"rect","","generator.width"},height{"rect","","generator.height"};
    const auto original=encode(s.document());auto revision=s.revision();
    numeric(w,"Width","=ref(\"rect\",\"\",\"generator.height\") * 3");
    check(s.revision()==revision+1&&evaluate(s.document()).at(width)==120,"Numeric row accepts persistent expression");
    check(property(s.document(),width).expression.has_value()&&named<QLineEdit>(w,"Width")->text()=="120","Compact field displays evaluated number separately from source");
    numeric(w,"Width","500");check(evaluate(s.document()).at(width)==120&&property(s.document(),width).expression.has_value(),"Typing cannot silently unlink a formula");
    s.undo(s.revision());w.host.edited();check(encode(s.document())==original,"Expression undo restores authored state exactly");
    auto* field=named<QLineEdit>(w,"Width");field->setFocus();field->selectAll();QApplication::clipboard()->setText("=ref(\"rect\",\"\",\"generator.height\")\n * 4");QTest::keyClick(field,Qt::Key_V,Qt::ControlModifier);QApplication::processEvents();
    auto* draft=named<QPlainTextEdit>(w,"Width expression");check(draft->toPlainText().contains('\n')&&encode(s.document())==original,"Multiline paste opens inline draft without committing");
    draft->setPlainText("1 /");QTest::keyClick(draft,Qt::Key_Return,Qt::ControlModifier);check(encode(s.document())==original,"Incomplete expression does not replace valid data");
    draft->setPlainText("ref(\"rect\",\"\",\"generator.height\")\n * 4");QTest::keyClick(draft,Qt::Key_Return,Qt::ControlModifier);QApplication::processEvents();check(evaluate(s.document()).at(width)==160,"Ctrl+Enter atomically applies multiline draft");
    auto* fx=named<QPushButton>(w,"Width expression editor");fx->click();draft=named<QPlainTextEdit>(w,"Width expression");draft->setPlainText("900");
    s.apply({Set{height,50}},s.revision());w.host.edited();draft=named<QPlainTextEdit>(w,"Width expression");check(draft->toPlainText()=="900","External revision refresh preserves draft text");
    const auto before=encode(s.document());QTest::keyClick(draft,Qt::Key_Return,Qt::ControlModifier);check(encode(s.document())==before,"Stale draft rejects instead of overwriting external changes");
    QTest::keyClick(draft,Qt::Key_Escape);QApplication::processEvents();check(encode(s.document())==before,"Escape discards UI draft only");
    s.apply({Link{width,{height,2,0}}},s.revision());w.host.edited();revision=s.revision();numeric(w,"Width","=75");
    check(s.revision()==revision&&property(s.document(),width).binding.has_value(),"Formula typing cannot replace an existing link implicitly");
    draft=named<QPlainTextEdit>(w,"Width expression");QCheckBox* replace=nullptr;for(auto* p:w.findChildren<QCheckBox*>())if(p->isVisible()&&p->text()=="Replace existing link")replace=p;
    check(replace,"Explicit link replacement action is visible");replace->setChecked(true);QTest::keyClick(draft,Qt::Key_Return,Qt::ControlModifier);QApplication::processEvents();
    check(evaluate(s.document()).at(width)==75&&!property(s.document(),width).binding&&property(s.document(),width).expression,"Explicit replacement installs expression in one command");
    check(encode(decode(encode(s.document())))==encode(s.document()),"Expression UI result survives native reopen");
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","offscreen");QApplication app(argc,argv);try{controls();std::cout<<"Expression UI passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

#include "window.hpp"
#include <QApplication>
#include <QClipboard>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QTemporaryDir>
#include <QTest>
#include <algorithm>
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
    auto peer=default_primitive("peer-source","nect.shape.rectangle");peer.parameters.at("height").literal=40;
    s.apply({CreatePrimitive{"comp","","rect-peer","Rectangle",peer}},s.revision());w.host.edited();
    auto* fx_search=named<QPushButton>(w,"Width expression editor");fx_search->click();
    draft=named<QPlainTextEdit>(w,"Width expression");draft->clear();
    auto* panel=draft->parentWidget();check(panel&&panel->objectName()=="nect-expression-panel","Visible expression panel owns the draft");
    const auto buttons=panel->findChildren<QPushButton*>();
    const auto insert_it=std::find_if(buttons.begin(),buttons.end(),
        [](QPushButton* button){return button->text()=="Insert reference…";});
    check(insert_it!=buttons.end(),"Expression reference insert button exists");auto* insert=*insert_it;
    const auto before_search=encode(s.document());const auto search_revision=s.revision();
    insert->click();QApplication::processEvents();
    QDialog* dialog=nullptr;
    for(auto* top:QApplication::topLevelWidgets())if(auto* candidate=qobject_cast<QDialog*>(top);candidate&&candidate->isVisible()&&candidate->windowTitle()=="Insert expression reference")dialog=candidate;
    check(dialog,"Expression reference dialog opens");
    auto* search=dialog->findChild<QLineEdit*>();auto* list=dialog->findChild<QListWidget*>();
    check(search&&list,"Expression dialog exposes search and source list");
    search->setText("ReCt-PeEr//generator.height");QApplication::processEvents();
    QListWidgetItem* selected=nullptr;
    for(int i=0;i<list->count();++i)if(!list->item(i)->isHidden()){
        check(!selected,"Exact stable Ref search disambiguates duplicate display names");selected=list->item(i);
    }
    check(selected&&selected->data(Qt::UserRole).toString()=="ref(\"rect-peer\",\"\",\"generator.height\")"&&
          s.revision()==search_revision&&encode(s.document())==before_search,
          "Case-insensitive stable object/point/field search retains the selected exact Ref without committing");
    list->setCurrentItem(selected);
    search->setText("rect//generator.height");QApplication::processEvents();
    check(!list->currentItem()&&encode(s.document())==before_search,
          "Hiding a selected source clears the pending exact Ref without modifying the document");
    QTest::mouseClick(dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok),Qt::LeftButton);QApplication::processEvents();
    check(dialog->isVisible()&&draft->toPlainText().isEmpty()&&s.revision()==search_revision,
          "Confirming a hidden prior choice does not insert or commit it");
    search->setText("ReCt-PeEr//generator.height");QApplication::processEvents();
    list->setCurrentItem(selected);
    QTest::mouseClick(dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok),Qt::LeftButton);QApplication::processEvents();
    check(draft->toPlainText()=="ref(\"rect-peer\",\"\",\"generator.height\")"&&encode(s.document())==before_search,
          "Expression insertion uses the exact chosen source ID and leaves draft uncommitted");
    QTest::keyClick(draft,Qt::Key_Return,Qt::ControlModifier);QApplication::processEvents();
    check(s.revision()==search_revision+1&&property(s.document(),width).expression->source=="ref(\"rect-peer\",\"\",\"generator.height\")"&&
          evaluate(s.document()).at(width)==40,"Expression draft commits the exact duplicate-name source through Session");
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","offscreen");QApplication app(argc,argv);try{controls();std::cout<<"Expression UI passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

#include "text_layout_batch_control.hpp"
#include "host.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QTemporaryDir>
#include <QPointer>
#include <iostream>
using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const char* reason){++checks;if(!ok)throw std::runtime_error(reason);}
Document fixture(){
 auto d=empty_document("sizing-document","composition","artboard");
 for(const auto* id:{"source","first","second"}){Object o;o.id=id;o.name=id;o.kind=Kind::text;o.text=default_text(std::string(id)+"-text",id);d.objects.emplace(id,o);d.compositions[0].roots.push_back(id);}
 auto& first=*d.objects.at("first").text;first.parameters.at("frame_width").binding=Binding{{"source","","text.frame_width"},.5,0};
 first.family_driver=TextFamilyDriver{{"source","","text.family"}};first.alignment="center";first.direction="vertical";
 auto& second=*d.objects.at("second").text;second.layout="frame";second.parameters.at("frame_height").literal=97.123456789;
 return d;
}
struct Controls{
 QTemporaryDir directory;Host host;QWidget parent;QPointer<QWidget> box;
 Controls():host(directory.path()){host.session=Session(fixture());host.changed=[this]{rebuild();};rebuild();}
 ~Controls(){host.changed={};}
 void rebuild(){delete box.data();box=make_text_layout_batch_controls(host,{"first","second"},&parent);}
 QComboBox* editor(){return box->findChild<QComboBox*>("text-layout-batch-editor");}
 QPushButton* apply(){return box->findChild<QPushButton*>("text-layout-batch-apply");}
 QString error(){return box->findChild<QLabel*>("text-layout-batch-status")->text();}
 void choose(const char* value){editor()->setCurrentIndex(editor()->findData(QString::fromLatin1(value)));}
};
}
int main(int argc,char** argv){QApplication app(argc,argv);try{
 Controls c;const auto before=c.host.session.document();const auto bytes=encode(before);
 check(c.editor()->currentText()=="Mixed"&&!c.apply()->isEnabled(),"Mixed is explicit and not an authored enum");
 c.choose("frame");check(encode(c.host.session.document())==bytes,"Draft selection doesn't change source");c.apply()->click();
 auto expected=before;expected.objects.at("first").text->layout="frame";
 check(c.host.session.document()==expected&&c.host.session.revision()==1,"Only sizing modes change in one commit");
 check(decode(encode(c.host.session.document()))==expected,"Native roundtrip preserves exact frame sources");
 check(!c.apply()->isEnabled(),"Shared unchanged sizing mode is no-op");
 c.host.session.undo(1);c.rebuild();check(c.host.session.document()==before,"Undo restores mixed modes and every other field");
 c.host.session.redo(c.host.session.revision());c.rebuild();check(c.host.session.document()==expected,"Redo restores one batch");
 c.choose("auto");c.apply()->click();check(c.host.session.document().objects.at("second").text->parameters==before.objects.at("second").text->parameters,"Frame values survive Auto mode");
 c.choose("frame");const auto rev=c.host.session.revision();c.host.session.apply({Rename{"source","Changed"}},rev);const auto stale=encode(c.host.session.document());
 c.apply()->click();check(encode(c.host.session.document())==stale&&c.error().startsWith("REVISION_CONFLICT"),"Stale draft refuses atomically");
 auto linked=fixture();linked.objects.at("second").text->layout_driver=TextLayoutDriver{{"source","","text.layout"}};
 c.host.session=Session(linked);c.host.session_id+="-linked";c.rebuild();
 check(!c.editor()->isEnabled()&&!c.apply()->isEnabled()&&c.error().startsWith("DRIVEN_PROPERTY"),"One linked sizing mode refuses whole batch");
 check(c.host.session.document()==linked,"Refusal never unlinks or clears frame state");
 c.host.session=Session(fixture());c.host.session_id+="-fresh";c.rebuild();c.choose("frame");c.host.session.begin_gesture(0);
 c.apply()->click();check(c.host.session.revision()==0&&c.error().startsWith("GESTURE_ACTIVE"),"Foreign gesture remains active and unchanged");c.host.session.cancel_gesture();
 std::cout<<checks<<" checks passed\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

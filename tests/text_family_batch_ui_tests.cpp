#include "text_family_batch_control.hpp"
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
int checks=0;void check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
Document fixture(){
 auto d=empty_document("family-document","composition","artboard");
 for(const auto* id:{"source","first","second"}){Object o;o.id=id;o.name=id;o.kind=Kind::text;o.text=default_text(std::string(id)+"-text",id);d.objects.emplace(id,o);d.compositions[0].roots.push_back(id);}
 auto& first=*d.objects.at("first").text;first.family="Arial";first.locale_driver=TextLocaleDriver{{"source","","text.locale"}};
 first.parameters.at("font_size").binding=Binding{{"source","","text.font_size"},1.25,0};first.weight_expression=Expression{"500",1};
 auto& second=*d.objects.at("second").text;second.family="Consolas";second.italic=true;second.font_features={{"kern",0,"whole_text"}};
 return d;
}
struct Controls{
 QTemporaryDir directory;Host host;QWidget parent;QPointer<QWidget> box;
 Controls():host(directory.path()){host.session=Session(fixture());host.changed=[this]{rebuild();};rebuild();}
 ~Controls(){host.changed={};}
 void rebuild(){delete box.data();box=make_text_family_batch_controls(host,{"first","second"},&parent);}
 QComboBox* editor(){return box->findChild<QComboBox*>("text-family-batch-editor");}
 QPushButton* apply(){return box->findChild<QPushButton*>("text-family-batch-apply");}
 QString error(){return box->findChild<QLabel*>("text-family-batch-status")->text();}
};
}
int main(int argc,char** argv){QApplication app(argc,argv);try{
 Controls c;const auto before=c.host.session.document();const auto native=encode(before);
 check(c.editor()->currentText().isEmpty()&&!c.apply()->isEnabled(),"Mixed is an empty draft, never a fake family");
 c.editor()->setEditText("Arial");check(encode(c.host.session.document())==native,"Typing stays a draft");
 c.box->findChild<QPushButton*>("text-family-batch-cancel")->click();
 check(c.editor()->currentText().isEmpty()&&encode(c.host.session.document())==native,"Cancel clears only draft");
 c.editor()->setEditText("Arial");c.apply()->click();auto expected=before;expected.objects.at("second").text->family="Arial";
 check(c.host.session.document()==expected&&c.host.session.revision()==1,"One canonical batch changes only family");
 check(!c.apply()->isEnabled(),"Unchanged shared value cannot make history");
 check(decode(encode(c.host.session.document()))==expected,"Native source roundtrip");
 c.host.session.undo(1);c.rebuild();check(c.host.session.document()==before,"Undo restores distinct fonts and rich source fields");
 c.host.session.redo(c.host.session.revision());c.rebuild();check(c.host.session.document()==expected,"Redo restores the exact batch");
 c.editor()->setEditText("Consolas");c.host.session.apply({Rename{"source","Other edit"}},c.host.session.revision());const auto stale=encode(c.host.session.document());
 c.apply()->click();check(encode(c.host.session.document())==stale&&c.error().startsWith("REVISION_CONFLICT"),"Stale context cannot retarget or partially mutate");
 auto linked=fixture();linked.objects.at("second").text->family_driver=TextFamilyDriver{{"source","","text.family"}};
 c.host.session=Session(linked);c.host.session_id+="-linked";c.rebuild();
 check(!c.editor()->isEnabled()&&!c.apply()->isEnabled()&&c.error().startsWith("DRIVEN_PROPERTY"),"One linked target refuses complete batch");
 check(c.host.session.document()==linked,"Link remains authored");
 std::cout<<checks<<" checks passed\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

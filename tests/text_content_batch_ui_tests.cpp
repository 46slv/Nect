#include "window.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QToolButton>
#include <iostream>
#include <stdexcept>

using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
void events(){QApplication::processEvents();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);}
const std::vector<Canvas::Selection> targets={{"first",""},{"second",""}};
const Ref source_ref{"source","","text.content"};
const std::string source_literal="Source\n日本語 🐈\n",first_literal="First\r\nliteral",second_literal="Second\n  literal  ";
struct Snapshot {
    Document document;std::uint64_t revision;HistoryInfo history;std::string native;
    explicit Snapshot(const Session& s):document(s.document()),revision(s.revision()),history(s.history()),native(encode(s.document())){}
    bool unchanged(const Session& s)const{return document==s.document()&&revision==s.revision()&&history==s.history()&&native==encode(s.document());}
};
Document fixture(){
    auto d=empty_document("content-document","composition","artboard");
    auto other=d.compositions.front();other.id="other-composition";other.artboards.front().id="other-artboard";d.compositions.push_back(other);
    Session s(d);s.apply({CreateText{"composition","","first","First",default_text("first-source",first_literal)},
        CreateText{"composition","","second","Second",default_text("second-source",second_literal)},
        CreateText{"other-composition","","source","Source",default_text("source-text",source_literal)},
        CreatePrimitive{"composition","","shape","Shape",default_primitive("shape-source","nect.shape.rectangle")}},s.revision());
    return s.document();
}
void load(Window& w){w.host.session=Session(fixture());w.host.session_id+="-fresh";w.host.edited();
    w.canvas->set_active_artboard("composition","artboard",false);w.canvas->set_selections(targets);events();}
template<class T>T* visible(Window& w,const QString& name){for(auto* widget:w.findChildren<T*>(name))if(widget->isVisible())return widget;throw std::runtime_error("Visible control missing: "+name.toStdString());}
QDialog* open(Window& w){visible<QPushButton>(w,"text-content-batch-link")->click();events();return visible<QDialog>(w,"text-source-picker");}
QListWidgetItem* source_item(QDialog* d,const Id& id){auto* list=d->findChild<QListWidget*>("text-source-picker-list");
    for(int i=0;i<list->count();++i){auto* item=list->item(i);auto ref=QJsonDocument::fromJson(item->data(Qt::UserRole).toByteArray()).object();
        if(ref.value("object").toString()==QString::fromStdString(id))return item;}return nullptr;}
void choose(QDialog* d,const Id& id="source"){auto* item=source_item(d,id);check(item,"Stable source Ref is offered");d->findChild<QListWidget*>()->setCurrentItem(item);events();}
void apply(QDialog* d){d->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->click();events();}
QString status(QDialog* d){return d->findChild<QLabel*>("text-source-picker-status")->text();}
void cold(const Document& doc){QTemporaryDir files;Host writer(files.path()+"/writer");writer.session=Session(doc);
    const auto path=files.path()+"/linked.nect";writer.save(path);writer.flush();Host reader(files.path()+"/cold");reader.open(path);
    check(reader.session.document()==doc&&encode(reader.session.document())==encode(doc),"Cold native Host reopen preserves exact literals, UTF-8 and stable source Refs");
    for(const auto& target:targets)check(text_content_property(reader.session.document(),{target.object,"","text.content"}).evaluated==source_literal,
        "Cold target evaluation follows the retained source");}
void primary(Window& w){load(w);auto& s=w.host.session;const Snapshot before(s);auto* d=open(w);
    check(d->findChild<QLabel*>("text-source-picker-target")->text().contains("2 selected Text objects"),"Chooser identifies the actual target count");
    check(!source_item(d,"first")&&!source_item(d,"second"),"Every captured target is excluded from source choices");
    choose(d);check(w.canvas->selections()==std::vector<Canvas::Selection>{{"source",""}}&&w.canvas->active_composition()=="other-composition", "Browsing cross-composition source navigates without changing captured targets");
    check(before.unchanged(s),"Browsing is not a document edit");apply(d);
    check(w.canvas->selections()==targets&&w.canvas->active_composition()=="composition"&&w.canvas->active_artboard()=="artboard", "Apply restores captured targets and active board");
    Session expected(before.document);expected.apply({LinkTextContent{{"first","","text.content"},source_ref,false},LinkTextContent{{"second","","text.content"},source_ref,false}},0);
    check(s.document()==expected.document()&&s.history()==expected.history()&&s.revision()==before.revision+1,"All target links use one canonical Session transaction");
    for(const auto& target:targets)check(text_content_property(s.document(),{target.object,"","text.content"}).driver==TextContentDriver{source_ref}&&
        evaluate_text_content(s.document(),target.object)==source_literal,"Retained source fans out to both target properties");
    check(s.document().objects.at("first").text->content==first_literal&&s.document().objects.at("second").text->content==second_literal,"Link preserves exact distinct authored literals");
    const auto linked=s.document();cold(linked);s.undo(s.revision());w.host.edited();events();check(s.document()==before.document,"One Undo removes the entire fanout");
    s.redo(s.revision());w.host.edited();events();check(s.document()==linked,"One Redo restores both source Refs");
    auto changed=*s.document().objects.at("source").text;changed.content="Changed\n日本語\n";
    s.apply({UpdateText{"source",changed}},s.revision());w.host.edited();events();
    check(evaluate_text_content(s.document(),"first")==changed.content&&evaluate_text_content(s.document(),"second")==changed.content,"Editing source content updates both targets");
    s.undo(s.revision());w.host.edited();events();check(s.document()==linked,"Undo source edit restores fanout evaluation and exact native state");
}
void guards(Window& w){load(w);auto& s=w.host.session;const Snapshot cancelled(s);auto* d=open(w);choose(d);d->reject();events();
    check(cancelled.unchanged(s)&&w.canvas->selections()==targets&&w.canvas->active_composition()=="composition"&&w.canvas->active_artboard()=="artboard","Cancel restores retained selection and board without history");
    // One driven target makes the whole transaction reject, even when the
    // requested source is identical. No earlier literal target may commit.
    s.apply({LinkTextContent{{"second","","text.content"},source_ref,false}},s.revision());w.host.edited();events();const Snapshot driven(s);
    d=open(w);auto* replace=d->findChild<QCheckBox*>("text-content-replace");check(replace&&replace->isVisible()&&!replace->isChecked(),"Existing source replacement requires explicit unchecked consent");
    choose(d);apply(d);check(driven.unchanged(s)&&status(d).startsWith("DRIVEN_PROPERTY"),"Identical source with replace=false rejects the whole batch atomically");
    replace->setChecked(true);apply(d);check(s.revision()==driven.revision+1&&evaluate_text_content(s.document(),"first")==source_literal,"Explicit replacement applies the retained batch once");
    s.undo(s.revision());w.host.edited();events();check(s.document()==driven.document,"Replacement Undo restores the exact earlier driver distribution");
    d=open(w);choose(d);s.apply({Rename{"source","Renamed source"}},s.revision());const Snapshot stale(s);apply(d);
    check(stale.unchanged(s)&&status(d).startsWith("REVISION_CONFLICT"),"Stale chooser cannot apply any target");d->reject();events();
    check(w.canvas->selections()==targets,"Stale Cancel restores selection in the same Session");
    load(w);s.apply({LinkTextContent{source_ref,{"first","","text.content"},false}},s.revision());w.host.edited();events();const Snapshot cycle(s);d=open(w);choose(d);apply(d);
    check(cycle.unchanged(s)&&status(d).startsWith("DEPENDENCY_CYCLE"),"Final dependency cycle rejects every target atomically");d->reject();events();
    load(w);d=open(w);choose(d);w.host.session=Session(fixture());w.host.session_id+="-replacement";w.host.edited();w.canvas->set_selection("shape");events();const Snapshot swapped(s);
    apply(d);check(swapped.unchanged(s)&&status(d).startsWith("SESSION_CONFLICT"),"Retained chooser refuses a replacement Session even with identical IDs");
    d->reject();events();check(w.canvas->selections()==std::vector<Canvas::Selection>{{"shape",""}},"Cancel never restores old IDs into another Session");
    load(w);w.canvas->set_selections({{"first",""},{"shape",""}});events();
    check(!visible<QPushButton>(w,"text-content-batch-link")->isEnabled()&&visible<QLabel>(w,"text-content-batch-status")->text().startsWith("Unavailable:"),"Mixed Text and other object selection is explicitly unavailable without filtering");
    w.canvas->set_selection("first");events();visible<QToolButton>(w,"text-content-driver")->menu()->actions().front()->trigger();events();d=visible<QDialog>(w,"text-source-picker");
    check(d->findChild<QLabel*>("text-source-picker-target")->text().contains("1 selected Text objects"),"Single Text reuses the retained content source picker");d->reject();events();
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","offscreen");QApplication app(argc,argv);try{
    QTemporaryDir files;QSettings settings(files.filePath("settings.ini"),QSettings::IniFormat);
    Window w(files.path(),std::make_unique<FolderLibrary>(settings));w.resize(1200,900);w.show();events();primary(w);guards(w);
    std::cout<<"PASS "<<checks<<" retained Text content Qt checks (physical OS input NOT_RUN)\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}}

#include "window.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QTreeWidget>
#include <QToolButton>
#include <iostream>
using namespace nect;
using namespace nect::desktop;
namespace {
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
template<class T>T* widget(Window& w,const char* name){QApplication::processEvents();for(auto* p:w.findChildren<T*>())if(p->isVisible()&&p->objectName()==name)return p;throw std::runtime_error(std::string("Missing ")+name);}
void reject_empty_hidden_source(Window& w,QToolButton* driver,const char* name,const Ref& source){
    auto& session=w.host.session;const auto before=session.document();const auto revision=session.revision();
    bool empty=false,hidden=false,canceled=false;
    QTimer::singleShot(0,&w,[&]{
        auto* dialog=w.findChild<QDialog*>(name);
        auto* search=dialog?dialog->findChild<QLineEdit*>(QString(name)+"-search"):nullptr;
        auto* combo=dialog?dialog->findChild<QComboBox*>(QString(name)+"-source"):nullptr;
        auto* buttons=dialog?dialog->findChild<QDialogButtonBox*>():nullptr;
        if(!dialog||!search||!combo||!buttons)return;
        buttons->button(QDialogButtonBox::Apply)->click();empty=dialog->isVisible()&&combo->currentIndex()<0;
        search->setText(QString::fromStdString(source.object+" / "+source.field));combo->setCurrentIndex(0);
        search->setText("no such source");buttons->button(QDialogButtonBox::Apply)->click();
        hidden=dialog->isVisible()&&combo->count()==0&&combo->currentIndex()<0;
        buttons->button(QDialogButtonBox::Cancel)->click();canceled=!dialog->isVisible();
    });
    driver->menu()->actions().front()->trigger();QApplication::processEvents();
    check(empty&&hidden&&canceled&&session.revision()==revision&&session.document()==before,
        "Boolean chooser rejects empty/hidden sources and Cancel without authored mutation");
}
void context(Window& w,const QString& prefix){bool chosen=false;QTimer::singleShot(0,[&]{auto* menu=qobject_cast<QMenu*>(QApplication::activePopupWidget());if(!menu)return;for(auto* a:menu->actions())if(a->text().startsWith(prefix)&&a->isEnabled()){chosen=true;menu->setActiveAction(a);QTest::keyClick(menu,Qt::Key_Return);return;}menu->close();});
    QMetaObject::invokeMethod(w.canvas,"customContextMenuRequested",Qt::DirectConnection,Q_ARG(QPoint,QPoint(250,250)));QApplication::processEvents();check(chosen,"Context action is discoverable and enabled");}
void controls(){
    QTemporaryDir tmp;Window w(tmp.path());w.host.session=Session(empty_document("doc","comp","board"));auto& s=w.host.session;
    auto background=default_primitive("background-source","nect.shape.rectangle");auto content=default_primitive("content-source","nect.shape.rectangle");auto clip=default_primitive("clip-source","nect.shape.circle");
    s.apply({CreatePrimitive{"comp","","background","Background",background},CreatePrimitive{"comp","","content","Content",content},CreatePrimitive{"comp","","clip","Circle mask",clip}},s.revision());w.host.edited();w.show();QApplication::processEvents();
    const auto original=encode(s.document());auto revision=s.revision();w.canvas->set_selections({{"clip",""},{"content",""}});
    context(w,"Mask With Top · Circle mask");check(s.revision()==revision+1,"Mask is one transaction");const auto group=w.canvas->selected_object;
    check(s.document().objects.at(group).compositing.mask->source=="clip"&&!s.document().objects.at("clip").visible,"Paint order selects source and hides normal artwork");
    auto* outline=widget<QCheckBox>(w,"mask-show-outline");const auto authored=encode(s.document());outline->setChecked(!outline->isChecked());check(encode(s.document())==authored,"Outline is viewport state only");
    widget<QPushButton>(w,"mask-edit-source")->click();check(w.canvas->selected_object=="clip"&&!s.document().objects.at("clip").visible,"Editing selects retained hidden source without showing its paint");
    w.canvas->set_selection(group);widget<QComboBox>(w,"object-blend")->setCurrentIndex(1);check(s.document().objects.at(group).compositing.blend=="multiply","Blend row uses shared Session command");
    widget<QCheckBox>(w,"mask-enabled")->setChecked(false);check(!s.document().objects.at(group).compositing.mask->enabled,"Mask bypass preserves source");
    s.undo(s.revision());w.host.edited();s.undo(s.revision());w.host.edited();s.undo(s.revision());w.host.edited();check(encode(s.document())==original,"Mask, blend and bypass Undo restores exact source structure");
    w.canvas->set_selections({{"content",""},{"clip",""}});context(w,"Mask With Bottom · Content");
    const auto bottom=w.canvas->selected_object;check(s.document().objects.at(bottom).compositing.mask->source=="content","Bottom uses first painted object");
    check(encode(decode(encode(s.document())))==encode(s.document()),"Masked UI document survives native readback");
    const auto masked=encode(s.document());const auto masked_revision=s.revision();
    w.canvas->set_selections({{bottom,""},{"background",""}});context(w,"Put Inside · Masked Group");
    check(s.revision()==masked_revision&&encode(s.document())==masked&&
          w.statusBar()->currentMessage().startsWith("PUT_INSIDE_APPEARANCE"),
        "Put Inside rejects a masked destination without changing authored state");
    s.apply({SetMask{bottom,std::nullopt}},s.revision());w.host.edited();
    w.canvas->set_selections({{bottom,""},{"background",""}});context(w,"Put Inside · Masked Group");
    check(s.document().objects.at(bottom).children.front()=="background"&&s.document().compositions.front().roots.size()==1,
        "Put Inside targets the now neutral top selected Group and retains order");
}
void alpha_mask_controls(){
    QTemporaryDir tmp;Window w(tmp.path());w.host.session=Session(empty_document("alpha-ui-doc","alpha-ui-comp","alpha-ui-board"));
    auto& session=w.host.session;
    session.apply({CreatePrimitive{"alpha-ui-comp","","target","Target",default_primitive("target-source","nect.shape.rectangle")},
        CreatePrimitive{"alpha-ui-comp","","source","Source",default_primitive("source-source","nect.shape.circle")},
        SetMask{"target",GeometryMask{"alpha-ui-mask","source"}}},session.revision());
    w.host.edited();w.show();QApplication::processEvents();w.canvas->set_selection("target");QApplication::processEvents();
    auto* mode=widget<QComboBox>(w,"mask-mode");mode->setCurrentIndex(1);
    check(session.document().objects.at("target").compositing.mask->mode=="alpha"&&
        widget<QCheckBox>(w,"mask-invert")->isEnabled(),
        "Mask Inspector selects Alpha through the shared Session command");
    auto* invert=widget<QCheckBox>(w,"mask-invert");invert->setChecked(true);
    check(session.document().objects.at("target").compositing.mask->invert&&
        !widget<QComboBox>(w,"mask-fill-rule")->isEnabled(),
        "Alpha inversion is authored while Geometry-only fill rule is unavailable");
    widget<QComboBox>(w,"mask-mode")->setCurrentIndex(0);
    check(session.document().objects.at("target").compositing.mask->mode=="geometry"&&
        !session.document().objects.at("target").compositing.mask->invert&&
        widget<QComboBox>(w,"mask-fill-rule")->isEnabled(),
        "Mask Inspector restores Geometry mode with its unchanged fill-rule control");
}
void visibility_inspector(){
    QTemporaryDir tmp;Window w(tmp.path());w.host.session=Session(empty_document("visibility-doc","visibility-comp","visibility-board"));
    auto& session=w.host.session;
    session.apply({CreatePrimitive{"visibility-comp","","source","Source",default_primitive("source-primitive","nect.shape.rectangle")},
        CreatePrimitive{"visibility-comp","","target","Target",default_primitive("target-primitive","nect.shape.rectangle")}},session.revision());
    w.host.edited();w.show();QApplication::processEvents();
    session.apply({SetVisibility{"source",false}},session.revision());w.host.edited();w.canvas->set_selection("target");QApplication::processEvents();
    auto* link_button=widget<QToolButton>(w,"object-visible-driver");
    check(widget<QCheckBox>(w,"object-visible")->isChecked()&&widget<QCheckBox>(w,"object-visible")->isEnabled(),
        "Visibility Inspector starts from the authored literal");
    const Ref visibility_source{"source","","object.visible"};
    reject_empty_hidden_source(w,link_button,"object-visible-source-dialog",visibility_source);
    link_button=widget<QToolButton>(w,"object-visible-driver");
    bool stale=false;
    QTimer::singleShot(0,&w,[&]{
        auto* dialog=w.findChild<QDialog*>("object-visible-source-dialog");
        auto* combo=dialog?dialog->findChild<QComboBox*>("object-visible-source-dialog-source"):nullptr;
        auto* status=dialog?dialog->findChild<QLabel*>("object-visible-source-dialog-status"):nullptr;
        auto* buttons=dialog?dialog->findChild<QDialogButtonBox*>():nullptr;
        if(!dialog||!combo||!status||!buttons)return;
        combo->setCurrentIndex(0);session.apply({SetVisibility{"source",true}},session.revision());
        buttons->button(QDialogButtonBox::Apply)->click();
        stale=dialog->isVisible()&&status->text().contains("REVISION_CONFLICT")&&
            !object_visibility_state(session.document(),{"target","","object.visible"}).driver;
        buttons->button(QDialogButtonBox::Cancel)->click();
    });
    link_button->menu()->actions().front()->trigger();w.host.edited();QApplication::processEvents();
    check(stale,"Visibility chooser rejects a stale draft without linking the target");
    session.apply({SetVisibility{"source",false}},session.revision());w.host.edited();
    link_button=widget<QToolButton>(w,"object-visible-driver");
    bool picked=false;
    QTimer::singleShot(0,&w,[&]{
        auto* dialog=w.findChild<QDialog*>("object-visible-source-dialog");
        auto* search=dialog?dialog->findChild<QLineEdit*>("object-visible-source-dialog-search"):nullptr;
        auto* combo=dialog?dialog->findChild<QComboBox*>("object-visible-source-dialog-source"):nullptr;
        auto* buttons=dialog?dialog->findChild<QDialogButtonBox*>():nullptr;
        if(!dialog||!search||!combo||!buttons)return;
        search->setText("SOURCE / OBJECT.VISIBLE");
        if(combo->count()==1&&combo->itemText(0).contains("object.visible")) {
            combo->setCurrentIndex(0);w.canvas->set_selection("source");QApplication::processEvents();
            buttons->button(QDialogButtonBox::Apply)->click();picked=true;
            w.canvas->set_selection("target");QApplication::processEvents();
        } else dialog->reject();
    });
    link_button->menu()->actions().front()->trigger();QApplication::processEvents();
    const auto linked=object_visibility_state(session.document(),{"target","","object.visible"});
    check(picked&&linked.literal&&linked.driver==Ref{"source","","object.visible"}&&!linked.evaluated,
        "Inspector Link action selects a stable source and preserves the authored checkbox value");
    auto* source_button=widget<QToolButton>(w,"object-visible-driver");
    check(source_button->menu()->actions().size()==3&&source_button->menu()->actions()[1]->isEnabled(),
        "Inspector exposes Link, expression draft, and unlink actions for Object visibility");
    const Expression expression{" ! ref ( \"source\" , \"\" , \"object.visible\" ) ",1};
    bool applied_expression=false;
    QTimer::singleShot(0,&w,[&]{
        auto* dialog=w.findChild<QDialog*>("object-visible-expression-dialog");
        auto* source=dialog?dialog->findChild<QPlainTextEdit*>("object-visible-expression-source"):nullptr;
        auto* buttons=dialog?dialog->findChild<QDialogButtonBox*>():nullptr;
        if(!dialog||!source||!buttons)return;
        source->setPlainText(QString::fromStdString(expression.source));
        buttons->button(QDialogButtonBox::Apply)->click();applied_expression=!dialog->isVisible();
    });
    source_button->menu()->actions()[1]->trigger();QApplication::processEvents();
    auto expressed=object_visibility_state(session.document(),{"target","","object.visible"});
    check(applied_expression&&expressed.literal&&!expressed.driver&&expressed.expression==expression&&expressed.evaluated,
        "Inspector Apply authors the exact expression through the shared Session command");
    auto* tree=w.findChild<QTreeWidget*>();QTreeWidgetItem* target_row=nullptr;
    QTreeWidgetItemIterator row_iterator(tree);
    while(*row_iterator){if((*row_iterator)->data(0,Qt::UserRole)=="target"&&(*row_iterator)->data(0,Qt::UserRole+1).toString().isEmpty())target_row=*row_iterator;++row_iterator;}
    check(target_row&&target_row->text(0).contains("ƒ")&&!target_row->text(0).contains("◌"),
        "Structure tree shows expression source without a hidden marker while evaluated visibility is true");
    check(!widget<QCheckBox>(w,"object-visible")->isEnabled()&&
        widget<QLabel>(w,"object-visible-status")->text().contains("Evaluated own visibility: true")&&
        widget<QLabel>(w,"object-visible-status")->text().contains(QString::fromStdString(expression.source)),
        "Expression Inspector disables direct editing and shows exact source and evaluated own visibility");
    const auto expression_document=session.document();const auto expression_revision=session.revision();
    source_button=widget<QToolButton>(w,"object-visible-driver");bool canceled=false;
    QTimer::singleShot(0,&w,[&]{
        auto* dialog=w.findChild<QDialog*>("object-visible-expression-dialog");
        auto* source=dialog?dialog->findChild<QPlainTextEdit*>("object-visible-expression-source"):nullptr;
        auto* buttons=dialog?dialog->findChild<QDialogButtonBox*>():nullptr;
        if(!dialog||!source||!buttons)return;
        source->setPlainText("false");buttons->button(QDialogButtonBox::Cancel)->click();canceled=!dialog->isVisible();
    });
    source_button->menu()->actions()[1]->trigger();QApplication::processEvents();
    check(canceled&&session.document()==expression_document&&session.revision()==expression_revision,
        "Cancel discards the Object visibility expression draft without changing authored state");
    session.apply({SetVisibility{"source",true}},session.revision());w.host.edited();
    check(!object_visibility_state(session.document(),{"target","","object.visible"}).evaluated&&
        widget<QLabel>(w,"object-visible-status")->text().contains("Evaluated own visibility: false"),
        "Inspector expression follows the source authored visibility after a Session edit");
    tree=w.findChild<QTreeWidget*>();target_row=nullptr;QTreeWidgetItemIterator hidden_iterator(tree);
    while(*hidden_iterator){if((*hidden_iterator)->data(0,Qt::UserRole)=="target"&&(*hidden_iterator)->data(0,Qt::UserRole+1).toString().isEmpty())target_row=*hidden_iterator;++hidden_iterator;}
    check(target_row&&target_row->text(0).contains("◌")&&target_row->text(0).contains("ƒ")&&
        target_row->toolTip(0).contains("Visibility authored: true")&&target_row->toolTip(0).contains("evaluated own: false"),
        "Structure tree hidden marker follows evaluated own visibility while tooltip preserves authored literal");
    auto* unlink_button=widget<QToolButton>(w,"object-visible-driver");
    check(unlink_button->menu()->actions().size()==3&&unlink_button->menu()->actions()[2]->isEnabled(),
        "Inspector exposes unlink and freeze for an expression-driven value");
    unlink_button->menu()->actions()[2]->trigger();QApplication::processEvents();
    const auto frozen=object_visibility_state(session.document(),{"target","","object.visible"});
    check(!frozen.literal&&!frozen.driver&&!frozen.expression&&widget<QCheckBox>(w,"object-visible")->isEnabled()&&
        widget<QLabel>(w,"object-visible-status")->text().contains("Evaluated own visibility: false"),
        "Inspector Unlink freezes the evaluated value and returns control to its checkbox");
    session.apply({SetVisibility{"source",false}},session.revision());w.host.edited();
    check(!object_visibility_state(session.document(),{"target","","object.visible"}).evaluated,
        "Unlinked Inspector value remains frozen when its former source changes");
}
void isolation_inspector(){
    QTemporaryDir tmp;Window w(tmp.path());w.host.session=Session(empty_document("isolation-doc","isolation-comp","isolation-board"));
    auto& session=w.host.session;
    session.apply({CreatePrimitive{"isolation-comp","","source","Isolation Source",default_primitive("source-primitive","nect.shape.rectangle")},
        CreatePrimitive{"isolation-comp","","target","Isolation Target",default_primitive("target-primitive","nect.shape.rectangle")},
        SetCompositing{"source","normal",true}},session.revision());
    w.host.edited();w.show();QApplication::processEvents();w.canvas->set_selection("target");QApplication::processEvents();
    const Ref target{"target","","composite.isolated"},source{"source","","composite.isolated"};
    auto* isolation=widget<QCheckBox>(w,"object-isolated");
    check(!isolation->isChecked()&&isolation->isEnabled(),"Isolation Inspector starts from the authored literal");
    auto* driver=widget<QToolButton>(w,"object-isolated-driver");
    reject_empty_hidden_source(w,driver,"object-isolated-source-dialog",source);
    driver=widget<QToolButton>(w,"object-isolated-driver");
    bool stale=false;
    QTimer::singleShot(0,&w,[&]{
        auto* dialog=w.findChild<QDialog*>("object-isolated-source-dialog");
        auto* combo=dialog?dialog->findChild<QComboBox*>("object-isolated-source-dialog-source"):nullptr;
        auto* status=dialog?dialog->findChild<QLabel*>("object-isolated-source-dialog-status"):nullptr;
        auto* buttons=dialog?dialog->findChild<QDialogButtonBox*>():nullptr;
        if(!dialog||!combo||!status||!buttons)return;
        combo->setCurrentIndex(0);session.apply({SetCompositing{"source","normal",false}},session.revision());
        buttons->button(QDialogButtonBox::Apply)->click();
        stale=dialog->isVisible()&&status->text().contains("REVISION_CONFLICT")&&
            !composite_isolation_state(session.document(),target).driver;
        buttons->button(QDialogButtonBox::Cancel)->click();
    });
    driver->menu()->actions().front()->trigger();w.host.edited();QApplication::processEvents();
    check(stale,"Isolation chooser rejects a stale draft without linking the target");
    session.apply({SetCompositing{"source","normal",true}},session.revision());w.host.edited();
    driver=widget<QToolButton>(w,"object-isolated-driver");bool picked=false;
    QTimer::singleShot(0,&w,[&]{
        auto* dialog=w.findChild<QDialog*>("object-isolated-source-dialog");
        auto* search=dialog?dialog->findChild<QLineEdit*>("object-isolated-source-dialog-search"):nullptr;
        auto* combo=dialog?dialog->findChild<QComboBox*>("object-isolated-source-dialog-source"):nullptr;
        auto* buttons=dialog?dialog->findChild<QDialogButtonBox*>():nullptr;
        if(!dialog||!search||!combo||!buttons)return;
        search->setText("SOURCE / COMPOSITE.ISOLATED");
        if(combo->count()==1&&combo->itemText(0).contains("composite.isolated")) {
            combo->setCurrentIndex(0);w.canvas->set_selection("source");QApplication::processEvents();
            buttons->button(QDialogButtonBox::Apply)->click();picked=true;
            w.canvas->set_selection("target");QApplication::processEvents();
        } else dialog->reject();
    });
    driver->menu()->actions().front()->trigger();QApplication::processEvents();
    auto state=composite_isolation_state(session.document(),target);
    check(picked&&state.literal==false&&state.driver==source&&state.evaluated,
        "Inspector links a stable isolation source and retains the false authored literal");
    check(!widget<QCheckBox>(w,"object-isolated")->isEnabled()&&
        widget<QLabel>(w,"object-isolated-status")->text().contains("Evaluated authored isolation: true")&&
        widget<QLabel>(w,"object-isolated-status")->text().contains("Isolation Source"),
        "Driven Inspector disables literal editing and shows source and evaluated authored value");
    session.apply({SetCompositing{"target","multiply",false}},session.revision());w.host.edited();
    state=composite_isolation_state(session.document(),target);
    check(state.literal==false&&state.driver==source&&state.evaluated&&
        session.document().objects.at("target").compositing.blend=="multiply",
        "Inspector blend edit preserves the isolation driver and authored literal");
    session.apply({SetCompositing{"source","normal",false}},session.revision());w.host.edited();
    state=composite_isolation_state(session.document(),target);
    check(!state.evaluated&&widget<QLabel>(w,"object-isolated-status")->text().contains("Evaluated authored isolation: false"),
        "Inspector follows a source edit without changing the target literal");
    driver=widget<QToolButton>(w,"object-isolated-driver");
    check(driver->menu()->actions().size()==3&&driver->menu()->actions()[1]->isEnabled()&&driver->menu()->actions()[2]->isEnabled(),
        "Inspector offers expression replacement and explicit unlink for a driven isolation value");
    const Expression expression{" ! ref ( \"source\" , \"\" , \"composite.isolated\" ) ",1};
    bool applied_expression=false;
    QTimer::singleShot(0,&w,[&]{
        auto* dialog=w.findChild<QDialog*>("composite-isolated-expression-dialog");
        auto* source_editor=dialog?dialog->findChild<QPlainTextEdit*>("composite-isolated-expression-source"):nullptr;
        auto* buttons=dialog?dialog->findChild<QDialogButtonBox*>():nullptr;
        if(!dialog||!source_editor||!buttons)return;
        source_editor->setPlainText(QString::fromStdString(expression.source));
        buttons->button(QDialogButtonBox::Apply)->click();applied_expression=!dialog->isVisible();
    });
    driver->menu()->actions()[1]->trigger();QApplication::processEvents();
    state=composite_isolation_state(session.document(),target);
    check(applied_expression&&state.literal==false&&!state.driver&&state.expression==expression&&state.evaluated&&
        !widget<QCheckBox>(w,"object-isolated")->isEnabled()&&
        widget<QLabel>(w,"object-isolated-status")->text().contains(QString::fromStdString(expression.source)),
        "Inspector Apply replaces the link with exact expression text and keeps the driven checkbox read-only");
    const auto expression_document=session.document();const auto expression_revision=session.revision();
    driver=widget<QToolButton>(w,"object-isolated-driver");bool canceled=false;
    QTimer::singleShot(0,&w,[&]{
        auto* dialog=w.findChild<QDialog*>("composite-isolated-expression-dialog");
        auto* source_editor=dialog?dialog->findChild<QPlainTextEdit*>("composite-isolated-expression-source"):nullptr;
        auto* buttons=dialog?dialog->findChild<QDialogButtonBox*>():nullptr;
        if(!dialog||!source_editor||!buttons)return;
        source_editor->setPlainText("false");buttons->button(QDialogButtonBox::Cancel)->click();canceled=!dialog->isVisible();
    });
    driver->menu()->actions()[1]->trigger();QApplication::processEvents();
    check(canceled&&session.document()==expression_document&&session.revision()==expression_revision,
        "Cancel discards the Composite isolation expression draft without changing authored state");
    driver=widget<QToolButton>(w,"object-isolated-driver");bool stale_expression=false;
    QTimer::singleShot(0,&w,[&]{
        auto* dialog=w.findChild<QDialog*>("composite-isolated-expression-dialog");
        auto* source_editor=dialog?dialog->findChild<QPlainTextEdit*>("composite-isolated-expression-source"):nullptr;
        auto* status=dialog?dialog->findChild<QLabel*>("composite-isolated-expression-status"):nullptr;
        auto* buttons=dialog?dialog->findChild<QDialogButtonBox*>():nullptr;
        if(!dialog||!source_editor||!status||!buttons)return;
        session.apply({SetCompositing{"source","normal",true}},session.revision());
        source_editor->setPlainText(QString::fromStdString(expression.source));buttons->button(QDialogButtonBox::Apply)->click();
        stale_expression=dialog->isVisible()&&status->text().contains("REVISION_CONFLICT")&&
            composite_isolation_state(session.document(),target).expression==expression;
        buttons->button(QDialogButtonBox::Cancel)->click();
    });
    driver->menu()->actions()[1]->trigger();QApplication::processEvents();
    check(stale_expression,"Inspector rejects a stale expression draft after source state changes");
    w.host.edited();
    check(!composite_isolation_state(session.document(),target).evaluated&&
        session.document().objects.at("target").compositing.blend=="multiply",
        "Expression reads authored isolation while another term still keeps effective scene isolation");
    driver=widget<QToolButton>(w,"object-isolated-driver");
    driver->menu()->actions()[2]->trigger();QApplication::processEvents();
    state=composite_isolation_state(session.document(),target);
    check(!state.literal&&!state.driver&&!state.expression&&!widget<QCheckBox>(w,"object-isolated")->isChecked()&&
        widget<QCheckBox>(w,"object-isolated")->isEnabled(),
        "Inspector unlink freezes evaluated expression value and re-enables its literal toggle");
    session.apply({SetCompositing{"source","normal",true}},session.revision());w.host.edited();
    check(!composite_isolation_state(session.document(),target).evaluated,
        "Unlinked Inspector value stays frozen when its former source changes");
}
void mask_enabled_inspector(){
    QTemporaryDir tmp;Window w(tmp.path());w.host.session=Session(empty_document("mask-link-doc","mask-link-comp","mask-link-board"));
    auto& session=w.host.session;
    session.apply({CreatePrimitive{"mask-link-comp","","target","Mask Target",default_primitive("target-source","nect.shape.rectangle")},
        CreatePrimitive{"mask-link-comp","","source","Mask Source",default_primitive("source-source","nect.shape.rectangle")},
        CreatePrimitive{"mask-link-comp","","geometry","Mask Geometry",default_primitive("geometry-source","nect.shape.rectangle")},
        SetMask{"target",GeometryMask{"target-mask","source",1,false,"evenodd"}},
        SetMask{"source",GeometryMask{"source-mask","geometry",1,true,"nonzero"}}},session.revision());
    w.host.edited();w.show();QApplication::processEvents();w.canvas->set_selection("target");QApplication::processEvents();
    const auto target=geometry_mask_enabled_ref("target","target-mask");
    const auto source=geometry_mask_enabled_ref("source","source-mask");
    check(!geometry_mask_enabled_state(session.document(),target).literal&&
        widget<QCheckBox>(w,"mask-enabled")->isEnabled(),
        "Mask Inspector starts with its authored bypass literal editable");
    auto* driver=widget<QToolButton>(w,"mask-enabled-driver");
    reject_empty_hidden_source(w,driver,"mask-enabled-source-dialog",source);
    driver=widget<QToolButton>(w,"mask-enabled-driver");
    bool stale=false;
    QTimer::singleShot(0,&w,[&]{
        auto* dialog=w.findChild<QDialog*>("mask-enabled-source-dialog");
        auto* combo=dialog?dialog->findChild<QComboBox*>("mask-enabled-source-dialog-source"):nullptr;
        auto* status=dialog?dialog->findChild<QLabel*>("mask-enabled-source-dialog-status"):nullptr;
        auto* buttons=dialog?dialog->findChild<QDialogButtonBox*>():nullptr;
        if(!dialog||!combo||!status||!buttons)return;
        combo->setCurrentIndex(0);
        auto changed=*session.document().objects.at("source").compositing.mask;changed.enabled=false;
        session.apply({SetMask{"source",changed}},session.revision());
        buttons->button(QDialogButtonBox::Apply)->click();
        stale=dialog->isVisible()&&status->text().contains("REVISION_CONFLICT")&&
            !geometry_mask_enabled_state(session.document(),target).driver;
        buttons->button(QDialogButtonBox::Cancel)->click();
    });
    driver->menu()->actions().front()->trigger();w.host.edited();QApplication::processEvents();
    check(stale,"Mask chooser rejects a stale draft without linking the target");
    auto restored=*session.document().objects.at("source").compositing.mask;restored.enabled=true;
    session.apply({SetMask{"source",restored}},session.revision());w.host.edited();
    driver=widget<QToolButton>(w,"mask-enabled-driver");bool picked=false;
    QTimer::singleShot(0,&w,[&]{
        auto* dialog=w.findChild<QDialog*>("mask-enabled-source-dialog");
        auto* search=dialog?dialog->findChild<QLineEdit*>("mask-enabled-source-dialog-search"):nullptr;
        auto* combo=dialog?dialog->findChild<QComboBox*>("mask-enabled-source-dialog-source"):nullptr;
        auto* buttons=dialog?dialog->findChild<QDialogButtonBox*>():nullptr;
        if(!dialog||!search||!combo||!buttons)return;
        search->setText("SOURCE / MASK.SOURCE-MASK.ENABLED");
        if(combo->count()==1&&combo->itemText(0).contains("mask.source-mask.enabled")) {
            combo->setCurrentIndex(0);w.canvas->set_selection("source");QApplication::processEvents();
            buttons->button(QDialogButtonBox::Apply)->click();picked=true;
            w.canvas->set_selection("target");QApplication::processEvents();
        } else dialog->reject();
    });
    driver->menu()->actions().front()->trigger();QApplication::processEvents();
    auto state=geometry_mask_enabled_state(session.document(),target);
    check(picked&&!state.literal&&state.driver==source&&state.evaluated,
        "Inspector links an exact mask instance while preserving the false literal");
    check(!widget<QCheckBox>(w,"mask-enabled")->isEnabled()&&
        widget<QLabel>(w,"mask-enabled-state")->text().contains("Evaluated enabled: true")&&
        widget<QLabel>(w,"mask-enabled-state")->text().contains("source-mask"),
        "Driven mask Inspector disables its checkbox and shows source and evaluated value");
    auto source_mask=*session.document().objects.at("source").compositing.mask;source_mask.enabled=false;
    session.apply({SetMask{"source",source_mask}},session.revision());w.host.edited();
    check(!geometry_mask_enabled_state(session.document(),target).evaluated&&
        widget<QLabel>(w,"mask-enabled-state")->text().contains("Evaluated enabled: false"),
        "Inspector follows source bypass changes without altering the target literal");
    const Expression expression{" ! ref ( \"source\" , \"\" , \"mask.source-mask.enabled\" ) ",1};
    driver=widget<QToolButton>(w,"mask-enabled-driver");const auto cancel_revision=session.revision();bool expression_cancelled=false;
    QTimer::singleShot(0,&w,[&]{
        auto* dialog=w.findChild<QDialog*>("mask-enabled-expression-dialog");
        auto* source_editor=dialog?dialog->findChild<QPlainTextEdit*>("mask-enabled-expression-source"):nullptr;
        auto* buttons=dialog?dialog->findChild<QDialogButtonBox*>():nullptr;
        if(!dialog||!source_editor||!buttons){if(dialog)dialog->reject();return;}
        source_editor->setPlainText(QString::fromStdString(expression.source));
        expression_cancelled=session.revision()==cancel_revision&&
            geometry_mask_enabled_state(session.document(),target).driver==source&&
            !geometry_mask_enabled_state(session.document(),target).expression;
        buttons->button(QDialogButtonBox::Cancel)->click();
    });
    driver->menu()->actions()[1]->trigger();QApplication::processEvents();
    check(expression_cancelled&&session.revision()==cancel_revision&&
        geometry_mask_enabled_state(session.document(),target).driver==source,
        "Inspector Cancel leaves the captured mask source and Session revision unchanged");
    driver=widget<QToolButton>(w,"mask-enabled-driver");bool expression_applied=false;
    QTimer::singleShot(0,&w,[&]{
        auto* dialog=w.findChild<QDialog*>("mask-enabled-expression-dialog");
        auto* source_editor=dialog?dialog->findChild<QPlainTextEdit*>("mask-enabled-expression-source"):nullptr;
        auto* buttons=dialog?dialog->findChild<QDialogButtonBox*>():nullptr;
        if(!dialog||!source_editor||!buttons){if(dialog)dialog->reject();return;}
        source_editor->setPlainText(QString::fromStdString(expression.source));
        buttons->button(QDialogButtonBox::Apply)->click();expression_applied=true;
    });
    driver->menu()->actions()[1]->trigger();QApplication::processEvents();
    state=geometry_mask_enabled_state(session.document(),target);
    check(expression_applied&&!state.literal&&!state.driver&&state.expression==expression&&state.evaluated,
        "Inspector Apply explicitly replaces the link with the exact expression while preserving its literal");
    check(!widget<QCheckBox>(w,"mask-enabled")->isEnabled()&&
        widget<QToolButton>(w,"mask-enabled-driver")->text()=="Expression…"&&
        widget<QLabel>(w,"mask-enabled-state")->text().contains(QString::fromStdString(expression.source)),
        "Expression-driven mask Inspector disables its literal checkbox and displays the exact source");
    source_mask=*session.document().objects.at("source").compositing.mask;source_mask.enabled=true;
    session.apply({SetMask{"source",source_mask}},session.revision());w.host.edited();
    check(!geometry_mask_enabled_state(session.document(),target).evaluated,
        "Inspector expression follows source bypass changes through the shared Session");
    driver=widget<QToolButton>(w,"mask-enabled-driver");
    check(driver->menu()->actions().size()==3&&driver->menu()->actions()[2]->isEnabled(),
        "Inspector offers explicit unlink for a driven mask expression");
    driver->menu()->actions()[2]->trigger();QApplication::processEvents();
    state=geometry_mask_enabled_state(session.document(),target);
    check(!state.literal&&!state.driver&&!state.expression&&widget<QCheckBox>(w,"mask-enabled")->isEnabled(),
        "Inspector unlink freezes evaluated false and re-enables the authored bypass checkbox");
    source_mask=*session.document().objects.at("source").compositing.mask;source_mask.enabled=false;
    session.apply({SetMask{"source",source_mask}},session.revision());w.host.edited();
    check(!geometry_mask_enabled_state(session.document(),target).evaluated,
        "Unlinked target remains frozen when its former source mask changes");
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","offscreen");QApplication app(argc,argv);try{controls();alpha_mask_controls();visibility_inspector();isolation_inspector();mask_enabled_inspector();std::cout<<"Compositing UI passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

#pragma once

// Origin Y's own input is a distinct completion owner from Font size reverse Tab.
// Qualify its actual neutral route first; never focus an action programmatically.
namespace text_path_origin_source_keyboard_window_smoke {
using text_path_batch_window_smoke::check;
using text_path_batch_window_smoke::events;
using text_path_batch_window_smoke::same;
inline int neutral_tabs=0;
inline void context(bool pending,bool link=false,const std::string& cause="current") {
    std::cout<<"ORIGIN_SOURCE_BEGIN pending="<<pending<<" link="<<link<<" cause="<<cause<<" platform="
        <<QGuiApplication::platformName().toStdString()<<" Qt="<<qVersion()<<std::endl;
    auto d=text_path_rechoose_window_smoke::fixture(false);d.id="origin-own-source-keyboard-document";
    const Ref target{"second","","text.origin_y"},source{"source","","text.origin_y"};
    check(decode(encode(d))==d,"Independent full Origin Y fixture native-validates BEFORE Window");
    Session initial(d),scalar(d);auto scalar_document=d;
    if(pending&&cause!="invalid"){scalar_document.objects.at("second").text->parameters.at("origin_y").literal=64;
        scalar.apply({EditProperties{{target},64,false}},scalar.revision());}
    check(scalar.document()==scalar_document&&decode(encode(scalar_document))==scalar_document,
        "Independent full Origin Y scalar/history oracle native-validates BEFORE Window");
    Session linked=scalar;auto linked_document=scalar_document;
    auto& value=linked_document.objects.at("second").text->parameters.at("origin_y");
    value.binding=Binding{source,1,0};value.expression.reset();
    linked.apply({LinkProperties{{target},source,false}},linked.revision());
    check(linked.document()==linked_document&&decode(encode(linked_document))==linked_document,
        "Independent full exact Origin Y Link oracle native-validates BEFORE Window");
    auto replacement=scalar_document;replacement.objects.at("second").text->id="incoming-origin-text";
    replacement.objects.at("second").name="Incoming whole Origin Y target";
    check(decode(encode(replacement))==replacement,"Whole incoming target/source identity native-validates BEFORE Window");
    Session incoming=scalar;incoming.apply({Rename{"source","Incoming Origin Y source revision"}},incoming.revision());
    auto before=incoming;bool refused=false;
    try{incoming.apply({LinkProperties{{target},source,false}},scalar.revision());}
    catch(const Error& error){check(error.code=="REVISION_CONFLICT","Independent stale Origin Y Link reports exact refusal BEFORE Window");refused=true;}
    check(refused&&same(incoming,before),"Independent stale Link refusal retains whole incoming Session BEFORE Window");
    Session refused_session=initial;auto refused_document=d;
    if(cause=="source"){refused_document.objects.at("second").text->id="incoming-origin-text";refused_session=Session(refused_document);}
    if(cause=="target"){refused_document.objects.at("second").name="Incoming whole Origin Y target";refused_session=Session(refused_document);}
    if(cause=="revision"){refused_document.objects.at("source").name="Incoming own scalar revision";refused_session.apply({Rename{"source","Incoming own scalar revision"}},refused_session.revision());}
    check(refused_session.document()==refused_document&&decode(encode(refused_document))==refused_document,
        "Exact independent whole first-completion refusal Session native-validates BEFORE Window");
    QTemporaryDir scratch;check(scratch.isValid(),"Origin Y Window owns settings/native/recovery scratch");
    QSettings settings(scratch.filePath("settings.ini"),QSettings::IniFormat);
    Window w(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(settings),&settings);
    w.resize(1400,900);w.host.session=initial;w.host.session_id="origin-own-source-keyboard-session";
    w.host.edited();w.show();w.activateWindow();events();
    w.canvas->set_active_artboard("composition","artboard",false);w.canvas->set_selection("second");events();
    auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Production Properties dock exists");dock->show();dock->raise();events();
    auto* area=w.findChild<QScrollArea*>("inspector-scroll");
    QPointer<QLineEdit> input,start=w.findChild<QLineEdit*>("text-path-start"),spacing=w.findChild<QLineEdit*>("text-path-spacing");
    QPointer<QComboBox> contour=w.findChild<QComboBox*>("text-path-contour");
    for(auto* current:w.findChildren<QLineEdit*>()){
        const auto ref=QJsonDocument::fromJson(current->property("nect-reference").toByteArray()).object();
        if(current->isVisible()&&ref.value("object")=="second"&&ref.value("point").toString().isEmpty()&&ref.value("field")=="text.origin_y")input=current;
    }
    check(area&&input&&start&&spacing&&contour,"Exact Origin Y input and original Path controls exist");
    text_path_pending_chooser_window_smoke::PendingCleanup cleanup{w,input};
    start->setText("1100");spacing->setText("3.5");
    area->ensureWidgetVisible(input);events();text_path_rechoose_window_smoke::point(w,input);
    if(pending){QTest::keyClick(w.windowHandle(),Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(input,cause=="invalid"?"bad-number":"64");events();}
    check(input->hasFocus()&&input->isModified()==pending&&same(w.host.session,initial),
        "Actual Origin Y draft is full-Session neutral before keyboard completion");
    const auto original_input=input,original_start=start;const auto original_contour=contour;
    const auto selected=w.canvas->selections();const auto composition=w.canvas->active_composition(),artboard=w.canvas->active_artboard();
    const auto session_id=w.host.session_id;
    text_path_keyboard_window_smoke::FocusOutProof focus_out;input->installEventFilter(&focus_out);
    int completions=0;QObject::connect(input,&QLineEdit::editingFinished,&w,[&]{++completions;});
    if(cause!="current"){
        w.host.session=refused_session;
        QTest::keyClick(w.windowHandle(),Qt::Key_Tab);events();
        const auto code=cause=="source"?"TEXT_EDIT_CONFLICT":cause=="target"?"PROPERTY_CONFLICT":cause=="revision"?"REVISION_CONFLICT":"INVALID_VALUE";
        check(w.statusBar()->currentMessage().startsWith(code)&&same(w.host.session,refused_session),"First own Origin Y Tab refuses exact incoming context or invalid scalar atomically");
        check(original_input&&original_contour&&original_start&&input->isModified()&&input->text()==(cause=="invalid"?"bad-number":"64")&&
            start->text()=="1100"&&spacing->text()=="3.5"&&!w.findChild<QDialog*>("property-source-picker"),"Rejected first completion retains original editable scalar and raw Path controls");
        check(w.host.session_id==session_id&&w.canvas->selections()==selected&&w.canvas->active_composition()==composition&&w.canvas->active_artboard()==artboard,
            "Rejected own scalar completion preserves exact original target/session/scope");
        std::cout<<"ORIGIN_SOURCE_REFUSAL cause="<<cause<<" code="<<code<<" revision="<<w.host.session.revision()<<" completions="<<completions<<" atomic=1 controls=live"<<std::endl;
        const auto native=scratch.filePath("origin-refusal.nect");w.host.save(native);w.host.flush();check(same(w.host.session,refused_session),"Refusal save preserves full incoming Session/history");
        Window cold(scratch.filePath("refusal-cold"),std::make_unique<FolderLibrary>(settings),&settings);cold.setAttribute(Qt::WA_DontShowOnScreen);cold.host.open(native);cold.show();events();
        check(cold.host.session.document()==refused_document&&encode(cold.host.session.document())==encode(refused_document),"Cold native retains complete incoming refusal state");
        cold.host.changed={};cold.hide();events();return;
    }
    QPointer<QPushButton> action;int tabs=0;
    const int limit=pending?neutral_tabs:8;
    for(;tabs<limit;){
        QTest::keyClick(w.windowHandle(),Qt::Key_Tab);events();++tabs;
        auto* focus=QApplication::focusWidget();
        const auto ref=focus?QJsonDocument::fromJson(focus->property("nect-reference").toByteArray()).object():QJsonObject{};
        std::cout<<"ORIGIN_SOURCE_TAB pending="<<pending<<" tab="<<tabs<<" reason="<<focus_out.reason<<" completions="<<completions
            <<" focus="<<(focus?focus->objectName().toStdString():"null")<<" field="<<ref.value("field").toString().toStdString()
            <<" original_input="<<bool(original_input)<<" original_path="<<bool(original_contour)<<" revision="<<w.host.session.revision()
            <<" start="<<(w.findChild<QLineEdit*>("text-path-start")?w.findChild<QLineEdit*>("text-path-start")->text().toStdString():"missing")<<std::endl;
        check(same(w.host.session,scalar),"First and subsequent actual Tab retain independent complete Origin Y scalar Session");
        if(focus&&focus->objectName()=="property-source-pick"&&ref.value("object")=="second"&&ref.value("point").toString().isEmpty()&&ref.value("field")=="text.origin_y"){
            action=qobject_cast<QPushButton*>(focus);break;
        }
    }
    if(!pending)neutral_tabs=tabs;
    check(action&&tabs==neutral_tabs&&focus_out.reason==Qt::TabFocusReason,
        "Pending traversal reaches exact neutral-qualified own Origin Y source action");
    check(w.host.session_id==session_id&&w.canvas->selections()==selected&&w.canvas->active_composition()==composition&&w.canvas->active_artboard()==artboard,
        "Origin Y completion preserves exact original target/session/scope");
    start=w.findChild<QLineEdit*>("text-path-start");spacing=w.findChild<QLineEdit*>("text-path-spacing");contour=w.findChild<QComboBox*>("text-path-contour");
    check(start&&spacing&&contour&&start->text()=="1100"&&spacing->text()=="3.5","Origin Y completion retains full original raw Path drafts");
    std::cout<<"ORIGIN_SOURCE_ROUTE pending="<<pending<<" tabs="<<tabs<<" original_input="<<bool(original_input)<<" original_start="<<bool(original_start)<<std::endl;
    int pressed=0,released=0,clicked=0;
    QObject::connect(action,&QPushButton::pressed,&w,[&]{++pressed;});QObject::connect(action,&QPushButton::released,&w,[&]{++released;});
    QObject::connect(action,&QPushButton::clicked,&w,[&]{++clicked;});
    QTest::keyPress(w.windowHandle(),Qt::Key_Space);events();
    check(action&&action->isDown()&&pressed==1&&released==0&&clicked==0&&!w.findChild<QDialog*>("property-source-picker")&&same(w.host.session,scalar),
        "Actual Space press arms own source action without authored mutation");
    QTest::keyRelease(w.windowHandle(),Qt::Key_Space);events();
    QPointer<QDialog> picker=w.findChild<QDialog*>("property-source-picker");
    check(picker&&picker->isVisible()&&picker->parentWidget()==&w&&picker->testAttribute(Qt::WA_DeleteOnClose)&&pressed==1&&released==1&&clicked==1,
        "Actual Space release opens exactly Window-owned Origin Y picker");
    auto* search=picker->findChild<QLineEdit*>("property-source-picker-search");auto* list=picker->findChild<QListWidget*>("property-source-picker-list");
    check(search&&search->hasFocus()&&list&&same(w.host.session,scalar),"Own Source picker opens neutral with existing search focus");
    if(link){
        QTest::keyClicks(search,"source text.origin_y");events();QTest::keyClick(picker->windowHandle(),Qt::Key_Tab);events();
        check(list->hasFocus(),"Actual search Tab reaches existing Source list");QTest::keyClick(picker->windowHandle(),Qt::Key_Home);events();
        auto* chosen=list->currentItem();const auto ref=chosen?QJsonDocument::fromJson(chosen->data(Qt::UserRole).toByteArray()).object():QJsonObject{};
        check(chosen&&!chosen->isHidden()&&ref.value("object")=="source"&&ref.value("field")=="text.origin_y"&&same(w.host.session,scalar),
            "Actual source browsing resolves exact stable source without authored mutation");
        QTest::keyClick(picker->windowHandle(),Qt::Key_Tab);events();auto* buttons=picker->findChild<QDialogButtonBox*>();
        check(buttons&&buttons->button(QDialogButtonBox::Ok)->hasFocus(),"Actual source-list Tab reaches existing OK");
        QTest::keyClick(picker->windowHandle(),Qt::Key_Space);events();
        check(!picker&&same(w.host.session,linked),"Existing keyboard OK links frozen own target with full independent Session");
        text_path_batch_window_smoke::history(w,"Undo");linked.undo(linked.revision());check(same(w.host.session,linked)&&linked.document()==scalar_document,"Link Undo retains independent own scalar");
        if(pending){text_path_batch_window_smoke::history(w,"Undo");linked.undo(linked.revision());check(same(w.host.session,linked)&&linked.document()==d,"Own Scalar Undo remains separate");
            text_path_batch_window_smoke::history(w,"Redo");linked.redo(linked.revision());check(same(w.host.session,linked),"Independent own Scalar Redo");}
        text_path_batch_window_smoke::history(w,"Redo");linked.redo(linked.revision());check(same(w.host.session,linked)&&linked.document()==linked_document,"Independent Link Redo restores full linked native state");scalar=linked;
    }else{
        QTest::keyClick(picker->windowHandle(),Qt::Key_Escape);events();
        check(!picker&&action&&start&&spacing&&contour&&same(w.host.session,scalar)&&start->text()=="1100"&&spacing->text()=="3.5",
            "Escape Cancel retains full own scalar Session and original raw controls");
        if(pending){auto history=scalar;text_path_batch_window_smoke::history(w,"Undo");history.undo(history.revision());check(same(w.host.session,history)&&history.document()==d,"Cancel retains independent own Scalar Undo");
            text_path_batch_window_smoke::history(w,"Redo");history.redo(history.revision());check(same(w.host.session,history),"Cancel retains independent own Scalar Redo");scalar=history;}
    }
    check(w.host.session_id==session_id&&w.canvas->selections()==selected&&w.canvas->active_composition()==composition&&w.canvas->active_artboard()==artboard,"Picker completion restores exact original target/session/scope");
    auto native=scratch.filePath("origin-own-source.nect");w.host.save(native);w.host.flush();check(same(w.host.session,scalar),"Save retains complete independent Session/history");
    Window cold(scratch.filePath("cold-recovery"),std::make_unique<FolderLibrary>(settings),&settings);cold.setAttribute(Qt::WA_DontShowOnScreen);cold.resize(1400,900);cold.host.open(native);cold.show();events();
    check(cold.host.session.document()==scalar.document()&&encode(cold.host.session.document())==encode(scalar.document()),"Cold Window preserves entire Text/Path/Contour/Composition/Refs/native state");
    cold.host.changed={};cold.hide();events();
    std::cout<<"ORIGIN_SOURCE_PASS pending="<<pending<<" link="<<link<<" native=PASS physical OS input NOT_RUN"<<std::endl;
}
inline void run(){const auto before=text_path_batch_window_smoke::checks;context(false);context(true);context(true,true);
    context(true,false,"source");context(true,false,"target");context(true,false,"revision");context(true,false,"invalid");
    std::cout<<"OriginOwnSourceKeyboard Windows cases=7 checks="<<text_path_batch_window_smoke::checks-before<<" failures=0 physical OS input NOT_RUN"<<std::endl;}
}

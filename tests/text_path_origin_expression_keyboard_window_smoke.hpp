#pragma once
#include <QPlainTextEdit>

namespace text_path_origin_expression_keyboard_window_smoke {
using text_path_batch_window_smoke::check;
using text_path_batch_window_smoke::events;
using text_path_batch_window_smoke::same;
inline void context(bool pending,bool apply,const std::string& incoming_mode={}) {
    std::cout<<"ORIGIN_EXPRESSION_BEGIN pending="<<pending<<" apply="<<apply<<" incoming="<<incoming_mode<<" platform="
        <<QGuiApplication::platformName().toStdString()<<" Qt="<<qVersion()<<std::endl;
    auto d=text_path_rechoose_window_smoke::fixture(false);d.id="origin-expression-keyboard-document";
    const Ref target{"second","","text.origin_y"};
    check(decode(encode(d))==d,"Whole independent Origin Y expression fixture native-validates BEFORE Window");
    Session initial(d),scalar(d);auto scalar_document=d;
    if(pending){scalar_document.objects.at("second").text->parameters.at("origin_y").literal=64;
        scalar.apply({EditProperties{{target},64,false}},scalar.revision());}
    check(scalar.document()==scalar_document&&decode(encode(scalar_document))==scalar_document,
        "Independent whole Origin Y numeric completion oracle native-validates BEFORE Window");
    Session expression=scalar;auto expression_document=scalar_document;
    auto& value=expression_document.objects.at("second").text->parameters.at("origin_y");
    value.expression=Expression{"80+4",1};value.binding.reset();
    expression.apply({SetExpression{{target},{"80+4",1},false}},expression.revision());
    check(expression.document()==expression_document&&decode(encode(expression_document))==expression_document,
        "Independent entire Origin Y expression/history/native oracle exists BEFORE Window");
    Session incoming=scalar;auto incoming_document=scalar_document;
    if(incoming_mode=="source") {
        auto base=d;base.objects.at("second").name="Incoming full Origin expression target";
        base.objects.at("second").text->id="incoming-origin-expression-text";
        base.objects.at("second").text->content="Incoming 日本語 body";
        incoming=Session(base);incoming.apply({EditProperties{{target},64,false}},incoming.revision());
        incoming_document=base;incoming_document.objects.at("second").text->parameters.at("origin_y").literal=64;
        check(incoming.revision()==scalar.revision(),"Independent changed full Text source retains equal scalar revision BEFORE Window");
    }else if(incoming_mode=="revision") {
        incoming.apply({Rename{"source","Incoming independent revision"}},incoming.revision());
        incoming_document.objects.at("source").name="Incoming independent revision";
        auto stale=incoming;bool refused=false;
        try{stale.apply({SetExpression{{target},{"80+4",1},false}},scalar.revision());}
        catch(const Error& error){check(error.code=="REVISION_CONFLICT","Independent canonical stale expression reports REVISION_CONFLICT BEFORE Window");refused=true;}
        check(refused&&same(stale,incoming),"Independent stale expression refuses the entire incoming Session/history/gesture atomically BEFORE Window");
    }
    if(!incoming_mode.empty())check(incoming.document()==incoming_document&&decode(encode(incoming_document))==incoming_document,
        "Independent whole incoming Text/Path/Contour/Composition/source/history oracle native-validates BEFORE Window");
    // The production result renders Error::what(), without its separate code.
    // Bind both to the existing frozen Text context owner before opening Window.
    const Error expected_refusal=incoming_mode=="source"
        ?Error("TEXT_EDIT_CONFLICT","The original Text source no longer exists. This draft cannot target a replacement.")
        :Error("REVISION_CONFLICT","The document was edited. Copy this draft, cancel, and reopen the latest Text.");
    const auto expected_result=QString::fromUtf8(expected_refusal.what())+"\nCommitted result is unchanged.";
    QTemporaryDir scratch;check(scratch.isValid(),"Origin Y expression owns settings/recovery/native scratch");
    QSettings settings(scratch.filePath("settings.ini"),QSettings::IniFormat);
    Window w(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(settings),&settings);
    w.resize(1400,900);w.host.session=initial;w.host.session_id="origin-expression-keyboard-session";
    w.host.edited();w.show();w.activateWindow();events();
    w.canvas->set_active_artboard("composition","artboard",false);w.canvas->set_selection("second");events();
    auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Production Properties dock exists");dock->show();dock->raise();events();
    auto* area=w.findChild<QScrollArea*>("inspector-scroll");QPointer<QLineEdit> input;
    auto* start=w.findChild<QLineEdit*>("text-path-start");auto* spacing=w.findChild<QLineEdit*>("text-path-spacing");
    for(auto* current:w.findChildren<QLineEdit*>()){
        const auto ref=QJsonDocument::fromJson(current->property("nect-reference").toByteArray()).object();
        if(current->isVisible()&&ref.value("object")=="second"&&ref.value("point").toString().isEmpty()&&ref.value("field")=="text.origin_y")input=current;
    }
    check(area&&input&&start&&spacing,"Exact original own Origin Y and Path inputs exist");
    text_path_pending_chooser_window_smoke::PendingCleanup cleanup{w,input};
    start->setText("1100");spacing->setText("3.5");area->ensureWidgetVisible(input);events();text_path_rechoose_window_smoke::point(w,input);
    if(pending){QTest::keyClick(w.windowHandle(),Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(input,"64");events();}
    check(same(w.host.session,initial)&&input->hasFocus()&&input->isModified()==pending,"Own numeric draft remains entirely Session-neutral");
    const auto session_id=w.host.session_id;const auto composition=w.canvas->active_composition(),artboard=w.canvas->active_artboard();const auto selected=w.canvas->selections();
    QTest::keyClick(w.windowHandle(),Qt::Key_Tab);events();
    QPointer<QPushButton> action=qobject_cast<QPushButton*>(QApplication::focusWidget());
    check(action&&action->objectName()=="property-expression"&&action->property("nect-text-scalar-fx-action-object").toString()=="second"&&
        action->property("nect-text-scalar-fx-action-point").toString().isEmpty()&&action->property("nect-text-scalar-fx-action-field").toString()=="text.origin_y"&&same(w.host.session,scalar),
        "Actual first Tab reaches exact same-target existing own expression action with full scalar Session");
    int pressed=0,released=0,clicked=0;
    QObject::connect(action,&QPushButton::pressed,&w,[&]{++pressed;});QObject::connect(action,&QPushButton::released,&w,[&]{++released;});QObject::connect(action,&QPushButton::clicked,&w,[&]{++clicked;});
    QTest::keyPress(w.windowHandle(),Qt::Key_Space);events();
    check(action&&action->isDown()&&pressed==1&&released==0&&clicked==0&&!w.findChild<QWidget*>("nect-expression-panel")&&same(w.host.session,scalar),
        "Actual existing expression Space press is source-neutral and has no editor yet");
    QTest::keyRelease(w.windowHandle(),Qt::Key_Space);events();
    auto* panel=w.findChild<QWidget*>("nect-expression-panel");QPointer<QPlainTextEdit> editor=panel?panel->findChild<QPlainTextEdit*>():nullptr;
    const auto ref=editor?QJsonDocument::fromJson(editor->property("nect-reference").toByteArray()).object():QJsonObject{};
    check(panel&&editor&&editor->isVisible()&&editor->hasFocus()&&ref.value("object")=="second"&&ref.value("field")=="text.origin_y"&&
        pressed==1&&released==1&&clicked==1&&same(w.host.session,scalar),"Actual expression Space release opens exactly own editor with full Session neutral");
    check(editor->toPlainText()==QString::number(evaluate(scalar.document()).at(target),'g',17),"Expression editor starts with independent evaluated own scalar");
    start=w.findChild<QLineEdit*>("text-path-start");spacing=w.findChild<QLineEdit*>("text-path-spacing");
    check(start&&spacing&&start->text()=="1100"&&spacing->text()=="3.5","Own expression opening retains raw original Path drafts");
    QTest::keyClick(w.windowHandle(),Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(editor,"80+4");events();
    check(editor->toPlainText()=="80+4"&&same(w.host.session,scalar),"Actual expression draft remains complete Session/history neutral");
    if(!incoming_mode.empty()) {
        auto* result=panel->findChild<QLabel*>("nect-expression-result");check(result,"Exact own expression result exists before incoming context");
        w.host.session=incoming;
        check(editor&&editor->hasFocus()&&same(w.host.session,incoming),"Incoming full Session is installed with existing owned editor and actual focus retained");
        QTest::keyClick(w.windowHandle(),Qt::Key_Return,Qt::ControlModifier);events();
        std::cout<<"ORIGIN_EXPRESSION_INCOMING_RESULT mode="<<incoming_mode<<" owner_code="<<expected_refusal.code<<" result="<<result->text().toStdString()<<std::endl;
        check(editor&&editor->isVisible()&&editor->hasFocus()&&editor->toPlainText()=="80+4"&&result->text()==expected_result,
            "Actual Ctrl+Enter retains own focused editor/draft and reports exact incoming context refusal");
        check(same(w.host.session,incoming)&&w.host.session_id==session_id&&w.canvas->selections()==selected&&
            w.canvas->active_composition()==composition&&w.canvas->active_artboard()==artboard,
            "Actual incoming refusal preserves entire Session/history/revision/gesture and target/context");
        QTest::keyClick(w.windowHandle(),Qt::Key_Escape);events();
        check(!editor&&same(w.host.session,incoming),"Actual Escape discards only stale own expression and retains complete incoming Session");
        auto history=incoming;text_path_batch_window_smoke::history(w,"Undo");history.undo(history.revision());
        check(same(w.host.session,history),"Incoming refusal and Cancel add no history: exact independent incoming Undo remains intact");
        text_path_batch_window_smoke::history(w,"Redo");history.redo(history.revision());
        check(same(w.host.session,history)&&history.document()==incoming_document,"Independent incoming Redo restores entire incoming native source");scalar=history;
    }else if(apply){
        QTest::keyClick(w.windowHandle(),Qt::Key_Return,Qt::ControlModifier);events();
        check(!editor&&same(w.host.session,expression),"Actual existing Ctrl+Enter applies exact own expression and independent full canonical Session");
        text_path_batch_window_smoke::history(w,"Undo");expression.undo(expression.revision());check(same(w.host.session,expression)&&expression.document()==scalar_document,"Expression Undo retains independent own scalar completion");
        if(pending){text_path_batch_window_smoke::history(w,"Undo");expression.undo(expression.revision());check(same(w.host.session,expression)&&expression.document()==d,"Own scalar Undo remains separate from expression");
            text_path_batch_window_smoke::history(w,"Redo");expression.redo(expression.revision());check(same(w.host.session,expression),"Independent own scalar Redo");}
        text_path_batch_window_smoke::history(w,"Redo");expression.redo(expression.revision());check(same(w.host.session,expression)&&expression.document()==expression_document,"Independent own expression Redo");scalar=expression;
    }else{
        QTest::keyClick(w.windowHandle(),Qt::Key_Escape);events();
        check(!editor&&same(w.host.session,scalar),"Actual expression Escape discards only expression draft with full scalar Session retained");
        if(pending){auto history=scalar;text_path_batch_window_smoke::history(w,"Undo");history.undo(history.revision());check(same(w.host.session,history)&&history.document()==d,"Expression Cancel retains independent numeric Undo");
            text_path_batch_window_smoke::history(w,"Redo");history.redo(history.revision());check(same(w.host.session,history),"Expression Cancel retains independent numeric Redo");scalar=history;}
    }
    check(w.host.session_id==session_id&&w.canvas->selections()==selected&&w.canvas->active_composition()==composition&&w.canvas->active_artboard()==artboard,"Expression completion retains exact target/session/Composition scope");
    const auto native=scratch.filePath("origin-expression.nect");w.host.save(native);w.host.flush();check(same(w.host.session,scalar),"Save retains entire independent Session/history");
    Window cold(scratch.filePath("cold"),std::make_unique<FolderLibrary>(settings),&settings);cold.setAttribute(Qt::WA_DontShowOnScreen);cold.resize(1400,900);cold.host.open(native);cold.show();events();
    check(cold.host.session.document()==scalar.document()&&encode(cold.host.session.document())==encode(scalar.document()),"Cold owned Window preserves full native expression/Text/Path/Composition/Refs");
    cold.host.changed={};cold.hide();events();
    std::cout<<"ORIGIN_EXPRESSION_PASS pending="<<pending<<" apply="<<apply<<" incoming="<<incoming_mode<<" native=PASS physical OS input NOT_RUN"<<std::endl;
}
inline void run(){const int before=text_path_batch_window_smoke::checks;context(false,false);context(false,true);context(true,false);context(true,true);
    std::cout<<"OriginExpressionKeyboard Windows cases=4 checks="<<text_path_batch_window_smoke::checks-before<<" failures=0 physical OS input NOT_RUN"<<std::endl;}
inline void run_incoming(){const int before=text_path_batch_window_smoke::checks;context(true,false,"source");context(true,false,"revision");
    std::cout<<"OriginExpressionKeyboardIncoming Windows cases=2 checks="<<text_path_batch_window_smoke::checks-before<<" failures=0 physical OS input NOT_RUN"<<std::endl;}
}

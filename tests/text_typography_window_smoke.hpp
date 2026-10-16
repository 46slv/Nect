#pragma once
#include <QAction>
#include <QDockWidget>
#include <QJsonDocument>
#include <QJsonObject>
#include <QScrollArea>
#include <QStatusBar>
#include <QWindow>

// Actual production Window/dialog pointer routes with native-valid fixtures.
// This qualifies authored settings, not installed-font coverage or OS input.
namespace text_typography_window_smoke {
inline int checks=0;
inline void check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
inline void events(){QApplication::processEvents();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QTest::qWait(20);}
inline bool same(const Session& a,const Session& b){return a.document()==b.document()&&a.preview_document()==b.preview_document()&&
    encode(a.document())==encode(b.document())&&a.history()==b.history()&&a.revision()==b.revision()&&
    a.gesture_generation()==b.gesture_generation()&&a.gesture_active()==b.gesture_active();}
inline Document native_fixture(){
    auto d=empty_document("typography-window-document","composition","artboard");
    Object text;text.id="text";text.name="Whole authored Text";text.kind=Kind::text;text.text=default_text("text-source","Keep 日本語 AV ffi");
    text.text->family="Arial";text.text->locale="ja-JP";text.text->weight=500;
    text.text->font_features={{"KERN",1,"whole_text"},{"lig ",4,"whole_text"},{"<br>",17,"whole_text"}};
    text.text->additional_axis_values={{"wdth",87.1234567890123},{"WGHT",1e3}};
    text.text->content_driver=TextContentDriver{{"source","","text.content"}};
    text.text->path_attachment=TextPathAttachment{"path","contour","distance",10,2,false};
    Object source=text;source.id="source";source.name="Content source";source.text->id="content-source";
    source.text->content_driver.reset();source.text->path_attachment.reset();
    Object path;path.id="path";path.name="Attached Path";path.kind=Kind::path;
    Contour contour;contour.id="contour";Point a;a.id="first";a.x.literal=10;a.y.literal=100;
    Point b;b.id="last";b.x.literal=600;b.y.literal=100;contour.points={a,b};path.contours={contour};
    d.objects.emplace(text.id,text);d.objects.emplace(source.id,source);d.objects.emplace(path.id,path);
    d.compositions.front().roots={"text","source","path"};
    auto other=empty_document("unused","other-composition","other-artboard").compositions.front();d.compositions.push_back(other);
    return d;
}
inline void history(Window& w,const char* title){for(auto* action:w.findChildren<QAction*>())if(action->text()==QString::fromLatin1(title)){
    check(action->isEnabled(),"Production History action enabled");action->trigger();events();return;}throw std::runtime_error("History action missing");}
inline void cold(Window& w,const Session& expected){
    QTemporaryDir scratch;check(scratch.isValid(),"Native cold reopen owns scratch");const auto path=scratch.filePath("typography.nect");
    w.host.save(path);w.host.flush();check(same(w.host.session,expected),"Native save preserves full live Session");
    Window reopened(scratch.filePath("recovery"));reopened.setAttribute(Qt::WA_DontShowOnScreen);reopened.resize(1400,900);
    reopened.host.open(path);reopened.show();events();
    check(reopened.host.session.document()==expected.document()&&encode(reopened.host.session.document())==encode(expected.document()),
        "Cold production Window retains complete source/style/tag order/values/IDs/Refs/attachment");
    reopened.host.changed={};reopened.hide();events();
}
inline void point(QWidget& window,QWidget* widget){
    const auto pos=widget->mapTo(&window,widget->rect().center());check(window.childAt(pos)==widget,"Actual pointer hits intended production control");
    QTest::mouseClick(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,pos);events();
}
inline void context(const std::string& action,const std::string& phase,const std::string& cause,bool pending=false){
    const bool axis=action.starts_with("axis"),edit=action.ends_with("edit"),remove=action.ends_with("remove");
    QTemporaryDir scratch;check(scratch.isValid(),"Typography Window owns scratch");const auto document=native_fixture();
    check(decode(encode(document))==document,"Typography fixture native-validates before GUI");
    QSettings settings(scratch.filePath("settings.ini"),QSettings::IniFormat);
    Window w(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(settings),&settings);
    if(!pending)w.setAttribute(Qt::WA_DontShowOnScreen);w.resize(1400,900);w.host.session=Session(document);
    w.host.session_id="typography-window-session";w.host.edited();w.show();if(pending)w.activateWindow();events();
    w.canvas->set_active_artboard("composition","artboard",false);w.canvas->set_selection("text");events();
    auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Production Properties exists");dock->show();dock->raise();events();
    auto* area=w.findChild<QScrollArea*>("inspector-scroll");check(area,"Production Inspector scroll exists");
    const auto name=axis?(remove?"text-font-axis-remove":edit?"text-font-axis-edit":"text-font-axis-add"):
        (remove?"text-font-feature-remove":edit?"text-font-feature-edit":"text-font-feature-add");
    QPointer<QPushButton> button;
    for(auto* item:w.findChildren<QPushButton*>(name))if(!edit&&!remove||item->property("font-tag").toString()==(axis?"wdth":"KERN"))button=item;
    check(button&&button->isEnabled(),"Exact existing whole-Text feature/axis action exists");
    QPointer<QLineEdit> size;const Session original=w.host.session;
    if(pending){for(auto* input:w.findChildren<QLineEdit*>()){
        const auto ref=QJsonDocument::fromJson(input->property("nect-reference").toByteArray()).object();
        if(input->isVisible()&&ref.value("object").toString()=="text"&&ref.value("field").toString()=="text.font_size")size=input;
    }
        check(size,"Production Font size exists");area->ensureWidgetVisible(size);events();point(w,size);
        QTest::keyClick(size,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(size,"64");events();
        check(size->hasFocus()&&size->isModified()&&same(w.host.session,original),"Legitimate focused Font size draft is fully Session-neutral");
    }
    area->ensureWidgetVisible(button);events();
    const auto replace=[&]{auto incoming=document;
        if(cause=="body")incoming.objects.at("text").text->content="Incoming authored body";
        else if(cause=="driver")incoming.objects.at("text").text->content_driver.reset();
        else if(cause=="style")incoming.objects.at("text").text->weight=600;
        else if(cause=="object")incoming.objects.at("text").name="Incoming whole Object";
        else if(cause=="features")incoming.objects.at("text").text->font_features.front().parameter=9;
        else if(cause=="axes")incoming.objects.at("text").text->additional_axis_values.at("wdth")=90;
        else if(cause=="attachment")incoming.objects.at("text").text->path_attachment->start=20;
        else if(cause=="composition"){incoming.compositions.back().roots=std::move(incoming.compositions.front().roots);incoming.compositions.front().roots.clear();}
        else if(cause=="document")incoming.id="incoming-document";
        else if(cause=="source-id")incoming.objects.at("text").text->id="replacement-source";
        else if(cause=="unrelated")incoming.compositions.back().name="Independent composition";
        else if(cause=="path")incoming.objects.at("path").contours.front().points.front().y.literal=105;
        else if(cause!="equivalent")throw std::runtime_error("Unknown replacement fixture");
        check((incoming==document)==(cause=="equivalent"),"Replacement changes exactly the intended authored fixture");
        check(decode(encode(incoming))==incoming,"Incoming fixture native-validates before replacement");Session replacement(incoming);
        if(pending&&phase=="apply")replacement.apply({EditProperties{{{"text","","text.font_size"}},64,false}},replacement.revision());
        check(replacement.revision()==w.host.session.revision(),"Incoming replacement has same revision");w.host.session=std::move(replacement);
    };
    const bool replacement=cause=="body"||cause=="driver"||cause=="style"||cause=="object"||cause=="features"||cause=="axes"||
        cause=="attachment"||cause=="composition"||cause=="document"||cause=="source-id"||cause=="equivalent"||cause=="unrelated"||cause=="path";
    const auto change=[&]{
        if(replacement)replace();
        else if(cause=="session")w.host.session_id+="-incoming";
        else if(cause=="revision")w.host.session.apply({Rename{"source","External edit"}},w.host.session.revision());
        else if(cause=="preview"||cause=="generation"){
            w.host.session.begin_gesture(w.host.session.revision());w.host.session.update_gesture({Rename{"source","Preview edit"}});
            if(cause=="generation")w.host.session.cancel_gesture();
        }else if(cause=="selection"||cause=="selection-return"){w.canvas->set_selection("source");if(cause=="selection-return")w.canvas->set_selection("text");events();}
        else if(cause=="scope"){w.canvas->set_active_artboard("other-composition","other-artboard",false);events();}
        else if(cause=="refresh"){w.host.edited();events();}
    };
    const bool stale=cause=="body"||cause=="driver"||cause=="style"||cause=="object"||cause=="features"||cause=="axes"||cause=="attachment"||
        cause=="composition"||cause=="document"||cause=="source-id"||cause=="session"||cause=="revision"||cause=="preview"||cause=="generation"||
        cause=="selection"||cause=="selection-return"||cause=="scope";
    const auto code=cause=="document"||cause=="session"?"SESSION_CONFLICT":cause=="revision"||cause=="generation"?"REVISION_CONFLICT":
        cause=="preview"?"GESTURE_ACTIVE":cause=="composition"||cause=="selection"||cause=="selection-return"||cause=="scope"?"SELECTION_CONFLICT":
        cause=="source-id"?"TEXT_EDIT_CONFLICT":"PROPERTY_CONFLICT";
    if(phase=="entry")change();Session expected=w.host.session;const auto selected=w.canvas->selections();
    auto complete=expected.document();
    if(pending&&!(phase=="entry"&&stale)){
        complete.objects.at("text").text->parameters.at("font_size").literal=64;
        expected.apply({EditProperties{{{"text","","text.font_size"}},64,false}},expected.revision());
        check(expected.document()==complete,"Independent full Document equals canonical Font size command");
    }
    point(w,button);QPointer<QDialog> dialog=w.findChild<QDialog*>("text-typography-dialog");
    std::cout<<"TypographyWindow "<<action<<" "<<phase<<" "<<cause<<" pending="<<pending<<" dialog="<<bool(dialog&&dialog->isVisible())
        <<" actual="<<w.host.session.revision()<<" before="<<expected.revision()<<std::endl;
    check(same(w.host.session,expected),"Entry preserves full incoming Session or commits only legitimate Font size");
    if(phase=="entry"&&stale){
        check(!dialog||!dialog->isVisible(),"Stale entry never opens a writable dialog");check(w.statusBar()->currentMessage().contains(code),"Entry reports precise context refusal");
        if(pending)check(size&&size->isModified()&&size->text()=="64","Stale entry retains unfinished Font size");
        w.host.changed={};w.hide();events();return;
    }
    check(dialog&&dialog->isVisible()&&dialog->parentWidget()==&w,"Actual first pointer opens a Window-owned dialog");
    auto* tag=dialog->findChild<QLineEdit*>("text-typography-tag");auto* value=dialog->findChild<QLineEdit*>("text-typography-value");
    check(tag&&value,"Exact tag/value fields exist");
    const QString key=axis?(edit||remove?"wdth":" A  "):(edit||remove?"KERN":"%3  ");
    const QString numeric=axis?"-12.375":"4294967295";
    if(!edit&&!remove){point(*dialog,tag);QTest::keyClicks(tag,key);events();}else check(tag->isReadOnly(),"Existing exact tag is immutable");
    if(!remove){point(*dialog,value);QTest::keyClick(value,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(value,numeric);events();}
    check(tag->text()==key&&(remove||value->text()==numeric)&&same(w.host.session,expected),"Exact padded/case-sensitive draft and values remain Session-neutral");
    if(phase=="apply")change();expected=w.host.session;complete=expected.document();
    const auto selection_now=w.canvas->selections();
    if(cause=="cancel"){
        point(*dialog,dialog->findChild<QPushButton*>("text-typography-cancel"));
        check(!dialog&&same(w.host.session,expected),"Explicit Cancel destroys dialog and preserves full Session");
        if(pending){history(w,"Undo");expected.undo(expected.revision());check(same(w.host.session,expected)&&expected.document()==document,"Cancel leaves only separately undoable Font size");}
        w.host.changed={};w.hide();events();return;
    }
    auto* apply=dialog->findChild<QPushButton*>("text-typography-apply");check(apply,"Explicit Apply exists");point(*dialog,apply);
    if(phase=="apply"&&stale){
        check(same(w.host.session,expected),"Stale Apply retains full incoming Document/native/History/preview/revision/generation");
        check(dialog&&dialog->isVisible()&&dialog->findChild<QLabel*>("text-typography-status")->text().contains(code),"Stale Apply retains exact draft and precise visible refusal");
        check(tag->text()==key&&(remove||value->text()==numeric)&&w.canvas->selections()==selection_now,"Rejected Apply preserves exact draft and selection");
        point(*dialog,dialog->findChild<QPushButton*>("text-typography-cancel"));check(same(w.host.session,expected),"Cancel after rejection preserves incoming full Session");
    }else{
        auto& text=*complete.objects.at("text").text;Command command;
        if(axis){if(remove){text.additional_axis_values.erase("wdth");command=RemoveTextAdditionalAxis{"text","wdth"};}
            else {text.additional_axis_values[key.toStdString()]=-12.375;command=SetTextAdditionalAxis{"text",key.toStdString(),-12.375};}}
        else if(remove){text.font_features.erase(text.font_features.begin());command=RemoveTextFontFeature{"text","KERN"};}
        else if(edit){text.font_features.front().parameter=4294967295u;command=UpdateTextFontFeature{"text","KERN",4294967295u};}
        else {text.font_features.push_back({key.toStdString(),4294967295u,"whole_text"});command=AddTextFontFeature{"text",{key.toStdString(),4294967295u,"whole_text"}};}
        expected.apply({command},expected.revision());check(expected.document()==complete,"Independent complete authored Document equals canonical typed command");
        check(!dialog&&same(w.host.session,expected),"Actual Apply equals complete canonical Session and closes owned dialog");
        check(w.canvas->selections()==selected&&decode(encode(complete))==complete,"Selection/tags/order/value/source/style/IDs/Refs/attachment/native preserved");
        history(w,"Undo");expected.undo(expected.revision());check(same(w.host.session,expected),"Typography Undo is independent of pending scalar");
        if(pending){history(w,"Undo");expected.undo(expected.revision());check(same(w.host.session,expected),"Second Undo restores original Font size");
            history(w,"Redo");expected.redo(expected.revision());check(same(w.host.session,expected),"Font size Redo remains independent");}
        history(w,"Redo");expected.redo(expected.revision());check(same(w.host.session,expected),"Typography Redo restores complete authored result");cold(w,expected);
    }
    w.host.changed={};w.hide();events();
}
inline int run(){int cases=0,failures=0;
    const auto test=[&](const std::string& action,const std::string& phase,const std::string& cause,bool pending=false){++cases;
        try{context(action,phase,cause,pending);}catch(const std::exception& error){++failures;std::cerr<<"TYPOGRAPHY_WINDOW_RED "<<action<<" "<<phase<<" "<<cause<<" pending="<<pending<<": "<<error.what()<<std::endl;}};
    for(const auto* action:{"feature-add","feature-edit","feature-remove","axis-add","axis-edit","axis-remove"}){
        for(const auto* cause:{"body","composition"})test(action,"entry",cause);
        for(const auto* cause:{"valid","body","driver","style","object","features","axes","attachment","composition","equivalent","unrelated","path","refresh","cancel"})test(action,"apply",cause);
        test(action,"apply","valid",true);test(action,"entry","body",true);test(action,"apply","body",true);test(action,"apply","cancel",true);
    }
    for(const auto* cause:{"document","session","revision","source-id","preview","generation","selection","selection-return","scope"})test("feature-add","apply",cause);
    std::cout<<"TypographyWindow cases="<<cases<<" checks="<<checks<<" failures="<<failures<<" physical OS input NOT_RUN"<<std::endl;return failures?1:0;
}
}

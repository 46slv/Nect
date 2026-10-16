#pragma once
#include "window.hpp"
#include "visual_style.hpp"
#include <QAction>
#include <QDockWidget>
#include <QScrollArea>
#include <QTest>
#include <QWindow>

// Native-validated, actual production Window pointers; synthetic Qt input is
// separate from physical OS input. Expected edits use canonical Session batches.
namespace text_path_batch_window_smoke {
inline int checks=0;
inline void check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
inline void events(){QApplication::processEvents();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QTest::qWait(20);}
inline bool same(const Session& a,const Session& b){return a.document()==b.document()&&a.preview_document()==b.preview_document()&&
    encode(a.document())==encode(b.document())&&a.history()==b.history()&&a.revision()==b.revision()&&
    a.gesture_generation()==b.gesture_generation()&&a.gesture_active()==b.gesture_active();}
inline Document native_fixture(bool attached){
    auto document=::fixture(attached);
    auto contour=document.objects.at("path").contours.front();contour.id="second-contour";
    for(auto& point:contour.points){point.id+="-second";point.y.literal+=100;}
    document.objects.at("path").contours.push_back(contour);
    auto composition=document.compositions.front();composition.id="other-composition";composition.name="Independent";composition.roots.clear();
    composition.artboards.front().id="other-artboard";document.compositions.push_back(composition);
    return document;
}
inline void history(Window& w,const char* label){for(auto* action:w.findChildren<QAction*>())if(action->text()==QString::fromLatin1(label)){
    check(action->isEnabled(),"Production history action enabled");action->trigger();events();return;}throw std::runtime_error("History action missing");}
inline void cold(Window& w,const Session& expected){QTemporaryDir files;check(files.isValid(),"Owned native cold directory");
    const auto path=files.filePath("text-path.nect");w.host.save(path);w.host.flush();check(same(w.host.session,expected),"Save preserves live full Session");
    Window reopened(files.filePath("recovery"));reopened.setAttribute(Qt::WA_DontShowOnScreen);reopened.resize(1400,900);
    reopened.host.open(path);reopened.show();events();
    check(reopened.host.session.document()==expected.document()&&encode(reopened.host.session.document())==encode(expected.document()),
        "Cold Window preserves complete native Text/Path/source/style/IDs/Refs/attachments");
    reopened.host.changed={};reopened.hide();events();
}
inline void context(const std::string& action,const std::string& cause,bool common_path=false){
    const bool attached=action!="attach";
    QTemporaryDir scratch;check(scratch.isValid(),"Text Path test owns scratch");auto document=native_fixture(attached);
    if(common_path)document.objects.at("second").text->path_attachment=document.objects.at("first").text->path_attachment;
    check(decode(encode(document))==document,"Fixture native-validates before Window operation");
    Window w(scratch.filePath("recovery"));w.setAttribute(Qt::WA_DontShowOnScreen);w.resize(1400,900);
    w.host.session=Session(document);w.host.session_id="text-path-window-context";w.host.edited();w.show();events();
    w.canvas->set_active_artboard("composition","artboard",false);w.canvas->set_selections({{"first",{}},{"second",{}}});events();
    auto* properties=w.findChild<QDockWidget*>("properties");check(properties,"Properties dock exists");properties->show();properties->raise();events();
    auto* area=w.findChild<QScrollArea*>("inspector-scroll");auto* path=w.findChild<QComboBox*>("text-path-batch-path");
    auto* contour=w.findChild<QComboBox*>("text-path-batch-contour");auto* start=w.findChild<QLineEdit*>("text-path-batch-start");
    auto* spacing=w.findChild<QLineEdit*>("text-path-batch-spacing");auto* apply=w.findChild<QPushButton*>("text-path-batch-apply");
    auto* detach=w.findChild<QPushButton*>("text-path-batch-detach");auto* cancel=w.findChild<QPushButton*>("text-path-batch-cancel");
    check(area&&path&&contour&&start&&spacing&&apply&&detach&&cancel,"Production whole-Text panel exists");
    if(common_path)check(path->currentData().toString()=="path"&&contour->currentData().toString()=="contour","Common attachment initially retains its exact source");
    const Session original=w.host.session;const auto index=path->findData("path");check(index>0,"Explicit Path ID exists");
    path->setCurrentIndex(index);const auto contour_index=contour->findData("contour");check(contour_index>0,"Explicit Contour ID exists");
    contour->setCurrentIndex(contour_index);
    if(!attached){start->setText("1000");spacing->setText("3.5");}else spacing->setText("5");
    events();check(apply->isEnabled()&&same(w.host.session,original),"Path/Contour/numeric browsing remains Session-neutral");
    auto* button=cause=="cancel"||cause=="cancel-stale-path"?cancel:action=="detach"?detach:apply;
    area->ensureWidgetVisible(button);events();const auto position=button->mapTo(&w,button->rect().center());
    check(w.childAt(position)==button,"Actual Window pointer hits exact Apply/Detach/Cancel");
    const bool replacement=cause=="body"||cause=="driver"||cause=="style"||cause=="object"||cause=="attachment"||cause=="path"||
        cause=="path-label"||cause=="rechoose"||cause=="cancel-stale-path"||cause=="composition"||cause=="document"||cause=="equivalent"||cause=="unrelated"||cause=="unselected-path";
    if(replacement){auto incoming=w.host.session.document();
        if(cause=="body")incoming.objects.at("second").text->content="Incoming Text";
        else if(cause=="driver")incoming.objects.at("first").text->content_driver.reset();
        else if(cause=="style")incoming.objects.at("second").text->parameters.at("font_size").literal=33;
        else if(cause=="object")incoming.objects.at("second").name="Incoming authored target";
        else if(cause=="attachment")incoming.objects.at("second").text->path_attachment=TextPathAttachment{"path","contour","distance",1100,2,false};
        else if(cause=="path"||cause=="rechoose"||cause=="cancel-stale-path")incoming.objects.at("path").contours.front().points.front().y.literal=110;
        else if(cause=="path-label")incoming.objects.at("path").name="Incoming chosen Path";
        else if(cause=="composition"){incoming.compositions.back().roots=std::move(incoming.compositions.front().roots);incoming.compositions.front().roots.clear();}
        else if(cause=="document")incoming.id="incoming-text-path-document";
        else if(cause=="unrelated")incoming.compositions.back().name="Independent composition rename";
        else if(cause=="unselected-path")incoming.objects.at("other-path").name="Independent unchosen Path";
        check((incoming==w.host.session.document())==(cause=="equivalent"),"Incoming replacement changes only intended context");
        check(decode(encode(incoming))==incoming,"Incoming same-revision replacement native-validates");w.host.session=Session(incoming);
    }else if(cause=="session")w.host.session_id+="-incoming";
    else if(cause=="revision")w.host.session.apply({Rename{"source","Incoming revision"}},w.host.session.revision());
    else if(cause=="generation"||cause=="preview"){
        w.host.session.begin_gesture(w.host.session.revision());w.host.session.update_gesture({Rename{"source","Incoming preview"}});
        if(cause=="generation")w.host.session.cancel_gesture();
    }
    if(cause=="rechoose"){
        const Session before=w.host.session;path->setCurrentIndex(0);path->setCurrentIndex(index);contour->setCurrentIndex(contour->findData("contour"));events();
        check(same(w.host.session,before)&&apply->isEnabled(),"Explicit rechoose accepts current Path/Contour source without an edit");
    }
    Session expected=w.host.session;const auto selected=w.canvas->selections();
    QTest::mouseClick(w.windowHandle(),Qt::LeftButton,Qt::NoModifier,position);events();
    const bool source_refusal=(cause=="path"||cause=="path-label")&&action!="detach";
    const bool refusal=cause=="body"||cause=="driver"||cause=="style"||cause=="object"||cause=="attachment"||source_refusal||cause=="composition"||
        cause=="document"||cause=="session"||cause=="revision"||cause=="generation"||cause=="preview";
    std::cout<<"TextPath "<<action<<" "<<cause<<(common_path?" common-source":"")<<" actual_revision="<<w.host.session.revision()<<" before="<<expected.revision()<<std::endl;
    if(refusal){check(same(w.host.session,expected),"Stale Text/Path/composition preserves complete incoming Session atomically");
        check(path->currentData().toString()=="path"&&contour->currentData().toString()=="contour","Refusal retains exact Path/Contour draft");
        const auto* status=w.findChild<QLabel*>("text-path-batch-status");
        const auto code=cause=="session"||cause=="document"?"SESSION_CONFLICT":cause=="revision"||cause=="generation"?"REVISION_CONFLICT":
            cause=="preview"?"GESTURE_ACTIVE":cause=="composition"?"SELECTION_CONFLICT":"PROPERTY_CONFLICT";
        check(status&&status->text().startsWith(code),"Stale context has exact visible status");
        check(w.canvas->selections()==selected,"Refusal preserves exact whole-Text selection");
    }else if(cause=="cancel-stale-path"){
        check(same(w.host.session,expected),"Cancel after source replacement remains completely neutral");spacing->setText("6");events();
        const auto* status=w.findChild<QLabel*>("text-path-batch-status");
        std::cout<<"Cancel-source neutral="<<same(w.host.session,expected)<<" enabled="<<apply->isEnabled()<<" status="<<(status?status->text().toStdString():"missing")<<std::endl;
        check(same(w.host.session,expected)&&!apply->isEnabled()&&status&&status->text().startsWith("PROPERTY_CONFLICT"),
            "Cancel restores the initial source snapshot instead of silently accepting a changed Path");
    }else if(cause=="cancel"){check(same(w.host.session,expected),"Cancel is completely neutral");check(path->currentData().toString().isEmpty(),"Cancel restores initial mixed/detached draft");}
    else{
        auto complete=expected.document();std::vector<Command> commands;
        for(const auto* id:{"first","second"}){auto text=*complete.objects.at(id).text;
            if(action=="detach")text.path_attachment.reset();
            else if(!attached)text.path_attachment=TextPathAttachment{"path","contour","distance",1000,3.5,false};
            else{text.path_attachment->path="path";text.path_attachment->contour="contour";text.path_attachment->spacing=5;}
            complete.objects.at(id).text=text;commands.emplace_back(UpdateText{id,text});
        }
        expected.apply(commands,expected.revision());check(expected.document()==complete,"Independent complete Document equals canonical UpdateText vector");
        check(same(w.host.session,expected),"Pointer Apply/Detach equals canonical full Document/native/History/preview/revision/generation");
        check(w.canvas->selections()==selected,"Atomic edit preserves whole-Text IDs/selection");check(decode(encode(complete))==complete,"Complete result native readback");
        history(w,"Undo");expected.undo(expected.revision());check(same(w.host.session,expected),"Production Undo equals complete canonical Session");
        history(w,"Redo");expected.redo(expected.revision());check(same(w.host.session,expected),"Production Redo equals complete canonical Session");cold(w,expected);
    }
    w.host.changed={};w.hide();events();
}
inline int run(){int cases=0,failures=0;
    for(const auto* action:{"attach","update","detach"})for(const auto* cause:{"valid","cancel","body","driver","style","object","attachment","path","path-label","composition",
        "document","session","revision","generation","preview","equivalent","unrelated"}){
        ++cases;try{context(action,cause);}catch(const std::exception& error){++failures;std::cerr<<"TEXT_PATH_RED "<<action<<" "<<cause<<": "<<error.what()<<std::endl;}
    }
    for(const auto* action:{"attach","update"})for(const auto* cause:{"rechoose","unselected-path"}){
        ++cases;try{context(action,cause);}catch(const std::exception& error){++failures;std::cerr<<"TEXT_PATH_RED "<<action<<" "<<cause<<": "<<error.what()<<std::endl;}
    }
    for(const auto* cause:{"path","cancel-stale-path"}){
        ++cases;try{context("update",cause,true);}catch(const std::exception& error){++failures;std::cerr<<"TEXT_PATH_RED common-source "<<cause<<": "<<error.what()<<std::endl;}
    }
    std::cout<<"TextPath Windows context cases="<<cases<<" checks="<<checks<<" failures="<<failures<<" physical OS input NOT_RUN"<<std::endl;
    if(failures)throw std::runtime_error("Text Path production Window context refusals failed");return checks;
}
}

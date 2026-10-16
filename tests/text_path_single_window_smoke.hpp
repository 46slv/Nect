#pragma once
#include <QJsonDocument>
#include <QStatusBar>

namespace text_path_single_window_smoke {
using text_path_batch_window_smoke::check;
using text_path_batch_window_smoke::events;
using text_path_batch_window_smoke::same;
inline Document fixture(bool attached){
    auto document=text_path_batch_window_smoke::native_fixture(false);
    auto& text=*document.objects.at("second").text;
    text.content_driver=TextContentDriver{{"source","","text.content"}};
    text.font_features={{"kern",1,"whole_text"}};text.additional_axis_values={{"wdth",87.5}};
    if(attached)text.path_attachment=TextPathAttachment{"path","contour","distance",1000,2,false};
    return document;
}
inline void context(const std::string& action,const std::string& cause,bool pending=false){
    QTemporaryDir scratch;check(scratch.isValid(),"Single Text Path owns scratch");auto document=fixture(action!="attach");
    check(decode(encode(document))==document,"Single Text fixture native-validates before GUI");
    Window w(scratch.filePath("recovery"));if(!pending)w.setAttribute(Qt::WA_DontShowOnScreen);w.resize(1400,900);
    w.host.session=Session(document);w.host.session_id="single-text-path-context";w.host.edited();w.show();if(pending)w.activateWindow();events();
    w.canvas->set_active_artboard("composition","artboard",false);w.canvas->set_selection("second");events();
    auto* properties=w.findChild<QDockWidget*>("properties");check(properties,"Single Text Properties dock exists");properties->show();properties->raise();events();
    auto* area=w.findChild<QScrollArea*>("inspector-scroll");auto* choice=w.findChild<QComboBox*>("text-path-contour");
    auto* start=w.findChild<QLineEdit*>("text-path-start");auto* spacing=w.findChild<QLineEdit*>("text-path-spacing");
    auto* button=w.findChild<QPushButton*>(action=="detach"?"text-path-detach":"text-path-apply");
    check(area&&choice&&start&&spacing&&button,"Existing single Path controls exist");
    const Session original=w.host.session;const auto index=choice->findData("path\ncontour");check(index>0,"Exact Path/Contour pair exists");
    choice->setCurrentIndex(index);start->setText("1100");spacing->setText("3.5");events();
    check(same(w.host.session,original),"Attachment draft is fully Session-neutral");
    QLineEdit* size=nullptr;
    if(pending){for(auto* input:w.findChildren<QLineEdit*>()){
        const auto ref=QJsonDocument::fromJson(input->property("nect-reference").toByteArray()).object();
        if(input->isVisible()&&ref.value("object").toString()=="second"&&ref.value("field").toString()=="text.font_size")size=input;
    }
        check(size,"Existing same-object Font size input exists");area->ensureWidgetVisible(size);events();
        QTest::mouseClick(w.windowHandle(),Qt::LeftButton,Qt::NoModifier,size->mapTo(&w,size->rect().center()));
        QTest::keyClick(size,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(size,"64");events();
        std::cout<<"Single-pending fixture focus="<<size->hasFocus()<<" modified="<<size->isModified()<<" neutral="<<same(w.host.session,original)<<std::endl;
        check(size->hasFocus()&&size->isModified()&&same(w.host.session,original),"Pending Font size64 is completely neutral before attachment press");
    }
    area->ensureWidgetVisible(button);events();const auto position=button->mapTo(&w,button->rect().center());
    check(w.childAt(position)==button,"Actual Window pointer hits single Path action");
    const bool replacement=cause!="valid";
    if(replacement){auto incoming=w.host.session.document();
        if(cause=="body")incoming.objects.at("second").text->content="Incoming Text body";
        else if(cause=="driver")incoming.objects.at("second").text->content_driver.reset();
        else if(cause=="style")incoming.objects.at("second").text->weight=600;
        else if(cause=="object")incoming.objects.at("second").name="Incoming whole target";
        else if(cause=="attachment")incoming.objects.at("second").text->path_attachment=TextPathAttachment{"path","contour","distance",1200,3,false};
        else if(cause=="path"||cause=="rechoose")incoming.objects.at("path").contours.front().points.front().y.literal=110;
        else if(cause=="composition"){incoming.compositions.back().roots=std::move(incoming.compositions.front().roots);incoming.compositions.front().roots.clear();}
        else if(cause=="unrelated")incoming.compositions.back().name="Independent incoming composition";
        else if(cause!="equivalent")throw std::runtime_error("Unrecognized single Text fixture cause");
        check((incoming==w.host.session.document())==(cause=="equivalent"),"Incoming single-Text replacement changes intended state");
        check(decode(encode(incoming))==incoming,"Incoming single-Text replacement native-validates");w.host.session=Session(incoming);
    }
    if(cause=="rechoose"){const Session before=w.host.session;choice->setCurrentIndex(0);choice->setCurrentIndex(index);events();
        check(same(w.host.session,before),"Explicit Path/Contour rechoose stays Session-neutral");}
    Session expected=w.host.session;const auto selected=w.canvas->selections();
    QTest::mouseClick(w.windowHandle(),Qt::LeftButton,Qt::NoModifier,position);events();
    const bool refusal=cause=="body"||cause=="driver"||cause=="style"||cause=="object"||cause=="attachment"||cause=="composition"||
        (cause=="path"&&action!="detach");
    std::cout<<"SingleTextPath "<<action<<" "<<cause<<" pending="<<pending<<" actual_revision="<<w.host.session.revision()<<" expected_before="<<expected.revision()
        <<" status="<<w.statusBar()->currentMessage().toStdString()<<std::endl;
    if(refusal){check(same(w.host.session,expected),"Stale single Text/Path/Composition refuses before consuming scalar/attachment intent");
        check(w.canvas->selections()==selected,"Refusal preserves whole Text selection");check(choice->currentData().toString()=="path\ncontour","Refusal retains exact source draft");
        check(w.statusBar()->currentMessage().contains(cause=="composition"?"SELECTION_CONFLICT":"PROPERTY_CONFLICT"),"Single Text conflict reports exact visible reason");
        if(pending)check(size->isModified()&&size->text()=="64","Refusal retains unfinished Font size");
    }else{
        auto complete=expected.document();if(pending){complete.objects.at("second").text->parameters.at("font_size").literal=64;
            expected.apply({EditProperties{{{"second","","text.font_size"}},64,false}},expected.revision());check(expected.document()==complete,"Independent Font size document equals canonical scalar command");}
        auto text=*complete.objects.at("second").text;
        if(action=="detach")text.path_attachment.reset();else text.path_attachment=TextPathAttachment{"path","contour","distance",1100,3.5,false};
        complete.objects.at("second").text=text;expected.apply({UpdateText{"second",text}},expected.revision());
        check(expected.document()==complete,"Independent complete single Text document equals canonical UpdateText");
        check(same(w.host.session,expected),"Actual first Path press equals full canonical scalar-plus-attachment Session");
        check(w.canvas->selections()==selected&&decode(encode(complete))==complete,"Source/style/Refs/IDs/selection/native preserved");
        text_path_batch_window_smoke::history(w,"Undo");expected.undo(expected.revision());check(same(w.host.session,expected),"Attachment Undo is independent of pending Font size");
        if(pending){text_path_batch_window_smoke::history(w,"Undo");expected.undo(expected.revision());check(same(w.host.session,expected),"Second Undo restores complete original Font size/source");}
        check(expected.document()==document||replacement,"Original source restored after separate Undo");
        if(pending){text_path_batch_window_smoke::history(w,"Redo");expected.redo(expected.revision());check(same(w.host.session,expected),"Font size Redo restores independent scalar");}
        text_path_batch_window_smoke::history(w,"Redo");expected.redo(expected.revision());check(same(w.host.session,expected),"Attachment Redo restores complete canonical result");text_path_batch_window_smoke::cold(w,expected);
    }
    w.host.changed={};w.hide();events();
}
inline int run(bool pending_only=false){int cases=0,failures=0;const auto before=text_path_batch_window_smoke::checks;
    if(!pending_only)for(const auto* action:{"attach","update","detach"})for(const auto* cause:{"valid","body","driver","style","object","attachment","path","composition","equivalent","unrelated"}){
        ++cases;try{context(action,cause);}catch(const std::exception& error){++failures;std::cerr<<"SINGLE_TEXT_PATH_RED "<<action<<" "<<cause<<": "<<error.what()<<std::endl;}
    }
    for(const auto* action:{"attach","detach"})for(const auto* cause:{"valid","body","path","rechoose"}){
        ++cases;try{context(action,cause,true);}catch(const std::exception& error){++failures;std::cerr<<"SINGLE_TEXT_PATH_RED pending "<<action<<" "<<cause<<": "<<error.what()<<std::endl;}
    }
    std::cout<<"SingleTextPath Windows cases="<<cases<<" checks="<<text_path_batch_window_smoke::checks-before<<" failures="<<failures<<" physical OS input NOT_RUN"<<std::endl;
    if(failures)throw std::runtime_error("Single Text Path Window context failed");return text_path_batch_window_smoke::checks-before;
}
}

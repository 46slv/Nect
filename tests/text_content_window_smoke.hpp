#pragma once
#include <QPlainTextEdit>

namespace text_content_window_smoke {
using text_typography_window_smoke::check;
using text_typography_window_smoke::events;
using text_typography_window_smoke::same;
using text_typography_window_smoke::point;
using text_typography_window_smoke::history;
using text_typography_window_smoke::cold;
using text_typography_window_smoke::checks;
inline const std::string edited="Changed 日本語\n  second\tline\xC2\xA0" "with space\nsoft\xE2\x80\xA8" "break";
inline Document fixture(bool linked,bool on_path=false){
    auto d=text_family_window_smoke::fixture(false);d.id="content-window-document";
    d.objects.at("path").contours.front().points.back().x.literal=6000;
    auto& t=*d.objects.at("text").text;t.content="Keep 日本語\r\n  raw\tbody\xC2\xA0space\xE2\x80\xA8soft";t.path_attachment->start=3000;
    t.content_driver.reset();t.path_attachment.reset();
    d.objects.at("source").text->content="Source 日本語\r\nexact\xC2\xA0" "body";
    d.objects.at("alternate").text->content="Alternate 日本語\n  chosen\tbody";
    if(linked)t.content_driver=TextContentDriver{{"source","","text.content"}};
    if(on_path){t.content="Path 日本語\t\xC2\xA0" "body";t.path_attachment=TextPathAttachment{"path","contour","distance",3000,2,false};
        d.objects.at("source").text->content="Source 日本語 single line";d.objects.at("alternate").text->content="Alternate 日本語 single line";}
    check(decode(encode(d))==d,"Complete authored multiline Text fixture native-validates");return d;
}
inline void context(const std::string& action,const std::string& phase,const std::string& cause,bool pending=false,bool on_path=false){
    const bool linked=action=="linked-edit"||action=="replace"||action=="unlink";
    const bool link=action=="link"||action=="replace";
    QTemporaryDir scratch;check(scratch.isValid(),"Content Window owns scratch");auto document=fixture(linked,on_path);
    const auto proposed=on_path?std::string("Changed 日本語 single line"):edited;
    if(cause=="chosen-lineage")document.objects.at("alternate").text->content_driver=TextContentDriver{{"source","","text.content"}};
    check(decode(encode(document))==document,"Complete content driver lineage fixture native-validates");
    const auto intent=[&](Session& s){
        std::vector<Command> commands;
        if(link)commands.push_back(LinkTextContent{{"text","","text.content"},{"alternate","","text.content"},linked});
        else if(action=="unlink")commands.push_back(UnlinkTextContent{{"text","","text.content"}});
        else{if(linked)commands.push_back(UnlinkTextContent{{"text","","text.content"}});
            auto t=*s.document().objects.at("text").text;t.content=proposed;t.content_driver.reset();commands.push_back(UpdateText{"text",t});}
        s.apply(commands,s.revision());
    };
    Session valid(document);if(pending)valid.apply({EditProperties{{{"text","","text.font_size"}},64,false}},valid.revision());intent(valid);
    check(decode(encode(valid.document()))==valid.document(),"Independent full authored content intent native-validates before GUI");
    QSettings settings(scratch.filePath("settings.ini"),QSettings::IniFormat);
    Window w(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(settings),&settings);
    if(!pending)w.setAttribute(Qt::WA_DontShowOnScreen);w.resize(1400,900);
    w.host.session=Session(document);w.host.session_id="content-window-session";w.host.edited();w.show();if(pending)w.activateWindow();events();
    w.canvas->set_active_artboard("composition","artboard",false);w.canvas->set_selection("text");events();
    auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Production Properties exists");dock->show();dock->raise();events();
    auto* area=w.findChild<QScrollArea*>("inspector-scroll");check(area,"Production Inspector exists");
    QPointer<QPushButton> edit=w.findChild<QPushButton*>("edit-text-content");
    QPointer<QToolButton> drive=w.findChild<QToolButton*>("text-content-driver");
    check(edit&&drive,"Existing content Edit/Driver controls exist");QPointer<QMenu> menu=drive->menu();QPointer<QLineEdit> scalar;
    Session expected=w.host.session;
    if(pending){for(auto* input:w.findChildren<QLineEdit*>()){
        const auto ref=QJsonDocument::fromJson(input->property("nect-reference").toByteArray()).object();
        if(input->isVisible()&&ref.value("object")=="text"&&ref.value("field")=="text.font_size")scalar=input;
    }
        check(scalar,"Pending Font size exists");area->ensureWidgetVisible(scalar);events();point(w,scalar);
        QTest::keyClick(scalar,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(scalar,"64");events();
        check(scalar->hasFocus()&&scalar->isModified()&&same(w.host.session,expected),"Pending scalar stays complete-Session-neutral");
    }
    const auto change=[&]{
        if(cause=="session")w.host.session_id+="-incoming";
        else if(cause=="revision")w.host.session.apply({Rename{"path","External edit"}},w.host.session.revision());
        else if(cause=="preview"||cause=="generation"){w.host.session.begin_gesture(w.host.session.revision());w.host.session.update_gesture({Rename{"path","Preview"}});if(cause=="generation")w.host.session.cancel_gesture();}
        else if(cause=="selection"||cause=="selection-return"){w.canvas->set_selection("path");if(cause=="selection-return")w.canvas->set_selection("text");events();}
        else if(cause=="scope"){w.canvas->set_active_artboard("other-composition","other-artboard",false);events();}
        else if(cause=="refresh"){w.host.edited();events();}
        else if(cause!="valid"&&cause!="cancel"&&cause!="untouched"){
            auto incoming=w.host.session.document();
            if(cause=="body")incoming.objects.at("text").text->content="Incoming authored\nraw body";
            else if(cause=="style")incoming.objects.at("text").text->family="Times New Roman";
            else if(cause=="object")incoming.objects.at("text").name="Incoming whole Object";
            else if(cause=="source-id")incoming.objects.at("text").text->id="replacement-source";
            else if(cause=="driver")incoming.objects.at("text").text->content_driver=TextContentDriver{{"alternate","","text.content"}};
            else if(cause=="composition"){incoming.compositions.back().roots=std::move(incoming.compositions.front().roots);incoming.compositions.front().roots.clear();}
            else if(cause=="document")incoming.id="incoming-document";
            else if(cause=="chosen-body"||cause=="rechoose")incoming.objects.at("alternate").text->content="Incoming chosen\nraw body";
            else if(cause=="chosen-style")incoming.objects.at("alternate").text->family="Times New Roman";
            else if(cause=="chosen-source-id")incoming.objects.at("alternate").text->id="replacement-chosen-source";
            else if(cause=="chosen-driver")incoming.objects.at("alternate").text->content_driver=TextContentDriver{{"source","","text.content"}};
            else if(cause=="chosen-lineage"||cause=="linked-body")incoming.objects.at("source").text->content="Incoming lineage\nraw body";
            else if(cause=="path")incoming.objects.at("path").contours.front().points.front().y.literal=103;
            else if(cause=="unrelated")incoming.compositions.back().name="Independent Composition";
            else if(cause=="unrelated-source")incoming.objects.at("source").text->family="Independent family";
            else if(cause!="equivalent")throw std::runtime_error("Unknown content fixture cause");
            check(decode(encode(incoming))==incoming,"Incoming full content fixture native-validates");
            const auto rev=w.host.session.revision();Session replacement(incoming);
            if(rev){check(pending&&rev==1,"Replacement retains one legitimate scalar revision");auto base=incoming;
                base.objects.at("text").text->parameters.at("font_size")=document.objects.at("text").text->parameters.at("font_size");
                replacement=Session(base);replacement.apply({EditProperties{{{"text","","text.font_size"}},64,false}},replacement.revision());}
            check(replacement.revision()==rev,"Replacement retains same revision");w.host.session=std::move(replacement);
        }
    };
    const bool stale=cause=="body"||cause=="style"||cause=="object"||cause=="source-id"||cause=="driver"||cause=="composition"||
        cause=="document"||cause=="session"||cause=="revision"||cause=="preview"||cause=="generation"||cause=="selection"||
        cause=="selection-return"||cause=="scope"||cause=="chosen-body"||cause=="chosen-style"||cause=="chosen-source-id"||
        cause=="chosen-driver"||cause=="chosen-lineage"||(cause=="linked-body"&&linked);
    if(phase=="entry")change();expected=w.host.session;
    if(pending&&!(stale&&phase=="entry"))expected.apply({EditProperties{{{"text","","text.font_size"}},64,false}},expected.revision());
    const auto applyIntent=[&]{
        auto complete=expected.document();auto& t=*complete.objects.at("text").text;
        if(link)t.content_driver=TextContentDriver{{"alternate","","text.content"}};
        else if(action=="unlink"){t.content=evaluate_text_content(complete,"text");t.content_driver.reset();}
        else{t.content=proposed;t.content_driver.reset();}
        intent(expected);check(expected.document()==complete,"Independent complete Document equals canonical content commands");
        check(same(w.host.session,expected),"Content Apply equals complete canonical Session");
    };
    const auto originalSelection=w.canvas->selections();const auto originalComposition=w.canvas->active_composition(),originalArtboard=w.canvas->active_artboard();
    QWidget* control=link||action=="unlink"?static_cast<QWidget*>(drive):static_cast<QWidget*>(edit);
    if(control)area->ensureWidgetVisible(control);events();bool opened=false,handled=false;std::exception_ptr error;
    QTimer chooseMenu(&w),dialogTimer(&w),deadline(&w);QElapsedTimer visible;chooseMenu.setInterval(20);dialogTimer.setInterval(20);
    QObject::connect(&chooseMenu,&QTimer::timeout,&w,[&]{
        if(!menu||!menu->isVisible())return;if(!opened){opened=true;visible.start();}if(visible.elapsed()<QApplication::doubleClickInterval()+20)return;chooseMenu.stop();
        try{if(action=="unlink"&&phase=="apply"){change();expected=w.host.session;}
            QTest::mouseClick(menu,Qt::LeftButton,Qt::NoModifier,menu->actionGeometry(menu->actions().at(action=="unlink"?1:0)).center());
            if(action=="unlink"){if(stale)check(same(w.host.session,expected),"Stale content Unlink preserves complete incoming Session");else applyIntent();handled=true;}
        }catch(...){error=std::current_exception();if(menu)menu->close();}
    });
    QObject::connect(&dialogTimer,&QTimer::timeout,&w,[&]{
        QPointer<QDialog> dialog=w.findChild<QDialog*>(link?"text-source-picker":"text-editor-dialog");if(!dialog||!dialog->isVisible())return;dialogTimer.stop();opened=true;
        try{
            check(same(w.host.session,expected),"Content entry commits only legitimate scalar");
            QPointer<QListWidget> list;QListWidgetItem* chosen=nullptr;
            const auto chooseSource=[&]{check(list&&chosen,"Exact chosen content Ref exists");list->scrollToItem(chosen);events();
                QTest::mouseClick(list->viewport(),Qt::LeftButton,Qt::NoModifier,list->visualItemRect(chosen).center());events();check(list->currentItem()==chosen,"Pointer chooses exact content source");};
            if(link){list=dialog->findChild<QListWidget*>("text-source-picker-list");check(list,"Existing content picker exists");
                for(int i=0;i<list->count();++i){auto* item=list->item(i);const auto ref=QJsonDocument::fromJson(item->data(Qt::UserRole).toByteArray()).object();
                    if(ref.value("object")=="alternate"&&ref.value("field")=="text.content")chosen=item;}
                chooseSource();if(linked){auto* replace=dialog->findChild<QCheckBox*>("text-content-replace");check(replace,"Explicit replacement exists");
                    const auto hit=replace->mapTo(dialog,QPoint(10,replace->height()/2));check(dialog->childAt(hit)==replace,"Pointer hits replacement indicator");
                    QTest::mouseClick(dialog->windowHandle(),Qt::LeftButton,Qt::NoModifier,hit);events();check(replace->isChecked(),"Explicit replacement pointer is checked");}
            }else{
                auto* editor=dialog->findChild<QPlainTextEdit*>("text-content-editor");check(editor,"Existing multiline staged editor exists");
                if(linked)point(*dialog,dialog->findChild<QPushButton*>("unlink-text-content-driver"));
                if(cause!="untouched")editor->setPlainText(QString::fromStdString(proposed));
                events();
            }
            check(same(w.host.session,expected),"Multiline draft/source choice is complete-Session-neutral");
            if(phase=="apply")change();expected=w.host.session;
            if(cause=="rechoose")chooseSource();
            const auto selection=w.canvas->selections();const auto composition=w.canvas->active_composition(),artboard=w.canvas->active_artboard();
            auto* buttons=dialog->findChild<QDialogButtonBox*>();check(buttons,"Explicit content Apply/Cancel exists");
            if(cause=="cancel"){point(*dialog,buttons->button(QDialogButtonBox::Cancel));check(same(w.host.session,expected),"Content Cancel preserves complete Session");}
            else{point(*dialog,buttons->button(QDialogButtonBox::Apply));
                if(stale&&phase=="apply"){check(same(w.host.session,expected),"Stale content Apply preserves complete incoming Session");
                    check(dialog&&dialog->isVisible(),"Refused content draft remains open");point(*dialog,buttons->button(QDialogButtonBox::Cancel));check(same(w.host.session,expected),"Cancel after refusal stays neutral");}
                else if(cause=="untouched"){check(same(w.host.session,expected),"Untouched raw authored multiline bytes remain exact");}
                else applyIntent();
            }
            const bool external=cause=="selection"||cause=="selection-return"||cause=="scope"||cause=="document"||cause=="session"||cause=="composition";
            check(w.canvas->selections()==(link&&!external?originalSelection:selection)&&w.canvas->active_composition()==(link&&!external?originalComposition:composition)&&
                w.canvas->active_artboard()==(link&&!external?originalArtboard:artboard),"Content completion retains exact owned or external selection/scope");
        }catch(...){error=std::current_exception();}if(dialog&&dialog->isVisible())dialog->reject();handled=true;
    });
    deadline.setSingleShot(true);QObject::connect(&deadline,&QTimer::timeout,&w,[&]{error=std::make_exception_ptr(std::runtime_error("Owned content menu/dialog timeout"));chooseMenu.stop();dialogTimer.stop();if(menu)menu->close();if(auto* d=qobject_cast<QDialog*>(QApplication::activeModalWidget()))d->reject();});
    deadline.start(10000);if(link||action=="unlink")chooseMenu.start();if(action!="unlink")dialogTimer.start();
    if(control)point(w,control);else check(stale&&same(w.host.session,expected),"Expired content control cannot retarget replacement");
    if(!(stale&&phase=="entry"))(void)QTest::qWaitFor([&]{return bool(error)||handled;},7000);
    deadline.stop();chooseMenu.stop();dialogTimer.stop();events();if(error)std::rethrow_exception(error);
    if(stale&&phase=="entry"){check(same(w.host.session,expected),"Stale content entry refuses before consuming pending scalar");check(!opened,"Stale content entry stays closed");}
    else check(opened&&handled,"First pointer reaches existing content action");
    if(!stale&&cause!="untouched"){
        if(cause!="cancel"){history(w,"Undo");expected.undo(expected.revision());check(same(w.host.session,expected),"Content Undo is separate from scalar");}
        if(pending){history(w,"Undo");expected.undo(expected.revision());check(same(w.host.session,expected),"Second Undo restores scalar");history(w,"Redo");expected.redo(expected.revision());check(same(w.host.session,expected),"Independent scalar Redo");}
        if(cause!="cancel"){history(w,"Redo");expected.redo(expected.revision());check(same(w.host.session,expected),"Content Redo retains complete state");}cold(w,expected);
    }
    w.host.changed={};w.hide();events();
}
inline int run(bool discovery,bool replace_only=false){
    int cases=0,failures=0;const auto test=[&](const std::string& action,const std::string& phase,const std::string& cause,bool pending=false,bool on_path=false){
        ++cases;std::cout<<"ContentCase "<<action<<" "<<phase<<" "<<cause<<" pending="<<pending<<std::endl;
        try{context(action,phase,cause,pending,on_path);}catch(const std::exception& e){++failures;std::cerr<<"CONTENT_WINDOW_RED "<<action<<" "<<phase<<" "<<cause<<" pending="<<pending<<": "<<e.what()<<std::endl;}
    };
    if(replace_only){test("replace","entry","body");test("replace","apply","body");test("replace","apply","valid");test("replace","apply","chosen-style");}
    else if(discovery){for(const auto* a:{"edit","linked-edit","link","replace","unlink"}){test(a,"entry","body");test(a,"apply","body");test(a,"apply","valid");}
        test("link","apply","chosen-body");test("replace","apply","chosen-style");test("edit","entry","body",true);test("link","entry","body",true);
    }else{
        for(const auto* a:{"edit","linked-edit","link","replace","unlink"}){
            for(const auto* c:{"body","composition"})test(a,"entry",c);
            for(const auto* c:{"valid","body","style","object","driver","source-id","composition","equivalent","path","unrelated"})test(a,"apply",c);
            test(a,"apply","valid",true);test(a,"entry","body",true);test(a,"apply","body",true);if(std::string(a)!="unlink")test(a,"apply","cancel",true);
        }
        for(const auto* a:{"link","replace"})for(const auto* c:{"chosen-body","chosen-style","chosen-source-id","chosen-driver","chosen-lineage","rechoose","unrelated-source"})test(a,"apply",c);
        for(const auto* a:{"edit","link"})for(const auto* c:{"document","session","revision","preview","generation","selection","selection-return","scope","refresh"})test(a,"apply",c);
        for(const auto* a:{"linked-edit","replace","unlink"})test(a,"apply","linked-body");
        test("edit","apply","untouched");
        for(const auto* a:{"edit","linked-edit","link","replace","unlink"})test(a,"apply","valid",false,true);
    }
    std::cout<<"ContentWindow cases="<<cases<<" checks="<<checks<<" failures="<<failures<<" physical OS input NOT_RUN"<<std::endl;return failures?1:0;
}
}

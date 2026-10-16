#pragma once
#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QElapsedTimer>
#include <QListWidget>
#include <QMenu>
#include <QToolButton>
#include <QTimer>
#include <QStyle>
#include <QStyleOptionComboBox>

namespace text_family_window_smoke {
inline bool popup_debug=false;
using text_typography_window_smoke::check;
using text_typography_window_smoke::events;
using text_typography_window_smoke::same;
using text_typography_window_smoke::native_fixture;
using text_typography_window_smoke::point;
using text_typography_window_smoke::history;
using text_typography_window_smoke::cold;
using text_typography_window_smoke::checks;
inline Document fixture(bool linked){
    auto d=native_fixture();d.id="family-window-document";
    d.objects.at("path").contours.front().points.back().x.literal=3000;
    d.objects.at("source").text->family="Times New Roman";
    Object alternate=d.objects.at("source");alternate.id="alternate";alternate.name="Explicit alternate source";
    alternate.text->id="alternate-source";alternate.text->family="Courier New";
    d.objects.emplace(alternate.id,alternate);d.compositions.front().roots.push_back(alternate.id);
    if(linked)d.objects.at("text").text->family_driver=TextFamilyDriver{{"source","","text.family"}};
    check(decode(encode(d))==d,"Family fixture validates before GUI");return d;
}
inline void context(const std::string& action,const std::string& phase,const std::string& cause,bool pending=false){
    const bool linked=action=="replace"||action=="unlink",literal=action=="literal",combo=action=="combo";
    QTemporaryDir scratch;check(scratch.isValid(),"Family Window owns scratch");const auto document=fixture(linked);
    Session validIntent(document);
    if(pending)validIntent.apply({EditProperties{{{"text","","text.font_size"}},64,false}},validIntent.revision());
    if(action=="link"||action=="replace")validIntent.apply({LinkTextFamily{{"text","","text.family"},{"alternate","","text.family"},linked}},validIntent.revision());
    else if(action=="unlink"||literal){auto t=*validIntent.document().objects.at("text").text;t.family="Authored custom family";t.family_driver.reset();
        if(action=="unlink")validIntent.apply({UnlinkTextFamily{{"text","","text.family"}},UpdateText{"text",t}},validIntent.revision());
        else validIntent.apply({UpdateText{"text",t}},validIntent.revision());}
    check(decode(encode(validIntent.document()))==validIntent.document(),"Complete typed family intent validates before GUI");
    QSettings settings(scratch.filePath("settings.ini"),QSettings::IniFormat);
    Window w(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(settings),&settings);
    if(!pending)w.setAttribute(Qt::WA_DontShowOnScreen);w.resize(1400,900);w.host.session=Session(document);
    w.host.session_id="family-window-session";w.host.edited();w.show();if(pending)w.activateWindow();events();
    w.canvas->set_active_artboard("composition","artboard",false);w.canvas->set_selection("text");events();
    auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Properties exists");dock->show();dock->raise();events();
    auto* area=w.findChild<QScrollArea*>("inspector-scroll");check(area,"Inspector exists");
    QPointer<QComboBox> family=w.findChild<QComboBox*>("text-family");
    QPointer<QToolButton> drive=w.findChild<QToolButton*>("text-family-driver");
    check(family&&drive,"Existing family controls exist");QPointer<QMenu> menu=drive->menu();
    if(combo)check(QTest::qWaitFor([&]{return family->completer()&&family->completer()->model()->rowCount()>0;},3000),
        "Existing family catalogue is available before pointer entry; no repertoire claim");
    QPointer<QLineEdit> size;Session expected=w.host.session;
    if(pending){for(auto* input:w.findChildren<QLineEdit*>()){
        const auto ref=QJsonDocument::fromJson(input->property("nect-reference").toByteArray()).object();
        if(input->isVisible()&&ref.value("object").toString()=="text"&&ref.value("field").toString()=="text.font_size")size=input;
    }
        check(size,"Font size exists");area->ensureWidgetVisible(size);events();point(w,size);
        QTest::keyClick(size,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(size,"64");events();
        check(size->hasFocus()&&size->isModified()&&same(w.host.session,expected),"Focused pending size is Session-neutral");
    }
    const auto change=[&]{
        if(cause=="session")w.host.session_id+="-incoming";
        else if(cause=="revision")w.host.session.apply({Rename{"path","External edit"}},w.host.session.revision());
        else if(cause=="preview"||cause=="generation"){
            w.host.session.begin_gesture(w.host.session.revision());w.host.session.update_gesture({Rename{"path","Preview"}});
            if(cause=="generation")w.host.session.cancel_gesture();
        }else if(cause=="selection"||cause=="selection-return"){
            w.canvas->set_selection("path");if(cause=="selection-return")w.canvas->set_selection("text");events();
        }else if(cause=="scope"){w.canvas->set_active_artboard("other-composition","other-artboard",false);events();}
        else if(cause=="refresh"){w.host.edited();events();}
        else if(cause!="valid"&&cause!="cancel"){
            auto incoming=w.host.session.document();
            if(cause=="body")incoming.objects.at("text").text->content="Incoming authored body";
            else if(cause=="style")incoming.objects.at("text").text->weight=600;
            else if(cause=="driver")incoming.objects.at("text").text->family_driver=TextFamilyDriver{{"alternate","","text.family"}};
            else if(cause=="source-id")incoming.objects.at("text").text->id="replacement-text-source";
            else if(cause=="attachment")incoming.objects.at("text").text->path_attachment->start=20;
            else if(cause=="composition"){incoming.compositions.back().roots=std::move(incoming.compositions.front().roots);incoming.compositions.front().roots.clear();}
            else if(cause=="document")incoming.id="incoming-document";
            else if(cause=="chosen-family")incoming.objects.at("alternate").text->family="Incoming family";
            else if(cause=="chosen-body")incoming.objects.at("alternate").text->content="Incoming chosen source";
            else if(cause=="linked-family")incoming.objects.at("source").text->family="Incoming linked family";
            else if(cause=="unrelated-source")incoming.objects.at("source").text->weight=900;
            else if(cause=="path")incoming.objects.at("path").contours.front().points.front().y.literal=103;
            else if(cause=="unrelated")incoming.compositions.back().name="Independent Composition";
            else if(cause!="equivalent")throw std::runtime_error("Unknown family fixture");
            check(decode(encode(incoming))==incoming,"Incoming family fixture validates before replacement");
            const auto oldRevision=w.host.session.revision();Session replacement(incoming);
            // A same-revision replacement must retain its own history; create the
            // pending-size transaction from the original native fixture instead.
            if(oldRevision){auto base=incoming;base.objects.at("text").text->parameters.at("font_size")=document.objects.at("text").text->parameters.at("font_size");
                replacement=Session(base);replacement.apply({EditProperties{{{"text","","text.font_size"}},64,false}},replacement.revision());}
            check(replacement.revision()==oldRevision,"Replacement uses the same revision");w.host.session=std::move(replacement);
        }
    };
    const bool stale=cause=="body"||cause=="style"||cause=="driver"||cause=="source-id"||cause=="attachment"||cause=="composition"||
        cause=="document"||cause=="session"||cause=="revision"||cause=="preview"||cause=="generation"||cause=="selection"||cause=="selection-return"||cause=="scope"||
        cause=="chosen-family"||cause=="chosen-body"||(cause=="linked-family"&&linked);
    if(phase=="entry")change();expected=w.host.session;
    auto complete=expected.document();
    if(pending&&!(phase=="entry"&&stale)){
        complete.objects.at("text").text->parameters.at("font_size").literal=64;
        expected.apply({EditProperties{{{"text","","text.font_size"}},64,false}},expected.revision());
        check(expected.document()==complete,"Independent full Document equals canonical size command");
    }
    const auto finish=[&](QDialog* dialog){
        if(phase=="apply")change();expected=w.host.session;complete=expected.document();
        const auto selectionBefore=w.canvas->selections();
        const auto compositionBefore=w.canvas->active_composition(),artboardBefore=w.canvas->active_artboard();
        if(cause=="cancel"){
            point(*dialog,dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Cancel));
            check(same(w.host.session,expected),"Explicit Cancel keeps complete authored Session");return;
        }
        auto* buttons=dialog->findChild<QDialogButtonBox*>();point(*dialog,buttons->button(QDialogButtonBox::Apply));
        if(phase=="apply"&&stale){
            check(same(w.host.session,expected),"Stale family Apply refuses complete incoming Session atomically");
            check(dialog->isVisible(),"Refused family draft remains open");
            point(*dialog,buttons->button(QDialogButtonBox::Cancel));check(same(w.host.session,expected),"Cancel after refusal is neutral");
            if(cause=="selection"||cause=="selection-return"||cause=="scope")
                check(w.canvas->selections()==selectionBefore&&w.canvas->active_composition()==compositionBefore&&w.canvas->active_artboard()==artboardBefore,
                    "Cancel after stale external selection/scope preserves that incoming context");
        }else{
            std::vector<Command> commands;
            if(action=="unlink"){
                complete.objects.at("text").text->family="Authored custom family";complete.objects.at("text").text->family_driver.reset();
                commands={UnlinkTextFamily{{"text","","text.family"}},UpdateText{"text",*complete.objects.at("text").text}};
            }else{
                complete.objects.at("text").text->family_driver=TextFamilyDriver{{"alternate","","text.family"}};
                commands={LinkTextFamily{{"text","","text.family"},{"alternate","","text.family"},linked}};
            }
            expected.apply(commands,expected.revision());check(expected.document()==complete,"Independent complete Document equals typed family commands");
            check(same(w.host.session,expected),"Family Apply preserves complete canonical Session");
        }
    };
    if(combo||literal){
        area->ensureWidgetVisible(family);events();
        if(combo){QStyleOptionComboBox option;option.initFrom(family);option.editable=family->isEditable();option.currentText=family->currentText();
            const auto arrowRect=family->style()->subControlRect(QStyle::CC_ComboBox,&option,QStyle::SC_ComboBoxArrow,family);
            const auto oldPoint=QPoint(family->width()-8,family->height()/2);
            if(popup_debug)std::cout<<"PopupTrace old-hit="<<family->style()->hitTestComplexControl(QStyle::CC_ComboBox,&option,oldPoint,family)
                <<" arrow="<<arrowRect.x()<<","<<arrowRect.y()<<","<<arrowRect.width()<<","<<arrowRect.height()
                <<" old-point="<<oldPoint.x()<<","<<oldPoint.y()<<" rows="<<family->model()->rowCount()<<std::endl;
            const auto arrow=family->mapTo(&w,arrowRect.center());check(w.childAt(arrow)==family,"Pointer hits actual styled family arrow");
            if(popup_debug){
                QObject::connect(family,QOverload<int>::of(&QComboBox::activated),&w,[&](int index){std::cout<<"PopupTrace activated="<<index<<" value="<<family->currentText().toStdString()<<std::endl;});
                QTest::mousePress(w.windowHandle(),Qt::LeftButton,Qt::NoModifier,arrow);
                std::cout<<"PopupTrace press exists="<<bool(family)<<" view="<<bool(family&&family->view()->isVisible())<<" revision="<<w.host.session.revision()
                    <<" rows="<<(family?family->count():-1)<<" size-alive="<<bool(size)<<" committed="<<(size?size->property("nect-text-family-committed-revision").toString().toStdString():"gone")<<std::endl;
                QTest::mouseRelease(w.windowHandle(),Qt::LeftButton,Qt::NoModifier,arrow);
                std::cout<<"PopupTrace release exists="<<bool(family)<<" view="<<bool(family&&family->view()->isVisible())<<" focus="<<(QApplication::focusWidget()?QApplication::focusWidget()->objectName().toStdString():"none")<<std::endl;
            }else QTest::mouseClick(w.windowHandle(),Qt::LeftButton,Qt::NoModifier,arrow);
            events();
            check(same(w.host.session,expected),"Family entry retains full incoming Session or size only");
            // Native Windows Qt animates the popup before its item view is visible.
            // Observe that one pointer attempt; never reopen it to manufacture PASS.
            const bool opened=stale?bool(family&&family->view()->isVisible()):
                QTest::qWaitFor([&]{return family&&family->view()->isVisible();},2000);
            if(opened)QTest::keyClick(family->view(),Qt::Key_Escape);events();
            std::cout<<"FamilyPopup "<<phase<<" "<<cause<<" pending="<<pending<<" opened="<<opened<<" exists="<<bool(family)<<" status="<<w.statusBar()->currentMessage().toStdString()<<std::endl;
            check(same(w.host.session,expected),"Popup Escape retains full Session and independent size");
            check(opened!=stale,"Family popup opens only for retained context");
        }else{
            QPointer<QLineEdit> editor=family->lineEdit();point(w,editor);
            check(family&&editor,"Family literal control survives ordinary size handoff");
            QTest::keyClick(editor,Qt::Key_A,Qt::ControlModifier);
            QTest::keyClicks(editor,"Authored custom family");events();
            check(same(w.host.session,expected),"Literal family draft remains neutral");if(phase=="apply")change();expected=w.host.session;complete=expected.document();
            QTest::keyClick(family->lineEdit(),Qt::Key_Return);events();
            if(stale)check(same(w.host.session,expected),"Stale literal family Apply refuses complete incoming Session");
            else{complete.objects.at("text").text->family="Authored custom family";expected.apply({UpdateText{"text",*complete.objects.at("text").text}},expected.revision());
                check(expected.document()==complete&&same(w.host.session,expected),"Literal family equals independent complete Document and UpdateText");}
        }
    }else{
        area->ensureWidgetVisible(drive);events();bool menuOpen=false,dialogOpen=false;std::exception_ptr callbackError;
        QTimer menuTimer(&w),dialogTimer(&w),deadline(&w);QElapsedTimer menuVisible;bool menuChosen=false,dialogHandled=false;
        menuTimer.setInterval(20);dialogTimer.setInterval(20);
        QObject::connect(&menuTimer,&QTimer::timeout,&w,[&]{
            if(!menu||!menu->isVisible())return;
            menuOpen=true;if(!menuVisible.isValid()){menuVisible.start();return;}
            if(menuVisible.elapsed()<QApplication::doubleClickInterval()+20)return;
            menuTimer.stop();
            try{QTest::mouseClick(menu,Qt::LeftButton,Qt::NoModifier,menu->actionGeometry(menu->actions().at(action=="unlink"?1:0)).center());menuChosen=true;}
            catch(...){callbackError=std::current_exception();if(menu)menu->close();}
        });
        if(action=="unlink")QObject::connect(&dialogTimer,&QTimer::timeout,&w,[&]{
            auto* dialog=w.findChild<QDialog*>("text-family-dialog");dialogOpen=dialog&&dialog->isVisible();if(!dialogOpen)return;
            dialogTimer.stop();
            try{check(same(w.host.session,expected),"Driver entry commits only legitimate size");
                point(*dialog,dialog->findChild<QCheckBox*>("unlink-text-family-driver"));
                auto* editor=dialog->findChild<QComboBox*>("text-family-editor")->lineEdit();point(*dialog,editor);
                QTest::keyClick(editor,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(editor,"Authored custom family");events();finish(dialog);
            }catch(...){callbackError=std::current_exception();}if(dialog->isVisible())dialog->reject();dialogHandled=true;
        });
        deadline.setSingleShot(true);QObject::connect(&deadline,&QTimer::timeout,&w,[&]{
            callbackError=std::make_exception_ptr(std::runtime_error("Owned family menu/dialog observation timed out"));
            menuTimer.stop();dialogTimer.stop();if(menu)menu->close();
            if(auto* dialog=w.findChild<QDialog*>("text-family-dialog"))dialog->reject();
        });
        deadline.start(10000);menuTimer.start();if(action=="unlink")dialogTimer.start();
        point(w,drive);
        if(!(phase=="entry"&&stale))(void)QTest::qWaitFor([&]{return bool(callbackError)||(action=="unlink"?dialogHandled:menuChosen);},7000);
        deadline.stop();menuTimer.stop();dialogTimer.stop();events();
        std::cout<<"FamilyPointer "<<action<<" "<<phase<<" "<<cause<<" pending="<<pending<<" menu="<<menuOpen<<" chosen="<<menuChosen<<" dialog="<<dialogOpen
            <<" status="<<w.statusBar()->currentMessage().toStdString()<<std::endl;
        if(callbackError)std::rethrow_exception(callbackError);
        check(same(w.host.session,expected),"Driver entry/Apply retains complete canonical Session");
        if(phase=="entry"&&stale){check(!menuOpen,"Stale entry never opens writable family menu");}
        else if(action=="unlink")check(menuOpen&&dialogOpen,"Actual driver opens owned unlink dialog");
        else{
            QPointer<QDialog> dialog=w.findChild<QDialog*>("text-source-picker");check(menuOpen&&dialog&&dialog->isVisible(),"Actual driver opens owned source chooser");
            auto* list=dialog->findChild<QListWidget*>("text-source-picker-list");QListWidgetItem* item=nullptr;
            for(int i=0;i<list->count();++i){const auto ref=QJsonDocument::fromJson(list->item(i)->data(Qt::UserRole).toByteArray()).object();if(ref.value("object")=="alternate"&&ref.value("field")=="text.family")item=list->item(i);}
            check(item&&!item->isHidden(),"Explicit exact source Ref is available");
            QTest::mouseClick(dialog->windowHandle(),Qt::LeftButton,Qt::NoModifier,list->viewport()->mapTo(dialog,list->visualItemRect(item).center()));events();
            check(same(w.host.session,expected),"Chosen source preview is authored-state neutral");finish(dialog);
            if(dialog&&dialog->isVisible())dialog->reject();events();
        }
    }
    if(!(stale&&phase=="entry")&&!(stale&&phase=="apply")&&!combo){
        if(cause!="cancel"){history(w,"Undo");expected.undo(expected.revision());check(same(w.host.session,expected),"Family Undo is separate from size");}
        if(pending){history(w,"Undo");expected.undo(expected.revision());check(same(w.host.session,expected),"Second Undo restores ordinary size");history(w,"Redo");expected.redo(expected.revision());check(same(w.host.session,expected),"Size Redo is independent");}
        if(cause!="cancel"){history(w,"Redo");expected.redo(expected.revision());check(same(w.host.session,expected),"Family Redo retains full authored source");}cold(w,expected);
    }
    w.host.changed={};w.hide();events();
}
inline int run(bool discovery=false,bool pending_debug=false,bool popup_only=false){int cases=0,failures=0;popup_debug=popup_only;
    const auto test=[&](const std::string& action,const std::string& phase,const std::string& cause,bool pending=false){++cases;
        try{context(action,phase,cause,pending);}catch(const std::exception& e){++failures;std::cerr<<"FAMILY_WINDOW_RED "<<action<<" "<<phase<<" "<<cause<<" pending="<<pending<<": "<<e.what()<<std::endl;}};
    if(popup_only)test("combo","entry","valid",true);
    else if(pending_debug){for(const auto* action:{"link","replace","unlink","literal"})test(action,"apply","valid",true);test("combo","entry","valid",true);}
    else if(discovery){for(const auto* action:{"combo","link","replace","unlink"}){test(action,"entry","body");test(action,action==std::string("combo")?"entry":"apply",action==std::string("combo")?"style":"body");}
        test("link","apply","chosen-family");test("replace","apply","chosen-body");test("literal","apply","body");test("link","entry","body",true);
    }else{
        for(const auto* action:{"link","replace","unlink"}){
            for(const auto* cause:{"body","composition"})test(action,"entry",cause);
            for(const auto* cause:{"valid","body","style","driver","source-id","attachment","composition","equivalent","path","unrelated","refresh","cancel"})test(action,"apply",cause);
            test(action,"apply","valid",true);test(action,"entry","body",true);test(action,"apply","body",true);test(action,"apply","cancel",true);
        }
        for(const auto* action:{"link","replace"})for(const auto* cause:{"chosen-family","chosen-body","unrelated-source"})test(action,"apply",cause);
        for(const auto* cause:{"document","session","revision","preview","generation","selection","selection-return","scope"}){test("link","apply",cause);test("unlink","apply",cause);}
        for(const auto* cause:{"valid","body","style","attachment","composition","equivalent","path","unrelated"}){test("combo","entry",cause);test("literal","apply",cause);}
        test("unlink","apply","linked-family");test("replace","apply","linked-family");
        test("literal","apply","valid",true);test("literal","apply","body",true);test("combo","entry","valid",true);
    }
    std::cout<<"FamilyWindow cases="<<cases<<" checks="<<checks<<" failures="<<failures<<" physical OS input NOT_RUN"<<std::endl;return failures?1:0;
}
}

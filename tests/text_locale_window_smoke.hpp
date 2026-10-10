#pragma once

namespace text_locale_window_smoke {
using text_typography_window_smoke::check;
using text_typography_window_smoke::events;
using text_typography_window_smoke::same;
using text_typography_window_smoke::point;
using text_typography_window_smoke::history;
using text_typography_window_smoke::cold;
using text_typography_window_smoke::checks;
inline Document fixture(bool linked){
    auto d=text_family_window_smoke::fixture(false);d.id="locale-window-document";
    // Exact language tags use a native-valid horizontal/auto Text-on-Path fixture.
    d.objects.at("source").text->locale="en-GB";d.objects.at("alternate").text->locale="zh-Hant-TW";
    d.objects.at("text").text->path_attachment->start=1500;
    if(linked)d.objects.at("text").text->locale_driver=TextLocaleDriver{{"source","","text.locale"}};
    check(decode(encode(d))==d,"Complete locale fixture native-validates");return d;
}
inline void context(const std::string& action,const std::string& phase,const std::string& cause,const std::string& pending={}){
    const bool linked=action=="replace"||action=="unlink"||action=="linked-edit";
    const bool link=action=="link"||action=="replace";
    QTemporaryDir scratch;check(scratch.isValid(),"Locale Window owns scratch");auto document=fixture(linked);
    if(cause=="chosen-lineage")document.objects.at("alternate").text->locale_driver=TextLocaleDriver{{"source","","text.locale"}};
    check(decode(encode(document))==document,"Chosen locale dependency fixture native-validates");
    Session validIntent(document);
    if(!pending.empty())validIntent.apply({EditProperties{{{"text","","text."+pending}},64,false}},validIntent.revision());
    if(link)validIntent.apply({LinkTextLocale{{"text","","text.locale"},{"alternate","","text.locale"},linked}},validIntent.revision());
    else if(action=="unlink")validIntent.apply({UnlinkTextLocale{{"text","","text.locale"}}},validIntent.revision());
    else{std::vector<Command> commands;if(linked)commands.push_back(UnlinkTextLocale{{"text","","text.locale"}});
        auto t=*validIntent.document().objects.at("text").text;t.locale=linked?"ja-JP":"fr-CA";t.locale_driver.reset();commands.push_back(UpdateText{"text",t});validIntent.apply(commands,validIntent.revision());}
    check(decode(encode(validIntent.document()))==validIntent.document(),"Complete intended locale commands native-validate before GUI");
    QSettings settings(scratch.filePath("settings.ini"),QSettings::IniFormat);
    Window w(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(settings),&settings);
    if(pending.empty())w.setAttribute(Qt::WA_DontShowOnScreen);w.resize(1400,900);
    w.host.session=Session(document);w.host.session_id="locale-window-session";w.host.edited();w.show();if(!pending.empty())w.activateWindow();events();
    w.canvas->set_active_artboard("composition","artboard",false);w.canvas->set_selection("text");events();
    auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Production Properties exists");dock->show();dock->raise();events();
    auto* area=w.findChild<QScrollArea*>("inspector-scroll");check(area,"Production Inspector exists");
    QPointer<QToolButton> drive=w.findChild<QToolButton*>("text-locale-driver");check(drive,"Existing locale Driver exists");
    QPointer<QLineEdit> display=w.findChild<QLineEdit*>("text-locale");check(display&&display->isReadOnly(),"Existing locale display stays read-only");
    QPointer<QMenu> menu=drive->menu();QPointer<QLineEdit> scalar;Session expected=w.host.session;
    QObject observations;
    if(!pending.empty()){
        for(auto* input:w.findChildren<QLineEdit*>()){
            const auto ref=QJsonDocument::fromJson(input->property("nect-reference").toByteArray()).object();
            if(input->isVisible()&&ref.value("object")=="text"&&ref.value("field")==QString::fromStdString("text."+pending))scalar=input;
        }
        check(scalar,"Requested pending locale scalar exists");area->ensureWidgetVisible(scalar);events();point(w,scalar);
        QTest::keyClick(scalar,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(scalar,"64");events();
        check(scalar->hasFocus()&&scalar->isModified()&&same(w.host.session,expected),"Focused locale draft is fully neutral");
        QObject::connect(qApp,&QApplication::focusChanged,&observations,[](QWidget* old,QWidget* next){
            std::cout<<"LocaleFocus "<<(old?old->metaObject()->className():"none")<<"/"<<(old?old->objectName().toStdString():"")
                <<" -> "<<(next?next->metaObject()->className():"none")<<"/"<<(next?next->objectName().toStdString():"")<<std::endl;
        });
        QObject::connect(menu,&QMenu::aboutToShow,&observations,[&]{std::cout<<"LocaleMenuShow rev="<<w.host.session.revision()
            <<" focus="<<(QApplication::focusWidget()?QApplication::focusWidget()->metaObject()->className():"none")<<std::endl;});
        QObject::connect(menu,&QMenu::aboutToHide,&observations,[&]{std::cout<<"LocaleMenuHide rev="<<w.host.session.revision()
            <<" focus="<<(QApplication::focusWidget()?QApplication::focusWidget()->metaObject()->className():"none")<<std::endl;});
        QObject::connect(drive,&QObject::destroyed,&observations,[]{std::cout<<"LocaleDriverDestroyed"<<std::endl;});
    }
    const auto change=[&]{
        if(cause=="session")w.host.session_id+="-incoming";
        else if(cause=="revision")w.host.session.apply({Rename{"path","External edit"}},w.host.session.revision());
        else if(cause=="preview"||cause=="generation"){w.host.session.begin_gesture(w.host.session.revision());w.host.session.update_gesture({Rename{"path","Preview"}});if(cause=="generation")w.host.session.cancel_gesture();}
        else if(cause=="selection"||cause=="selection-return"){w.canvas->set_selection("path");if(cause=="selection-return")w.canvas->set_selection("text");events();}
        else if(cause=="scope"){w.canvas->set_active_artboard("other-composition","other-artboard",false);events();}
        else if(cause=="refresh"){w.host.edited();events();}
        else if(cause!="valid"&&cause!="cancel"){
            auto incoming=w.host.session.document();
            if(cause=="body")incoming.objects.at("text").text->content="Incoming authored body";
            else if(cause=="style")incoming.objects.at("text").text->family="Times New Roman";
            else if(cause=="locale")incoming.objects.at("text").text->locale="en-GB";
            else if(cause=="driver")incoming.objects.at("text").text->locale_driver=TextLocaleDriver{{"alternate","","text.locale"}};
            else if(cause=="source-id")incoming.objects.at("text").text->id="replacement-text-source";
            else if(cause=="composition"){incoming.compositions.back().roots=std::move(incoming.compositions.front().roots);incoming.compositions.front().roots.clear();}
            else if(cause=="document")incoming.id="incoming-document";
            else if(cause=="chosen-locale")incoming.objects.at("alternate").text->locale="ja-JP";
            else if(cause=="chosen-body"||cause=="rechoose")incoming.objects.at("alternate").text->content="Incoming chosen Text";
            else if(cause=="chosen-source-id")incoming.objects.at("alternate").text->id="replacement-chosen-source";
            else if(cause=="chosen-driver")incoming.objects.at("alternate").text->locale_driver=TextLocaleDriver{{"source","","text.locale"}};
            else if(cause=="chosen-lineage")incoming.objects.at("source").text->locale="ja-JP";
            else if(cause=="linked-locale")incoming.objects.at("source").text->locale="zh-Hant-TW";
            else if(cause=="unrelated-source")incoming.objects.at("source").text->family="Independent family";
            else if(cause=="path")incoming.objects.at("path").contours.front().points.front().y.literal=103;
            else if(cause=="unrelated")incoming.compositions.back().name="Independent Composition";
            else if(cause!="equivalent")throw std::runtime_error("Unknown locale fixture cause");
            check(decode(encode(incoming))==incoming,"Incoming locale fixture native-validates");
            const auto rev=w.host.session.revision();Session replacement(incoming);
            if(rev){check(!pending.empty()&&rev==1,"Replacement retains one legitimate scalar revision");auto base=incoming;
                base.objects.at("text").text->parameters.at(pending)=document.objects.at("text").text->parameters.at(pending);
                replacement=Session(base);replacement.apply({EditProperties{{{"text","","text."+pending}},64,false}},replacement.revision());}
            check(replacement.revision()==rev,"Replacement retains same revision");w.host.session=std::move(replacement);
        }
    };
    const bool stale=cause=="body"||cause=="style"||cause=="locale"||cause=="driver"||cause=="source-id"||cause=="composition"||
        cause=="document"||cause=="session"||cause=="revision"||cause=="preview"||cause=="generation"||cause=="selection"||cause=="selection-return"||cause=="scope"||
        cause=="chosen-locale"||cause=="chosen-body"||cause=="chosen-source-id"||cause=="chosen-driver"||cause=="chosen-lineage"||(cause=="linked-locale"&&linked);
    if(phase=="entry")change();expected=w.host.session;auto complete=expected.document();
    if(!pending.empty()&&!(phase=="entry"&&stale)){
        complete.objects.at("text").text->parameters.at(pending).literal=64;
        expected.apply({EditProperties{{{"text","","text."+pending}},64,false}},expected.revision());
        check(expected.document()==complete,"Independent complete Document equals canonical scalar command");
    }
    const auto applyIntent=[&]{
        std::vector<Command> commands;
        if(link){complete.objects.at("text").text->locale_driver=TextLocaleDriver{{"alternate","","text.locale"}};commands={LinkTextLocale{{"text","","text.locale"},{"alternate","","text.locale"},linked}};}
        else if(action=="unlink"){complete.objects.at("text").text->locale=text_locale_property(complete,{"text","","text.locale"}).evaluated;complete.objects.at("text").text->locale_driver.reset();commands={UnlinkTextLocale{{"text","","text.locale"}}};}
        else{if(linked)commands.push_back(UnlinkTextLocale{{"text","","text.locale"}});auto& t=*complete.objects.at("text").text;t.locale=linked?"ja-JP":"fr-CA";t.locale_driver.reset();commands.push_back(UpdateText{"text",t});}
        expected.apply(commands,expected.revision());check(expected.document()==complete,"Independent complete Document equals canonical locale command");
        check(same(w.host.session,expected),"Locale Apply equals complete canonical Session");
    };
    const auto originalSelection=w.canvas->selections();const auto originalComposition=w.canvas->active_composition(),originalArtboard=w.canvas->active_artboard();
    area->ensureWidgetVisible(drive);events();bool menuOpen=false,handled=false;std::exception_ptr error;
    QTimer chooseMenu(&w),dialogTimer(&w),deadline(&w);QElapsedTimer visible;chooseMenu.setInterval(20);dialogTimer.setInterval(20);
    QObject::connect(&chooseMenu,&QTimer::timeout,&w,[&]{
        if(!menu||!menu->isVisible())return;if(!menuOpen){menuOpen=true;visible.start();}if(visible.elapsed()<QApplication::doubleClickInterval()+20)return;chooseMenu.stop();
        try{if(action=="unlink"&&phase=="apply")change();if(action=="unlink"){expected=w.host.session;complete=expected.document();}
            QTest::mouseClick(menu,Qt::LeftButton,Qt::NoModifier,menu->actionGeometry(menu->actions().at(action=="unlink"?2:link?1:0)).center());
            if(action=="unlink"){if(stale)check(same(w.host.session,expected),"Stale locale Unlink refuses complete incoming Session");else applyIntent();handled=true;}
        }catch(...){error=std::current_exception();if(menu)menu->close();}
    });
    QObject::connect(&dialogTimer,&QTimer::timeout,&w,[&]{
        QPointer<QDialog> dialog=w.findChild<QDialog*>(link?"text-source-picker":"text-locale-dialog");if(!dialog||!dialog->isVisible())return;dialogTimer.stop();
        try{
            check(same(w.host.session,expected),"Locale dialog entry commits only legitimate scalar");
            QPointer<QListWidget> list;QListWidgetItem* chosen=nullptr;
            const auto chooseSource=[&]{
                check(list&&chosen,"Exact authored locale source exists");list->scrollToItem(chosen);events();
                QTest::mouseClick(list->viewport(),Qt::LeftButton,Qt::NoModifier,list->visualItemRect(chosen).center());events();
                check(list->currentItem()==chosen,"First source pointer chooses exact stable Ref");
            };
            if(link){
                list=dialog->findChild<QListWidget*>("text-source-picker-list");
                check(list,"Existing searchable locale picker exists");
                for(int i=0;i<list->count();++i){auto* item=list->item(i);
                    const auto ref=QJsonDocument::fromJson(item->data(Qt::UserRole).toByteArray()).object();
                    if(ref.value("object")=="alternate"&&ref.value("field")=="text.locale")chosen=item;
                }
                chooseSource();
            }else{
                if(action=="linked-edit")point(*dialog,dialog->findChild<QCheckBox*>("unlink-text-locale-driver"));
                auto* editor=dialog->findChild<QLineEdit*>("text-locale-editor");
                check(editor&&editor->isEnabled(),"Existing staged locale editor is enabled");point(*dialog,editor);
                QTest::keyClick(editor,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(editor,linked?"ja-JP":"fr-CA");events();
            }
            check(same(w.host.session,expected),"Locale/source choice remains fully neutral");
            if(phase=="apply")change();expected=w.host.session;complete=expected.document();
            if(cause=="rechoose"){chooseSource();check(same(w.host.session,expected),"Explicit same source rechoose remains Session-neutral");}
            const auto selection=w.canvas->selections();const auto composition=w.canvas->active_composition(),artboard=w.canvas->active_artboard();
            auto* buttons=dialog->findChild<QDialogButtonBox*>();check(buttons,"Explicit locale Apply/Cancel exists");
            if(cause=="cancel"){point(*dialog,buttons->button(QDialogButtonBox::Cancel));check(same(w.host.session,expected),"Locale Cancel retains full Session");}
            else{point(*dialog,buttons->button(QDialogButtonBox::Apply));
                if(stale&&phase=="apply"){check(same(w.host.session,expected),"Stale locale Apply refuses complete incoming Session");check(dialog&&dialog->isVisible(),"Refused locale/source draft remains open");point(*dialog,buttons->button(QDialogButtonBox::Cancel));check(same(w.host.session,expected),"Cancel after locale refusal stays neutral");}
                else applyIntent();}
            const bool external=cause=="selection"||cause=="selection-return"||cause=="scope"||cause=="document"||cause=="session"||cause=="composition";
            check(w.canvas->selections()==(link&&!external?originalSelection:selection)&&
                w.canvas->active_composition()==(link&&!external?originalComposition:composition)&&
                w.canvas->active_artboard()==(link&&!external?originalArtboard:artboard),"Locale completion preserves exact owned or external selection/scope");
        }catch(...){error=std::current_exception();}if(dialog&&dialog->isVisible())dialog->reject();handled=true;
    });
    deadline.setSingleShot(true);QObject::connect(&deadline,&QTimer::timeout,&w,[&]{error=std::make_exception_ptr(std::runtime_error("Owned locale menu/dialog timeout"));chooseMenu.stop();dialogTimer.stop();if(menu)menu->close();if(auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget()))dialog->reject();});
    deadline.start(10000);chooseMenu.start();if(action!="unlink")dialogTimer.start();
    if(drive)point(w,drive);else check(stale&&same(w.host.session,expected),"Expired original locale control cannot retarget replacement");
    if(!(stale&&phase=="entry"))(void)QTest::qWaitFor([&]{return bool(error)||handled;},7000);
    deadline.stop();chooseMenu.stop();dialogTimer.stop();events();if(error)std::rethrow_exception(error);
    if(!pending.empty())std::cout<<"LocalePointerTrace action="<<action<<" menu="<<menuOpen<<" handled="<<handled
        <<" actual-revision="<<w.host.session.revision()<<" expected-revision="<<expected.revision()
        <<" scalar-alive="<<bool(scalar)<<" scalar-focus="<<bool(scalar&&scalar->hasFocus())
        <<" focus="<<(QApplication::focusWidget()?QApplication::focusWidget()->objectName().toStdString():"none")
        <<" status="<<w.statusBar()->currentMessage().toStdString()<<std::endl;
    if(stale&&phase=="entry"){check(same(w.host.session,expected),"Stale locale entry refuses before consuming pending scalar");check(!menuOpen,"Stale locale entry keeps menu closed");}
    else check(menuOpen&&handled,"First driver pointer reaches existing locale action");
    if(!stale){if(cause!="cancel"){history(w,"Undo");expected.undo(expected.revision());check(same(w.host.session,expected),"Locale Undo is separate from scalar");}
        if(!pending.empty()){history(w,"Undo");expected.undo(expected.revision());check(same(w.host.session,expected),"Second Undo restores scalar");history(w,"Redo");expected.redo(expected.revision());check(same(w.host.session,expected),"Scalar Redo is independent");}
        if(cause!="cancel"){history(w,"Redo");expected.redo(expected.revision());check(same(w.host.session,expected),"Locale Redo retains full authored state");}cold(w,expected);}
    w.host.changed={};w.hide();events();
}
inline int run(bool discovery,bool rechoose_only=false,bool pending_debug=false){
    int cases=0,failures=0;const auto test=[&](const std::string& action,const std::string& phase,const std::string& cause,const std::string& pending={}){
        ++cases;std::cout<<"LocaleCase "<<action<<" "<<phase<<" "<<cause<<" pending="<<pending<<std::endl;
        try{context(action,phase,cause,pending);}catch(const std::exception& e){++failures;std::cerr<<"LOCALE_WINDOW_RED "<<action<<" "<<phase<<" "<<cause<<" pending="<<pending<<": "<<e.what()<<std::endl;}
    };
    if(pending_debug){test("edit","apply","valid","font_size");test("link","apply","valid","font_size");test("link","apply","composition");test("replace","apply","composition");}
    else if(rechoose_only){for(const auto* action:{"link","replace"})test(action,"apply","rechoose");}
    else if(discovery){for(const auto* action:{"edit","linked-edit","link","replace","unlink"}){test(action,"entry","body");test(action,"apply","body");test(action,"apply","valid");}
        test("link","apply","chosen-locale");test("replace","apply","chosen-body");test("edit","entry","body","font_size");test("link","entry","body","font_size");}
    else{for(const auto* action:{"edit","linked-edit","link","replace","unlink"}){
        for(const auto* cause:{"body","composition"})test(action,"entry",cause);
        for(const auto* cause:{"valid","body","style","locale","driver","source-id","composition","equivalent","path","unrelated"})test(action,"apply",cause);
        test(action,"apply","valid","font_size");test(action,"entry","body","font_size");test(action,"apply","body","font_size");if(std::string(action)!="unlink")test(action,"apply","cancel","font_size");
    }
        for(const auto* action:{"link","replace"})for(const auto* cause:{"chosen-locale","chosen-body","chosen-source-id","chosen-driver","chosen-lineage","rechoose","unrelated-source"})test(action,"apply",cause);
        for(const auto* action:{"edit","link"})for(const auto* cause:{"document","session","revision","preview","generation","selection","selection-return","scope","refresh"})test(action,"apply",cause);
        for(const auto* action:{"linked-edit","replace","unlink"})test(action,"apply","linked-locale");
    }
    std::cout<<"LocaleWindow cases="<<cases<<" checks="<<checks<<" failures="<<failures<<" physical OS input NOT_RUN"<<std::endl;return failures?1:0;
}
}

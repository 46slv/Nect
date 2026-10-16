#pragma once

namespace text_layout_window_smoke {
using text_typography_window_smoke::check;
using text_typography_window_smoke::events;
using text_typography_window_smoke::same;
using text_typography_window_smoke::point;
using text_typography_window_smoke::history;
using text_typography_window_smoke::cold;
using text_typography_window_smoke::checks;
inline Document fixture(bool linked){
    auto d=text_family_window_smoke::fixture(false);d.id="layout-window-document";
    // Fixed-frame sizing is incompatible with Text-on-Path. Keep the rest of
    // the complete authored fixture, including its independent Path and Refs.
    d.objects.at("text").text->path_attachment.reset();
    d.objects.at("source").text->layout="frame";d.objects.at("alternate").text->layout="frame";
    if(linked)d.objects.at("text").text->layout_driver=TextLayoutDriver{{"source","","text.layout"}};
    check(decode(encode(d))==d,"Complete sizing fixture native-validates");return d;
}
inline void choose(QWidget& dialog,QComboBox* combo,int index){
    check(combo&&combo->isEnabled()&&index>=0,"Exact sizing combo choice exists");point(dialog,combo);
    QTest::qWait(QApplication::doubleClickInterval()+20);auto* view=combo->view();
    const auto item=combo->model()->index(index,0);view->scrollTo(item);events();
    QTest::mouseClick(view->viewport(),Qt::LeftButton,Qt::NoModifier,view->visualRect(item).center());events();
    check(combo->currentIndex()==index,"First combo pointer chooses exact sizing option");
}
inline void context(const std::string& action,const std::string& phase,const std::string& cause,const std::string& pending={}){
    const bool linked=action=="replace"||action=="unlink"||action=="linked-edit";
    const bool link=action=="link"||action=="replace";
    QTemporaryDir scratch;check(scratch.isValid(),"Sizing Window owns scratch");auto document=fixture(linked);
    if(cause=="chosen-lineage")document.objects.at("alternate").text->layout_driver=TextLayoutDriver{{"source","","text.layout"}};
    check(decode(encode(document))==document,"Chosen sizing dependency fixture native-validates");
    Session validIntent(document);
    if(!pending.empty())validIntent.apply({EditProperties{{{"text","","text."+pending}},64,false}},validIntent.revision());
    if(link)validIntent.apply({LinkTextLayout{{"text","","text.layout"},{"alternate","","text.layout"},linked}},validIntent.revision());
    else if(action=="unlink")validIntent.apply({UnlinkTextLayout{{"text","","text.layout"}}},validIntent.revision());
    else{std::vector<Command> commands;if(linked)commands.push_back(UnlinkTextLayout{{"text","","text.layout"}});
        auto t=*validIntent.document().objects.at("text").text;t.layout=linked?"auto":"frame";t.layout_driver.reset();commands.push_back(UpdateText{"text",t});validIntent.apply(commands,validIntent.revision());}
    check(decode(encode(validIntent.document()))==validIntent.document(),"Complete intended sizing commands native-validate before GUI");
    QSettings settings(scratch.filePath("settings.ini"),QSettings::IniFormat);
    Window w(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(settings),&settings);
    if(pending.empty())w.setAttribute(Qt::WA_DontShowOnScreen);w.resize(1400,900);
    w.host.session=Session(document);w.host.session_id="layout-window-session";w.host.edited();w.show();if(!pending.empty())w.activateWindow();events();
    w.canvas->set_active_artboard("composition","artboard",false);w.canvas->set_selection("text");events();
    auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Production Properties exists");dock->show();dock->raise();events();
    auto* area=w.findChild<QScrollArea*>("inspector-scroll");check(area,"Production Inspector exists");
    QPointer<QToolButton> drive=w.findChild<QToolButton*>("text-layout-driver");check(drive,"Existing sizing Driver exists");
    auto* display=w.findChild<QComboBox*>("text-layout");check(display&&!display->isEnabled(),"Existing sizing display uses explicit Edit sizing");
    QPointer<QMenu> menu=drive->menu();QPointer<QLineEdit> scalar;Session expected=w.host.session;
    if(!pending.empty()){
        for(auto* input:w.findChildren<QLineEdit*>()){
            const auto ref=QJsonDocument::fromJson(input->property("nect-reference").toByteArray()).object();
            if(input->isVisible()&&ref.value("object")=="text"&&ref.value("field")==QString::fromStdString("text."+pending))scalar=input;
        }
        check(scalar,"Requested pending sizing scalar exists");area->ensureWidgetVisible(scalar);events();point(w,scalar);
        QTest::keyClick(scalar,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(scalar,"64");events();
        check(scalar->hasFocus()&&scalar->isModified()&&same(w.host.session,expected),"Focused sizing draft is fully neutral");
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
            else if(cause=="layout")incoming.objects.at("text").text->layout="frame";
            else if(cause=="driver")incoming.objects.at("text").text->layout_driver=TextLayoutDriver{{"alternate","","text.layout"}};
            else if(cause=="source-id")incoming.objects.at("text").text->id="replacement-text-source";
            else if(cause=="composition"){incoming.compositions.back().roots=std::move(incoming.compositions.front().roots);incoming.compositions.front().roots.clear();}
            else if(cause=="document")incoming.id="incoming-document";
            else if(cause=="chosen-layout")incoming.objects.at("alternate").text->layout="auto";
            else if(cause=="chosen-body"||cause=="rechoose")incoming.objects.at("alternate").text->content="Incoming chosen Text";
            else if(cause=="chosen-source-id")incoming.objects.at("alternate").text->id="replacement-chosen-source";
            else if(cause=="chosen-driver")incoming.objects.at("alternate").text->layout_driver=TextLayoutDriver{{"source","","text.layout"}};
            else if(cause=="chosen-lineage")incoming.objects.at("source").text->layout="auto";
            else if(cause=="linked-layout")incoming.objects.at("source").text->layout="auto";
            else if(cause=="unrelated-source")incoming.objects.at("source").text->family="Independent family";
            else if(cause=="path")incoming.objects.at("path").contours.front().points.front().y.literal=103;
            else if(cause=="unrelated")incoming.compositions.back().name="Independent Composition";
            else if(cause!="equivalent")throw std::runtime_error("Unknown sizing fixture cause");
            check(decode(encode(incoming))==incoming,"Incoming sizing fixture native-validates");
            const auto rev=w.host.session.revision();Session replacement(incoming);
            if(rev){check(!pending.empty()&&rev==1,"Replacement retains one legitimate scalar revision");auto base=incoming;
                base.objects.at("text").text->parameters.at(pending)=document.objects.at("text").text->parameters.at(pending);
                replacement=Session(base);replacement.apply({EditProperties{{{"text","","text."+pending}},64,false}},replacement.revision());}
            check(replacement.revision()==rev,"Replacement retains same revision");w.host.session=std::move(replacement);
        }
    };
    const bool stale=cause=="body"||cause=="style"||cause=="layout"||cause=="driver"||cause=="source-id"||cause=="composition"||
        cause=="document"||cause=="session"||cause=="revision"||cause=="preview"||cause=="generation"||cause=="selection"||cause=="selection-return"||cause=="scope"||
        cause=="chosen-layout"||cause=="chosen-body"||cause=="chosen-source-id"||cause=="chosen-driver"||cause=="chosen-lineage"||(cause=="linked-layout"&&linked);
    if(phase=="entry")change();expected=w.host.session;auto complete=expected.document();
    if(!pending.empty()&&!(phase=="entry"&&stale)){
        complete.objects.at("text").text->parameters.at(pending).literal=64;
        expected.apply({EditProperties{{{"text","","text."+pending}},64,false}},expected.revision());
        check(expected.document()==complete,"Independent complete Document equals canonical scalar command");
    }
    const auto applyIntent=[&]{
        std::vector<Command> commands;
        if(link){complete.objects.at("text").text->layout_driver=TextLayoutDriver{{"alternate","","text.layout"}};commands={LinkTextLayout{{"text","","text.layout"},{"alternate","","text.layout"},linked}};}
        else if(action=="unlink"){complete.objects.at("text").text->layout=text_layout_property(complete,{"text","","text.layout"}).evaluated;complete.objects.at("text").text->layout_driver.reset();commands={UnlinkTextLayout{{"text","","text.layout"}}};}
        else{if(linked)commands.push_back(UnlinkTextLayout{{"text","","text.layout"}});auto& t=*complete.objects.at("text").text;t.layout=linked?"auto":"frame";t.layout_driver.reset();commands.push_back(UpdateText{"text",t});}
        expected.apply(commands,expected.revision());check(expected.document()==complete,"Independent complete Document equals canonical sizing command");
        check(same(w.host.session,expected),"Sizing Apply equals complete canonical Session");
    };
    area->ensureWidgetVisible(drive);events();bool menuOpen=false,handled=false;std::exception_ptr error;
    QTimer chooseMenu(&w),dialogTimer(&w),deadline(&w);QElapsedTimer visible;chooseMenu.setInterval(20);dialogTimer.setInterval(20);
    QObject::connect(&chooseMenu,&QTimer::timeout,&w,[&]{
        if(!menu||!menu->isVisible())return;if(!menuOpen){menuOpen=true;visible.start();}if(visible.elapsed()<QApplication::doubleClickInterval()+20)return;chooseMenu.stop();
        try{if(action=="unlink"&&phase=="apply")change();if(action=="unlink"){expected=w.host.session;complete=expected.document();}
            QTest::mouseClick(menu,Qt::LeftButton,Qt::NoModifier,menu->actionGeometry(menu->actions().at(action=="unlink"?2:link?1:0)).center());
            if(action=="unlink"){if(stale)check(same(w.host.session,expected),"Stale sizing Unlink refuses complete incoming Session");else applyIntent();handled=true;}
        }catch(...){error=std::current_exception();if(menu)menu->close();}
    });
    QObject::connect(&dialogTimer,&QTimer::timeout,&w,[&]{
        QPointer<QDialog> dialog=w.findChild<QDialog*>(link?"text-layout-source-dialog":"text-layout-dialog");if(!dialog||!dialog->isVisible())return;dialogTimer.stop();
        try{
            check(same(w.host.session,expected),"Sizing dialog entry commits only legitimate scalar");
            auto* combo=dialog->findChild<QComboBox*>(link?"text-layout-source":"text-layout-editor");
            if(action=="linked-edit")point(*dialog,dialog->findChild<QCheckBox*>("unlink-text-layout-driver"));
            int index=linked?0:1;
            if(link){index=-1;for(int i=0;i<combo->count();++i)if(combo->itemText(i).endsWith(" — alternate"))index=i;}
            choose(*dialog,combo,index);check(same(w.host.session,expected),"Sizing/source choice remains fully neutral");
            if(phase=="apply")change();expected=w.host.session;complete=expected.document();
            if(cause=="rechoose"){
                choose(*dialog,combo,index);
                check(same(w.host.session,expected),"Explicit source rechoose retains the complete incoming Session");
            }
            const auto selection=w.canvas->selections();const auto composition=w.canvas->active_composition(),artboard=w.canvas->active_artboard();
            auto* buttons=dialog->findChild<QDialogButtonBox*>();check(buttons,"Explicit sizing Apply/Cancel exists");
            if(cause=="cancel"){point(*dialog,buttons->button(QDialogButtonBox::Cancel));check(same(w.host.session,expected),"Sizing Cancel retains full Session");}
            else{point(*dialog,buttons->button(QDialogButtonBox::Apply));
                if(stale&&phase=="apply"){check(same(w.host.session,expected),"Stale sizing Apply refuses complete incoming Session");check(dialog&&dialog->isVisible(),"Refused sizing/source draft remains open");point(*dialog,buttons->button(QDialogButtonBox::Cancel));check(same(w.host.session,expected),"Cancel after sizing refusal stays neutral");}
                else applyIntent();}
            check(w.canvas->selections()==selection&&w.canvas->active_composition()==composition&&w.canvas->active_artboard()==artboard,"Sizing completion preserves exact selection/scope");
        }catch(...){error=std::current_exception();}if(dialog&&dialog->isVisible())dialog->reject();handled=true;
    });
    deadline.setSingleShot(true);QObject::connect(&deadline,&QTimer::timeout,&w,[&]{error=std::make_exception_ptr(std::runtime_error("Owned sizing menu/dialog timeout"));chooseMenu.stop();dialogTimer.stop();if(menu)menu->close();if(auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget()))dialog->reject();});
    deadline.start(10000);chooseMenu.start();if(action!="unlink")dialogTimer.start();
    if(drive)point(w,drive);else check(stale&&same(w.host.session,expected),"Expired original sizing control cannot retarget replacement");
    if(!(stale&&phase=="entry"))(void)QTest::qWaitFor([&]{return bool(error)||handled;},7000);
    deadline.stop();chooseMenu.stop();dialogTimer.stop();events();if(error)std::rethrow_exception(error);
    if(stale&&phase=="entry"){check(same(w.host.session,expected),"Stale sizing entry refuses before consuming pending scalar");check(!menuOpen,"Stale sizing entry keeps menu closed");}
    else check(menuOpen&&handled,"First driver pointer reaches existing sizing action");
    if(!stale){if(cause!="cancel"){history(w,"Undo");expected.undo(expected.revision());check(same(w.host.session,expected),"Sizing Undo is separate from scalar");}
        if(!pending.empty()){history(w,"Undo");expected.undo(expected.revision());check(same(w.host.session,expected),"Second Undo restores scalar");history(w,"Redo");expected.redo(expected.revision());check(same(w.host.session,expected),"Scalar Redo is independent");}
        if(cause!="cancel"){history(w,"Redo");expected.redo(expected.revision());check(same(w.host.session,expected),"Sizing Redo retains full authored state");}cold(w,expected);}
    w.host.changed={};w.hide();events();
}
inline int run(bool discovery,bool rechoose_only=false){
    int cases=0,failures=0;const auto test=[&](const std::string& action,const std::string& phase,const std::string& cause,const std::string& pending={}){
        ++cases;std::cout<<"SizingCase "<<action<<" "<<phase<<" "<<cause<<" pending="<<pending<<std::endl;
        try{context(action,phase,cause,pending);}catch(const std::exception& e){++failures;std::cerr<<"SIZING_WINDOW_RED "<<action<<" "<<phase<<" "<<cause<<" pending="<<pending<<": "<<e.what()<<std::endl;}
    };
    if(rechoose_only){for(const auto* action:{"link","replace"})test(action,"apply","rechoose");}
    else if(discovery){for(const auto* action:{"edit","linked-edit","link","replace","unlink"}){test(action,"entry","body");test(action,"apply","body");test(action,"apply","valid");}
        test("link","apply","chosen-layout");test("replace","apply","chosen-body");for(const auto* scalar:{"font_size","frame_width","frame_height"})test("link","entry","body",scalar);}
    else{for(const auto* action:{"edit","linked-edit","link","replace","unlink"}){
        for(const auto* cause:{"body","composition"})test(action,"entry",cause);
        for(const auto* cause:{"valid","body","style","layout","driver","source-id","composition","equivalent","path","unrelated"})test(action,"apply",cause);
        for(const auto* scalar:{"font_size","frame_width","frame_height"}){test(action,"apply","valid",scalar);test(action,"entry","body",scalar);test(action,"apply","body",scalar);if(std::string(action)!="unlink")test(action,"apply","cancel",scalar);}
    }
        for(const auto* action:{"link","replace"})for(const auto* cause:{"chosen-layout","chosen-body","chosen-source-id","chosen-driver","chosen-lineage","rechoose","unrelated-source"})test(action,"apply",cause);
        for(const auto* cause:{"document","session","revision","preview","generation","selection","selection-return","scope","refresh"})test("link","apply",cause);
        for(const auto* action:{"linked-edit","replace","unlink"})test(action,"apply","linked-layout");
    }
    std::cout<<"SizingWindow cases="<<cases<<" checks="<<checks<<" failures="<<failures<<" physical OS input NOT_RUN"<<std::endl;return failures?1:0;
}
}

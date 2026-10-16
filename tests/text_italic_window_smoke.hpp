#pragma once
#include <QInputDialog>

namespace text_italic_window_smoke {
using text_typography_window_smoke::check;
using text_typography_window_smoke::events;
using text_typography_window_smoke::same;
using text_typography_window_smoke::point;
using text_typography_window_smoke::history;
using text_typography_window_smoke::cold;
using text_typography_window_smoke::checks;
inline Document fixture(bool linked,bool expression=false){
    auto d=text_family_window_smoke::fixture(false);d.id="italic-window-document";
    d.objects.at("source").text->italic=true;d.objects.at("alternate").text->italic=true;
    if(linked)d.objects.at("text").text->italic_driver=Ref{"source","","text.italic"};
    if(expression)d.objects.at("text").text->italic_driver=Expression{"ref(\"source\",\"\",\"text.italic\")",1};
    check(decode(encode(d))==d,"Complete italic fixture native-validates");return d;
}
inline void context(const std::string& action,const std::string& phase,const std::string& cause,bool pending=false){
    const bool linked=action=="replace"||action=="unlink",expression=action=="replace-expression",literal=action=="literal";
    QTemporaryDir scratch;check(scratch.isValid(),"Italic Window owns scratch");const auto document=fixture(linked,expression);
    Session intent(document);
    if(pending)intent.apply({EditProperties{{{"text","","text.font_size"}},64,false}},intent.revision());
    if(literal){auto t=*intent.document().objects.at("text").text;t.italic=true;intent.apply({UpdateText{"text",t}},intent.revision());}
    else if(action=="link"||action=="replace")intent.apply({LinkTextItalic{{"text","","text.italic"},{"alternate","","text.italic"},linked}},intent.revision());
    else if(action=="unlink")intent.apply({UnlinkTextItalic{{"text","","text.italic"}}},intent.revision());
    else intent.apply({SetTextItalicExpression{{"text","","text.italic"},Expression{"ref(\"alternate\",\"\",\"text.italic\")",1},expression}},intent.revision());
    check(decode(encode(intent.document()))==intent.document(),"Complete canonical italic intent native-validates");
    QSettings settings(scratch.filePath("settings.ini"),QSettings::IniFormat);
    Window w(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(settings),&settings);
    if(!pending)w.setAttribute(Qt::WA_DontShowOnScreen);w.resize(1400,900);
    w.host.session=Session(document);w.host.session_id="italic-window-session";w.host.edited();w.show();if(pending)w.activateWindow();events();
    w.canvas->set_active_artboard("composition","artboard",false);w.canvas->set_selection("text");events();
    auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Production Properties exists");dock->show();dock->raise();events();
    auto* area=w.findChild<QScrollArea*>("inspector-scroll");check(area,"Production Inspector exists");
    QPointer<QCheckBox> box=w.findChild<QCheckBox*>("text-italic");
    QPointer<QToolButton> drive=w.findChild<QToolButton*>("text-italic-driver");check(box&&drive,"Existing italic controls exist");
    QPointer<QMenu> menu=drive->menu();QPointer<QLineEdit> size;Session expected=w.host.session;
    if(pending){for(auto* input:w.findChildren<QLineEdit*>()){
        const auto ref=QJsonDocument::fromJson(input->property("nect-reference").toByteArray()).object();
        if(input->isVisible()&&ref.value("object")=="text"&&ref.value("field")=="text.font_size")size=input;
    }
        check(size,"Font size exists");area->ensureWidgetVisible(size);events();point(w,size);
        QTest::keyClick(size,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(size,"64");events();
        check(size->hasFocus()&&size->isModified()&&same(w.host.session,expected),"Focused size draft is fully neutral");
    }
    const auto change=[&]{
        if(cause=="session")w.host.session_id+="-incoming";
        else if(cause=="revision")w.host.session.apply({Rename{"path","External edit"}},w.host.session.revision());
        else if(cause=="preview"||cause=="generation"){w.host.session.begin_gesture(w.host.session.revision());w.host.session.update_gesture({Rename{"path","Preview"}});if(cause=="generation")w.host.session.cancel_gesture();}
        else if(cause=="selection"||cause=="selection-return"){w.canvas->set_selection("path");if(cause=="selection-return")w.canvas->set_selection(literal?"text":"alternate");events();}
        else if(cause=="scope"){w.canvas->set_active_artboard("other-composition","other-artboard",false);events();}
        else if(cause=="refresh"){w.host.edited();events();}
        else if(cause!="valid"&&cause!="cancel"){
            auto incoming=w.host.session.document();
            if(cause=="body")incoming.objects.at("text").text->content="Incoming authored body";
            else if(cause=="style")incoming.objects.at("text").text->family="Times New Roman";
            else if(cause=="italic")incoming.objects.at("text").text->italic=true;
            else if(cause=="driver")incoming.objects.at("text").text->italic_driver=Expression{"false",1};
            else if(cause=="source-id")incoming.objects.at("text").text->id="replacement-text-source";
            else if(cause=="attachment")incoming.objects.at("text").text->path_attachment->start=20;
            else if(cause=="composition"){incoming.compositions.back().roots=std::move(incoming.compositions.front().roots);incoming.compositions.front().roots.clear();}
            else if(cause=="document")incoming.id="incoming-document";
            else if(cause=="chosen-italic")incoming.objects.at("alternate").text->italic=false;
            else if(cause=="chosen-body")incoming.objects.at("alternate").text->content="Incoming chosen Text";
            else if(cause=="linked-italic")incoming.objects.at("source").text->italic=false;
            else if(cause=="unrelated-source")incoming.objects.at("source").text->family="Independent family";
            else if(cause=="path")incoming.objects.at("path").contours.front().points.front().y.literal=103;
            else if(cause=="unrelated")incoming.compositions.back().name="Independent Composition";
            else if(cause!="equivalent")throw std::runtime_error("Unknown italic fixture cause");
            check(decode(encode(incoming))==incoming,"Incoming italic fixture native-validates");
            const auto rev=w.host.session.revision();Session replacement(incoming);
            if(rev){auto base=incoming;base.objects.at("text").text->parameters.at("font_size")=document.objects.at("text").text->parameters.at("font_size");
                replacement=Session(base);replacement.apply({EditProperties{{{"text","","text.font_size"}},64,false}},replacement.revision());}
            check(replacement.revision()==rev,"Replacement retains same revision");w.host.session=std::move(replacement);
        }
    };
    const bool stale=cause=="body"||cause=="style"||cause=="italic"||cause=="driver"||cause=="source-id"||cause=="attachment"||cause=="composition"||
        cause=="document"||cause=="session"||cause=="revision"||cause=="preview"||cause=="generation"||cause=="selection"||cause=="selection-return"||cause=="scope"||
        cause=="chosen-italic"||cause=="chosen-body"||(cause=="linked-italic"&&(linked||expression));
    if(phase=="entry"||literal)change();expected=w.host.session;auto complete=expected.document();
    if(pending&&!((phase=="entry"||literal)&&stale)){complete.objects.at("text").text->parameters.at("font_size").literal=64;
        expected.apply({EditProperties{{{"text","","text.font_size"}},64,false}},expected.revision());
        check(expected.document()==complete,"Independent complete Document equals canonical size command");}
    const auto applyIntent=[&]{
        std::vector<Command> commands;
        if(literal){complete.objects.at("text").text->italic=true;commands={UpdateText{"text",*complete.objects.at("text").text}};}
        else if(action=="link"||action=="replace"){complete.objects.at("text").text->italic_driver=Ref{"alternate","","text.italic"};commands={LinkTextItalic{{"text","","text.italic"},{"alternate","","text.italic"},linked}};}
        else if(action=="unlink"){complete.objects.at("text").text->italic=evaluate_text_italic(complete,"text");complete.objects.at("text").text->italic_driver.reset();commands={UnlinkTextItalic{{"text","","text.italic"}}};}
        else{complete.objects.at("text").text->italic_driver=Expression{"ref(\"alternate\",\"\",\"text.italic\")",1};commands={SetTextItalicExpression{{"text","","text.italic"},std::get<Expression>(*complete.objects.at("text").text->italic_driver),expression}};}
        expected.apply(commands,expected.revision());check(expected.document()==complete,"Independent complete Document equals canonical italic command");
        check(same(w.host.session,expected),"Italic Apply equals complete canonical Session");
    };
    bool legacy_expression_commit=false;
    const auto finish=[&](QDialog* dialog){
        if(phase=="apply")change();expected=w.host.session;complete=expected.document();
        const auto selection=w.canvas->selections();const auto composition=w.canvas->active_composition(),artboard=w.canvas->active_artboard();
        auto* buttons=dialog->findChild<QDialogButtonBox*>();check(buttons,"Explicit commit/Cancel exists");
        if(cause=="cancel"){point(*dialog,buttons->button(QDialogButtonBox::Cancel));check(same(w.host.session,expected),"Cancel retains full Session");return;}
        const bool legacy=qobject_cast<QInputDialog*>(dialog);
        point(*dialog,buttons->button(legacy?QDialogButtonBox::Ok:QDialogButtonBox::Apply));
        // QInputDialog authors only after getText returns to the menu callback.
        if(legacy){legacy_expression_commit=true;return;}
        if(stale&&phase=="apply"){check(same(w.host.session,expected),"Stale italic Apply refuses complete incoming Session");
            check(dialog->isVisible(),"Refused italic draft remains open");point(*dialog,buttons->button(QDialogButtonBox::Cancel));check(same(w.host.session,expected),"Cancel after refusal stays neutral");
            if(cause=="selection"||cause=="selection-return"||cause=="scope")check(w.canvas->selections()==selection&&w.canvas->active_composition()==composition&&w.canvas->active_artboard()==artboard,"Cancel preserves external selection/scope");
        }else applyIntent();
    };
    if(literal){
        if(!box){check(stale&&same(w.host.session,expected),"Expired original checkbox cannot retarget replacement controls");w.host.changed={};w.hide();events();return;}
        area->ensureWidgetVisible(box);events();const auto pos=box->mapTo(&w,QPoint(8,box->height()/2));check(w.childAt(pos)==box,"Pointer hits original checkbox indicator");
        QTest::mouseClick(w.windowHandle(),Qt::LeftButton,Qt::NoModifier,pos);events();
        if(stale)check(same(w.host.session,expected),"Stale checkbox refuses full incoming Session without consuming size");else applyIntent();
    }else{
        area->ensureWidgetVisible(drive);events();bool menuOpen=false,menuChosen=false,handled=false;std::exception_ptr error;
        QTimer choose(&w),dialogTimer(&w),deadline(&w);QElapsedTimer visible;choose.setInterval(20);dialogTimer.setInterval(20);
        QObject::connect(&choose,&QTimer::timeout,&w,[&]{
            if(!menu||!menu->isVisible())return;menuOpen=true;if(!visible.isValid()){visible.start();return;}
            if(visible.elapsed()<QApplication::doubleClickInterval()+20)return;choose.stop();
            try{if(action=="unlink"&&phase=="apply")change();if(action=="unlink"){expected=w.host.session;complete=expected.document();}
                QTest::mouseClick(menu,Qt::LeftButton,Qt::NoModifier,menu->actionGeometry(menu->actions().at(action=="unlink"?2:action=="expression"||expression?1:0)).center());menuChosen=true;
                if(action=="unlink"){if(stale)check(same(w.host.session,expected),"Stale Unlink refuses full incoming Session");else applyIntent();handled=true;}
            }catch(...){error=std::current_exception();if(menu)menu->close();}
        });
        if(action=="expression"||expression)QObject::connect(&dialogTimer,&QTimer::timeout,&w,[&]{
            auto* dialog=w.findChild<QDialog*>("text-italic-expression-dialog");if(!dialog||!dialog->isVisible())return;dialogTimer.stop();
            try{check(same(w.host.session,expected),"Expression entry retains full Session");auto* editor=dialog->findChild<QPlainTextEdit*>("text-italic-expression-draft");check(editor,"Existing expression draft exists");point(*dialog,editor->viewport());
                QTest::keyClick(editor,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(editor,"ref(\"alternate\",\"\",\"text.italic\")");
                if(expression)point(*dialog,dialog->findChild<QCheckBox*>("text-italic-expression-replace"));events();check(same(w.host.session,expected),"Expression draft stays neutral");finish(dialog);
            }catch(...){error=std::current_exception();}if(dialog->isVisible())dialog->reject();handled=true;
        });
        deadline.setSingleShot(true);QObject::connect(&deadline,&QTimer::timeout,&w,[&]{error=std::make_exception_ptr(std::runtime_error("Owned italic menu/dialog timeout"));choose.stop();dialogTimer.stop();if(menu)menu->close();if(auto* d=qobject_cast<QDialog*>(QApplication::activeModalWidget()))d->reject();});
        deadline.start(10000);choose.start();if(action=="expression"||expression)dialogTimer.start();point(w,drive);
        if(!(stale&&phase=="entry"))(void)QTest::qWaitFor([&]{return bool(error)||((action=="expression"||expression||action=="unlink")?handled:menuChosen);},7000);
        deadline.stop();choose.stop();dialogTimer.stop();events();if(error)std::rethrow_exception(error);
        if(legacy_expression_commit){if(stale)check(same(w.host.session,expected),"Legacy expression completion refuses full incoming Session");else applyIntent();}
        if(stale&&phase=="entry"){check(same(w.host.session,expected),"Stale driver entry refuses before consuming size");check(!menuOpen,"Stale driver entry does not open menu");}
        else if(action=="link"||action=="replace"){
            QPointer<QDialog> dialog=w.findChild<QDialog*>("text-source-picker");check(menuOpen&&dialog&&dialog->isVisible(),"First driver pointer opens picker");check(same(w.host.session,expected),"Picker commits only legitimate size");
            auto* list=dialog->findChild<QListWidget*>("text-source-picker-list");QListWidgetItem* item=nullptr;
            for(int i=0;i<list->count();++i){const auto ref=QJsonDocument::fromJson(list->item(i)->data(Qt::UserRole).toByteArray()).object();if(ref.value("object")=="alternate"&&ref.value("field")=="text.italic")item=list->item(i);}
            check(item&&!item->isHidden(),"Exact italic source Ref exists");QTest::mouseClick(dialog->windowHandle(),Qt::LeftButton,Qt::NoModifier,list->viewport()->mapTo(dialog,list->visualItemRect(item).center()));events();
            check(same(w.host.session,expected),"Chosen source preview preserves full authored Session");finish(dialog);if(dialog&&dialog->isVisible())dialog->reject();events();
        }else check(menuOpen&&handled,"First pointer reaches existing italic action");
    }
    if(!stale){
        if(cause!="cancel"){history(w,"Undo");expected.undo(expected.revision());check(same(w.host.session,expected),"Italic Undo is separate from size");}
        if(pending){history(w,"Undo");expected.undo(expected.revision());check(same(w.host.session,expected),"Second Undo restores size");history(w,"Redo");expected.redo(expected.revision());check(same(w.host.session,expected),"Size Redo is independent");}
        if(cause!="cancel"){history(w,"Redo");expected.redo(expected.revision());check(same(w.host.session,expected),"Italic Redo retains full authored state");}cold(w,expected);
    }
    w.host.changed={};w.hide();events();
}
inline int run(bool discovery,bool expression_only=false,bool expired_only=false){
    int cases=0,failures=0;const auto test=[&](const std::string& action,const std::string& phase,const std::string& cause,bool pending=false){++cases;std::cout<<"ItalicCase "<<action<<" "<<phase<<" "<<cause<<" pending="<<pending<<std::endl;
        try{context(action,phase,cause,pending);}catch(const std::exception& e){++failures;std::cerr<<"ITALIC_WINDOW_RED "<<action<<" "<<phase<<" "<<cause<<" pending="<<pending<<": "<<e.what()<<std::endl;}};
    if(expired_only){for(const auto* cause:{"selection","selection-return","scope"})test("literal","entry",cause);}
    else if(expression_only){for(const auto* action:{"expression","replace-expression"}){test(action,"entry","body");test(action,"apply","body");}}
    else if(discovery){for(const auto* action:{"literal","link","replace","expression","replace-expression","unlink"})test(action,"entry","body");
        for(const auto* action:{"link","replace","expression","replace-expression","unlink"})test(action,"apply","body");test("link","apply","chosen-italic");test("replace","apply","chosen-body");test("link","entry","body",true);}
    else{for(const auto* action:{"literal","link","replace","expression","replace-expression","unlink"}){
        for(const auto* cause:{"body","composition"})test(action,"entry",cause);
        for(const auto* cause:{"valid","body","style","italic","driver","source-id","attachment","composition","equivalent","path","unrelated"})test(action,"apply",cause);
        test(action,"apply","valid",true);test(action,"entry","body",true);test(action,"apply","body",true);
        if(std::string(action)!="literal"&&std::string(action)!="unlink")test(action,"apply","cancel",true);
    }
        for(const auto* action:{"link","replace"})for(const auto* cause:{"chosen-italic","chosen-body","unrelated-source"})test(action,"apply",cause);
        for(const auto* cause:{"document","session","revision","preview","generation","selection","selection-return","scope","refresh"}){test("link","apply",cause);test("expression","apply",cause);if(std::string(cause)!="refresh")test("literal","entry",cause);}
        test("replace","apply","linked-italic");test("replace-expression","apply","linked-italic");test("unlink","apply","linked-italic");
    }
    std::cout<<"ItalicWindow cases="<<cases<<" checks="<<checks<<" failures="<<failures<<" physical OS input NOT_RUN"<<std::endl;return failures?1:0;
}
}

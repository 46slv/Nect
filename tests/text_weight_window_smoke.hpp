#pragma once
#include <QSpinBox>
#include <QPlainTextEdit>
#include <QStyleOptionSpinBox>

namespace text_weight_window_smoke {
using text_typography_window_smoke::check;
using text_typography_window_smoke::events;
using text_typography_window_smoke::same;
using text_typography_window_smoke::point;
using text_typography_window_smoke::history;
using text_typography_window_smoke::cold;
using text_typography_window_smoke::checks;
inline Document fixture(bool linked,bool expression=false){
    auto d=text_family_window_smoke::fixture(false);d.id="weight-window-document";
    d.objects.at("source").text->weight=700;d.objects.at("alternate").text->weight=650;
    if(linked)d.objects.at("text").text->weight_driver=TextWeightDriver{{"source","","text.weight"},10};
    if(expression)d.objects.at("text").text->weight_expression=Expression{"ref(\"source\",\"\",\"text.weight\") - 25",1};
    check(decode(encode(d))==d,"Complete weight fixture native-validates");return d;
}
inline void context(const std::string& action,const std::string& phase,const std::string& cause,bool pending=false){
    const bool linked=action=="replace"||action=="unlink",expression=action=="replace-expression";
    const bool scalar=action=="spin"||action=="literal";
    QTemporaryDir scratch;check(scratch.isValid(),"Weight Window owns scratch");const auto document=fixture(linked,expression);
    Session intent(document);
    if(pending)intent.apply({EditProperties{{{"text","","text.font_size"}},64,false}},intent.revision());
    if(scalar){auto text=*intent.document().objects.at("text").text;text.weight=600;intent.apply({UpdateText{"text",text}},intent.revision());}
    else if(action=="link"||action=="replace")intent.apply({LinkTextWeight{{"text","","text.weight"},{"alternate","","text.weight"},linked}},intent.revision());
    else if(action=="unlink")intent.apply({UnlinkTextWeight{{"text","","text.weight"}}},intent.revision());
    else intent.apply({SetTextWeightExpression{{"text","","text.weight"},Expression{"ref(\"alternate\",\"\",\"text.weight\") - 25",1},expression}},intent.revision());
    check(decode(encode(intent.document()))==intent.document(),"Complete typed weight intent native-validates");
    QSettings settings(scratch.filePath("settings.ini"),QSettings::IniFormat);
    Window w(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(settings),&settings);
    if(!pending)w.setAttribute(Qt::WA_DontShowOnScreen);w.resize(1400,900);
    w.host.session=Session(document);w.host.session_id="weight-window-session";w.host.edited();w.show();if(pending)w.activateWindow();events();
    w.canvas->set_active_artboard("composition","artboard",false);w.canvas->set_selection("text");events();
    auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Properties exists");dock->show();dock->raise();events();
    auto* area=w.findChild<QScrollArea*>("inspector-scroll");check(area,"Inspector exists");
    QPointer<QSpinBox> spin=w.findChild<QSpinBox*>("text-weight");
    QPointer<QToolButton> drive=w.findChild<QToolButton*>("text-weight-driver");check(spin&&drive,"Production weight controls exist");
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
        else if(cause=="selection"||cause=="selection-return"){w.canvas->set_selection("path");if(cause=="selection-return")w.canvas->set_selection(scalar?"text":"alternate");events();}
        else if(cause=="scope"){w.canvas->set_active_artboard("other-composition","other-artboard",false);events();}
        else if(cause=="refresh"){w.host.edited();events();}
        else if(cause!="valid"&&cause!="cancel"){
            auto incoming=w.host.session.document();
            if(cause=="body")incoming.objects.at("text").text->content="Incoming authored body";
            else if(cause=="style")incoming.objects.at("text").text->family="Times New Roman";
            else if(cause=="weight")incoming.objects.at("text").text->weight=800;
            else if(cause=="driver"){incoming.objects.at("text").text->weight_expression.reset();incoming.objects.at("text").text->weight_driver=TextWeightDriver{{"alternate","","text.weight"},-15};}
            else if(cause=="source-id")incoming.objects.at("text").text->id="replacement-text-source";
            else if(cause=="attachment")incoming.objects.at("text").text->path_attachment->start=20;
            else if(cause=="composition"){incoming.compositions.back().roots=std::move(incoming.compositions.front().roots);incoming.compositions.front().roots.clear();}
            else if(cause=="document")incoming.id="incoming-document";
            else if(cause=="chosen-weight")incoming.objects.at("alternate").text->weight=900;
            else if(cause=="chosen-body")incoming.objects.at("alternate").text->content="Incoming chosen Text";
            else if(cause=="linked-weight")incoming.objects.at("source").text->weight=750;
            else if(cause=="unrelated-source")incoming.objects.at("source").text->family="Independent family";
            else if(cause=="path")incoming.objects.at("path").contours.front().points.front().y.literal=103;
            else if(cause=="unrelated")incoming.compositions.back().name="Independent Composition";
            else if(cause!="equivalent")throw std::runtime_error("Unknown weight fixture cause");
            check(decode(encode(incoming))==incoming,"Incoming weight fixture native-validates");
            const auto rev=w.host.session.revision();Session replacement(incoming);
            if(rev){auto base=incoming;base.objects.at("text").text->parameters.at("font_size")=document.objects.at("text").text->parameters.at("font_size");
                replacement=Session(base);replacement.apply({EditProperties{{{"text","","text.font_size"}},64,false}},replacement.revision());}
            check(replacement.revision()==rev,"Replacement retains same revision");w.host.session=std::move(replacement);
        }
    };
    const bool stale=cause=="body"||cause=="style"||cause=="weight"||cause=="driver"||cause=="source-id"||cause=="attachment"||cause=="composition"||
        cause=="document"||cause=="session"||cause=="revision"||cause=="preview"||cause=="generation"||cause=="selection"||cause=="selection-return"||cause=="scope"||
        cause=="chosen-weight"||cause=="chosen-body"||(cause=="linked-weight"&&(linked||expression));
    if(phase=="entry")change();expected=w.host.session;auto complete=expected.document();
    if(pending&&!(phase=="entry"&&stale)){complete.objects.at("text").text->parameters.at("font_size").literal=64;
        expected.apply({EditProperties{{{"text","","text.font_size"}},64,false}},expected.revision());
        check(expected.document()==complete,"Independent complete Document equals canonical size command");}
    const auto applyIntent=[&]{
        std::vector<Command> commands;
        if(scalar){complete.objects.at("text").text->weight=600;commands={UpdateText{"text",*complete.objects.at("text").text}};}
        else if(action=="link"||action=="replace"){complete.objects.at("text").text->weight_expression.reset();complete.objects.at("text").text->weight_driver=TextWeightDriver{{"alternate","","text.weight"},0};
            commands={LinkTextWeight{{"text","","text.weight"},{"alternate","","text.weight"},linked}};}
        else if(action=="unlink"){complete.objects.at("text").text->weight=evaluate_text_weight(complete,"text");complete.objects.at("text").text->weight_driver.reset();complete.objects.at("text").text->weight_expression.reset();commands={UnlinkTextWeight{{"text","","text.weight"}}};}
        else{complete.objects.at("text").text->weight_driver.reset();complete.objects.at("text").text->weight_expression=Expression{"ref(\"alternate\",\"\",\"text.weight\") - 25",1};
            commands={SetTextWeightExpression{{"text","","text.weight"},*complete.objects.at("text").text->weight_expression,expression}};}
        expected.apply(commands,expected.revision());check(expected.document()==complete,"Independent complete Document equals canonical weight command");
        check(same(w.host.session,expected),"Weight Apply equals complete canonical Session");
    };
    const auto finish=[&](QDialog* dialog){
        if(phase=="apply")change();expected=w.host.session;complete=expected.document();
        const auto selection=w.canvas->selections();const auto composition=w.canvas->active_composition(),artboard=w.canvas->active_artboard();
        auto* buttons=dialog->findChild<QDialogButtonBox*>();check(buttons,"Explicit Apply/Cancel exists");
        if(cause=="cancel"){point(*dialog,buttons->button(QDialogButtonBox::Cancel));check(same(w.host.session,expected),"Cancel retains full Session");return;}
        point(*dialog,buttons->button(QDialogButtonBox::Apply));
        if(stale&&phase=="apply"){check(same(w.host.session,expected),"Stale weight Apply refuses complete incoming Session");
            check(dialog->isVisible(),"Refused weight draft remains open");point(*dialog,buttons->button(QDialogButtonBox::Cancel));
            check(same(w.host.session,expected),"Cancel after refusal stays neutral");
            if(cause=="selection"||cause=="selection-return"||cause=="scope")check(w.canvas->selections()==selection&&w.canvas->active_composition()==composition&&w.canvas->active_artboard()==artboard,"Cancel preserves external scope/selection");
        }else applyIntent();
    };
    if(scalar){
        area->ensureWidgetVisible(spin);events();
        const auto originalValue=spin->value();
        if(action=="spin"){QStyleOptionSpinBox opt;opt.initFrom(spin);opt.subControls=QStyle::SC_All;
            auto up=spin->style()->subControlRect(QStyle::CC_SpinBox,&opt,QStyle::SC_SpinBoxUp,spin);
            auto p=spin->mapTo(&w,up.center());check(!up.isEmpty()&&w.childAt(p)==spin,"Actual pointer resolves to styled spin step");
            QTest::mouseClick(w.windowHandle(),Qt::LeftButton,Qt::NoModifier,p);events();
        }else{QPointer<QLineEdit> editor=spin->findChild<QLineEdit*>();point(w,editor);check(spin&&editor,"Original literal control survives first pointer");
            if(!(stale&&phase=="entry")){QTest::keyClick(editor,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(editor,"600");events();}}
        std::cout<<"WeightEntry "<<action<<" "<<phase<<" "<<cause<<" pending="<<pending<<" actual-rev="<<w.host.session.revision()
            <<" expected-rev="<<expected.revision()<<" actual-weight="<<w.host.session.document().objects.at("text").text->weight<<" spin="<<bool(spin)<<std::endl;
        check(same(w.host.session,expected),"Weight entry commits only independent legitimate size");
        if(stale&&phase=="entry")check(spin&&spin->value()==originalValue,"Stale scalar entry keeps the original weight draft");
        if(!(stale&&phase=="entry")){
            check(spin,"Existing scalar survives pending size");if(phase=="apply")change();expected=w.host.session;complete=expected.document();
            if(!spin)check(stale&&same(w.host.session,expected),"Destroyed stale scalar draft cannot retarget replacement controls");
            else{QTest::keyClick(spin,Qt::Key_Return);events();if(stale)check(same(w.host.session,expected),"Stale scalar finish refuses full incoming Session");else applyIntent();}
        }
    }else{
        area->ensureWidgetVisible(drive);events();bool menuOpen=false,menuChosen=false,handled=false;std::exception_ptr error;
        QTimer choose(&w),dialogTimer(&w),deadline(&w);QElapsedTimer visible;
        choose.setInterval(20);dialogTimer.setInterval(20);
        QObject::connect(&choose,&QTimer::timeout,&w,[&]{
            if(!menu||!menu->isVisible())return;menuOpen=true;if(!visible.isValid()){visible.start();return;}
            if(visible.elapsed()<QApplication::doubleClickInterval()+20)return;choose.stop();
            try{if(action=="unlink"&&phase=="apply")change();if(action=="unlink"){expected=w.host.session;complete=expected.document();}
                QTest::mouseClick(menu,Qt::LeftButton,Qt::NoModifier,menu->actionGeometry(menu->actions().at(action=="unlink"?2:action=="expression"||expression?1:0)).center());menuChosen=true;
                if(action=="unlink"){if(stale)check(same(w.host.session,expected),"Stale Unlink refuses complete incoming Session");else applyIntent();handled=true;}
            }catch(...){error=std::current_exception();if(menu)menu->close();}
        });
        if(action=="expression"||expression)QObject::connect(&dialogTimer,&QTimer::timeout,&w,[&]{
            auto* dialog=w.findChild<QDialog*>("text-weight-expression-dialog");if(!dialog||!dialog->isVisible())return;dialogTimer.stop();
            try{check(same(w.host.session,expected),"Expression entry retains full Session");
                auto* editor=dialog->findChild<QPlainTextEdit*>("text-weight-expression-draft");check(editor,"Existing expression draft exists");point(*dialog,editor->viewport());
                QTest::keyClick(editor,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(editor,"ref(\"alternate\",\"\",\"text.weight\") - 25");
                if(expression)point(*dialog,dialog->findChild<QCheckBox*>("text-weight-expression-replace"));events();
                check(same(w.host.session,expected),"Expression draft remains neutral");finish(dialog);
            }catch(...){error=std::current_exception();}if(dialog->isVisible())dialog->reject();handled=true;
        });
        deadline.setSingleShot(true);QObject::connect(&deadline,&QTimer::timeout,&w,[&]{
            error=std::make_exception_ptr(std::runtime_error("Owned weight menu/dialog timeout"));choose.stop();dialogTimer.stop();if(menu)menu->close();
            if(auto* d=w.findChild<QDialog*>("text-weight-expression-dialog"))d->reject();
        });
        deadline.start(10000);choose.start();if(action=="expression"||expression)dialogTimer.start();point(w,drive);
        if(!(stale&&phase=="entry"))(void)QTest::qWaitFor([&]{return bool(error)||((action=="expression"||expression||action=="unlink")?handled:menuChosen);},7000);
        deadline.stop();choose.stop();dialogTimer.stop();events();if(error)std::rethrow_exception(error);
        std::cout<<"WeightPointer "<<action<<" "<<phase<<" "<<cause<<" pending="<<pending<<" menu="<<menuOpen<<" chosen="<<menuChosen<<" handled="<<handled<<std::endl;
        if(stale&&phase=="entry"){check(same(w.host.session,expected),"Stale driver entry refuses before consuming size");check(!menuOpen,"Stale driver entry does not open writable menu");}
        else if(action=="link"||action=="replace"){
            QPointer<QDialog> dialog=w.findChild<QDialog*>("text-source-picker");check(menuOpen&&dialog&&dialog->isVisible(),"First driver pointer opens owned picker");
            check(same(w.host.session,expected),"Picker entry commits only legitimate size");
            auto* list=dialog->findChild<QListWidget*>("text-source-picker-list");QListWidgetItem* item=nullptr;
            for(int i=0;i<list->count();++i){const auto ref=QJsonDocument::fromJson(list->item(i)->data(Qt::UserRole).toByteArray()).object();
                if(ref.value("object")=="alternate"&&ref.value("field")=="text.weight")item=list->item(i);}
            check(item&&!item->isHidden(),"Exact weight source Ref exists");
            QTest::mouseClick(dialog->windowHandle(),Qt::LeftButton,Qt::NoModifier,list->viewport()->mapTo(dialog,list->visualItemRect(item).center()));events();
            check(same(w.host.session,expected),"Chosen source preview preserves full authored Session");finish(dialog);if(dialog&&dialog->isVisible())dialog->reject();events();
        }else check(menuOpen&&handled,"First driver pointer reaches existing weight action");
    }
    if(!stale){
        if(cause!="cancel"){history(w,"Undo");expected.undo(expected.revision());check(same(w.host.session,expected),"Weight Undo is separate from size");}
        if(pending){history(w,"Undo");expected.undo(expected.revision());check(same(w.host.session,expected),"Second Undo restores size");history(w,"Redo");expected.redo(expected.revision());check(same(w.host.session,expected),"Size Redo is independent");}
        if(cause!="cancel"){history(w,"Redo");expected.redo(expected.revision());check(same(w.host.session,expected),"Weight Redo retains full authored state");}cold(w,expected);
    }
    w.host.changed={};w.hide();events();
}
inline int run(bool discovery,bool pending_debug=false){
    int cases=0,failures=0;
    const auto test=[&](const std::string& action,const std::string& phase,const std::string& cause,bool pending=false){++cases;
        std::cout<<"WeightCase "<<action<<" "<<phase<<" "<<cause<<" pending="<<pending<<std::endl;
        try{context(action,phase,cause,pending);}catch(const std::exception& e){++failures;std::cerr<<"WEIGHT_WINDOW_RED "<<action<<" "<<phase<<" "<<cause<<" pending="<<pending<<": "<<e.what()<<std::endl;}};
    if(pending_debug){test("spin","apply","valid",true);test("spin","apply","body",true);test("literal","apply","valid",true);test("literal","entry","body",true);}
    else if(discovery){
        for(const auto* action:{"spin","literal","link","replace","expression","replace-expression","unlink"})test(action,"entry","body");
        for(const auto* action:{"spin","literal","link","replace","expression","replace-expression","unlink"})test(action,"apply","body");
        test("link","apply","chosen-weight");test("replace","apply","chosen-body");test("link","entry","body",true);
    }else{
        for(const auto* action:{"spin","literal","link","replace","expression","replace-expression","unlink"}){
            for(const auto* cause:{"body","composition"})test(action,"entry",cause);
            for(const auto* cause:{"valid","body","style","weight","driver","source-id","attachment","composition","equivalent","path","unrelated","refresh"}){
                if((std::string(action)=="spin"||std::string(action)=="literal")&&std::string(cause)=="refresh")continue;
                test(action,"apply",cause);
            }
            test(action,"apply","valid",true);test(action,"entry","body",true);test(action,"apply","body",true);
            if(std::string(action)!="spin"&&std::string(action)!="literal"&&std::string(action)!="unlink")test(action,"apply","cancel",true);
        }
        for(const auto* action:{"link","replace"})for(const auto* cause:{"chosen-weight","chosen-body","unrelated-source"})test(action,"apply",cause);
        for(const auto* cause:{"document","session","revision","preview","generation","selection","selection-return","scope"}){test("link","apply",cause);test("expression","apply",cause);test("spin","apply",cause);}
        test("replace","apply","linked-weight");test("replace-expression","apply","linked-weight");test("unlink","apply","linked-weight");
    }
    std::cout<<"WeightWindow cases="<<cases<<" checks="<<checks<<" failures="<<failures<<" physical OS input NOT_RUN"<<std::endl;return failures?1:0;
}
}

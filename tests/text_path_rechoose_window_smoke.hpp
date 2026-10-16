#pragma once
#include <QAbstractItemView>
#include <QSettings>

// Current-item pointer activation is distinct from toggling the combo index.
// Complete expected documents use canonical commands, with isolated settings.
namespace text_path_rechoose_window_smoke {
using text_path_batch_window_smoke::check;
using text_path_batch_window_smoke::events;
using text_path_batch_window_smoke::same;
inline Document fixture(bool attached) {
    auto d=text_path_single_window_smoke::fixture(attached);
    d.id="single-path-rechoose-document";
    auto& text=*d.objects.at("second").text;
    text.content="Retained 日本語\t\xC2\xA0" "body";
    text.locale="ja-JP";text.direction="horizontal";text.layout="auto";
    text.parameters.at("tracking").expression=Expression{"2",1};
    d.objects.at("source").text->content="Source 日本語\t\xC2\xA0" "body";
    for(auto& contour:d.objects.at("path").contours)contour.points.back().x.literal=6000;
    check(decode(encode(d))==d,"Complete independent single-line Path fixture native-validates");
    return d;
}
inline void point(Window& w,QWidget* control) {
    check(control,"Original production control still exists");
    const auto pos=control->mapTo(&w,control->rect().center());
    check(w.childAt(pos)==control,"Window pointer hits exact original production control");
    QTest::mouseClick(w.windowHandle(),Qt::LeftButton,Qt::NoModifier,pos);events();
}
inline void context(const std::string& action,const std::string& cause="current",bool pending=false) {
    const auto d=fixture(action!="attach");
    Session independent(d);auto authored=independent.document();
    auto& proposed=*authored.objects.at("second").text;
    if(action=="detach")proposed.path_attachment.reset();
    else proposed.path_attachment=TextPathAttachment{"path","contour","distance",1100,3.5,false};
    independent.apply({UpdateText{"second",proposed}},independent.revision());
    check(independent.document()==authored&&decode(encode(authored))==authored,
        "Independent complete attachment command native-validates before Window");
    QTemporaryDir scratch;check(scratch.isValid(),"Current-item Path Window owns scratch");
    QSettings settings(scratch.filePath("settings.ini"),QSettings::IniFormat);
    Window w(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(settings),&settings);
    w.resize(1400,900);w.host.session=Session(d);w.host.session_id="current-item-path-session";
    w.host.edited();w.show();w.activateWindow();events();
    w.canvas->set_active_artboard("composition","artboard",false);w.canvas->set_selection("second");events();
    auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Production Properties exists");dock->show();dock->raise();events();
    auto* area=w.findChild<QScrollArea*>("inspector-scroll");
    QPointer<QComboBox> choice=w.findChild<QComboBox*>("text-path-contour");
    QPointer<QLineEdit> start=w.findChild<QLineEdit*>("text-path-start"),spacing=w.findChild<QLineEdit*>("text-path-spacing");
    QPointer<QPushButton> button=w.findChild<QPushButton*>(action=="detach"?"text-path-detach":"text-path-apply");
    check(area&&choice&&start&&spacing&&button,"Original existing single Path controls exist");
    const int index=choice->findData("path\ncontour");check(index>0,"Exact stable Path/Contour exists");
    choice->setCurrentIndex(index);start->setText("1100");spacing->setText("3.5");events();
    auto incoming=d;incoming.objects.at("path").name="Changed full chosen Path";
    incoming.objects.at("path").contours.front().points.front().y.literal=110;
    if(cause=="whole-path")incoming.objects.at("path").contours.back().points.back().y.literal=215;
    else if(cause=="target")incoming.objects.at("second").name="Incoming whole target";
    else if(cause=="membership"){
        auto& roots=incoming.compositions.front().roots;roots.erase(std::remove(roots.begin(),roots.end(),"path"),roots.end());
        incoming.compositions.back().roots.push_back("path");
    }else if(cause=="document")incoming.id="incoming-current-item-document";
    check(decode(encode(incoming))==incoming,"Changed complete chosen Path native-validates");
    Session expected(incoming);auto complete=incoming;auto next=*complete.objects.at("second").text;
    if(action=="detach")next.path_attachment.reset();
    else next.path_attachment=TextPathAttachment{"path","contour","distance",1100,3.5,false};
    complete.objects.at("second").text=next;
    Session oracle(incoming);
    if(cause=="membership"){
        const Session before=oracle;bool rejected=false;
        try{oracle.apply({UpdateText{"second",next}},oracle.revision());}
        catch(const Error& e){check(e.code=="CROSS_COMPOSITION","Independent canonical attachment reports exact Composition refusal");rejected=true;}
        check(rejected&&same(oracle,before),"Native-valid incoming membership refuses incompatible attachment atomically before GUI");
    }else{
        oracle.apply({UpdateText{"second",next}},oracle.revision());
        check(oracle.document()==complete&&decode(encode(complete))==complete,
            "Independent incoming full Path/target command native-validates");
    }
    w.host.session=expected;
    if(cause=="session")w.host.session_id+="-incoming";
    else if(cause=="revision")w.host.session.apply({Rename{"source","Incoming revision"}},w.host.session.revision());
    else if(cause=="generation"||cause=="preview"){
        w.host.session.begin_gesture(w.host.session.revision());w.host.session.update_gesture({Rename{"source","Incoming preview"}});
        if(cause=="generation")w.host.session.cancel_gesture();
    }else if(cause=="selection"||cause=="selection-return"){
        w.canvas->set_selection("path");if(cause=="selection-return")w.canvas->set_selection("second");events();
    }else if(cause=="scope"){w.canvas->set_active_artboard("other-composition","other-artboard",false);events();}
    else if(cause=="refresh"){w.host.edited();events();}
    expected=w.host.session;const auto selected=w.canvas->selections();
    const auto composition=w.canvas->active_composition(),artboard=w.canvas->active_artboard();
    if(cause=="selection"||cause=="selection-return"||cause=="scope"||cause=="refresh"){
        check(!choice&&!button&&!start&&!spacing,"QPointer proves original chooser/actions expired after external context change");
        check(same(w.host.session,expected)&&w.canvas->selections()==selected&&w.canvas->active_composition()==composition&&w.canvas->active_artboard()==artboard,
            "Expired original controls cannot retarget or consume incoming Session/context");
        std::cout<<"PATH_RECHOOSE_EXPIRED "<<action<<" "<<cause<<" native=PASS original-controls=expired"<<std::endl;
        w.host.changed={};w.hide();events();return;
    }
    int activated=0,changed=0,activatedIndex=-1;
    QObject::connect(choice,qOverload<int>(&QComboBox::activated),&w,[&](int i){activatedIndex=i;++activated;});
    QObject::connect(choice,qOverload<int>(&QComboBox::currentIndexChanged),&w,[&](int){++changed;});
    area->ensureWidgetVisible(choice);events();point(w,choice);
    check(choice&&choice->view()->isVisible(),"Actual chooser pointer opens original popup");
    auto* view=choice->view();const auto item=view->model()->index(index,0);view->scrollTo(item);events();
    const auto rect=view->visualRect(item);check(rect.isValid()&&view->viewport()->rect().contains(rect.center()),"Exact current item is a visible pointer target");
    auto* popup=view->window();const auto hit=view->viewport()->mapTo(popup,rect.center());
    check(popup->childAt(hit)==view->viewport(),"Native popup pointer hits exact selected-item viewport");
    QTest::qWait(QApplication::doubleClickInterval()+20);
    QTest::mouseMove(popup->windowHandle(),hit);events();
    QTest::mouseClick(popup->windowHandle(),Qt::LeftButton,Qt::NoModifier,hit);events();
    std::cout<<"PATH_RECHOOSE_INPUT "<<action<<" activated="<<activated<<" changed="<<changed
        <<" index="<<(choice?choice->currentIndex():-1)<<" expected="<<index<<" popup="<<view->isVisible()<<std::endl;
    check(choice&&activated==1&&activatedIndex==index&&changed==0&&choice->currentIndex()==index,
        "Actual pointer rechooses same current item without changing index");
    check(same(w.host.session,expected),"Current-item source rechoose is complete-Session-neutral");
    check(w.canvas->selections()==selected&&w.canvas->active_composition()==composition&&w.canvas->active_artboard()==artboard,
        "Current-item choice preserves exact target and scope");
    std::cout<<"PATH_RECHOOSE_QUALIFIED "<<action<<" "<<cause<<" pending="<<pending<<" native=PASS activated="<<activated<<" changed="<<changed<<std::endl;
    if(cause=="path-after"){
        auto after=expected.document();after.objects.at("path").contours.back().points.front().x.literal=25;
        check(decode(encode(after))==after,"Post-choice whole Path replacement native-validates");
        w.host.session=Session(after);expected=w.host.session;
    }
    QPointer<QLineEdit> scalar;
    if(pending){for(auto* input:w.findChildren<QLineEdit*>()){
        const auto ref=QJsonDocument::fromJson(input->property("nect-reference").toByteArray()).object();
        if(input->isVisible()&&ref.value("object")=="second"&&ref.value("field")=="text.font_size")scalar=input;
    }
        check(scalar,"Independent pending Font size exists");area->ensureWidgetVisible(scalar);events();point(w,scalar);
        QTest::keyClick(scalar,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(scalar,"64");events();
        check(scalar&&scalar->hasFocus()&&scalar->isModified()&&same(w.host.session,expected),"Pending Font size is complete-Session-neutral after rechoose");
    }
    const bool refusal=cause=="target"||cause=="membership"||cause=="document"||cause=="session"||cause=="revision"||cause=="generation"||cause=="preview"||
        (cause=="path-after"&&action!="detach");
    oracle=expected;complete=expected.document();
    if(!refusal){
        if(pending){complete.objects.at("second").text->parameters.at("font_size").literal=64;
            oracle.apply({EditProperties{{{"second","","text.font_size"}},64,false}},oracle.revision());
            check(oracle.document()==complete,"Independent Font size command matches full authored Document");}
        next=*complete.objects.at("second").text;
        if(action=="detach")next.path_attachment.reset();
        else next.path_attachment=TextPathAttachment{"path","contour","distance",1100,3.5,false};
        complete.objects.at("second").text=next;oracle.apply({UpdateText{"second",next}},oracle.revision());
        check(oracle.document()==complete&&decode(encode(complete))==complete,"Independent scalar/attachment result native-validates before first action");
    }
    check(button,"Original action survives qualified source rechoose");area->ensureWidgetVisible(button);events();point(w,button);
    std::cout<<"PATH_RECHOOSE_RESULT "<<action<<" "<<cause<<" pending="<<pending<<" revision="<<w.host.session.revision()<<" expected="<<oracle.revision()
        <<" status="<<w.statusBar()->currentMessage().toStdString()<<std::endl;
    if(refusal){
        check(same(w.host.session,expected),"New activated route refuses stale target/Path/context before consuming attachment/scalar");
        const auto code=cause=="document"||cause=="session"?"SESSION_CONFLICT":cause=="revision"||cause=="generation"?"REVISION_CONFLICT":
            cause=="preview"?"GESTURE_ACTIVE":cause=="membership"?"SELECTION_CONFLICT":"PROPERTY_CONFLICT";
        check(w.statusBar()->currentMessage().startsWith(code),"Qualified new activation reports exact conflict status");
        check(choice&&button&&choice->currentData().toString()=="path\ncontour"&&w.canvas->selections()==selected&&
            w.canvas->active_composition()==composition&&w.canvas->active_artboard()==artboard,"Refusal retains original draft and exact external context");
        if(pending)check(scalar&&scalar->isModified()&&scalar->text()=="64","Refusal preserves pending Font size draft");
        w.host.changed={};w.hide();events();return;
    }
    check(same(w.host.session,oracle),"Qualified same-item rechoose accepts full current Path and canonical attachment intent");
    check(w.canvas->selections()==selected&&w.canvas->active_composition()==composition&&w.canvas->active_artboard()==artboard,
        "Attachment completion preserves exact target and scope");
    text_path_batch_window_smoke::history(w,"Undo");oracle.undo(oracle.revision());
    check(same(w.host.session,oracle),"Attachment Undo preserves complete changed chosen Path/raw Text/Refs");
    if(pending){text_path_batch_window_smoke::history(w,"Undo");oracle.undo(oracle.revision());
        check(same(w.host.session,oracle),"Font size has its own second Undo");}
    check(oracle.document()==expected.document()&&oracle.preview_document()==expected.document()&&encode(oracle.document())==encode(expected.document()),
        "Separate Undo restores complete incoming authored state; canonical Session comparison retains real revision/history");
    if(pending){text_path_batch_window_smoke::history(w,"Redo");oracle.redo(oracle.revision());
        check(same(w.host.session,oracle),"Font size has its own first Redo");}
    text_path_batch_window_smoke::history(w,"Redo");oracle.redo(oracle.revision());
    check(same(w.host.session,oracle),"Attachment Redo restores complete canonical Session");
    const auto native=scratch.filePath("result.nect");w.host.save(native);w.host.flush();
    check(same(w.host.session,oracle),"Native save preserves complete live canonical Session");
    Window reopened(scratch.filePath("cold-recovery"),std::make_unique<FolderLibrary>(settings),&settings);
    reopened.setAttribute(Qt::WA_DontShowOnScreen);reopened.resize(1400,900);reopened.host.open(native);reopened.show();events();
    check(reopened.host.session.document()==oracle.document()&&encode(reopened.host.session.document())==encode(oracle.document()),
        "Isolated cold Window preserves complete raw Text/source/styles/Refs/Path/Composition/attachment");
    reopened.host.changed={};reopened.hide();events();
    w.host.changed={};w.hide();events();
}
inline void run() {
    int cases=0,failures=0;const int before=text_path_batch_window_smoke::checks;
    const auto measure=[&](const char* action,const char* cause,bool pending=false){++cases;try{context(action,cause,pending);}catch(const std::exception& e){++failures;std::cerr<<"PATH_RECHOOSE_RED "<<action<<" "<<cause<<" pending="<<pending<<": "<<e.what()<<std::endl;}};
    for(const auto* action:{"attach","update","detach"}){measure(action,"current");measure(action,"current",true);measure(action,"path-after");}
    for(const auto* cause:{"whole-path","target","membership","document","session","revision","generation","preview","selection","selection-return","scope","refresh"})measure("attach",cause);
    measure("attach","target",true);measure("attach","path-after",true);
    std::cout<<"PathRechoose Windows cases="<<cases<<" checks="<<text_path_batch_window_smoke::checks-before<<" failures="<<failures<<" physical OS input NOT_RUN"<<std::endl;
    if(failures)throw std::runtime_error("Current-item single Path discovery failed");
}
}

#pragma once

// Pending-before-chooser focus is distinct from the qualified current-item rechoose.
namespace text_path_pending_chooser_window_smoke {
using text_path_batch_window_smoke::check;
using text_path_batch_window_smoke::events;
using text_path_batch_window_smoke::same;
struct PendingCleanup {
    Window& window;QPointer<QLineEdit>& scalar;
    ~PendingCleanup(){if(scalar)scalar->setModified(false);window.host.changed={};window.hide();events();}
};
inline void context(bool attached,const std::string& cause) {
    std::cout<<"PATH_PENDING_BEGIN attached="<<attached<<" cause="<<cause<<std::endl;
    auto d=text_path_rechoose_window_smoke::fixture(attached);
    d.id="pending-before-path-chooser-document";
    Session initial(d),scalar(d),attachment(d);
    auto scalar_document=d;scalar_document.objects.at("second").text->parameters.at("font_size").literal=64;
    scalar.apply({EditProperties{{{"second","","text.font_size"}},64,false}},scalar.revision());
    check(scalar.document()==scalar_document&&decode(encode(scalar_document))==scalar_document,
        "Independent complete scalar intent native-validates before Window");
    auto complete=scalar_document;auto text=*complete.objects.at("second").text;
    text.path_attachment=TextPathAttachment{"path",cause=="alternate"?"second-contour":"contour","distance",1100,3.5,false};
    complete.objects.at("second").text=text;attachment=scalar;
    attachment.apply({UpdateText{"second",text}},attachment.revision());
    check(attachment.document()==complete&&decode(encode(complete))==complete,
        "Independent complete scalar then attachment native-validates before Window");
    auto qualified_incoming=d;
    if(cause=="target")qualified_incoming.objects.at("second").name="Incoming whole target";
    else if(cause=="document")qualified_incoming.id="pending-incoming-document";
    else if(cause=="path-before"){qualified_incoming.objects.at("path").name="Full current chosen Path";qualified_incoming.objects.at("path").contours.back().points.back().y.literal=240;}
    check(decode(encode(qualified_incoming))==qualified_incoming,"Complete incoming fixture native-validates BEFORE production Window");
    QTemporaryDir scratch;check(scratch.isValid(),"Pending chooser owns isolated scratch");
    QSettings settings(scratch.filePath("settings.ini"),QSettings::IniFormat);
    Window w(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(settings),&settings);
    w.resize(1400,900);w.host.session=initial;w.host.session_id="pending-before-path-chooser-session";
    w.host.edited();w.show();w.activateWindow();events();
    w.canvas->set_active_artboard("composition","artboard",false);w.canvas->set_selection("second");events();
    auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Production Properties exists");dock->show();dock->raise();events();
    auto* area=w.findChild<QScrollArea*>("inspector-scroll");
    QPointer<QComboBox> choice=w.findChild<QComboBox*>("text-path-contour");
    QPointer<QPushButton> button=w.findChild<QPushButton*>("text-path-apply");
    QPointer<QLineEdit> start=w.findChild<QLineEdit*>("text-path-start"),spacing=w.findChild<QLineEdit*>("text-path-spacing"),scalar_control;
    for(auto* input:w.findChildren<QLineEdit*>()) {
        const auto ref=QJsonDocument::fromJson(input->property("nect-reference").toByteArray()).object();
        if(input->isVisible()&&ref.value("object")=="second"&&ref.value("field")=="text.font_size")scalar_control=input;
    }
    check(area&&choice&&button&&start&&spacing&&scalar_control,"Original same-target production chooser/scalar/actions exist");
    int activated=0,changed=0;
    PendingCleanup cleanup{w,scalar_control};
    const auto selected=w.canvas->selections();const auto composition=w.canvas->active_composition(),artboard=w.canvas->active_artboard();
    const int index=choice->findData(cause=="alternate"?"path\nsecond-contour":"path\ncontour");
    check(index>0,"Exact stable Path/Contour available before pointer");
    start->setText("1100");spacing->setText("3.5");
    area->ensureWidgetVisible(scalar_control);events();text_path_rechoose_window_smoke::point(w,scalar_control);
    const auto draft=cause=="invalid"?"not-a-number":"64";
    scalar_control->selectAll();QTest::keyClicks(scalar_control,draft);events();
    check(scalar_control&&scalar_control->hasFocus()&&scalar_control->isModified()&&same(w.host.session,initial),
        "Pending modified Font size stays complete-Session-neutral before chooser focus");
    area->ensureWidgetVisible(choice);events();
    check(choice&&scalar_control&&scalar_control->hasFocus()&&same(w.host.session,initial),
        "Scroll preserves pending scalar and exact original chooser before FIRST pointer");
    const bool refuses_open=cause=="target"||cause=="document"||cause=="session"||cause=="revision"||cause=="generation"||cause=="preview"||cause=="invalid";
    if(cause=="target"||cause=="document"||cause=="path-before") {
        auto incoming=d;
        if(cause=="target")incoming.objects.at("second").name="Incoming whole target";
        else if(cause=="document")incoming.id=qualified_incoming.id;
        else {incoming.objects.at("path").name="Full current chosen Path";incoming.objects.at("path").contours.back().points.back().y.literal=240;}
        check(decode(encode(incoming))==incoming,"Incoming full native fixture validates before first pointer");
        w.host.session=Session(incoming);
    } else if(cause=="session")w.host.session_id+="-incoming";
    else if(cause=="revision")w.host.session.apply({Rename{"source","Incoming revision"}},w.host.session.revision());
    else if(cause=="generation"||cause=="preview") {
        w.host.session.begin_gesture(w.host.session.revision());w.host.session.update_gesture({Rename{"source","Incoming preview"}});
        if(cause=="generation")w.host.session.cancel_gesture();
    }
    std::cout<<"PATH_PENDING_INCOMING cause="<<cause<<std::endl;
    initial=w.host.session;scalar=initial;
    if(!refuses_open) {
        auto scalar_complete=initial.document();scalar_complete.objects.at("second").text->parameters.at("font_size").literal=64;
        scalar.apply({EditProperties{{{"second","","text.font_size"}},64,false}},scalar.revision());
        check(scalar.document()==scalar_complete&&decode(encode(scalar_complete))==scalar_complete,
            "Complete current incoming own scalar native-validates before first pointer");
    }
    const auto pos=choice->mapTo(&w,choice->rect().center());
    check(w.childAt(pos)==choice,"FIRST Window pointer hits exact original Source contour");
    QTest::mouseMove(w.windowHandle(),pos);events();QTest::qWait(QApplication::doubleClickInterval()+20);
    QObject::connect(choice,qOverload<int>(&QComboBox::activated),&w,[&](int){++activated;});
    QObject::connect(choice,qOverload<int>(&QComboBox::currentIndexChanged),&w,[&](int){++changed;});
    QTest::mousePress(w.windowHandle(),Qt::LeftButton,Qt::NoModifier,pos);
    std::cout<<"PATH_PENDING_PRESS attached="<<attached<<" cause="<<cause<<" revision="<<w.host.session.revision()
        <<" scalar_expected="<<scalar.revision()<<" original_choice="<<bool(choice)<<" original_scalar="<<bool(scalar_control)
        <<" original_button="<<bool(button)<<" popup="<<(choice&&choice->view()->isVisible())
        <<" activated="<<activated<<" changed="<<changed<<" status="<<w.statusBar()->currentMessage().toStdString()<<std::endl;
    events();QTest::mouseRelease(w.windowHandle(),Qt::LeftButton,Qt::NoModifier,pos);events();
    if(refuses_open) {
        const auto code=cause=="document"||cause=="session"?"SESSION_CONFLICT":cause=="revision"||cause=="generation"?"REVISION_CONFLICT":
            cause=="preview"?"GESTURE_ACTIVE":cause=="invalid"?"INVALID_VALUE":"PROPERTY_CONFLICT";
        check(same(w.host.session,initial),"Stale target/context or invalid scalar refuses BEFORE consuming any authored state/history");
        check(choice&&button&&scalar_control&&!choice->view()->isVisible()&&scalar_control->text()==draft,
            "Open refusal retains original chooser/action and exact pending draft without popup");
        check(w.statusBar()->currentMessage().startsWith(code),"Open refusal reports exact guarded status");
        check(w.canvas->selections()==selected&&w.canvas->active_composition()==composition&&w.canvas->active_artboard()==artboard,
            "Open refusal preserves exact incoming selection/scope");
        std::cout<<"PATH_PENDING_REFUSED "<<cause<<" native=PASS unchanged=1 status="<<w.statusBar()->currentMessage().toStdString()<<std::endl;
        return;
    }
    check(same(w.host.session,scalar),"FIRST chooser pointer completes ONLY canonical own Font size scalar");
    check(w.canvas->selections()==selected&&w.canvas->active_composition()==composition&&w.canvas->active_artboard()==artboard,
        "FIRST chooser pointer preserves exact target and scope");
    check(choice&&button&&scalar_control&&choice->view()->isVisible(),
        "FIRST chooser pointer keeps original production controls and opens real popup");
    check(!scalar_control->isModified()&&scalar_control->text()=="64"&&activated==0&&changed==0,
        "Scalar completes once; popup opening is source-neutral");
    QPointer<QAbstractItemView> view=choice->view();QPointer<QWidget> popup=view->window();
    if(cause=="selection-return") {
        w.canvas->set_selection("path");w.canvas->set_selection("second");events();
        check(!choice&&!button&&!scalar_control&&!start&&!spacing,"Original controls really expire after external selection/return");
        check(same(w.host.session,scalar)&&w.canvas->selections()==selected,"Expired controls cannot consume another edit or retarget source");
        return;
    }
    const auto rearm=[&] {
        check(!choice&&!button&&!scalar_control&&!start&&!spacing,
            "QPointer proves prepared original controls expire after verified popup completion");
        choice=w.findChild<QComboBox*>("text-path-contour");button=w.findChild<QPushButton*>("text-path-apply");
        start=w.findChild<QLineEdit*>("text-path-start");spacing=w.findChild<QLineEdit*>("text-path-spacing");
        for(auto* input:w.findChildren<QLineEdit*>()) {
            const auto ref=QJsonDocument::fromJson(input->property("nect-reference").toByteArray()).object();
            if(input->isVisible()&&ref.value("object")=="second"&&ref.value("field")=="text.font_size")scalar_control=input;
        }
        check(choice&&button&&scalar_control&&start&&spacing,"Exact target source/scalar controls rearm after popup");
        check(start->text()=="1100"&&spacing->text()=="3.5"&&scalar_control->text()=="64"&&same(w.host.session,scalar),
            "Verified refresh preserves exact raw drafts and full canonical scalar Session without an authored edit");
    };
    if(cause=="cancel") {
        QTest::keyClick(popup->windowHandle(),Qt::Key_Escape);events();rearm();
        check(same(w.host.session,scalar)&&choice&&!choice->view()->isVisible()&&activated==0&&changed==0,
            "Actual popup Escape keeps only scalar history; attachment/source remain neutral");
    } else {
        const int prior_index=choice->currentIndex();const auto item=view->model()->index(index,0);view->scrollTo(item);events();
        const auto rect=view->visualRect(item);check(rect.isValid()&&view->viewport()->rect().contains(rect.center()),"Exact stable contour is visible pointer target");
        const auto hit=view->viewport()->mapTo(popup,rect.center());check(popup->childAt(hit)==view->viewport(),"Native popup hits exact contour viewport");
        QTest::qWait(QApplication::doubleClickInterval()+20);QTest::mouseMove(popup->windowHandle(),hit);events();
        QTest::mouseClick(popup->windowHandle(),Qt::LeftButton,Qt::NoModifier,hit);events();rearm();
        check(choice&&activated==1&&changed==(prior_index==index?0:1)&&choice->currentIndex()==index&&!choice->view()->isVisible(),
            "Actual contour choice has exact activation/index semantics after first scalar completion");
        check(same(w.host.session,scalar)&&start&&spacing&&start->text()=="1100"&&spacing->text()=="3.5",
            "Contour choice preserves full canonical scalar Session and original numeric drafts");
        if(cause=="scalar-again") {
            check(scalar_control,"Current Font size field exists after source choice");
            area->ensureWidgetVisible(scalar_control);events();text_path_rechoose_window_smoke::point(w,scalar_control);
            scalar_control->selectAll();QTest::keyClicks(scalar_control,"72");events();
            check(scalar_control&&scalar_control->isModified()&&same(w.host.session,scalar),
                "Another ordinary Font size draft remains neutral after chooser completion");
            scalar.apply({EditProperties{{{"second","","text.font_size"}},72,false}},scalar.revision());
            check(decode(encode(scalar.document()))==scalar.document(),"Independent second scalar native-validates before attachment");
        }
        const bool refuses_attach=cause=="path-after"||cause=="membership-after";
        if(refuses_attach) {
            auto incoming=d;
            if(cause=="path-after"){incoming.objects.at("path").name="Incoming after choice";incoming.objects.at("path").contours.back().points.back().y.literal=250;}
            else {auto& roots=incoming.compositions.front().roots;roots.erase(std::remove(roots.begin(),roots.end(),"path"),roots.end());incoming.compositions.back().roots.push_back("path");}
            check(decode(encode(incoming))==incoming,"Independent after-choice full source native-validates");
            Session replacement(incoming);replacement.apply({EditProperties{{{"second","","text.font_size"}},64,false}},replacement.revision());
            check(replacement.revision()==scalar.revision()&&decode(encode(replacement.document()))==replacement.document(),
                "Incoming after-choice replacement retains exact scalar revision with full independent history");
            w.host.session=replacement;scalar=replacement;
        }
        Session expected=scalar;auto expected_document=scalar.document();auto next=*expected_document.objects.at("second").text;
        next.path_attachment=TextPathAttachment{"path",cause=="alternate"?"second-contour":"contour","distance",1100,3.5,false};
        expected_document.objects.at("second").text=next;
        if(!refuses_attach) {
            expected.apply({UpdateText{"second",next}},expected.revision());
            check(expected.document()==expected_document&&decode(encode(expected_document))==expected_document,
                "Chosen current full Path attachment native-validates independently before action");
        } else if(cause=="membership-after") {
            Session negative=scalar;bool refused=false;
            try{negative.apply({UpdateText{"second",next}},negative.revision());}
            catch(const Error& e){refused=e.code=="CROSS_COMPOSITION";}
            check(refused&&same(negative,scalar),"Independent canonical membership oracle refuses atomic incompatible attachment");
        }
        check(button,"Original attach action survives chooser interaction");area->ensureWidgetVisible(button);events();text_path_rechoose_window_smoke::point(w,button);
        if(refuses_attach) {
            const auto code=cause=="path-after"?"PROPERTY_CONFLICT":"SELECTION_CONFLICT";
            check(same(w.host.session,scalar)&&w.statusBar()->currentMessage().startsWith(code),
                "Changed chosen full source refuses attachment atomically after its independent scalar step");
            check(choice&&button&&start&&spacing&&choice->currentIndex()==index&&start->text()=="1100"&&spacing->text()=="3.5",
                "Attachment refusal keeps original stable source and numeric drafts");
            return;
        }
        check(same(w.host.session,expected),"Attach/update is its own atomic canonical history step after neutral choice");
        text_path_batch_window_smoke::history(w,"Undo");expected.undo(expected.revision());
        check(same(w.host.session,expected)&&expected.document()==scalar.document(),"Attachment Undo restores complete scalar-only authored state");
        scalar=expected;
    }
    if(cause=="scalar-again"){text_path_batch_window_smoke::history(w,"Undo");scalar.undo(scalar.revision());
        check(same(w.host.session,scalar),"Second scalar has its own complete Undo");}
    text_path_batch_window_smoke::history(w,"Undo");scalar.undo(scalar.revision());
    check(same(w.host.session,scalar)&&scalar.document()==initial.document(),"Separate scalar Undo restores complete incoming raw Text/style/Refs/Path/scope");
    text_path_batch_window_smoke::history(w,"Redo");scalar.redo(scalar.revision());
    check(same(w.host.session,scalar),"Scalar Redo restores independent complete canonical Session");
    if(cause=="scalar-again"){text_path_batch_window_smoke::history(w,"Redo");scalar.redo(scalar.revision());
        check(same(w.host.session,scalar),"Second scalar has its own complete Redo");}
    if(cause!="cancel") {
        text_path_batch_window_smoke::history(w,"Redo");scalar.redo(scalar.revision());
        check(same(w.host.session,scalar),"Attachment Redo is separate from scalar Redo");
    }
    const auto native=scratch.filePath("result.nect");w.host.save(native);w.host.flush();
    check(same(w.host.session,scalar),"Native save preserves complete live canonical Session/history");
    Window reopened(scratch.filePath("cold-recovery"),std::make_unique<FolderLibrary>(settings),&settings);
    reopened.setAttribute(Qt::WA_DontShowOnScreen);reopened.resize(1400,900);reopened.host.open(native);reopened.show();events();
    check(reopened.host.session.document()==scalar.document()&&encode(reopened.host.session.document())==encode(scalar.document()),
        "Isolated cold native Window preserves complete raw Text/Refs/style/Path/Contour/Composition/attachment");
    check(w.canvas->selections()==selected&&w.canvas->active_composition()==composition&&w.canvas->active_artboard()==artboard,
        "History/native path retains exact target and scope");
    std::cout<<"PATH_PENDING_PASS attached="<<attached<<" cause="<<cause<<" activated="<<activated<<" changed="<<changed<<" native=PASS"<<std::endl;
    reopened.host.changed={};reopened.hide();events();w.host.changed={};w.hide();events();
}
inline void run(const std::string& only={}) {
    const int before=text_path_batch_window_smoke::checks;int failures=0,cases=0;
    const auto measure=[&](bool attached,const char* cause){++cases;try{context(attached,cause);}catch(const std::exception& e){++failures;std::cerr<<"PATH_PENDING_RED attached="<<attached<<" "<<cause<<": "<<e.what()<<std::endl;}};
    if(!only.empty()){measure(only=="attached",only=="attached"?"current":only.c_str());
        std::cout<<"PathPending isolated="<<only<<" cases="<<cases<<" checks="<<text_path_batch_window_smoke::checks-before<<" failures="<<failures<<std::endl;
        if(failures)throw std::runtime_error("Pending isolated acceptance failed");return;}
    measure(false,"current");measure(true,"current");measure(false,"alternate");measure(false,"path-before");measure(false,"scalar-again");
    measure(false,"cancel");measure(true,"cancel");
    for(const auto* cause:{"target","document","session","revision","generation","preview","invalid","path-after","membership-after","selection-return"})measure(false,cause);
    std::cout<<"PathPending Windows cases="<<cases<<" checks="<<text_path_batch_window_smoke::checks-before
        <<" failures="<<failures<<" physical OS input NOT_RUN"<<std::endl;
    if(failures)throw std::runtime_error("Pending-before-chooser focused acceptance failed");
}
}
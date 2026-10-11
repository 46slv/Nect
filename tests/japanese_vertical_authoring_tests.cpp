#include "window.hpp"
#include "visual_style.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QAbstractItemView>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QDir>
#include <QFile>
#include <QInputMethodEvent>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QWindow>
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);++checks;}
void events(){QApplication::processEvents();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QTest::qWait(20);}
const std::string body="「日本。、」ABC12\n（文章）123";
Document fixture(){
    auto d=empty_document("japanese-authoring-document","composition","artboard");
    d.compositions.front().artboards.front().width=960;
    d.compositions.front().artboards.front().height=1000;
    Object text;text.id="text";text.name="Editable Japanese specimen";text.kind=Kind::text;
    text.text=default_text("text-source","「日本。、」XYZ34\n（文章）567");
    auto& t=*text.text;t.family="Yu Gothic";t.locale="ja-JP";t.weight=400;t.italic=false;
    t.direction="horizontal";t.layout="frame";t.alignment="start";
    t.parameters.at("font_size").literal=24;t.parameters.at("frame_width").literal=600;
    t.parameters.at("frame_height").literal=900;t.parameters.at("origin_x").literal=17.25;
    t.parameters.at("origin_y").literal=-11.5;
    text.stack={default_operation("text-fill","nect.paint.fill")};
    Object path;path.id="sentinel";path.name="Unrelated authored Path";path.kind=Kind::path;
    Contour contour;contour.id="sentinel-contour";Point a;a.id="first";a.x.literal=50;a.y.literal=70;
    Point b;b.id="last";b.x.literal=150;b.y.literal=90;contour.points={a,b};path.contours={contour};
    d.objects.emplace(text.id,text);d.objects.emplace(path.id,path);d.compositions.front().roots={"text","sentinel"};
    auto other=empty_document("unused","other-composition","other-artboard").compositions.front();
    other.name="Preserve independent Composition";d.compositions.push_back(other);
    validate(d);check(decode(encode(d))==d,"Complete initial Document native-validates before Window");return d;
}
struct Expected {
    Document initial,vertical,edited,styled;Session start,writing,content,style;
    Expected():initial(fixture()),vertical(initial),edited(initial),styled(initial),start(initial),writing(initial),content(initial),style(initial){
        vertical.objects.at("text").text->direction="vertical";
        edited=vertical;edited.objects.at("text").text->content=body;
        styled=edited;styled.objects.at("text").text->parameters.at("font_size").literal=48;
        writing.apply({UpdateText{"text",*vertical.objects.at("text").text}},writing.revision());
        content=writing;content.apply({UpdateText{"text",*edited.objects.at("text").text}},content.revision());
        style=content;style.apply({EditProperties{{{"text","","text.font_size"}},48,false}},style.revision());
        check(writing.document()==vertical&&content.document()==edited&&style.document()==styled,
            "Independent whole Documents equal canonical direction/content/style History before Window");
        for(const auto* d:{&vertical,&edited,&styled})check(decode(encode(*d))==*d,"Every intended whole Document native-validates before Window");
    }
};
void same(const Session& actual,const Session& expected){
    check(actual.document()==expected.document()&&actual.preview_document()==expected.preview_document()&&
        encode(actual.document())==encode(expected.document())&&actual.history()==expected.history()&&
        actual.revision()==expected.revision()&&actual.gesture_active()==expected.gesture_active()&&
        actual.gesture_generation()==expected.gesture_generation(),"Complete Document/History/revision/gesture equals independent Session");
}
void layout(const Document& document){
    // Fixed Yu Gothic specimen already independently qualified by
    // japanese_vertical_oracle_tests.cpp (Renderer1 glyph/run/outline oracle).
    // These expectations precede Window and never come from the UI result.
    const auto actual=evaluate_text_projection(document,"text",evaluate(document));
    check(!actual.overflow&&actual.column_baselines_x.size()==2,"Mixed Japanese specimen retains exactly two columns without overflow");
    check(std::abs(actual.column_baselines_x[0]-578.8125)<0.003&&
        std::abs(actual.column_baselines_x[1]-500.0625)<0.003,"Qualified columns proceed from right to left at fixed independent baselines");
    check(actual.font_runs.size()==4,"Qualified mixed specimen retains four shaped runs");
    const std::array<unsigned,4> starts{0,6,12,16},lengths{6,5,4,3};
    for(std::size_t i=0;i<4;++i){const auto& r=actual.font_runs[i];
        check(r.utf16_start==starts[i]&&r.utf16_length==lengths[i]&&r.sideways==(i%2==0),
            "Japanese and Latin/digit runs retain independent range and vertical orientation");
        check(r.family=="Yu Gothic"&&r.resolved_weight==400&&r.simulations==0&&r.missing_glyph_count==0&&
            r.fallback==false&&r.font_em_size==48,"Qualified installed Yu Gothic has no fallback, synthetic style or missing glyph");
    }
    const auto& first=actual.font_runs[0].glyph_indices;const auto& second=actual.font_runs[2].glyph_indices;
    check(first.size()==6&&second.size()==4,"Japanese run glyph cardinalities preserve the qualified specimen");
    check(first[0]==7913&&first[3]==7890&&first[4]==7889&&first[5]==7914&&second[0]==7901&&second[3]==7902,
        "Brackets and punctuation retain independently qualified vertical-form glyph IDs");
}
void pointer(Window& window,QWidget* widget){
    check(widget&&widget->isVisible()&&widget->isEnabled(),"Exact production control is visible and enabled");
    if(auto* scroll=window.findChild<QScrollArea*>("inspector-scroll"))scroll->ensureWidgetVisible(widget);
    events();const auto pos=widget->mapTo(&window,widget->rect().center());
    check(window.childAt(pos)==widget,"Native Window pointer hits exact production control");
    QTest::mouseClick(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,pos);events();
}
void show(Window& window){
    window.resize(1400,1000);window.show();window.activateWindow();events();
    auto* dock=window.findChild<QDockWidget*>("properties");check(dock,"Production Properties dock exists");dock->show();dock->raise();events();
}
QByteArray bytes(const QString& path){QFile file(path);check(file.open(QIODevice::ReadOnly),"Owned file can be read");return file.readAll();}
void freeze_expected(const QString& root,const Expected& expected){
    QJsonObject receipt;
    for(const auto& [name,session]:std::array<std::pair<const char*,const Session*>,4>{
        {{"initial",&expected.start},{"writing",&expected.writing},{"content",&expected.content},{"style",&expected.style}}}){
        QFile file(root+"/expected-"+name+".nect");const auto native=encode(session->document());
        check(file.open(QIODevice::WriteOnly|QIODevice::NewOnly)&&file.write(native.data(),static_cast<qint64>(native.size()))==static_cast<qint64>(native.size()),
            "Complete independent pre-Window expected Document saved once");
        const auto h=session->history();QJsonArray states;
        for(const auto& state:h.states)states.append(QJsonObject{{"id",static_cast<qint64>(state.id)},
            {"label",QString::fromStdString(state.label)},{"estimated_bytes",static_cast<qint64>(state.estimated_bytes)}});
        receipt.insert(name,QJsonObject{{"revision",static_cast<qint64>(session->revision())},{"states",states},
            {"current_id",static_cast<qint64>(h.current_id)},{"retained_bytes",static_cast<qint64>(h.retained_bytes)},
            {"max_entries",static_cast<qint64>(h.max_entries)},{"max_bytes",static_cast<qint64>(h.max_bytes)},
            {"pruned_entries",static_cast<qint64>(h.pruned_entries)},{"gesture_generation",static_cast<qint64>(session->gesture_generation())},
            {"gesture_active",session->gesture_active()}});
    }
    QFile history(root+"/expected-history-before-window.json");const auto data=QJsonDocument(receipt).toJson();
    check(history.open(QIODevice::WriteOnly|QIODevice::NewOnly)&&history.write(data)==data.size(),
        "Complete independent canonical History for all stages saved BEFORE Window");
}
void cold(const QString& root){
    Expected expected;layout(expected.styled);QSettings settings(root+"/settings.ini",QSettings::IniFormat);
    Window window(root+"/cold-recovery",std::make_unique<FolderLibrary>(settings),&settings);
    window.host.open(root+"/authored.nect");show(window);Session reopened(expected.styled);same(window.host.session,reopened);
    window.canvas->set_active_artboard("composition","artboard",false);window.canvas->set_selection("text");events();
    const auto* direction=window.findChild<QComboBox*>("text-direction");const auto* edit=window.findChild<QPushButton*>("edit-text-content");
    check(direction&&direction->isEnabled()&&direction->currentIndex()==1&&edit&&edit->isEnabled(),
        "Separate native Window reopen retains usable Writing/content controls and exact stable object");
    layout(window.host.session.document());window.host.save(root+"/cold-resave.nect");window.host.flush();same(window.host.session,reopened);
    check(bytes(root+"/authored.nect")==bytes(root+"/cold-resave.nect"),"Separate-process resave retains byte-exact complete authored native state");
    check(settings.value("unrelated/value").toByteArray()==QByteArray("keep\0bytes",10),"Cold reopen preserves unrelated owned preferences");
}
void flow(){
    Expected expected;layout(expected.styled); // Full native-valid oracle/History BEFORE any Window.
    QTemporaryDir temporary;check(temporary.isValid(),"Flow owns settings, recovery and all native files");
    const auto capture=qEnvironmentVariable("NECT_JAPANESE_AUTHORING_ROOT");
    const auto root=capture.isEmpty()?temporary.path():capture;
    check(QDir(root).exists()&&!QFile::exists(root+"/authored.nect"),"Requested evidence directory is fresh and owned");
    freeze_expected(root,expected);
    QSettings settings(root+"/settings.ini",QSettings::IniFormat);settings.setValue("unrelated/value",QByteArray("keep\0bytes",10));settings.sync();
    Window window(root+"/recovery",std::make_unique<FolderLibrary>(settings),&settings);
    window.host.session=expected.start;window.host.edited();show(window);
    window.canvas->set_active_artboard("composition","artboard",false);window.canvas->set_selection("text");events();
    auto* direction=window.findChild<QComboBox*>("text-direction");pointer(window,direction);
    auto* view=direction->view();const auto row=view->model()->index(1,0);
    check(view->isVisible()&&!view->visualRect(row).isEmpty(),"Direct Writing control opens actual Vertical choice");
    QTest::mouseMove(view->viewport(),view->visualRect(row).center());QTest::qWait(QApplication::doubleClickInterval()+20);
    QTest::mouseClick(view->viewport(),Qt::LeftButton,Qt::NoModifier,view->visualRect(row).center());events();same(window.host.session,expected.writing);
    auto* edit=window.findChild<QPushButton*>("edit-text-content");bool handled=false;std::exception_ptr failure;
    QTimer timer,deadline;timer.setInterval(20);deadline.setSingleShot(true);
    QObject::connect(&timer,&QTimer::timeout,&window,[&]{
        auto* dialog=window.findChild<QDialog*>("text-editor-dialog");if(!dialog||!dialog->isVisible())return;timer.stop();
        try{auto* editor=dialog->findChild<QPlainTextEdit*>("text-content-editor");auto* buttons=dialog->findChild<QDialogButtonBox*>();
            check(editor&&buttons,"Existing staged body editor and Apply exist");editor->setFocus();editor->selectAll();
            QInputMethodEvent input;input.setCommitString(QString::fromStdString(body));QApplication::sendEvent(editor,&input);events();
            check(editor->toPlainText()==QString::fromStdString(body),"IME commit enters exact mixed two-column body");same(window.host.session,expected.writing);
            const auto pos=buttons->button(QDialogButtonBox::Apply)->mapTo(dialog,buttons->button(QDialogButtonBox::Apply)->rect().center());
            check(dialog->childAt(pos)==buttons->button(QDialogButtonBox::Apply),"Native dialog pointer hits Apply");
            QTest::mouseClick(dialog->windowHandle(),Qt::LeftButton,Qt::NoModifier,pos);events();same(window.host.session,expected.content);handled=true;
        }catch(...){failure=std::current_exception();}if(dialog->isVisible())dialog->reject();
    });
    QObject::connect(&deadline,&QTimer::timeout,&window,[&]{failure=std::make_exception_ptr(std::runtime_error("Body editor deadline"));
        if(auto* d=window.findChild<QDialog*>("text-editor-dialog"))d->reject();});
    timer.start();deadline.start(10000);pointer(window,edit);timer.stop();deadline.stop();if(failure)std::rethrow_exception(failure);check(handled,"Body editor completed");
    QLineEdit* size=nullptr;for(auto* input:window.findChildren<QLineEdit*>()){
        const auto ref=QJsonDocument::fromJson(input->property("nect-reference").toByteArray()).object();
        if(input->isVisible()&&ref.value("object")=="text"&&ref.value("field")=="text.font_size")size=input;
    }
    pointer(window,size);QTest::keyClick(size,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(size,"48");same(window.host.session,expected.content);
    QTest::keyClick(size,Qt::Key_Return);events();same(window.host.session,expected.style);layout(window.host.session.document());
    auto canonical=expected.style;QApplication::setActiveWindow(&window);window.canvas->setFocus();events();
    check(window.isActiveWindow()&&window.canvas->hasFocus(),"Actual native Window/Canvas owns Undo shortcut focus");
    // Undo every accepted edit in this single flow, then Redo each canonical entry.
    for(int i=0;i<3;++i){canonical.undo(canonical.revision());QTest::keySequence(window.canvas,QKeySequence::Undo);events();same(window.host.session,canonical);}
    check(window.host.session.document()==expected.initial,"Three public Undo shortcuts restore complete original horizontal source");
    for(int i=0;i<3;++i){canonical.redo(canonical.revision());QTest::keySequence(window.canvas,QKeySequence::Redo);events();same(window.host.session,canonical);}
    layout(window.host.session.document());window.host.save(root+"/authored.nect");window.host.flush();same(window.host.session,canonical);
    check(decode(bytes(root+"/authored.nect").toStdString())==expected.styled,"Independent saved full Document equals pre-Window oracle");
    events();window.canvas->fit_artboard();events();same(window.host.session,canonical);
    check(window.grab().save(root+"/authoring.png"),"Settled fitted native Window evidence captured");
    const auto nativeBefore=bytes(root+"/authored.nect");QProcess child;child.start(QCoreApplication::applicationFilePath(),{"--cold",root});
    check(child.waitForStarted(5000),"Separate native cold process starts");
    if(!child.waitForFinished(20000)){child.kill();child.waitForFinished();throw std::runtime_error("Owned cold process deadline");}
    std::cout<<child.readAllStandardOutput().toStdString();std::cerr<<child.readAllStandardError().toStdString();
    check(child.exitStatus()==QProcess::NormalExit&&child.exitCode()==0,"Separate native Host/Window cold reopen passed");
    same(window.host.session,canonical);check(bytes(root+"/authored.nect")==nativeBefore,"Cold process never alters original saved bytes");
    check(settings.value("unrelated/value").toByteArray()==QByteArray("keep\0bytes",10),"Whole flow preserves unrelated owned preferences");
}
}
int main(int argc,char** argv){
    qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);app.setStyle("Fusion");app.setStyleSheet(application_style_sheet());
    try{const auto fonts=text_fonts();
        if(std::find(fonts.begin(),fonts.end(),"Yu Gothic")==fonts.end()){
            std::cout<<"ENV_MISSING_FIXTURE: DirectWrite installed Yu Gothic required\n";return 77;}
        // Fixed glyph IDs belong to this qualified installed TTC, not every
        // version of a family with the same name. Never embed/distribute it.
        QFile font(qEnvironmentVariable("WINDIR")+"/Fonts/YuGothR.ttc");
        if(!font.open(QIODevice::ReadOnly)||QCryptographicHash::hash(font.readAll(),QCryptographicHash::Sha256).toHex()!=
            "cd24b5617c1e26572b0a7fe7e558f7121ca42c36f7b50e8d9216ac222a1c0a93"){
            std::cout<<"ENV_MISSING_FIXTURE: qualified Yu Gothic Regular TTC unavailable or changed\n";return 77;}
        std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()
            <<"; production DirectWrite font discovery confirms Yu Gothic\n";
        if(app.arguments().size()==3&&app.arguments()[1]=="--cold")cold(app.arguments()[2]);else flow();
        std::cout<<"PASS Japanese vertical native "<<(app.arguments().contains("--cold")?"cold":"authoring")<<": "<<checks<<" checks\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL Japanese vertical native authoring: "<<e.what()<<'\n';return 1;}
}

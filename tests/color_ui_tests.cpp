#include "colors.hpp"
#include "window.hpp"
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QIcon>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMimeData>
#include <QPushButton>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTest>
#include <iostream>

using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);++checks;}
template<class T>T* widget(QObject& parent,const char* name) {
    auto* result=parent.findChild<T*>(name);check(result!=nullptr,(std::string("Missing widget: ")+name).c_str());return result;
}
void action(QPushButton* button,const char* name) {
    auto* item=button->menu()->findChild<QAction*>(name);check(item!=nullptr,"Color menu exposes the requested production action");
    item->trigger();QApplication::processEvents();
}
void type(QLineEdit* input,const char* text) {
    input->setFocus();QTest::keyClick(input,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(input,text);QApplication::processEvents();
}
void click(QObject& parent,const char* name) {QTest::mouseClick(widget<QPushButton>(parent,name),Qt::LeftButton);QApplication::processEvents();}
Ref stroke_ref(const Document& document,const Id& object){return operation_ref(object,document.objects.at(object).legacy_stroke,"color");}
ColorValue color(const Document& document,const Ref& ref){return color_value(document,ref,evaluate(document));}
bool independent(const Document& document,const Ref& ref) {
    for(const auto& channel:color_channels(document,ref))if(nect::property(document,channel).binding)return false;
    return true;
}
NamedColor named(Id id,std::string name,ColorValue value) {
    NamedColor result;result.id=std::move(id);result.name=std::move(name);
    for(std::size_t i=0;i<4;++i)result.rgba[i].literal=value.rgba[i];return result;
}
void select_named(QListWidget* list,const Id& id) {
    for(int i=0;i<list->count();++i)if(list->item(i)->data(Qt::UserRole).toString().toStdString()==id) {
        list->setCurrentItem(list->item(i));QApplication::processEvents();return;
    }
    throw std::runtime_error("Named color is missing from the manager");
}
}

int main(int argc,char** argv) {
    qputenv("QT_QPA_PLATFORM","offscreen");QApplication app(argc,argv);
    try {
        QTemporaryDir recovery;Window window(recovery.path());window.show();QApplication::processEvents();
        auto* tools=dynamic_cast<ColorTools*>(window.findChild<QObject*>("color-tools"));check(tools!=nullptr,"Window owns its color tools");
        auto& session=window.host.session;
        auto apply=[&](const std::vector<Command>& commands){session.apply(commands,session.revision());window.host.edited();QApplication::processEvents();};
        const auto comp=session.document().compositions.front().id;
        Point a,b;a.id="point-a";b.id="point-b";a.x.literal=100;b.x.literal=200;a.y.literal=b.y.literal=200;
        ColorValue palette;palette.rgba={0.2,0.3,0.9,1};
        apply({CreatePath{comp,"","path-a","A",{{"contour-a",false,{a}}}},CreatePath{comp,"","path-b","B",{{"contour-b",false,{b}}}},
            CreateNamedColor{named("brand","Brand",palette)}});
        const auto source=stroke_ref(session.document(),"path-a"),target=stroke_ref(session.document(),"path-b");
        const Ref brand{"brand","","color"};
        QWidget controls(&window);
        auto* source_menu=tools->menu_button(source,&controls);auto* target_menu=tools->menu_button(target,&controls);
        auto* brand_menu=tools->menu_button(brand,&controls);
        const auto before_draft=encode(session.document());auto revision=session.revision();
        action(source_menu,"color-edit-expressions");
        auto* expression_dialog=widget<QDialog>(window,"color-expression-editor");
        auto* expression_buttons=expression_dialog->findChild<QDialogButtonBox*>();
        check(expression_buttons!=nullptr,"RGBA expression editor exposes Apply and Cancel");
        type(widget<QLineEdit>(*expression_dialog,"color-expression-0"),"=ref(\"brand\",\"\",\"color.r\")");
        QTest::mouseClick(expression_buttons->button(QDialogButtonBox::Cancel),Qt::LeftButton);QApplication::processEvents();
        check(session.revision()==revision&&encode(session.document())==before_draft,"Canceled RGBA draft does not mutate authored Color");
        action(source_menu,"color-edit-expressions");expression_dialog=widget<QDialog>(window,"color-expression-editor");
        expression_buttons=expression_dialog->findChild<QDialogButtonBox*>();
        for(int i=0;i<4;++i) {
            const auto text=QString("=ref(\"brand\",\"\",\"color.%1\")").arg("rgba"[i]);
            type(widget<QLineEdit>(*expression_dialog,QString("color-expression-%1").arg(i).toUtf8().constData()),text.toUtf8().constData());
        }
        QTest::mouseClick(expression_buttons->button(QDialogButtonBox::Ok),Qt::LeftButton);QApplication::processEvents();
        check(session.revision()==revision+1&&color(session.document(),source)==palette,
            "RGBA expressions commit together through the production Color menu");
        for(const auto& channel:color_channels(session.document(),source))
            check(nect::property(session.document(),channel).expression.has_value(),"All four channels retain authored expressions");
        ColorValue changed_brand;changed_brand.rgba={0.3,0.4,0.5,0.6};apply({SetColor{brand,changed_brand}});
        check(color(session.document(),source)==changed_brand,"RGBA expressions follow stable named Color refs");
        action(source_menu,"color-edit-expressions");expression_dialog=widget<QDialog>(window,"color-expression-editor");
        expression_buttons=expression_dialog->findChild<QDialogButtonBox*>();
        auto* replace=widget<QCheckBox>(*expression_dialog,"color-expression-replace");
        type(widget<QLineEdit>(*expression_dialog,"color-expression-0"),"=0.1");
        type(widget<QLineEdit>(*expression_dialog,"color-expression-3"),"=1/0");
        revision=session.revision();const auto before_invalid=encode(session.document());
        QTest::mouseClick(expression_buttons->button(QDialogButtonBox::Ok),Qt::LeftButton);QApplication::processEvents();
        check(session.revision()==revision&&encode(session.document())==before_invalid,
            "Replacing existing expressions requires explicit intent and leaves the full Color unchanged");
        replace->setChecked(true);
        QTest::mouseClick(expression_buttons->button(QDialogButtonBox::Ok),Qt::LeftButton);QApplication::processEvents();
        check(session.revision()==revision&&encode(session.document())==before_invalid&&
            widget<QLabel>(*expression_dialog,"color-expression-status")->text().contains("unchanged"),
            "Invalid fourth expression rejects earlier channel edits atomically");
        expression_dialog->reject();QApplication::processEvents();
        action(source_menu,"color-edit-expressions");expression_dialog=widget<QDialog>(window,"color-expression-editor");
        expression_buttons=expression_dialog->findChild<QDialogButtonBox*>();
        widget<QCheckBox>(*expression_dialog,"color-expression-replace")->setChecked(true);
        type(widget<QLineEdit>(*expression_dialog,"color-expression-0"),"0.7");
        QTest::mouseClick(expression_buttons->button(QDialogButtonBox::Ok),Qt::LeftButton);QApplication::processEvents();
        const auto channels=color_channels(session.document(),source);
        check(color(session.document(),source).rgba[0]==0.7&&
            !nect::property(session.document(),channels[0]).expression&&
            nect::property(session.document(),channels[1]).expression.has_value(),
            "Explicit literal replacement changes only the selected channel and preserves other expressions");
        action(source_menu,"color-edit-expressions");expression_dialog=widget<QDialog>(window,"color-expression-editor");
        expression_buttons=expression_dialog->findChild<QDialogButtonBox*>();
        type(widget<QLineEdit>(*expression_dialog,"color-expression-0"),"=0.2");
        apply({RenameNamedColor{"brand","Brand renamed"}});
        revision=session.revision();const auto before_stale=encode(session.document());
        QTest::mouseClick(expression_buttons->button(QDialogButtonBox::Ok),Qt::LeftButton);QApplication::processEvents();
        check(session.revision()==revision&&encode(session.document())==before_stale&&
            widget<QLabel>(*expression_dialog,"color-expression-status")->text().contains("changed"),
            "A stale RGBA draft cannot overwrite a newer Session revision");
        expression_dialog->reject();QApplication::processEvents();
        apply({RenameNamedColor{"brand","Brand"}});
        apply({UnlinkColor{source},SetColor{brand,palette}});
        tools->show_manager();QApplication::processEvents();
        auto* manager=widget<QDialog>(window,"color-manager");auto* history=widget<QListWidget>(*manager,"copied-colors");
        auto* used=widget<QListWidget>(*manager,"used-colors");auto* named_list=widget<QListWidget>(*manager,"named-colors");
        check(named_list->count()==1,"Named-color fixture exposes its swatch");
        const auto swatch=named_list->item(0)->icon();
        const auto normal_swatch=swatch.pixmap(QSize(28,20),QIcon::Normal,QIcon::Off).toImage();
        check(!normal_swatch.isNull(),"Named-color swatch contains actual color pixels");
        for(const auto mode:{QIcon::Normal,QIcon::Active,QIcon::Selected,QIcon::Disabled})
            for(const auto state:{QIcon::Off,QIcon::On})
                check(swatch.pixmap(QSize(28,20),mode,state).toImage()==normal_swatch,
                    "Selection, activation and disabled states preserve exact swatch pixels");
        check(history->count()==0&&widget<QLabel>(*manager,"color-history-scope")->text().contains("window session only"),
            "Copied history starts empty and declares its window-session scope");
        ColorValue precise;precise.rgba={0.1234567890123,0.4321098765432,0.8765432109876,0.6543210987654};
        apply({SetColor{source,precise},SetColor{target,precise}});
        check(history->count()==0,"Color authoring and refresh do not create copied history");
        check(used->count()==1&&widget<QListWidget>(*manager,"color-usages")->count()==2,
            "Document inventory groups exact equal evaluated paint inputs and excludes palette-only entries");
        action(source_menu,"color-copy-value");
        check(history->count()==1&&QApplication::clipboard()->mimeData()->hasFormat("application/x-nect-color-value")&&
            !QApplication::clipboard()->mimeData()->hasFormat("application/x-nect-color-reference"),
            "Copy Value records one explicit entry and carries a structured value without a link reference");
        ColorValue black;apply({SetColor{target,black}});action(target_menu,"color-paste-value");
        check(color(session.document(),target)==precise&&independent(session.document(),target),
            "Paste Value preserves full precision while keeping the target independent");
        ColorValue changed;changed.rgba={0.8,0.1,0.2,1};apply({SetColor{source,changed}});
        check(color(session.document(),target)==precise&&history->count()==1,"Later source edits do not change a pasted value or create history");
        action(brand_menu,"color-copy-reference");action(target_menu,"color-paste-link");
        check(color_link(session.document(),target)==std::optional<Ref>{brand}&&color(session.document(),target)==palette,
            "Copy Reference and Paste Link create four stable channel bindings");
        check(history->count()==2,"Copy Reference also records its evaluated value exactly once");
        apply({SetColor{brand,precise}});select_named(named_list,"brand");
        type(widget<QLineEdit>(*manager,"named-color-name"),"Precise brand");click(*manager,"named-color-apply");
        check(color(session.document(),brand)==precise&&color(session.document(),target)==precise&&
            color_link(session.document(),target)==std::optional<Ref>{brand},
            "A name-only Apply preserves exact non-8-bit channels and existing links without reading rounded HEX");
        select_named(named_list,"brand");type(widget<QLineEdit>(*manager,"named-color-channel-0"),"0.9");click(*manager,"named-color-apply");
        check(color(session.document(),target).rgba[0]==0.9&&color(session.document(),source)==changed&&history->count()==2,
            "Editing a named color propagates to linked targets and leaves independent paint/history unchanged");
        revision=session.revision();click(*manager,"named-color-delete");
        check(session.revision()==revision&&session.document().named_colors.contains("brand")&&color_link(session.document(),target)==std::optional<Ref>{brand},
            "Deleting a referenced named color rejects without damaging links");
        const auto frozen=color(session.document(),target);action(target_menu,"color-unlink");
        check(independent(session.document(),target)&&color(session.document(),target)==frozen,"Unlink explicitly freezes evaluated RGBA");
        type(widget<QLineEdit>(*manager,"named-color-channel-0"),"0.1");click(*manager,"named-color-apply");
        check(color(session.document(),target)==frozen,"Unlinked paint stays fixed after subsequent named-color edits");

        auto* name=widget<QLineEdit>(*manager,"named-color-name");type(name,"Draft name");
        apply({RenameNamedColor{"brand","External name"},CreateNamedColor{named("another","Another",black)}});
        check(named_list->currentItem()->data(Qt::UserRole)=="brand"&&name->text()=="Draft name"&&name->isModified(),
            "External refresh preserves selected stable identity and an unfinished name draft");
        revision=session.revision();click(*manager,"named-color-apply");
        check(session.revision()==revision&&session.document().named_colors.at("brand").name=="External name"&&
            window.statusBar()->currentMessage().contains("COLOR_DRAFT_CONFLICT"),"Applying a stale name draft rejects a real conflict");
        click(*manager,"named-color-discard");check(name->text()=="External name"&&!name->isModified(),"Discard explicitly reloads the current name");
        auto* red=widget<QLineEdit>(*manager,"named-color-channel-0");type(red,"0.7");
        ColorValue external;external.rgba={0.4,0.5,0.6,1};apply({SetColor{brand,external}});
        check(red->text()=="0.7"&&red->isModified(),"External RGBA edits do not overwrite a numeric draft");
        revision=session.revision();click(*manager,"named-color-apply");
        check(session.revision()==revision&&color(session.document(),brand)==external,"Stale RGBA Apply does not overwrite a newer color");
        click(*manager,"named-color-discard");

        auto* unsupported=new QMimeData;
        unsupported->setData("application/x-nect-color-value",QJsonDocument(QJsonObject{{"version",1},{"space","display-p3"},
            {"profile","display-p3"},{"alpha","straight"},{"rgba",QJsonArray{1,0,0,1}}}).toJson(QJsonDocument::Compact));
        unsupported->setText("#FFFFFFFF");QApplication::clipboard()->setMimeData(unsupported);revision=session.revision();
        action(target_menu,"color-paste-value");
        check(session.revision()==revision&&color(session.document(),target)==frozen&&window.statusBar()->currentMessage().contains("UNSUPPORTED_COLOR"),
            "Unsupported structured color is rejected instead of silently falling back to its lossy HEX text");
        auto* duplicate=new QMimeData;
        duplicate->setData("application/x-nect-color-value",R"({"version":1,"space":"display-p3","space":"srgb","profile":"srgb","alpha":"straight","rgba":[1,0,0,1]})");
        duplicate->setText("#FFFFFFFF");QApplication::clipboard()->setMimeData(duplicate);revision=session.revision();
        action(target_menu,"color-paste-value");
        check(session.revision()==revision&&color(session.document(),target)==frozen,
            "Duplicate structured keys reject before Qt parsing can replace unsupported color metadata");
        QApplication::clipboard()->setText("#00FF00");tools->refresh();
        check(history->count()==2,"Unrelated clipboard changes are not monitored or recorded");
        for(int i=0;i<35;++i) {
            ColorValue value;value.rgba={i/100.0,0.2,0.3,1};apply({SetColor{source,value}});action(source_menu,"color-copy-value");
        }
        check(history->count()==32,"Only the most recent 32 explicit copied values are retained");
        action(source_menu,"color-copy-value");check(history->count()==32,"Copying an identical exact value deduplicates history");
        auto* history_target=tools->menu_button(target,&controls);auto* history_menu=history_target->menu()->findChild<QMenu*>("color-history-menu");
        check(history_menu&&!history_menu->actions().empty(),"Per-target color menu exposes copied history values");
        history_menu->actions().front()->trigger();QApplication::processEvents();
        check(color(session.document(),target)==color(session.document(),source)&&history->count()==32,
            "Applying a copied history value is independent and does not add a new copy event");

        action(brand_menu,"color-copy-reference");const auto copied_brand=color(session.document(),brand);
        action(target_menu,"color-link-named");auto* picker=widget<QDialog>(window,"named-color-picker");
        auto* sources=widget<QListWidget>(*picker,"named-color-sources");sources->setCurrentRow(0);
        window.host.create_document();QApplication::processEvents();revision=session.revision();
        auto* picker_buttons=picker->findChild<QDialogButtonBox*>();check(picker_buttons!=nullptr,"Named-color picker has explicit Apply and Cancel controls");
        QTest::mouseClick(picker_buttons->button(QDialogButtonBox::Ok),Qt::LeftButton);QApplication::processEvents();
        check(session.revision()==revision&&window.statusBar()->currentMessage().contains("SESSION_CONFLICT"),
            "A late named-color picker cannot mutate a replacement document session");
        picker->reject();QApplication::processEvents();
        const auto replacement_comp=session.document().compositions.front().id;
        Point point;point.id="new-point";apply({CreatePath{replacement_comp,"","new-path","New target",{{"new-contour",false,{point}}}}});
        const auto replacement=stroke_ref(session.document(),"new-path");auto* replacement_menu=tools->menu_button(replacement,&controls);
        revision=session.revision();action(replacement_menu,"color-paste-link");
        check(session.revision()==revision&&window.statusBar()->currentMessage().contains("COLOR_DOCUMENT_CONFLICT"),
            "A copied reference cannot link into a different document even when clipboard HEX is valid");
        action(replacement_menu,"color-paste-value");
        check(color(session.document(),replacement)==copied_brand&&independent(session.document(),replacement),
            "The structured copied value remains usable independently across documents");
        check(history->count()==32,"Explicit copy history remains scoped to the Window across document changes");
        std::cout<<"PASS "<<checks<<" color UI checks\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}

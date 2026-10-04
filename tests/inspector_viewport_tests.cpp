#include "macro_boolean_window_smoke.hpp"
#include "visual_style.hpp"
#include <QDockWidget>
#include <QScrollBar>
#include <QFile>
#include <QFont>
#include <QJsonArray>
#include <QJsonObject>
#include <iostream>
using namespace macro_boolean_window_smoke;
int main(int argc,char** argv) {
    QApplication app(argc,argv);app.setStyle("Fusion");app.setStyleSheet(application_style_sheet());
    // Qualification can supply the host's desktop message font when the
    // offscreen plugin's default metrics differ from the native platform.
    const auto font_family=qEnvironmentVariable("NECT_INSPECTOR_FONT");
    if(!font_family.isEmpty()) {
        auto font=app.font();font.setFamily(font_family);
        const auto pixels=qEnvironmentVariableIntValue("NECT_INSPECTOR_FONT_PIXELS");
        if(pixels>0)font.setPixelSize(pixels);
        app.setFont(font);
    }
    try {
        QTemporaryDir files;require(files.isValid(),"Temporary directory exists");
        QSettings settings(files.filePath("library.ini"),QSettings::IniFormat);
        Window window(files.path()+"/recovery",std::make_unique<FolderLibrary>(settings));
        window.setAttribute(Qt::WA_DontShowOnScreen);window.resize(1000,650);window.show();
        const auto sample=qEnvironmentVariable("NECT_INSPECTOR_SAMPLE");
        if(sample.isEmpty())load(window);
        else {window.host.open(sample);window.canvas->set_selection("path");window.refresh();events();}
        auto* dock=window.findChild<QDockWidget*>("properties");
        auto* scroll=window.findChild<QScrollArea*>("inspector-scroll");
        require(dock&&scroll,"Production Properties panel exists");
        dock->setFixedWidth(300);events();QTest::qWait(20);events();
        require(dock->width()==300,"Properties dock is exactly 300px under production styling");
        auto* horizontal=scroll->horizontalScrollBar();horizontal->setValue(0);
        // Reveal vertically only: ensureWidgetVisible silently scrolls right and
        // used to conceal clipped controls at the default horizontal position.
        auto reveal_vertical=[&](QWidget* control) {
            const auto top=control->mapTo(scroll->widget(),QPoint{}).y();
            scroll->verticalScrollBar()->setValue(top-scroll->viewport()->height()/2);
            events();QTest::qWait(20);events();
            require(horizontal->value()==0,"Narrow Inspector checks keep horizontal scrolling at zero");
        };
        const Snapshot before(window.host.session);
        for(const auto* parameter:{amount_id,copies_id}) {
            auto* field=numeric(window,parameter);reveal_vertical(field);
            const auto point=field->mapTo(scroll->viewport(),QPoint(0,0));
            require(point.x()>=0&&point.x()+field->width()<=scroll->viewport()->width(),"Macro number fits narrow Inspector");
        }
        int resets=0;QJsonArray reset_geometry;
        for(auto* reset:window.findChildren<QPushButton*>("macro-reset-amount-instance"))if(reset->isVisible()) {
            reveal_vertical(reset);const auto point=reset->mapTo(scroll->viewport(),QPoint(0,0));
            reset_geometry.append(QJsonObject{{"x",point.x()},{"width",reset->width()},{"viewport_width",scroll->viewport()->width()},{"content_width",scroll->widget()->width()},{"horizontal_scroll",horizontal->value()}});
            const auto evidence=qEnvironmentVariable("NECT_INSPECTOR_EVIDENCE");
            if(!evidence.isEmpty()) {
                window.grab().save(evidence+"/inspector-reset-"+QString::number(resets)+".png");
                QFile receipt(evidence+"/inspector-geometry.json");
                if(receipt.open(QIODevice::WriteOnly))receipt.write(QJsonDocument(QJsonObject{{"dock_width",dock->width()},{"font",app.font().toString()},{"reset_geometry",reset_geometry}}).toJson());
            }
            require(point.x()>=0&&point.x()+reset->width()<=scroll->viewport()->width(),"Macro Reset is not horizontally clipped");
            require(scroll->viewport()->rect().contains(QRect(point,reset->size()))&&reset->visibleRegion().contains(reset->rect()),
                "Complete Reset label and border are visible after vertical scrolling only");
            require(reset->width()>=reset->minimumSizeHint().width(),"Macro Reset retains full button width");++resets;
        }
        require(resets==2,"Both numeric Macro resets checked");
        require(before.unchanged(window.host.session),"Layout and reveal do not mutate document or history");
        // Keep the original vertical minimum-sizing and context checks below.
        // Also exercise each exposed Reset through its real mouse callback.
        for(const auto* parameter:{amount_id,copies_id}) {
            auto* field=numeric(window,parameter);
            auto* reset_button=field->parentWidget()->findChild<QPushButton*>("macro-reset-amount-instance");
            require(reset_button,"Each numeric field retains its existing named Reset callback");
            horizontal->setValue(0);reveal_vertical(reset_button);
            const Snapshot prior(window.host.session);
            Session expected(prior.document);
            expected.apply({MacroCommand{ResetMacroOverride{"path","instance",parameter}}},expected.revision());
            QTest::mouseClick(reset_button,Qt::LeftButton);events();QTest::qWait(20);events();
            require(window.host.session.document()==expected.document()&&window.host.session.revision()==prior.revision+1,
                "Visible Reset click performs exactly the canonical override reset command");
            const auto state=window.host.session.history().current_id;
            history_action(window,"Undo");
            require(window.host.session.document()==prior.document&&encode(window.host.session.document())==prior.native,
                "One existing Undo restores the exact pre-Reset overrides and native state");
            history_action(window,"Redo");
            require(window.host.session.document()==expected.document()&&window.host.session.history().current_id==state,
                "One existing Redo restores the same Reset history boundary");
            history_action(window,"Undo");
            require(window.host.session.document()==prior.document,"Reset callback check leaves authored state restored");
        }
        auto* bar=scroll->verticalScrollBar();require(bar->maximum()>100,"Fixture has meaningful Inspector overflow");
        bar->setValue(bar->maximum()/2);const int saved=bar->value();
        window.refresh(false);window.refresh(false);events();QTest::qWait(20);events();
        require(bar->value()==saved,"Repeated same-context refresh preserves scroll");
        bar->setValue(bar->maximum());window.canvas->set_selection("other");window.host.edited();
        events();QTest::qWait(20);events();
        require(bar->value()==0,"Selection plus immediate host refresh preserves pending context reset");
        bar->setValue(bar->maximum());auto* add=window.findChild<QAction*>("add-text");require(add,"Add Text action exists");
        add->trigger();events();QTest::qWait(20);events();
        auto* edit=visible<QPushButton>(window,"edit-text-content");require(edit,"New text has an editing entry");
        const QRect edit_rect(edit->mapTo(scroll->viewport(),QPoint(0,0)),edit->size());
        require(scroll->viewport()->rect().contains(edit_rect),"New text editing entry is reachable immediately after deep scroll");
        window.host.changed={};window.hide();
        std::cout<<"PASS "<<checks<<" Inspector viewport checks\n";return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<" after "<<checks<<" checks\n";return 1;}
}

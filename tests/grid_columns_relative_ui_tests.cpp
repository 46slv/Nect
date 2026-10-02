#include "window.hpp"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QLineEdit>
#include <QKeyEvent>
#include <QPointer>
#include <QPushButton>
#include <QSettings>
#include <QStatusBar>
#include <QTemporaryDir>
#include <array>
#include <bit>
#include <iomanip>
#include <iostream>
#include <stdexcept>

using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);++checks;}
void events(){QApplication::processEvents();}
template<class T>T* widget(Window& window,const char* name){
    for(auto* item:window.findChildren<T*>(name))if(item->isVisible())return item;
    throw std::runtime_error(std::string("Missing visible control: ")+name);
}
Document fixture(){
    auto document=empty_document("relative-doc","comp","board");
    auto& board=document.compositions.front().artboards.front();
    board.width=1200;board.height=900;
    board.layout=ArtboardLayout{Margin{10,20,30,40},Grid{"grid",{20,20,1000,800},2,3,7,8}};
    board.layout->grid->rows_expression=Expression{"3",1};
    return document;
}
const Artboard& board(const Session& session){return session.document().compositions.front().artboards.front();}
void draft(Window& window,const QString& value){
    auto* input=widget<QLineEdit>(window,"grid-columns");input->setText(value);
    check(QMetaObject::invokeMethod(input,"textEdited",Qt::DirectConnection,Q_ARG(QString,value)),"draft signal delivered");
    events();
}
void enter(Window& window){
    check(QMetaObject::invokeMethod(widget<QLineEdit>(window,"grid-columns"),"returnPressed",Qt::DirectConnection),"Enter delivered");
    events();
}
void open(Window& window){window.refresh();window.show();events();widget<QPushButton>(window,"artboard-edit")->click();events();}
void restore(Window& window){window.host.edited();events();}
void literal_cases(Window& window){
    auto& session=window.host.session;session=Session(fixture());
    window.canvas->set_active_artboard("comp","board",false);open(window);
    const auto original=encode(session.document());const auto revision=session.revision();
    draft(window,"+=3");
    check(encode(session.document())==original&&session.revision()==revision,"relative preview leaves authored bytes and revision unchanged");
    check(session.preview_document().compositions.front().artboards.front().layout->grid->columns==5,"2 +=3 previews 5");
    // Repeated validation must always use the captured authored count, never its preview.
    draft(window,"+=3");enter(window);
    auto expected=fixture();expected.compositions.front().artboards.front().layout->grid->columns=5;
    check(session.document()==expected&&session.revision()==revision+1,"relative commit is one revision and preserves all siblings and sources");
    check(decode(encode(session.document()))==expected,"native roundtrip preserves the stable Grid identity and sources");
    session.undo(session.revision());restore(window);
    check(encode(session.document())==original&&!session.can_undo(),"one Undo restores exact native bytes and history");
    session.redo(session.revision());restore(window);
    check(session.document()==expected,"Redo restores exact committed relative edit");
    draft(window,"-=3");enter(window);
    check(board(session).layout->grid->columns==2,"relative subtraction returns 5 to 2");

    for(const auto* invalid:{"+=","-=","+=1.5","+=1e0","+=-1","+=+1","+= 1","*=2","0","1001","+=999","-=2","+=9223372036854775807","+=18446744073709551616","NaN"}){
        const auto bytes=encode(session.document());const auto rev=session.revision();const auto history=session.history();
        draft(window,invalid);enter(window);
        check(encode(session.document())==bytes&&session.revision()==rev&&session.history()==history,"invalid or out-of-range draft cannot mutate authored bytes, revision or history");
        check(widget<QLineEdit>(window,"grid-columns")->text()==invalid,"rejected draft remains visible instead of becoming zero");
        window.refresh();events();
    }
    // A valid draft after an error still resolves from the original count.
    draft(window,"+=1");enter(window);check(board(session).layout->grid->columns==3,"valid correction recovers after rejected draft");
    draft(window,"+=2");
    session.cancel_gesture(); // Another command owner cancels the preview before applying.
    auto changed=board(session);changed.name="Changed elsewhere";
    session.apply({UpdateArtboard{"comp",changed}},session.revision());
    const auto fresh=encode(session.document());const auto fresh_revision=session.revision();const auto fresh_history=session.history();
    enter(window);
    check(encode(session.document())==fresh&&session.revision()==fresh_revision&&session.history()==fresh_history,"stale relative draft cannot overwrite a newer command");
}
void boundary_cases(Window& window){
    auto document=fixture();auto& grid=*document.compositions.front().artboards.front().layout->grid;
    grid.columns=1;grid.column_gutter=0;
    window.host.session=Session(document);restore(window);
    draft(window,"+=999");enter(window);
    check(board(window.host.session).layout->grid->columns==1000,"relative addition accepts the upper endpoint");
    draft(window,"-=999");enter(window);
    check(board(window.host.session).layout->grid->columns==1,"relative subtraction accepts the lower endpoint");
}
void driven_cases(Window& window){
    auto document=fixture();document.compositions.front().artboards.front().layout->grid->columns_expression=Expression{"4",1};
    window.host.session=Session(document);restore(window);
    auto* input=widget<QLineEdit>(window,"grid-columns");check(input->isReadOnly(),"expression-driven columns remain read-only");
    const auto before=encode(window.host.session.document());const auto rev=window.host.session.revision();
    draft(window,"+=3");enter(window);
    check(encode(window.host.session.document())==before&&window.host.session.revision()==rev,"driven relative signal cannot mutate or unlink the source");
    document=fixture();
    Artboard source{"source","Source",1500,0,1200,900};
    source.layout=ArtboardLayout{{},Grid{"source-grid",{0,0,1000,800},4,1,0,0}};
    document.compositions.front().artboards.push_back(source);
    document.compositions.front().artboards.front().layout->grid->columns_driver=Ref{"source-grid","","grid.columns"};
    window.host.session=Session(document);restore(window);
    check(widget<QLineEdit>(window,"grid-columns")->isReadOnly(),"linked columns remain read-only");
    const auto linked=encode(window.host.session.document());const auto linked_revision=window.host.session.revision();
    draft(window,"-=1");enter(window);
    check(encode(window.host.session.document())==linked&&window.host.session.revision()==linked_revision,"linked relative signal preserves the source and history");
}
void template_cases(Window& window){
    auto document=fixture();auto& composition=document.compositions.front();
    composition.artboards.push_back({"target","Target",1500,0,1200,900});
    Session seeded(document);seeded.apply({
        ArtboardTemplateCommand{CreateArtboardTemplate{"comp",{"template","Template","board",{}}}},
        ArtboardTemplateCommand{AssignArtboardTemplate{"comp","target","template",{}}}},0);
    window.host.session=Session(seeded.document());window.canvas->set_active_artboard("comp","target",false);restore(window);
    auto& session=window.host.session;const auto before=encode(session.document());
    const auto source=session.document().compositions.front().artboards.front();
    const auto inherited=evaluate_artboard(session.document().compositions.front(),"target").layout->grid.value();
    draft(window,"5");enter(window);const auto absolute=session.document();
    session.undo(session.revision());restore(window);
    check(encode(session.document())==before,"absolute control Undo restores exact inheritance");
    draft(window,"+=3");enter(window);
    const auto& target=session.document().compositions.front().artboards.back();
    check(target.template_assignment&&target.template_assignment->grid_overridden&&!target.template_assignment->margin_overridden,"relative edit overrides only the Template Grid family");
    // The existing family override snapshots evaluated inherited values.
    // Source expressions remain on the Template source; existing local sources are tested below.
    const Grid wanted{inherited.id,inherited.bounds,5,inherited.rows,inherited.column_gutter,inherited.row_gutter};
    check(evaluate_artboard(session.document().compositions.front(),"target").layout->grid==wanted,"Template relative override preserves inherited identity and evaluated siblings");
    check(session.document()==absolute,"relative Template edit matches the established absolute override path exactly");
    check(session.document().compositions.front().artboards.front()==source,"Template source remains unchanged");
    check(decode(encode(session.document()))==session.document(),"Template relative override survives native roundtrip");
    session.undo(session.revision());restore(window);
    check(encode(session.document())==before&&!session.can_undo(),"one Undo removes the override and restores exact inherited native state");

    auto local=inherited;local.rows_expression.reset();
    session.apply({ArtboardTemplateCommand{SetArtboardTemplateOverride{"comp","target","layout.grid",std::optional<Grid>{local}}},
        ArtboardTemplateCommand{SetArtboardTemplateOverride{"comp","target","layout.margin",std::optional<Margin>{Margin{2,3,4,5}}}},
        GridRowsCommand{SetGridRowsExpression{{local.id,"","grid.rows"},{"6",1},false}}},session.revision());
    session=Session(session.document());restore(window);
    auto expected=session.document();expected.compositions.front().artboards.back().layout->grid->columns=5;
    const auto local_before=encode(session.document());
    draft(window,"+=3");enter(window);
    check(session.document()==expected,"existing Template overrides, sibling expressions and identities are preserved exactly");
    session.undo(session.revision());restore(window);
    check(encode(session.document())==local_before&&!session.can_undo(),"one Undo restores exact preexisting Template overrides");
}

Document precision_fixture(int mode){
    auto document=fixture();auto& grid=*document.compositions.front().artboards.front().layout->grid;
    grid.bounds={20.123456789012344,21.987654321098766,1000.1234567890123,800.98765432109872};
    grid.column_gutter=7.123456789012344;grid.row_gutter=8.987654321098766;
    if(mode==0)return document;
    document.compositions.front().artboards.push_back({"target","Target",1500,0,1200,900});
    Session seeded(document);seeded.apply({
        ArtboardTemplateCommand{CreateArtboardTemplate{"comp",{"template","Template","board",{}}}},
        ArtboardTemplateCommand{AssignArtboardTemplate{"comp","target","template",{}}}},0);
    if(mode==2){
        const auto evaluated=evaluate_artboard(seeded.document().compositions.front(),"target").layout->grid.value();
        const Grid inherited{evaluated.id,evaluated.bounds,evaluated.columns,evaluated.rows,evaluated.column_gutter,evaluated.row_gutter};
        seeded.apply({ArtboardTemplateCommand{SetArtboardTemplateOverride{"comp","target","layout.grid",std::optional<Grid>{inherited}}}},seeded.revision());
    }
    return seeded.document();
}
std::array<double,6> decimals(const Grid& grid){
    return {grid.bounds.x,grid.bounds.y,grid.bounds.width,grid.bounds.height,grid.column_gutter,grid.row_gutter};
}
const Grid& authored_grid(const Document& document,int mode){
    return *document.compositions.front().artboards[mode==0?0:1].layout->grid;
}
Document expected_columns(const Document& document,int mode,std::size_t columns){
    if(mode!=1){auto result=document;result.compositions.front().artboards[mode==0?0:1].layout->grid->columns=columns;return result;}
    const auto evaluated=evaluate_artboard(document.compositions.front(),"target").layout->grid.value();
    const Grid inherited{evaluated.id,evaluated.bounds,columns,evaluated.rows,evaluated.column_gutter,evaluated.row_gutter};
    Session expected(document);expected.apply({ArtboardTemplateCommand{SetArtboardTemplateOverride{"comp","target","layout.grid",std::optional<Grid>{inherited}}}},0);
    return expected.document();
}
QByteArray file_bytes(const QString& path){
    QFile file(path);check(file.open(QIODevice::ReadOnly),"read owned native fixture");return file.readAll();
}
void put_bytes(const QString& path,const std::string& bytes){
    QFile file(path);check(file.open(QIODevice::WriteOnly),"write owned native fixture");
    check(file.write(QByteArray::fromStdString(bytes))==static_cast<qint64>(bytes.size()),"write complete owned native bytes");
}
void load_precision(Window& window,const Document& document,int mode){
    static int fixture_index=0;
    const auto path=window.host.recovery_directory()+"/grid-fixture-"+QString::number(++fixture_index)+".nect";
    put_bytes(path,encode(document));window.host.session.cancel_gesture();window.host.open(path);
    window.canvas->set_active_artboard("comp",mode==0?"board":"target",false);restore(window);
}
void exact_grid(const Document& actual,const Document& expected,int mode){
    const auto actual_values=decimals(authored_grid(actual,mode));const auto expected_values=decimals(authored_grid(expected,mode));
    for(std::size_t index=0;index<actual_values.size();++index)
        check(std::bit_cast<std::uint64_t>(actual_values[index])==std::bit_cast<std::uint64_t>(expected_values[index]),"exact Grid double bits preserved");
    check(encode(actual)==encode(expected),"exact native bytes include identities, siblings, sources and override presence");
}
void number_draft(Window& window,const char* name,const QString& value){
    auto* input=widget<QLineEdit>(window,name);input->setText(value);
    check(QMetaObject::invokeMethod(input,"textEdited",Qt::DirectConnection,Q_ARG(QString,value)),"Grid sibling draft signal delivered");events();
}
void number_enter(Window& window,const char* name){
    check(QMetaObject::invokeMethod(widget<QLineEdit>(window,name),"returnPressed",Qt::DirectConnection),"Grid sibling Enter delivered");events();
}
int cold_open(const QString& native,const QString& expected_file,const QString& directory,int mode){
    const auto expected_bytes=file_bytes(expected_file);const auto expected=decode(expected_bytes.toStdString());
    const auto source_bytes=file_bytes(native);Host cold(directory);cold.open(native);
    exact_grid(cold.session.document(),expected,mode);
    check(cold.session.revision()==0&&!cold.session.can_undo()&&!cold.session.can_redo(),"cold Host has fresh zero-history Session");
    check(file_bytes(native)==source_bytes&&source_bytes==expected_bytes,"cold Host preserves exact destination bytes");
    cold.recover();check(file_bytes(cold.persistence()["recovery_file"].toString())==expected_bytes,"cold Host recovery retains exact sources and decimals");
    std::cout<<"PASS "<<checks<<" cold Host Grid precision checks\n";return 0;
}
void persistence_cases(Window& window,const Document& before,const Document& expected,int mode){
    auto& host=window.host;const auto directory=host.recovery_directory();
    const auto native=directory+"/grid-saved-"+QString::number(mode)+".nect";
    const auto expected_file=directory+"/grid-expected-"+QString::number(mode)+".nect";
    put_bytes(expected_file,encode(expected));host.save(native);host.recover();events();
    check(file_bytes(native).toStdString()==encode(expected),"Host Save As persists exact Grid native bytes");
    check(file_bytes(host.persistence()["recovery_file"].toString()).toStdString()==encode(expected),"Host recovery persists exact committed Grid bytes");
    draft(window,"+=1");
    check(host.session.preview_document()!=expected&&encode(host.session.document())==encode(expected),"later preview stays separate from committed Grid");
    host.recover();check(file_bytes(native).toStdString()==encode(expected)&&
        file_bytes(host.persistence()["recovery_file"].toString()).toStdString()==encode(expected),"native and recovery never persist relative preview");
    widget<QPushButton>(window,"grid-columns-cancel")->click();events();
    check(!host.session.gesture_active()&&encode(host.session.document())==encode(expected),"Cancel leaves exact committed Grid and sources");
    host.session.undo(host.session.revision());restore(window);exact_grid(host.session.document(),before,mode==1?0:mode);
    check(encode(host.session.document())==encode(before)&&!host.session.can_undo(),"one Undo restores exact inherited/local authored bytes");
    host.recover();check(file_bytes(native).toStdString()==encode(before)&&
        file_bytes(host.persistence()["recovery_file"].toString()).toStdString()==encode(before),"Undo native and recovery are exact");
    host.session.redo(host.session.revision());restore(window);exact_grid(host.session.document(),expected,mode);host.recover();
    check(file_bytes(native).toStdString()==encode(expected),"Redo native storage is exact");
    QProcess child;child.start(QCoreApplication::applicationFilePath(),QStringList{"--cold-open",native,expected_file,
        directory+"/cold-"+QString::number(mode),QString::number(mode)});
    check(child.waitForStarted(5000),"distinct cold Host process starts");
    check(child.waitForFinished(10000)&&child.exitStatus()==QProcess::NormalExit&&child.exitCode()==0,"distinct cold Host process reopens exact native state");
    std::cout<<child.readAllStandardOutput().toStdString();
}
void precision_cases(Window& window){
    for(int mode=0;mode<3;++mode)for(const auto* edit:{"+=3","5"}){
        const auto before=precision_fixture(mode);const auto expected=expected_columns(before,mode,5);
        load_precision(window,before,mode);draft(window,edit);
        check(encode(window.host.session.document())==encode(before)&&window.host.session.revision()==0,"precision preview leaves native bytes/revision unchanged");
        exact_grid(window.host.session.preview_document(),expected,mode);draft(window,edit);enter(window);
        exact_grid(window.host.session.document(),expected,mode);
        check(window.host.session.revision()==1&&window.host.session.can_undo(),"intentional precision edit is one revision/Undo");
        if(std::string(edit)=="+=3")persistence_cases(window,before,expected,mode);
        else{
            window.host.session.undo(window.host.session.revision());restore(window);
            check(encode(window.host.session.document())==encode(before)&&!window.host.session.can_undo(),"absolute precision one Undo is exact");
            window.host.session.redo(window.host.session.revision());restore(window);exact_grid(window.host.session.document(),expected,mode);
        }
    }
}
void precision_noop_cases(Window& window){
    const char* fields[]={"grid-x","grid-y","grid-width","grid-height","grid-column-gutter","grid-row-gutter"};
    for(int mode=0;mode<3;++mode){
        const auto before=precision_fixture(mode);load_precision(window,before,mode);
        const auto history=window.host.session.history();
        for(const auto* edit:{"2","+=0","-=0","  +=0  "}){
            draft(window,edit);check(encode(window.host.session.preview_document())==encode(before),"zero-net preview preserves exact inherited/local native state");enter(window);
            check(encode(window.host.session.document())==encode(before)&&window.host.session.revision()==0&&
                window.host.session.history()==history&&!window.host.session.can_undo()&&!window.host.session.can_redo(),"zero-net numeric commit preserves bytes, inheritance, revision and History");
        }
        for(const auto* name:fields){number_enter(window,name);
            check(encode(window.host.session.document())==encode(before)&&window.host.session.revision()==0&&window.host.session.history()==history,"unchanged Grid double Enter preserves exact state/History");}
        widget<QPushButton>(window,"grid-apply")->click();events();
        const auto expected=expected_columns(before,mode,2);
        exact_grid(window.host.session.document(),expected,mode);
        check(window.host.session.revision()==(mode==1?1:0),"explicit Grid Apply retains family materialization semantics");
        if(mode==1){window.host.session.undo(window.host.session.revision());restore(window);
            check(encode(window.host.session.document())==encode(before)&&!window.host.session.can_undo(),"explicit inherited Apply has one exact Undo");}
    }
    const auto before=precision_fixture(1);load_precision(window,before,1);
    number_draft(window,"grid-y","24.123456789012344");draft(window,"+=0");
    auto expected=expected_columns(before,1,2);expected.compositions.front().artboards.back().layout->grid->bounds.y=24.123456789012344;
    exact_grid(window.host.session.preview_document(),expected,1);
    widget<QPushButton>(window,"grid-columns-cancel")->click();events();
    check(encode(window.host.session.document())==encode(before)&&window.host.session.revision()==0&&!window.host.session.gesture_active(),"Cancel of partially edited inherited Grid preserves inheritance and zero history");
    number_draft(window,"grid-y","24.123456789012344");draft(window,"+=0");enter(window);
    exact_grid(window.host.session.document(),expected,1);
    check(window.host.session.revision()==1,"zero columns delta does not discard an intentional sibling draft");
    window.host.session.undo(window.host.session.revision());restore(window);
    check(encode(window.host.session.document())==encode(before),"partially edited family has exact one Undo");
}
void precision_sibling_cases(Window& window){
    const char* fields[]={"grid-x","grid-y","grid-width","grid-height","grid-column-gutter","grid-row-gutter"};
    const char* values[]={"24.123456789012344","25.987654321098766","850.12345678901234","700.98765432109872","9.1234567890123444","10.987654321098766"};
    for(int mode=0;mode<3;++mode)for(int index=0;index<6;++index){
        const auto before=precision_fixture(mode);auto expected=expected_columns(before,mode,2);
        auto& grid=*expected.compositions.front().artboards[mode==0?0:1].layout->grid;
        double* destinations[]={&grid.bounds.x,&grid.bounds.y,&grid.bounds.width,&grid.bounds.height,&grid.column_gutter,&grid.row_gutter};
        *destinations[index]=QString::fromLatin1(values[index]).toDouble();load_precision(window,before,mode);
        number_draft(window,fields[index],QString::fromLatin1(values[index]));number_enter(window,fields[index]);
        exact_grid(window.host.session.document(),expected,mode);check(window.host.session.revision()==1,"intentional double edit commits without rounding its siblings");
        window.host.session.undo(window.host.session.revision());restore(window);
        check(encode(window.host.session.document())==encode(before)&&!window.host.session.can_undo(),"intentional double edit Undo restores exact bytes");
        window.host.session.redo(window.host.session.revision());restore(window);exact_grid(window.host.session.document(),expected,mode);
    }
    const auto before=precision_fixture(1);load_precision(window,before,1);number_draft(window,"grid-y","NaN");draft(window,"+=0");enter(window);
    check(encode(window.host.session.document())==encode(before)&&window.host.session.revision()==0&&!window.host.session.can_undo(),"zero-net columns cannot bypass invalid sibling draft atomicity");
    number_draft(window,"grid-y","25.987654321098766");draft(window,"+=3");enter(window);
    auto expected=expected_columns(before,1,5);expected.compositions.front().artboards.back().layout->grid->bounds.y=25.987654321098766;
    exact_grid(window.host.session.document(),expected,1);check(window.host.session.revision()==1,"correcting invalid sibling recovers and commits one exact family edit");
}

void precision_source_cases(Window& window){
    const char* fields[]={"grid-x","grid-y","grid-width","grid-height","grid-column-gutter","grid-row-gutter"};
    for(int mode:{0,2}){
        auto before=precision_fixture(mode);
        Artboard source{"other","Independent source",3000,0,25.123456789012344,800.3210987654321};
        before.compositions.front().artboards.push_back(source);
        before.compositions.front().artboards.push_back({"gutter-source","Gutter source",4500,0,9.987654321098766,10.123456789012344});
        auto& grid=*before.compositions.front().artboards[mode==0?0:1].layout->grid;
        grid.bounds_x_driver=Ref{"other","","artboard.width"};grid.bounds_y_expression=Expression{"24.234567890123456",1};
        grid.bounds_width_expression=Expression{"1000.3210987654321",1};grid.bounds_height_driver=Ref{"other","","artboard.height"};
        grid.column_gutter_driver=Ref{"gutter-source","","artboard.width"};grid.row_gutter_expression=Expression{"8.135791357913579",1};
        grid.rows_expression=Expression{"4",1};
        load_precision(window,before,mode);
        for(const auto* name:fields){auto* input=widget<QLineEdit>(window,name);check(input->isReadOnly(),"every driven Grid double is read-only");input->setText("not a number");}
        const auto expected=expected_columns(before,mode,5);draft(window,"+=3");enter(window);exact_grid(window.host.session.document(),expected,mode);
        check(window.host.session.revision()==1,"relative columns preserve six literal/driver/expression siblings in one transaction");
        persistence_cases(window,before,expected,mode);
    }
    for(int mode=0;mode<3;++mode){
        const auto before=precision_fixture(mode);load_precision(window,before,mode);draft(window,"+=3");enter(window);
        window.host.session.undo(window.host.session.revision());restore(window);
        const auto revision=window.host.session.revision();const auto history=window.host.session.history();draft(window,"+=0");enter(window);
        check(encode(window.host.session.document())==encode(before)&&window.host.session.revision()==revision&&
            window.host.session.history()==history&&window.host.session.can_redo(),"zero-net numeric edit preserves exact bytes and existing Redo cursor");
        window.host.session.redo(window.host.session.revision());restore(window);exact_grid(window.host.session.document(),expected_columns(before,mode,5),mode);
    }
}

void creation_and_stale_cases(Window& window){
    for(int mode:{0,1}){
        auto before=mode==0?fixture():precision_fixture(1);
        if(mode==0)before.compositions.front().artboards.front().layout->grid.reset();
        else{Session seeded(before);seeded.apply({ArtboardTemplateCommand{SetArtboardTemplateOverride{"comp","target","layout.grid",std::optional<Grid>{}}}},0);before=seeded.document();}
        load_precision(window,before,mode);draft(window,"+=0");
        const auto preview=window.host.session.preview_document();const auto& grid=authored_grid(preview,mode);
        check(!grid.id.empty()&&grid.columns==1&&grid.rows==1&&grid.bounds==LayoutRect{0,0,1200,900}&&grid.column_gutter==0&&grid.row_gutter==0,"zero delta still creates a previously absent Grid with established defaults");
        if(mode==1)check(grid.id==before.compositions.front().artboards.back().template_assignment->grid_id,"suppressed Template Grid recreation retains assignment identity");
        enter(window);exact_grid(window.host.session.document(),preview,mode);
        check(window.host.session.revision()==1,"new Grid creation remains one transaction");
        window.host.session.undo(window.host.session.revision());restore(window);
        check(encode(window.host.session.document())==encode(before)&&!window.host.session.can_undo(),"new Grid creation one Undo restores exact absent/suppressed state");
    }
    const auto before=precision_fixture(1);load_precision(window,before,1);draft(window,"+=3");
    window.host.session.cancel_gesture();window.host.session_id+="-replacement";enter(window);
    check(encode(window.host.session.document())==encode(before)&&window.host.session.revision()==0&&!window.host.session.can_undo(),"stale Host Session refuses precision relative draft without changing inheritance");
}

int transition_cases(Window&,bool diagnostic){
    int failures=0;
    for(int mode=0;mode<3;++mode)for(int scenario=0;scenario<3;++scenario){
        QTemporaryDir owned;check(owned.isValid(),"transition fixture scratch exists");
        QSettings settings(owned.filePath("library.ini"),QSettings::IniFormat);
        Window isolated(owned.path(),std::make_unique<FolderLibrary>(settings));
        auto& window=isolated;
        const auto before=precision_fixture(mode);window.host.session=Session(before);
        window.canvas->set_active_artboard("comp",mode==0?"board":"target",false);open(window);
        const auto bytes=encode(before);const auto revision=window.host.session.revision();const auto history=window.host.session.history();
        if(scenario==0){draft(window,"+=3");draft(window,"+=0");}
        else if(scenario==1){draft(window,"+=999999");draft(window,"+=0");}
        else{const auto displayed=widget<QLineEdit>(window,"grid-y")->text();number_draft(window,"grid-y","24.123456789012344");number_draft(window,"grid-y",displayed);}
        const bool preview_restored=encode(window.host.session.preview_document())==bytes;
        if(scenario==2)number_enter(window,"grid-y");else enter(window);
        const bool exact=encode(window.host.session.document())==bytes&&encode(window.host.session.preview_document())==bytes;
        const bool history_preserved=window.host.session.revision()==revision&&window.host.session.history()==history&&!window.host.session.can_undo()&&!window.host.session.can_redo();
        const bool closed=!window.host.session.gesture_active();
        const bool recovered=!window.statusBar()->currentMessage().startsWith("INVALID_");
        const bool passed=preview_restored&&exact&&history_preserved&&closed&&recovered;
        if(diagnostic){
            std::cout<<"mode="<<mode<<" scenario="<<scenario<<" preview_restored="<<preview_restored<<" exact="<<exact
                <<" history_preserved="<<history_preserved<<" gesture_closed="<<closed<<" invalidity_cleared="<<recovered<<"\n";
            if(!passed)++failures;
        }else{
            check(preview_restored,"zero-net transition restores exact committed preview");
            check(exact,"zero-net transition preserves exact authored/inherited/native bytes");
            check(history_preserved,"zero-net transition preserves revision and History");
            check(closed,"zero-net Enter closes only the prior owned Grid gesture");
            check(recovered,"valid zero-net correction clears prior Grid invalidity");
        }
    }
    if(diagnostic)std::cout<<(failures?"FAIL ":"PASS ")<<failures<<" Grid zero-net transition discrepancies\n";
    return failures?1:0;
}

void foreign_gesture_cases(Window&){
    for(int mode=0;mode<3;++mode)for(int scenario=0;scenario<4;++scenario){
        QTemporaryDir owned;check(owned.isValid(),"foreign gesture fixture scratch exists");
        QSettings settings(owned.filePath("library.ini"),QSettings::IniFormat);
        Window window(owned.path(),std::make_unique<FolderLibrary>(settings));
        const auto before=precision_fixture(mode);window.host.session=Session(before);
        window.canvas->set_active_artboard("comp",mode==0?"board":"target",false);open(window);draft(window,"+=3");
        auto& session=window.host.session;session.cancel_gesture();
        if(scenario==1){auto changed=session.document().compositions.front().artboards[mode==0?0:1];changed.name="External committed edit";
            session.apply({UpdateArtboard{"comp",changed}},session.revision());}
        else if(scenario==2){auto replacement=before;replacement.id="replacement-document";session=Session(replacement);}
        else if(scenario==3){session=Session(before);window.host.session_id+="-replacement";}
        const auto committed=session.document();const auto revision=session.revision();const auto history=session.history();
        session.begin_gesture(revision);auto changed=committed.compositions.front().artboards[mode==0?0:1];changed.name="Foreign gesture preview";
        session.update_gesture({UpdateArtboard{"comp",changed}});const auto foreign=encode(session.preview_document());const auto generation=session.gesture_generation();
        draft(window,"+=0");enter(window);
        check(session.gesture_active()&&session.gesture_generation()==generation,"Grid no-op refuses a newer foreign gesture generation");
        check(encode(session.preview_document())==foreign&&encode(session.document())==encode(committed),"Grid no-op preserves exact foreign preview and committed bytes");
        check(session.revision()==revision&&session.history()==history,"foreign gesture refusal preserves revision and History");
        session.cancel_gesture();restore(window);draft(window,"+=3");enter(window);
        exact_grid(session.document(),expected_columns(committed,mode,5),mode);
        check(session.revision()==revision+1,"Grid recovers after the foreign owner finishes");
        session.undo(session.revision());restore(window);check(encode(session.document())==encode(committed),"recovered Grid edit remains one exact Undo");
    }
    for(int mode=0;mode<3;++mode){
        QTemporaryDir owned;QSettings settings(owned.filePath("library.ini"),QSettings::IniFormat);
        Window window(owned.path(),std::make_unique<FolderLibrary>(settings));const auto before=precision_fixture(mode);
        window.host.session=Session(before);window.canvas->set_active_artboard("comp",mode==0?"board":"target",false);open(window);
        draft(window,"+=999999");auto& session=window.host.session;session.begin_gesture(session.revision());
        auto changed=before.compositions.front().artboards[mode==0?0:1];changed.name="Foreign preview after invalid Grid";session.update_gesture({UpdateArtboard{"comp",changed}});
        const auto foreign=encode(session.preview_document());const auto generation=session.gesture_generation();draft(window,"+=0");enter(window);
        check(session.gesture_active()&&session.gesture_generation()==generation&&encode(session.preview_document())==foreign,"invalid-to-zero refuses and preserves an unowned foreign gesture");
        session.cancel_gesture();draft(window,"+=0");enter(window);
        check(!session.gesture_active()&&encode(session.document())==encode(before)&&session.revision()==0&&!session.can_undo(),"invalid-to-zero recovers after the foreign gesture ends without History");
    }
}
void redo_transition_cases(Window&){
    for(int mode=0;mode<3;++mode){
        QTemporaryDir owned;QSettings settings(owned.filePath("library.ini"),QSettings::IniFormat);
        Window window(owned.path(),std::make_unique<FolderLibrary>(settings));const auto before=precision_fixture(mode);
        window.host.session=Session(before);window.canvas->set_active_artboard("comp",mode==0?"board":"target",false);open(window);
        draft(window,"+=3");enter(window);window.host.session.undo(window.host.session.revision());restore(window);
        const auto revision=window.host.session.revision();const auto history=window.host.session.history();draft(window,"+=3");draft(window,"+=0");enter(window);
        check(encode(window.host.session.document())==encode(before)&&encode(window.host.session.preview_document())==encode(before)&&
            !window.host.session.gesture_active()&&window.host.session.revision()==revision&&window.host.session.history()==history&&window.host.session.can_redo(),
            "changed-to-zero transition preserves exact existing Redo cursor and closes its owned preview");
        window.host.session.redo(window.host.session.revision());restore(window);exact_grid(window.host.session.document(),expected_columns(before,mode,5),mode);
    }
}

void dismiss_grid(Window& window,const char* action,bool process=true){
    if(std::string(action)=="Escape"){
        QKeyEvent escape(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier);
        check(QCoreApplication::sendEvent(widget<QLineEdit>(window,"grid-columns"),&escape),"actual Grid Escape key event delivered");
    }else widget<QPushButton>(window,action)->click();
    if(process){events();if(std::string(action)=="Escape")events();}
}
int lifecycle_cases(Window&,bool diagnostic){
    int failures=0;
    for(int mode=0;mode<3;++mode)for(const auto* action:{"grid-columns-cancel","Escape"}){
        QTemporaryDir owned;check(owned.isValid(),"dismissal fixture scratch exists");
        QSettings settings(owned.filePath("library.ini"),QSettings::IniFormat);
        Window window(owned.path(),std::make_unique<FolderLibrary>(settings));
        const auto before=precision_fixture(mode);window.host.session=Session(before);
        window.canvas->set_active_artboard("comp",mode==0?"board":"target",false);open(window);draft(window,"+=3");
        auto& session=window.host.session;const auto grid_generation=session.gesture_generation();session.cancel_gesture();
        session.begin_gesture(session.revision());auto changed=before.compositions.front().artboards[mode==0?0:1];
        changed.name="Live foreign gesture";session.update_gesture({UpdateArtboard{"comp",changed}});
        const auto preview=encode(session.preview_document());const auto generation=session.gesture_generation();const auto history=session.history();
        dismiss_grid(window,action);
        const bool kept=generation!=grid_generation&&session.gesture_active()&&session.gesture_generation()==generation&&encode(session.preview_document())==preview;
        const bool authored=encode(session.document())==encode(before)&&session.revision()==0&&session.history()==history&&!session.can_undo()&&!session.can_redo();
        if(diagnostic){std::cout<<"mode="<<mode<<" action="<<action<<" foreign_kept="<<kept<<" authored_preserved="<<authored<<"\n";if(!kept||!authored)++failures;}
        else{
            check(kept,"Grid Cancel or actual Escape preserves a newer foreign gesture and exact preview");
            check(authored,"obsolete Grid dismissal preserves exact native bytes, revision and History");
            session.commit_gesture();check(encode(session.document())==preview&&session.revision()==1&&session.can_undo(),"foreign owner can still commit its untouched preview after Grid dismissal");
            session.undo(session.revision());check(encode(session.document())==encode(before)&&!session.can_undo(),"one Undo restores exact bytes after preserved foreign commit");
        }
    }
    if(diagnostic)std::cout<<(failures?"FAIL ":"PASS ")<<failures<<" Grid dismissal ownership discrepancies\n";
    return failures?1:0;
}

void lifecycle_control_cases(Window&){
    const char* actions[]={"grid-bounds-x-cancel","grid-bounds-x-cancel-expression","grid-bounds-y-cancel","grid-bounds-y-cancel-expression",
        "grid-bounds-width-cancel","grid-bounds-width-cancel-expression","grid-bounds-height-cancel","grid-bounds-height-cancel-expression",
        "grid-columns-cancel","grid-columns-cancel-expression","grid-rows-cancel","grid-rows-cancel-expression",
        "grid-column-gutter-cancel","grid-row-gutter-cancel","Escape"};
    for(int mode=0;mode<3;++mode)for(const auto* action:actions)for(bool foreign:{false,true}){
        if(qEnvironmentVariableIsSet("NECT_GRID_TRACE_LIFECYCLE"))std::cout<<"dismissal control mode="<<mode<<" action="<<action<<" foreign="<<foreign<<" checks="<<checks<<"\n";
        QTemporaryDir owned;QSettings settings(owned.filePath("library.ini"),QSettings::IniFormat);
        Window window(owned.path(),std::make_unique<FolderLibrary>(settings));const auto before=precision_fixture(mode);
        window.host.session=Session(before);window.canvas->set_active_artboard("comp",mode==0?"board":"target",false);open(window);draft(window,"+=3");
        auto& session=window.host.session;std::string foreign_preview;std::uint64_t generation=0;
        if(foreign){session.cancel_gesture();session.begin_gesture(session.revision());
            auto changed=before.compositions.front().artboards[mode==0?0:1];changed.name="Foreign across Grid dismissal buttons";
            session.update_gesture({UpdateArtboard{"comp",changed}});foreign_preview=encode(session.preview_document());generation=session.gesture_generation();}
        const auto history=session.history();dismiss_grid(window,action);
        check(encode(session.document())==encode(before)&&session.revision()==0&&session.history()==history&&!session.can_undo()&&!session.can_redo(),"every Grid dismissal preserves exact authored bytes, revision and History");
        if(foreign){check(session.gesture_active()&&session.gesture_generation()==generation&&encode(session.preview_document())==foreign_preview,"every Grid dismissal leaves a live unowned generation and its exact preview intact");session.cancel_gesture();}
        else{
            check(!session.gesture_active()&&encode(session.preview_document())==encode(before),"every owned Grid dismissal cancels only its own preview");
            check(widget<QLineEdit>(window,"grid-columns")->text()=="2","owned Grid dismissal restores current displayed values");
            draft(window,"+=3");enter(window);exact_grid(session.document(),expected_columns(before,mode,5),mode);
            session.undo(session.revision());check(encode(session.document())==encode(before)&&!session.can_undo(),"editing after owned dismissal still has one exact Undo");
        }
    }
    for(int mode=0;mode<3;++mode)for(const auto* action:{"grid-columns-cancel","Escape"})for(int replacement=0;replacement<3;++replacement){
        QTemporaryDir owned;QSettings settings(owned.filePath("library.ini"),QSettings::IniFormat);
        Window window(owned.path(),std::make_unique<FolderLibrary>(settings));const auto before=precision_fixture(mode);
        window.host.session=Session(before);window.canvas->set_active_artboard("comp",mode==0?"board":"target",false);open(window);draft(window,"+=3");
        auto& session=window.host.session;session.cancel_gesture();
        if(replacement==0){auto changed=before.compositions.front().artboards[mode==0?0:1];changed.name="Newer committed revision";session.apply({UpdateArtboard{"comp",changed}},session.revision());}
        else if(replacement==1){auto document=before;document.id="replacement-document";session=Session(document);}
        else{session=Session(before);window.host.session_id+="-replacement";}
        const auto committed=encode(session.document());const auto revision=session.revision();const auto history=session.history();
        session.begin_gesture(revision);auto changed=session.document().compositions.front().artboards[mode==0?0:1];changed.name="Foreign replacement context";
        session.update_gesture({UpdateArtboard{"comp",changed}});const auto preview=encode(session.preview_document());const auto generation=session.gesture_generation();
        dismiss_grid(window,action);
        check(session.gesture_active()&&session.gesture_generation()==generation&&encode(session.preview_document())==preview,"Grid dismissal validates revision, Document and Host Session even when generations collide");
        check(encode(session.document())==committed&&session.revision()==revision&&session.history()==history,"replacement-context dismissal preserves exact committed bytes and History");
        session.cancel_gesture();
    }
    for(int mode=0;mode<3;++mode)for(const auto* action:{"grid-columns-cancel","Escape"})for(bool last_valid:{false,true}){
        QTemporaryDir owned;QSettings settings(owned.filePath("library.ini"),QSettings::IniFormat);
        Window window(owned.path(),std::make_unique<FolderLibrary>(settings));const auto before=precision_fixture(mode);
        window.host.session=Session(before);window.canvas->set_active_artboard("comp",mode==0?"board":"target",false);open(window);
        if(last_valid)draft(window,"+=3");draft(window,"+=999999");dismiss_grid(window,action);
        check(!window.host.session.gesture_active()&&encode(window.host.session.document())==encode(before)&&encode(window.host.session.preview_document())==encode(before)&&window.host.session.revision()==0&&!window.host.session.can_undo(),"owned invalid Grid dismissal restores exact authored and preview bytes without History");
        draft(window,"+=3");enter(window);exact_grid(window.host.session.document(),expected_columns(before,mode,5),mode);
    }
    for(int mode=0;mode<3;++mode)for(bool foreign:{false,true}){
        QTemporaryDir owned;QSettings settings(owned.filePath("library.ini"),QSettings::IniFormat);
        Window window(owned.path(),std::make_unique<FolderLibrary>(settings));const auto before=precision_fixture(mode);
        window.host.session=Session(before);window.canvas->set_active_artboard("comp",mode==0?"board":"target",false);open(window);
        draft(window,"+=999999");QPointer<QLineEdit> old=widget<QLineEdit>(window,"grid-columns");window.refresh();events();
        check(old&&!old->isVisible(),"invalid old Grid scope is hidden while awaiting deferred deletion");
        auto& session=window.host.session;std::string preview;std::uint64_t generation=0;
        if(foreign){session.begin_gesture(session.revision());auto changed=before.compositions.front().artboards[mode==0?0:1];changed.name="Foreign while hidden";
            session.update_gesture({UpdateArtboard{"comp",changed}});preview=encode(session.preview_document());generation=session.gesture_generation();}
        draft(window,"+=0");enter(window);QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);events();
        check(old.isNull(),"old Grid scope is safely destroyed by explicit DeferredDelete processing");
        check(encode(session.document())==encode(before)&&session.revision()==0&&!session.can_undo(),"hidden scope cleanup preserves exact authored bytes and History");
        if(foreign){check(session.gesture_active()&&session.gesture_generation()==generation&&encode(session.preview_document())==preview,"hidden Grid cleanup preserves a live foreign gesture");session.cancel_gesture();}
        else check(!session.gesture_active()&&encode(session.preview_document())==encode(before)&&!window.statusBar()->currentMessage().startsWith("INVALID_"),"hidden inactive Grid invalidity is retired without a gesture");
    }
    for(int mode=0;mode<3;++mode)for(bool foreign:{false,true}){
        QTemporaryDir owned;QSettings settings(owned.filePath("library.ini"),QSettings::IniFormat);
        Window window(owned.path(),std::make_unique<FolderLibrary>(settings));const auto before=precision_fixture(mode);
        window.host.session=Session(before);window.canvas->set_active_artboard("comp",mode==0?"board":"target",false);open(window);draft(window,"+=3");
        QPointer<QLineEdit> old=widget<QLineEdit>(window,"grid-columns");window.refresh();events();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);events();
        check(old.isNull(),"active old Grid scope is explicitly destroyed before dismissal");
        auto& session=window.host.session;std::string preview;std::uint64_t generation=0;
        if(foreign){session.cancel_gesture();session.begin_gesture(session.revision());auto changed=before.compositions.front().artboards[mode==0?0:1];changed.name="Foreign after active scope deletion";session.update_gesture({UpdateArtboard{"comp",changed}});preview=encode(session.preview_document());generation=session.gesture_generation();}
        QKeyEvent escape(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier);QCoreApplication::sendEvent(widget<QLineEdit>(window,"grid-columns"),&escape);events();events();
        check(encode(session.document())==encode(before)&&session.revision()==0&&!session.can_undo(),"dismissal after active scope deletion preserves exact native bytes and History");
        if(foreign){check(session.gesture_active()&&session.gesture_generation()==generation&&encode(session.preview_document())==preview,"Grid ownership survives scope destruction and preserves an unowned live gesture");session.cancel_gesture();}
        else check(!session.gesture_active()&&encode(session.preview_document())==encode(before),"deleted-scope dismissal still cancels its own exact Grid gesture");
    }
    for(int mode=0;mode<3;++mode)for(bool foreign:{false,true}){
        QTemporaryDir owned;QSettings settings(owned.filePath("library.ini"),QSettings::IniFormat);
        Window window(owned.path(),std::make_unique<FolderLibrary>(settings));const auto before=precision_fixture(mode);
        window.host.session=Session(before);window.canvas->set_active_artboard("comp",mode==0?"board":"target",false);open(window);draft(window,"+=3");
        QPointer<QLineEdit> input=widget<QLineEdit>(window,"grid-columns");dismiss_grid(window,"Escape",false);
        auto& session=window.host.session;check(!session.gesture_active(),"owned Escape cancels synchronously before deferred rebuild");
        if(foreign){session.begin_gesture(session.revision());auto changed=before.compositions.front().artboards[mode==0?0:1];changed.name="Foreign before Escape callback drains";session.update_gesture({UpdateArtboard{"comp",changed}});}
        else{input->setText("+=3");check(QMetaObject::invokeMethod(input,"textEdited",Qt::DirectConnection,Q_ARG(QString,QString("+=3"))),"new Grid sibling draft starts before old Escape callback drains");}
        const auto preview=encode(session.preview_document());const auto generation=session.gesture_generation();events();
        check(session.gesture_active()&&session.gesture_generation()==generation&&encode(session.preview_document())==preview,"deferred Grid Escape does not discard a subsequent real foreign or Grid draft");
        check(input&&input->isVisible()&&widget<QLineEdit>(window,"grid-columns")==input,"obsolete Escape callback does not rebuild a subsequent editor draft");
        if(foreign)session.cancel_gesture();else{enter(window);exact_grid(session.document(),expected_columns(before,mode,5),mode);}
    }
}

void lifecycle_collateral_cases(Window&){
    for(int mode=0;mode<3;++mode)for(const auto* action:{"grid-columns-cancel","Escape"}){
        QTemporaryDir owned;QSettings settings(owned.filePath("library.ini"),QSettings::IniFormat);
        Window window(owned.path(),std::make_unique<FolderLibrary>(settings));const auto before=precision_fixture(mode);
        window.host.session=Session(before);window.canvas->set_active_artboard("comp",mode==0?"board":"target",false);open(window);
        draft(window,"+=3");enter(window);auto& session=window.host.session;session.undo(session.revision());restore(window);
        const auto revision=session.revision();const auto history=session.history();draft(window,"+=3");dismiss_grid(window,action);
        check(!session.gesture_active()&&encode(session.document())==encode(before)&&encode(session.preview_document())==encode(before)&&session.revision()==revision&&session.history()==history&&session.can_redo(),"owned Grid dismissal preserves the exact existing Redo cursor and native bytes");
        session.redo(session.revision());restore(window);exact_grid(session.document(),expected_columns(before,mode,5),mode);
    }
    for(int mode=0;mode<3;++mode)for(bool escape:{false,true}){
        QTemporaryDir owned;QSettings settings(owned.filePath("library.ini"),QSettings::IniFormat);
        Window window(owned.path(),std::make_unique<FolderLibrary>(settings));const auto before=precision_fixture(mode);
        window.host.session=Session(before);window.canvas->set_active_artboard("comp",mode==0?"board":"target",false);open(window);
        draft(window,"+=3");dismiss_grid(window,"grid-columns-cancel");number_draft(window,"margin-left","15");
        auto& session=window.host.session;const auto preview=encode(session.preview_document());const auto generation=session.gesture_generation();
        widget<QPushButton>(window,"grid-columns-cancel")->click();events();
        check(session.gesture_active()&&session.gesture_generation()==generation&&encode(session.preview_document())==preview,"Grid Cancel does not discard another visible layout family's real draft");
        if(escape){QKeyEvent event(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier);QCoreApplication::sendEvent(widget<QLineEdit>(window,"margin-left"),&event);events();events();}
        else{widget<QPushButton>(window,"margin-left-cancel")->click();events();}
        check(!session.gesture_active()&&encode(session.document())==encode(before)&&encode(session.preview_document())==encode(before)&&session.revision()==0&&!session.can_undo(),"Margin owned Cancel/Escape remains unchanged after Grid ownership-token cleanup");
    }
}

int rebuilt_cancel_cases(Window&,bool diagnostic,bool delete_scope=true){
    int failures=0;
    for(int mode=0;mode<3;++mode)for(int scenario=0;scenario<(diagnostic?2:6);++scenario){
        QTemporaryDir owned;check(owned.isValid(),"rebuilt Cancel fixture scratch exists");
        QSettings settings(owned.filePath("library.ini"),QSettings::IniFormat);
        Window window(owned.path(),std::make_unique<FolderLibrary>(settings));const auto before=precision_fixture(mode);
        window.host.session=Session(before);window.canvas->set_active_artboard("comp",mode==0?"board":"target",false);open(window);
        if(scenario==5){draft(window,"+=3");enter(window);window.host.session.undo(window.host.session.revision());restore(window);}
        draft(window,"+=3");
        QPointer<QLineEdit> old=widget<QLineEdit>(window,"grid-columns");window.refresh();events();
        if(delete_scope){QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);events();}
        const bool retired=delete_scope?old.isNull():old&&!old->isVisible();auto* rebuilt=widget<QPushButton>(window,"grid-columns-cancel");
        auto& session=window.host.session;
        const bool foreign=scenario>0&&scenario<5;
        if(foreign){session.cancel_gesture();
            if(scenario==2){auto changed=before.compositions.front().artboards[mode==0?0:1];changed.name="Newer revision before rebuilt Cancel";session.apply({UpdateArtboard{"comp",changed}},session.revision());}
            else if(scenario==3){auto document=before;document.id="replacement-document";session=Session(document);}
            else if(scenario==4){session=Session(before);window.host.session_id+="-replacement";}
            session.begin_gesture(session.revision());auto changed=session.document().compositions.front().artboards[mode==0?0:1];changed.name="Foreign after active scope deletion";session.update_gesture({UpdateArtboard{"comp",changed}});
        }
        const auto committed=encode(session.document());const auto preview=encode(session.preview_document());
        const auto revision=session.revision();const auto history=session.history();const auto generation=session.gesture_generation();
        const bool can_undo=session.can_undo(),can_redo=session.can_redo();rebuilt->click();events();
        const bool correct=foreign?(session.gesture_active()&&session.gesture_generation()==generation&&encode(session.preview_document())==preview):
            (!session.gesture_active()&&encode(session.preview_document())==committed);
        const bool authored=encode(session.document())==committed&&session.revision()==revision&&session.history()==history&&session.can_undo()==can_undo&&session.can_redo()==can_redo;
        if(diagnostic){std::cout<<"mode="<<mode<<" scenario="<<scenario<<" phase="<<(delete_scope?"deleted":"hidden")<<" scope_retired="<<retired<<" correct="<<correct<<" authored_preserved="<<authored<<"\n";if(!retired||!correct||!authored)++failures;}
        else{
            check(retired,"rebuilt Cancel verifies the active old scope is explicitly deleted or hidden awaiting deletion");
            check(correct,"new visible Grid Cancel recovers its destroyed owned scope while preserving foreign/context previews");
            check(authored,"rebuilt Cancel preserves exact native bytes, revision, History and Undo/Redo cursor");
            if(foreign)session.cancel_gesture();
            else{
                check(widget<QLineEdit>(window,"grid-columns")->text()=="2","rebuilt owned Cancel restores current visible controls");
                if(scenario==5){check(session.can_redo(),"rebuilt owned Cancel retains the existing Redo entry");session.redo(session.revision());restore(window);}
                else{draft(window,"+=3");enter(window);}
                exact_grid(session.document(),expected_columns(before,mode,5),mode);
                session.undo(session.revision());check(encode(session.document())==encode(before)&&!session.can_undo(),"editing or Redo after rebuilt owned Cancel retains one exact Undo");
            }
        }
    }
    if(diagnostic)std::cout<<(failures?"FAIL ":"PASS ")<<failures<<" rebuilt Grid Cancel discrepancies\n";
    return failures?1:0;
}

int inherited_noop_probe(Window& window){
    int failures=0;
    for(const auto* edit:{"2","+=0","-=0"}){
        auto document=precision_fixture(1);document.compositions.front().artboards.front().layout->grid=fixture().compositions.front().artboards.front().layout->grid;
        load_precision(window,document,1);const auto before=encode(document);draft(window,edit);enter(window);
        const auto& target=window.host.session.document().compositions.front().artboards.back();
        if(encode(window.host.session.document())!=before||window.host.session.revision()!=0||window.host.session.can_undo()){
            ++failures;std::cerr<<"RED inherited numeric "<<edit<<": grid_overridden="<<target.template_assignment->grid_overridden
                <<", revision="<<window.host.session.revision()<<", can_undo="<<window.host.session.can_undo()<<"\n";
        }
    }
    std::cout<<(failures?"FAIL ":"PASS ")<<failures<<" inherited numeric no-op discrepancies\n";return failures?1:0;
}

int precision_probe(Window& window){
    int failures=0;const char* names[]={"plain","Template inherited","Template local override"};
    const char* fields[]={"bounds.x","bounds.y","bounds.width","bounds.height","column_gutter","row_gutter"};
    for(int mode=0;mode<3;++mode)for(const auto* edit:{"+=3","5"}){
        const auto document=precision_fixture(mode);const auto expected=expected_columns(document,mode,5);
        load_precision(window,document,mode);draft(window,edit);enter(window);
        const auto actual_values=decimals(authored_grid(window.host.session.document(),mode));
        const auto expected_values=decimals(authored_grid(expected,mode));
        for(std::size_t index=0;index<expected_values.size();++index){
            if(std::bit_cast<std::uint64_t>(actual_values[index])!=std::bit_cast<std::uint64_t>(expected_values[index])){
                ++failures;std::cerr<<"RED "<<names[mode]<<" "<<edit<<" "<<fields[index]<<" "<<std::setprecision(17)
                    <<expected_values[index]<<" -> "<<actual_values[index]<<"\n";
            }
        }
        if(encode(window.host.session.document())!=encode(expected)){++failures;std::cerr<<"RED exact native bytes: "<<names[mode]<<" "<<edit<<"\n";}
    }
    std::cout<<(failures?"FAIL ":"PASS ")<<failures<<" strict precision probe discrepancies\n";return failures?1:0;
}
}
int main(int argc,char** argv){
    qputenv("QT_QPA_PLATFORM","offscreen");QApplication app(argc,argv);
    try{
        if(argc==6&&std::string(argv[1])=="--cold-open")return cold_open(QString::fromLocal8Bit(argv[2]),QString::fromLocal8Bit(argv[3]),QString::fromLocal8Bit(argv[4]),std::stoi(argv[5]));
        QTemporaryDir scratch;check(scratch.isValid(),"owned test scratch exists");
        if(qEnvironmentVariableIsSet("NECT_GRID_KEEP_FIXTURES")){scratch.setAutoRemove(false);std::cout<<"Grid fixture directory: "<<scratch.path().toStdString()<<"\n";}
        QSettings settings(scratch.filePath("library.ini"),QSettings::IniFormat);
        Window window(scratch.path(),std::make_unique<FolderLibrary>(settings));
        if(argc==2&&std::string(argv[1])=="--rebuilt-hidden-cancel-probe"){open(window);return rebuilt_cancel_cases(window,true,false);}
        if(argc==2&&std::string(argv[1])=="--rebuilt-cancel-probe"){open(window);return rebuilt_cancel_cases(window,true);}
        if(argc==2&&std::string(argv[1])=="--lifecycle-controls-probe"){open(window);lifecycle_control_cases(window);std::cout<<"PASS "<<checks<<" lifecycle control checks\n";return 0;}
        if(argc==2&&std::string(argv[1])=="--lifecycle-probe"){open(window);return lifecycle_cases(window,true);}
        if(argc==2&&std::string(argv[1])=="--transition-probe"){open(window);return transition_cases(window,true);}
        if(argc==2&&std::string(argv[1])=="--inherited-noop-probe"){open(window);return inherited_noop_probe(window);}
        if(argc==2&&std::string(argv[1])=="--precision-probe"){open(window);return precision_probe(window);}
        literal_cases(window);boundary_cases(window);driven_cases(window);template_cases(window);
        precision_cases(window);precision_noop_cases(window);precision_sibling_cases(window);precision_source_cases(window);creation_and_stale_cases(window);transition_cases(window,false);foreign_gesture_cases(window);redo_transition_cases(window);lifecycle_cases(window,false);lifecycle_control_cases(window);lifecycle_collateral_cases(window);rebuilt_cancel_cases(window,false);rebuilt_cancel_cases(window,false,false);
        std::cout<<"PASS "<<checks<<" Grid columns relative UI checks\n";return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<" (after "<<checks<<" checks)\n";return 1;}
}

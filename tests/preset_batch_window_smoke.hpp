#pragma once
#include "macro_boolean_window_smoke.hpp"
#include "window.hpp"
#include <QAction>
#include <QComboBox>
#include <QDockWidget>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QTest>
#include <QWindow>
#include <iostream>
#include <set>

// Native-valid fixtures and actual production Window pointers. Synthetic Qt
// events establish wiring/context behavior, never physical OS-input acceptance.
namespace preset_batch_window_smoke {
using namespace nect;
using namespace nect::desktop;
inline int checks=0;
inline void check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
inline void events(){QApplication::processEvents();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QTest::qWait(20);}
inline bool same(const Session& a,const Session& b){return a.document()==b.document()&&a.preview_document()==b.preview_document()&&
    encode(a.document())==encode(b.document())&&a.history()==b.history()&&a.revision()==b.revision()&&
    a.gesture_generation()==b.gesture_generation()&&a.gesture_active()==b.gesture_active();}
inline Document fixture(unsigned pin){
    auto document=macro_boolean_window_smoke::fixture();
    PresetDefinition preset;preset.id="selected-preset";preset.schema_version=2;preset.label="Retained authored stack";
    preset.category="Local";preset.tags={"ordered","retained"};
    if(pin){PresetEntry entry;entry.kind="macro";entry.type=macro_entry_type;entry.macro_definition="boolean-definition";
        entry.pinned_revision=pin;entry.overrides[macro_boolean_window_smoke::amount_id]=4.25;
        if(pin==2){entry.overrides[macro_boolean_window_smoke::copies_id]=3;
            entry.boolean_overrides[macro_boolean_window_smoke::offset_enabled_id]=true;
            entry.boolean_overrides[macro_boolean_window_smoke::repeater_enabled_id]=false;}
        preset.entries.push_back(entry);
    }else{auto offset=default_operation("fixture-offset","nect.shape.offset");offset.parameters.at("amount").literal=3.25;
        PresetEntry entry;entry.type=offset.type;for(const auto& [key,value]:offset.parameters)entry.parameters[key]=value.literal;
        preset.entries.push_back(entry);}
    auto stroke=default_operation("fixture-stroke","nect.paint.stroke");stroke.version=2;
    stroke.parameters.at("width").literal=3.5;stroke.parameters.emplace("miter_limit",Scalar{9,{}});
    PresetEntry style;style.type=stroke.type;style.version=2;style.line_join="bevel";style.line_cap="round";style.composite="above";
    for(const auto& [key,value]:stroke.parameters)style.parameters[key]=value.literal;
    preset.entries.push_back(style);document.preset_definitions.emplace(preset.id,preset);
    auto unrelated=preset;unrelated.id="unselected-preset";unrelated.label="Independent Preset";
    document.preset_definitions.emplace(unrelated.id,unrelated);
    Session retained(document);retained.apply({Link{{"legacy","legacy-point-1","x"},Binding{{"path","path-point-1","x"},1,2}}},0);
    return retained.document();
}
inline void history(Window& w,const char* label){for(auto* action:w.findChildren<QAction*>())if(action->text()==QString::fromLatin1(label)){
    check(action->isEnabled(),"Production history action is enabled");action->trigger();events();return;}throw std::runtime_error("History action missing");}
inline void cold(Window& w,const Session& expected){QTemporaryDir files;check(files.isValid(),"Owned native cold directory exists");
    const auto path=files.filePath("preset-batch.nect");w.host.save(path);w.host.flush();
    check(same(w.host.session,expected),"Native save preserves the whole live Session/history");
    Window reopened(files.filePath("recovery"));reopened.setAttribute(Qt::WA_DontShowOnScreen);reopened.resize(1400,900);
    reopened.host.open(path);reopened.show();events();
    check(reopened.host.session.document()==expected.document()&&encode(reopened.host.session.document())==encode(expected.document()),
        "Cold production Window preserves native087, complete sources/Refs, exact entries, pins, overrides and Presets");
    reopened.host.changed={};reopened.hide();events();
}
inline void context(unsigned pin,const std::string& cause,bool mixed_text=false){
    QTemporaryDir scratch;check(scratch.isValid(),"Preset test owns scratch");auto document=fixture(pin);
    if(mixed_text){auto& object=document.objects.at("other");object.kind=Kind::text;object.contours.clear();
        object.text=default_text("other-text","保持する日本語 Text");}
    check(decode(encode(document))==document,"Preset and retained target dependencies native-validate before GUI");
    Window w(scratch.filePath("recovery"));w.setAttribute(Qt::WA_DontShowOnScreen);w.resize(1400,900);
    w.host.session=Session(document);w.host.session_id="preset-batch-context";w.host.edited();w.show();events();
    w.canvas->set_active_artboard("composition","artboard",false);w.canvas->set_selections({{"path",{}},{"other",{}}});events();
    auto* properties=w.findChild<QDockWidget*>("properties");check(properties,"Properties dock exists");properties->show();properties->raise();events();
    auto* area=w.findChild<QScrollArea*>("inspector-scroll");auto* catalog=w.findChild<QComboBox*>("preset-batch-catalog");
    auto* apply=w.findChild<QPushButton*>("preset-batch-apply");auto* cancel=w.findChild<QPushButton*>("preset-batch-cancel");
    check(area&&catalog&&apply&&cancel,"Production whole-selection Preset panel exists");
    const Session original=w.host.session;const auto index=catalog->findData("selected-preset");check(index>0,"Catalog selects the exact Preset ID");
    catalog->setCurrentIndex(index);events();check(apply->isEnabled()&&same(w.host.session,original),"Preset browsing is completely Session-neutral");
    auto chosen=document.preset_definitions.at("selected-preset");
    auto* action=cause=="cancel"?cancel:apply;area->ensureWidgetVisible(action);events();
    const auto position=action->mapTo(&w,action->rect().center());check(w.childAt(position)==action,"Actual Window pointer hits exact Preset Apply/Cancel");
    const bool replacement=cause=="source"||cause=="rechoose"||cause=="metadata"||cause=="dependency"||cause=="target"||cause=="target-override"||
        cause=="target-pinned"||cause=="document"||cause=="equivalent"||cause=="unrelated"||cause=="unselected"||cause=="later"||cause=="old-source";
    if(replacement){auto incoming=w.host.session.document();
        if(cause=="source"||cause=="rechoose")incoming.preset_definitions.at("selected-preset").entries.back().parameters.at("width")=8;
        else if(cause=="metadata")incoming.preset_definitions.at("selected-preset").label="Incoming named Preset";
        else if(cause=="dependency"||cause=="target-pinned"||cause=="old-source"){
            const auto changed=cause=="dependency"?pin:cause=="target-pinned"?2u:1u;
            for(auto& node:incoming.macro_definitions.at("boolean-definition").revisions.at(changed).nodes)
                if(node.operation.id=="offset-target")node.operation.parameters.at("amount").literal=17;
        }else if(cause=="target")incoming.objects.at("other").contours.front().points.front().x.literal=9;
        else if(cause=="target-override"){
            for(auto& entry:incoming.objects.at("other").stack)if(entry.macro)entry.macro->overrides[macro_boolean_window_smoke::amount_id]=17;
        }
        else if(cause=="document")incoming.id="incoming-preset-document";
        else if(cause=="unrelated")incoming.objects.at("legacy").name="Independent retained source";
        else if(cause=="unselected")incoming.preset_definitions.at("unselected-preset").entries.back().parameters.at("width")=12;
        else if(cause=="later"){auto& definition=incoming.macro_definitions.at("boolean-definition");auto graph=definition.revisions.at(2);
            graph.revision=3;definition.revisions.emplace(3,graph);definition.latest_revision=3;}
        check((incoming==w.host.session.document())==(cause=="equivalent"),"Replacement changes the intended exact incoming context");
        check(decode(encode(incoming))==incoming,"Incoming same-revision replacement is independently native-valid");w.host.session=Session(incoming);
    }else if(cause=="session")w.host.session_id+="-replacement";
    else if(cause=="revision")w.host.session.apply({Rename{"legacy","Incoming revision"}},w.host.session.revision());
    else if(cause=="generation"||cause=="preview"){
        w.host.session.begin_gesture(w.host.session.revision());w.host.session.update_gesture({Rename{"legacy","Incoming preview"}});
        if(cause=="generation")w.host.session.cancel_gesture();
    }
    if(cause=="rechoose"){
        const Session before=w.host.session;catalog->setCurrentIndex(0);catalog->setCurrentIndex(index);events();
        check(same(w.host.session,before)&&apply->isEnabled(),"Explicit re-choosing accepts current source without an authored edit");
        chosen=w.host.session.document().preset_definitions.at("selected-preset");
    }
    Session expected=w.host.session;QTest::mouseClick(w.windowHandle(),Qt::LeftButton,Qt::NoModifier,position);events();
    const bool refusal=cause=="source"||cause=="metadata"||cause=="dependency"||cause=="target"||cause=="target-override"||
        cause=="target-pinned"||cause=="document"||cause=="session"||cause=="revision"||cause=="generation"||cause=="preview";
    if(refusal){std::cout<<"Preset pin"<<pin<<" "<<cause<<" actual_revision="<<w.host.session.revision()<<" expected_revision="<<expected.revision()<<std::endl;
        check(same(w.host.session,expected),"Stale Preset batch preserves incoming Document/native/History/preview/revision/generation atomically");
        check(catalog->currentData().toString()=="selected-preset","Refusal retains the chosen exact Preset draft");
        const auto* status=w.findChild<QLabel*>("preset-batch-status");
        const auto code=cause=="document"||cause=="session"?"SESSION_CONFLICT":cause=="preview"?"GESTURE_ACTIVE":
            cause=="revision"||cause=="generation"?"REVISION_CONFLICT":"PROPERTY_CONFLICT";
        check(status&&status->text().startsWith(code),"Exact refusal code identifies source/target/edit context");
    }else if(cause=="cancel")check(same(w.host.session,expected)&&catalog->currentIndex()==0&&!apply->isEnabled(),"Cancel clears only the Preset draft");
    else{
        ApplyPresetBatch batch;batch.preset="selected-preset";auto manual=expected.document();std::set<Id> fresh;
        for(const auto* id:{"path","other"}){
            const auto before=expected.document().objects.at(id).stack.size();const auto& after=w.host.session.document().objects.at(id).stack;
            check(after.size()==before+chosen.entries.size(),"One Apply appends the complete chosen sequence per retained target");
            const auto& first_id=after.at(before).id;const std::string suffix="-op-1";
            check(first_id.ends_with(suffix),"Actual fresh prefix follows existing canonical batch allocation");
            const auto prefix=first_id.substr(0,first_id.size()-suffix.size());batch.targets.push_back({id,prefix});
            for(std::size_t i=0;i<chosen.entries.size();++i){const auto& entry=chosen.entries[i];const auto entry_id=prefix+"-op-"+std::to_string(i+1);
                check(after[before+i].id==entry_id&&fresh.insert(entry_id).second,"Stable fresh entries are unique and ordered across all targets");
                ProcessingEntry result;
                if(entry.kind=="macro"){result.id=entry_id;result.type=macro_entry_type;result.enabled=entry.enabled;
                    result.macro=MacroInstance{entry.macro_definition,entry.pinned_revision,entry.overrides,entry.boolean_overrides};}
                else{auto op=default_operation(entry_id,entry.type);op.version=entry.version;op.enabled=entry.enabled;
                    if(entry.version==2&&entry.type=="nect.paint.stroke")op.parameters.emplace("miter_limit",Scalar{4,{}});
                    for(auto& [key,value]:op.parameters)value.literal=entry.parameters.at(key);
                    op.composite=entry.composite;op.fill_rule=entry.fill_rule;op.line_join=entry.line_join;op.line_cap=entry.line_cap;result=ProcessingEntry{op};}
                manual.objects.at(id).stack.push_back(result);
            }
        }
        check(w.host.session.document()==manual,"Complete independent Document retains all sources/styles/Refs, pins/overrides and unrelated state");
        expected.apply_preset_command(PresetCommand{batch},expected.revision());check(same(w.host.session,expected),"Production Apply equals independent canonical atomic batch/history");
        history(w,"Undo");expected.undo(expected.revision());check(same(w.host.session,expected),"One Undo restores every independent target and source");
        history(w,"Redo");expected.redo(expected.revision());check(same(w.host.session,expected),"One Redo restores the exact fresh entries and complete history");cold(w,expected);
    }
    if(w.host.session.gesture_active())w.host.session.cancel_gesture();w.host.changed={};w.hide();events();
    std::cout<<"PASS existing Preset pin"<<pin<<" / "<<cause<<(mixed_text?" / mixed Text":"")<<"; physical input NOT_RUN\n";
}
inline int run(){bool failed=false;for(unsigned pin:{0u,1u,2u})for(const auto* cause:{"valid","cancel","source","metadata","dependency","target","target-override","target-pinned",
    "document","session","revision","generation","preview","equivalent","unrelated","unselected","later","old-source"}){
    if((pin==0&&std::string(cause)=="dependency")||(pin==1&&std::string(cause)=="old-source")||(pin==2&&std::string(cause)=="target-pinned"))continue;
    try{context(pin,cause);}catch(const std::exception& error){failed=true;std::cerr<<"Preset pin"<<pin<<" "<<cause<<": "<<error.what()<<'\n';}
    }
    for(unsigned pin:{0u,1u,2u})try{context(pin,"rechoose");}catch(const std::exception& error){failed=true;std::cerr<<"Preset rechoose pin"<<pin<<": "<<error.what()<<'\n';}
    for(unsigned pin:{0u,2u})try{context(pin,"valid",true);}catch(const std::exception& error){failed=true;std::cerr<<"Preset mixed Text pin"<<pin<<": "<<error.what()<<'\n';}
    check(!failed,"Existing Preset batch preserves exact chosen source/whole targets/retained dependencies");return checks;}
}

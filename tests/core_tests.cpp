#include "nect/io.hpp"
#include <cmath>
#include <iostream>
#include <limits>

using namespace nect;

namespace {
int count=0;
void check(bool b,const std::string& reason) {
    if(!b) throw std::runtime_error(reason);
    ++count;
}
template<class F> void rejects(const std::string& code,F action) {
    try { action(); }
    catch(const Error& e) {
        check(e.code==code,"Wrong error: "+e.code+" expected "+code);
        return;
    }
    throw std::runtime_error("Expected rejection: "+code);
}
}

int main() {
    try {
        const Ref ax{"path-A","point-A1","x"}, bx{"path-B","point-B1","x"};
        const auto original=demo_document();
        Session s(original);

        check(evaluate(s.document()).at(ax)==100,"Literal coordinate");
        s.apply({Link{bx,{ax,2,5,"copy_local_value"}}},0);
        check(evaluate(s.document()).at(bx)==205,"100*2+5");
        s.apply({Set{ax,120}},1);
        check(evaluate(s.document()).at(bx)==245,"120*2+5");
        rejects("DRIVEN_PROPERTY",[&]{s.apply({Set{bx,3}},2);});

        const auto before=encode(s.document());
        rejects("DEPENDENCY_CYCLE",[&]{s.apply({Link{ax,{bx,1,0,"copy_local_value"}}},2);});
        check(s.revision()==2&&encode(s.document())==before,"Cycle rejection is atomic");
        rejects("REVISION_CONFLICT",[&]{s.apply({Set{ax,0}},0);});
        rejects("NON_FINITE",[&]{s.apply({Set{ax,std::numeric_limits<double>::infinity()}},2);});
        rejects("MISSING_REFERENCE",[&]{s.apply({Set{ax,777},Set{{"absent","","x"},10}},2);});
        check(encode(s.document())==before&&s.revision()==2,"Failed batch preserves document");
        rejects("UNIT_MISMATCH",[&]{s.apply({Link{ax,{{"path-A","point-A1","in.angle"},1,0,"copy_local_value"}}},2);});

        s.apply({Rename{"path-A","Renamed"},
                 ReorderPoints{"path-A","contour-A",{"point-A2","point-A1"}}},2);
        check(evaluate(s.document()).at(bx)==245,"Rename/reorder preserve stable references");
        check(resolve_name(s.document(),"Renamed","point-A1","x")==ax,"Name resolves to stable ID");

        s.undo(3);
        check(s.document().objects.at("path-A").name=="Curve A"&&s.revision()==4,"Undo restores state");
        s.redo(4);
        check(s.document().objects.at("path-A").name=="Renamed","Redo restores edit");

        s.apply({Unlink{bx}},5);
        check(!property(s.document(),bx).binding&&property(s.document(),bx).literal==245,
              "Unlink freezes evaluated value");
        s.apply({Set{ax,130}},6);
        check(evaluate(s.document()).at(bx)==245,"Unlinked property independent");

        const auto stored=encode(s.document());
        check(encode(decode(stored))==stored,"Deterministic native save/load");

        auto invalid=original;
        invalid.objects.at("path-B").contours[0].points[0].id="point-A1";
        rejects("DUPLICATE_ID",[&]{validate(invalid);});
        invalid=original;
        invalid.compositions[0].roots.pop_back();
        rejects("ORPHAN_OBJECT",[&]{validate(invalid);});
        invalid=original;
        invalid.compositions[0].roots.push_back("path-A");
        rejects("INVALID_HIERARCHY",[&]{validate(invalid);});

        Session grouped(original);
        rejects("NONCONTIGUOUS_GROUP",[&]{
            grouped.apply({GroupContiguous{"comp-main","",{"path-B","path-A"},"group-bad","Bad"}},0);
        });
        grouped.apply({GroupContiguous{"comp-main","",{"path-A","path-B"},"group-good","Group"}},0);
        check(grouped.document().objects.at("group-good").children==std::vector<Id>({"path-A","path-B"}),
              "Grouping retains child order");
        check(property(grouped.document(),ax).literal==100,"Neutral grouping retains local geometry");

        auto plain=original;
        auto mapped=original;
        mapped.collections={{"set-one","Set one",{"path-A","path-B"}},{"set-two","Set two",{"path-A"}}};
        check(export_svg(plain,"comp-main","art-main")==export_svg(mapped,"comp-main","art-main"),
              "Membership alone does not render");

        mapped.objects.at("path-A").name="<Tag>&\"";
        check(export_svg(mapped,"comp-main","art-main").find("&lt;Tag&gt;&amp;&quot;")!=std::string::npos,
              "XML text escaping");
        check(export_svg(original,"comp-main","art-main").find("M 100 150 C ")!=std::string::npos,
              "Hand specified initial SVG coordinate");
        rejects("MISSING_ARTBOARD",[&]{export_svg(original,"comp-main","missing");});

        auto zero=original;
        auto& p=zero.objects.at("path-A").contours[0].points[0];
        p.in_length.literal=0;
        p.in_angle.literal=72;
        check(property(decode(encode(zero)),{"path-A","point-A1","in.angle"}).literal==72,
              "Zero-length handle keeps authored angle");

        Session author(empty_document("new-doc","new-comp","new-art"));
        Point p1,p2; p1.id="new-p1";p1.x.literal=10;p1.y.literal=20;
        p2.id="new-p2";p2.x.literal=90;p2.y.literal=20;
        author.apply({CreatePath{"new-comp","","new-path","Drawn",{{"new-contour",false,{p1}}}},
            AddPoint{"new-path","new-contour",p2}},0);
        const Ref created{"new-path","new-p1","x"};
        check(author.document().objects.size()==1 && property(author.document(),created).literal==10,
            "Create from empty document through commands");
        const auto start=encode(author.document());
        author.begin_gesture(1);
        check(!author.preview_values(),"Beginning preview has no stale evaluated projection");
        author.update_gesture({Set{created,40}});
        check(author.preview_values()&&*author.preview_values()==evaluate(author.preview_document()),"Validated preview values equal independent evaluation");
        check(author.revision()==1 && encode(author.document())==start &&
            property(author.preview_document(),created).literal==40,"Preview is not committed state");
        rejects("GESTURE_ACTIVE",[&]{author.apply({Set{created,999}},1);});
        rejects("GESTURE_ACTIVE",[&]{author.undo(1);});
        rejects("OUT_OF_RANGE",[&]{author.update_gesture({Set{{"new-path","new-p1","in.length"},-2}});});
        check(property(author.preview_document(),created).literal==40,"Failed preview preserves valid preview");
        check(author.preview_values()&&author.preview_values()->at(created)==40,"Failed update retains matching last-valid derived values");
        author.cancel_gesture();
        check(!author.preview_values(),"Cancellation discards derived preview values");
        check(author.revision()==1 && encode(author.document())==start,"Cancel retains revision and authored state");
        author.begin_gesture(1);
        author.update_gesture({Set{created,30}});
        author.update_gesture({Set{created,50}});
        author.commit_gesture();
        check(!author.preview_values(),"Commit invalidates preview-derived values");
        author.undo(2);
        check(encode(author.document())==start,"Whole gesture has one undo entry");
        author.redo(3);
        check(property(author.document(),created).literal==50,"Gesture redo");
        author.begin_gesture(4);
        author.update_gesture({Set{created,90}});author.update_gesture({});check(!author.preview_values(),"Empty update cannot retain displaced evaluation");author.commit_gesture();
        check(author.revision()==4 && property(author.document(),created).literal==50,"Drag back to start is no-op");
        author.apply({CloseContour{"new-path","new-contour",true},
            ReorderPoints{"new-path","new-contour",{"new-p2","new-p1"}}},4);
        check(property(author.document(),created).literal==50,"Created point identity survives reorder");
        Point p3;p3.id="other-p";
        author.apply({CreatePath{"new-comp","","other-path","Other",{{"other-contour",false,{p3}}}},
            Link{{"other-path","other-p","x"},{created,1,0,"copy_local_value"}}},5);
        const auto linked=encode(author.document());
        rejects("MISSING_REFERENCE",[&]{author.apply({DeleteObjects{{"new-path"}}},6);});
        check(encode(author.document())==linked&&author.revision()==6,"Deletion cannot break surviving references");
        author.apply({Unlink{{"other-path","other-p","x"}},DeleteObjects{{"new-path"}}},6);
        check(author.document().objects.size()==1&&evaluate(author.document()).at({"other-path","other-p","x"})==50,
            "Explicit freeze and delete is atomic");
        check(encode(decode(encode(author.document())))==encode(author.document()),"New operations round trip unchanged schema");

        std::cout<<"PASS "<<count<<" core checks\n";
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"FAIL: "<<e.what()<<'\n';
        return 1;
    }
}

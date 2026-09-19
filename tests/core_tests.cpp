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

        std::cout<<"PASS "<<count<<" core checks\n";
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"FAIL: "<<e.what()<<'\n';
        return 1;
    }
}

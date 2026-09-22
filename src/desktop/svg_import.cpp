#include "svg_import.hpp"
#include <QColor>
#include <QRegularExpression>
#include <QXmlStreamReader>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <numbers>
#include <set>

namespace nect::desktop {
namespace {
void need(bool condition,const char* code,const std::string& message) {if(!condition)throw Error(code,message);}
constexpr Affine identity{1,0,0,1,0,0};
// SVG's compact number grammar permits adjacent signs and decimal points.
class Numbers {
public:
    QString text;qsizetype at=0;
    explicit Numbers(QString value):text(std::move(value)){}
    void space(){while(at<text.size()&&text[at].isSpace())++at;}
    bool end(){space();return at==text.size();}
    bool number_ahead(){space();return at<text.size()&&(text[at].isDigit()||text[at]=='+'||text[at]=='-'||text[at]=='.');}
    double number() {
        space();static const QRegularExpression pattern(R"([+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][+-]?[0-9]+)?)");
        const auto match=pattern.match(text,at,QRegularExpression::NormalMatch,QRegularExpression::AnchorAtOffsetMatchOption);
        need(match.hasMatch(),"SVG_SYNTAX","Expected a finite SVG number");at=match.capturedEnd();
        bool ok=false;const auto value=match.captured().toDouble(&ok);need(ok&&std::isfinite(value)&&std::abs(value)<=1e7,"SVG_RANGE","SVG number exceeds supported range");
        return value;
    }
    double following(){space();if(at<text.size()&&text[at]==','){++at;space();}return number();}
};
double scalar(QString text,bool length=false) {
    text=text.trimmed();if(length&&text.endsWith("px"))text.chop(2);
    Numbers n(text);const auto result=n.number();need(n.end(),"SVG_UNSUPPORTED","Only finite unitless/px lengths are supported");return result;
}
double opacity(QString text) {
    text=text.trimmed();const bool percent=text.endsWith('%');if(percent)text.chop(1);
    return std::clamp(scalar(text)/(percent?100:1),0.0,1.0);
}
Affine transform(QString text) {
    Numbers n(text);Affine result=identity;
    while(!n.end()) {
        const auto start=n.at;while(n.at<n.text.size()&&n.text[n.at].isLetter())++n.at;
        const auto name=n.text.mid(start,n.at-start);n.space();need(n.at<n.text.size()&&n.text[n.at++]=='(' ,"SVG_SYNTAX","Invalid transform list");
        std::vector<double> v;n.space();if(n.at<n.text.size()&&n.text[n.at]!=')') {
            v.push_back(n.number());while(true){n.space();if(n.at>=n.text.size()||n.text[n.at]==')')break;v.push_back(n.following());need(v.size()<=6,"SVG_SYNTAX","Too many transform arguments");}
        }
        need(n.at<n.text.size()&&n.text[n.at++]==')',"SVG_SYNTAX","Unclosed transform");Affine next=identity;
        if(name=="matrix"&&v.size()==6)std::copy(v.begin(),v.end(),next.begin());
        else if(name=="translate"&&(v.size()==1||v.size()==2)){next[4]=v[0];next[5]=v.size()==2?v[1]:0;}
        else if(name=="scale"&&(v.size()==1||v.size()==2)){next[0]=v[0];next[3]=v.size()==2?v[1]:v[0];}
        else if(name=="rotate"&&(v.size()==1||v.size()==3)) {
            const auto a=v[0]*std::numbers::pi/180,c=std::cos(a),s=std::sin(a);next={c,s,-s,c,0,0};
            if(v.size()==3){next[4]=v[1]-c*v[1]+s*v[2];next[5]=v[2]-s*v[1]-c*v[2];}
        } else if((name=="skewX"||name=="skewY")&&v.size()==1)next[name=="skewX"?2:1]=std::tan(v[0]*std::numbers::pi/180);
        else throw Error("SVG_UNSUPPORTED","Unsupported transform or argument count: "+name.toStdString());
        result=compose(result,next);for(auto value:result)need(std::isfinite(value)&&std::abs(value)<=1e7,"SVG_RANGE","Transform exceeds supported range");
        n.space();if(n.at<n.text.size()&&n.text[n.at]==','){++n.at;need(!n.end(),"SVG_SYNTAX","Trailing transform separator");}
    }
    return result;
}
void handle(Point& p,bool outgoing,Vec2 control) {
    const auto dx=control.x-p.x.literal,dy=control.y-p.y.literal;
    (outgoing?p.out_length:p.in_length).literal=std::hypot(dx,dy);
    (outgoing?p.out_angle:p.in_angle).literal=std::atan2(dy,dx)*180/std::numbers::pi;
}
std::vector<Contour> path(QString text,const Id& id,std::size_t& total) {
    Numbers n(text);std::vector<Contour> contours;Vec2 current{},start{},control{};char previous=0,command=0;std::size_t point_id=0;
    auto add=[&](Vec2 v){need(++total<=10000,"SVG_LIMIT","SVG point limit10000");Point p;p.id=id+"-p"+std::to_string(point_id++);p.x.literal=v.x;p.y.literal=v.y;contours.back().points.push_back(p);current=v;};
    auto move=[&](Vec2 v){Contour c;c.id=id+"-c"+std::to_string(contours.size());contours.push_back(c);add(v);start=v;};
    while(!n.end()) {
        if(n.text[n.at].isLetter())command=n.text[n.at++].toLatin1();else need(command!=0,"SVG_SYNTAX","Path command required");
        const auto upper=static_cast<char>(std::toupper(static_cast<unsigned char>(command)));const bool relative=command!=upper;
        need(std::string("MLHVCSQTZ").find(upper)!=std::string::npos,"SVG_UNSUPPORTED","Unsupported path command (elliptical arcs are not yet supported)");
        if(upper=='Z') {
            need(!contours.empty()&&!contours.back().closed,"SVG_SYNTAX","Close requires an open subpath");auto& c=contours.back();
            if(c.points.size()>1&&current.x==start.x&&current.y==start.y){c.points.front().in_angle=c.points.back().in_angle;c.points.front().in_length=c.points.back().in_length;c.points.pop_back();}
            c.closed=true;current=start;previous='Z';command=0;continue;
        }
        need(upper=='M'||!contours.empty(),"SVG_SYNTAX","Path must start with moveto");
        bool first=true;
        auto value=[&](){const auto v=first?n.number():n.following();first=false;return v;};
        const auto origin=current;
        auto pair=[&](){Vec2 p{value(),value()};if(relative){p.x+=origin.x;p.y+=origin.y;}return p;};
        if(upper=='M'){move(pair());command=relative?'l':'L';previous='M';}
        else {
            if(contours.back().closed)move(current);
            if(upper=='L')add(pair());
            else if(upper=='H'){const auto x=value();add({x+(relative?origin.x:0),origin.y});}
            else if(upper=='V'){const auto y=value();add({origin.x,y+(relative?origin.y:0)});}
            else {
                Vec2 c1{},c2{},end{};
                if(upper=='C'){c1=pair();c2=pair();end=pair();control=c2;}
                else if(upper=='S'){c1=(previous=='C'||previous=='S')?Vec2{2*origin.x-control.x,2*origin.y-control.y}:origin;c2=pair();end=pair();control=c2;}
                else {const auto q=upper=='Q'?pair():((previous=='Q'||previous=='T')?Vec2{2*origin.x-control.x,2*origin.y-control.y}:origin);end=pair();
                    c1={origin.x+2*(q.x-origin.x)/3,origin.y+2*(q.y-origin.y)/3};c2={end.x+2*(q.x-end.x)/3,end.y+2*(q.y-end.y)/3};control=q;}
                handle(contours.back().points.back(),true,c1);add(end);handle(contours.back().points.back(),false,c2);
            }
            previous=upper;
        }
        n.space();if(n.at<n.text.size()&&n.text[n.at]==','){++n.at;need(n.number_ahead(),"SVG_SYNTAX","Trailing path separator");}
    }
    need(!contours.empty(),"SVG_SYNTAX","Empty path data is unsupported");return contours;
}
struct Style {QString fill="black",stroke="none",rule="nonzero";double fill_alpha=1,stroke_alpha=1,width=1;};
using Attributes=std::map<QString,QString>;
// Basic shapes become editable paths. Elliptical segments use cubic spans <=45deg.
std::vector<Contour> shape(const QString& tag,Attributes& a,const Id& id,std::size_t& total) {
    auto length=[&](const QString& key,double fallback=0.0){auto i=a.find(key);if(i==a.end())return fallback;const auto v=scalar(i->second,true);a.erase(i);return v;};
    auto radius=[&](const QString& key)->std::optional<double>{auto i=a.find(key);if(i==a.end())return {};if(i->second.trimmed()=="auto"){a.erase(i);return {};}const auto v=length(key);need(v>=0,"SVG_RANGE","Negative SVG radius");return v;};
    Contour c;c.id=id+"-c0";std::size_t serial=0;
    auto add=[&](Vec2 v){need(++total<=10000,"SVG_LIMIT","SVG point limit10000");Point p;p.id=id+"-p"+std::to_string(serial++);p.x.literal=v.x;p.y.literal=v.y;c.points.push_back(p);};
    auto close=[&](bool curved=false){c.closed=true;if(c.points.size()>1){auto& first=c.points.front();const auto& last=c.points.back();if(curved||(first.x.literal==last.x.literal&&first.y.literal==last.y.literal)){first.in_angle=last.in_angle;first.in_length=last.in_length;c.points.pop_back();}}};
    auto arc=[&](Vec2 center,double rx,double ry,double start,double sweep) {
        const int spans=static_cast<int>(std::ceil(std::abs(sweep)/(std::numbers::pi/4)));const double step=sweep/spans,k=4.0/3*std::tan(step/4);
        for(int i=0;i<spans;++i){const auto t=start+i*step,u=t+step;
            const Vec2 c1{center.x+rx*(std::cos(t)-k*std::sin(t)),center.y+ry*(std::sin(t)+k*std::cos(t))};
            const Vec2 c2{center.x+rx*(std::cos(u)+k*std::sin(u)),center.y+ry*(std::sin(u)-k*std::cos(u))};
            handle(c.points.back(),true,c1);add({center.x+rx*std::cos(u),center.y+ry*std::sin(u)});handle(c.points.back(),false,c2);
        }
    };
    if(tag=="line") {const Vec2 p{length("x1"),length("y1")},q{length("x2"),length("y2")};add(p);add(q);}
    else if(tag=="polyline"||tag=="polygon") {
        need(a.contains("points"),"SVG_UNSUPPORTED","Empty point-list shapes are unsupported");Numbers n(a.at("points"));a.erase("points");
        bool first=true;while(!n.end()){const auto x=first?n.number():n.following();first=false;const auto y=n.following();add({x,y});}
        need(c.points.size()>=2,"SVG_UNSUPPORTED","Point-list shapes need at least two points");if(tag=="polygon")close();
    } else if(tag=="circle"||tag=="ellipse") {
        const Vec2 center{length("cx"),length("cy")};double rx,ry;
        if(tag=="circle"){rx=ry=length("r");need(rx>=0,"SVG_RANGE","Negative circle radius");}
        else {const auto x=radius("rx"),y=radius("ry");rx=x.value_or(y.value_or(0));ry=y.value_or(x.value_or(0));}
        need(rx>0&&ry>0,"SVG_UNSUPPORTED","Zero-size circles/ellipses are unsupported");
        add({center.x+rx,center.y});arc(center,rx,ry,0,2*std::numbers::pi);close(true);
    } else {
        const auto x=length("x"),y=length("y"),w=length("width"),h=length("height");need(w>=0&&h>=0,"SVG_RANGE","Negative rectangle size");need(w>0&&h>0,"SVG_UNSUPPORTED","Zero-size rectangles are unsupported");
        const auto xr=radius("rx"),yr=radius("ry");const auto rx=std::min(w/2,xr.value_or(yr.value_or(0))),ry=std::min(h/2,yr.value_or(xr.value_or(0)));
        if(rx==0||ry==0){add({x,y});add({x+w,y});add({x+w,y+h});add({x,y+h});close();}
        else {
            add({x+rx,y});add({x+w-rx,y});arc({x+w-rx,y+ry},rx,ry,-std::numbers::pi/2,std::numbers::pi/2);
            add({x+w,y+h-ry});arc({x+w-rx,y+h-ry},rx,ry,0,std::numbers::pi/2);
            add({x+rx,y+h});arc({x+rx,y+h-ry},rx,ry,std::numbers::pi/2,std::numbers::pi/2);
            add({x,y+ry});arc({x+rx,y+ry},rx,ry,std::numbers::pi,std::numbers::pi/2);close(true);
        }
    }
    return {std::move(c)};
}
Attributes attributes(const QXmlStreamReader& xml) {
    Attributes result;for(const auto& a:xml.attributes()) {
        need(a.namespaceUri().isEmpty(),"SVG_UNSUPPORTED","Namespaced attributes are unsupported");result.emplace(a.name().toString(),a.value().toString());
    }
    if(const auto style=result.find("style");style!=result.end()) {
        for(const auto& part:style->second.split(';',Qt::SkipEmptyParts)) {
            const auto colon=part.indexOf(':');need(colon>0&&part.indexOf(':',colon+1)<0,"SVG_UNSUPPORTED","Unsupported inline style");
            const auto key=part.left(colon).trimmed();need(std::set<QString>{"fill","stroke","fill-opacity","stroke-opacity","stroke-width","fill-rule","opacity","stroke-linecap","stroke-linejoin","stroke-miterlimit"}.contains(key),"SVG_UNSUPPORTED","Unsupported style property: "+key.toStdString());
            result[key]=part.mid(colon+1).trimmed();
        }
        result.erase("style");
    }
    return result;
}
Style style(Attributes& a,Style s,double& alpha,Affine& matrix) {
    auto take=[&](const QString& key)->std::optional<QString>{auto i=a.find(key);if(i==a.end())return {};auto v=i->second;a.erase(i);return v;};
    if(auto v=take("fill"))s.fill=*v;if(auto v=take("stroke"))s.stroke=*v;
    if(auto v=take("fill-rule")){need(*v=="nonzero"||*v=="evenodd","SVG_UNSUPPORTED","Unsupported fill-rule");s.rule=*v;}
    if(auto v=take("fill-opacity"))s.fill_alpha=opacity(*v);if(auto v=take("stroke-opacity"))s.stroke_alpha=opacity(*v);
    if(auto v=take("stroke-width")){s.width=scalar(*v,true);need(s.width>=0,"SVG_RANGE","Negative stroke-width");}
    if(auto v=take("opacity"))alpha=opacity(*v);if(auto v=take("transform"))matrix=transform(*v);
    if(auto v=take("stroke-linecap"))need(*v=="butt","SVG_UNSUPPORTED","Only butt stroke caps supported");
    if(auto v=take("stroke-linejoin"))need(*v=="miter","SVG_UNSUPPORTED","Only miter stroke joins supported");
    if(auto v=take("stroke-miterlimit"))need(scalar(*v)==4,"SVG_UNSUPPORTED","Only stroke miterlimit4 supported");
    return s;
}
class Reader {
    QXmlStreamReader xml;Id composition,prefix;std::string name;std::size_t nodes=0,parsed_points=0;double x,y;
    Id fresh(){need(++nodes<=128,"SVG_LIMIT","SVG element limit128");return prefix+"-n"+std::to_string(nodes);}
    void placement(const Id& id,const Affine& matrix,double alpha) {
        constexpr const char* fields[]{"transform.a","transform.b","transform.c","transform.d","transform.tx","transform.ty"};
        for(std::size_t i=0;i<6;++i)if(matrix[i]!=identity[i])plan.commands.push_back(Set{{id,"",fields[i]},matrix[i]});
        if(alpha!=1)plan.commands.push_back(Set{{id,"","composite.opacity"},alpha});
    }
    void paint(const Id& id,const Style& s) {
        plan.commands.push_back(RemoveOperation{id,id+"-stroke"});std::size_t index=0;
        for(const bool stroke:{false,true}) {
            const auto color=(stroke?s.stroke:s.fill).trimmed();if(color=="none"||(stroke&&s.width==0))continue;
            need(!color.contains('(')&&!color.contains('!')&&( !color.startsWith('#')||color.size()==4||color.size()==7),"SVG_UNSUPPORTED","Unsupported SVG color: "+color.toStdString());
            const QColor value(color);need(value.isValid()&&value.alpha()==255,"SVG_UNSUPPORTED","Only opaque named/HEX sRGB colors supported; use opacity attributes");
            auto op=default_operation(id+(stroke?"-paint-stroke":"-paint-fill"),stroke?"nect.paint.stroke":"nect.paint.fill");op.fill_rule=s.rule.toStdString();
            op.parameters["r"].literal=value.redF();op.parameters["g"].literal=value.greenF();op.parameters["b"].literal=value.blueF();op.parameters["a"].literal=stroke?s.stroke_alpha:s.fill_alpha;
            if(stroke)op.parameters["width"].literal=s.width;plan.commands.push_back(AddOperation{id,std::move(op),index++});
        }
    }
    void next() {xml.readNext();need(!xml.hasError(),"SVG_XML",xml.errorString().toStdString());need(xml.tokenType()!=QXmlStreamReader::DTD&&xml.tokenType()!=QXmlStreamReader::EntityReference&&xml.tokenType()!=QXmlStreamReader::ProcessingInstruction,"SVG_UNSUPPORTED","DTD, entities and processing instructions are unsupported");}
    Id element(Style inherited,unsigned depth,bool root=false) {
        need(depth<=32,"SVG_LIMIT","SVG nesting limit32");need(xml.namespaceUri().isEmpty()||xml.namespaceUri()==u"http://www.w3.org/2000/svg","SVG_UNSUPPORTED","Foreign XML content unsupported");
        const auto tag=xml.name().toString();need(root?tag=="svg":std::set<QString>{"g","path","rect","circle","ellipse","line","polyline","polygon"}.contains(tag),"SVG_UNSUPPORTED","Unsupported SVG element: "+tag.toStdString());
        auto a=attributes(xml);std::string label=root?name:tag.toStdString();if(a.contains("id")){label=a.at("id").toStdString();a.erase("id");}
        double alpha=1;Affine matrix=identity;const auto inherited_style=style(a,inherited,alpha,matrix);const auto id=root?prefix:fresh();
        if(root) {
            a.erase("version");std::optional<std::array<double,4>> box;
            if(a.contains("viewBox")){Numbers v(a.at("viewBox"));box=std::array<double,4>{v.number(),v.following(),v.following(),v.following()};need(v.end()&&(*box)[2]>0&&(*box)[3]>0,"SVG_RANGE","Positive viewBox required");a.erase("viewBox");}
            plan.width=a.contains("width")?scalar(a.at("width"),true):box?(*box)[2]:0;plan.height=a.contains("height")?scalar(a.at("height"),true):box?(*box)[3]:0;a.erase("width");a.erase("height");
            need(plan.width>0&&plan.height>0,"SVG_RANGE","SVG requires positive dimensions or viewBox");
            const auto aspect=a.contains("preserveAspectRatio")?a.at("preserveAspectRatio").trimmed():QString("xMidYMid meet");a.erase("preserveAspectRatio");
            need(aspect=="none"||aspect=="xMidYMid"||aspect=="xMidYMid meet","SVG_UNSUPPORTED","Only none or xMidYMid meet aspect ratio supported");
            Affine viewport=identity;
            if(box){double sx=plan.width/(*box)[2],sy=plan.height/(*box)[3],ox=0,oy=0;if(aspect!="none"){sx=sy=std::min(sx,sy);ox=(plan.width-(*box)[2]*sx)/2;oy=(plan.height-(*box)[3]*sy)/2;}viewport={sx,0,0,sy,ox-(*box)[0]*sx,oy-(*box)[1]*sy};}
            matrix=compose(Affine{1,0,0,1,x,y},compose(matrix,viewport));
        }
        std::vector<Id> children;
        const bool drawable=tag!="svg"&&tag!="g";
        if(drawable) {
            std::vector<Contour> contours;
            if(tag=="path"){need(a.contains("d"),"SVG_SYNTAX","Path requires d");contours=path(a.at("d"),id,parsed_points);a.erase("d");}
            else contours=shape(tag,a,id,parsed_points);
            for(const auto& c:contours)plan.points+=c.points.size();
            plan.commands.push_back(CreatePath{composition,"",id,label,std::move(contours)});paint(id,inherited_style);++plan.paths;
        }
        need(a.empty(),"SVG_UNSUPPORTED",a.empty()?"":"Unsupported SVG attribute: "+a.begin()->first.toStdString());
        while(true) {
            next();if(xml.isEndElement())break;need(!xml.atEnd(),"SVG_XML","Unexpected SVG end");
            if(xml.isStartElement()) {
                if(xml.name()==u"title"||xml.name()==u"desc") {
                    need((xml.namespaceUri().isEmpty()||xml.namespaceUri()==u"http://www.w3.org/2000/svg")&&xml.attributes().empty(),"SVG_UNSUPPORTED","Foreign metadata or metadata attributes unsupported");
                    while(true){next();if(xml.isEndElement())break;need(xml.isCharacters()||xml.isComment(),"SVG_UNSUPPORTED","Nested metadata unsupported");}
                } else {need(!drawable,"SVG_UNSUPPORTED","Shape child content unsupported");children.push_back(element(inherited_style,depth+1));}
            } else if(xml.isCharacters())need(xml.isWhitespace(),"SVG_UNSUPPORTED","Unexpected SVG text");
        }
        if(!drawable) {need(!children.empty(),"SVG_UNSUPPORTED","Empty SVG Groups unsupported");plan.commands.push_back(GroupContiguous{composition,"",children,id,label});}
        placement(id,matrix,alpha);need(plan.commands.size()<=1000,"SVG_LIMIT","SVG command limit1000");return id;
    }
public:
    SvgImportPlan plan;
    Reader(std::string_view bytes,Id comp,Id stem,std::string label,double px,double py):xml(QByteArray(bytes.data(),static_cast<qsizetype>(bytes.size()))),composition(std::move(comp)),prefix(std::move(stem)),name(std::move(label)),x(px),y(py) {xml.setEntityExpansionLimit(1024);}
    SvgImportPlan read(){while(!xml.atEnd()){next();if(xml.isStartElement()){need(plan.root.empty(),"SVG_XML","One SVG root required");plan.root=element({},0,true);}}need(!plan.root.empty()&&plan.paths>0,"SVG_SYNTAX","SVG has no path artwork");return std::move(plan);}
};
}
SvgImportPlan read_svg(std::string_view bytes,const Id& composition,const Id& prefix,const std::string& name,double x,double y) {
    need(!bytes.empty()&&bytes.size()<=1024*1024,"SVG_LIMIT","SVG source limit1 MiB");
    need(QRegularExpression("^[A-Za-z0-9_-]{1,40}$").match(QString::fromStdString(prefix)).hasMatch(),"INVALID_ID","SVG prefix requires1..40 identifier characters");
    need(std::isfinite(x)&&std::isfinite(y),"SVG_RANGE","Finite import position required");return Reader(bytes,composition,prefix,name,x,y).read();
}
}

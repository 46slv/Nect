#pragma once
#include <QApplication>
#include <QCoreApplication>
#include <QFontInfo>
#include <QFontMetricsF>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPainter>
#include <QScreen>
#include <QStyle>
#include <QtMath>
#include <QLabel>
#include <QLayout>
#include <QWidget>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>

namespace batch_angle_test {
// Preserve the normal offscreen contract. Native-platform comparison is an
// explicit diagnostic opt-in, never an implicit way to make a failing test pass.
inline void configure_test_qpa() {
    const auto incoming=qgetenv("QT_QPA_PLATFORM"),requested=qgetenv("NECT_TEST_QPA_PLATFORM");
    const auto selected=requested.isEmpty()?QByteArray("offscreen"):requested;
    if(qEnvironmentVariableIsSet("NECT_GEOMETRY_DIAGNOSTICS")||!requested.isEmpty()||
       (!incoming.isEmpty()&&incoming!=selected))
        std::cerr<<"BATCH_QPA_REQUEST incoming="<<std::quoted(incoming.toStdString())
            <<" test-override="<<std::quoted(requested.toStdString())
            <<" selected="<<std::quoted(selected.toStdString())<<'\n';
    qputenv("QT_QPA_PLATFORM",selected);
}
inline void report_test_qpa() {
    if(qEnvironmentVariableIsSet("NECT_GEOMETRY_DIAGNOSTICS")||
       !qEnvironmentVariableIsEmpty("NECT_TEST_QPA_PLATFORM"))
        std::cerr<<"BATCH_QPA_EFFECTIVE platform="<<QGuiApplication::platformName().toStdString()
            <<" qt="<<qVersion()<<" scale-factor="<<qEnvironmentVariable("QT_SCALE_FACTOR").toStdString()<<'\n';
}
inline std::string codepoints(const QString& text) {
    QStringList points;
    for(const auto point:text.toUcs4())points.append("U+"+QString::number(point,16).rightJustified(4,'0').toUpper());
    return points.join(' ').toStdString();
}
inline std::string rectangle(const QRectF& rect) {
    const auto number=[](qreal value){return QString::number(value,'g',17);};
    return QString("%1,%2 %3x%4 [left=%5 right=%6]").arg(number(rect.x()),number(rect.y()),
        number(rect.width()),number(rect.height()),number(rect.left()),number(rect.right())).toStdString();
}
inline void report_font(const char* kind,const QFont& font,const QWidget* device) {
    const QFontInfo info(font);const QFontMetricsF metrics(font,device);
    std::cerr<<"CAPTION_FONT kind="<<kind<<" toString="<<std::quoted(font.toString().toStdString())
        <<" resolveMask="<<font.resolveMask()<<" families="<<std::quoted(font.families().join('|').toStdString())
        <<" resolved-family="<<std::quoted(info.family().toStdString())
        <<" resolved-style="<<std::quoted(info.styleName().toStdString())
        <<" resolved-point="<<info.pointSizeF()<<" resolved-pixel="<<info.pixelSize()
        <<" exact="<<info.exactMatch()<<" weight="<<info.weight()<<" italic="<<info.italic()
        <<" style-hint="<<font.styleHint()<<" strategy="<<font.styleStrategy()
        <<" hinting="<<font.hintingPreference()<<" height="<<metrics.height()
        <<" ascent="<<metrics.ascent()<<" descent="<<metrics.descent()<<" leading="<<metrics.leading()
        <<" min-left-bearing="<<metrics.minLeftBearing()<<" min-right-bearing="<<metrics.minRightBearing()<<'\n';
}
inline qreal report_text_metrics(const char* kind,const QFontMetricsF& metrics,const QString& text) {
    const auto bounds=metrics.boundingRect(text),ink=metrics.tightBoundingRect(text);
    const auto advance=metrics.horizontalAdvance(text);
    const auto logical_span=std::max(advance,bounds.right())-std::min(qreal(0),bounds.left());
    const auto span=std::max(advance,ink.right())-std::min(qreal(0),ink.left());
    const auto precision=std::cerr.precision();std::cerr<<std::setprecision(17);
    std::cerr<<"CAPTION_RESERVE kind="<<kind<<" text="<<std::quoted(QString(text).replace('\n',"\\n").toStdString())
        <<" codepoints="<<std::quoted(codepoints(text))<<" advance="<<advance
        <<" bounds="<<rectangle(bounds)<<" tight="<<rectangle(ink)
        <<" logical-span="<<logical_span<<" span="<<span<<" ceil="<<std::ceil(span)<<'\n';
    std::cerr.precision(precision);
    return span;
}
inline QStringList caption_reserve_samples(const QLabel* note,int target_count) {
    QStringList candidates{QStringLiteral("Common \u00b7 %1").arg(target_count),
        QStringLiteral("Mixed \u00b7 %1").arg(target_count),note->text().split('\n').value(1)};
    for(const double magnitude:{0.0,29.74488,999.9999,9999999.0,0.0001234567,0.0009999999,
            1.234567e-5,1.999999e9,2e9,std::numeric_limits<double>::denorm_min(),
            std::numeric_limits<double>::min(),std::numeric_limits<double>::max()})
        for(const double sign:{-1.0,1.0}) {
            const auto delta=sign*magnitude;auto text=QString::number(delta,'g',7);
            if(delta>=0)text.prepend('+');candidates.append(text+QStringLiteral("\u00b0"));
        }
    return candidates;
}
inline void report_caption_reserve(const QLabel* note) {
    if(!note||!qEnvironmentVariableIsSet("NECT_GEOMETRY_DIAGNOSTICS"))return;
    const QWidget* dial=nullptr;
    for(const auto* child:note->parentWidget()->findChildren<QWidget*>(QString{},Qt::FindDirectChildrenOnly))
        if(child->property("nect-targets").isValid()){dial=child;break;}
    const auto targets=dial?QJsonDocument::fromJson(dial->property("nect-targets").toByteArray()).array():QJsonArray{};
    const auto source_font=note->font();auto resolved_font=source_font;
    resolved_font.setResolveMask(QFont::AllPropertiesResolved);
    const QFontMetricsF source_metrics(source_font,note),resolved_metrics(resolved_font,note);
    std::cerr<<"CAPTION_WIDGET stage=initial-settled dial="<<(dial?dial->objectName().toStdString():"unknown")
        <<" targets="<<targets.size()<<" platform="<<QGuiApplication::platformName().toStdString()
        <<" style="<<note->style()->metaObject()->className()<<":"<<note->style()->objectName().toStdString()
        <<" app-style="<<QApplication::style()->metaObject()->className()<<":"<<QApplication::style()->objectName().toStdString()
        <<" logical-dpi="<<note->logicalDpiX()<<","<<note->logicalDpiY()
        <<" physical-dpi="<<note->physicalDpiX()<<","<<note->physicalDpiY()<<" dpr="<<note->devicePixelRatioF()
        <<" minimum="<<note->minimumWidth()<<" allocation="<<note->width()<<"x"<<note->height()
        <<" text="<<std::quoted(QString(note->text()).replace('\n',"\\n").toStdString())
        <<" codepoints="<<std::quoted(codepoints(note->text()))<<'\n';
    if(const auto* screen=note->screen())
        std::cerr<<"CAPTION_SCREEN name="<<std::quoted(screen->name().toStdString())
            <<" logical-dpi="<<screen->logicalDotsPerInchX()<<","<<screen->logicalDotsPerInchY()
            <<" physical-dpi="<<screen->physicalDotsPerInchX()<<","<<screen->physicalDotsPerInchY()
            <<" dpr="<<screen->devicePixelRatio()<<'\n';
    report_font("source",source_font,note);report_font("all-resolved",resolved_font,note);
    report_font("application",QApplication::font(),note);
    // Settled-widget replay, not a claim to observe construction-time font state.
    const auto candidates=caption_reserve_samples(note,targets.size());
    qreal replay_minimum=98;QString max_text;
    for(const auto& text:candidates) {
        report_text_metrics("source",source_metrics,text);
        const auto span=report_text_metrics("all-resolved",resolved_metrics,text);
        if(std::ceil(span)>replay_minimum){replay_minimum=std::ceil(span);max_text=text;}
    }
    std::cerr<<"CAPTION_RESERVE_SUMMARY samples="<<candidates.size()<<" actual-minimum="<<note->minimumWidth()
        <<" replay-minimum="<<replay_minimum<<" max-text="<<std::quoted(max_text.toStdString())
        <<" matches="<<(replay_minimum==note->minimumWidth())<<'\n';
    QString glyphs=note->text()+candidates.join(' ');QString seen;
    for(const auto glyph:glyphs) {
        if(glyph.isSpace()||seen.contains(glyph))continue;seen+=glyph;
        std::cerr<<"CAPTION_GLYPH codepoint="<<codepoints(QString(glyph))
            <<" present="<<resolved_metrics.inFont(glyph)<<" advance="<<resolved_metrics.horizontalAdvance(glyph)
            <<" left-bearing="<<resolved_metrics.leftBearing(glyph)<<" right-bearing="<<resolved_metrics.rightBearing(glyph)
            <<" bounds="<<rectangle(resolved_metrics.boundingRect(QString(glyph)))<<'\n';
    }
}
inline QRect global_rect(const QWidget* widget) {
    return {widget->mapToGlobal(QPoint{}),widget->size()};
}
// Capture the outer layout contract as well as painted allocations. A roomy host
// may hide a changing minimum hint that reflows the same form at narrower metrics.
struct Geometry {
    QRect dial,row,caption,dial_global,row_global;
    QSize row_hint,row_minimum,caption_minimum;
    explicit Geometry(const QWidget* knob) {
        const auto* parent=knob->parentWidget();const auto* note=parent->findChild<QLabel*>();
        dial=knob->geometry();row=parent->geometry();caption=note?note->geometry():QRect{};
        dial_global=global_rect(knob);row_global=global_rect(parent);
        row_hint=parent->sizeHint();row_minimum=parent->minimumSizeHint();
        caption_minimum=note?note->minimumSize():QSize{};
    }
    bool stable(const QWidget* knob) const {
        const Geometry now(knob);
        const bool same=now.dial.size()==QSize(44,44)&&now.row.height()==80&&
            dial==now.dial&&row==now.row&&caption==now.caption&&
            dial_global==now.dial_global&&row_global==now.row_global&&
            row_hint==now.row_hint&&row_minimum==now.row_minimum&&caption_minimum==now.caption_minimum;
        if(!same||qEnvironmentVariableIsSet("NECT_GEOMETRY_DIAGNOSTICS")) {
            const auto rectangle=[](const QRect& r) {
                return QString("%1,%2 %3x%4").arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height()).toStdString();
            };
            const auto size=[](const QSize& s) {return QString("%1x%2").arg(s.width()).arg(s.height()).toStdString();};
            const auto* note=knob->parentWidget()->findChild<QLabel*>();
            std::cerr<<"BATCH_GEOMETRY "<<knob->objectName().toStdString()<<" stable="<<same
                <<" dial="<<rectangle(dial)<<" -> "<<rectangle(now.dial)
                <<" row="<<rectangle(row)<<" -> "<<rectangle(now.row)
                <<" caption="<<rectangle(caption)<<" -> "<<rectangle(now.caption)
                <<" global-dial="<<rectangle(dial_global)<<" -> "<<rectangle(now.dial_global)
                <<" global-row="<<rectangle(row_global)<<" -> "<<rectangle(now.row_global)
                <<" row-hint="<<size(row_hint)<<" -> "<<size(now.row_hint)
                <<" row-minimum="<<size(row_minimum)<<" -> "<<size(now.row_minimum);
            if(note)std::cerr<<" label-hint="<<size(note->sizeHint())<<" label-minimum="<<size(note->minimumSizeHint())
                <<" hfw="<<note->heightForWidth(note->width())<<" policy="<<note->sizePolicy().horizontalPolicy()
                <<" font="<<QFontInfo(note->font()).family().toStdString()<<" pixel-size="<<QFontInfo(note->font()).pixelSize()
                <<" text="<<QString(note->text()).replace('\n'," / ").toStdString();
            std::cerr<<'\n';
        }
        return same;
    }
};
inline bool caption_has_reserved_width(const QLabel* note) {
    report_caption_reserve(note);
    if(!note)return false;
    const QWidget* dial=nullptr;
    for(const auto* child:note->parentWidget()->findChildren<QWidget*>(QString{},Qt::FindDirectChildrenOnly))
        if(child->property("nect-targets").isValid()){dial=child;break;}
    if(!dial)return false;
    const auto targets=QJsonDocument::fromJson(dial->property("nect-targets").toByteArray()).array();
    auto font=note->font();font.setResolveMask(QFont::AllPropertiesResolved);
    const QFontMetricsF metrics(font,note);
    qreal expected=98;
    for(const auto& sample:caption_reserve_samples(note,targets.size())) {
        const auto ink=metrics.tightBoundingRect(sample);
        const auto advance=metrics.horizontalAdvance(sample);
        const auto span=std::max(advance,ink.right())-std::min(qreal(0),ink.left());
        if(!std::isfinite(advance)||advance<0||!std::isfinite(ink.left())||
           !std::isfinite(ink.right())||ink.width()<0||!std::isfinite(span)||span>QWIDGETSIZE_MAX)return false;
        expected=std::max(expected,std::ceil(span));
    }
    // These fixtures keep font and DPI unchanged between row construction and
    // initial capture. This deliberately rejects a giant logical-origin reserve;
    // it does not promise live re-reservation after a font/DPI change.
    const bool matches=note->minimumWidth()==expected;
    if(!matches||qEnvironmentVariableIsSet("NECT_GEOMETRY_DIAGNOSTICS"))
        std::cerr<<"CAPTION_RESERVATION_CHECK actual="<<note->minimumWidth()<<" expected="<<expected
            <<" matches="<<matches<<'\n';
    return matches&&note->width()>=note->minimumWidth();
}
// Test the actual plain batch-caption paint contract without any QWidget size
// hint/minimum clamp. TextDontClip also prevents drawText from stopping layout
// after the last line that fits in the requested rectangle.
struct CaptionInk {
    QRect bounds;
    int pixels=0,outside=0;
    bool supported=false,truncated=false;
    bool fits() const {return supported&&pixels>0&&outside==0&&!truncated;}
    bool overflows() const {return supported&&pixels>0&&outside>0&&!truncated;}
};
inline CaptionInk caption_ink(const QLabel* source,const QString& text,int width,int height) {
    CaptionInk result;
    if(!source||width<=0||height<=0||source->frameWidth()!=0||source->margin()!=0||
       source->contentsMargins()!=QMargins{}||source->indent()>0||
       source->alignment()!=(Qt::AlignVCenter|Qt::AlignLeft)||
       source->textFormat()==Qt::RichText||source->textFormat()==Qt::MarkdownText||text.contains('<')||
       text.isRightToLeft()||source->buddy()!=nullptr||
       (source->textInteractionFlags()&(Qt::TextSelectableByMouse|Qt::TextSelectableByKeyboard)))return result;
    result.supported=true;
    auto font=source->font();font.setResolveMask(QFont::AllPropertiesResolved);
    const QFontMetricsF metrics(font,source);
    const QRectF allocation(0,0,width,height);
    const int flags=Qt::AlignLeft|Qt::AlignVCenter|Qt::TextForceLeftToRight|
        (source->wordWrap()?Qt::TextWordWrap:0)|Qt::TextDontClip;
    const auto layout=metrics.boundingRect(allocation,flags,text);
    // The canvas contains the entire unclipped layout and glyph overhang, not
    // merely the allocation being tested. Ink on its edge invalidates the proof.
    const int padding=qCeil(metrics.height()+qAbs(metrics.minLeftBearing())+qAbs(metrics.minRightBearing()))+2;
    const auto canvas=allocation.united(layout).adjusted(-padding,-padding,padding,padding).toAlignedRect();
    const qreal dpr=source->devicePixelRatioF();
    QImage image(qCeil(canvas.width()*dpr),qCeil(canvas.height()*dpr),QImage::Format_ARGB32_Premultiplied);
    if(image.isNull())return result;
    image.setDevicePixelRatio(dpr);
    image.setDotsPerMeterX(qRound(source->logicalDpiX()/0.0254));
    image.setDotsPerMeterY(qRound(source->logicalDpiY()/0.0254));
    image.fill(Qt::transparent);
    const QPointF origin=-QPointF(canvas.topLeft());
    QPainter painter(&image);painter.setFont(font);painter.setPen(Qt::white);
    painter.drawText(allocation.translated(origin),flags,text);painter.end();
    const auto target=allocation.translated(origin);
    const auto allowed=QRectF(target.x()*dpr,target.y()*dpr,target.width()*dpr,target.height()*dpr).toAlignedRect();
    for(int y=0;y<image.height();++y) {
        const auto* scan=reinterpret_cast<const QRgb*>(image.constScanLine(y));
        for(int x=0;x<image.width();++x)if(qAlpha(scan[x])) {
            ++result.pixels;result.bounds|=QRect(x,y,1,1);
            if(!allowed.contains(x,y))++result.outside;
            if(x==0||y==0||x==image.width()-1||y==image.height()-1)result.truncated=true;
        }
    }
    result.bounds.translate(-allowed.topLeft());
    return result;
}
inline QSize caption_hint_size(const QLabel* source,const QString& text,int width) {
    QLabel probe;
    auto font=source->font();font.setResolveMask(QFont::AllPropertiesResolved);
    probe.setFont(font);probe.setWordWrap(source->wordWrap());probe.setTextFormat(source->textFormat());
    probe.setContentsMargins(source->contentsMargins());probe.setMargin(source->margin());
    probe.setIndent(source->indent());probe.setAlignment(source->alignment());probe.setText(text);
    // Diagnostic only: a top-level probe can use a different platform font/DPI
    // context. Neither its hints nor the live label's fixed-height clamp prove fit.
    return {probe.minimumSizeHint().width(),probe.heightForWidth(width)};
}
inline bool caption_fits(const QLabel* note,const QString& text,int width,int height) {
    if(!note)return false;
    const auto required=caption_hint_size(note,text,width);
    const auto ink=caption_ink(note,text,width,height);
    const bool fits=ink.fits();
    if(!fits||qEnvironmentVariableIsSet("NECT_GEOMETRY_DIAGNOSTICS"))
        std::cerr<<"CAPTION_FIT fits="<<fits<<" probe-hint="<<required.width()<<"x"<<required.height()
            <<" allocated="<<width<<"x"<<height<<" ink="<<ink.bounds.x()<<","<<ink.bounds.y()<<" "<<ink.bounds.width()<<"x"<<ink.bounds.height()
            <<" outside="<<ink.outside<<" truncated="<<ink.truncated<<" text="<<QString(text).replace('\n'," / ").toStdString()<<'\n';
    return fits;
}
inline bool caption_fits(const QLabel* note) {
    return note&&caption_fits(note,note->text(),note->width(),note->height());
}
inline bool wider_inspector_caption_expands(QWidget* dial) {
    auto* scroll=dial->window()->findChild<QWidget*>("inspector-scroll");
    auto* dock=dial->window()->findChild<QWidget*>("properties");
    auto* note=dial->parentWidget()->findChild<QLabel*>();
    if(!scroll||!dock||!note)return false;
    const auto scroll_min=scroll->minimumWidth(),scroll_max=scroll->maximumWidth();
    const auto dock_min=dock->minimumWidth(),dock_max=dock->maximumWidth();
    const auto width=note->width(),minimum=note->minimumWidth();
    scroll->setFixedWidth(680);dock->setFixedWidth(700);QCoreApplication::processEvents();
    const bool expands=note->width()>width&&note->minimumWidth()==minimum&&
        dial->size()==QSize(44,44)&&dial->parentWidget()->height()==80&&caption_fits(note);
    scroll->setMinimumWidth(scroll_min);scroll->setMaximumWidth(scroll_max);
    dock->setMinimumWidth(dock_min);dock->setMaximumWidth(dock_max);QCoreApplication::processEvents();
    return expands;
}
inline bool narrow_caption_samples_fit(const QLabel* note) {
    if(!note)return false;
    // Prove this oracle catches horizontal overflow even when height is abundant.
    if(!caption_ink(note,QString(200,'W'),98,1000).overflows())return false;
    if(!caption_ink(note,QStringLiteral("Common \u00b7 3\n+X zero\n\u0394 +90\u00b0"),300,1).overflows())return false;
    // Measured Windows caption allocation; use this host's actual font metrics.
    for(const auto& text:QStringList{QStringLiteral("Common \u00b7 3\n+X zero\n\u0394 +999.9999\u00b0"),
            QStringLiteral("Common \u00b7 12\n+X zero\n\u0394 +999.9999\u00b0"),
            QStringLiteral("Common \u00b7 32\n+X zero\n\u0394 +999.9999\u00b0"),
            QStringLiteral("Mixed \u00b7 32\ntop zero\n\u0394 -999.9999\u00b0")})
        if(!caption_fits(note,text,98,80))return false;
    for(const auto& delta:QStringList{"+0.0001234567","-0.0009999999","+1.234567e-05","-1.999999e+09",
            "+4.940656e-324","-2.225074e-308","+1.797693e+308"})
        if(!caption_fits(note,QStringLiteral("Common \u00b7 32\ntop zero\n\u0394 ")+delta+QStringLiteral("\u00b0"),
                note->minimumWidth(),80))return false;
    return true;
}
}

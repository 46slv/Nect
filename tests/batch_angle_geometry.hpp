#pragma once
#include <QFontInfo>
#include <QLabel>
#include <QLayout>
#include <QWidget>
#include <iostream>

namespace batch_angle_test {
inline QRect global_rect(const QWidget* widget) {
    return {widget->mapToGlobal(QPoint{}),widget->size()};
}
// Capture the outer layout contract as well as painted allocations. A roomy host
// may hide a changing minimum hint that reflows the same form at narrower metrics.
struct Geometry {
    QRect dial,row,caption,dial_global,row_global;
    QSize row_hint,row_minimum;
    explicit Geometry(const QWidget* knob) {
        const auto* parent=knob->parentWidget();const auto* note=parent->findChild<QLabel*>();
        dial=knob->geometry();row=parent->geometry();caption=note?note->geometry():QRect{};
        dial_global=global_rect(knob);row_global=global_rect(parent);
        row_hint=parent->sizeHint();row_minimum=parent->minimumSizeHint();
    }
    bool stable(const QWidget* knob) const {
        const Geometry now(knob);
        const bool same=now.dial.size()==QSize(44,44)&&now.row.height()==80&&
            dial==now.dial&&row==now.row&&caption==now.caption&&
            dial_global==now.dial_global&&row_global==now.row_global&&
            row_hint==now.row_hint&&row_minimum==now.row_minimum;
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
inline QSize caption_required_size(const QLabel* source,const QString& text,int width) {
    QLabel probe;
    probe.setFont(source->font());probe.setWordWrap(source->wordWrap());probe.setTextFormat(source->textFormat());
    probe.setContentsMargins(source->contentsMargins());probe.setMargin(source->margin());
    probe.setIndent(source->indent());probe.setAlignment(source->alignment());probe.setText(text);
    // The unconstrained minimum width includes the largest unbreakable word.
    // Height alone would miss horizontal clipping now that live hints are ignored.
    return {probe.minimumSizeHint().width(),probe.heightForWidth(width)};
}
inline bool caption_fits(const QLabel* note,const QString& text,int width,int height) {
    if(!note)return false;
    const auto required=caption_required_size(note,text,width);
    const bool fits=required.width()<=width&&required.height()<=height;
    if(!fits||qEnvironmentVariableIsSet("NECT_GEOMETRY_DIAGNOSTICS"))
        std::cerr<<"CAPTION_FIT fits="<<fits<<" required="<<required.width()<<"x"<<required.height()
            <<" allocated="<<width<<"x"<<height<<" text="<<QString(text).replace('\n'," / ").toStdString()<<'\n';
    return fits;
}
inline bool caption_fits(const QLabel* note) {
    return note&&caption_fits(note,note->text(),note->width(),note->height());
}
inline bool narrow_caption_samples_fit(const QLabel* note) {
    if(!note)return false;
    // Prove this oracle catches horizontal overflow even when height is abundant.
    const auto too_wide=caption_required_size(note,QString(200,'W'),98);
    if(too_wide.width()<=98)return false;
    // Measured Windows caption allocation; use this host's actual font metrics.
    for(const auto& text:QStringList{"Common · 3\n+X zero\nΔ +999.9999°",
            "Common · 12\n+X zero\nΔ +999.9999°", "Common · 32\n+X zero\nΔ +999.9999°",
            "Mixed · 32\ntop zero\nΔ -999.9999°"})
        if(!caption_fits(note,text,98,80))return false;
    return true;
}
}

#include "flattened_svg_export.hpp"
#include "host.hpp"
#include "canvas.hpp"
#include "storage.hpp"
#include <QBuffer>
#include <QColorSpace>
#include <QFileInfo>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QPointer>
#include <QImageWriter>
#include <QJsonArray>
#include <QSaveFile>
#include <QXmlStreamWriter>
#include <algorithm>

namespace nect::desktop {
void show_flattened_svg_export_dialog(Host& host,const Id& composition,const Id& artboard,QWidget* parent) {
    const QPointer<Host> safe_host(&host);
    const auto identity=host.session_id;const auto revision=host.session.revision();
    if(host.session.gesture_active())throw Error("GESTURE_ACTIVE","Finish or cancel the current gesture before exporting");
    bool accepted=false;
    const auto scale=QInputDialog::getDouble(parent,"Export flattened SVG",
        "All artwork becomes one image. Pixels per document unit:",1,.001,16,3,&accepted);
    if(!accepted)return;
    if(!safe_host||safe_host->session_id!=identity)throw Error("SESSION_CONFLICT","Document changed while export settings were open");
    const auto path=QFileDialog::getSaveFileName(parent,"Export flattened SVG",{},"SVG (*.svg)",nullptr,QFileDialog::DontConfirmOverwrite);
    if(path.isEmpty())return;
    if(!safe_host||safe_host->session_id!=identity)throw Error("SESSION_CONFLICT","Document changed while export settings were open");
    const auto output=QFileInfo(path).suffix().isEmpty()?path+".svg":path;
    if(QFileInfo::exists(output)&&QMessageBox::question(parent,"Replace SVG?","Replace the existing output file?",
        QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
    if(!safe_host||safe_host->session_id!=identity)throw Error("SESSION_CONFLICT","Document changed while export settings were open");
    (void)export_flattened_svg(*safe_host,output,composition,artboard,scale,revision);
}
QJsonObject export_flattened_svg(Host& host,const QString& path,const Id& composition,
    const Id& artboard,double scale,std::uint64_t expected) {
    if(expected!=host.session.revision())throw Error("REVISION_CONFLICT","Refresh revision before exporting");
    if(host.session.gesture_active())throw Error("GESTURE_ACTIVE","Finish or cancel the current gesture before exporting");
    if(path.isEmpty()||!QFileInfo(path).isAbsolute()||QFileInfo(path).suffix().compare("svg",Qt::CaseInsensitive)!=0)
        throw Error("EXPORT_TARGET","Flattened SVG requires an absolute .svg destination");
    if(same_native_path(path,host.file_path))throw Error("EXPORT_TARGET","Export cannot replace the native source file");
    const auto& document=host.session.document();
    for(const auto& [id,asset]:document.raster_assets)
        if(asset.mode=="linked"&&same_native_path(path,QString::fromStdString(asset.locator)))
            throw Error("EXPORT_TARGET","Export cannot replace a linked source image");
    const auto comp=std::find_if(document.compositions.begin(),document.compositions.end(),[&](const auto& c){return c.id==composition;});
    if(comp==document.compositions.end())throw Error("MISSING_COMPOSITION",composition);
    const auto board=evaluate_artboard(*comp,artboard);
    auto image=Canvas::render_artboard(document,composition,artboard,scale,false);
    image.setColorSpace(QColorSpace{});
    QByteArray png;QBuffer buffer(&png);buffer.open(QIODevice::WriteOnly);
    QImageWriter encoder(&buffer,"png");
    if(!encoder.write(image))throw Error("EXPORT_ENCODE",encoder.errorString().toStdString());
    if(png.size()<33||png.mid(12,4)!="IHDR")throw Error("EXPORT_ENCODE","PNG encoder did not produce a standard header");
    // Pixels are already sRGB; use the same standard sRGB chunk as PNG export.
    png.insert(33,QByteArray::fromHex("000000017352474200aece1ce9"));
    QByteArray svg;QBuffer xml_buffer(&svg);xml_buffer.open(QIODevice::WriteOnly);
    QXmlStreamWriter xml(&xml_buffer);xml.writeStartDocument();
    xml.writeStartElement("svg");xml.writeDefaultNamespace("http://www.w3.org/2000/svg");
    xml.writeNamespace("http://www.w3.org/1999/xlink","xlink");
    const auto number=[](double value){return QString::number(value,'g',17);};
    xml.writeAttribute("version","1.1");xml.writeAttribute("width",number(board.width));
    xml.writeAttribute("height",number(board.height));
    xml.writeAttribute("viewBox","0 0 "+number(board.width)+" "+number(board.height));
    xml.writeAttribute("overflow","hidden");
    xml.writeStartElement("desc");xml.writeCharacters("Appearance-only raster derivative. Editable artwork remains in the Nect document.");xml.writeEndElement();
    xml.writeStartElement("image");xml.writeAttribute("x","0");xml.writeAttribute("y","0");
    // Keep the raster's exact sample spacing; clip any ceil-rounded fringe at
    // the outer SVG viewport rather than squeezing it into the nominal frame.
    xml.writeAttribute("width",number(image.width()/scale));xml.writeAttribute("height",number(image.height()/scale));
    xml.writeAttribute("preserveAspectRatio","none");
    xml.writeAttribute("http://www.w3.org/1999/xlink","href","data:image/png;base64,"+QString::fromLatin1(png.toBase64()));
    xml.writeEndElement();xml.writeEndElement();xml.writeEndDocument();
    if(xml.hasError())throw Error("EXPORT_ENCODE","Could not encode flattened SVG XML");
    QSaveFile output(path);output.setDirectWriteFallback(false);
    if(!output.open(QIODevice::WriteOnly))throw Error("IO_ERROR",output.errorString().toStdString());
    if(output.write(svg)!=svg.size()||!output.commit())throw Error("IO_ERROR",output.errorString().toStdString());
    return {{"path",path},{"kind","flattened_svg"},{"editable",false},
        {"pixel_width",image.width()},{"pixel_height",image.height()},{"scale",scale},
        {"width",board.width},{"height",board.height},{"color_space","sRGB"},
        {"background","authored_on_transparent"},{"revision",static_cast<qint64>(expected)},
        {"losses",QJsonArray{"All artwork is rasterized into one embedded PNG; no editable paths, text or layers are retained in this derivative"}}};
}
}

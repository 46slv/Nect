#include "host.hpp"
#include "nect/analysis_adoption.hpp"
#include "svg_import.hpp"
#include "canvas.hpp"
#include <QSaveFile>
#include <QFileInfo>
#include <QImageWriter>
#include <QBuffer>
#include <QColorSpace>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QLocalSocket>
#include <QUuid>
#include <QDateTime>
#include <QCryptographicHash>
#include <limits>
#include <memory>
#include <algorithm>
#include <array>
#include <map>
#include <tuple>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <numbers>
#include <set>

namespace nect::desktop {
Id new_id() { return QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString(); }
namespace {
QString string(const QJsonObject& o,const char* key) {
    if(!o.value(key).isString()) throw Error("INVALID_REQUEST",std::string("String required: ")+key);
    return o.value(key).toString();
}
}

Host::Host(QString recovery_directory,QObject* parent,ProtectionWriter writer)
    :QObject(parent),session(empty_document(new_id(),new_id(),new_id())),
     session_id(QString::fromStdString(new_id())),recovery_directory_(std::move(recovery_directory)),writer_(std::move(writer)) {
    backup_clock_.start();
    recovery_timer_.setSingleShot(true);
    recovery_timer_.setInterval(1000);
    connect(&recovery_timer_,&QTimer::timeout,this,[this] {
        pending_due_=true;start_protection();
    });
    completion_timer_.setInterval(25);
    connect(&completion_timer_,&QTimer::timeout,this,[this]{collect_protection();});
    server_.setSocketOptions(QLocalServer::UserAccessOption);
    connect(&server_,&QLocalServer::newConnection,this,[this] {
        while(auto* socket=server_.nextPendingConnection()) {
            auto buffer=std::make_shared<QByteArray>();
            connect(socket,&QLocalSocket::readyRead,this,[this,socket,buffer] {
                buffer->append(socket->readAll());
                if(buffer->size()>64*1024*1024) { socket->abort(); return; }
                for(;;) {
                    const auto newline=buffer->indexOf('\n');
                    if(newline<0) break;
                    const auto line=buffer->left(newline);
                    buffer->remove(0,newline+1);
                    socket->write(dispatch(line)+'\n');
                }
            });
            connect(socket,&QLocalSocket::disconnected,socket,&QObject::deleteLater);
        }
    });
}
Host::~Host() {
    changed={};status_changed={};recovery_timer_.stop();completion_timer_.stop();
    pending_.reset();pending_due_=false;drain_protection();
}
void Host::listen(const QString& endpoint) {
    if(!server_.listen(endpoint)) throw Error("IPC_ERROR",server_.errorString().toStdString());
}
void Host::edited() {
    queue_protection();
    if(changed) changed();
}
ProtectionSnapshot Host::snapshot() const {
    ProtectionSnapshot result;result.document=session.document();result.session_id=session_id;
    result.revision=session.revision();result.native_file=file_path;result.recovery_directory=recovery_directory_;
    return result;
}
void Host::queue_protection() {
    // Session::document is the last committed state even while a gesture has a
    // preview. Only one newest pending copy is retained beside the running job.
    pending_=snapshot();
    if(!pending_due_&&!recovery_timer_.isActive())recovery_timer_.start();
    if(pending_due_)start_protection();
    refresh_status();
}
void Host::start_protection() {
    if(running_.valid()||!pending_||!pending_due_)return;
    auto job=std::move(*pending_);pending_.reset();pending_due_=false;
    job.expected_native=file_stamp_;job.recovery_lease=recovery_lease_;
    job.write_native=!file_path.isEmpty()&&saved_revision_!=job.revision&&native_error_.code!="FILE_CHANGED";
    const auto now=backup_clock_.elapsed();
    job.native_backup=now-last_native_backup_>=30000;job.recovery_backup=now-last_recovery_backup_>=30000;
    running_revision_=job.revision;
    try {
        running_=std::async(std::launch::async,[writer=writer_,job=std::move(job)]() mutable {return writer(std::move(job));});
        completion_timer_.start();
    } catch(const std::exception& e) {
        running_revision_.reset();recovery_error_={"IO_ERROR",QString::fromUtf8(e.what())};
    }
    refresh_status();
}
void Host::accept(ProtectionResult result) {
    // Session replacement drains the worker. This guard also prevents accidental
    // publication if a future caller ever violates that ordering.
    if(result.session_id!=session_id)return;
    recovery_lease_=std::move(result.recovery_lease);
    if(result.recovery_written) {
        protected_revision_=result.revision;protected_file_=result.native_file;recovery_error_={};
        if(result.recovery_backup)last_recovery_backup_=backup_clock_.elapsed();
    } else if(!result.recovery_error.empty())recovery_error_=std::move(result.recovery_error);
    if(result.native_file==file_path) {
        if(result.native_written) {
            saved_revision_=result.revision;file_stamp_=std::move(result.native_stamp);native_error_={};
            if(result.native_backup)last_native_backup_=backup_clock_.elapsed();
        } else if(!result.native_error.empty())native_error_=std::move(result.native_error);
    }
}
void Host::drain_protection() {
    completion_timer_.stop();
    if(running_.valid())try {accept(running_.get());}
    catch(const std::exception& e) {recovery_error_={"IO_ERROR",QString::fromUtf8(e.what())};}
    running_revision_.reset();
}
void Host::collect_protection() {
    if(!running_.valid()||running_.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return;
    drain_protection();
    if(pending_due_)start_protection();
    refresh_status();
}
QJsonObject Host::persistence() const {
    auto revision=[](const auto& value)->QJsonValue{return value?QJsonValue(static_cast<qint64>(*value)):QJsonValue(QJsonValue::Null);};
    auto error=[](const StorageError& value)->QJsonValue{return value.empty()?QJsonValue(QJsonValue::Null):
        QJsonValue(QJsonObject{{"code",value.code},{"message",value.message}});};
    const auto phase=!native_error_.empty()||!recovery_error_.empty()?"failed":running_revision_?"writing":pending_?"pending":
        !dirty()?"saved":protected_revision_==session.revision()?"protected":"unprotected";
    return {{"phase",phase},{"live_revision",static_cast<qint64>(session.revision())},
        {"saved_revision",revision(saved_revision_)},{"recovery_revision",revision(protected_revision_)},
        {"writing_revision",revision(running_revision_)},{"pending_revision",pending_?QJsonValue(static_cast<qint64>(pending_->revision)):QJsonValue(QJsonValue::Null)},
        {"native_error",error(native_error_)},{"recovery_error",error(recovery_error_)},
        {"recovery_file",QDir(recovery_directory_).filePath(session_id+".nect")},
        {"cadence_ms",1000},{"backup_interval_ms",30000},{"backup_generations",10},
        {"retained_inactive_recovery_sessions",20},{"inactive_recovery_budget_bytes",128*1024*1024}};
}
void Host::refresh_status() {
    const auto recovery=protected_revision_?"Recovery r"+QString::number(*protected_revision_):QString("Recovery pending");
    if(!native_error_.empty())save_status=(native_error_.code=="FILE_CHANGED"?
        QString("Source changed externally · Save As or reopen"):"Save failed: "+native_error_.message)+" · "+recovery;
    else if(!recovery_error_.empty())save_status="Recovery failed: "+recovery_error_.message+
        (!dirty()?" · Saved r"+QString::number(*saved_revision_):QString());
    else if(running_revision_||pending_)save_status=(running_revision_?"Writing r"+QString::number(*running_revision_):QString("Save pending"))+
        (pending_?" · latest r"+QString::number(pending_->revision):QString())+" · "+recovery;
    else if(!dirty())save_status="Saved · revision "+QString::number(*saved_revision_);
    else save_status=recovery+" · choose Save As for a native file";
    if(status_changed)status_changed();
}
void Host::reset(Document document,const QString& path,FileStamp stamp) {
    if(session.gesture_active()) throw Error("GESTURE_ACTIVE","Finish the current gesture first");
    if(std::none_of(document.compositions.begin(),document.compositions.end(),[](const auto& c){return !c.artboards.empty();}))
        throw Error("DESKTOP_DOCUMENT_REQUIREMENT","The desktop needs a Composition with an Artboard; source file is unchanged");
    // Protect the outgoing document before replacing the live Session.
    flush();
    session=Session(std::move(document));link_observations_.clear();
    session_id=QString::fromStdString(new_id());
    file_path=path;file_stamp_=std::move(stamp);
    saved_revision_=path.isEmpty()?std::nullopt:std::optional<std::uint64_t>(0);
    protected_revision_.reset();protected_file_.clear();recovery_lease_.reset();native_error_={};recovery_error_={};
    last_native_backup_=-30000;last_recovery_backup_=-30000;
    queue_protection();
    if(changed) changed();
}
void Host::create_document() { reset(empty_document(new_id(),new_id(),new_id()),{}); }
void Host::open(const QString& path) {
    const auto absolute=native_path(path);
    if(same_native_path(absolute,file_path))flush();
    auto loaded=load_native(absolute);reset(std::move(loaded.document),absolute,std::move(loaded.stamp));
}
void Host::open_recovery(const QString& path) {
    auto loaded=load_native(native_path(path));reset(std::move(loaded.document),{});
}
void Host::save(const QString& path) {
    if(session.gesture_active()) throw Error("GESTURE_ACTIVE","Finish the gesture before saving");
    if(path.isEmpty()) throw Error("IO_ERROR","Choose a native file path");
    const auto absolute=native_path(path);
    drain_protection();
    const auto bytes=QByteArray::fromStdString(encode(session.document()));
    FileStamp stamp;
    const auto current_target=same_native_path(absolute,file_path);
    try {stamp=store_native(absolute,bytes,current_target?std::optional<FileStamp>(file_stamp_):std::nullopt,true);}
    catch(const Error& error) {
        if(current_target)native_error_={QString::fromStdString(error.code),QString::fromUtf8(error.what())};
        save_status="Save failed: "+QString::fromUtf8(error.what());if(status_changed)status_changed();throw;
    }
    file_path=absolute;file_stamp_=std::move(stamp);saved_revision_=session.revision();native_error_={};
    // Preserve this explicit save on the next background replacement, even if
    // it occurs inside the normal thirty-second generation cadence.
    last_native_backup_=-30000;queue_protection();
    if(changed) changed();
}
void Host::recover() {
    // Explicit flush/open/new/close may wait; periodic jobs never wait on the UI
    // thread. Flush uses committed authority even when invoked during a preview.
    recovery_timer_.stop();pending_.reset();pending_due_=false;drain_protection();
    if(protected_revision_!=session.revision()||protected_file_!=file_path||(!file_path.isEmpty()&&dirty()&&native_error_.code!="FILE_CHANGED")) {
        auto job=snapshot();job.expected_native=file_stamp_;job.recovery_lease=recovery_lease_;
        job.write_native=!file_path.isEmpty()&&dirty()&&native_error_.code!="FILE_CHANGED";
        const auto now=backup_clock_.elapsed();
        job.native_backup=now-last_native_backup_>=30000;job.recovery_backup=now-last_recovery_backup_>=30000;
        accept(writer_(std::move(job)));
    }
    refresh_status();
    if(protected_revision_!=session.revision())throw Error(recovery_error_.empty()?"IO_ERROR":recovery_error_.code.toStdString(),
        recovery_error_.empty()?"Committed revision is not protected":recovery_error_.message.toStdString());
}
void Host::flush() {
    try {recover();}
    catch(const Error&) {
        // A broken recovery folder must not trap a document whose exact latest
        // native bytes are still readable. Explicit recover remains strict.
        if(!dirty())try {if(load_native(file_path).stamp==file_stamp_)return;}catch(const std::exception&) {}
        throw;
    }
}

namespace {
QString image_path(const QString& path) {
    // Reject URLs, UNC/device paths and relative paths before touching filesystem.
    if(path.size()<3||!path[0].isLetter()||path[1]!=':'||(path[2]!='/'&&path[2]!='\\')||path.contains(QChar(0)))
        throw Error("INVALID_ASSET_LOCATOR","Image files require an absolute local drive path");
    return QDir::cleanPath(QDir::fromNativeSeparators(path));
}
std::vector<unsigned char> image_bytes(const QString& path) {
    const QFileInfo before(path);if(!before.exists())throw Error("ASSET_MISSING","Linked file is missing; accepted pixels remain available");
    if(!before.isFile())throw Error("ASSET_UNREADABLE","Image source must be a regular file");
    if(before.size()>static_cast<qint64>(raster_source_limit))throw Error("ASSET_LIMIT","Image source byte limit 8 MiB");
    QFile file(path);if(!file.open(QIODevice::ReadOnly))throw Error("ASSET_UNREADABLE",file.errorString().toStdString());
    const auto bytes=file.read(static_cast<qint64>(raster_source_limit)+1);
    if(file.error()!=QFileDevice::NoError)throw Error("ASSET_UNREADABLE",file.errorString().toStdString());
    const QFileInfo after(path);
    if(bytes.size()>static_cast<qint64>(raster_source_limit))throw Error("ASSET_LIMIT","Image source byte limit 8 MiB");
    if(bytes.size()!=before.size()||after.size()!=before.size()||after.lastModified()!=before.lastModified())throw Error("ASSET_CHANGED_DURING_READ","Image file changed during reading; no update was accepted");
    return {reinterpret_cast<const unsigned char*>(bytes.constData()),reinterpret_cast<const unsigned char*>(bytes.constData())+bytes.size()};
}
void asset_revision(const Session& session,std::uint64_t expected) {
    if(session.revision()!=expected)throw Error("REVISION_CONFLICT","Refresh revision before asset operations");
    if(session.gesture_active())throw Error("GESTURE_ACTIVE","Finish the gesture before asset operations");
}
}
QJsonObject Host::import_svg(const QString& path,const Id& composition,const Id& prefix,const std::string& name,double x,double y,std::uint64_t expected) {
    asset_revision(session,expected);
    if(path.size()<3||!path[0].isLetter()||path[1]!=':'||(path[2]!='/'&&path[2]!='\\')||path.contains(QChar(0)))
        throw Error("INVALID_SVG_PATH","SVG requires an absolute local drive path");
    const auto absolute=QDir::cleanPath(QDir::fromNativeSeparators(path));const QFileInfo before(absolute);
    if(!before.isFile())throw Error("SVG_IO","SVG source must be an existing regular file");
    constexpr qint64 limit=1024*1024;if(before.size()>limit)throw Error("SVG_LIMIT","SVG source limit1 MiB");
    QFile input(absolute);if(!input.open(QIODevice::ReadOnly))throw Error("SVG_IO",input.errorString().toStdString());
    const auto bytes=input.read(limit+1);const QFileInfo after(absolute);
    if(input.error()!=QFileDevice::NoError)throw Error("SVG_IO",input.errorString().toStdString());
    if(bytes.size()>limit)throw Error("SVG_LIMIT","SVG source limit1 MiB");
    if(bytes.size()!=before.size()||after.size()!=before.size()||after.lastModified()!=before.lastModified())throw Error("SVG_CHANGED_DURING_READ","SVG changed while reading; nothing imported");
    const auto plan=read_svg(std::string_view(bytes.constData(),static_cast<std::size_t>(bytes.size())),composition,prefix,name,x,y);
    apply_serializable(session,plan.commands,expected);edited();
    return {{"root",QString::fromStdString(plan.root)},{"paths",static_cast<qint64>(plan.paths)},{"points",static_cast<qint64>(plan.points)},
        {"width",plan.width},{"height",plan.height},{"conversion","Editable cubic paths and Groups; Shapes/arcs become editable paths; elliptical portions use cubic approximation. SVG viewport maps coordinates, not an authored crop/Artboard. Original file unchanged."}};
}
void Host::import_image(const QString& path,const std::string& mode,const Id& composition,const Id& parent,
    const Id& asset,const Id& object,const std::string& name,double x,double y,std::uint64_t expected) {
    asset_revision(session,expected);
    if(mode!="linked"&&mode!="embedded")throw Error("INVALID_ASSET_MODE","Choose linked or embedded explicitly");
    const auto absolute=image_path(path);auto payload=make_raster(image_bytes(absolute));
    RasterAsset source{asset,name,mode,mode=="linked"?absolute.toStdString():std::string{},payload};
    apply_serializable(session,{AddRasterAsset{source},CreateImage{composition,parent,object,name,{asset,{double(payload->width())},{double(payload->height())}}},
        Set{{object,"","transform.tx"},x},Set{{object,"","transform.ty"},y}},expected);
    if(mode=="linked")link_observations_[asset]={payload->sha256(),source.locator,QJsonObject{{"state","current"},{"checked_at",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}}};
    edited();
}
void Host::update_asset(const Id& id,const std::string& action,const QString& path,std::uint64_t expected) {
    asset_revision(session,expected);
    const auto found=session.document().raster_assets.find(id);if(found==session.document().raster_assets.end())throw Error("MISSING_ASSET",id);
    auto replacement=found->second;
    if(action=="embed") {replacement.mode="embedded";replacement.locator.clear();}
    else if(action=="reload"||action=="relink") {
        if(action=="reload"&&replacement.mode!="linked")throw Error("INVALID_ASSET_MODE","Only linked assets can reload; Relink can attach an embedded asset");
        const auto absolute=image_path(action=="reload"?QString::fromStdString(replacement.locator):path);
        replacement.payload=make_raster(image_bytes(absolute));replacement.mode="linked";replacement.locator=absolute.toStdString();
    } else throw Error("UNSUPPORTED_ASSET_ACTION",action);
    apply_serializable(session,{ReplaceRasterAsset{replacement}},expected);
    if(replacement.mode=="linked")link_observations_[id]={replacement.payload->sha256(),replacement.locator,QJsonObject{{"state","current"},{"checked_at",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}}};
    else link_observations_.erase(id);
    edited();
}
QJsonObject Host::asset_status(const Id& id) const {
    const auto found=session.document().raster_assets.find(id);if(found==session.document().raster_assets.end())throw Error("MISSING_ASSET",id);
    const auto& asset=found->second;
    QJsonObject out{{"asset",QString::fromStdString(id)},{"state",asset.mode=="embedded"?"embedded":"unchecked"},{"checked_at",QJsonValue::Null},
        {"accepted_sha256",QString::fromStdString(asset.payload->sha256())},{"display","accepted_cached_pixels"}};
    if(asset.mode=="linked")if(const auto observation=link_observations_.find(id);observation!=link_observations_.end()&&
        observation->second.hash==asset.payload->sha256()&&observation->second.locator==asset.locator) {
        for(auto it=observation->second.value.begin();it!=observation->second.value.end();++it)out[it.key()]=it.value();
    }
    return out;
}
QJsonObject Host::check_asset(const Id& id) {
    const auto found=session.document().raster_assets.find(id);if(found==session.document().raster_assets.end())throw Error("MISSING_ASSET",id);
    const auto& asset=found->second;if(asset.mode=="embedded")return asset_status(id);
    QJsonObject observation{{"checked_at",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}};
    try {
        const auto bytes=image_bytes(image_path(QString::fromStdString(asset.locator)));
        const auto digest=QCryptographicHash::hash(QByteArray::fromRawData(reinterpret_cast<const char*>(bytes.data()),static_cast<qsizetype>(bytes.size())),QCryptographicHash::Sha256).toHex();
        observation["state"]=digest.toStdString()==asset.payload->sha256()?"current":"changed";
        observation["observed_sha256"]=QString::fromLatin1(digest);
    } catch(const Error& e) {
        observation["state"]=e.code=="ASSET_MISSING"?"missing":e.code=="ASSET_LIMIT"||e.code=="ASSET_CHANGED_DURING_READ"?"changed":"unreadable";
        observation["message"]=QString::fromUtf8(e.what());
    }
    link_observations_[id]={asset.payload->sha256(),asset.locator,std::move(observation)};return asset_status(id);
}

QJsonObject Host::export_png(const QString& path,const Id& composition,const Id& artboard,double scale,bool white_background,std::uint64_t expected) {
    if(expected!=session.revision())throw Error("REVISION_CONFLICT","Refresh revision before exporting");
    if(session.gesture_active())throw Error("GESTURE_ACTIVE","Finish or cancel the current gesture before exporting");
    if(path.isEmpty()||!QFileInfo(path).isAbsolute()||QFileInfo(path).suffix().compare("png",Qt::CaseInsensitive)!=0)
        throw Error("EXPORT_TARGET","PNG export requires an absolute .png destination");
    if(same_native_path(path,file_path))throw Error("EXPORT_TARGET","Export cannot replace the native source file");
    for(const auto& [id,asset]:session.document().raster_assets)
        if(asset.mode=="linked"&&same_native_path(path,QString::fromStdString(asset.locator)))
            throw Error("EXPORT_TARGET","Export cannot replace a linked source image");
    const auto image=Canvas::render_artboard(session.document(),composition,artboard,scale,white_background);
    QSaveFile output(path);output.setDirectWriteFallback(false);
    if(!output.open(QIODevice::WriteOnly))throw Error("IO_ERROR",output.errorString().toStdString());
    // Qt's synthesized ICC profile is not accepted by all Windows WIC color
    // transforms. Pixels already use sRGB; declare the standard PNG sRGB chunk
    // instead of introducing a profile conversion for these same values.
    auto pixels=image;pixels.setColorSpace(QColorSpace{});
    QByteArray encoded;QBuffer buffer(&encoded);buffer.open(QIODevice::WriteOnly);
    QImageWriter writer(&buffer,"png");
    if(!writer.write(pixels))throw Error("EXPORT_ENCODE",writer.errorString().toStdString());
    if(encoded.size()<33||encoded.mid(12,4)!="IHDR")throw Error("EXPORT_ENCODE","PNG encoder did not produce a standard header");
    // length=1, type=sRGB, perceptual intent=0, CRC32(type+intent)=AECE1CE9.
    encoded.insert(33,QByteArray::fromHex("000000017352474200aece1ce9"));
    if(output.write(encoded)!=encoded.size())throw Error("IO_ERROR",output.errorString().toStdString());
    if(!output.commit())throw Error("IO_ERROR",output.errorString().toStdString());
    const auto comp=std::find_if(session.document().compositions.begin(),session.document().compositions.end(),[&](const auto& c){return c.id==composition;});
    const auto authored_background=evaluate_artboard(*comp,artboard).background;
    QJsonValue underlay=QJsonValue::Null;
    if(authored_background){QJsonArray rgba;for(double channel:authored_background->rgba)rgba.append(channel);
        underlay=QJsonObject{{"space","srgb"},{"profile","srgb"},{"alpha","straight"},{"rgba",rgba}};
    }
    return {{"path",path},{"width",image.width()},{"height",image.height()},{"scale",scale},{"authored_background",underlay},
        {"background",white_background?"white":"transparent"},{"color_space","sRGB"},{"revision",static_cast<qint64>(expected)}};
}

namespace {
QString analysis_child_id(const QString& analysis_id,const char* kind,const QJsonObject& payload) {
    const auto canonical=QJsonDocument(QJsonObject{{"analysis_id",analysis_id},{"payload",payload}})
        .toJson(QJsonDocument::Compact);
    const auto digest=QCryptographicHash::hash(canonical,QCryptographicHash::Sha256).toHex();
    return QStringLiteral("analysis.%1.v1:%2").arg(QString::fromLatin1(kind),QString::fromLatin1(digest));
}
QString analysis_pixel_digest(const QImage& image) {
    QCryptographicHash hash(QCryptographicHash::Sha256);
    QByteArray row(image.width()*4,Qt::Uninitialized);
    for(int y=0;y<image.height();++y) {
        for(int x=0;x<image.width();++x) {
            const auto pixel=image.pixel(x,y);
            row[4*x]=static_cast<char>(qAlpha(pixel));
            row[4*x+1]=static_cast<char>(qRed(pixel));
            row[4*x+2]=static_cast<char>(qGreen(pixel));
            row[4*x+3]=static_cast<char>(qBlue(pixel));
        }
        hash.addData(row);
    }
    return QString::fromLatin1(hash.result().toHex());
}
struct AnalysisOperatorDescriptor {
    const char* type_id;
    int version;
    const char* input_domain;
    const char* coordinate_space;
};
constexpr std::array<AnalysisOperatorDescriptor,3> analysis_operators{{
    {"nect.analysis.image.regions",1,"artboard.rgba8_srgb_premultiplied","artboard-output-pixels"},
    {"nect.analysis.vector.geometry",1,"document.path_geometry","object-local-du-and-composition-du"},
    {"nect.analysis.document.structure",1,"document.composition_structure","stable-document-identities"}
}};
QString analysis_snapshot_id(const QJsonObject& snapshot) {
    const auto canonical=QJsonDocument(snapshot).toJson(QJsonDocument::Compact);
    const auto digest=QCryptographicHash::hash(canonical,QCryptographicHash::Sha256).toHex();
    return QStringLiteral("analysis.v1:%1").arg(QString::fromLatin1(digest));
}
QJsonObject analysis_point_json(Vec2 point) {
    if(!std::isfinite(point.x)||!std::isfinite(point.y))
        throw Error("ANALYSIS_NON_FINITE","Vector analysis produced a non-finite coordinate");
    return {{"x",point.x},{"y",point.y}};
}
QJsonObject analysis_bounds_json(const Bounds& bounds) {
    for(const auto value:{bounds.left,bounds.top,bounds.right,bounds.bottom})
        if(!std::isfinite(value))throw Error("ANALYSIS_NON_FINITE","Vector bounds contain a non-finite value");
    const auto width=bounds.right-bounds.left,height=bounds.bottom-bounds.top;
    if(!std::isfinite(width)||!std::isfinite(height))
        throw Error("ANALYSIS_NON_FINITE","Vector bounds exceed the finite coordinate range");
    return {{"x",bounds.left},{"y",bounds.top},{"width",width},{"height",height}};
}
void analysis_parameter_keys(const QJsonObject& parameters,const QStringList& allowed) {
    for(auto it=parameters.begin();it!=parameters.end();++it)
        if(!allowed.contains(it.key()))throw Error("INVALID_ANALYSIS_PARAMETER",it.key().toStdString());
}
QString analysis_parameter_string(const QJsonObject& parameters,const char* key) {
    const auto value=parameters.value(QLatin1String(key));
    if(!value.isString()||value.toString().isEmpty())
        throw Error("INVALID_ANALYSIS_PARAMETER",std::string("Nonempty string required: ")+key);
    return value.toString();
}
double analysis_parameter_number(const QJsonObject& parameters,const char* key) {
    const auto value=parameters.value(QLatin1String(key));
    if(!value.isDouble()||!std::isfinite(value.toDouble()))
        throw Error("INVALID_ANALYSIS_PARAMETER",std::string("Finite number required: ")+key);
    return value.toDouble();
}
int analysis_parameter_integer(const QJsonObject& parameters,const char* key) {
    const auto value=analysis_parameter_number(parameters,key);
    if(std::floor(value)!=value||value<std::numeric_limits<int>::min()||value>std::numeric_limits<int>::max())
        throw Error("INVALID_ANALYSIS_PARAMETER",std::string("Integer required: ")+key);
    return static_cast<int>(value);
}
bool analysis_parameter_boolean(const QJsonObject& parameters,const char* key,bool fallback=false) {
    if(!parameters.contains(QLatin1String(key)))return fallback;
    const auto value=parameters.value(QLatin1String(key));
    if(!value.isBool())throw Error("INVALID_ANALYSIS_PARAMETER",std::string("Boolean required: ")+key);
    return value.toBool();
}
std::map<Id,Id> analysis_structural_parents(const Document& document,const Composition& composition) {
    std::map<Id,Id> parents;std::set<Id> active;
    std::function<void(const Id&,const Id&,unsigned)> visit=[&](const Id& id,const Id& parent,unsigned depth) {
        if(depth>128)throw Error("ANALYSIS_LIMIT","Structure depth exceeds 128 objects");
        const auto found=document.objects.find(id);
        if(found==document.objects.end())throw Error("MISSING_OBJECT",id);
        if(active.contains(id)||parents.contains(id))throw Error("INVALID_HIERARCHY","Repeated structural object: "+id);
        active.insert(id);parents.emplace(id,parent);
        for(const auto& child:found->second.children)visit(child,id,depth+1);
        active.erase(id);
    };
    for(const auto& root:composition.roots)visit(root,{},0);
    return parents;
}
}

QJsonObject analyze_region_pixels(const QImage& image,int threshold,double scale,std::uint64_t source_revision,
    bool include_color_groups,bool include_color_components,
    std::optional<std::uint64_t> intersect_color_component_index,
    std::optional<QString> intersect_color_component_id,
    const Id& document_id,const Id& composition_id,const Id& artboard_id) {
    if(include_color_components&&!include_color_groups)
        throw Error("INVALID_REQUEST","include_color_components requires include_color_groups to be true");
    if(intersect_color_component_index&&intersect_color_component_id)
        throw Error("INVALID_REQUEST","Supply only one color component selector");
    if((intersect_color_component_index||intersect_color_component_id)&&(!include_color_groups||!include_color_components))
        throw Error("INVALID_REQUEST","Color component selection requires include_color_groups and include_color_components to be true");
    if(threshold<1||threshold>255)throw Error("INVALID_THRESHOLD","Alpha threshold must be an integer from 1 through 255");
    if(!std::isfinite(scale)||scale<=0||scale>16)throw Error("EXPORT_SCALE","Analysis scale must be greater than zero and at most 16");
    if(image.isNull()||image.width()<1||image.height()<1)throw Error("RENDER_ALLOCATION","Could not read rendered analysis pixels");
    if(image.format()!=QImage::Format_ARGB32_Premultiplied)
        throw Error("INVALID_REQUEST","Analysis pixels must be ARGB32_Premultiplied Canvas output");
    constexpr std::uint64_t max_pixels=4'000'000;
    constexpr std::size_t max_regions=10'000;
    constexpr std::size_t max_edge_runs=100'000;
    constexpr std::size_t max_line_candidates=10'000;
    constexpr std::size_t max_morphology_runs=20'000;
    constexpr std::uint64_t max_boundary_edges=200'000;
    const auto width=static_cast<std::uint64_t>(image.width());
    const auto height=static_cast<std::uint64_t>(image.height());
    const auto pixel_count=width*height;
    if(pixel_count>max_pixels)throw Error("ANALYSIS_LIMIT","Region analysis is limited to 4,000,000 output pixels");

    constexpr int analysis_behavior_version=1;
    const QJsonObject snapshot{{"document_id",QString::fromStdString(document_id)},
        {"composition",QString::fromStdString(composition_id)},{"artboard",QString::fromStdString(artboard_id)},
        {"source_revision",static_cast<qint64>(source_revision)},{"scale",scale},{"threshold",threshold},
        {"width",image.width()},{"height",image.height()},
        {"pixel_format","ARGB32_Premultiplied"},{"color_space","sRGB"},
        {"alpha_domain","8-bit premultiplied Canvas output alpha byte"},
        {"coordinate_space","artboard-output-pixels"},{"origin","top-left"},
        {"analysis_behavior_version",analysis_behavior_version},
        {"pixel_sha256",analysis_pixel_digest(image)}};
    const auto analysis_id=QStringLiteral("analysis.v1:%1").arg(QString::fromLatin1(
        QCryptographicHash::hash(QJsonDocument(snapshot).toJson(QJsonDocument::Compact),
            QCryptographicHash::Sha256).toHex()));
    const auto child_id=[&](const char* kind,const QJsonObject& payload) {
        return analysis_child_id(analysis_id,kind,payload);
    };

    constexpr auto no_region=std::numeric_limits<std::uint32_t>::max();
    std::vector<std::uint32_t> pixel_regions(static_cast<std::size_t>(pixel_count),no_region);
    std::vector<std::uint32_t> queue;
    QJsonArray regions;
    const auto alpha_at=[&](std::uint32_t index) {
        const auto y=static_cast<int>(index/width),x=static_cast<int>(index%width);
        return qAlpha(image.pixel(x,y));
    };
    for(std::uint32_t seed=0;seed<static_cast<std::uint32_t>(pixel_count);++seed) {
        if(pixel_regions[seed]!=no_region||alpha_at(seed)<threshold)continue;
        if(static_cast<std::size_t>(regions.size())>=max_regions)
            throw Error("ANALYSIS_LIMIT","Region analysis is limited to 10,000 components");
        const auto region_index=static_cast<std::uint32_t>(regions.size());
        queue.clear();queue.push_back(seed);pixel_regions[seed]=region_index;
        std::size_t cursor=0;
        std::uint64_t area=0;
        auto min_x=static_cast<int>(seed%width),max_x=min_x;
        auto min_y=static_cast<int>(seed/width),max_y=min_y;
        const auto enqueue=[&](std::uint32_t index) {
            if(pixel_regions[index]==no_region&&alpha_at(index)>=threshold) {
                pixel_regions[index]=region_index;queue.push_back(index);
            }
        };
        while(cursor<queue.size()) {
            const auto index=queue[cursor++];
            const auto x=static_cast<int>(index%width),y=static_cast<int>(index/width);
            ++area;min_x=std::min(min_x,x);max_x=std::max(max_x,x);min_y=std::min(min_y,y);max_y=std::max(max_y,y);
            if(x>0)enqueue(index-1);
            if(static_cast<std::uint64_t>(x)+1<width)enqueue(index+1);
            if(y>0)enqueue(index-static_cast<std::uint32_t>(width));
            if(static_cast<std::uint64_t>(y)+1<height)enqueue(index+static_cast<std::uint32_t>(width));
        }
        QJsonObject region{{"area",static_cast<qint64>(area)},
            {"x",min_x},{"y",min_y},{"width",max_x-min_x+1},{"height",max_y-min_y+1}};
        region.insert("id",child_id("region",region));
        regions.append(region);
    }

    QJsonArray edge_runs;
    std::uint64_t edge_pixel_count=0;
    const auto foreground_at=[&](int x,int y) {return qAlpha(image.pixel(x,y))>=threshold;};
    const auto edge_at=[&](int x,int y) {
        return foreground_at(x,y)&&(x==0||!foreground_at(x-1,y)||x+1==image.width()||!foreground_at(x+1,y)||
            y==0||!foreground_at(x,y-1)||y+1==image.height()||!foreground_at(x,y+1));
    };
    for(int y=0;y<image.height();++y) {
        int x=0;
        while(x<image.width()) {
            while(x<image.width()&&!edge_at(x,y))++x;
            if(x==image.width())break;
            const auto start=x;
            do {++edge_pixel_count;++x;} while(x<image.width()&&edge_at(x,y));
            if(static_cast<std::size_t>(edge_runs.size())>=max_edge_runs)
                throw Error("ANALYSIS_LIMIT","Region analysis is limited to 100,000 edge runs");
            edge_runs.append(QJsonObject{{"y",y},{"x",start},{"width",x-start}});
        }
    }

    std::uint64_t boundary_edge_count=0;
    for(std::uint32_t index=0;index<static_cast<std::uint32_t>(pixel_count);++index) {
        const auto region=pixel_regions[index];
        if(region==no_region)continue;
        const auto x=static_cast<int>(index%width),y=static_cast<int>(index/width);
        if(x==0||pixel_regions[index-1]!=region)++boundary_edge_count;
        if(static_cast<std::uint64_t>(x)+1==width||pixel_regions[index+1]!=region)++boundary_edge_count;
        if(y==0||pixel_regions[index-static_cast<std::uint32_t>(width)]!=region)++boundary_edge_count;
        if(static_cast<std::uint64_t>(y)+1==height||pixel_regions[index+static_cast<std::uint32_t>(width)]!=region)++boundary_edge_count;
    }
    if(boundary_edge_count>max_boundary_edges)
        throw Error("ANALYSIS_LIMIT","Region analysis is limited to 200,000 directed boundary edges");

    struct DirectedEdge {std::uint32_t region;int x;int y;int direction;bool used=false;};
    struct Vertex {int x;int y;};
    struct Contour {std::uint32_t region;std::vector<Vertex> vertices;};
    std::vector<DirectedEdge> boundary_edges;
    boundary_edges.reserve(static_cast<std::size_t>(boundary_edge_count));
    std::map<std::tuple<std::uint32_t,int,int,int>,std::size_t> outgoing_edges;
    const auto add_edge=[&](std::uint32_t region,int x,int y,int direction) {
        const auto edge_index=boundary_edges.size();
        boundary_edges.push_back({region,x,y,direction,false});
        const auto [unused,inserted]=outgoing_edges.emplace(std::tuple{region,x,y,direction},edge_index);
        (void)unused;
        if(!inserted)throw Error("ANALYSIS_CONTOUR","Duplicate directed region boundary edge");
    };
    for(std::uint32_t index=0;index<static_cast<std::uint32_t>(pixel_count);++index) {
        const auto region=pixel_regions[index];
        if(region==no_region)continue;
        const auto x=static_cast<int>(index%width),y=static_cast<int>(index/width);
        // E, S, W, N with foreground on the right in top-left image coordinates.
        if(y==0||pixel_regions[index-static_cast<std::uint32_t>(width)]!=region)add_edge(region,x,y,0);
        if(static_cast<std::uint64_t>(x)+1==width||pixel_regions[index+1]!=region)add_edge(region,x+1,y,1);
        if(static_cast<std::uint64_t>(y)+1==height||pixel_regions[index+static_cast<std::uint32_t>(width)]!=region)
            add_edge(region,x+1,y+1,2);
        if(x==0||pixel_regions[index-1]!=region)add_edge(region,x,y+1,3);
    }

    const auto endpoint=[](const DirectedEdge& edge) {
        switch(edge.direction) {
        case 0:return Vertex{edge.x+1,edge.y};
        case 1:return Vertex{edge.x,edge.y+1};
        case 2:return Vertex{edge.x-1,edge.y};
        default:return Vertex{edge.x,edge.y-1};
        }
    };
    std::vector<Contour> contours;
    for(std::size_t start=0;start<boundary_edges.size();++start) {
        if(boundary_edges[start].used)continue;
        std::vector<Vertex> cycle;
        auto current=start;
        while(true) {
            auto& edge=boundary_edges[current];
            if(edge.used) {
                if(current==start)break;
                throw Error("ANALYSIS_CONTOUR","Region boundary trace entered an earlier cycle");
            }
            edge.used=true;
            cycle.push_back({edge.x,edge.y});
            const auto end=endpoint(edge);
            const std::array<int,4> turn_order{
                (edge.direction+1)%4,edge.direction,(edge.direction+3)%4,(edge.direction+2)%4};
            auto next=std::numeric_limits<std::size_t>::max();
            for(const auto direction:turn_order) {
                const auto found=outgoing_edges.find(std::tuple{edge.region,end.x,end.y,direction});
                if(found!=outgoing_edges.end()) {next=found->second;break;}
            }
            if(next==std::numeric_limits<std::size_t>::max())
                throw Error("ANALYSIS_CONTOUR","Region boundary trace could not close");
            current=next;
        }

        std::int64_t signed_area_twice=0;
        for(std::size_t i=0;i<cycle.size();++i) {
            const auto& a=cycle[i];const auto& b=cycle[(i+1)%cycle.size()];
            signed_area_twice+=static_cast<std::int64_t>(a.x)*b.y-static_cast<std::int64_t>(b.x)*a.y;
        }
        if(signed_area_twice<0)continue; // Counterclockwise inner/hole boundary.
        if(signed_area_twice==0)throw Error("ANALYSIS_CONTOUR","Region boundary has zero signed area");

        std::vector<Vertex> simplified;
        simplified.reserve(cycle.size());
        for(std::size_t i=0;i<cycle.size();++i) {
            const auto& previous=cycle[(i+cycle.size()-1)%cycle.size()];
            const auto& current_vertex=cycle[i];
            const auto& next_vertex=cycle[(i+1)%cycle.size()];
            if((previous.x==current_vertex.x&&current_vertex.x==next_vertex.x)||
               (previous.y==current_vertex.y&&current_vertex.y==next_vertex.y))continue;
            simplified.push_back(current_vertex);
        }
        if(simplified.size()<4)throw Error("ANALYSIS_CONTOUR","Region outer contour has fewer than four corners");
        const auto first=std::min_element(simplified.begin(),simplified.end(),[](const auto& a,const auto& b) {
            return std::tie(a.y,a.x)<std::tie(b.y,b.x);
        });
        std::rotate(simplified.begin(),first,simplified.end());
        contours.push_back({boundary_edges[start].region,std::move(simplified)});
    }
    std::sort(contours.begin(),contours.end(),[](const auto& a,const auto& b) {
        if(a.region!=b.region)return a.region<b.region;
        const auto& a_start=a.vertices.front();const auto& b_start=b.vertices.front();
        if(a_start.y!=b_start.y)return a_start.y<b_start.y;
        if(a_start.x!=b_start.x)return a_start.x<b_start.x;
        return std::lexicographical_compare(a.vertices.begin(),a.vertices.end(),b.vertices.begin(),b.vertices.end(),
            [](const auto& left,const auto& right) {return std::tie(left.x,left.y)<std::tie(right.x,right.y);});
    });
    QJsonArray outer_contours;
    for(const auto& contour:contours) {
        QJsonArray vertices;
        for(const auto& vertex:contour.vertices) {
            QJsonArray point;point.append(vertex.x);point.append(vertex.y);vertices.append(point);
        }
        QJsonObject output{{"region_index",static_cast<int>(contour.region)},
            {"region_id",regions.at(static_cast<int>(contour.region)).toObject().value("id")},
            {"closed",true},{"vertices",vertices}};
        auto identity_payload=output;
        identity_payload.remove("region_index");
        output.insert("id",child_id("contour",identity_payload));
        outer_contours.append(output);
    }

    // Derive exact one-pixel runs only after the existing region, edge-run,
    // and contour limits have been checked, preserving their error precedence.
    std::vector<std::uint8_t> foreground(static_cast<std::size_t>(pixel_count));
    for(std::uint32_t index=0;index<static_cast<std::uint32_t>(pixel_count);++index)
        foreground[index]=alpha_at(index)>=threshold?1:0;
    const auto foreground_at_pixel=[&](int x,int y) {
        return x>=0&&y>=0&&x<image.width()&&y<image.height()&&
            foreground[static_cast<std::size_t>(y)*static_cast<std::size_t>(image.width())+static_cast<std::size_t>(x)]!=0;
    };
    struct LineDirection {int dx;int dy;const char* name;};
    constexpr std::array<LineDirection,4> line_directions{{
        {1,0,"horizontal"},{0,1,"vertical"},{1,1,"down_diagonal"},{1,-1,"up_diagonal"}}};
    const auto line_eligible=[&](int x,int y,std::size_t direction) {
        if(!foreground_at_pixel(x,y))return false;
        if(direction==0)return !foreground_at_pixel(x,y-1)&&!foreground_at_pixel(x,y+1);
        if(direction==1)return !foreground_at_pixel(x-1,y)&&!foreground_at_pixel(x+1,y);
        return !foreground_at_pixel(x-1,y)&&!foreground_at_pixel(x+1,y)&&
            !foreground_at_pixel(x,y-1)&&!foreground_at_pixel(x,y+1);
    };
    QJsonArray line_candidates;
    for(std::size_t direction=0;direction<line_directions.size();++direction) {
        const auto [dx,dy,name]=line_directions[direction];
        for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x) {
            if(!line_eligible(x,y,direction)||line_eligible(x-dx,y-dy,direction))continue;
            auto end_x=x,end_y=y;
            int length=1;
            while(line_eligible(end_x+dx,end_y+dy,direction)) {
                end_x+=dx;end_y+=dy;++length;
            }
            if(length<3)continue;
            if(static_cast<std::size_t>(line_candidates.size())>=max_line_candidates)
                throw Error("ANALYSIS_LIMIT","Region analysis is limited to 10,000 line candidates");
            QJsonArray start_point;start_point.append(x);start_point.append(y);
            QJsonArray end_point;end_point.append(end_x);end_point.append(end_y);
            QJsonObject line{{"direction",name},{"start",start_point},
                {"end",end_point},{"length_pixels",length}};
            line.insert("id",child_id("line",line));
            line_candidates.append(line);
        }
    }
    QJsonArray morphology_runs;
    std::uint64_t morphology_area=0;
    const auto append_morphology_run=[&](int y,int start,int end) {
        if(static_cast<std::size_t>(morphology_runs.size())>=max_morphology_runs)
            throw Error("ANALYSIS_LIMIT","Region analysis is limited to 20,000 morphology runs");
        const auto run_width=end-start;
        morphology_area+=static_cast<std::uint64_t>(run_width);
        morphology_runs.append(QJsonObject{{"y",y},{"x",start},{"width",run_width}});
    };
    for(int y=0;y<image.height();++y) {
        bool in_run=false;
        int run_start=0;
        for(int x=0;x<image.width();++x) {
            const auto dilated=foreground_at_pixel(x,y)||foreground_at_pixel(x-1,y)||
                foreground_at_pixel(x+1,y)||foreground_at_pixel(x,y-1)||foreground_at_pixel(x,y+1);
            if(dilated&&!in_run) {run_start=x;in_run=true;}
            else if(!dilated&&in_run) {append_morphology_run(y,run_start,x);in_run=false;}
        }
        if(in_run)append_morphology_run(y,run_start,image.width());
    }
    QJsonObject morphology{{"operation","dilate"},{"kernel","cross-4-radius-1"},
        {"border","outside-background-clipped"},{"coordinate_space","artboard-output-pixels"},
        {"area",static_cast<qint64>(morphology_area)},{"runs",morphology_runs}};
    morphology.insert("id",child_id("morphology",morphology));
    QJsonArray erosion_runs;
    std::uint64_t erosion_area=0;
    const auto append_erosion_run=[&](int y,int start,int end) {
        if(static_cast<std::size_t>(erosion_runs.size())>=max_morphology_runs)
            throw Error("ANALYSIS_LIMIT","Region analysis is limited to 20,000 erosion runs");
        const auto run_width=end-start;
        erosion_area+=static_cast<std::uint64_t>(run_width);
        erosion_runs.append(QJsonObject{{"y",y},{"x",start},{"width",run_width}});
    };
    for(int y=0;y<image.height();++y) {
        bool in_run=false;
        int run_start=0;
        for(int x=0;x<image.width();++x) {
            const auto eroded=foreground_at_pixel(x,y)&&foreground_at_pixel(x-1,y)&&
                foreground_at_pixel(x+1,y)&&foreground_at_pixel(x,y-1)&&foreground_at_pixel(x,y+1);
            if(eroded&&!in_run) {run_start=x;in_run=true;}
            else if(!eroded&&in_run) {append_erosion_run(y,run_start,x);in_run=false;}
        }
        if(in_run)append_erosion_run(y,run_start,image.width());
    }
    QJsonObject erosion{{"operation","erode"},{"kernel","cross-4-radius-1"},
        {"border","outside-background"},{"coordinate_space","artboard-output-pixels"},
        {"area",static_cast<qint64>(erosion_area)},{"runs",erosion_runs}};
    erosion.insert("id",child_id("erosion",erosion));
    QJsonArray mask_boolean_runs;
    std::uint64_t mask_boolean_area=0;
    const auto append_mask_boolean_run=[&](int y,int start,int end) {
        if(static_cast<std::size_t>(mask_boolean_runs.size())>=max_morphology_runs)
            throw Error("ANALYSIS_LIMIT","Region analysis is limited to 20,000 mask Boolean runs");
        const auto run_width=end-start;
        mask_boolean_area+=static_cast<std::uint64_t>(run_width);
        mask_boolean_runs.append(QJsonObject{{"y",y},{"x",start},{"width",run_width}});
    };
    std::size_t erosion_index=0;
    for(const auto& morphology_value:morphology_runs) {
        const auto morphology_run=morphology_value.toObject();
        const auto y=morphology_run.value("y").toInt();
        const auto start=morphology_run.value("x").toInt();
        const auto end=start+morphology_run.value("width").toInt();
        auto cursor=start;
        while(erosion_index<static_cast<std::size_t>(erosion_runs.size())) {
            const auto erosion_run=erosion_runs.at(static_cast<int>(erosion_index)).toObject();
            const auto erosion_y=erosion_run.value("y").toInt();
            const auto erosion_start=erosion_run.value("x").toInt();
            const auto erosion_end=erosion_start+erosion_run.value("width").toInt();
            if(erosion_y<y||(erosion_y==y&&erosion_end<=start)) {++erosion_index;continue;}
            if(erosion_y>y||erosion_start>=end)break;
            if(erosion_start>cursor)append_mask_boolean_run(y,cursor,std::min(erosion_start,end));
            cursor=std::max(cursor,erosion_end);
            ++erosion_index;
            if(cursor>=end)break;
        }
        if(cursor<end)append_mask_boolean_run(y,cursor,end);
    }
    QJsonObject mask_boolean{{"operation","difference"},
        {"operands",QJsonArray{
            QJsonObject{{"role","left"},{"mask","morphology"},{"operation","dilate"}},
            QJsonObject{{"role","right"},{"mask","erosion"},{"operation","erode"}}}},
        {"coordinate_space","artboard-output-pixels"},{"width",image.width()},{"height",image.height()},
        {"source_revision",static_cast<qint64>(source_revision)},
        {"area",static_cast<qint64>(mask_boolean_area)},{"runs",mask_boolean_runs}};
    mask_boolean.insert("id",child_id("mask_boolean",mask_boolean));
    QJsonObject result{{"regions",regions},{"edge_runs",edge_runs},{"edge_pixel_count",static_cast<qint64>(edge_pixel_count)},
        {"analysis_id",analysis_id},{"analysis_behavior_version",analysis_behavior_version},
        {"edge_rule","foreground-4-neighbor"},{"threshold",threshold},{"connectivity",4},{"scale",scale},
        {"width",image.width()},{"height",image.height()},{"color_space","sRGB"},
        {"coordinate_space","artboard-output-pixels"},{"origin","top-left"},
        {"pixel_format","ARGB32_Premultiplied"},{"alpha_domain","8-bit premultiplied Canvas output alpha byte"},
        {"source_revision",static_cast<qint64>(source_revision)},
        {"outer_contours",outer_contours},{"contour_rule","foreground-right-clockwise-outer"},
        {"contour_coordinate_space","artboard-output-pixel-corners"},
        {"contour_closed","implicit-last-to-first"},{"line_candidates",line_candidates},
        {"line_rule","exact-one-pixel-wide-4-direction-min3"},
        {"line_coordinate_space","artboard-output-pixel-centers"},{"morphology",morphology},{"erosion",erosion},
        {"mask_boolean",mask_boolean}};
    if(include_color_groups) {
        constexpr std::size_t max_color_groups=256;
        constexpr std::size_t max_color_runs=20'000;
        struct ColorRun {int y,x,width;};
        struct ColorGroup {
            std::uint64_t area=0;
            int min_x=0,min_y=0,max_x=0,max_y=0;
            bool has_bounds=false;
            std::vector<ColorRun> runs;
        };
        const auto straight=image.convertToFormat(QImage::Format_ARGB32);
        if(straight.isNull())throw Error("RENDER_ALLOCATION","Could not convert rendered pixels for color grouping");
        std::map<std::array<int,3>,ColorGroup> groups;
        std::size_t total_runs=0;
        for(int y=0;y<image.height();++y) {
            int x=0;
            while(x<image.width()) {
                while(x<image.width()&&qAlpha(image.pixel(x,y))<threshold)++x;
                if(x==image.width())break;
                const auto first=straight.pixel(x,y);
                const std::array<int,3> key{qRed(first),qGreen(first),qBlue(first)};
                const auto start=x;
                do {
                    ++x;
                    if(x==image.width()||qAlpha(image.pixel(x,y))<threshold)break;
                    const auto pixel=straight.pixel(x,y);
                    if(qRed(pixel)!=key[0]||qGreen(pixel)!=key[1]||qBlue(pixel)!=key[2])break;
                } while(true);
                auto group=groups.find(key);
                if(group==groups.end()) {
                    if(groups.size()>=max_color_groups)
                        throw Error("ANALYSIS_LIMIT","Color grouping is limited to 256 distinct output RGB keys");
                    group=groups.emplace(key,ColorGroup{}).first;
                }
                if(total_runs>=max_color_runs)
                    throw Error("ANALYSIS_LIMIT","Color grouping is limited to 20,000 output color runs");
                ++total_runs;
                auto& value=group->second;
                const auto run_width=x-start;
                value.runs.push_back({y,start,run_width});
                value.area+=static_cast<std::uint64_t>(run_width);
                if(!value.has_bounds) {
                    value.min_x=start;value.max_x=x-1;value.min_y=y;value.max_y=y;value.has_bounds=true;
                } else {
                    value.min_x=std::min(value.min_x,start);value.max_x=std::max(value.max_x,x-1);
                    value.min_y=std::min(value.min_y,y);value.max_y=std::max(value.max_y,y);
                }
            }
        }
        QJsonArray color_groups;
        for(const auto& [rgb,value]:groups) {
            QJsonArray runs;
            for(const auto& run:value.runs)
                runs.append(QJsonObject{{"y",run.y},{"x",run.x},{"width",run.width}});
            QJsonObject group{{"rgb",QJsonArray{rgb[0],rgb[1],rgb[2]}},
                {"area",static_cast<qint64>(value.area)},
                {"bounds",QJsonObject{{"x",value.min_x},{"y",value.min_y},
                    {"width",value.max_x-value.min_x+1},{"height",value.max_y-value.min_y+1}}},
                {"runs",runs}};
            group.insert("id",child_id("color_group",group));
            color_groups.append(group);
        }
        result.insert("color_groups",color_groups);
        result.insert("color_key_domain","output-srgb-straight-rgb8");
        result.insert("color_alpha_rule","output-alpha-byte-ge-threshold");

        if(include_color_components) {
            constexpr std::size_t max_color_components=10'000;
            QJsonArray color_components;
            std::size_t component_count=0;
            for(const auto& [rgb,value]:groups) {
                const auto& runs=value.runs;
                std::vector<std::size_t> parents(runs.size());
                std::vector<std::size_t> ranks(runs.size(),0);
                for(std::size_t index=0;index<parents.size();++index)parents[index]=index;
                const auto find_root=[&](std::size_t index) {
                    auto root=index;
                    while(parents[root]!=root)root=parents[root];
                    while(parents[index]!=index) {
                        const auto next=parents[index];parents[index]=root;index=next;
                    }
                    return root;
                };
                const auto unite=[&](std::size_t left,std::size_t right) {
                    auto left_root=find_root(left),right_root=find_root(right);
                    if(left_root==right_root)return;
                    if(ranks[left_root]<ranks[right_root])std::swap(left_root,right_root);
                    parents[right_root]=left_root;
                    if(ranks[left_root]==ranks[right_root])++ranks[left_root];
                };

                // D8 runs are row-major maximal spans. Only overlapping half-open
                // intervals on immediately adjacent rows share 4-connectivity.
                for(std::size_t row_begin=0;row_begin<runs.size();) {
                    auto row_end=row_begin+1;
                    while(row_end<runs.size()&&runs[row_end].y==runs[row_begin].y)++row_end;
                    if(row_end<runs.size()&&runs[row_end].y==runs[row_begin].y+1) {
                        auto next_end=row_end+1;
                        while(next_end<runs.size()&&runs[next_end].y==runs[row_end].y)++next_end;
                        auto previous=row_begin,current=row_end;
                        while(previous<row_end&&current<next_end) {
                            const auto previous_end=runs[previous].x+runs[previous].width;
                            const auto current_end=runs[current].x+runs[current].width;
                            if(previous_end<=runs[current].x) {++previous;continue;}
                            if(current_end<=runs[previous].x) {++current;continue;}
                            unite(previous,current);
                            if(previous_end<=current_end)++previous;
                            if(current_end<=previous_end)++current;
                        }
                    }
                    row_begin=row_end;
                }

                struct ComponentOutput {
                    std::uint64_t area=0;
                    int min_x=0,min_y=0,max_x=0,max_y=0;
                    std::vector<ColorRun> runs;
                    bool has_bounds=false;
                };
                std::map<std::size_t,std::size_t> component_by_root;
                std::vector<ComponentOutput> components;
                for(std::size_t run_index=0;run_index<runs.size();++run_index) {
                    const auto root=find_root(run_index);
                    auto found=component_by_root.find(root);
                    if(found==component_by_root.end()) {
                        if(component_count>=max_color_components)
                            throw Error("ANALYSIS_LIMIT","Color component analysis is limited to 10,000 components");
                        found=component_by_root.emplace(root,components.size()).first;
                        components.emplace_back();
                        ++component_count;
                    }
                    auto& component=components[found->second];
                    const auto& run=runs[run_index];
                    const auto max_x=run.x+run.width-1;
                    component.area+=static_cast<std::uint64_t>(run.width);
                    if(!component.has_bounds) {
                        component.min_x=run.x;component.max_x=max_x;
                        component.min_y=run.y;component.max_y=run.y;component.has_bounds=true;
                    } else {
                        component.min_x=std::min(component.min_x,run.x);
                        component.max_x=std::max(component.max_x,max_x);
                        component.min_y=std::min(component.min_y,run.y);
                        component.max_y=std::max(component.max_y,run.y);
                    }
                    component.runs.push_back(run);
                }
                for(const auto& component:components) {
                    QJsonArray component_runs;
                    for(const auto& run:component.runs)
                        component_runs.append(QJsonObject{{"y",run.y},{"x",run.x},{"width",run.width}});
                    QJsonObject output{{"component_index",static_cast<int>(color_components.size())},
                        {"rgb",QJsonArray{rgb[0],rgb[1],rgb[2]}},
                        {"area",static_cast<qint64>(component.area)},
                        {"bounds",QJsonObject{{"x",component.min_x},{"y",component.min_y},
                            {"width",component.max_x-component.min_x+1},{"height",component.max_y-component.min_y+1}}},
                        {"runs",component_runs}};
                    auto identity_payload=output;
                    identity_payload.remove("component_index");
                    output.insert("id",child_id("color_component",identity_payload));
                    color_components.append(output);
                }
            }
            result.insert("color_components",color_components);

            if(intersect_color_component_index||intersect_color_component_id) {
                int selected_index=-1;
                if(intersect_color_component_index) {
                    if(*intersect_color_component_index>=static_cast<std::uint64_t>(color_components.size()))
                        throw Error("INVALID_REQUEST","intersect_color_component_index is outside this request's color_components result");
                    selected_index=static_cast<int>(*intersect_color_component_index);
                } else {
                    for(int index=0;index<color_components.size();++index)
                        if(color_components.at(index).toObject().value("id").toString()==*intersect_color_component_id) {
                            selected_index=index;break;
                        }
                    if(selected_index<0)
                        throw Error("INVALID_REQUEST","intersect_color_component_id is not a component of this analysis snapshot");
                }
                const auto selected=color_components.at(selected_index).toObject();
                const auto component_runs=selected.value("runs").toArray();
                std::size_t component_run_index=0,mask_run_index=0;
                std::uint64_t intersection_area=0;
                QJsonArray intersection_runs;
                while(component_run_index<static_cast<std::size_t>(component_runs.size())&&
                      mask_run_index<static_cast<std::size_t>(mask_boolean_runs.size())) {
                    const auto component_run=component_runs.at(static_cast<int>(component_run_index)).toObject();
                    const auto mask_run=mask_boolean_runs.at(static_cast<int>(mask_run_index)).toObject();
                    const auto component_y=component_run.value("y").toInt();
                    const auto mask_y=mask_run.value("y").toInt();
                    if(component_y<mask_y) {++component_run_index;continue;}
                    if(mask_y<component_y) {++mask_run_index;continue;}

                    const auto component_start=component_run.value("x").toInt();
                    const auto component_end=component_start+component_run.value("width").toInt();
                    const auto mask_start=mask_run.value("x").toInt();
                    const auto mask_end=mask_start+mask_run.value("width").toInt();
                    const auto start=std::max(component_start,mask_start);
                    const auto end=std::min(component_end,mask_end);
                    if(start<end) {
                        const auto run_width=end-start;
                        intersection_area+=static_cast<std::uint64_t>(run_width);
                        intersection_runs.append(QJsonObject{{"y",component_y},{"x",start},{"width",run_width}});
                    }
                    if(component_end<=mask_end)++component_run_index;
                    if(mask_end<=component_end)++mask_run_index;
                }
                QJsonObject intersection{
                    {"operation","intersection"},
                    {"component_index",selected_index},{"component_id",selected.value("id")},
                    {"analysis_id",analysis_id},
                    {"rgb",selected.value("rgb")},
                    {"other_operand","mask_boolean"},
                    {"coordinate_space","artboard-output-pixels"},
                    {"width",image.width()},{"height",image.height()},
                    {"source_revision",static_cast<qint64>(source_revision)},
                    {"area",static_cast<qint64>(intersection_area)},
                    {"runs",intersection_runs}};
                auto identity_payload=intersection;
                identity_payload.remove("component_index");
                intersection.insert("id",child_id("intersection",identity_payload));
                result.insert("color_component_mask_intersection",intersection);
            }
        }
    }
    return result;
}

QJsonObject Host::analyze_dataset(const QString& operator_type_id,int operator_version,const QString& input_domain,
    const Id& composition_id,const QJsonObject& parameters,std::uint64_t expected) {
    if(expected!=session.revision())throw Error("REVISION_CONFLICT","Refresh revision before analysis");
    if(session.gesture_active())throw Error("GESTURE_ACTIVE","Finish or cancel the current gesture before analysis");
    const auto descriptor=std::find_if(analysis_operators.begin(),analysis_operators.end(),[&](const auto& item) {
        return operator_type_id==QLatin1String(item.type_id);
    });
    if(descriptor==analysis_operators.end())throw Error("UNSUPPORTED_ANALYSIS_OPERATOR",operator_type_id.toStdString());
    if(operator_version!=descriptor->version)
        throw Error("UNSUPPORTED_ANALYSIS_VERSION",operator_type_id.toStdString()+" version "+std::to_string(operator_version));
    if(input_domain!=QLatin1String(descriptor->input_domain))
        throw Error("UNSUPPORTED_ANALYSIS_DOMAIN",input_domain.toStdString());
    const auto& document=session.document();
    const auto composition=std::find_if(document.compositions.begin(),document.compositions.end(),[&](const auto& item) {
        return item.id==composition_id;
    });
    if(composition==document.compositions.end())throw Error("MISSING_COMPOSITION",composition_id);
    constexpr int analysis_behavior_version=1;
    const auto document_id=QString::fromStdString(document.id);
    const auto composition_id_json=QString::fromStdString(composition_id);
    const auto operator_type=QString::fromLatin1(descriptor->type_id);
    const auto coordinate_space=QString::fromLatin1(descriptor->coordinate_space);
    const auto finish=[&](const QString& analysis_id,const QJsonArray& records,const QJsonObject& limits) {
        return QJsonObject{{"type","analysis.dataset/v1"},{"output_domain","analysis.dataset/v1"},
            {"analysis_id",analysis_id},{"analysis_behavior_version",analysis_behavior_version},
            {"operator_type_id",operator_type},{"operator_version",descriptor->version},
            {"document_id",document_id},{"composition_id",composition_id_json},
            {"source_revision",static_cast<qint64>(expected)},
            {"input_domain",input_domain},{"coordinate_space",coordinate_space},
            {"records",records},{"warnings",QJsonArray{}},{"limits",limits}};
    };

    if(operator_type==QLatin1String("nect.analysis.image.regions")) {
        analysis_parameter_keys(parameters,{"artboard_id","scale","threshold","include_color_groups",
            "include_color_components","intersect_color_component_index","intersect_color_component_id"});
        if(!parameters.contains("artboard_id")||!parameters.contains("scale")||!parameters.contains("threshold"))
            throw Error("INVALID_ANALYSIS_PARAMETER","Image regions requires artboard_id, scale and threshold");
        const auto artboard=analysis_parameter_string(parameters,"artboard_id").toStdString();
        const auto scale=analysis_parameter_number(parameters,"scale");
        const auto threshold=analysis_parameter_integer(parameters,"threshold");
        const auto include_color_groups=analysis_parameter_boolean(parameters,"include_color_groups");
        const auto include_color_components=analysis_parameter_boolean(parameters,"include_color_components");
        std::optional<std::uint64_t> component_index;
        if(parameters.contains("intersect_color_component_index")) {
            const auto value=analysis_parameter_number(parameters,"intersect_color_component_index");
            if(std::floor(value)!=value||value<0)
                throw Error("INVALID_ANALYSIS_PARAMETER","intersect_color_component_index must be a nonnegative integer");
            component_index=value>=18446744073709551616.0?std::numeric_limits<std::uint64_t>::max():
                static_cast<std::uint64_t>(value);
        }
        std::optional<QString> component_id;
        if(parameters.contains("intersect_color_component_id"))
            component_id=analysis_parameter_string(parameters,"intersect_color_component_id");
        const auto legacy=analyze_regions(composition_id,artboard,scale,threshold,expected,include_color_groups,
            include_color_components,component_index,component_id);
        const auto analysis_id=legacy.value("analysis_id").toString();
        QJsonObject record_data{{"result",legacy}};
        const auto record_id=analysis_child_id(analysis_id,"dataset_record",
            QJsonObject{{"operator_type_id",operator_type},{"data",record_data}});
        const QJsonArray records{QJsonObject{{"id",record_id},
            {"type","nect.analysis.image.regions.record@1"},{"data",record_data}}};
        return finish(analysis_id,records,QJsonObject{{"output_pixels",4000000},{"regions",10000},
            {"edge_runs",100000},{"directed_boundary_edges",200000},{"line_candidates",10000},
            {"boolean_mask_runs",20000},{"color_groups",256},{"color_runs",20000},
            {"color_components",10000}});
    }

    const auto structural_parents=analysis_structural_parents(document,*composition);
    if(operator_type==QLatin1String("nect.analysis.vector.geometry")) {
        analysis_parameter_keys(parameters,{"object_id","contour_id"});
        if(!parameters.contains("object_id"))throw Error("INVALID_ANALYSIS_PARAMETER","Vector geometry requires object_id");
        const auto object_id=analysis_parameter_string(parameters,"object_id").toStdString();
        const auto object_it=document.objects.find(object_id);
        if(object_it==document.objects.end())throw Error("MISSING_OBJECT",object_id);
        if(!structural_parents.contains(object_id))throw Error("CROSS_COMPOSITION","Vector target is outside the selected Composition");
        const auto& object=object_it->second;
        if(object.kind!=Kind::path)throw Error("INVALID_DOMAIN","Vector geometry requires a Path object");
        if(object.source)throw Error("UNSUPPORTED_VECTOR_SOURCE","Vector geometry v1 accepts retained Path contours");
        const std::optional<Id> selected_contour=parameters.contains("contour_id")?
            std::optional<Id>(analysis_parameter_string(parameters,"contour_id").toStdString()):std::nullopt;
        const auto values=evaluate(document);
        const auto transforms=evaluate_transforms(document,values);
        const auto contours=path_contours(object,&values);
        std::vector<const Contour*> selected;
        std::size_t point_count=0;
        for(const auto& contour:contours) {
            if(selected_contour&&contour.id!=*selected_contour)continue;
            selected.push_back(&contour);point_count+=contour.points.size();
        }
        if(selected_contour&&selected.empty())throw Error("MISSING_CONTOUR",*selected_contour);
        if(selected.empty())throw Error("INVALID_VECTOR_GEOMETRY","Path has no retained contours");
        constexpr std::size_t max_contours=1024,max_points=10000;
        if(selected.size()>max_contours||point_count>max_points)
            throw Error("ANALYSIS_LIMIT","Vector geometry is limited to 1024 contours and 10,000 points");
        // This intake reports retained source contours, so both bounds use
        // their source geometry rather than transient Path Deform output.
        const auto local_bounds=object_bounds(document,object_id,values,transforms,false,true);
        const auto composition_bounds=object_bounds(document,object_id,values,transforms,true,true);
        if(!local_bounds||!composition_bounds)
            throw Error("INVALID_VECTOR_GEOMETRY","Path has no finite geometric bounds");
        QJsonArray bare_contours;
        for(const auto* contour:selected) {
            QJsonArray bare_points,ordered_point_ids;
            for(std::size_t index=0;index<contour->points.size();++index) {
                const auto& point=contour->points[index];
                const auto value=[&](const char* field) {
                    const auto found=values.find({object_id,point.id,field});
                    if(found==values.end())throw Error("MISSING_REFERENCE",object_id+"/"+point.id+"/"+field);
                    if(!std::isfinite(found->second))throw Error("ANALYSIS_NON_FINITE","Evaluated vector point is non-finite");
                    return found->second;
                };
                const Vec2 anchor{value("x"),value("y")};
                const auto handle=[&](const char* angle_field,const char* length_field) {
                    const auto radians=value(angle_field)*std::numbers::pi/180.0;
                    const auto length=value(length_field);
                    const Vec2 local{anchor.x+std::cos(radians)*length,anchor.y+std::sin(radians)*length};
                    if(!std::isfinite(local.x)||!std::isfinite(local.y))
                        throw Error("ANALYSIS_NON_FINITE","Evaluated vector handle is non-finite");
                    return local;
                };
                const auto incoming=handle("in.angle","in.length");
                const auto outgoing=handle("out.angle","out.length");
                QJsonObject coordinates{{"anchor",QJsonObject{
                        {"object_local",analysis_point_json(anchor)},
                        {"composition",analysis_point_json(map_point(transforms.at(object_id).world,anchor))}}},
                    {"incoming_handle",QJsonObject{
                        {"object_local",analysis_point_json(incoming)},
                        {"composition",analysis_point_json(map_point(transforms.at(object_id).world,incoming))}}},
                    {"outgoing_handle",QJsonObject{
                        {"object_local",analysis_point_json(outgoing)},
                        {"composition",analysis_point_json(map_point(transforms.at(object_id).world,outgoing))}}}};
                bare_points.append(QJsonObject{{"point_id",QString::fromStdString(point.id)},
                    {"order",static_cast<int>(index)},{"coordinates",coordinates}});
                ordered_point_ids.append(QString::fromStdString(point.id));
            }
            const auto sampler=build_path_sampler(document,object_id,contour->id,values);
            if(!std::isfinite(sampler.length)||sampler.length<=0)
                throw Error("ANALYSIS_NON_FINITE","Canonical Path length must be finite and positive");
            bare_contours.append(QJsonObject{{"contour_id",QString::fromStdString(contour->id)},
                {"closed",contour->closed},{"ordered_point_ids",ordered_point_ids},
                {"canonical_length",QJsonObject{{"value",sampler.length},{"coordinate_space","composition"}}},
                {"points",bare_points}});
        }
        const auto transform_parent=object.transform_parent?
            QJsonValue(QString::fromStdString(*object.transform_parent)):QJsonValue(QJsonValue::Null);
        const auto structural_parent=structural_parents.at(object_id).empty()?QJsonValue(QJsonValue::Null):
            QJsonValue(QString::fromStdString(structural_parents.at(object_id)));
        const QJsonObject bare_object{{"object_id",QString::fromStdString(object_id)},
            {"kind","path"},{"name",QString::fromStdString(object.name)},
            {"structural_parent_id",structural_parent},
            {"transform_parent_id",transform_parent},
            {"local_bounds",analysis_bounds_json(*local_bounds)},
            {"composition_bounds",analysis_bounds_json(*composition_bounds)},
            {"contours",bare_contours}};
        const QJsonObject snapshot{{"analysis_behavior_version",analysis_behavior_version},
            {"operator_type_id",operator_type},{"operator_version",descriptor->version},
            {"document_id",document_id},{"composition_id",composition_id_json},
            {"source_revision",static_cast<qint64>(expected)},{"input_domain",input_domain},
            {"coordinate_space",coordinate_space},{"parameters",parameters},{"source",bare_object}};
        const auto analysis_id=analysis_snapshot_id(snapshot);
        QJsonArray output_contours;
        for(const auto& contour_value:bare_contours) {
            auto contour=contour_value.toObject();const auto contour_id=contour.value("contour_id").toString();
            contour.insert("id",analysis_child_id(analysis_id,"contour",
                QJsonObject{{"object_id",QString::fromStdString(object_id)},{"contour_id",contour_id}}));
            QJsonArray output_points;
            for(const auto& point_value:contour.value("points").toArray()) {
                auto point=point_value.toObject();const auto point_id=point.value("point_id").toString();
                point.insert("id",analysis_child_id(analysis_id,"point",QJsonObject{
                    {"object_id",QString::fromStdString(object_id)},{"contour_id",contour_id},{"point_id",point_id}}));
                output_points.append(point);
            }
            contour.insert("points",output_points);output_contours.append(contour);
        }
        QJsonObject output_object=bare_object;
        output_object.insert("id",analysis_child_id(analysis_id,"object",QJsonObject{{"object_id",QString::fromStdString(object_id)}}));
        output_object.insert("contours",output_contours);
        const auto record_id=analysis_child_id(analysis_id,"vector_record",QJsonObject{{"object_id",QString::fromStdString(object_id)}});
        const QJsonArray records{QJsonObject{{"id",record_id},{"type","nect.analysis.vector.geometry.record@1"},
            {"data",QJsonObject{{"object",output_object}}}}};
        return finish(analysis_id,records,QJsonObject{{"contours",1024},{"points",10000}});
    }

    analysis_parameter_keys(parameters,{});
    constexpr std::size_t max_objects=10000,max_artboards=1024,max_collections=1024,max_collection_members=20000;
    if(structural_parents.size()>max_objects||composition->artboards.size()>max_artboards||
       document.collections.size()>max_collections)
        throw Error("ANALYSIS_LIMIT","Document structure exceeds 10,000 objects, 1,024 Artboards or 1,024 Collections");
    std::size_t collection_members=0;
    for(const auto& collection:document.collections) {
        collection_members+=collection.members.size();
        if(collection_members>max_collection_members)
            throw Error("ANALYSIS_LIMIT","Document structure is limited to 20,000 Collection member relations");
    }
    const auto values=evaluate(document);
    (void)evaluate_transforms(document,values);
    const auto visibility=evaluate_object_visibilities(document);
    const auto isolation=evaluate_composite_isolations(document);
    const auto mask_enabled=evaluate_geometry_mask_enableds(document);
    const auto kind_name=[](Kind kind)->const char* {
        switch(kind) {
            case Kind::group:return "group";
            case Kind::path:return "path";
            case Kind::text:return "text";
            case Kind::image:return "image";
            case Kind::instance:return "instance";
        }
        return "unknown";
    };
    QJsonArray root_ids;
    for(const auto& id:composition->roots)root_ids.append(QString::fromStdString(id));
    QJsonArray bare_objects;
    for(const auto& [id,parent]:structural_parents) {
        const auto& object=document.objects.at(id);
        QJsonArray children;
        for(const auto& child:object.children)children.append(QString::fromStdString(child));
        const auto transform_parent=object.transform_parent?
            QJsonValue(QString::fromStdString(*object.transform_parent)):QJsonValue(QJsonValue::Null);
        QJsonValue mask=QJsonValue::Null;
        if(object.compositing.mask) {
            const auto& source=*object.compositing.mask;
            mask=QJsonObject{{"id",QString::fromStdString(source.id)},
                {"source_object_id",QString::fromStdString(source.source)},
                {"enabled",mask_enabled.at(geometry_mask_enabled_ref(id,source.id))},
                {"fill_rule",QString::fromStdString(source.fill_rule)}};
        }
        const auto opacity=values.find({id,"","composite.opacity"});
        if(opacity==values.end()||!std::isfinite(opacity->second))
            throw Error("ANALYSIS_NON_FINITE","Evaluated compositing opacity is missing or non-finite");
        const QJsonObject compositing{{"opacity",opacity->second},
            {"blend",QString::fromStdString(object.compositing.blend)},
            {"isolated",isolation.at(id)},{"mask",mask}};
        bare_objects.append(QJsonObject{{"object_id",QString::fromStdString(id)},
            {"kind",kind_name(object.kind)},{"name",QString::fromStdString(object.name)},
            {"structural_parent_id",parent.empty()?QJsonValue(QJsonValue::Null):QJsonValue(QString::fromStdString(parent))},
            {"child_ids",children},{"visible",visibility.at(id)},
            {"compositing",compositing},{"transform_parent_id",transform_parent}});
    }
    QJsonArray bare_artboards;
    for(const auto& source:composition->artboards) {
        const auto board=evaluate_artboard(*composition,source.id);
        for(const auto value:{board.x,board.y,board.width,board.height})
            if(!std::isfinite(value))throw Error("ANALYSIS_NON_FINITE","Artboard frame contains a non-finite value");
        bare_artboards.append(QJsonObject{{"artboard_id",QString::fromStdString(board.id)},
            {"name",QString::fromStdString(board.name)},
            {"frame",QJsonObject{{"x",board.x},{"y",board.y},{"width",board.width},{"height",board.height}}}});
    }
    QJsonArray bare_collections;
    for(const auto& collection:document.collections) {
        QJsonArray members;std::set<Id> unique;
        for(const auto& member:collection.members) {
            if(!document.objects.contains(member))throw Error("MISSING_OBJECT",member);
            if(!unique.insert(member).second)throw Error("DUPLICATE_MEMBER",member);
            members.append(QString::fromStdString(member));
        }
        bare_collections.append(QJsonObject{{"collection_id",QString::fromStdString(collection.id)},
            {"name",QString::fromStdString(collection.name)},{"member_ids",members}});
    }
    const QJsonObject bare_data{{"root_ids",root_ids},{"objects",bare_objects},
        {"artboards",bare_artboards},{"collections",bare_collections}};
    const QJsonObject snapshot{{"analysis_behavior_version",analysis_behavior_version},
        {"operator_type_id",operator_type},{"operator_version",descriptor->version},
        {"document_id",document_id},{"composition_id",composition_id_json},
        {"source_revision",static_cast<qint64>(expected)},{"input_domain",input_domain},
        {"coordinate_space",coordinate_space},{"source",bare_data}};
    const auto analysis_id=analysis_snapshot_id(snapshot);
    QJsonArray output_objects;
    for(const auto& object_value:bare_objects) {
        auto object=object_value.toObject();
        object.insert("id",analysis_child_id(analysis_id,"object",
            QJsonObject{{"object_id",object.value("object_id").toString()}}));
        output_objects.append(object);
    }
    QJsonArray output_artboards;
    for(const auto& artboard_value:bare_artboards) {
        auto artboard=artboard_value.toObject();
        artboard.insert("id",analysis_child_id(analysis_id,"artboard",
            QJsonObject{{"artboard_id",artboard.value("artboard_id").toString()}}));
        output_artboards.append(artboard);
    }
    QJsonArray output_collections;
    for(const auto& collection_value:bare_collections) {
        auto collection=collection_value.toObject();
        collection.insert("id",analysis_child_id(analysis_id,"collection",
            QJsonObject{{"collection_id",collection.value("collection_id").toString()}}));
        output_collections.append(collection);
    }
    const QJsonObject data{{"document_id",document_id},{"composition_id",composition_id_json},
        {"root_ids",root_ids},{"objects",output_objects},{"artboards",output_artboards},
        {"collections",output_collections}};
    const auto record_id=analysis_child_id(analysis_id,"structure",QJsonObject{{"composition_id",composition_id_json}});
    const QJsonArray records{QJsonObject{{"id",record_id},{"type","nect.analysis.document.structure.record@1"},
        {"data",data}}};
    return finish(analysis_id,records,QJsonObject{{"objects",static_cast<qint64>(max_objects)},
        {"artboards",static_cast<qint64>(max_artboards)},{"collections",static_cast<qint64>(max_collections)},
        {"collection_member_relations",static_cast<qint64>(max_collection_members)}});
}

QJsonObject Host::analyze_regions(const Id& composition,const Id& artboard,double scale,int threshold,std::uint64_t expected,
    bool include_color_groups,bool include_color_components,
    std::optional<std::uint64_t> intersect_color_component_index,
    std::optional<QString> intersect_color_component_id) {
    if(include_color_components&&!include_color_groups)
        throw Error("INVALID_REQUEST","include_color_components requires include_color_groups to be true");
    if(intersect_color_component_index&&intersect_color_component_id)
        throw Error("INVALID_REQUEST","Supply only one color component selector");
    if((intersect_color_component_index||intersect_color_component_id)&&(!include_color_groups||!include_color_components))
        throw Error("INVALID_REQUEST","Color component selection requires include_color_groups and include_color_components to be true");
    if(expected!=session.revision())throw Error("REVISION_CONFLICT","Refresh revision before analyzing regions");
    if(session.gesture_active())throw Error("GESTURE_ACTIVE","Finish or cancel the current gesture before analyzing regions");
    if(!std::isfinite(scale)||scale<=0||scale>16)throw Error("EXPORT_SCALE","Analysis scale must be greater than zero and at most 16");
    if(threshold<1||threshold>255)throw Error("INVALID_THRESHOLD","Alpha threshold must be an integer from 1 through 255");
    const auto found=std::find_if(session.document().compositions.begin(),session.document().compositions.end(),
        [&](const auto& value){return value.id==composition;});
    if(found==session.document().compositions.end())throw Error("MISSING_COMPOSITION",composition);
    const auto board=evaluate_artboard(*found,artboard);
    const auto pixel_width=std::ceil(board.width*scale),pixel_height=std::ceil(board.height*scale);
    constexpr double renderer_axis_limit=8192;
    constexpr double renderer_pixel_limit=16'777'216;
    if(!std::isfinite(pixel_width)||!std::isfinite(pixel_height)||pixel_width<1||pixel_height<1||
       pixel_width>renderer_axis_limit||pixel_height>renderer_axis_limit||pixel_width*pixel_height>renderer_pixel_limit)
        throw Error("EXPORT_LIMIT","PNG output is limited to 8192 pixels per axis and 16,777,216 pixels");
    if(pixel_width*pixel_height>4'000'000)
        throw Error("ANALYSIS_LIMIT","Region analysis is limited to 4,000,000 output pixels");
    const auto image=Canvas::render_artboard(session.document(),composition,artboard,scale,false,false);
    return analyze_region_pixels(image,threshold,scale,expected,include_color_groups,include_color_components,
        intersect_color_component_index,intersect_color_component_id,session.document().id,composition,artboard);
}

QJsonObject Host::adopt_analysis_contour(const Id& composition,const Id& artboard,double scale,int threshold,
    const QString& analysis_id,const QString& contour_id,const std::string& name,
    const QString& expected_session,std::uint64_t expected) {
    if(expected_session!=session_id)throw Error("SESSION_CONFLICT","Document changed; analyze the current document again");
    if(analysis_id.isEmpty()||contour_id.isEmpty())throw Error("INVALID_REQUEST","Analysis and outer contour IDs required");
    // Re-render the exact committed source: IDs are snapshot identities, never a
    // client-supplied vertex list or an index into a possibly different result.
    const auto analysis=analyze_regions(composition,artboard,scale,threshold,expected);
    if(analysis.value("analysis_id").toString()!=analysis_id)
        throw Error("ANALYSIS_CONFLICT","Analysis snapshot changed; analyze again before adopting");
    QJsonObject selected;
    for(const auto& candidate:analysis.value("outer_contours").toArray()) {
        const auto contour=candidate.toObject();
        if(contour.value("id").toString()==contour_id){selected=contour;break;}
    }
    if(selected.isEmpty())throw Error("INVALID_REQUEST","Outer contour does not belong to this analysis snapshot");
    const auto comp=std::find_if(session.document().compositions.begin(),session.document().compositions.end(),
        [&](const auto& value){return value.id==composition;});
    const auto board=evaluate_artboard(*comp,artboard);
    std::vector<Vec2> corners;
    std::vector<Id> point_ids;
    QJsonArray authored_point_ids;
    for(const auto& vertex:selected.value("vertices").toArray()) {
        const auto xy=vertex.toArray();
        corners.push_back({xy.at(0).toDouble(),xy.at(1).toDouble()});
        point_ids.push_back(new_id());
        authored_point_ids.append(QString::fromStdString(point_ids.back()));
    }
    const auto object_id=new_id(),authored_contour_id=new_id();
    auto contour=adopt_analysis_outer_contour(corners,
        {{board.x,board.y},scale,analysis.value("width").toInt(),analysis.value("height").toInt()},
        authored_contour_id,point_ids);
    // Root insertion keeps Composition coordinates and preserves every source
    // object. CreatePath supplies the normal editable Path and one Undo entry.
    session.apply({CreatePath{composition,{},object_id,name,{std::move(contour)}}},expected);
    QJsonObject result{{"object_id",QString::fromStdString(object_id)},
        {"contour_id",QString::fromStdString(authored_contour_id)},{"point_ids",authored_point_ids},
        {"source_analysis_id",analysis_id},{"source_contour_id",contour_id},
        {"source_revision",static_cast<qint64>(expected)},{"revision",static_cast<qint64>(session.revision())},
        {"coordinate_space","composition"},{"geometry","outer-contour-straight-anchors"},
        {"holes_preserved",false}};
    edited();
    return result;
}

QByteArray Host::dispatch(const QByteArray& input) {
    QJsonObject response;
    try {
        validate_json(std::string_view(input.constData(),static_cast<std::size_t>(input.size())));
        auto outer=QJsonDocument::fromJson(input).object();
        const auto operation=string(outer,"op");
        const QStringList allowed=operation=="hello"?QStringList{"op"}:
             operation=="adopt_analysis_contour"?QStringList{"op","session_id","document_id","expected_revision","composition","artboard","scale","threshold","analysis_id","contour_id","name"}:
            (operation=="analysis_dataset"?QStringList{"op","session_id","document_id","expected_revision","operator_type_id","operator_version","input_domain","composition_id","parameters"}:
            (operation=="analyze_regions"?QStringList{"op","session_id","document_id","expected_revision","composition","artboard","scale","threshold","include_color_groups","include_color_components","intersect_color_component_index","intersect_color_component_id"}:
            (operation=="export_png"?QStringList{"op","session_id","document_id","expected_revision","path","composition","artboard","scale","background"}:
             operation=="core"?QStringList{"op","session_id","document_id","request"}:
             operation=="import_svg"?QStringList{"op","session_id","document_id","expected_revision","path","composition","prefix","name","x","y"}:
                (operation=="import_image"?QStringList{"op","session_id","document_id","expected_revision","path","mode","composition","parent","asset","id","name","x","y"}:
                 operation=="asset"?QStringList{"op","session_id","document_id","expected_revision","asset","action","path"}:
                 QStringList{"op","session_id","document_id","expected_revision","path"}))));
        for(auto it=outer.begin();it!=outer.end();++it)
            if(!allowed.contains(it.key()))throw Error("UNKNOWN_FIELD",it.key().toStdString());
        if(string(outer,"op")=="hello") {
            response={{"ok",true},{"session_id",session_id},{"document_id",QString::fromStdString(session.document().id)},
                {"revision",static_cast<qint64>(session.revision())},{"file",file_path},{"save_status",save_status},
                {"persistence",persistence()},
                {"transport","nect-local-session"},{"protocol",1}};
        } else {
            if(string(outer,"session_id")!=session_id || string(outer,"document_id").toStdString()!=session.document().id)
                throw Error("SESSION_CONFLICT","Refresh the live document/session identity");
            const auto op=string(outer,"op");
            const auto before=session.revision();
            if(op=="core") {
                if(!outer.value("request").isObject()) throw Error("INVALID_REQUEST","Request object required");
                const auto bytes=QJsonDocument(outer.value("request").toObject()).toJson(QJsonDocument::Compact);
                response=QJsonDocument::fromJson(QByteArray::fromStdString(request(session,
                    std::string_view(bytes.constData(),static_cast<std::size_t>(bytes.size()))))).object();
                if(session.revision()!=before) edited();
            } else if(op=="analysis_dataset") {
                if(!outer.value("expected_revision").isDouble()||!outer.value("operator_version").isDouble()||
                   !outer.value("parameters").isObject())
                    throw Error("INVALID_REQUEST","Integer expected_revision/operator_version and parameters object required");
                const auto revision=outer.value("expected_revision").toDouble();
                if(!std::isfinite(revision)||std::floor(revision)!=revision||revision<0||
                   revision>=std::ldexp(1.0,63))
                    throw Error("INVALID_REQUEST","expected_revision must be a nonnegative integer");
                const auto version=outer.value("operator_version").toDouble();
                if(!std::isfinite(version)||std::floor(version)!=version||version<1||version>std::numeric_limits<int>::max())
                    throw Error("INVALID_REQUEST","operator_version must be a positive integer");
                response={{"ok",true},{"result",analyze_dataset(string(outer,"operator_type_id"),static_cast<int>(version),
                    string(outer,"input_domain"),string(outer,"composition_id").toStdString(),
                    outer.value("parameters").toObject(),static_cast<std::uint64_t>(revision))}};
            } else {
                const auto expected=outer.value("expected_revision");
                if(!expected.isDouble() || expected.toDouble()!=static_cast<double>(session.revision()))
                    throw Error("REVISION_CONFLICT","Refresh revision before file/session operations");
                if(op=="export_png") {
                    const auto background=string(outer,"background");
                    if(!outer.value("scale").isDouble()||(background!="white"&&background!="transparent"))
                        throw Error("INVALID_REQUEST","Numeric scale and transparent/white background required");
                    response={{"ok",true},{"result",export_png(string(outer,"path"),string(outer,"composition").toStdString(),
                        string(outer,"artboard").toStdString(),outer.value("scale").toDouble(),background=="white",session.revision())}};
                } else if(op=="adopt_analysis_contour") {
                    if(!outer.value("scale").isDouble()||!outer.value("threshold").isDouble())
                        throw Error("INVALID_REQUEST","Numeric scale and integer alpha threshold required");
                    const auto threshold=outer.value("threshold").toDouble();
                    if(!std::isfinite(threshold)||std::floor(threshold)!=threshold||threshold<1||threshold>255)
                        throw Error("INVALID_THRESHOLD","Alpha threshold must be an integer from 1 through 255");
                    response={{"ok",true},{"result",adopt_analysis_contour(string(outer,"composition").toStdString(),
                        string(outer,"artboard").toStdString(),outer.value("scale").toDouble(),static_cast<int>(threshold),
                        string(outer,"analysis_id"),string(outer,"contour_id"),string(outer,"name").toStdString(),
                        string(outer,"session_id"),session.revision())}};
                } else if(op=="analyze_regions") {
                    if(!outer.value("scale").isDouble()||!outer.value("threshold").isDouble())
                        throw Error("INVALID_REQUEST","Numeric scale and integer alpha threshold required");
                    const auto threshold=outer.value("threshold").toDouble();
                    if(!std::isfinite(threshold)||std::floor(threshold)!=threshold||threshold<1||threshold>255)
                        throw Error("INVALID_THRESHOLD","Alpha threshold must be an integer from 1 through 255");
                    bool include_color_groups=false;
                    if(outer.contains("include_color_groups")) {
                        if(!outer.value("include_color_groups").isBool())
                            throw Error("INVALID_REQUEST","include_color_groups must be a Boolean");
                        include_color_groups=outer.value("include_color_groups").toBool();
                    }
                    bool include_color_components=false;
                    if(outer.contains("include_color_components")) {
                        if(!outer.value("include_color_components").isBool())
                            throw Error("INVALID_REQUEST","include_color_components must be a Boolean");
                        include_color_components=outer.value("include_color_components").toBool();
                    }
                    std::optional<std::uint64_t> intersect_color_component_index;
                    if(outer.contains("intersect_color_component_index")) {
                        const auto value=outer.value("intersect_color_component_index");
                        if(!value.isDouble())
                            throw Error("INVALID_REQUEST","intersect_color_component_index must be a nonnegative integer");
                        const auto number=value.toDouble();
                        if(!std::isfinite(number)||std::floor(number)!=number||number<0)
                            throw Error("INVALID_REQUEST","intersect_color_component_index must be a nonnegative integer");
                        // Preserve D8/D9 cap precedence for any integral selector: the sentinel
                        // is still rejected against the bounded component array after D9 runs.
                        intersect_color_component_index=number>=18446744073709551616.0?
                            std::numeric_limits<std::uint64_t>::max():static_cast<std::uint64_t>(number);
                    }
                    std::optional<QString> intersect_color_component_id;
                    if(outer.contains("intersect_color_component_id")) {
                        const auto value=outer.value("intersect_color_component_id");
                        if(!value.isString()||value.toString().isEmpty())
                            throw Error("INVALID_REQUEST","intersect_color_component_id must be a nonempty string");
                        intersect_color_component_id=value.toString();
                    }
                    if(intersect_color_component_index&&intersect_color_component_id)
                        throw Error("INVALID_REQUEST","Supply only one color component selector");
                    if(include_color_components&&!include_color_groups)
                        throw Error("INVALID_REQUEST","include_color_components requires include_color_groups to be true");
                    if((intersect_color_component_index||intersect_color_component_id)&&
                       (!include_color_groups||!include_color_components))
                        throw Error("INVALID_REQUEST","Color component selection requires include_color_groups and include_color_components to be true");
                    response={{"ok",true},{"result",analyze_regions(string(outer,"composition").toStdString(),
                        string(outer,"artboard").toStdString(),outer.value("scale").toDouble(),static_cast<int>(threshold),session.revision(),
                        include_color_groups,include_color_components,intersect_color_component_index,
                        intersect_color_component_id)}};
                } else if(op=="import_svg") {
                    if(!outer.value("x").isDouble()||!outer.value("y").isDouble())throw Error("INVALID_REQUEST","Numeric x/y required");
                    response={{"ok",true},{"result",import_svg(string(outer,"path"),string(outer,"composition").toStdString(),string(outer,"prefix").toStdString(),
                        string(outer,"name").toStdString(),outer.value("x").toDouble(),outer.value("y").toDouble(),session.revision())}};
                } else if(op=="import_image") {
                    const auto asset=string(outer,"asset").toStdString(),object=string(outer,"id").toStdString();
                    if(!outer.value("x").isDouble()||!outer.value("y").isDouble())throw Error("INVALID_REQUEST","Numeric x/y required");
                    import_image(string(outer,"path"),string(outer,"mode").toStdString(),string(outer,"composition").toStdString(),string(outer,"parent").toStdString(),
                        asset,object,string(outer,"name").toStdString(),outer.value("x").toDouble(),outer.value("y").toDouble(),session.revision());
                    response={{"ok",true},{"result",QJsonObject{{"changed_ids",QJsonArray{QString::fromStdString(asset),QString::fromStdString(object)}},{"status",asset_status(asset)}}}};
                } else if(op=="asset") {
                    const auto id=string(outer,"asset").toStdString(),action=string(outer,"action").toStdString();
                    if(action=="check"||action=="status")response={{"ok",true},{"result",action=="check"?check_asset(id):asset_status(id)}};
                    else {
                        update_asset(id,action,action=="relink"?string(outer,"path"):QString{},session.revision());
                        QJsonArray changed_ids{QString::fromStdString(id)};
                        for(const auto& [object_id,object]:session.document().objects)if(object.image&&object.image->asset==id)changed_ids.append(QString::fromStdString(object_id));
                        response={{"ok",true},{"result",QJsonObject{{"changed_ids",changed_ids},{"status",asset_status(id)}}}};
                    }
                } else if(op=="save") save(string(outer,"path"));
                else if(op=="open") open(string(outer,"path"));
                else if(op=="open_recovery") open_recovery(string(outer,"path"));
                else if(op=="new") create_document();
                else if(op=="recover") recover();
                else throw Error("UNSUPPORTED_OPERATION",op.toStdString());
                if(response.isEmpty())response={{"ok",true}};
            }
            response["session_id"]=session_id;
            response["document_id"]=QString::fromStdString(session.document().id);
            response["revision"]=static_cast<qint64>(session.revision());
        }
    } catch(const Error& e) {
        response={{"ok",false},{"error",QJsonObject{{"code",QString::fromStdString(e.code)},
            {"message",QString::fromUtf8(e.what())}}},{"revision",static_cast<qint64>(session.revision())}};
    } catch(const std::exception& e) {
        response={{"ok",false},{"error",QJsonObject{{"code","INVALID_REQUEST"},{"message",QString::fromUtf8(e.what())}}}};
    }
    return QJsonDocument(response).toJson(QJsonDocument::Compact);
}
}

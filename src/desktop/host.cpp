#include "host.hpp"
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
    return {{"path",path},{"width",image.width()},{"height",image.height()},{"scale",scale},
        {"background",white_background?"white":"transparent"},{"color_space","sRGB"},{"revision",static_cast<qint64>(expected)}};
}

QJsonObject analyze_region_pixels(const QImage& image,int threshold,double scale,std::uint64_t source_revision) {
    if(threshold<1||threshold>255)throw Error("INVALID_THRESHOLD","Alpha threshold must be an integer from 1 through 255");
    if(!std::isfinite(scale)||scale<=0||scale>16)throw Error("EXPORT_SCALE","Analysis scale must be greater than zero and at most 16");
    if(image.isNull()||image.width()<1||image.height()<1)throw Error("RENDER_ALLOCATION","Could not read rendered analysis pixels");
    constexpr std::uint64_t max_pixels=4'000'000;
    constexpr std::size_t max_regions=10'000;
    constexpr std::size_t max_edge_runs=100'000;
    constexpr std::size_t max_line_candidates=10'000;
    constexpr std::uint64_t max_boundary_edges=200'000;
    const auto width=static_cast<std::uint64_t>(image.width());
    const auto height=static_cast<std::uint64_t>(image.height());
    const auto pixel_count=width*height;
    if(pixel_count>max_pixels)throw Error("ANALYSIS_LIMIT","Region analysis is limited to 4,000,000 output pixels");

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
        regions.append(QJsonObject{{"area",static_cast<qint64>(area)},
            {"x",min_x},{"y",min_y},{"width",max_x-min_x+1},{"height",max_y-min_y+1}});
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
        outer_contours.append(QJsonObject{{"region_index",static_cast<int>(contour.region)},
            {"closed",true},{"vertices",vertices}});
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
            line_candidates.append(QJsonObject{{"direction",name},{"start",start_point},
                {"end",end_point},{"length_pixels",length}});
        }
    }
    return {{"regions",regions},{"edge_runs",edge_runs},{"edge_pixel_count",static_cast<qint64>(edge_pixel_count)},
        {"edge_rule","foreground-4-neighbor"},{"threshold",threshold},{"connectivity",4},{"scale",scale},
        {"width",image.width()},{"height",image.height()},{"color_space","sRGB"},
        {"coordinate_space","artboard-output-pixels"},{"origin","top-left"},
        {"pixel_format","ARGB32_Premultiplied"},{"alpha_domain","8-bit premultiplied Canvas output alpha byte"},
        {"source_revision",static_cast<qint64>(source_revision)},
        {"outer_contours",outer_contours},{"contour_rule","foreground-right-clockwise-outer"},
        {"contour_coordinate_space","artboard-output-pixel-corners"},
        {"contour_closed","implicit-last-to-first"},{"line_candidates",line_candidates},
        {"line_rule","exact-one-pixel-wide-4-direction-min3"},
        {"line_coordinate_space","artboard-output-pixel-centers"}};
}

QJsonObject Host::analyze_regions(const Id& composition,const Id& artboard,double scale,int threshold,std::uint64_t expected) {
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
    const auto image=Canvas::render_artboard(session.document(),composition,artboard,scale,false);
    return analyze_region_pixels(image,threshold,scale,expected);
}

QByteArray Host::dispatch(const QByteArray& input) {
    QJsonObject response;
    try {
        validate_json(std::string_view(input.constData(),static_cast<std::size_t>(input.size())));
        auto outer=QJsonDocument::fromJson(input).object();
        const auto operation=string(outer,"op");
        const QStringList allowed=operation=="hello"?QStringList{"op"}:
            (operation=="analyze_regions"?QStringList{"op","session_id","document_id","expected_revision","composition","artboard","scale","threshold"}:
            (operation=="export_png"?QStringList{"op","session_id","document_id","expected_revision","path","composition","artboard","scale","background"}:
             operation=="core"?QStringList{"op","session_id","document_id","request"}:
             operation=="import_svg"?QStringList{"op","session_id","document_id","expected_revision","path","composition","prefix","name","x","y"}:
                (operation=="import_image"?QStringList{"op","session_id","document_id","expected_revision","path","mode","composition","parent","asset","id","name","x","y"}:
                 operation=="asset"?QStringList{"op","session_id","document_id","expected_revision","asset","action","path"}:
                 QStringList{"op","session_id","document_id","expected_revision","path"})));
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
                } else if(op=="analyze_regions") {
                    if(!outer.value("scale").isDouble()||!outer.value("threshold").isDouble())
                        throw Error("INVALID_REQUEST","Numeric scale and integer alpha threshold required");
                    const auto threshold=outer.value("threshold").toDouble();
                    if(!std::isfinite(threshold)||std::floor(threshold)!=threshold||threshold<1||threshold>255)
                        throw Error("INVALID_THRESHOLD","Alpha threshold must be an integer from 1 through 255");
                    response={{"ok",true},{"result",analyze_regions(string(outer,"composition").toStdString(),
                        string(outer,"artboard").toStdString(),outer.value("scale").toDouble(),static_cast<int>(threshold),session.revision())}};
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

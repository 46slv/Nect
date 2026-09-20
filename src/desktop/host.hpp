#pragma once
#include "nect/io.hpp"
#include "protection.hpp"
#include <QObject>
#include <QLocalServer>
#include <QTimer>
#include <QString>
#include <functional>
#include <future>
#include <QJsonObject>
#include <QElapsedTimer>

namespace nect::desktop {
Id new_id();
class Host : public QObject {
public:
    explicit Host(QString recovery_directory, QObject* parent=nullptr,ProtectionWriter writer=protect_snapshot);
    ~Host() override;
    Session session;
    QString session_id;
    QString file_path;
    QString save_status = "New document";
    std::function<void()> changed;
    std::function<void()> status_changed;
    QByteArray dispatch(const QByteArray& input);
    void listen(const QString& endpoint);
    QString endpoint() const { return server_.fullServerName(); }
    void edited();
    void create_document();
    void open(const QString& path);
    void open_recovery(const QString& path);
    void save(const QString& path);
    void recover();
    void flush();
    QString recovery_directory() const { return recovery_directory_; }
    bool dirty() const { return saved_revision_ != session.revision() || file_path.isEmpty(); }
    QJsonObject persistence() const;
    void import_image(const QString& path,const std::string& mode,const Id& composition,const Id& parent,
        const Id& asset,const Id& object,const std::string& name,double x,double y,std::uint64_t expected);
    void update_asset(const Id& asset,const std::string& action,const QString& path,std::uint64_t expected);
    QJsonObject check_asset(const Id& asset);
    QJsonObject asset_status(const Id& asset) const;
private:
    QLocalServer server_;
    struct LinkObservation { std::string hash,locator; QJsonObject value; };
    std::map<Id,LinkObservation> link_observations_;
    QTimer recovery_timer_;
    QTimer completion_timer_;
    QString recovery_directory_;
    std::optional<std::uint64_t> saved_revision_,protected_revision_;
    FileStamp file_stamp_;
    QString protected_file_;
    StorageError native_error_,recovery_error_;
    ProtectionWriter writer_;
    std::optional<ProtectionSnapshot> pending_;
    std::future<ProtectionResult> running_;
    std::optional<std::uint64_t> running_revision_;
    bool pending_due_=false;
    QElapsedTimer backup_clock_;
    qint64 last_native_backup_=-30000,last_recovery_backup_=-30000;
    std::shared_ptr<QLockFile> recovery_lease_;
    ProtectionSnapshot snapshot() const;
    void queue_protection();
    void start_protection();
    void collect_protection();
    void drain_protection();
    void accept(ProtectionResult result);
    void refresh_status();
    void reset(Document document,const QString& path,FileStamp stamp={});
};
}

#pragma once
#include "nect/io.hpp"
#include <QObject>
#include <QLocalServer>
#include <QTimer>
#include <QString>
#include <functional>

namespace nect::desktop {
Id new_id();
class Host : public QObject {
public:
    explicit Host(QString recovery_directory, QObject* parent=nullptr);
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
    void save(const QString& path);
    void recover();
    QString recovery_directory() const { return recovery_directory_; }
    bool dirty() const { return saved_revision_ != session.revision() || file_path.isEmpty(); }
private:
    QLocalServer server_;
    QTimer recovery_timer_;
    QString recovery_directory_;
    std::uint64_t saved_revision_ = 0;
    QString protected_session_;
    std::uint64_t protected_revision_ = ~std::uint64_t(0);
    void protect();
    void reset(Document document, const QString& path);
};
}

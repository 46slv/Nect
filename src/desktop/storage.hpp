#pragma once
#include "nect/io.hpp"
#include <QByteArray>
#include <QString>
#include <optional>

namespace nect::desktop {
// Hash the actual bytes read, including an older native format's spelling.
struct FileStamp {
    bool exists=false;
    QByteArray sha256;
    bool operator==(const FileStamp&) const = default;
};
struct LoadedNative { Document document; FileStamp stamp; };
QString native_path(const QString& path);
bool same_native_path(const QString& first,const QString& second);
LoadedNative load_native(const QString& path);
// Cooperative lock + optimistic content check + QSaveFile replacement. No direct
// write fallback. A null expectation is reserved for explicit Save As consent.
// Previous generations are retained only when requested, at most ten per file.
// Failed writes never prune prior generations. Thread-safe, no Session/UI access.
FileStamp store_native(const QString& path,const QByteArray& bytes,
                      const std::optional<FileStamp>& expected,bool keep_previous);
}

#pragma once
#include "nect/io.hpp"
#include <QByteArray>
#include <QString>
#include <optional>
#ifdef NECT_STORAGE_FAULT_TESTING
#include <functional>
#endif

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
#ifdef NECT_STORAGE_FAULT_TESTING
// Explicit test-build instrumentation of this writer, never a runtime switch.
// Install in the calling writer thread; nesting restores the previous callback.
// Paths are canonical. after_write is before QSaveFile::commit, not an fsync
// claim; after_commit precedes readback and the caller's success receipt.
enum class NativeWritePhase { before_open,before_write,after_write,before_commit,after_commit,before_readback };
enum class NativeWriteFault { none,permission_denied,no_space,short_write };
using NativeWriteHook=std::function<NativeWriteFault(const QString&,NativeWritePhase)>;
class ScopedNativeWriteHook {
public:
    explicit ScopedNativeWriteHook(NativeWriteHook hook);
    ~ScopedNativeWriteHook();
    ScopedNativeWriteHook(const ScopedNativeWriteHook&)=delete;
    ScopedNativeWriteHook& operator=(const ScopedNativeWriteHook&)=delete;
private:
    NativeWriteHook previous_;
};
#endif
}

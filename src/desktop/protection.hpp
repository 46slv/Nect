#pragma once
#include "storage.hpp"
#include <QLockFile>
#include <functional>
#include <memory>

namespace nect::desktop {
struct StorageError { QString code,message; bool empty() const {return code.isEmpty();} };
struct ProtectionSnapshot {
    Document document;
    QString session_id,native_file,recovery_directory;
    std::uint64_t revision=0;
    std::optional<FileStamp> expected_native;
    bool write_native=false,native_backup=false,recovery_backup=false;
    std::shared_ptr<QLockFile> recovery_lease;
};
struct ProtectionResult {
    QString session_id,native_file;
    std::uint64_t revision=0;
    bool native_written=false,recovery_written=false,native_backup=false,recovery_backup=false;
    FileStamp native_stamp;
    StorageError native_error,recovery_error;
    std::shared_ptr<QLockFile> recovery_lease;
};
// Worker boundary: an immutable committed snapshot in, storage receipts out.
// This function never reads a live Session or invokes UI callbacks.
ProtectionResult protect_snapshot(ProtectionSnapshot snapshot);
using ProtectionWriter=std::function<ProtectionResult(ProtectionSnapshot)>;
}

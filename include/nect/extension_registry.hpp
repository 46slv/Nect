#pragma once
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace nect::extension {
// Developer-only owner. No dependency on Nect core, native codec or catalog.
class Error : public std::runtime_error {
public:
    std::string code;
    Error(std::string code, std::string message);
};
struct ScalarType {
    std::string type_id, label;
    unsigned behavior_version = 1;
    double default_factor = 1, minimum_factor = 0, maximum_factor = 4;
};
struct Package {
    std::filesystem::path root, manifest_path, binary_path;
    std::string package_id, package_version, sha256;
    ScalarType type;
};
// Immutable, manifest-only snapshot; no library/callback reference is retained.
// Each explicit root contains extension.json itself or immediate package dirs.
class Snapshot {
public:
    static Snapshot discover(const std::vector<std::filesystem::path>& roots);
    const std::vector<Package>& packages() const noexcept { return packages_; }
private:
    std::vector<Package> packages_;
};
struct OwnedTestTrust {
    std::filesystem::path package_root;
    std::string package_id, package_version, sha256;
    std::string os = "windows", arch = "x86_64";
};
struct LoadedState;
class ScalarInstance {
public:
    ScalarInstance(ScalarInstance&&) noexcept;
    ScalarInstance& operator=(ScalarInstance&&) noexcept;
    ~ScalarInstance();
    ScalarInstance(const ScalarInstance&) = delete;
    ScalarInstance& operator=(const ScalarInstance&) = delete;
    double evaluate(double value, double factor);
    // Explicit close reports a broken destroy callback. Destructor also closes;
    // on failure the DLL/file lock stays resident until process exit.
    void close();
private:
    friend class DeveloperHarness;
    explicit ScalarInstance(std::shared_ptr<LoadedState>, void*);
    void reset() noexcept;
    std::shared_ptr<LoadedState> state_;
    void* instance_ = nullptr;
};
class DeveloperHarness {
public:
    // Explicit owned-test command only: discovery, document open and product
    // Session commands have no load entrypoint. Trust is exact and not sticky.
    static DeveloperHarness load_owned_test(const Package&, const OwnedTestTrust&);
    ScalarInstance create_scalar_instance() const;
    const Package& package() const;
private:
    explicit DeveloperHarness(std::shared_ptr<LoadedState>);
    std::shared_ptr<LoadedState> state_;
};
} // namespace nect::extension

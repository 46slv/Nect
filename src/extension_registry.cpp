#include "nect/extension_registry.hpp"
#include "nect/extension_abi.h"
#include <boost/json.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <set>
#include <sstream>
#include <utility>
#if !defined(_WIN32) || !defined(_M_X64)
#error R06-C2A loader is qualified only for Windows x86_64
#endif
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>

namespace nect::extension {
namespace {
namespace fs = std::filesystem;
namespace json = boost::json;
[[noreturn]] void fail(const char* code, const std::string& message) { throw Error(code, message); }

bool utf8(const std::string& text) {
    if (text.empty() || text.find('\0') != std::string::npos) return false;
    return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
        static_cast<int>(text.size()), nullptr, 0) > 0;
}
bool namespaced(const std::string& s) {
    if (s.empty() || s.size() > 255 || s.find('.') == std::string::npos) return false;
    bool segment = false;
    for (unsigned char c : s) {
        if (c == '.') { if (!segment) return false; segment = false; }
        else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                 (c >= '0' && c <= '9') || c == '_' || c == '-') segment = true;
        else return false;
    }
    return segment;
}
const json::object& object(const json::value& v) {
    if (!v.is_object()) fail("MANIFEST_SCHEMA", "Expected JSON object");
    return v.as_object();
}
void fields(const json::object& o, std::initializer_list<const char*> required, bool metadata = false) {
    for (auto key : required) if (!o.contains(key)) fail("MANIFEST_SCHEMA", std::string("Missing ") + key);
    for (const auto& kv : o) {
        const std::string key(kv.key());
        bool known = false;
        for (auto expected : required) known |= key == expected;
        if (!known && !(metadata && key.rfind("metadata:", 0) == 0 && namespaced(key.substr(9))))
            fail("MANIFEST_SCHEMA", "Unknown field " + key);
    }
}
std::string string(const json::value& v) {
    if (!v.is_string()) fail("MANIFEST_SCHEMA", "Expected string");
    std::string s(v.as_string());
    if (s.size() >= NECT_EXT_STRING_LIMIT || !utf8(s)) fail("MANIFEST_SCHEMA", "Invalid bounded UTF-8 string");
    return s;
}
unsigned integer(const json::value& v) {
    uint64_t n = 0;
    if (v.is_uint64()) n = v.as_uint64();
    else if (v.is_int64() && v.as_int64() >= 0) n = static_cast<uint64_t>(v.as_int64());
    else fail("MANIFEST_SCHEMA", "Expected nonnegative integer");
    if (n > UINT32_MAX) fail("MANIFEST_SCHEMA", "Integer out of range");
    return static_cast<unsigned>(n);
}
double number(const json::value& v) {
    if (!v.is_number()) fail("MANIFEST_SCHEMA", "Expected finite number");
    const double n = v.to_number<double>();
    if (!std::isfinite(n)) fail("MANIFEST_SCHEMA", "Expected finite number");
    return n;
}
const json::array& one(const json::value& v) {
    if (!v.is_array() || v.as_array().size() != 1) fail("MANIFEST_SCHEMA", "C2A requires exactly one item");
    return v.as_array();
}
void equal(const json::value& v, const char* expected) {
    if (string(v) != expected) fail("MANIFEST_SCHEMA", std::string("C2A requires ") + expected);
}
bool hash_string(const std::string& s) {
    return s.size() == 64 && std::all_of(s.begin(), s.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
}

// Boost.JSON accepts duplicate keys. After syntax validation, walk only tokens
// to reject ambiguous members (including escaped spellings) at every level.
class UniqueKeys {
public:
    explicit UniqueKeys(const std::string& s) : s_(s) { value(); }
private:
    const std::string& s_; size_t p_ = 0;
    void space() { while (p_ < s_.size() && (s_[p_] == ' ' || s_[p_] == '\t' || s_[p_] == '\r' || s_[p_] == '\n')) ++p_; }
    std::string quoted() {
        const size_t begin = p_++;
        while (s_[p_] != '"') { if (s_[p_] == '\\') ++p_; ++p_; }
        ++p_;
        return std::string(json::parse(s_.substr(begin, p_ - begin)).as_string());
    }
    void value() {
        space();
        if (s_[p_] == '{') {
            ++p_; space(); std::set<std::string> keys;
            if (s_[p_] != '}') for (;;) {
                const auto key = quoted();
                if (!keys.insert(key).second) fail("MANIFEST_SCHEMA", "Duplicate JSON member " + key);
                space(); ++p_; value(); space();
                if (s_[p_] != ',') break;
                ++p_; space();
            }
            ++p_;
        } else if (s_[p_] == '[') {
            ++p_; space();
            if (s_[p_] != ']') for (;;) { value(); space(); if (s_[p_] != ',') break; ++p_; }
            ++p_;
        } else if (s_[p_] == '"') quoted();
        else { while (p_ < s_.size() && s_[p_] != ',' && s_[p_] != '}' && s_[p_] != ']' && s_[p_] != ' ' && s_[p_] != '\n' && s_[p_] != '\r' && s_[p_] != '\t') ++p_; }
    }
};
struct File {
    HANDLE handle = INVALID_HANDLE_VALUE;
    explicit File(const fs::path& path, bool directory = false) {
        // Deny writes/rename while checking hash and loading; never a writable handle.
        handle = CreateFileW(path.c_str(), directory ? 0 : GENERIC_READ, FILE_SHARE_READ,
            nullptr, OPEN_EXISTING, directory ? FILE_FLAG_BACKUP_SEMANTICS : FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE) fail("PACKAGE_PATH", "Cannot open package path");
    }
    ~File() { if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle); }
    File(const File&) = delete;
    File& operator=(const File&) = delete;
    fs::path final_path() const {
        std::wstring s(32768, L'\0');
        const DWORD n = GetFinalPathNameByHandleW(handle, s.data(), static_cast<DWORD>(s.size()), FILE_NAME_NORMALIZED);
        if (!n || n >= s.size()) fail("PACKAGE_PATH", "Cannot resolve final path");
        s.resize(n); return fs::path(s);
    }
};
bool same_path(const fs::path& a, const fs::path& b) { return _wcsicmp(a.c_str(), b.c_str()) == 0; }
bool inside(const fs::path& root, const fs::path& path) {
    auto p = path.begin();
    for (const auto& r : root) { if (p == path.end() || !same_path(r, *p)) return false; ++p; }
    return p != path.end();
}
std::string sha256(File& file) {
    LARGE_INTEGER zero{};
    if (!SetFilePointerEx(file.handle, zero, nullptr, FILE_BEGIN)) fail("PACKAGE_SHA", "Cannot rewind binary");
    BCRYPT_ALG_HANDLE algorithm = nullptr; BCRYPT_HASH_HANDLE hash = nullptr;
    struct Cleanup { BCRYPT_ALG_HANDLE& a; BCRYPT_HASH_HANDLE& h; ~Cleanup() { if (h) BCryptDestroyHash(h); if (a) BCryptCloseAlgorithmProvider(a, 0); } } cleanup{algorithm, hash};
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0 ||
        BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) < 0) fail("PACKAGE_SHA", "SHA-256 unavailable");
    std::array<unsigned char, 65536> chunk{};
    for (;;) {
        DWORD n = 0;
        if (!ReadFile(file.handle, chunk.data(), static_cast<DWORD>(chunk.size()), &n, nullptr)) fail("PACKAGE_SHA", "Cannot hash binary");
        if (!n) break;
        if (BCryptHashData(hash, chunk.data(), n, 0) < 0) fail("PACKAGE_SHA", "Cannot hash binary");
    }
    std::array<unsigned char, 32> digest{};
    if (BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) < 0) fail("PACKAGE_SHA", "Cannot finish hash");
    std::ostringstream s; s << std::hex << std::setfill('0');
    for (auto byte : digest) s << std::setw(2) << static_cast<unsigned>(byte);
    return s.str();
}
std::string read_manifest(const fs::path& path) {
    if (fs::file_size(path) > 65536) fail("MANIFEST_SCHEMA", "Manifest exceeds 64 KiB");
    std::ifstream file(path, std::ios::binary);
    if (!file) fail("MANIFEST_SCHEMA", "Cannot read manifest");
    std::string s((std::istreambuf_iterator<char>(file)), {});
    if (s.size() > 65536) fail("MANIFEST_SCHEMA", "Manifest exceeds 64 KiB");
    return s;
}
Package parse_manifest(const fs::path& path) {
    Package p; p.manifest_path = fs::canonical(path); p.root = fs::canonical(path.parent_path());
    File root(p.root, true), manifest(path);
    if (!inside(root.final_path(), manifest.final_path())) fail("PACKAGE_PATH", "Manifest escapes package root");
    const auto bytes = read_manifest(path);
    json::value parsed;
    try { parsed = json::parse(bytes); } catch (const std::exception&) { fail("MANIFEST_SCHEMA", "Invalid JSON"); }
    UniqueKeys unique(bytes);
    const auto& o = object(parsed);
    fields(o, {"schema", "package_id", "package_version", "extension_api", "binaries", "types"}, true);
    equal(o.at("schema"), "nect.extension-package/1");
    p.package_id = string(o.at("package_id")); p.package_version = string(o.at("package_version"));
    if (!namespaced(p.package_id)) fail("MANIFEST_SCHEMA", "Package ID must be namespaced");
    const auto& api = object(o.at("extension_api")); fields(api, {"major", "min_minor", "max_minor"});
    if (integer(api.at("major")) != 1 || integer(api.at("min_minor")) != 0 || integer(api.at("max_minor")) != 0)
        fail("EXTENSION_API", "Only extension API 1.0 is qualified");
    const auto& binary = object(one(o.at("binaries"))[0]); fields(binary, {"os", "arch", "path", "sha256"});
    equal(binary.at("os"), "windows"); equal(binary.at("arch"), "x86_64");
    const auto relative = fs::u8path(string(binary.at("path")));
    if (relative.is_absolute() || relative.has_root_name() || relative.has_root_directory()) fail("PACKAGE_PATH", "Binary must be relative");
    for (const auto& part : relative) if (part == ".." || part.native().find(L':') != std::wstring::npos) fail("PACKAGE_PATH", "Traversal or stream path");
    p.binary_path = fs::canonical(p.root / relative);
    File file(p.binary_path);
    if (!inside(root.final_path(), file.final_path())) fail("PACKAGE_PATH", "Binary escapes package root");
    p.sha256 = string(binary.at("sha256"));
    if (!hash_string(p.sha256)) fail("MANIFEST_SCHEMA", "Expected lowercase SHA-256");
    if (sha256(file) != p.sha256) fail("PACKAGE_SHA", "Binary SHA-256 mismatch");
    const auto& type = object(one(o.at("types"))[0]);
    fields(type, {"type_id", "behavior_version", "kind", "label", "target_scope", "input_domain", "output_domain", "parameters", "exports"});
    p.type.type_id = string(type.at("type_id")); p.type.label = string(type.at("label"));
    p.type.behavior_version = integer(type.at("behavior_version"));
    if (!namespaced(p.type.type_id) || p.type.behavior_version != 1) fail("MANIFEST_SCHEMA", "C2A requires namespaced type BehaviorVersion 1");
    equal(type.at("kind"), "test_internal"); equal(type.at("target_scope"), "object");
    equal(type.at("input_domain"), "scalar_test"); equal(type.at("output_domain"), "scalar_test");
    const auto& parameter = object(one(type.at("parameters"))[0]);
    fields(parameter, {"key", "type", "unit", "default", "min", "max"});
    equal(parameter.at("key"), "factor"); equal(parameter.at("type"), "number"); equal(parameter.at("unit"), "unitless");
    p.type.default_factor = number(parameter.at("default")); p.type.minimum_factor = number(parameter.at("min")); p.type.maximum_factor = number(parameter.at("max"));
    if (p.type.default_factor != 1 || p.type.minimum_factor != 0 || p.type.maximum_factor != 4) fail("MANIFEST_SCHEMA", "C2A requires factor default 1, range [0,4]");
    const auto& exports = object(type.at("exports")); fields(exports, {"native", "svg"});
    if (!exports.at("native").is_bool() || !exports.at("native").as_bool()) fail("MANIFEST_SCHEMA", "C2A native export policy must be true");
    equal(exports.at("svg"), "unsupported");
    return p;
}

// One process-wide gate also serializes separate harnesses mapping the same DLL.
std::recursive_mutex callbacks;
thread_local bool in_callback = false;
struct CallbackScope {
    std::unique_lock<std::recursive_mutex> lock{callbacks};
    CallbackScope() { if (in_callback) fail("EXTENSION_REENTRANT", "C2A callback re-entry refused"); in_callback = true; }
    ~CallbackScope() { in_callback = false; }
};
struct LoaderErrorMode {
    DWORD previous = 0;
    LoaderErrorMode() {
        // LoadLibrary can otherwise wait on an OS loader error dialog for a
        // bad image. Scope this to the calling thread and restore it exactly.
        if (!SetThreadErrorMode(GetThreadErrorMode() | SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX, &previous))
            fail("EXTENSION_LOAD", "Cannot set bounded loader error reporting");
    }
    ~LoaderErrorMode() { SetThreadErrorMode(previous, nullptr); }
};
template<class F> auto call(F&& f) -> decltype(f()) {
    CallbackScope scope;
    try { return f(); } catch (const Error&) { throw; }
    catch (...) { fail("EXTENSION_EXCEPTION", "Provider violated no-exception ABI contract"); }
}
template<class T> void prefix(const T* v) {
    if (!v || v->struct_size < sizeof(T)) fail("EXTENSION_STRUCT_SIZE", "Undersized required v1 prefix");
    if (v->api_major != 1 || v->api_minor != 0) fail("EXTENSION_API", "Binary API prefix mismatch");
}
std::string copied(const char* s) {
    if (!s) fail("EXTENSION_DESCRIPTOR", "Missing UTF-8 descriptor string");
    size_t size = 0;
    while (size < NECT_EXT_STRING_LIMIT && s[size]) ++size;
    if (size == NECT_EXT_STRING_LIMIT) fail("EXTENSION_DESCRIPTOR", "Unbounded descriptor string");
    std::string value(s, size);
    if (!utf8(value)) fail("EXTENSION_DESCRIPTOR", "Invalid descriptor UTF-8");
    return value;
}
void agrees(const char* binary, const std::string& expected) {
    if (copied(binary) != expected) fail("EXTENSION_DESCRIPTOR", "Manifest/binary descriptor mismatch");
}
void status(NectExtStatusV1 s) { if (s != NECT_EXT_OK) fail("EXTENSION_STATUS", "Provider returned status " + std::to_string(static_cast<int>(s))); }
} // namespace

Error::Error(std::string c, std::string message) : std::runtime_error(std::move(message)), code(std::move(c)) {}
Snapshot Snapshot::discover(const std::vector<fs::path>& roots) {
    try {
        std::vector<fs::path> manifests;
        for (const auto& explicit_root : roots) {
            const auto root = fs::canonical(explicit_root);
            if (!fs::is_directory(root)) fail("PACKAGE_PATH", "Discovery root must be directory");
            if (fs::exists(root / "extension.json")) manifests.push_back(root / "extension.json");
            else for (const auto& entry : fs::directory_iterator(root)) {
                if (entry.is_directory() && fs::exists(entry.path() / "extension.json")) {
                    File parent(root, true), child(entry.path(), true);
                    if (!inside(parent.final_path(), child.final_path())) fail("PACKAGE_PATH", "Package directory escapes discovery root");
                    manifests.push_back(entry.path() / "extension.json");
                }
            }
        }
        std::sort(manifests.begin(), manifests.end());
        Snapshot s; std::set<std::string> packages, types;
        for (const auto& manifest : manifests) {
            auto p = parse_manifest(manifest);
            // No multi-version first-wins resolution in the frozen C2A snapshot.
            if (!packages.insert(p.package_id).second || !types.insert(p.type.type_id).second)
                fail("EXTENSION_DUPLICATE", "Duplicate package/type identity or version");
            s.packages_.push_back(std::move(p));
        }
        return s;
    } catch (const fs::filesystem_error&) { fail("PACKAGE_PATH", "Missing or invalid package path"); }
}

struct LoadedState {
    Package package;
    std::unique_ptr<File> binary_lock;
    HMODULE module = nullptr;
    NectExtApiV1 api{};
    NectExtScalarTestEvaluateV1 evaluate = nullptr;
    bool destroy_failed = false;
    ~LoadedState() {
        // Last shared owner is gone only after every instance destroy completed.
        std::lock_guard<std::recursive_mutex> lock(callbacks);
        if (destroy_failed) {
            // A throwing destroy may have freed the handle or left it live.
            // Never retry/double-free or unload code behind an uncertain handle.
            // Keep the module and its read-only file lock pinned to process exit.
            binary_lock.release();
        } else if (module) FreeLibrary(module);
    }
};
DeveloperHarness::DeveloperHarness(std::shared_ptr<LoadedState> state) : state_(std::move(state)) {}
DeveloperHarness DeveloperHarness::load_owned_test(const Package& package, const OwnedTestTrust& trust) {
    try {
        if (trust.os != "windows" || trust.arch != "x86_64" || trust.package_id != package.package_id ||
            trust.package_version != package.package_version || trust.sha256 != package.sha256 ||
            !same_path(fs::canonical(trust.package_root), package.root)) fail("EXTENSION_UNTRUSTED", "Explicit exact owned-test trust required");
        // Revalidate manifest and locked binary at the explicit execution boundary.
        const auto current = parse_manifest(package.manifest_path);
        if (current.package_id != package.package_id || current.package_version != package.package_version ||
            current.sha256 != package.sha256 || !same_path(current.binary_path, package.binary_path) ||
            current.type.type_id != package.type.type_id || current.type.label != package.type.label ||
            current.type.behavior_version != package.type.behavior_version)
            fail("EXTENSION_UNTRUSTED", "Snapshot identity changed; rediscover and approve exact identity");
        auto s = std::make_shared<LoadedState>(); s->package = current;
        s->binary_lock = std::make_unique<File>(current.binary_path);
        File root(current.root, true);
        if (!inside(root.final_path(), s->binary_lock->final_path())) fail("PACKAGE_PATH", "Binary escaped before load");
        if (sha256(*s->binary_lock) != trust.sha256) fail("PACKAGE_SHA", "Binary changed before load");
        CallbackScope load_scope;
        DWORD load_error = 0;
        {
            LoaderErrorMode error_mode;
            s->module = LoadLibraryExW(s->binary_lock->final_path().c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
            if (!s->module) load_error = GetLastError();
        }
        if (!s->module) fail("EXTENSION_LOAD", "Cannot load verified owned test DLL; Win32 error " + std::to_string(load_error));
        const auto entry = reinterpret_cast<NectExtGetApiV1>(GetProcAddress(s->module, "nect_extension_get_api_v1"));
        if (!entry) fail("EXTENSION_CALLBACK", "Missing C entrypoint nect_extension_get_api_v1");
        const NectExtHostV1 host{sizeof(NectExtHostV1), 1, 0, NECT_EXT_SCALAR_TEST_CAPABILITY};
        s->api.struct_size = sizeof(NectExtApiV1); s->api.api_major = 1; s->api.api_minor = 0;
        // Entry/descriptor are already inside the global callback scope.
        try {
            status(entry(&host, &s->api)); prefix(&s->api);
            agrees(s->api.package_id, current.package_id); agrees(s->api.package_version, current.package_version);
            if (s->api.type_count != 1) fail("EXTENSION_DESCRIPTOR", "Expected one test_internal type");
            if (!s->api.type_descriptor || !s->api.create_instance || !s->api.destroy_instance) fail("EXTENSION_CALLBACK", "Missing lifecycle callback");
            const auto* d = s->api.type_descriptor(0); prefix(d);
            agrees(d->type_id, current.type.type_id); agrees(d->label, current.type.label);
            agrees(d->kind, "test_internal"); agrees(d->target_scope, "object");
            agrees(d->input_domain, "scalar_test"); agrees(d->output_domain, "scalar_test");
            if (d->behavior_version != current.type.behavior_version || d->parameter_count != 1 || d->native_export != 1)
                fail("EXTENSION_DESCRIPTOR", "Descriptor version/count/export mismatch");
            agrees(d->svg_export, "unsupported"); prefix(d->parameters);
            const auto& p = *d->parameters;
            agrees(p.key, "factor"); agrees(p.type, "number"); agrees(p.unit, "unitless");
            if (p.default_value != 1 || p.minimum != 0 || p.maximum != 4) fail("EXTENSION_DESCRIPTOR", "Parameter schema mismatch");
            if (!d->scalar_test_evaluate) fail("EXTENSION_CALLBACK", "Missing scalar_test callback");
            s->evaluate = d->scalar_test_evaluate;
        } catch (const Error&) { throw; }
        catch (...) { fail("EXTENSION_EXCEPTION", "Provider violated no-exception ABI contract"); }
        return DeveloperHarness(std::move(s));
    } catch (const fs::filesystem_error&) { fail("PACKAGE_PATH", "Missing or invalid execution path"); }
}
const Package& DeveloperHarness::package() const {
    if (!state_) fail("EXTENSION_LIFETIME", "Moved-from developer harness");
    return state_->package;
}
ScalarInstance DeveloperHarness::create_scalar_instance() const {
    if (!state_) fail("EXTENSION_LIFETIME", "Moved-from developer harness");
    auto s = state_;
    NectExtInstanceV1* instance = nullptr;
    call([&] {
        auto cleanup = [&] {
            if (!instance) return;
            auto* owned = std::exchange(instance, nullptr);
            try { s->api.destroy_instance(owned); }
            catch (...) { s->destroy_failed = true; throw; }
        };
        NectExtStatusV1 result;
        try { result = s->api.create_instance(s->package.type.type_id.c_str(), s->package.type.behavior_version, &instance); }
        catch (...) { cleanup(); throw; }
        if (result != NECT_EXT_OK) { cleanup(); status(result); }
        if (!instance) fail("EXTENSION_CALLBACK", "Create returned null instance on OK");
    });
    return ScalarInstance(std::move(s), instance);
}
ScalarInstance::ScalarInstance(std::shared_ptr<LoadedState> state, void* instance) : state_(std::move(state)), instance_(instance) {}
ScalarInstance::ScalarInstance(ScalarInstance&& other) noexcept : state_(std::move(other.state_)), instance_(std::exchange(other.instance_, nullptr)) {}
ScalarInstance& ScalarInstance::operator=(ScalarInstance&& other) noexcept {
    if (this != &other) { reset(); state_ = std::move(other.state_); instance_ = std::exchange(other.instance_, nullptr); }
    return *this;
}
void ScalarInstance::reset() noexcept {
    try { close(); } catch (...) {} // explicit close is the reporting surface
}
ScalarInstance::~ScalarInstance() { reset(); }
void ScalarInstance::close() {
    call([&] {
        if (instance_) {
            auto s = state_;
            auto* instance = static_cast<NectExtInstanceV1*>(std::exchange(instance_, nullptr));
            try { s->api.destroy_instance(instance); }
            catch (...) { s->destroy_failed = true; state_.reset(); throw; }
        }
        state_.reset();
    });
}
double ScalarInstance::evaluate(double value, double factor) {
    return call([&] {
        if (!state_ || !instance_) fail("EXTENSION_LIFETIME", "No live scalar instance");
        if (!std::isfinite(value) || !std::isfinite(factor) || factor < 0 || factor > 4) fail("EXTENSION_PARAMETER", "Expected finite scalar and factor in [0,4]");
        const NectExtScalarTestInputV1 input{sizeof(NectExtScalarTestInputV1), 1, 0, value, factor};
        NectExtScalarTestOutputV1 output{sizeof(NectExtScalarTestOutputV1), 1, 0, 0};
        status(state_->evaluate(static_cast<NectExtInstanceV1*>(instance_), &input, &output));
        prefix(&output);
        if (!std::isfinite(output.value)) fail("EXTENSION_STATUS", "Provider returned nonfinite scalar");
        return output.value;
    });
}
} // namespace nect::extension

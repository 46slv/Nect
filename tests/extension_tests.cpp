#include "nect/extension_registry.hpp"
#include "nect/extension_abi.h"
#include "extension_test_package/fixture_stats.h"
#include <boost/json.hpp>
#include <array>
#include <atomic>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <thread>
#define NOMINMAX
#include <windows.h>
#include <winioctl.h>
#include <bcrypt.h>

namespace {
using namespace nect::extension;
namespace fs = std::filesystem;
namespace json = boost::json;
unsigned checks = 0;
void check(bool ok, const std::string& why) { if (!ok) throw std::runtime_error(why); ++checks; }
template<class F> void rejects(const char* code, F&& f) {
    try { f(); } catch (const Error& e) { check(e.code == code, std::string("Expected ") + code + ", got " + e.code + ": " + e.what()); return; }
    throw std::runtime_error(std::string("Expected ") + code);
}
std::string read(const fs::path& path) { std::ifstream file(path, std::ios::binary); return {(std::istreambuf_iterator<char>(file)), {}}; }
void write(const fs::path& path, const std::string& s) { std::ofstream file(path, std::ios::binary); file << s; if (!file) throw std::runtime_error("fixture write failed"); }
std::string hash(const fs::path& path) {
    const auto data = read(path); BCRYPT_ALG_HANDLE alg = nullptr; std::array<unsigned char, 32> digest{};
    check(BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0, "test hash provider");
    const auto result = BCryptHash(alg, nullptr, 0, reinterpret_cast<PUCHAR>(const_cast<char*>(data.data())), static_cast<ULONG>(data.size()), digest.data(), static_cast<ULONG>(digest.size()));
    BCryptCloseAlgorithmProvider(alg, 0); check(result >= 0, "test one-shot hash");
    std::ostringstream out; out << std::hex << std::setfill('0'); for (auto b : digest) out << std::setw(2) << static_cast<unsigned>(b); return out.str();
}
struct Fixtures {
    fs::path binaries, scratch;
    std::string template_text;
    fs::path make(const std::string& name, unsigned mode = 0) const {
        const auto root = scratch / name;
        fs::create_directories(root / "bin");
        fs::copy_file(binaries / ("nect_testop_" + std::to_string(mode) + ".dll"), root / "bin/nect_testop.dll");
        auto text = template_text;
        text.replace(text.find("@BINARY_SHA256@"), 15, hash(root / "bin/nect_testop.dll"));
        write(root / "extension.json", text); return root;
    }
};
template<class F> void mutate(const fs::path& root, F&& f) {
    auto v = json::parse(read(root / "extension.json")); f(v.as_object()); write(root / "extension.json", json::serialize(v));
}
Package package(const fs::path& root) { return Snapshot::discover({root}).packages().at(0); }
OwnedTestTrust trust(const Package& p) { return {p.root, p.package_id, p.package_version, p.sha256}; }
bool loaded(const fs::path& binary) { return GetModuleHandleW(binary.c_str()) != nullptr; }
NectFixtureStats stats(const fs::path& binary) {
    const auto module = GetModuleHandleW(binary.c_str()); check(module != nullptr, "stats require live module");
    const auto f = reinterpret_cast<void (NECT_EXT_CALL *)(NectFixtureStats*)>(GetProcAddress(module, "nect_fixture_stats"));
    check(f != nullptr, "owned instrumentation export"); NectFixtureStats result{}; result.struct_size = sizeof(result); f(&result); return result;
}
// An NTFS directory junction needs no privilege change. It exists solely in this
// newly-created qualification scratch; its target is also owned test scratch.
void junction(const fs::path& link, const fs::path& target) {
    fs::create_directory(link);
    const auto absolute = fs::absolute(target).wstring(); const std::wstring substitute = L"\\??\\" + absolute;
    struct Header { DWORD tag; WORD length, reserved, substitute_offset, substitute_length, print_offset, print_length; };
    const size_t path_bytes = (substitute.size() + absolute.size() + 2) * sizeof(wchar_t);
    std::vector<unsigned char> buffer(sizeof(Header) + path_bytes, 0);
    auto* h = reinterpret_cast<Header*>(buffer.data()); h->tag = IO_REPARSE_TAG_MOUNT_POINT;
    h->length = static_cast<WORD>(8 + path_bytes); h->substitute_length = static_cast<WORD>(substitute.size() * 2);
    h->print_offset = static_cast<WORD>((substitute.size() + 1) * 2); h->print_length = static_cast<WORD>(absolute.size() * 2);
    auto* chars = reinterpret_cast<wchar_t*>(buffer.data() + sizeof(Header));
    std::copy(substitute.begin(), substitute.end(), chars); std::copy(absolute.begin(), absolute.end(), chars + substitute.size() + 1);
    HANDLE file = CreateFileW(link.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (file == INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot create test junction handle");
    DWORD n = 0; const bool ok = DeviceIoControl(file, FSCTL_SET_REPARSE_POINT, buffer.data(), static_cast<DWORD>(buffer.size()), nullptr, 0, &n, nullptr) != 0;
    CloseHandle(file); check(ok, "test junction created");
}
struct ShortPrefix {
    void* allocation;
    uint32_t* size;
    ShortPrefix() {
        SYSTEM_INFO info{}; GetSystemInfo(&info);
        allocation = VirtualAlloc(nullptr, info.dwPageSize * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if (!allocation) throw std::runtime_error("Guard allocation failed");
        DWORD before = 0;
        if (!VirtualProtect(static_cast<char*>(allocation) + info.dwPageSize, info.dwPageSize, PAGE_NOACCESS, &before)) throw std::runtime_error("Guard protect failed");
        size = reinterpret_cast<uint32_t*>(static_cast<char*>(allocation) + info.dwPageSize - sizeof(uint32_t)); *size = 4;
    }
    ~ShortPrefix() { VirtualFree(allocation, 0, MEM_RELEASE); }
};

void discovery(const Fixtures& fixtures) {
    const auto root = fixtures.make("discovery"); const auto binary = root / "bin/nect_testop.dll";
    check(!loaded(binary), "not loaded initially");
    for (unsigned i = 0; i < 3; ++i) {
        const auto s = Snapshot::discover({root}); check(s.packages().size() == 1, "manifest-only type visible");
        check(s.packages()[0].type.label == "Test Multiply 倍", "UTF-8 manifest copied");
        check(!loaded(binary), "inspection never loads code");
    }
    const auto fake = fixtures.make("non-library"); write(fake / "bin/nect_testop.dll", "not executable native code");
    mutate(fake, [&](json::object& o) { o["binaries"].as_array()[0].as_object()["sha256"] = hash(fake / "bin/nect_testop.dll"); });
    const auto p = package(fake); check(!loaded(p.binary_path), "even a verified non-library is only inspected");
    const DWORD thread_mode = GetThreadErrorMode();
    rejects("EXTENSION_LOAD", [&] { DeveloperHarness::load_owned_test(p, trust(p)); });
    check(GetThreadErrorMode() == thread_mode, "bad image load restores calling thread error mode");
    const auto order = fixtures.scratch / "ordered"; fs::create_directory(order);
    auto b = fixtures.make("ordered/b"), a = fixtures.make("ordered/a");
    mutate(b, [](json::object& o) { o["package_id"] = "org.example.other"; o["types"].as_array()[0].as_object()["type_id"] = "org.example.other.multiply"; });
    const auto s = Snapshot::discover({order});
    check(s.packages()[0].root.filename() == "a" && s.packages()[1].root.filename() == "b", "deterministic path order");
    const auto duplicate = fixtures.make("duplicate");
    rejects("EXTENSION_DUPLICATE", [&] { Snapshot::discover({root, duplicate}); });
    mutate(duplicate, [](json::object& o) { o["package_id"] = "org.example.other"; });
    rejects("EXTENSION_DUPLICATE", [&] { Snapshot::discover({root, duplicate}); });
    mutate(duplicate, [](json::object& o) { o["package_id"] = "org.example.nect.testop"; o["package_version"] = "2.0.0"; o["types"].as_array()[0].as_object()["type_id"] = "org.example.other.multiply"; });
    rejects("EXTENSION_DUPLICATE", [&] { Snapshot::discover({root, duplicate}); });
    rejects("EXTENSION_DUPLICATE", [&] { Snapshot::discover({root, root}); });
}
void manifest_negatives(const Fixtures& fixtures) {
    const auto root = fixtures.make("schema"); const auto original = read(root / "extension.json");
    auto bad = [&](const char* code, auto f) { write(root / "extension.json", original); mutate(root, f); rejects(code, [&] { Snapshot::discover({root}); }); check(!loaded(root / "bin/nect_testop.dll"), "bad manifest never loads"); };
    bad("EXTENSION_API", [](json::object& o) { o["extension_api"].as_object()["major"] = 2; });
    bad("EXTENSION_API", [](json::object& o) { o["extension_api"].as_object()["min_minor"] = 1; });
    bad("EXTENSION_API", [](json::object& o) { o["extension_api"].as_object()["max_minor"] = 1; });
    bad("MANIFEST_SCHEMA", [](json::object& o) { o["typo"] = true; });
    bad("MANIFEST_SCHEMA", [](json::object& o) { o["metadata"] = true; });
    bad("MANIFEST_SCHEMA", [](json::object& o) { o["metadata:short"] = true; });
    bad("MANIFEST_SCHEMA", [](json::object& o) { o.erase("types"); });
    bad("MANIFEST_SCHEMA", [](json::object& o) { o["package_id"] = "label"; });
    bad("MANIFEST_SCHEMA", [](json::object& o) { o["binaries"].as_array()[0].as_object()["arch"] = "arm64"; });
    bad("MANIFEST_SCHEMA", [](json::object& o) { o["binaries"].as_array()[0].as_object()["sha256"] = "bad"; });
    bad("PACKAGE_SHA", [](json::object& o) { o["binaries"].as_array()[0].as_object()["sha256"] = std::string(64, '0'); });
    bad("PACKAGE_PATH", [](json::object& o) { o["binaries"].as_array()[0].as_object()["path"] = "../outside.dll"; });
    bad("PACKAGE_PATH", [](json::object& o) { o["binaries"].as_array()[0].as_object()["path"] = "C:/outside.dll"; });
    bad("PACKAGE_PATH", [](json::object& o) { o["binaries"].as_array()[0].as_object()["path"] = "bin/nect_testop.dll:stream"; });
    bad("MANIFEST_SCHEMA", [](json::object& o) { o["types"].as_array()[0].as_object()["kind"] = "effect"; });
    bad("MANIFEST_SCHEMA", [](json::object& o) { o["types"].as_array()[0].as_object()["input_domain"] = "Document"; });
    bad("MANIFEST_SCHEMA", [](json::object& o) { o["types"].as_array()[0].as_object()["behavior_version"] = 2; });
    bad("MANIFEST_SCHEMA", [](json::object& o) { o["types"].as_array()[0].as_object()["parameters"].as_array()[0].as_object()["max"] = 5; });
    bad("MANIFEST_SCHEMA", [](json::object& o) { o["types"].as_array()[0].as_object()["exports"].as_object()["svg"] = "supported"; });
    write(root / "extension.json", original);
    mutate(root, [](json::object& o) { o["metadata:org.example.note"] = json::object{{"optional", true}}; });
    check(package(root).package_id == "org.example.nect.testop", "explicit optional namespaced metadata allowed");
    auto duplicate = original; duplicate.insert(duplicate.find('{') + 1, "\"schema\":\"nect.extension-package/1\",");
    write(root / "extension.json", duplicate); rejects("MANIFEST_SCHEMA", [&] { package(root); });
    duplicate = original; duplicate.insert(duplicate.find('{') + 1, "\"sch\\u0065ma\":\"nect.extension-package/1\",");
    write(root / "extension.json", duplicate); rejects("MANIFEST_SCHEMA", [&] { package(root); });
    write(root / "extension.json", "{"); rejects("MANIFEST_SCHEMA", [&] { package(root); });
    write(root / "extension.json", std::string(65537, ' ')); rejects("MANIFEST_SCHEMA", [&] { package(root); });

    const auto escape = fixtures.make("junction-escape"), outside = fixtures.make("outside");
    junction(escape / "link", outside / "bin");
    mutate(escape, [](json::object& o) { o["binaries"].as_array()[0].as_object()["path"] = "link/nect_testop.dll"; });
    rejects("PACKAGE_PATH", [&] { package(escape); });
    const auto parent = fixtures.scratch / "root-escape"; fs::create_directory(parent); junction(parent / "package", outside);
    rejects("PACKAGE_PATH", [&] { Snapshot::discover({parent}); });
}
void trust_and_descriptors(const Fixtures& fixtures) {
    const auto root = fixtures.make("trust"); const auto p = package(root); const auto exact = trust(p);
    for (unsigned field = 0; field < 6; ++field) {
        auto t = exact;
        if (field == 0) t.package_id += ".other";
        if (field == 1) t.package_version = "2.0.0";
        if (field == 2) t.sha256[0] = t.sha256[0] == '0' ? '1' : '0';
        if (field == 3) t.os = "linux";
        if (field == 4) t.arch = "arm64";
        if (field == 5) t.package_root = fixtures.scratch;
        rejects("EXTENSION_UNTRUSTED", [&] { DeveloperHarness::load_owned_test(p, t); });
        check(!loaded(p.binary_path), "untrusted identity never loads");
    }
    const auto original = read(root / "extension.json");
    mutate(root, [](json::object& o) { o["package_version"] = "2.0.0"; });
    const auto changed = package(root);
    rejects("EXTENSION_UNTRUSTED", [&] { DeveloperHarness::load_owned_test(changed, exact); });
    rejects("EXTENSION_UNTRUSTED", [&] { DeveloperHarness::load_owned_test(p, exact); });
    write(root / "extension.json", original);
    fs::copy_file(fixtures.binaries / "nect_testop_14.dll", p.binary_path, fs::copy_options::overwrite_existing);
    rejects("PACKAGE_SHA", [&] { DeveloperHarness::load_owned_test(p, exact); });
    mutate(root, [&](json::object& o) { o["binaries"].as_array()[0].as_object()["sha256"] = hash(p.binary_path); });
    const auto new_hash = package(root);
    rejects("EXTENSION_UNTRUSTED", [&] { DeveloperHarness::load_owned_test(new_hash, exact); });
    check(!loaded(p.binary_path), "changed hash inspection stays non-executing");
    const std::array<const char*, 21> errors{{nullptr, "EXTENSION_API", "EXTENSION_DESCRIPTOR", "EXTENSION_CALLBACK", "EXTENSION_STATUS", nullptr,
        "EXTENSION_STRUCT_SIZE", "EXTENSION_STRUCT_SIZE", "EXTENSION_STRUCT_SIZE", "EXTENSION_CALLBACK", "EXTENSION_CALLBACK", nullptr, nullptr, nullptr, nullptr,
        nullptr, "EXTENSION_DESCRIPTOR", "EXTENSION_DESCRIPTOR", "EXTENSION_CALLBACK", nullptr, "EXTENSION_CALLBACK"}};
    for (unsigned mode = 1; mode < errors.size(); ++mode) {
        const auto test = package(fixtures.make("variant-" + std::to_string(mode), mode));
        if (errors[mode]) {
            rejects(errors[mode], [&] { DeveloperHarness::load_owned_test(test, trust(test)); });
            check(!loaded(test.binary_path), "failed registration releases library");
        } else {
            auto harness = DeveloperHarness::load_owned_test(test, trust(test));
            if (mode == 11 || mode == 12) {
                rejects(mode == 11 ? "EXTENSION_CALLBACK" : "EXTENSION_STATUS", [&] { harness.create_scalar_instance(); });
                check(stats(test.binary_path).live == 0, "failed create frees matching provider allocation");
            } else {
                auto instance = harness.create_scalar_instance();
                if (mode == 19) {
                    rejects("EXTENSION_EXCEPTION", [&] { instance.close(); });
                    rejects("EXTENSION_LIFETIME", [&] { instance.evaluate(3, 2); });
                    check(stats(test.binary_path).live == 1, "uncertain destroy is never retried");
                } else if (mode == 14) check(instance.evaluate(3, 2) == 6, "larger advertised descriptor prefixes accepted");
                else rejects(mode == 13 ? "EXTENSION_EXCEPTION" : "EXTENSION_STATUS", [&] { instance.evaluate(3, 2); });
            }
        }
        if (mode == 19) check(loaded(test.binary_path), "broken destroy pins owned DLL to process exit instead of unsafe unload");
        else check(!loaded(test.binary_path), "variant unloads after last ABI reference");
    }
}
void lifecycle(const Fixtures& fixtures) {
    const auto p = package(fixtures.make("lifecycle")); const Package retained = p;
    std::optional<ScalarInstance> survivor;
    {
        auto harness = DeveloperHarness::load_owned_test(p, trust(p));
        check(loaded(p.binary_path), "explicit load is observable");
        check(stats(p.binary_path).entries == 1, "one bounded entry query");
        survivor.emplace(harness.create_scalar_instance());
        check(survivor->evaluate(3, 2) == 6 && survivor->evaluate(-2, 4) == -8 && survivor->evaluate(10, 0) == 0, "deterministic bounded multiply");
        const auto before = stats(p.binary_path).evaluations;
        for (const auto factor : {-1.0, 4.01, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()})
            rejects("EXTENSION_PARAMETER", [&] { survivor->evaluate(1, factor); });
        rejects("EXTENSION_PARAMETER", [&] { survivor->evaluate(std::numeric_limits<double>::quiet_NaN(), 1); });
        check(stats(p.binary_path).evaluations == before, "invalid parameters never invoke provider");
        rejects("EXTENSION_STATUS", [&] { survivor->evaluate(std::numeric_limits<double>::max(), 4); });
        HANDLE writable = CreateFileW(p.binary_path.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        check(writable == INVALID_HANDLE_VALUE, "loaded verified binary is protected from modification");
        if (writable != INVALID_HANDLE_VALUE) CloseHandle(writable);
        {
            auto local = harness.create_scalar_instance(); auto moved = std::move(local);
            rejects("EXTENSION_LIFETIME", [&] { local.evaluate(1, 1); });
            check(moved.evaluate(2, 1) == 2, "move transfers matching handle");
        }
        check(stats(p.binary_path).destroys == 1 && stats(p.binary_path).live == 1, "exact provider destruction");
        auto second = DeveloperHarness::load_owned_test(p, trust(p)); auto other = second.create_scalar_instance();
        std::atomic<unsigned> failures{0}; std::vector<std::thread> threads;
        for (unsigned t = 0; t < 8; ++t) threads.emplace_back([&, t] {
            for (unsigned i = 0; i < 12; ++i) try {
                auto& instance = t % 2 ? *survivor : other;
                if (instance.evaluate(static_cast<double>(i), 2) != i * 2) ++failures;
            } catch (...) { ++failures; }
        });
        for (auto& thread : threads) thread.join();
        check(failures == 0 && stats(p.binary_path).maximum_active == 1, "concurrent callbacks serialized across same DLL harnesses");
    }
    check(loaded(p.binary_path), "live instance pins DLL after harness lifetime");
    check(survivor->evaluate(7, 1) == 7, "instance remains valid after harness scope");
    check(stats(p.binary_path).creates == 3 && stats(p.binary_path).destroys == 2 && stats(p.binary_path).live == 1, "matching provider owner counts");
    survivor.reset(); check(!loaded(p.binary_path), "last instance destruction precedes unload");
    check(retained.type.label == "Test Multiply 倍" && retained.package_id == p.package_id, "copied UTF-8 descriptors survive unload");
    check(!loaded(p.binary_path), "retained inspection data invokes no callback after unload");
}
void provider_prefixes(const Fixtures& fixtures) {
    const auto p = package(fixtures.make("provider-prefixes"));
    auto harness = DeveloperHarness::load_owned_test(p, trust(p));
    const auto module = GetModuleHandleW(p.binary_path.c_str());
    const auto entry = reinterpret_cast<NectExtGetApiV1>(GetProcAddress(module, "nect_extension_get_api_v1"));
    ShortPrefix short_prefix;
    NectExtHostV1 host{sizeof(host), 1, 0, 1}; NectExtApiV1 api{sizeof(api), 1, 0};
    check(entry(reinterpret_cast<NectExtHostV1*>(short_prefix.size), &api) == NECT_EXT_INVALID_ARGUMENT, "provider rejects short host without overread at guard page");
    check(entry(&host, reinterpret_cast<NectExtApiV1*>(short_prefix.size)) == NECT_EXT_INVALID_ARGUMENT, "provider rejects short API output without read/write overrun");
    host.api_major = 2; check(entry(&host, &api) == NECT_EXT_UNSUPPORTED, "provider API major check"); host.api_major = 1;
    host.capabilities = 0; check(entry(&host, &api) == NECT_EXT_UNSUPPORTED, "provider read-only capability check"); host.capabilities = 1;
    struct BigHost { NectExtHostV1 prefix; uint64_t tail; } big_host{{sizeof(BigHost), 1, 0, 1}, UINT64_C(0x12345678)};
    struct BigApi { NectExtApiV1 prefix; uint64_t tail; } big_api{{sizeof(BigApi), 1, 0}, UINT64_C(0xabcdef)};
    check(entry(&big_host.prefix, &big_api.prefix) == NECT_EXT_OK && big_api.tail == UINT64_C(0xabcdef) && big_host.tail == UINT64_C(0x12345678), "provider uses bounded required prefixes and preserves trailing fields");
    api = big_api.prefix; const auto* d = api.type_descriptor(0); check(api.type_descriptor(1) == nullptr, "bounded descriptor index");
    NectExtInstanceV1* instance = nullptr;
    check(api.create_instance("wrong.type", 1, &instance) == NECT_EXT_UNSUPPORTED && !instance, "provider rejects unknown type");
    check(api.create_instance(d->type_id, 2, &instance) == NECT_EXT_UNSUPPORTED && !instance, "provider rejects behavior upgrade");
    check(api.create_instance(d->type_id, 1, &instance) == NECT_EXT_OK && instance, "direct owned instance for guarded prefix test");
    NectExtScalarTestInputV1 input{sizeof(input), 1, 0, 2, 3}; NectExtScalarTestOutputV1 output{sizeof(output), 1, 0, 99};
    check(d->scalar_test_evaluate(instance, reinterpret_cast<NectExtScalarTestInputV1*>(short_prefix.size), &output) == NECT_EXT_INVALID_ARGUMENT && output.value == 99, "provider short input bounded/no output publish");
    check(d->scalar_test_evaluate(instance, &input, reinterpret_cast<NectExtScalarTestOutputV1*>(short_prefix.size)) == NECT_EXT_INVALID_ARGUMENT, "provider short output bounded");
    input.api_minor = 1; check(d->scalar_test_evaluate(instance, &input, &output) == NECT_EXT_UNSUPPORTED, "domain prefix minor rejection"); input.api_minor = 0;
    input.factor = 5; check(d->scalar_test_evaluate(instance, &input, &output) == NECT_EXT_INVALID_ARGUMENT && output.value == 99, "provider bounds independently enforced");
    api.destroy_instance(instance); check(stats(p.binary_path).live == 0, "guarded direct instance matched free");
}
} // namespace
int main(int argc, char** argv) {
    try {
        if (argc != 4) throw std::runtime_error("Expected manifest template, owned binaries, scratch root");
        Fixtures fixtures{fs::u8path(argv[2]), fs::u8path(argv[3]) / ("run-" + std::to_string(GetCurrentProcessId())), read(fs::u8path(argv[1]))};
        fs::create_directories(fixtures.scratch);
        std::cout << "discovery\n" << std::flush; discovery(fixtures);
        std::cout << "manifest negatives\n" << std::flush; manifest_negatives(fixtures);
        std::cout << "trust/descriptor negatives\n" << std::flush; trust_and_descriptors(fixtures);
        std::cout << "lifecycle/serialization\n" << std::flush; lifecycle(fixtures);
        std::cout << "provider guarded prefixes\n" << std::flush; provider_prefixes(fixtures);
        std::cout << "R06-C2A: " << checks << " focused checks PASS; developer-only scalar_test; no Document/Session/native/catalog integration\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << "FAIL after " << checks << " checks: " << e.what() << '\n'; return 1; }
}

#include "nect/extension_registry.hpp"
#include <iomanip>
#include <iostream>

int main(int argc, char** argv) {
    using namespace nect::extension;
    try {
        if (argc < 3) throw std::runtime_error("Usage: extension_harness inspect ROOT | evaluate-owned-test ROOT PACKAGE_ID VERSION SHA256 VALUE FACTOR");
        const auto snapshot = Snapshot::discover({std::filesystem::u8path(argv[2])});
        if (std::string(argv[1]) == "inspect" && argc == 3) {
            for (const auto& p : snapshot.packages())
                std::cout << p.package_id << ' ' << p.package_version << ' ' << p.sha256 << ' '
                          << p.type.type_id << "@" << p.type.behavior_version << " test_internal disabled/untrusted\n";
            return 0;
        }
        if (std::string(argv[1]) != "evaluate-owned-test" || argc != 8 || snapshot.packages().size() != 1)
            throw std::runtime_error("Expected one explicit owned test package and exact trust identity");
        const auto& p = snapshot.packages()[0];
        // Restrict this command to the package built from the owned tiny fixture.
        if (p.package_id != "org.example.nect.testop" || p.type.type_id != "org.example.nect.testop.multiply")
            throw std::runtime_error("This developer command accepts only the owned multiply fixture");
        std::cout << "Executing explicitly trusted in-process native code (not sandboxed).\n";
        const OwnedTestTrust trust{p.root, argv[3], argv[4], argv[5]};
        auto harness = DeveloperHarness::load_owned_test(p, trust);
        auto instance = harness.create_scalar_instance();
        auto numeric = [](const char* text) { size_t n = 0; const std::string s(text); const double v = std::stod(s, &n); if (n != s.size()) throw std::runtime_error("Invalid numeric argument"); return v; };
        std::cout << std::setprecision(17) << instance.evaluate(numeric(argv[6]), numeric(argv[7])) << '\n';
        return 0;
    } catch (const Error& e) { std::cerr << e.code << ": " << e.what() << '\n'; return 1; }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}

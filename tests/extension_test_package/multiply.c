#define NECT_EXT_BUILD_PROVIDER
#include "nect/extension_abi.h"
#include "fixture_stats.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#ifndef NECT_FIXTURE_MODE
#define NECT_FIXTURE_MODE 0
#endif
#if defined(__cplusplus)
#include <stdexcept>
#define C_LINKAGE extern "C"
#else
#define C_LINKAGE
#endif

struct NectExtInstanceV1 { uint32_t owner_marker; };
static volatile LONG active, maximum_active, entries, creates, destroys, evaluations, live;
static void enter_callback(void) {
    LONG n = InterlockedIncrement(&active), before;
    do { before = maximum_active; if (before >= n) break; }
    while (InterlockedCompareExchange(&maximum_active, n, before) != before);
}
static void leave_callback(void) { InterlockedDecrement(&active); }

static NectExtStatusV1 NECT_EXT_CALL evaluate(NectExtInstanceV1* instance,
    const NectExtScalarTestInputV1* input, NectExtScalarTestOutputV1* output) {
    double product;
    if (!instance || instance->owner_marker != 0x4e454354u || !input || !output ||
        input->struct_size < sizeof(*input) || output->struct_size < sizeof(*output)) return NECT_EXT_INVALID_ARGUMENT;
    if (input->api_major != 1 || input->api_minor != 0 || output->api_major != 1 || output->api_minor != 0) return NECT_EXT_UNSUPPORTED;
    if (!isfinite(input->value) || !isfinite(input->factor) || input->factor < 0 || input->factor > 4) return NECT_EXT_INVALID_ARGUMENT;
    enter_callback(); InterlockedIncrement(&evaluations);
    /* Widen overlap opportunity for the focused concurrent-call oracle. */
    Sleep(1);
#if NECT_FIXTURE_MODE == 5
    leave_callback(); return NECT_EXT_EVALUATION_ERROR;
#elif NECT_FIXTURE_MODE == 13 && defined(__cplusplus)
    /* Deliberate ABI violation: host's defensive C++ exception test only. */
    leave_callback(); throw std::runtime_error("deliberate fixture violation");
#endif
    product = input->value * input->factor;
    if (!isfinite(product)) { leave_callback(); return NECT_EXT_EVALUATION_ERROR; }
#if NECT_FIXTURE_MODE == 15
    output->value = HUGE_VAL;
#else
    output->value = product;
#endif
    leave_callback(); return NECT_EXT_OK;
}
static const struct { NectExtParameterV1 prefix; uint64_t tail[2]; } parameter = {
    {NECT_FIXTURE_MODE == 8 ? 4 : (NECT_FIXTURE_MODE == 14 ? sizeof(NectExtParameterV1) + 16 : sizeof(NectExtParameterV1)),
     1, 0, "factor", "number", "unitless", 1, 0, NECT_FIXTURE_MODE == 17 ? 5 : 4}, {0,0}
};
static const struct { NectExtDescriptorV1 prefix; uint64_t tail[2]; } descriptor = {
    {NECT_FIXTURE_MODE == 7 ? 4 : (NECT_FIXTURE_MODE == 14 ? sizeof(NectExtDescriptorV1) + 16 : sizeof(NectExtDescriptorV1)),
     1, 0, "org.example.nect.testop.multiply", 1, "test_internal",
     NECT_FIXTURE_MODE == 2 ? "wrong label" : (NECT_FIXTURE_MODE == 16 ? "\xff" : "Test Multiply \xe5\x80\x8d"),
     "object", "scalar_test", "scalar_test", 1, &parameter.prefix, 1, "unsupported",
     NECT_FIXTURE_MODE == 3 ? NULL : evaluate}, {0,0}
};
static const NectExtDescriptorV1* NECT_EXT_CALL type_descriptor(uint32_t index) {
    enter_callback(); leave_callback(); return index == 0 ? &descriptor.prefix : NULL;
}
static NectExtStatusV1 NECT_EXT_CALL create_instance(const char* type_id, uint32_t version, NectExtInstanceV1** out) {
    NectExtInstanceV1* instance;
    if (!out) return NECT_EXT_INVALID_ARGUMENT;
    *out = NULL;
    if (!type_id || strcmp(type_id, descriptor.prefix.type_id) != 0 || version != 1) return NECT_EXT_UNSUPPORTED;
    enter_callback(); InterlockedIncrement(&creates);
#if NECT_FIXTURE_MODE == 11
    leave_callback(); return NECT_EXT_OK;
#endif
    instance = (NectExtInstanceV1*)calloc(1, sizeof(*instance));
    if (!instance) { leave_callback(); return NECT_EXT_EVALUATION_ERROR; }
    instance->owner_marker = 0x4e454354u; *out = instance; InterlockedIncrement(&live);
    leave_callback(); return NECT_FIXTURE_MODE == 12 ? NECT_EXT_EVALUATION_ERROR : NECT_EXT_OK;
}
static void NECT_EXT_CALL destroy_instance(NectExtInstanceV1* instance) {
    enter_callback();
#if NECT_FIXTURE_MODE == 19 && defined(__cplusplus)
    leave_callback(); throw std::runtime_error("deliberate destroy violation");
#endif
    if (instance && instance->owner_marker == 0x4e454354u) {
        instance->owner_marker = 0; free(instance); InterlockedIncrement(&destroys); InterlockedDecrement(&live);
    }
    leave_callback();
}
C_LINKAGE __declspec(dllexport) void NECT_EXT_CALL nect_fixture_stats(NectFixtureStats* out) {
    if (!out || out->struct_size < sizeof(*out)) return;
    out->api_major = 1; out->api_minor = 0;
    out->entries = (uint32_t)entries; out->creates = (uint32_t)creates;
    out->destroys = (uint32_t)destroys; out->evaluations = (uint32_t)evaluations;
    out->live = (uint32_t)live; out->maximum_active = (uint32_t)maximum_active;
}
#if NECT_FIXTURE_MODE != 18
C_LINKAGE NECT_EXT_EXPORT NectExtStatusV1 NECT_EXT_CALL
nect_extension_get_api_v1(const NectExtHostV1* host, NectExtApiV1* out) {
    uint32_t size;
    /* Check size before reading any later prefix field, and before writing. */
    if (!host || !out || host->struct_size < sizeof(*host) || out->struct_size < sizeof(*out)) return NECT_EXT_INVALID_ARGUMENT;
    if (host->api_major != 1 || host->api_minor != 0 || out->api_major != 1 || out->api_minor != 0 ||
        !(host->capabilities & NECT_EXT_SCALAR_TEST_CAPABILITY)) return NECT_EXT_UNSUPPORTED;
    enter_callback(); InterlockedIncrement(&entries); size = out->struct_size;
    out->struct_size = NECT_FIXTURE_MODE == 6 ? 4 : size;
    out->api_major = NECT_FIXTURE_MODE == 1 ? 2 : 1; out->api_minor = 0;
    out->package_id = "org.example.nect.testop"; out->package_version = "1.0.0"; out->type_count = 1;
    out->type_descriptor = NECT_FIXTURE_MODE == 20 ? NULL : type_descriptor;
    out->create_instance = NECT_FIXTURE_MODE == 9 ? NULL : create_instance;
    out->destroy_instance = NECT_FIXTURE_MODE == 10 ? NULL : destroy_instance;
    leave_callback(); return NECT_FIXTURE_MODE == 4 ? NECT_EXT_UNSUPPORTED : NECT_EXT_OK;
}
#endif

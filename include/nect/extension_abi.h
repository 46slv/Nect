#ifndef NECT_EXTENSION_ABI_H
#define NECT_EXTENSION_ABI_H

/* R06-C2A: Windows x86_64 C ABI, scalar_test qualification only.
 * No product evaluator or Document/Session ABI is defined here.
 * All structs start with size/major/minor. Read only the advertised prefix;
 * v1.0 requires the complete v1 struct, accepts a larger trailing extension.
 * Strings are bounded UTF-8 NUL-terminated, immutable provider-owned data valid
 * until library unload. Host must copy retained descriptors/strings.
 * Callbacks are serialized and non-reentrant. Providers catch all exceptions.
 * Instances are provider-owned: only the matching destroy callback frees them.
 * No cross-module buffer allocation is needed in this profile.
 * Native code executes with process/user authority; this is NOT a sandbox.
 * Windows x64 default C layout with maximum member alignment 8; uint32_t status
 * enum values use the qualified MSVC 32-bit enum representation.
 */
#include <stdint.h>
#if defined(_WIN32)
#define NECT_EXT_CALL __cdecl
#if defined(NECT_EXT_BUILD_PROVIDER)
#define NECT_EXT_EXPORT __declspec(dllexport)
#else
#define NECT_EXT_EXPORT
#endif
#else
#define NECT_EXT_CALL
#define NECT_EXT_EXPORT
#endif
#ifdef __cplusplus
extern "C" {
#endif
#pragma pack(push, 8)

#define NECT_EXT_API_MAJOR 1u
#define NECT_EXT_API_MINOR 0u
#define NECT_EXT_SCALAR_TEST_CAPABILITY 1u
#define NECT_EXT_STRING_LIMIT 1024u

typedef enum NectExtStatusV1 {
    NECT_EXT_OK = 0,
    NECT_EXT_INVALID_ARGUMENT = 1,
    NECT_EXT_UNSUPPORTED = 2,
    NECT_EXT_EVALUATION_ERROR = 3
} NectExtStatusV1;

typedef struct NectExtInstanceV1 NectExtInstanceV1;
typedef struct NectExtHostV1 {
    uint32_t struct_size;
    uint32_t api_major;
    uint32_t api_minor;
    uint32_t capabilities; /* read-only scalar_test query; no OS/UI services */
} NectExtHostV1;

typedef struct NectExtScalarTestInputV1 {
    uint32_t struct_size;
    uint32_t api_major;
    uint32_t api_minor;
    double value; /* finite */
    double factor; /* finite, inclusive [0,4] */
} NectExtScalarTestInputV1;
typedef struct NectExtScalarTestOutputV1 {
    uint32_t struct_size;
    uint32_t api_major;
    uint32_t api_minor;
    double value; /* finite; published only on OK */
} NectExtScalarTestOutputV1;

typedef NectExtStatusV1 (NECT_EXT_CALL *NectExtScalarTestEvaluateV1)(
    NectExtInstanceV1*, const NectExtScalarTestInputV1*, NectExtScalarTestOutputV1*);

typedef struct NectExtParameterV1 {
    uint32_t struct_size;
    uint32_t api_major;
    uint32_t api_minor;
    const char* key;
    const char* type;
    const char* unit;
    double default_value;
    double minimum;
    double maximum;
} NectExtParameterV1;

typedef struct NectExtDescriptorV1 {
    uint32_t struct_size;
    uint32_t api_major;
    uint32_t api_minor;
    const char* type_id;
    uint32_t behavior_version;
    const char* kind;
    const char* label;
    const char* target_scope;
    const char* input_domain;
    const char* output_domain;
    uint32_t parameter_count;
    const NectExtParameterV1* parameters; /* C2A exactly one */
    uint32_t native_export; /* 1 */
    const char* svg_export; /* "unsupported" */
    NectExtScalarTestEvaluateV1 scalar_test_evaluate;
} NectExtDescriptorV1;

typedef struct NectExtApiV1 {
    uint32_t struct_size;
    uint32_t api_major;
    uint32_t api_minor;
    const char* package_id;
    const char* package_version;
    uint32_t type_count;
    const NectExtDescriptorV1* (NECT_EXT_CALL *type_descriptor)(uint32_t index);
    NectExtStatusV1 (NECT_EXT_CALL *create_instance)(
        const char* type_id, uint32_t behavior_version, NectExtInstanceV1** out);
    void (NECT_EXT_CALL *destroy_instance)(NectExtInstanceV1* instance);
} NectExtApiV1;

typedef NectExtStatusV1 (NECT_EXT_CALL *NectExtGetApiV1)(const NectExtHostV1*, NectExtApiV1*);
NECT_EXT_EXPORT NectExtStatusV1 NECT_EXT_CALL
nect_extension_get_api_v1(const NectExtHostV1* host, NectExtApiV1* out_api);

#pragma pack(pop)
#ifdef __cplusplus
}
#endif
#endif

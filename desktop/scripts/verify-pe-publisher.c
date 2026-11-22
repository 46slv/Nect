// Development verification only. Build from a Visual Studio developer shell:
// cl /W4 /WX verify-pe-publisher.c wintrust.lib crypt32.lib
// Input two is the *expected* SHA-256 of the leaf certificate in DER form.
// Windows trust alone does not identify an approved Nect component. This
// probe neither trusts a runtime peer nor changes any system trust setting.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#include <wintrust.h>
#pragma warning(push)
#pragma warning(disable:4201)
#include <softpub.h>
#pragma warning(pop)
#include <stdio.h>
#include <wchar.h>
#include <string.h>

static int read_hex(const wchar_t *in, BYTE expected[32]) {
    if (wcslen(in) != 64) return 0;
    for (unsigned i = 0; i < 32; ++i) {
        unsigned n = 0;
        for (unsigned j = 0; j < 2; ++j) {
            const wchar_t c = in[2*i+j];
            if (c >= L'0' && c <= L'9') n = (n*16) + (unsigned)(c-L'0');
            else if (c >= L'a' && c <= L'f') n = (n*16) + (unsigned)(c-L'a'+10);
            else return 0;
        }
        expected[i] = (BYTE)n;
    }
    return 1;
}

int wmain(int argc, wchar_t **argv) {
    BYTE expected[32];
    BYTE actual[32] = {0};
    DWORD actual_size = sizeof(actual);
    if (argc != 3 || !read_hex(argv[2], expected)) return 10;
    HANDLE file = CreateFileW(argv[1], GENERIC_READ, FILE_SHARE_READ,
                              NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return 11;
    WINTRUST_FILE_INFO info = {0};
    info.cbStruct = sizeof(info);
    info.pcwszFilePath = argv[1];
    info.hFile = file;
    WINTRUST_DATA data = {0};
    data.cbStruct = sizeof(data);
    data.dwUIChoice = WTD_UI_NONE;
    data.fdwRevocationChecks = WTD_REVOKE_WHOLECHAIN;
    data.dwUnionChoice = WTD_CHOICE_FILE;
    data.pFile = &info;
    data.dwStateAction = WTD_STATEACTION_VERIFY;
    data.dwProvFlags = WTD_REVOCATION_CHECK_CHAIN_EXCLUDE_ROOT | WTD_CACHE_ONLY_URL_RETRIEVAL;
    GUID action = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    LONG status = WinVerifyTrust(INVALID_HANDLE_VALUE, &action, &data);
    int result = 0;
    if (status != ERROR_SUCCESS) {
        printf("{\"status\":\"signature-not-verified\",\"provider\":\"%08lX\"}\n", (unsigned long)status);
        result = 2;
    } else {
        CRYPT_PROVIDER_DATA *provider = WTHelperProvDataFromStateData(data.hWVTStateData);
        CRYPT_PROVIDER_SGNR *signer = provider ? WTHelperGetProvSignerFromChain(provider, 0, FALSE, 0) : NULL;
        CRYPT_PROVIDER_CERT *leaf = signer ? WTHelperGetProvCertFromChain(signer, 0) : NULL;
        if (!leaf || !leaf->pCert || !CertGetCertificateContextProperty(leaf->pCert,
                CERT_SHA256_HASH_PROP_ID, actual, &actual_size) || actual_size != sizeof(actual)) {
            puts("{\"status\":\"signer-unavailable\"}");
            result = 3;
        } else if (memcmp(expected, actual, sizeof(actual)) != 0) {
            puts("{\"status\":\"signer-mismatch\"}");
            result = 4;
        } else {
            puts("{\"status\":\"expected-signer-verified\"}");
        }
    }
    data.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(INVALID_HANDLE_VALUE, &action, &data);
    CloseHandle(file);
    return result;
}

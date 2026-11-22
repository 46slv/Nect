/* Local offer setup only; no filesystem, listener, or document access. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <sddl.h>
#include <bcrypt.h>
#include <wchar.h>

int nect_pairing_seed(WCHAR *sid_out, DWORD sid_capacity, BYTE *nonce_out) {
    HANDLE token = NULL;
    TOKEN_GROUPS *groups = NULL;
    LPWSTR sid_text = NULL;
    DWORD needed = 0;
    BOOL prepared = FALSE;
    if (!sid_out || sid_capacity == 0 || !nonce_out) return 0;
    sid_out[0] = 0;
    SecureZeroMemory(nonce_out, 16);
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return 0;

    if (GetTokenInformation(token, TokenLogonSid, NULL, 0, &needed) ||
        GetLastError() != ERROR_INSUFFICIENT_BUFFER ||
        needed < sizeof(TOKEN_GROUPS) || needed > 4096) goto cleanup;
    groups = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, needed);
    if (!groups || !GetTokenInformation(token, TokenLogonSid, groups, needed, &needed) ||
        groups->GroupCount != 1 ||
        (groups->Groups[0].Attributes & SE_GROUP_LOGON_ID) != SE_GROUP_LOGON_ID ||
        !IsValidSid(groups->Groups[0].Sid) ||
        !ConvertSidToStringSidW(groups->Groups[0].Sid, &sid_text)) goto cleanup;

    size_t length = wcslen(sid_text);
    if (length + 1 > sid_capacity ||
        BCryptGenRandom(NULL, nonce_out, 16, BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) goto cleanup;
    memcpy(sid_out, sid_text, (length + 1) * sizeof(WCHAR));
    prepared = TRUE;
cleanup:
    if (!prepared) SecureZeroMemory(nonce_out, 16);
    if (sid_text) LocalFree(sid_text);
    if (groups) HeapFree(GetProcessHeap(), 0, groups);
    CloseHandle(token);
    return prepared ? 1 : 0;
}

/* Caller owns a selected application-private directory and its fresh leaf.
   The file belongs to this handle and is removed on close, not by later
   resolving a mutable path. This is metadata only and confers no authority. */
HANDLE nect_open_private_offer(const WCHAR *path, const WCHAR *logon_sid) {
    WCHAR rules[192];
    if (swprintf_s(rules, sizeof(rules)/sizeof(rules[0]),
                   L"D:P(A;;FA;;;%ls)", logon_sid) <= 0) return INVALID_HANDLE_VALUE;
    PSECURITY_DESCRIPTOR descriptor = NULL;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
            rules, SDDL_REVISION_1, &descriptor, NULL)) return INVALID_HANDLE_VALUE;
    SECURITY_ATTRIBUTES attrs = {0};
    attrs.nLength = sizeof(attrs);
    attrs.lpSecurityDescriptor = descriptor;
    HANDLE handle = CreateFileW(path, GENERIC_WRITE | DELETE,
        FILE_SHARE_READ | FILE_SHARE_DELETE, &attrs, CREATE_NEW,
        FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL);
    DWORD result = GetLastError();
    LocalFree(descriptor);
    SetLastError(result);
    return handle;
}

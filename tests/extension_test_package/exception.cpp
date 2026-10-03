// This deliberately broken owned fixture tests defensive exception rejection.
// It is never a valid provider: the public C ABI forbids escaping exceptions.
#ifndef NECT_FIXTURE_MODE
#define NECT_FIXTURE_MODE 13
#endif
#include "multiply.c"

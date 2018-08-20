#include "eneida_directx12.h"
#include "eneida_imgui.h"

struct test_dispatch_table
{
    void* Data;
    bool (*Initialize)(test_dispatch_table& Dispatch, directx12& Dx);
    void (*Shutdown)(test_dispatch_table& Dispatch, directx12& Dx);
    void (*Update)(test_dispatch_table& Dispatch, directx12& Dx, double Time, float DeltaTime);
};
// vim: set ts=4 sw=4 expandtab:

#include "eneida_external.h"
#include "eneida.h"
#include "eneida_common.cpp"
#include "eneida_directx12.cpp"
#include "eneida_imgui.cpp"


void *operator new[](size_t size, const char* /*name*/, int /*flags*/,
                     unsigned /*debugFlags*/, const char* /*file*/, int /*line*/)
{
    return malloc(size);
}

void *operator new[](size_t size, size_t alignment, size_t alignmentOffset, const char* /*name*/,
                     int /*flags*/, unsigned /*debugFlags*/, const char* /*file*/, int /*line*/)
{
    return _aligned_offset_malloc(size, alignment, alignmentOffset);
}

static void
UpdateFrameStats(HWND Window, const char *Name, double &OutTime, float &OutDeltaTime)
{
    static double PreviousTime = -1.0;
    static double HeaderRefreshTime = 0.0;
    static uint32_t FrameCount = 0;

    if (PreviousTime < 0.0)
    {
        PreviousTime = GetAbsoluteTime();
        HeaderRefreshTime = PreviousTime;
    }

    OutTime = GetAbsoluteTime();
    OutDeltaTime = (float)(OutTime - PreviousTime);
    PreviousTime = OutTime;

    if ((OutTime - HeaderRefreshTime) >= 1.0)
    {
        const double FramesPerSecond = FrameCount / (OutTime - HeaderRefreshTime);
        const double MilliSeconds = (1.0 / FramesPerSecond) * 1000.0;
        char Header[256];
        snprintf(Header, sizeof(Header), "[%.1f fps  %.3f ms] %s", FramesPerSecond, MilliSeconds, Name);
        SetWindowText(Window, Header);
        HeaderRefreshTime = OutTime;
        FrameCount = 0;
    }
    FrameCount++;
}

static LRESULT CALLBACK
ProcessWindowMessage(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
    switch (Message)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_KEYDOWN:
        if (WParam == VK_ESCAPE)
        {
            PostQuitMessage(0);
            return 0;
        }
        break;
    }
    return DefWindowProc(Window, Message, WParam, LParam);
}

static HWND
InitializeWindow(const char *Name, uint32_t Width, uint32_t Height)
{
    WNDCLASS WinClass = {};
    WinClass.lpfnWndProc = ProcessWindowMessage;
    WinClass.hInstance = GetModuleHandle(nullptr);
    WinClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    WinClass.lpszClassName = Name;
    if (!RegisterClass(&WinClass))
        assert(0);

    RECT Rect = { 0, 0, (LONG)Width, (LONG)Height };
    if (!AdjustWindowRect(&Rect, WS_OVERLAPPED | WS_SYSMENU | WS_CAPTION | WS_MINIMIZEBOX, 0))
        assert(0);

    HWND Window = CreateWindowEx(
        0, Name, Name, WS_OVERLAPPED | WS_SYSMENU | WS_CAPTION | WS_MINIMIZEBOX | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT,
        Rect.right - Rect.left, Rect.bottom - Rect.top,
        nullptr, nullptr, nullptr, 0);
    assert(Window);
    return Window;
}

static void
Draw(directx12 &Dx)
{
    ID3D12CommandAllocator *CmdAlloc = Dx.CmdAlloc[Dx.FrameIndex];
    ID3D12GraphicsCommandList *CmdList = Dx.CmdList;

    CmdAlloc->Reset();
    CmdList->Reset(CmdAlloc, nullptr);

    CmdList->RSSetViewports(1, &CD3DX12_VIEWPORT(0.0f, 0.0f, (float)Dx.Resolution[0], (float)Dx.Resolution[1]));
    CmdList->RSSetScissorRects(1, &CD3DX12_RECT(0, 0, Dx.Resolution[0], Dx.Resolution[1]));

    CmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(Dx.SwapBuffers[Dx.BackBufferIndex],
                                                                      D3D12_RESOURCE_STATE_PRESENT,
                                                                      D3D12_RESOURCE_STATE_RENDER_TARGET));

    D3D12_CPU_DESCRIPTOR_HANDLE BackBufferDescriptor = Dx.RenderTargetHeap.CpuStart;
    BackBufferDescriptor.ptr += Dx.BackBufferIndex * Dx.DescriptorSizeRtv;

    const float ClearColor[4] = { 0.0f, 0.2f, 0.4f, 1.0f };

    CmdList->OMSetRenderTargets(1, &BackBufferDescriptor, 0, nullptr);
    CmdList->ClearRenderTargetView(BackBufferDescriptor, ClearColor, 0, nullptr);

    /*
    cl->SetPipelineState(demo.pso);
    cl->SetGraphicsRootSignature(demo.rootSig);
    cl->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);

    for (uint32_t i = 0; i < 100000; ++i)
    {
        float p[2] = { Randomf(-0.7f, 0.7f), Randomf(-0.7f, 0.7f) };
        cl->SetGraphicsRoot32BitConstants(0, 2, p, 0);
        cl->DrawInstanced(1, 1, 0, 0);
    }
    */
    CmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(Dx.SwapBuffers[Dx.BackBufferIndex],
                                                                      D3D12_RESOURCE_STATE_RENDER_TARGET,
                                                                      D3D12_RESOURCE_STATE_PRESENT));
    VHR(CmdList->Close());

    Dx.CmdQueue->ExecuteCommandLists(1, (ID3D12CommandList**)&CmdList);
}

int CALLBACK
WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    SetProcessDPIAware();

    directx12 Dx = {};
    imgui_renderer GuiRenderer = {};

    const char* Name = "eneida";
    Dx.Window = InitializeWindow(Name, 1280, 720);
    InitializeDirectX12(Dx);

    ImGui::CreateContext();
    InitializeGuiRenderer(GuiRenderer, Dx);

    for (;;)
    {
        MSG Message = {};
        if (PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
        {
            DispatchMessage(&Message);
            if (Message.message == WM_QUIT)
                break;
        }
        else
        {
            double Time;
            float DeltaTime;
            UpdateFrameStats(Dx.Window, Name, Time, DeltaTime);
            Draw(Dx);
            RenderGui(GuiRenderer, Dx);
            PresentFrame(Dx);
        }
    }

    return 0;
}
// vim: set ts=4 sw=4 expandtab:

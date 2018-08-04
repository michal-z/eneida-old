#include "eneida_external.h"
#include "eneida.h"
#include "eneida_directx12.cpp"


static std::vector<uint8_t>
LoadFile(const char *FileName)
{
    FILE *File = fopen(FileName, "rb");
    assert(File);
    fseek(File, 0, SEEK_END);
    long Size = ftell(File);
    assert(Size != -1);
    std::vector<uint8_t> Content(Size);
    fseek(File, 0, SEEK_SET);
    fread(&Content[0], 1, Content.size(), File);
    fclose(File);
    return Content;
}

static double
GetTime()
{
    static LARGE_INTEGER StartCounter;
    static LARGE_INTEGER Frequency;
    if (StartCounter.QuadPart == 0)
    {
        QueryPerformanceFrequency(&Frequency);
        QueryPerformanceCounter(&StartCounter);
    }
    LARGE_INTEGER Counter;
    QueryPerformanceCounter(&Counter);
    return (Counter.QuadPart - StartCounter.QuadPart) / (double)Frequency.QuadPart;
}

static void
UpdateFrameStats(HWND Window, const char *Name, double &OutTime, float &OutDeltaTime)
{
    static double PreviousTime = -1.0;
    static double HeaderRefreshTime = 0.0;
    static uint32_t FrameCount = 0;

    if (PreviousTime < 0.0)
    {
        PreviousTime = GetTime();
        HeaderRefreshTime = PreviousTime;
    }

    OutTime = GetTime();
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

    D3D12_CPU_DESCRIPTOR_HANDLE BackBufferDescriptor = Dx.SwapBufferHeapStart;
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

    Dx.CmdQueue->ExecuteCommandLists(1, (ID3D12CommandList **)&CmdList);
}

static void
Initialize(const directx12 &Dx)
{
#if 0
    /* pso */ {
        std::vector<uint8_t> VsCode = LoadFile("VsTransform.cso");
        std::vector<uint8_t> PsCode = LoadFile("PsShade.cso");

        D3D12_GRAPHICS_PIPELINE_STATE_DESC PsoDesc = {};
        PsoDesc.VS = { VsCode.data(), VsCode.size() };
        PsoDesc.PS = { PsCode.data(), PsCode.size() };
        PsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        PsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        PsoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        PsoDesc.SampleMask = 0xffffffff;
        PsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
        PsoDesc.NumRenderTargets = 1;
        PsoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        PsoDesc.SampleDesc.Count = 1;

        VHR(Dx12->Device->CreateGraphicsPipelineState(&PsoDesc, IID_PPV_ARGS(&Data->PipelineState)));
        VHR(Dx12->Device->CreateRootSignature(0, VsCode.data(), VsCode.size(), IID_PPV_ARGS(&Data->RootSignature)));
    }
#endif
}

int CALLBACK
WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    SetProcessDPIAware();

    directx12 Dx = {};

    const char *Name = "eneida";
    Dx.Window = InitializeWindow(Name, 1280, 720);
    InitializeDirectX12(Dx);
    Initialize(Dx);

    /*
    int x, y, n;
    stbi_load("adsa", &x, &y, &n, 4);

    ImGui::CreateContext();
    */

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
            PresentFrame(Dx);
        }
    }

    return 0;
}
// vim: set ts=4 sw=4 expandtab:

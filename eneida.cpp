#include "eneida_external.h"
#include "eneida.h"


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

static void
InitializeDx12(dx12_context &Dx)
{
    IDXGIFactory4 *Factory;
#ifdef _DEBUG
    VHR(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&Factory)));
#else
    VHR(CreateDXGIFactory2(0, IID_PPV_ARGS(&Factory)));
#endif

#ifdef _DEBUG
    {
        ID3D12Debug *Dbg;
        D3D12GetDebugInterface(IID_PPV_ARGS(&Dbg));
        if (Dbg)
        {
            Dbg->EnableDebugLayer();
            ID3D12Debug1 *Dbg1;
            Dbg->QueryInterface(IID_PPV_ARGS(&Dbg1));
            if (Dbg1)
                Dbg1->SetEnableGPUBasedValidation(TRUE);
            SAFE_RELEASE(Dbg);
            SAFE_RELEASE(Dbg1);
        }
    }
#endif
    if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_1, IID_PPV_ARGS(&Dx.Device))))
    {
        // #TODO: Add MessageBox
        return;
    }

    D3D12_COMMAND_QUEUE_DESC CmdQueueDesc = {};
    CmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    CmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    CmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    VHR(Dx.Device->CreateCommandQueue(&CmdQueueDesc, IID_PPV_ARGS(&Dx.CmdQueue)));

    DXGI_SWAP_CHAIN_DESC SwapChainDesc = {};
    SwapChainDesc.BufferCount = 4;
    SwapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    SwapChainDesc.OutputWindow = Dx.Window;
    SwapChainDesc.SampleDesc.Count = 1;
    SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    SwapChainDesc.Windowed = TRUE;

    IDXGISwapChain *TempSwapChain;
    VHR(Factory->CreateSwapChain(Dx.CmdQueue, &SwapChainDesc, &TempSwapChain));
    VHR(TempSwapChain->QueryInterface(IID_PPV_ARGS(&Dx.SwapChain)));
    SAFE_RELEASE(TempSwapChain);
    SAFE_RELEASE(Factory);

    RECT Rect;
    GetClientRect(Dx.Window, &Rect);
    Dx.Resolution[0] = (uint32_t)Rect.right;
    Dx.Resolution[1] = (uint32_t)Rect.bottom;

    for (uint32_t Index = 0; Index < 2; ++Index)
        VHR(Dx.Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&Dx.CmdAlloc[Index])));

    Dx.DescriptorSize = Dx.Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    Dx.DescriptorSizeRtv = Dx.Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    /* swap buffers */ {
        D3D12_DESCRIPTOR_HEAP_DESC HeapDesc = {};
        HeapDesc.NumDescriptors = 4;
        HeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        HeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        VHR(Dx.Device->CreateDescriptorHeap(&HeapDesc, IID_PPV_ARGS(&Dx.SwapBufferHeap)));
        Dx.SwapBufferHeapStart = Dx.SwapBufferHeap->GetCPUDescriptorHandleForHeapStart();

        D3D12_CPU_DESCRIPTOR_HANDLE Handle = Dx.SwapBufferHeapStart;

        for (uint32_t Index = 0; Index < 4; ++Index)
        {
            VHR(Dx.SwapChain->GetBuffer(Index, IID_PPV_ARGS(&Dx.SwapBuffers[Index])));

            Dx.Device->CreateRenderTargetView(Dx.SwapBuffers[Index], nullptr, Handle);
            Handle.ptr += Dx.DescriptorSizeRtv;
        }
    }

    VHR(Dx.Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, Dx.CmdAlloc[0], nullptr, IID_PPV_ARGS(&Dx.CmdList)));
    VHR(Dx.CmdList->Close());

    VHR(Dx.Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&Dx.FrameFence)));
    Dx.FrameFenceEvent = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
}

static void
Shutdown(dx12_context &Dx)
{
    SAFE_RELEASE(Dx.CmdList);
    SAFE_RELEASE(Dx.CmdAlloc[0]);
    SAFE_RELEASE(Dx.CmdAlloc[1]);
    SAFE_RELEASE(Dx.SwapBufferHeap);
    for (int Index = 0; Index < 4; ++Index)
        SAFE_RELEASE(Dx.SwapBuffers[Index]);
    CloseHandle(Dx.FrameFenceEvent);
    SAFE_RELEASE(Dx.FrameFence);
    SAFE_RELEASE(Dx.SwapChain);
    SAFE_RELEASE(Dx.CmdQueue);
    SAFE_RELEASE(Dx.Device);
}

static void
Present(dx12_context &Dx)
{
    Dx.SwapChain->Present(0, 0);
    Dx.CmdQueue->Signal(Dx.FrameFence, ++Dx.FrameCount);

    const uint64_t GpuFrameCount = Dx.FrameFence->GetCompletedValue();

    if ((Dx.FrameCount - GpuFrameCount) >= 2)
    {
        Dx.FrameFence->SetEventOnCompletion(GpuFrameCount + 1, Dx.FrameFenceEvent);
        WaitForSingleObject(Dx.FrameFenceEvent, INFINITE);
    }

    Dx.FrameIndex = !Dx.FrameIndex;
    Dx.BackBufferIndex = Dx.SwapChain->GetCurrentBackBufferIndex();
}

static void
Flush(dx12_context &Dx)
{
    Dx.CmdQueue->Signal(Dx.FrameFence, ++Dx.FrameCount);
    Dx.FrameFence->SetEventOnCompletion(Dx.FrameCount, Dx.FrameFenceEvent);
    WaitForSingleObject(Dx.FrameFenceEvent, INFINITE);
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
Draw(dx12_context &Dx)
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
Initialize(const dx12_context &Dx)
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

    dx12_context Dx = {};

    const char *Name = "eneida";
    Dx.Window = InitializeWindow(Name, 1280, 720);
    InitializeDx12(Dx);
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
            Present(Dx);
        }
    }

    return 0;
}
// vim: set ts=4 sw=4 expandtab:

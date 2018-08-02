#include "FractalFlamesPch.h"


struct dx12_context
{
    ID3D12Device *Device;
    ID3D12CommandQueue *CmdQueue;
    ID3D12CommandAllocator *CmdAlloc[2];
    ID3D12GraphicsCommandList *CmdList;
    IDXGISwapChain3 *SwapChain;
    ID3D12DescriptorHeap *SwapBufferHeap;
    D3D12_CPU_DESCRIPTOR_HANDLE SwapBufferHeapStart;
    ID3D12Resource *SwapBuffers[4];
    ID3D12Fence *FrameFence;
    HANDLE FrameFenceEvent;
    HWND Window;
    uint32_t Resolution[2];
    uint32_t DescriptorSize;
    uint32_t DescriptorSizeRtv;
    uint32_t FrameIndex;
    uint32_t BackBufferIndex;
    uint64_t FrameCount;
};

struct dx12_resources
{
    ID3D12PipelineState *PipelineState;
    ID3D12RootSignature *RootSignature;
};

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
InitializeDx12(dx12_context *Dx12)
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
    if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_1, IID_PPV_ARGS(&Dx12->Device))))
    {
        // #TODO: Add MessageBox
        return;
    }

    D3D12_COMMAND_QUEUE_DESC CmdQueueDesc = {};
    CmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    CmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    CmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    VHR(Dx12->Device->CreateCommandQueue(&CmdQueueDesc, IID_PPV_ARGS(&Dx12->CmdQueue)));

    DXGI_SWAP_CHAIN_DESC SwapChainDesc = {};
    SwapChainDesc.BufferCount = 4;
    SwapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    SwapChainDesc.OutputWindow = Dx12->Window;
    SwapChainDesc.SampleDesc.Count = 1;
    SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    SwapChainDesc.Windowed = TRUE;

    IDXGISwapChain *TempSwapChain;
    VHR(Factory->CreateSwapChain(Dx12->CmdQueue, &SwapChainDesc, &TempSwapChain));
    VHR(TempSwapChain->QueryInterface(IID_PPV_ARGS(&Dx12->SwapChain)));
    SAFE_RELEASE(TempSwapChain);
    SAFE_RELEASE(Factory);

    RECT Rect;
    GetClientRect(Dx12->Window, &Rect);
    Dx12->Resolution[0] = (uint32_t)Rect.right;
    Dx12->Resolution[1] = (uint32_t)Rect.bottom;

    for (uint32_t Index = 0; Index < 2; ++Index)
        VHR(Dx12->Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&Dx12->CmdAlloc[Index])));

    Dx12->DescriptorSize = Dx12->Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    Dx12->DescriptorSizeRtv = Dx12->Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    /* swap buffers */ {
        D3D12_DESCRIPTOR_HEAP_DESC HeapDesc = {};
        HeapDesc.NumDescriptors = 4;
        HeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        HeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        VHR(Dx12->Device->CreateDescriptorHeap(&HeapDesc, IID_PPV_ARGS(&Dx12->SwapBufferHeap)));
        Dx12->SwapBufferHeapStart = Dx12->SwapBufferHeap->GetCPUDescriptorHandleForHeapStart();

        D3D12_CPU_DESCRIPTOR_HANDLE Handle = Dx12->SwapBufferHeapStart;

        for (uint32_t Index = 0; Index < 4; ++Index)
        {
            VHR(Dx12->SwapChain->GetBuffer(Index, IID_PPV_ARGS(&Dx12->SwapBuffers[Index])));

            Dx12->Device->CreateRenderTargetView(Dx12->SwapBuffers[Index], nullptr, Handle);
            Handle.ptr += Dx12->DescriptorSizeRtv;
        }
    }

    VHR(Dx12->Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, Dx12->CmdAlloc[0], nullptr, IID_PPV_ARGS(&Dx12->CmdList)));
    VHR(Dx12->CmdList->Close());

    VHR(Dx12->Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&Dx12->FrameFence)));
    Dx12->FrameFenceEvent = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
}

static void
Shutdown(dx12_context *Dx12)
{
    SAFE_RELEASE(Dx12->CmdList);
    SAFE_RELEASE(Dx12->CmdAlloc[0]);
    SAFE_RELEASE(Dx12->CmdAlloc[1]);
    SAFE_RELEASE(Dx12->SwapBufferHeap);
    for (int Index = 0; Index < 4; ++Index)
        SAFE_RELEASE(Dx12->SwapBuffers[Index]);
    CloseHandle(Dx12->FrameFenceEvent);
    SAFE_RELEASE(Dx12->FrameFence);
    SAFE_RELEASE(Dx12->SwapChain);
    SAFE_RELEASE(Dx12->CmdQueue);
    SAFE_RELEASE(Dx12->Device);
}

static void
Present(dx12_context *Dx12)
{
    Dx12->SwapChain->Present(0, 0);
    Dx12->CmdQueue->Signal(Dx12->FrameFence, ++Dx12->FrameCount);

    const uint64_t GpuFrameCount = Dx12->FrameFence->GetCompletedValue();

    if ((Dx12->FrameCount - GpuFrameCount) >= 2)
    {
        Dx12->FrameFence->SetEventOnCompletion(GpuFrameCount + 1, Dx12->FrameFenceEvent);
        WaitForSingleObject(Dx12->FrameFenceEvent, INFINITE);
    }

    Dx12->FrameIndex = !Dx12->FrameIndex;
    Dx12->BackBufferIndex = Dx12->SwapChain->GetCurrentBackBufferIndex();
}

static void
Flush(dx12_context *Dx12)
{
    Dx12->CmdQueue->Signal(Dx12->FrameFence, ++Dx12->FrameCount);
    Dx12->FrameFence->SetEventOnCompletion(Dx12->FrameCount, Dx12->FrameFenceEvent);
    WaitForSingleObject(Dx12->FrameFenceEvent, INFINITE);
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
UpdateFrameStats(HWND Window, const char *Name, double *Time, float *DeltaTime)
{
    static double PreviousTime = -1.0;
    static double HeaderRefreshTime = 0.0;
    static uint32_t FrameCount = 0;

    if (PreviousTime < 0.0)
    {
        PreviousTime = GetTime();
        HeaderRefreshTime = PreviousTime;
    }

    *Time = GetTime();
    *DeltaTime = *Time - PreviousTime;
    PreviousTime = *Time;

    if ((*Time - HeaderRefreshTime) >= 1.0)
    {
        const double FramesPerSecond = FrameCount / (*Time - HeaderRefreshTime);
        const double MilliSeconds = (1.0 / FramesPerSecond) * 1000.0;
        char Header[256];
        snprintf(Header, sizeof(Header), "[%.1f fps  %.3f ms] %s", FramesPerSecond, MilliSeconds, Name);
        SetWindowText(Window, Header);
        HeaderRefreshTime = *Time;
        FrameCount = 0;
    }
    FrameCount++;
}

static LRESULT CALLBACK
ProcessWindowMessage(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_KEYDOWN:
        if (wparam == VK_ESCAPE)
        {
            PostQuitMessage(0);
            return 0;
        }
        break;
    }
    return DefWindowProc(window, message, wparam, lparam);
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
Draw(dx12_context *Dx12)
{
    ID3D12CommandAllocator *CmdAlloc = Dx12->CmdAlloc[Dx12->FrameIndex];
    ID3D12GraphicsCommandList *CmdList = Dx12->CmdList;

    CmdAlloc->Reset();
    CmdList->Reset(CmdAlloc, nullptr);

    CmdList->RSSetViewports(1, &CD3DX12_VIEWPORT(0.0f, 0.0f, (float)Dx12->Resolution[0], (float)Dx12->Resolution[1]));
    CmdList->RSSetScissorRects(1, &CD3DX12_RECT(0, 0, Dx12->Resolution[0], Dx12->Resolution[1]));

    CmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(Dx12->SwapBuffers[Dx12->BackBufferIndex],
                                                                      D3D12_RESOURCE_STATE_PRESENT,
                                                                      D3D12_RESOURCE_STATE_RENDER_TARGET));

    D3D12_CPU_DESCRIPTOR_HANDLE BackBufferDescriptor = Dx12->SwapBufferHeapStart;
    BackBufferDescriptor.ptr += Dx12->BackBufferIndex * Dx12->DescriptorSizeRtv;

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
    CmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(Dx12->SwapBuffers[Dx12->BackBufferIndex],
                                                                      D3D12_RESOURCE_STATE_RENDER_TARGET,
                                                                      D3D12_RESOURCE_STATE_PRESENT));
    VHR(CmdList->Close());

    Dx12->CmdQueue->ExecuteCommandLists(1, (ID3D12CommandList **)&CmdList);
}

static void
Initialize(const dx12_context *Dx12, dx12_resources *Data)
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

    dx12_context Dx12 = {};
    dx12_resources Resources = {};

    const char *Name = "Fractal Flames";
    Dx12.Window = InitializeWindow(Name, 1280, 720);
    InitializeDx12(&Dx12);
    Initialize(&Dx12, &Resources);

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
            UpdateFrameStats(Dx12.Window, Name, &Time, &DeltaTime);
            Draw(&Dx12);
            Present(&Dx12);
        }
    }

    return 0;
}
// vim: set ts=4 sw=4 expandtab:

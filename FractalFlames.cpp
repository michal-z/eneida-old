#include "FractalFlamesPch.h"

#define k_DemoName "Fractal Flames"
#define k_DemoResolutionX 1280
#define k_DemoResolutionY 720

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
    uint32_t DescriptorSize;
    uint32_t DescriptorSizeRtv;
    uint32_t FrameIndex;
    uint32_t BackBufferIndex;
    uint64_t FrameCount;
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
    SAFE_RELEASE(demo.cmdAlloc[0]);
    SAFE_RELEASE(demo.cmdAlloc[1]);
    SAFE_RELEASE(demo.swapBufferHeap);
    for (int i = 0; i < 4; ++i)
        SAFE_RELEASE(demo.swapBuffers[i]);
    CloseHandle(demo.frameFenceEvent);
    SAFE_RELEASE(demo.frameFence);
    SAFE_RELEASE(demo.swapChain);
    SAFE_RELEASE(demo.cmdQueue);
    SAFE_RELEASE(demo.device);
}

static void
Present(Demo& demo)
{
    demo.swapChain->Present(0, 0);
    demo.cmdQueue->Signal(demo.frameFence, ++demo.frameCount);

    const uint64_t deviceFrameCount = demo.frameFence->GetCompletedValue();

    if ((demo.frameCount - deviceFrameCount) >= 2)
    {
        demo.frameFence->SetEventOnCompletion(deviceFrameCount + 1, demo.frameFenceEvent);
        WaitForSingleObject(demo.frameFenceEvent, INFINITE);
    }

    demo.frameIndex = !demo.frameIndex;
    demo.backBufferIndex = demo.swapChain->GetCurrentBackBufferIndex();
}

static void
Flush(Demo& demo)
{
    demo.cmdQueue->Signal(demo.frameFence, ++demo.frameCount);
    demo.frameFence->SetEventOnCompletion(demo.frameCount, demo.frameFenceEvent);
    WaitForSingleObject(demo.frameFenceEvent, INFINITE);
}

static double
GetTime()
{
    static LARGE_INTEGER startCounter;
    static LARGE_INTEGER frequency;
    if (startCounter.QuadPart == 0)
    {
        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&startCounter);
    }
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (counter.QuadPart - startCounter.QuadPart) / (double)frequency.QuadPart;
}

static void
UpdateFrameTime(HWND window, double& o_Time, double& o_DeltaTime)
{
    static double lastTime = -1.0;
    static double lastFpsTime = 0.0;
    static unsigned frameCount = 0;

    if (lastTime < 0.0)
    {
        lastTime = GetTime();
        lastFpsTime = lastTime;
    }

    o_Time = GetTime();
    o_DeltaTime = o_Time - lastTime;
    lastTime = o_Time;

    if ((o_Time - lastFpsTime) >= 1.0)
    {
        const double fps = frameCount / (o_Time - lastFpsTime);
        const double ms = (1.0 / fps) * 1000.0;
        char text[256];
        snprintf(text, sizeof(text), "[%.1f fps  %.3f ms] %s", fps, ms, k_DemoName);
        SetWindowText(window, text);
        lastFpsTime = o_Time;
        frameCount = 0;
    }
    frameCount++;
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

static void
InitializeWindow(Demo& demo)
{
    WNDCLASS winclass = {};
    winclass.lpfnWndProc = ProcessWindowMessage;
    winclass.hInstance = GetModuleHandle(nullptr);
    winclass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    winclass.lpszClassName = k_DemoName;
    if (!RegisterClass(&winclass))
        assert(0);

    RECT rect = { 0, 0, k_DemoResolutionX, k_DemoResolutionY };
    if (!AdjustWindowRect(&rect, WS_OVERLAPPED | WS_SYSMENU | WS_CAPTION | WS_MINIMIZEBOX, 0))
        assert(0);

    demo.window = CreateWindowEx(
        0, k_DemoName, k_DemoName, WS_OVERLAPPED | WS_SYSMENU | WS_CAPTION | WS_MINIMIZEBOX | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left, rect.bottom - rect.top,
        nullptr, nullptr, nullptr, 0);
    assert(demo.window);
}

static void
Draw(Demo& demo)
{
    ID3D12CommandAllocator* cmdAlloc = demo.cmdAlloc[demo.frameIndex];
    ID3D12GraphicsCommandList* cl = demo.cmdList;

    cmdAlloc->Reset();
    cl->Reset(cmdAlloc, nullptr);

    cl->RSSetViewports(1, &CD3DX12_VIEWPORT(0.0f, 0.0f, (float)k_DemoResolutionX, (float)k_DemoResolutionY));
    cl->RSSetScissorRects(1, &CD3DX12_RECT(0, 0, k_DemoResolutionX, k_DemoResolutionY));

    cl->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(demo.swapBuffers[demo.backBufferIndex],
                                                                 D3D12_RESOURCE_STATE_PRESENT,
                                                                 D3D12_RESOURCE_STATE_RENDER_TARGET));

    D3D12_CPU_DESCRIPTOR_HANDLE backBufferDescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(demo.swapBufferHeapStart,
                                                                                     demo.backBufferIndex,
                                                                                     demo.descriptorSizeRtv);
    const float clearColor[4] = { 0.0f, 0.2f, 0.4f, 1.0f };

    cl->OMSetRenderTargets(1, &backBufferDescriptor, 0, nullptr);
    cl->ClearRenderTargetView(backBufferDescriptor, clearColor, 0, nullptr);

    cl->SetPipelineState(demo.pso);
    cl->SetGraphicsRootSignature(demo.rootSig);
    cl->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);

    for (uint32_t i = 0; i < 100000; ++i)
    {
        float p[2] = { Randomf(-0.7f, 0.7f), Randomf(-0.7f, 0.7f) };
        cl->SetGraphicsRoot32BitConstants(0, 2, p, 0);
        cl->DrawInstanced(1, 1, 0, 0);
    }
    cl->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(demo.swapBuffers[demo.backBufferIndex],
                                                                 D3D12_RESOURCE_STATE_RENDER_TARGET,
                                                                 D3D12_RESOURCE_STATE_PRESENT));
    VHR(cl->Close());

    demo.cmdQueue->ExecuteCommandLists(1, (ID3D12CommandList**)&cl);
}

static void
Initialize(Demo& demo)
{
    /* pso */ {
        std::vector<uint8_t> vsCode = LoadFile("VsTransform.cso");
        std::vector<uint8_t> psCode = LoadFile("PsShade.cso");

        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.VS = { vsCode.data(), vsCode.size() };
        psoDesc.PS = { psCode.data(), psCode.size() };
        psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        psoDesc.SampleMask = 0xffffffff;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.SampleDesc.Count = 1;

        VHR(demo.device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&demo.pso)));
        VHR(demo.device->CreateRootSignature(0, vsCode.data(), vsCode.size(), IID_PPV_ARGS(&demo.rootSig)));
    }
}

int CALLBACK
WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    SetProcessDPIAware();

    Demo demo = {};
    InitializeWindow(demo);
    InitializeDx12(demo);
    Initialize(demo);

    for (;;)
    {
        MSG msg = {};
        if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
        {
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                break;
        }
        else
        {
            double time, deltaTime;
            UpdateFrameTime(demo.window, time, deltaTime);
            Draw(demo);
            Present(demo);
        }
    }

    return 0;
}
// vim: set ts=4 sw=4 expandtab:

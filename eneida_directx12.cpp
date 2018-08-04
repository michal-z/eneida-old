#define k_MaxNonShaderDescriptorCount 1000
#define k_MaxShaderDescriptorCount 1000

static void
InitializeDirectX12(directx12 &Dx)
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
ShutdownDirectX12(directx12 &Dx)
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
PresentFrame(directx12 &Dx)
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
FlushGpu(directx12 &Dx)
{
    Dx.CmdQueue->Signal(Dx.FrameFence, ++Dx.FrameCount);
    Dx.FrameFence->SetEventOnCompletion(Dx.FrameCount, Dx.FrameFenceEvent);
    WaitForSingleObject(Dx.FrameFenceEvent, INFINITE);
}

static void
AllocateNonShaderDescriptors(directx12 &Dx, uint32_t Count, D3D12_CPU_DESCRIPTOR_HANDLE &OutFirst)
{
    assert((Dx.NonShaderHeap.AllocatedCount + Count) < k_MaxNonShaderDescriptorCount);

    OutFirst.ptr = Dx.NonShaderHeap.CpuStart.ptr + Dx.NonShaderHeap.AllocatedCount * Dx.DescriptorSize;

    Dx.NonShaderHeap.AllocatedCount += Count;
}

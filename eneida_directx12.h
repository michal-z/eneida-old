struct descriptor_heap
{
    ID3D12DescriptorHeap* Heap;
    D3D12_CPU_DESCRIPTOR_HANDLE CpuStart;
    D3D12_GPU_DESCRIPTOR_HANDLE GpuStart;
    uint32_t Size;
    uint32_t Capacity;
};

struct directx12
{
    ID3D12Device* Device;
    ID3D12CommandQueue* CmdQueue;
    ID3D12CommandAllocator* CmdAlloc[2];
    ID3D12GraphicsCommandList* CmdList;
    IDXGISwapChain3* SwapChain;
    ID3D12Resource* SwapBuffers[4];
    ID3D12Resource* DepthBuffer;
    ID3D12Fence* FrameFence;
    HANDLE FrameFenceEvent;
    HWND Window;
    uint32_t Resolution[2];
    uint32_t DescriptorSize;
    uint32_t DescriptorSizeRtv;
    uint32_t FrameIndex;
    uint32_t BackBufferIndex;
    uint64_t FrameCount;

    descriptor_heap RenderTargetHeap;
    descriptor_heap DepthStencilHeap;

    // shader visible descriptor heaps
    descriptor_heap ShaderVisibleHeaps[2];

    // non-shader visible descriptor heap
    descriptor_heap NonShaderVisibleHeap;
};
// vim: set ts=4 sw=4 expandtab:

struct shader_descriptor_heap
{
    ID3D12DescriptorHeap *Heap;
    D3D12_CPU_DESCRIPTOR_HANDLE CpuStart;
    D3D12_GPU_DESCRIPTOR_HANDLE GpuStart;
    uint32_t AllocatedCount;
};

struct nonshader_descriptor_heap
{
    ID3D12DescriptorHeap *Heap;
    D3D12_CPU_DESCRIPTOR_HANDLE CpuStart;
    uint32_t AllocatedCount;
};

struct directx12
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

    // shader visible descriptor heaps
    shader_descriptor_heap ShaderHeaps[2];

    // shader non-visible descriptor heap
    nonshader_descriptor_heap NonShaderHeap;
};

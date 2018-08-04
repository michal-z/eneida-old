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

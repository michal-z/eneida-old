namespace FractalFlames
{

struct demo
{
    int x;
};

static void
Initialize(demo& Demo, directx12& Dx)
{
}

static void
Shutdown(demo& Demo, directx12& Dx)
{
}

static void
Update(demo& Demo, directx12& Dx, double FrameTime, float FrameDeltaTime)
{
    ImGui::ShowDemoWindow();
}

} // namespace FractalFlames
// vim: set ts=4 sw=4 expandtab:

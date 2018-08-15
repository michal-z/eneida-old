namespace Test1
{

struct test
{
    int x;
};

static void
Initialize(test& Test, directx12& Dx)
{
}

static void
Shutdown(test& Test, directx12& Dx)
{
}

static void
Update(test& Test, directx12& Dx, double FrameTime, float FrameDeltaTime)
{
    ImGui::ShowDemoWindow();
}

} // namespace FractalFlames
// vim: set ts=4 sw=4 expandtab:

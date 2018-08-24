namespace Test1
{

struct test1
{
    int x;
};

static bool
Initialize(test_dispatch_table& Dispatch, directx12& Dx)
{
    test1& Test = *(test1*)Dispatch.Data;
    return true;
}

static void
Shutdown(test_dispatch_table& Dispatch, directx12& Dx)
{
    test1& Test = *(test1*)Dispatch.Data;
}

static void
Update(test_dispatch_table& Dispatch, directx12& Dx, double Time, float DeltaTime)
{
    test1& Test = *(test1*)Dispatch.Data;

    ImGui::ShowDemoWindow();
}

static test_dispatch_table
GetDispatchTable()
{
    static test1 TestStorage = {};
    test_dispatch_table DispatchTable = {};
    DispatchTable.Data = &TestStorage;
    DispatchTable.Initialize = Initialize;
    DispatchTable.Shutdown = Shutdown;
    DispatchTable.Update = Update;
    return DispatchTable;
}

} // namespace Test1

static std::vector<test_dispatch_table>
GetAllTests()
{
    std::vector<test_dispatch_table> Tests;
    Tests.push_back(Test1::GetDispatchTable());
    return Tests;
}
// vim: set ts=4 sw=4 expandtab:

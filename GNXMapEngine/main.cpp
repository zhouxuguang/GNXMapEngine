#include "MapApplication.h"
#include "Runtime/GNXEngine/include/GNXMain.h"

int main(int argc, char *argv[])
{
    GNXEngine::WindowProps props("GNXMapEngine", 1280U, 720U);
    MapApplication app(props);
    app.RunLoop();
    // 自动化截图失败时返回非 0，便于批处理脚本判断
    return app.GetExitCode();
}

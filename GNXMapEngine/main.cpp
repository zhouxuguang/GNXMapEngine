#include "MapApplication.h"
#include "Runtime/GNXEngine/include/GNXMain.h"

int main(int argc, char *argv[])
{
    GNXEngine::WindowProps props("GNXMapEngine", 800U, 600U);
    MapApplication app(props);
    app.RunLoop();
    return 0;
}

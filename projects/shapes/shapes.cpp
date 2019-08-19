#include <array>
#include <vector>

#include <Windows.h>

#include "../common/BaseApp.h"
#include "../common/d3dx12.h"

using namespace std;

class ShapesDemo : public BaseApp
{
public:
    ShapesDemo(HINSTANCE hInstance);
protected:
private:
    void Update(const BaseTimer& gt);
    void Draw(const BaseTimer& gt);
};



ShapesDemo::ShapesDemo(HINSTANCE hInstance)
:
BaseApp(hInstance)
{
}



void ShapesDemo::Update(const BaseTimer& gt)
{

}


void ShapesDemo::Draw(const BaseTimer& gt)
{

}


// Write the win main func here
int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   PSTR      pCmdLine,
                   int       nShowCmd)
{
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    ShapesDemo demoApp(hInstance);
    
    int retCode = 0;
    if (demoApp.Initialize() == 0)
    {
        retCode = demoApp.Run();
    }
    else
    {
        cout << "Error initializing the shapes app...\n";
    }

    return retCode;
}


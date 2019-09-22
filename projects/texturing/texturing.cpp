#include <iostream>

#include "windows.h"

#include "BaseApp.h"

using namespace std;

// ====================================================================================================================
class TextureDemo : public BaseApp
{
public:
    TextureDemo(HINSTANCE hInstance)
        :
        BaseApp(hInstance)
    {}
    virtual void Update(const BaseTimer& timer) override;
    virtual void Draw(const BaseTimer& timer) override;

    bool Initialize() override;
private:
};

// ====================================================================================================================
void TextureDemo::Update(const BaseTimer& timer)
{

}

// ====================================================================================================================
void TextureDemo::Draw(const BaseTimer& timer)
{

}

// ====================================================================================================================
bool TextureDemo::Initialize()
{
    bool result = true;

    result = BaseApp::Initialize();
    if (result == true)
    {
        
    }

    return result;
}
// ====================================================================================================================
// Write the win main func here
int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   PSTR      pCmdLine,
                   int       nShowCmd)
{
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    TextureDemo demoApp(hInstance);

    int retCode = 0;
    if (demoApp.Initialize() == true)
    {
        retCode = demoApp.Run();
    }

    return retCode;
}




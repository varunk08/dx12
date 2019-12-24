#include <iostream>
#include <vector>
#include "DirectXColors.h"
#include "windows.h"
#include "BaseApp.h"

using namespace std;
using Microsoft::WRL::ComPtr;
using namespace DirectX;

class BlendApp : public BaseApp
{
public:
  BlendApp(HINSTANCE hInst);

  // Delete copy and assign.
  BlendApp(const BlendApp& app) = delete;
  BlendApp& operator=(const BlendApp& app) = delete;
  
  bool Initialize() override;

private:
  virtual void Update(const BaseTimer& timer) override;
  virtual void Draw(const BaseTimer& timer) override;
  
  virtual void OnMouseDown(WPARAM btnState, int x, int y) override { }
  virtual void OnMouseUp(WPARAM btnState, int x, int y) override { }
  virtual void OnMouseMove(WPARAM btnState, int x, int y) override { }
  virtual void OnKeyDown(WPARAM wparam) override {}
  
 
};

BlendApp::BlendApp(HINSTANCE h_inst)
  :
  BaseApp(h_inst)
{
  
}

bool BlendApp::Initialize()
{
  bool ret = BaseApp::Initialize();

  if (ret == true) {
  }

  return ret;
}

void BlendApp::Update(const BaseTimer& timer)
{
}

void BlendApp::Draw(const BaseTimer& timer)
{
}
  

int WINAPI
WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   PSTR      pCmdLine,
                   int       nShowCmd)
{
#if defined(DEBUG) | defined(_DEBUG)
  _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
  
  auto ret_code = 0;
  BlendApp demo_app(hInstance);

  if (demo_app.Initialize() == true) {
    ret_code = demo_app.Run();
  }
  
  return ret_code;
}

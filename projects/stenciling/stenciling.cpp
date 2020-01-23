#include <iostream>
#include <vector>
#include "BaseApp.h"

using namespace std;

// Our stenciling demo app, derived from the BaseApp ofcourse.
class StencilDemo final : public BaseApp
{
public:
  // Delete default and copy constructors, and copy operator.
  StencilDemo& operator=(const StencilDemo& other) = delete;
  StencilDemo(const StencilDemo& other) = delete;
  StencilDemo() = delete;

  // Constructor.
  StencilDemo(HINSTANCE hInstance)
    :
    BaseApp(hInstance)
  {
  }

  // Destructor.
  ~StencilDemo()
  {
    if (m_d3dDevice != nullptr)
      {
        FlushCommandQueue();
      }
  }
  
  virtual void OnResize()override
  {}
  
  virtual void Update(const BaseTimer& gt)override{}
  virtual void Draw(const BaseTimer& gt)override{}

  virtual void OnMouseDown(WPARAM btnState, int x, int y)override{}
  virtual void OnMouseUp(WPARAM btnState, int x, int y)override{}
  virtual void OnMouseMove(WPARAM btnState, int x, int y)override{}

  virtual bool Initialize() override
  {
    bool result = true;

    return result;
  }


  
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR cmdLine, int showCmd)
{
#if defined(DEBUG) | defined(_DEBUG)
  _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

  try
    {
      StencilDemo demoApp(hInstance);
      if (demoApp.Initialize() == false)
        {
          return 0;
        }
      return demoApp.Run();
    }
  catch (DxException& e)
    {
      MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
      return 0;
    }
  
}


/************************************************************************

Stenciling demo app.
TODO:
- Write WinMain
- Demo class
- Initialize function
- Load basic 3D geometry
- Camera controls
- Implement lighting
- Implement texturing
- Implement shadows
- Implement stenciling mirror.

 ***********************************************************************/

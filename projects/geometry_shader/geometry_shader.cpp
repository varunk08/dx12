#include <iostream>
#include <vector>
#include <array>
#include "DirectXColors.h"
#include "../common/BaseUtil.h"
#include "../common/BaseApp.h"
#include "../common/GeometryGenerator.h"
#include "../common/UploadBuffer.h"
#include "../common/DDSTextureLoader.h"

using Microsoft::WRL::ComPtr;
using namespace std;
using namespace DirectX;

// Our stenciling demo app, derived from the BaseApp ofcourse.
class GSDemo final : public BaseApp
{
public:
  // Delete default and copy constructors, and copy operator.
  GSDemo& operator=(const GSDemo& other) = delete;
  GSDemo(const GSDemo& other) = delete;
  GSDemo() = delete;

  // Constructor.
  GSDemo(HINSTANCE hInstance)
    :
    BaseApp(hInstance)
  {
  }

  // Destructor.
  ~GSDemo()
  {
    if (m_d3dDevice != nullptr) {
        FlushCommandQueue();
    }
  }

  virtual void OnResize()override
  {
  }

  void UpdateObjectCBs()
  {

  }
  void UpdateCamera(const BaseTimer& gt)
  {
  }

  void UpdatePassCB()
  {
  }

  virtual void Update(const BaseTimer& gt)override
  {
  }

  virtual void Draw(const BaseTimer& gt)override
  {

  }

  virtual void OnMouseDown(WPARAM btnState, int x, int y)override
  {
  }

  virtual void OnMouseUp(WPARAM btnState, int x, int y)override
  {
  }

  virtual void OnMouseMove(WPARAM btnState, int x, int y)override
  {
  }

  void LoadTextures()
  {
  }

  void BuildSceneGeometry()
  {

  }

  void BuildRenderObjects()
  {
  }

  void BuildMaterials()
  {
  }

  void BuildShadersAndInputLayout()
  {

  }

  void BuildRootSignature()
  {
  }

  // Builds the constant buffers etc needed in the shader program.
  void InitShaderResources()
  {

  }

  // Build descriptor heaps and shader resource views for our textures.
  void BuildShaderViews()
  {
  }

  // Builds shaders, descriptors and pipelines.
  void BuildPipelines()
  {

  }

  virtual bool Initialize() override
  {
  }

  // Member variables.
}; // Class StencilDemo

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR cmdLine, int showCmd)
{
#if defined(DEBUG) | defined(_DEBUG)
  _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

  try
    {
      GSDemo demoApp(hInstance);
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

Geometry shader app demo:

- clear screen
- render points randomly
- convert points to quads using the GS
- texture the quads
- optionally animate the textures

 ***********************************************************************/

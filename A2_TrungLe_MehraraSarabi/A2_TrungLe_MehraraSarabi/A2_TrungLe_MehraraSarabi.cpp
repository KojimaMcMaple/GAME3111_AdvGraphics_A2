//***************************************************************************************
// GAME3111_A2_TrungLe_MehraraSarabi
//
// Hold down '1' key to view scene in wireframe mode.
//***************************************************************************************

#include "../../Common/d3dApp.h"
#include "../../Common/MathHelper.h"
#include "../../Common/UploadBuffer.h"
#include "../../Common/GeometryGenerator.h"
#include "FrameResource.h"
#include "Waves.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")

const int gNumFrameResources = 3;

enum class ShapeType {
	kBox = 0,
	kSphere,
	kGeosphere,
	kCylinder,
	kGrid,
	kQuad,
	kDiamond,
	kUndefined
};

enum class RenderLayer : int
{
    Opaque = 0,
    Transparent,
    AlphaTested,
    AlphaTestedTreeSprites,
    Count
};

// Lightweight structure stores parameters to draw a shape.  This will
// vary from app-to-app.
struct RenderItem
{
    RenderItem() = default;
    RenderItem(const RenderItem& rhs) = delete;

    // World matrix of the shape that describes the object's local space
    // relative to the world space, which defines the position, orientation,
    // and scale of the object in the world.
    XMFLOAT4X4 World = MathHelper::Identity4x4();

    XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

    // Dirty flag indicating the object data has changed and we need to update the constant buffer.
    // Because we have an object cbuffer for each FrameResource, we have to apply the
    // update to each FrameResource.  Thus, when we modify obect data we should set 
    // NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
    int NumFramesDirty = gNumFrameResources;

    // Index into GPU constant buffer corresponding to the ObjectCB for this render item.
    UINT ObjCBIndex = -1;

    Material* Mat = nullptr;
    MeshGeometry* Geo = nullptr;

    // Primitive topology.
    //D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    // DrawIndexedInstanced parameters.
    UINT IndexCount = 0;
    UINT StartIndexLocation = 0;
    int BaseVertexLocation = 0;
};

class ShapesApp : public D3DApp
{
public:
    ShapesApp(HINSTANCE hInstance);
    ShapesApp(const ShapesApp& rhs) = delete;
    ShapesApp& operator=(const ShapesApp& rhs) = delete;
    ~ShapesApp();

    virtual bool Initialize()override;

private:
    virtual void OnResize()override;
    virtual void Update(const GameTimer& gt)override;
    virtual void Draw(const GameTimer& gt)override;

    virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
    virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
    virtual void OnMouseMove(WPARAM btnState, int x, int y)override;

    void OnKeyboardInput(const GameTimer& gt);
	void UpdateCamera(const GameTimer& gt);
    void AnimateMaterials(const GameTimer& gt);
	void UpdateObjectCBs(const GameTimer& gt);
    void UpdateMaterialCBs(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);
    void UpdateWaves(const GameTimer& gt);

    void LoadTextures();
    void BuildRootSignature();
    void BuildDescriptorHeaps();
    void BuildShadersAndInputLayout();
    void BuildLandGeometry();
    void BuildWavesGeometry();
    void BuildOneShapeGeometry(std::string shape_type, std::string shape_name, float param_a, float param_b, float param_c, float param_d = -999, float param_e = -999);
    void BuildShapeGeometry();
    void BuildTreeSpritesGeometry();
    void BuildPSOs();
    void BuildFrameResources();
    void BuildMaterials();
    void BuildRenderItems();
    void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);
    void BuildConstantBufferViews();
    void BuildOneRenderItem(std::string shape_type, std::string shape_name, std::string material, XMMATRIX scale_matrix, XMMATRIX translate_matrix, XMMATRIX tex_scale_matrix, UINT obj_idx);
    void BuildOneRenderItem(std::string shape_type, std::string shape_name, std::string material, XMMATRIX rotate_matrix, XMMATRIX scale_matrix, XMMATRIX translate_matrix, XMMATRIX tex_scale_matrix, UINT obj_idx);
    
    std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();

    float GetHillsHeight(float x, float z)const;
    XMFLOAT3 GetHillsNormal(float x, float z)const;
private:

    std::vector<std::unique_ptr<FrameResource>> mFrameResources;
    FrameResource* mCurrFrameResource = nullptr;
    int mCurrFrameResourceIndex = 0;

    UINT mCbvSrvDescriptorSize = 0;

    ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
    ComPtr<ID3D12DescriptorHeap> mCbvHeap = nullptr; //OBSOLETE

	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
    std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
    std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
    std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;

    //std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
    std::vector<D3D12_INPUT_ELEMENT_DESC> mStdInputLayout;
    std::vector<D3D12_INPUT_ELEMENT_DESC> mTreeSpriteInputLayout;

    RenderItem* mWavesRitem = nullptr;

	// List of all the render items.
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;

	// Render items divided by PSO.
	std::vector<RenderItem*> mRitemLayer[(int)RenderLayer::Count];

    std::unique_ptr<Waves> mWaves;

    PassConstants mMainPassCB;

    UINT mPassCbvOffset = 0; //OBSOLETE

    bool mIsWireframe = false;

	XMFLOAT3 mEyePos = { 0.0f, 0.0f, 0.0f };
	XMFLOAT4X4 mView = MathHelper::Identity4x4();
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();

    float mTheta = 1.5f*XM_PI;
    float mPhi = 0.2f*XM_PI;
    float mRadius = 120.0f;

    POINT mLastMousePos;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
    PSTR cmdLine, int showCmd)
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    try
    {
        ShapesApp theApp(hInstance);
        if(!theApp.Initialize())
            return 0;

        return theApp.Run();
    }
    catch(DxException& e)
    {
        MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
        return 0;
    }
}

ShapesApp::ShapesApp(HINSTANCE hInstance)
    : D3DApp(hInstance)
{
}

ShapesApp::~ShapesApp()
{
    if(md3dDevice != nullptr)
        FlushCommandQueue();
}

bool ShapesApp::Initialize()
{
    ::OutputDebugStringA(">>> Init started...\n");
    
    if(!D3DApp::Initialize())
        return false;

    // Reset the command list to prep for initialization commands.
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

    // Get the increment size of a descriptor in this heap type.  This is hardware specific, 
    // so we have to query this information.
    mCbvSrvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    
    mWaves = std::make_unique<Waves>(128, 128, 1.0f, 0.03f, 4.0f, 0.2f);

    LoadTextures();
    BuildRootSignature();
    BuildDescriptorHeaps();
    BuildShadersAndInputLayout();
    BuildLandGeometry();
    BuildWavesGeometry();
    BuildShapeGeometry();
    BuildTreeSpritesGeometry();
    BuildMaterials();
    BuildRenderItems();
    BuildFrameResources();
    //BuildConstantBufferViews();
    BuildPSOs();

    // Execute the initialization commands.
    ThrowIfFailed(mCommandList->Close());
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // Wait until initialization is complete.
    FlushCommandQueue();

    ::OutputDebugStringA(">>> Init DONE!\n");

    return true;
}
 
void ShapesApp::OnResize()
{
    D3DApp::OnResize();

    // The window resized, so update the aspect ratio and recompute the projection matrix.
    XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f*MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
    XMStoreFloat4x4(&mProj, P);
}

void ShapesApp::Update(const GameTimer& gt)
{
    ::OutputDebugStringA(">>> Update started...\n");
    OnKeyboardInput(gt);
	UpdateCamera(gt);

    // Cycle through the circular frame resource array.
    mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
    mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

    // Has the GPU finished processing the commands of the current frame resource?
    // If not, wait until the GPU has completed commands up to this fence point.
    if(mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
    {
        HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
        ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }

    AnimateMaterials(gt);
    UpdateObjectCBs(gt);
    UpdateMaterialCBs(gt);
    UpdateMainPassCB(gt);
    UpdateWaves(gt);
    ::OutputDebugStringA(">>> Update DONE!\n");
}

void ShapesApp::Draw(const GameTimer& gt)
{
    ::OutputDebugStringA(">>> Draw started...\n");
    auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

    // Reuse the memory associated with command recording.
    // We can only reset when the associated command lists have finished execution on the GPU.
    ThrowIfFailed(cmdListAlloc->Reset());

    // A command list can be reset after it has been added to the command queue via ExecuteCommandList.
    // Reusing the command list reuses memory.
    if(mIsWireframe)
    {
        ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque_wireframe"].Get()));
    }
    else
    {
        ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));
    }

    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    // Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    // Clear the back buffer and depth buffer.
    //mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
    mCommandList->ClearRenderTargetView(CurrentBackBufferView(), (float*)&mMainPassCB.FogColor, 0, nullptr);
    mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    // Specify the buffers we are going to render to.
    mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

    /*ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvHeap.Get() };
    mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

    int passCbvIndex = mPassCbvOffset + mCurrFrameResourceIndex;
    auto passCbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
    passCbvHandle.Offset(passCbvIndex, mCbvSrvUavDescriptorSize);
    mCommandList->SetGraphicsRootDescriptorTable(1, passCbvHandle);*/

    ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
    mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

    auto passCB = mCurrFrameResource->PassCB->Resource();
    mCommandList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());

    DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);

    mCommandList->SetPipelineState(mPSOs["alphaTested"].Get());
    DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::AlphaTested]);

    mCommandList->SetPipelineState(mPSOs["treeSprites"].Get());
    DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::AlphaTestedTreeSprites]);

    mCommandList->SetPipelineState(mPSOs["transparent"].Get());
    DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Transparent]);

    // Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    // Done recording commands.
    ThrowIfFailed(mCommandList->Close());

    // Add the command list to the queue for execution.
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // Swap the back and front buffers
    ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

    // Advance the fence value to mark commands up to this fence point.
    mCurrFrameResource->Fence = ++mCurrentFence;
    
    // Add an instruction to the command queue to set a new fence point. 
    // Because we are on the GPU timeline, the new fence point won't be 
    // set until the GPU finishes processing all the commands prior to this Signal().
    mCommandQueue->Signal(mFence.Get(), mCurrentFence);

    ::OutputDebugStringA(">>> Draw DONE!\n");
}

void ShapesApp::OnMouseDown(WPARAM btnState, int x, int y)
{
    mLastMousePos.x = x;
    mLastMousePos.y = y;

    SetCapture(mhMainWnd);
}

void ShapesApp::OnMouseUp(WPARAM btnState, int x, int y)
{
    ReleaseCapture();
}

void ShapesApp::OnMouseMove(WPARAM btnState, int x, int y)
{
    if((btnState & MK_LBUTTON) != 0)
    {
        // Make each pixel correspond to a quarter of a degree.
        float dx = XMConvertToRadians(0.25f*static_cast<float>(x - mLastMousePos.x));
        float dy = XMConvertToRadians(0.25f*static_cast<float>(y - mLastMousePos.y));

        // Update angles based on input to orbit camera around box.
        mTheta += dx;
        mPhi += dy;

        // Restrict the angle mPhi.
        mPhi = MathHelper::Clamp(mPhi, 0.1f, MathHelper::Pi - 0.1f);
    }
    else if((btnState & MK_RBUTTON) != 0)
    {
        // Make each pixel correspond to 0.2 unit in the scene.
        float dx = 0.05f*static_cast<float>(x - mLastMousePos.x);
        float dy = 0.05f*static_cast<float>(y - mLastMousePos.y);

        // Update the camera radius based on input.
        mRadius += dx - dy;

        // Restrict the radius.
        mRadius = MathHelper::Clamp(mRadius, 5.0f, 300.0f); //EDIT MOUSE DISTANCE
    }

    mLastMousePos.x = x;
    mLastMousePos.y = y;
}
 
void ShapesApp::OnKeyboardInput(const GameTimer& gt)
{
    if(GetAsyncKeyState('1') & 0x8000)
        mIsWireframe = true;
    else
        mIsWireframe = false;
}
 
void ShapesApp::UpdateCamera(const GameTimer& gt)
{
	// Convert Spherical to Cartesian coordinates.
	mEyePos.x = mRadius*sinf(mPhi)*cosf(mTheta);
	mEyePos.z = mRadius*sinf(mPhi)*sinf(mTheta);
	mEyePos.y = mRadius*cosf(mPhi);

	// Build the view matrix.
	XMVECTOR pos = XMVectorSet(mEyePos.x, mEyePos.y, mEyePos.z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&mView, view);
}

void ShapesApp::AnimateMaterials(const GameTimer& gt)
{
    // Scroll the water material texture coordinates.
    auto waterMat = mMaterials["water"].get();

    float& tu = waterMat->MatTransform(3, 0);
    float& tv = waterMat->MatTransform(3, 1);

    tu += 0.1f * gt.DeltaTime();
    tv += 0.02f * gt.DeltaTime();

    if (tu >= 1.0f)
        tu -= 1.0f;

    if (tv >= 1.0f)
        tv -= 1.0f;

    waterMat->MatTransform(3, 0) = tu;
    waterMat->MatTransform(3, 1) = tv;

    // Material has changed, so need to update cbuffer.
    waterMat->NumFramesDirty = gNumFrameResources;
}

void ShapesApp::UpdateObjectCBs(const GameTimer& gt)
{
    auto currObjectCB = mCurrFrameResource->ObjectCB.get();
	for(auto& e : mAllRitems)
	{
		// Only update the cbuffer data if the constants have changed.  
		// This needs to be tracked per frame resource.
		if(e->NumFramesDirty > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&e->World);
            XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
            XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));

			currObjectCB->CopyData(e->ObjCBIndex, objConstants);

			// Next FrameResource need to be updated too.
			e->NumFramesDirty--;
		}
	}
}

void ShapesApp::UpdateMaterialCBs(const GameTimer& gt)
{
    auto currMaterialCB = mCurrFrameResource->MaterialCB.get();
    for (auto& e : mMaterials)
    {
        // Only update the cbuffer data if the constants have changed.  If the cbuffer
        // data changes, it needs to be updated for each FrameResource.
        Material* mat = e.second.get();
        if (mat->NumFramesDirty > 0)
        {
            XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

            MaterialConstants matConstants;
            matConstants.DiffuseAlbedo = mat->DiffuseAlbedo;
            matConstants.FresnelR0 = mat->FresnelR0;
            matConstants.Roughness = mat->Roughness;
            XMStoreFloat4x4(&matConstants.MatTransform, XMMatrixTranspose(matTransform));

            currMaterialCB->CopyData(mat->MatCBIndex, matConstants);

            // Next FrameResource need to be updated too.
            mat->NumFramesDirty--;
        }
    }
}


void ShapesApp::UpdateMainPassCB(const GameTimer& gt)
{
    XMMATRIX view = XMLoadFloat4x4(&mView);
	XMMATRIX proj = XMLoadFloat4x4(&mProj);

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	mMainPassCB.EyePosW = mEyePos;
	mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	mMainPassCB.NearZ = 1.0f;
	mMainPassCB.FarZ = 1000.0f;
	mMainPassCB.TotalTime = gt.TotalTime();
	mMainPassCB.DeltaTime = gt.DeltaTime();
    mMainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
    mMainPassCB.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
    mMainPassCB.Lights[0].Strength = { 0.7f, 0.7f, 0.8f };
    mMainPassCB.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
    mMainPassCB.Lights[1].Strength = { 0.4f, 0.4f, 0.4f };
    mMainPassCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
    mMainPassCB.Lights[2].Strength = { 0.2f, 0.2f, 0.2f };

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);
}

void ShapesApp::LoadTextures() //EDIT TEXTURES HERE
{
    ::OutputDebugStringA(">>> LoadTextures started...\n");
    
    auto bricksTex = std::make_unique<Texture>();
    bricksTex->Name = "bricksTex";
    bricksTex->Filename = L"../../Textures/bricks.dds";
    ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
        mCommandList.Get(), bricksTex->Filename.c_str(),
        bricksTex->Resource, bricksTex->UploadHeap));

    auto stoneTex = std::make_unique<Texture>();
    stoneTex->Name = "stoneTex";
    stoneTex->Filename = L"../../Textures/stone.dds";
    ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
        mCommandList.Get(), stoneTex->Filename.c_str(),
        stoneTex->Resource, stoneTex->UploadHeap));

    auto tileTex = std::make_unique<Texture>();
    tileTex->Name = "tileTex";
    tileTex->Filename = L"../../Textures/tile.dds";
    ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
        mCommandList.Get(), tileTex->Filename.c_str(),
        tileTex->Resource, tileTex->UploadHeap));

    auto waterTex = std::make_unique<Texture>();
    waterTex->Name = "waterTex";
    waterTex->Filename = L"../../Textures/water1.dds";
    ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
        mCommandList.Get(), waterTex->Filename.c_str(),
        waterTex->Resource, waterTex->UploadHeap));

    auto treeArrayTex = std::make_unique<Texture>();
    treeArrayTex->Name = "treeArrayTex";
    treeArrayTex->Filename = L"../../Textures/treeArray.dds";
    ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
        mCommandList.Get(), treeArrayTex->Filename.c_str(),
        treeArrayTex->Resource, treeArrayTex->UploadHeap));

    mTextures[bricksTex->Name] = std::move(bricksTex);
    mTextures[stoneTex->Name] = std::move(stoneTex);
    mTextures[tileTex->Name] = std::move(tileTex);
    mTextures[waterTex->Name] = std::move(waterTex);
    mTextures[treeArrayTex->Name] = std::move(treeArrayTex);

    ::OutputDebugStringA(">>> LoadTextures DONE!\n");
}


void ShapesApp::UpdateWaves(const GameTimer& gt)
{
    // Every quarter second, generate a random wave.
    static float t_base = 0.0f;
    if ((mTimer.TotalTime() - t_base) >= 0.25f)
    {
        t_base += 0.25f;

        int i = MathHelper::Rand(4, mWaves->RowCount() - 5);
        int j = MathHelper::Rand(4, mWaves->ColumnCount() - 5);

        float r = MathHelper::RandF(0.2f, 0.5f);

        mWaves->Disturb(i, j, r);
    }

    // Update the wave simulation.
    mWaves->Update(gt.DeltaTime());

    // Update the wave vertex buffer with the new solution.
    auto currWavesVB = mCurrFrameResource->WavesVB.get();
    for (int i = 0; i < mWaves->VertexCount(); ++i)
    {
        Vertex v;
        v.Pos = mWaves->Position(i);
        v.Normal = mWaves->Normal(i);

        // Derive tex-coords from position by 
        // mapping [-w/2,w/2] --> [0,1]
        v.TexC.x = 0.5f + v.Pos.x / mWaves->Width();
        v.TexC.y = 0.5f - v.Pos.z / mWaves->Depth();

        currWavesVB->CopyData(i, v);
    }

    // Set the dynamic VB of the wave renderitem to the current frame VB.
    mWavesRitem->Geo->VertexBufferGPU = currWavesVB->Resource();
}

//assuming we have n renter items, we can populate the CBV heap with the following code where descriptors 0 to n-
//1 contain the object CBVs for the 0th frame resource, descriptors n to 2n−1 contains the
//object CBVs for 1st frame resource, descriptors 2n to 3n−1 contain the objects CBVs for
//the 2nd frame resource, and descriptors 3n, 3n + 1, and 3n + 2 contain the pass CBVs for the
//0th, 1st, and 2nd frame resource
void ShapesApp::BuildConstantBufferViews()
{
    ::OutputDebugStringA(">>> BuildConstantBufferViews started...\n");
    
    UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

    UINT objCount = (UINT)mRitemLayer[(int)RenderLayer::Count].size();

    // Need a CBV descriptor for each object for each frame resource.
    for(int frameIndex = 0; frameIndex < gNumFrameResources; ++frameIndex)
    {
        auto objectCB = mFrameResources[frameIndex]->ObjectCB->Resource();
        for(UINT i = 0; i < objCount; ++i)
        {
            D3D12_GPU_VIRTUAL_ADDRESS cbAddress = objectCB->GetGPUVirtualAddress();

            // Offset to the ith object constant buffer in the buffer.
            cbAddress += i*objCBByteSize;

            // Offset to the object cbv in the descriptor heap.
            int heapIndex = frameIndex*objCount + i;

			//we can get a handle to the first descriptor in a heap with the ID3D12DescriptorHeap::GetCPUDescriptorHandleForHeapStart
            auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCbvHeap->GetCPUDescriptorHandleForHeapStart());

			//our heap has more than one descriptor,we need to know the size to increment in the heap to get to the next descriptor
			//This is hardware specific, so we have to query this information from the device, and it depends on
			//the heap type.Recall that our D3DApp class caches this information: 	mCbvSrvUavDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            handle.Offset(heapIndex, mCbvSrvUavDescriptorSize);

            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
            cbvDesc.BufferLocation = cbAddress;
            cbvDesc.SizeInBytes = objCBByteSize;

            md3dDevice->CreateConstantBufferView(&cbvDesc, handle);
        }
    }

    UINT passCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(PassConstants));

    // Last three descriptors are the pass CBVs for each frame resource.
    for(int frameIndex = 0; frameIndex < gNumFrameResources; ++frameIndex)
    {
        auto passCB = mFrameResources[frameIndex]->PassCB->Resource();
        D3D12_GPU_VIRTUAL_ADDRESS cbAddress = passCB->GetGPUVirtualAddress();

        // Offset to the pass cbv in the descriptor heap.
        int heapIndex = mPassCbvOffset + frameIndex;
        auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCbvHeap->GetCPUDescriptorHandleForHeapStart());
        handle.Offset(heapIndex, mCbvSrvUavDescriptorSize);

        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
        cbvDesc.BufferLocation = cbAddress;
        cbvDesc.SizeInBytes = passCBByteSize;
        
        md3dDevice->CreateConstantBufferView(&cbvDesc, handle);
    }

    ::OutputDebugStringA(">>> BuildConstantBufferViews DONE!\n");
}

//A root signature defines what resources need to be bound to the pipeline before issuing a draw call and
//how those resources get mapped to shader input registers. there is a limit of 64 DWORDs that can be put in a root signature.
void ShapesApp::BuildRootSignature()
{
    ::OutputDebugStringA(">>> BuildRootSignature started...\n");
    
    CD3DX12_DESCRIPTOR_RANGE texTable;
    texTable.Init(
        D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
        1,  // number of descriptors
        0); // register t0

    // Root parameter can be a table, root descriptor or root constants.
    CD3DX12_ROOT_PARAMETER slotRootParameter[5]; //EDIT

    // Perfomance TIP: Order from most frequent to least frequent.
    slotRootParameter[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
    slotRootParameter[1].InitAsConstantBufferView(0); // register b0
    slotRootParameter[2].InitAsConstantBufferView(1); // register b1
    slotRootParameter[3].InitAsConstantBufferView(2); // register b2
    slotRootParameter[4].InitAsConstantBufferView(3); // register b3 //EDIT

    auto staticSamplers = GetStaticSamplers();

    // A root signature is an array of root parameters.
    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(5, slotRootParameter,
        (UINT)staticSamplers.size(), staticSamplers.data(),
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    // create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
        serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

    if (errorBlob != nullptr)
    {
        ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    }
    ThrowIfFailed(hr);

    ThrowIfFailed(md3dDevice->CreateRootSignature(
        0,
        serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(),
        IID_PPV_ARGS(mRootSignature.GetAddressOf())));

    ::OutputDebugStringA(">>> BuildRootSignature DONE!\n");
}

//If we have 3 frame resources and n render items, then we have three 3n object constant
//buffers and 3 pass constant buffers.Hence we need 3(n + 1) constant buffer views(CBVs).
//Thus we will need to modify our CBV heap to include the additional descriptors :
void ShapesApp::BuildDescriptorHeaps()
{
    ::OutputDebugStringA(">>> BuildDescriptorHeaps started...\n");
    
    //
    // Create the SRV heap.
    //
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = 5; //EDIT NUM OF DESCRIPTORS
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

    //
    // Fill out the heap with actual descriptors.
    //
    CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

    auto bricksTex = mTextures["bricksTex"]->Resource;
    auto stoneTex = mTextures["stoneTex"]->Resource;
    auto tileTex = mTextures["tileTex"]->Resource;
    auto waterTex = mTextures["waterTex"]->Resource;
    auto treeArrayTex = mTextures["treeArrayTex"]->Resource;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = bricksTex->GetDesc().Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = bricksTex->GetDesc().MipLevels;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
    md3dDevice->CreateShaderResourceView(bricksTex.Get(), &srvDesc, hDescriptor);

    // next descriptor
    hDescriptor.Offset(1, mCbvSrvDescriptorSize);

    srvDesc.Format = stoneTex->GetDesc().Format;
    srvDesc.Texture2D.MipLevels = stoneTex->GetDesc().MipLevels;
    md3dDevice->CreateShaderResourceView(stoneTex.Get(), &srvDesc, hDescriptor);

    // next descriptor
    hDescriptor.Offset(1, mCbvSrvDescriptorSize);

    srvDesc.Format = tileTex->GetDesc().Format;
    srvDesc.Texture2D.MipLevels = tileTex->GetDesc().MipLevels;
    md3dDevice->CreateShaderResourceView(tileTex.Get(), &srvDesc, hDescriptor);

    // next descriptor
    hDescriptor.Offset(1, mCbvSrvDescriptorSize);

    srvDesc.Format = waterTex->GetDesc().Format;
    srvDesc.Texture2D.MipLevels = waterTex->GetDesc().MipLevels;
    md3dDevice->CreateShaderResourceView(waterTex.Get(), &srvDesc, hDescriptor);

    // next descriptor
    hDescriptor.Offset(1, mCbvSrvDescriptorSize);

    auto desc = treeArrayTex->GetDesc();
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
    srvDesc.Format = treeArrayTex->GetDesc().Format;
    srvDesc.Texture2DArray.MostDetailedMip = 0;
    srvDesc.Texture2DArray.MipLevels = -1;
    srvDesc.Texture2DArray.FirstArraySlice = 0;
    srvDesc.Texture2DArray.ArraySize = treeArrayTex->GetDesc().DepthOrArraySize;
    md3dDevice->CreateShaderResourceView(treeArrayTex.Get(), &srvDesc, hDescriptor);

    ::OutputDebugStringA(">>> BuildDescriptorHeaps DONE!\n");
}

void ShapesApp::BuildShadersAndInputLayout()
{
    ::OutputDebugStringA(">>> BuildShadersAndInputLayout started...\n");
    
    const D3D_SHADER_MACRO defines[] =
    {
        "FOG", "1",
        NULL, NULL
    };

    const D3D_SHADER_MACRO alphaTestDefines[] =
    {
        "FOG", "1",
        "ALPHA_TEST", "1",
        NULL, NULL
    };

    mShaders["standardVS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "VS", "vs_5_1");
    mShaders["opaquePS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", defines, "PS", "ps_5_1");
    mShaders["alphaTestedPS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", alphaTestDefines, "PS", "ps_5_1");

    mShaders["treeSpriteVS"] = d3dUtil::CompileShader(L"Shaders\\TreeSprite.hlsl", nullptr, "VS", "vs_5_1");
    mShaders["treeSpriteGS"] = d3dUtil::CompileShader(L"Shaders\\TreeSprite.hlsl", nullptr, "GS", "gs_5_1");
    mShaders["treeSpritePS"] = d3dUtil::CompileShader(L"Shaders\\TreeSprite.hlsl", alphaTestDefines, "PS", "ps_5_1");

    mStdInputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    mTreeSpriteInputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "SIZE", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    ::OutputDebugStringA(">>> BuildShadersAndInputLayout DONE!\n");
}

void ShapesApp::BuildLandGeometry()
{
    GeometryGenerator geoGen;
    GeometryGenerator::MeshData grid = geoGen.CreateGrid(160.0f, 160.0f, 50, 50);

    //
    // Extract the vertex elements we are interested and apply the height function to
    // each vertex.  In addition, color the vertices based on their height so we have
    // sandy looking beaches, grassy low hills, and snow mountain peaks.
    //

    std::vector<Vertex> vertices(grid.Vertices.size());
    for (size_t i = 0; i < grid.Vertices.size(); ++i)
    {
        auto& p = grid.Vertices[i].Position;
        vertices[i].Pos = p;
        vertices[i].Pos.y = GetHillsHeight(p.x, p.z);
        vertices[i].Normal = GetHillsNormal(p.x, p.z);
        vertices[i].TexC = grid.Vertices[i].TexC;
    }

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);

    std::vector<std::uint16_t> indices = grid.GetIndices16();
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

    auto geo = std::make_unique<MeshGeometry>();
    geo->Name = "landGeo";

    ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
    CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

    ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
    CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

    geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

    geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

    geo->VertexByteStride = sizeof(Vertex);
    geo->VertexBufferByteSize = vbByteSize;
    geo->IndexFormat = DXGI_FORMAT_R16_UINT;
    geo->IndexBufferByteSize = ibByteSize;

    SubmeshGeometry submesh;
    submesh.IndexCount = (UINT)indices.size();
    submesh.StartIndexLocation = 0;
    submesh.BaseVertexLocation = 0;

    geo->DrawArgs["grid"] = submesh;

    mGeometries["landGeo"] = std::move(geo);
}

void ShapesApp::BuildWavesGeometry()
{
    ::OutputDebugStringA(">>> BuildWavesGeometry started...\n");
    
    std::vector<std::uint16_t> indices(3 * mWaves->TriangleCount()); // 3 indices per face
    assert(mWaves->VertexCount() < 0x0000ffff);

    // Iterate over each quad.
    int m = mWaves->RowCount();
    int n = mWaves->ColumnCount();
    int k = 0;
    for (int i = 0; i < m - 1; ++i)
    {
        for (int j = 0; j < n - 1; ++j)
        {
            indices[k] = i * n + j;
            indices[k + 1] = i * n + j + 1;
            indices[k + 2] = (i + 1) * n + j;

            indices[k + 3] = (i + 1) * n + j;
            indices[k + 4] = i * n + j + 1;
            indices[k + 5] = (i + 1) * n + j + 1;

            k += 6; // next quad
        }
    }

    UINT vbByteSize = mWaves->VertexCount() * sizeof(Vertex);
    UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

    auto geo = std::make_unique<MeshGeometry>();
    geo->Name = "waterGeo";

    // Set dynamically.
    geo->VertexBufferCPU = nullptr;
    geo->VertexBufferGPU = nullptr;

    ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
    CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

    geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

    geo->VertexByteStride = sizeof(Vertex);
    geo->VertexBufferByteSize = vbByteSize;
    geo->IndexFormat = DXGI_FORMAT_R16_UINT;
    geo->IndexBufferByteSize = ibByteSize;

    SubmeshGeometry submesh;
    submesh.IndexCount = (UINT)indices.size();
    submesh.StartIndexLocation = 0;
    submesh.BaseVertexLocation = 0;

    geo->DrawArgs["grid"] = submesh;

    mGeometries["waterGeo"] = std::move(geo);

    ::OutputDebugStringA(">>> BuildWavesGeometry DONE!\n");
}


void ShapesApp::BuildOneShapeGeometry(std::string shape_type, std::string shape_name, float param_a, float param_b, float param_c, float param_d, float param_e) {
    GeometryGenerator geoGen;
    GeometryGenerator::MeshData mesh;
    if (shape_type == "box" || 
        shape_type == "outterWall" ||
        shape_type == "tower" ||
        shape_type == "gate") {
        mesh = geoGen.CreateBox(param_a, param_b, param_c, param_d);
    }
    if (shape_type == "grid") {
        mesh = geoGen.CreateGrid(param_a, param_b, param_c, param_d);
    }
    if (shape_type == "sphere") {
        mesh = geoGen.CreateSphere(param_a, param_b, param_c);
    }
    if (shape_type == "cylinder" ||
        shape_type == "rolo") {
        mesh = geoGen.CreateCylinder(param_a, param_b, param_c, param_d, param_e);
    }
    if (shape_type == "wedge") {
        mesh = geoGen.CreateWedge(param_a, param_b, param_c, param_d);
    }
    if (shape_type == "cone") {
        mesh = geoGen.CreateCone(param_a, param_b, param_c, param_d);
    }
    if (shape_type == "pyramid") {
        mesh = geoGen.CreatePyramid(param_a, param_b, param_c);
    }
    if (shape_type == "truncatedPyramid") {
        mesh = geoGen.CreateTruncatedPyramid(param_a, param_b, param_c, param_d);
    }
    if (shape_type == "diamond" ||
        shape_type == "charm") {
        mesh = geoGen.CreateDiamond(param_a, param_b, param_c, param_d);
    }
    if (shape_type == "prism") {
        mesh = geoGen.CreateTriangularPrism(param_a, param_b, param_c);
    }
    if (shape_type == "torus") {
        mesh = geoGen.CreateTorus(param_a, param_b, param_c, param_d);
    }


    std::vector<Vertex> vertices(mesh.Vertices.size());
    for (size_t i = 0; i < mesh.Vertices.size(); ++i)
    {
        auto& p = mesh.Vertices[i].Position;
        vertices[i].Pos = p;
        vertices[i].Normal = mesh.Vertices[i].Normal;
        vertices[i].TexC = mesh.Vertices[i].TexC;
    }

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);

    std::vector<std::uint16_t> indices = mesh.GetIndices16();
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

    auto geo = std::make_unique<MeshGeometry>();
    geo->Name = shape_name;

    ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
    CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

    ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
    CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

    geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

    geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

    geo->VertexByteStride = sizeof(Vertex);
    geo->VertexBufferByteSize = vbByteSize;
    geo->IndexFormat = DXGI_FORMAT_R16_UINT;
    geo->IndexBufferByteSize = ibByteSize;

    SubmeshGeometry submesh;
    submesh.IndexCount = (UINT)indices.size();
    submesh.StartIndexLocation = 0;
    submesh.BaseVertexLocation = 0;

    geo->DrawArgs[shape_type] = submesh;

    mGeometries[shape_name] = std::move(geo);
}

void ShapesApp::BuildShapeGeometry()
{
    ::OutputDebugStringA(">>> BuildShapeGeometry started...\n");
    
    BuildOneShapeGeometry("box", "boxGeo", 1.0f, 1.0f, 1.0f, 3);
    BuildOneShapeGeometry("outterWall", "outterWallGeo", 1.0f, 1.0f, 1.0f, 3);
    BuildOneShapeGeometry("tower", "towerGeo", 1.0f, 1.0f, 1.0f, 3);
    BuildOneShapeGeometry("gate", "gateGeo", 1.0f, 1.0f, 1.0f, 3);
    BuildOneShapeGeometry("grid", "gridGeo", 70.0f, 70.0f, 60, 40);
    BuildOneShapeGeometry("sphere", "sphereGeo", 0.5f, 20, 20);
    BuildOneShapeGeometry("cylinder", "cylinderGeo", 1.0f, 1.0f, 2.0f, 20, 20);
    BuildOneShapeGeometry("rolo", "roloGeo", 1.0f, 0.5f, 1.0f, 20, 20);
    BuildOneShapeGeometry("wedge", "wedgeGeo", 1.0, 1.0f, 1.0, 3);
    BuildOneShapeGeometry("cone", "coneGeo", 1.0f, 2.0f, 20, 20);
    BuildOneShapeGeometry("pyramid", "pyramidGeo", 1.0, 1.0f, 20);
    BuildOneShapeGeometry("truncatedPyramid", "truncatedPyramidGeo", 1.0, 1.0f, 0.5f, 1);
    BuildOneShapeGeometry("diamond", "diamondGeo", 1.0f, 1.0f, 1.0f, 1);
    BuildOneShapeGeometry("charm", "charmGeo", 1.0f, 1.0f, 1.0f, 1);
    BuildOneShapeGeometry("prism", "prismGeo", 1.0f, 1.0f, 1);
    BuildOneShapeGeometry("torus", "torusGeo", 2.0f, 0.5f, 20, 20);

    // LEGACY CODES
    /*
    GeometryGenerator geoGen;
    GeometryGenerator::MeshData box = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 3); // ADD HERE
    GeometryGenerator::MeshData outterWall = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 3); 
    GeometryGenerator::MeshData tower = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 3); 
    GeometryGenerator::MeshData gate = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 3); 
    GeometryGenerator::MeshData grid = geoGen.CreateGrid(70.0f, 70.0f, 60, 40);
    GeometryGenerator::MeshData sphere = geoGen.CreateSphere(0.5f, 20, 20);
    GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder(1.0f, 1.0f, 2.0f, 20, 20);
    GeometryGenerator::MeshData rolo = geoGen.CreateCylinder(1.0f, 0.5f, 1.0f, 20, 20);
    GeometryGenerator::MeshData wedge = geoGen.CreateWedge(1.0, 1.0f, 1.0, 3); // MARY 1
    GeometryGenerator::MeshData cone = geoGen.CreateCone(1.0f, 2.0f, 20, 20); // MARY 1-2
    GeometryGenerator::MeshData pyramid = geoGen.CreatePyramid(1.0, 1.0f, 20); // MARY 1-3
    GeometryGenerator::MeshData truncatedPyramid = geoGen.CreateTruncatedPyramid(1.0, 1.0f, 0.5f, 1); // MARY 1-3
    GeometryGenerator::MeshData diamond = geoGen.CreateDiamond(1.0f, 1.0f, 1.0f, 1);
    GeometryGenerator::MeshData charm = geoGen.CreateDiamond(1.0f, 1.0f, 1.0f, 1);
    GeometryGenerator::MeshData prism = geoGen.CreateTriangularPrism(1.0f, 1.0f, 1);
    GeometryGenerator::MeshData torus = geoGen.CreateTorus(2.0f, 0.5f, 20,20);

    //
    // We are concatenating all the geometry into one big vertex/index buffer.  So
    // define the regions in the buffer each submesh covers.
    //

    // Cache the vertex offsets to each object in the concatenated vertex buffer.
    UINT boxVertexOffset = 0; // ADD HERE
    UINT outterWallVertexOffset = boxVertexOffset + (UINT)box.Vertices.size();
    UINT towerVertexOffset = outterWallVertexOffset + (UINT)outterWall.Vertices.size();
    UINT gateVertexOffset = towerVertexOffset + (UINT)tower.Vertices.size();
    UINT gridVertexOffset = gateVertexOffset + (UINT)gate.Vertices.size();
    UINT sphereVertexOffset = gridVertexOffset + (UINT)grid.Vertices.size();
    UINT cylinderVertexOffset = sphereVertexOffset + (UINT)sphere.Vertices.size();
    UINT roloVertexOffset = cylinderVertexOffset + (UINT)cylinder.Vertices.size(); // MARY 2
    UINT wedgeVertexOffset = roloVertexOffset + (UINT)rolo.Vertices.size(); // MARY 2
    UINT coneVertexOffset = wedgeVertexOffset + (UINT)wedge.Vertices.size(); // MARY 2-2
    UINT pyramidVertexOffset = coneVertexOffset + (UINT)cone.Vertices.size(); // MARY 2-3
    UINT truncatedPyramidVertexOffset = pyramidVertexOffset + (UINT)pyramid.Vertices.size();
    UINT diamondVertexOffset = truncatedPyramidVertexOffset + (UINT)truncatedPyramid.Vertices.size();
    UINT charmVertexOffset = diamondVertexOffset + (UINT)diamond.Vertices.size();
    UINT prismVertexOffset = charmVertexOffset + (UINT)charm.Vertices.size();
    UINT torusVertexOffset = prismVertexOffset + (UINT)prism.Vertices.size();


    // Cache the starting index for each object in the concatenated index buffer.
    UINT boxIndexOffset = 0; // ADD HERE
    UINT outterWallIndexOffset = boxIndexOffset + (UINT)box.Indices32.size();
    UINT towerIndexOffset = outterWallIndexOffset + (UINT)outterWall.Indices32.size();
    UINT gateIndexOffset = towerIndexOffset + (UINT)tower.Indices32.size();
    UINT gridIndexOffset = gateIndexOffset + (UINT)gate.Indices32.size();
    UINT sphereIndexOffset = gridIndexOffset + (UINT)grid.Indices32.size();
    UINT cylinderIndexOffset = sphereIndexOffset + (UINT)sphere.Indices32.size();
    UINT roloIndexOffset = cylinderIndexOffset + (UINT)cylinder.Indices32.size(); // MARY 3
    UINT wedgeIndexOffset = roloIndexOffset + (UINT)rolo.Indices32.size(); // MARY 3
    UINT coneIndexOffset = wedgeIndexOffset + (UINT)wedge.Indices32.size(); // MARY 3-2
    UINT pyramidIndexOffset = coneIndexOffset + (UINT)cone.Indices32.size(); // MARY 3-3
    UINT truncatedPyramidIndexOffset = pyramidIndexOffset + (UINT)pyramid.Indices32.size();
    UINT diamondIndexOffset = truncatedPyramidIndexOffset + (UINT)truncatedPyramid.Indices32.size();
    UINT charmIndexOffset = diamondIndexOffset + (UINT)diamond.Indices32.size();
    UINT prismIndexOffset = charmIndexOffset + (UINT)charm.Indices32.size();
    UINT torusIndexOffset = prismIndexOffset + (UINT)prism.Indices32.size();



    // Define the SubmeshGeometry that cover different 
    // regions of the vertex/index buffers.

    SubmeshGeometry boxSubmesh; // ADD HERE
    boxSubmesh.IndexCount = (UINT)box.Indices32.size();
    boxSubmesh.StartIndexLocation = boxIndexOffset;
    boxSubmesh.BaseVertexLocation = boxVertexOffset;

    SubmeshGeometry outterWallSubmesh;
    outterWallSubmesh.IndexCount = (UINT)outterWall.Indices32.size();
    outterWallSubmesh.StartIndexLocation = outterWallIndexOffset;
    outterWallSubmesh.BaseVertexLocation = outterWallVertexOffset;
    
    SubmeshGeometry towerWallSubmesh;
    towerWallSubmesh.IndexCount = (UINT)tower.Indices32.size();
    towerWallSubmesh.StartIndexLocation = towerIndexOffset;
    towerWallSubmesh.BaseVertexLocation = towerVertexOffset;
    
    SubmeshGeometry gateWallSubmesh;
    gateWallSubmesh.IndexCount = (UINT)gate.Indices32.size();
    gateWallSubmesh.StartIndexLocation = gateIndexOffset;
    gateWallSubmesh.BaseVertexLocation = gateVertexOffset;
    
    SubmeshGeometry gridSubmesh;
    gridSubmesh.IndexCount = (UINT)grid.Indices32.size();
    gridSubmesh.StartIndexLocation = gridIndexOffset;
    gridSubmesh.BaseVertexLocation = gridVertexOffset;

    SubmeshGeometry sphereSubmesh;
    sphereSubmesh.IndexCount = (UINT)sphere.Indices32.size();
    sphereSubmesh.StartIndexLocation = sphereIndexOffset;
    sphereSubmesh.BaseVertexLocation = sphereVertexOffset;

    SubmeshGeometry cylinderSubmesh;
    cylinderSubmesh.IndexCount = (UINT)cylinder.Indices32.size();
    cylinderSubmesh.StartIndexLocation = cylinderIndexOffset;
    cylinderSubmesh.BaseVertexLocation = cylinderVertexOffset;
    
    SubmeshGeometry roloSubmesh;
    roloSubmesh.IndexCount = (UINT)rolo.Indices32.size();
    roloSubmesh.StartIndexLocation = roloIndexOffset;
    roloSubmesh.BaseVertexLocation = roloVertexOffset;

    // MARY 4
    SubmeshGeometry wedgeSubmesh;
    wedgeSubmesh.IndexCount = (UINT)wedge.Indices32.size();
    wedgeSubmesh.StartIndexLocation = wedgeIndexOffset;
    wedgeSubmesh.BaseVertexLocation = wedgeVertexOffset;

    // MARY 4-2
    SubmeshGeometry coneSubmesh;
    coneSubmesh.IndexCount = (UINT)cone.Indices32.size();
    coneSubmesh.StartIndexLocation = coneIndexOffset;
    coneSubmesh.BaseVertexLocation = coneVertexOffset;

    // MARY 4-3
    SubmeshGeometry pyramidSubmesh;
    pyramidSubmesh.IndexCount = (UINT)pyramid.Indices32.size();
    pyramidSubmesh.StartIndexLocation = pyramidIndexOffset;
    pyramidSubmesh.BaseVertexLocation = pyramidVertexOffset;

    SubmeshGeometry truncatedPyramidSubmesh;
    truncatedPyramidSubmesh.IndexCount = (UINT)truncatedPyramid.Indices32.size();
    truncatedPyramidSubmesh.StartIndexLocation = truncatedPyramidIndexOffset;
    truncatedPyramidSubmesh.BaseVertexLocation = truncatedPyramidVertexOffset;

    SubmeshGeometry diamondSubmesh;
    diamondSubmesh.IndexCount = (UINT)diamond.Indices32.size();
    diamondSubmesh.StartIndexLocation = diamondIndexOffset;
    diamondSubmesh.BaseVertexLocation = diamondVertexOffset;
    
    SubmeshGeometry charmSubmesh;
    charmSubmesh.IndexCount = (UINT)charm.Indices32.size();
    charmSubmesh.StartIndexLocation = charmIndexOffset;
    charmSubmesh.BaseVertexLocation = charmVertexOffset;

    SubmeshGeometry prismSubmesh;
    prismSubmesh.IndexCount = (UINT)prism.Indices32.size();
    prismSubmesh.StartIndexLocation = prismIndexOffset;
    prismSubmesh.BaseVertexLocation = prismVertexOffset;

    SubmeshGeometry torusSubmesh;
    torusSubmesh.IndexCount = (UINT)torus.Indices32.size();
    torusSubmesh.StartIndexLocation = torusIndexOffset;
    torusSubmesh.BaseVertexLocation = torusVertexOffset;

    //
    // Extract the vertex elements we are interested in and pack the
    // vertices of all the meshes into one vertex buffer.
    //

    auto totalVertexCount =
        box.Vertices.size() +
        outterWall.Vertices.size() +
        tower.Vertices.size() +
        gate.Vertices.size() +
        grid.Vertices.size() +
        sphere.Vertices.size() +
        cylinder.Vertices.size() +
        rolo.Vertices.size() +
        wedge.Vertices.size() + // MARY 5
        cone.Vertices.size() + // MARY 5-2
        pyramid.Vertices.size() + // MARY 5-3
        truncatedPyramid.Vertices.size() +
        diamond.Vertices.size() +
        charm.Vertices.size() +
        prism.Vertices.size() +
        torus.Vertices.size();


    std::vector<Vertex> vertices(totalVertexCount);

    UINT k = 0;
    for (size_t i = 0; i < box.Vertices.size(); ++i, ++k) // ADD HERE
    {
        vertices[k].Pos = box.Vertices[i].Position;
        vertices[k].Normal = box.Vertices[i].Normal;
        vertices[k].TexC = box.Vertices[i].TexC;
    }

    for (size_t i = 0; i < outterWall.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = outterWall.Vertices[i].Position;
        vertices[k].Normal = outterWall.Vertices[i].Normal;
        vertices[k].TexC = outterWall.Vertices[i].TexC;
    }
    
    for (size_t i = 0; i < tower.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = tower.Vertices[i].Position;
        vertices[k].Normal = tower.Vertices[i].Normal;
        vertices[k].TexC = tower.Vertices[i].TexC;
    }
    
    for (size_t i = 0; i < gate.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = gate.Vertices[i].Position;
        vertices[k].Normal = gate.Vertices[i].Normal;
        vertices[k].TexC = gate.Vertices[i].TexC;
    }
    
    for (size_t i = 0; i < grid.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = grid.Vertices[i].Position;
        vertices[k].Normal = grid.Vertices[i].Normal;
        vertices[k].TexC = grid.Vertices[i].TexC;
    }

    for (size_t i = 0; i < sphere.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = sphere.Vertices[i].Position;
        vertices[k].Normal = sphere.Vertices[i].Normal;
        vertices[k].TexC = sphere.Vertices[i].TexC;
    }

    for (size_t i = 0; i < cylinder.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = cylinder.Vertices[i].Position;
        vertices[k].Normal = cylinder.Vertices[i].Normal;
        vertices[k].TexC = cylinder.Vertices[i].TexC;
    }
    
    for (size_t i = 0; i < rolo.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = rolo.Vertices[i].Position;
        vertices[k].Normal = rolo.Vertices[i].Normal;
        vertices[k].TexC = rolo.Vertices[i].TexC;
    }

    // MARY 6
    for (size_t i = 0; i < wedge.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = wedge.Vertices[i].Position;
        vertices[k].Normal = wedge.Vertices[i].Normal;
        vertices[k].TexC = wedge.Vertices[i].TexC;
    }
    // MARY 6-2
    for (size_t i = 0; i < cone.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = cone.Vertices[i].Position;
        vertices[k].Normal = cone.Vertices[i].Normal;
        vertices[k].TexC = cone.Vertices[i].TexC;
    }
    // MARY 6-3
    for (size_t i = 0; i < pyramid.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = pyramid.Vertices[i].Position;
        vertices[k].Normal = pyramid.Vertices[i].Normal;
        vertices[k].TexC = pyramid.Vertices[i].TexC;
    }

    for (size_t i = 0; i < truncatedPyramid.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = truncatedPyramid.Vertices[i].Position;
        vertices[k].Normal = truncatedPyramid.Vertices[i].Normal;
        vertices[k].TexC = truncatedPyramid.Vertices[i].TexC;
    }

    for (size_t i = 0; i < diamond.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = diamond.Vertices[i].Position;
        vertices[k].Normal = diamond.Vertices[i].Normal;
        vertices[k].TexC = diamond.Vertices[i].TexC;
    }
    
    for (size_t i = 0; i < charm.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = charm.Vertices[i].Position;
        vertices[k].Normal = charm.Vertices[i].Normal;
        vertices[k].TexC = charm.Vertices[i].TexC;
    }

    for (size_t i = 0; i < prism.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = prism.Vertices[i].Position;
        vertices[k].Normal = prism.Vertices[i].Normal;
        vertices[k].TexC = prism.Vertices[i].TexC;
    }

    for (size_t i = 0; i < torus.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = torus.Vertices[i].Position;
        vertices[k].Normal = torus.Vertices[i].Normal;
        vertices[k].TexC = torus.Vertices[i].TexC;
    }
    std::vector<std::uint16_t> indices;
    indices.insert(indices.end(), std::begin(box.GetIndices16()), std::end(box.GetIndices16())); // ADD HERE
    indices.insert(indices.end(), std::begin(outterWall.GetIndices16()), std::end(outterWall.GetIndices16()));
    indices.insert(indices.end(), std::begin(tower.GetIndices16()), std::end(tower.GetIndices16()));
    indices.insert(indices.end(), std::begin(gate.GetIndices16()), std::end(gate.GetIndices16()));
    indices.insert(indices.end(), std::begin(grid.GetIndices16()), std::end(grid.GetIndices16()));
    indices.insert(indices.end(), std::begin(sphere.GetIndices16()), std::end(sphere.GetIndices16()));
    indices.insert(indices.end(), std::begin(cylinder.GetIndices16()), std::end(cylinder.GetIndices16()));
    indices.insert(indices.end(), std::begin(rolo.GetIndices16()), std::end(rolo.GetIndices16()));
    indices.insert(indices.end(), std::begin(wedge.GetIndices16()), std::end(wedge.GetIndices16())); // MARY 7
    indices.insert(indices.end(), std::begin(cone.GetIndices16()), std::end(cone.GetIndices16())); // MARY 7-2
    indices.insert(indices.end(), std::begin(pyramid.GetIndices16()), std::end(pyramid.GetIndices16())); // MARY 7-3
    indices.insert(indices.end(), std::begin(truncatedPyramid.GetIndices16()), std::end(truncatedPyramid.GetIndices16()));
    indices.insert(indices.end(), std::begin(diamond.GetIndices16()), std::end(diamond.GetIndices16()));
    indices.insert(indices.end(), std::begin(charm.GetIndices16()), std::end(charm.GetIndices16()));
    indices.insert(indices.end(), std::begin(prism.GetIndices16()), std::end(prism.GetIndices16()));
    indices.insert(indices.end(), std::begin(torus.GetIndices16()), std::end(torus.GetIndices16()));



    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

    auto geo = std::make_unique<MeshGeometry>();
    geo->Name = "shapeGeo";

    ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
    CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

    ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
    CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

    geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

    geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

    geo->VertexByteStride = sizeof(Vertex);
    geo->VertexBufferByteSize = vbByteSize;
    geo->IndexFormat = DXGI_FORMAT_R16_UINT;
    geo->IndexBufferByteSize = ibByteSize;

    geo->DrawArgs["box"] = boxSubmesh; // ADD HERE
    geo->DrawArgs["outterWall"] = outterWallSubmesh;
    geo->DrawArgs["tower"] = towerWallSubmesh;
    geo->DrawArgs["gate"] = gateWallSubmesh;
    geo->DrawArgs["grid"] = gridSubmesh;
    geo->DrawArgs["sphere"] = sphereSubmesh;
    geo->DrawArgs["cylinder"] = cylinderSubmesh;
    geo->DrawArgs["rolo"] = roloSubmesh;
    geo->DrawArgs["wedge"] = wedgeSubmesh; // MARY 8
    geo->DrawArgs["cone"] = coneSubmesh; // MARY 8-2
    geo->DrawArgs["pyramid"] = pyramidSubmesh; // MARY 8-3
    geo->DrawArgs["truncatedPyramid"] = truncatedPyramidSubmesh;
    geo->DrawArgs["diamond"] = diamondSubmesh;
    geo->DrawArgs["charm"] = charmSubmesh;
    geo->DrawArgs["prism"] = prismSubmesh;
    geo->DrawArgs["torus"] = torusSubmesh;



    mGeometries[geo->Name] = std::move(geo);*/

    ::OutputDebugStringA(">>> BuildShapeGeometry DONE!\n");
}

void ShapesApp::BuildTreeSpritesGeometry()
{
    //step5
    struct TreeSpriteVertex
    {
        XMFLOAT3 Pos;
        XMFLOAT2 Size;
    };

    static const int treeCount = 16;
    std::array<TreeSpriteVertex, 16> vertices;
    for (UINT i = 0; i < treeCount; ++i)
    {
        float x = MathHelper::RandF(-45.0f, 45.0f);
        float z = MathHelper::RandF(-45.0f, 45.0f);
        float y = GetHillsHeight(x, z);

        // Move tree slightly above land height.
        y += 8.0f;

        vertices[i].Pos = XMFLOAT3(x, y, z);
        vertices[i].Size = XMFLOAT2(20.0f, 20.0f);
    }

    std::array<std::uint16_t, 16> indices =
    {
        0, 1, 2, 3, 4, 5, 6, 7,
        8, 9, 10, 11, 12, 13, 14, 15
    };

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(TreeSpriteVertex);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

    auto geo = std::make_unique<MeshGeometry>();
    geo->Name = "treeSpritesGeo";

    ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
    CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

    ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
    CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

    geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

    geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

    geo->VertexByteStride = sizeof(TreeSpriteVertex);
    geo->VertexBufferByteSize = vbByteSize;
    geo->IndexFormat = DXGI_FORMAT_R16_UINT;
    geo->IndexBufferByteSize = ibByteSize;

    SubmeshGeometry submesh;
    submesh.IndexCount = (UINT)indices.size();
    submesh.StartIndexLocation = 0;
    submesh.BaseVertexLocation = 0;

    geo->DrawArgs["points"] = submesh;

    mGeometries["treeSpritesGeo"] = std::move(geo);
}

void ShapesApp::BuildPSOs()
{
    ::OutputDebugStringA(">>> BuildPSOs started...\n");
    
    D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;

	//
	// PSO for opaque objects.
	//
    ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	opaquePsoDesc.InputLayout = { mStdInputLayout.data(), (UINT)mStdInputLayout.size() };
	opaquePsoDesc.pRootSignature = mRootSignature.Get();
	opaquePsoDesc.VS = 
	{ 
		reinterpret_cast<BYTE*>(mShaders["standardVS"]->GetBufferPointer()), 
		mShaders["standardVS"]->GetBufferSize()
	};
	opaquePsoDesc.PS = 
	{ 
		reinterpret_cast<BYTE*>(mShaders["opaquePS"]->GetBufferPointer()),
		mShaders["opaquePS"]->GetBufferSize()
	};
	opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    //opaquePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaquePsoDesc.SampleMask = UINT_MAX;
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaquePsoDesc.NumRenderTargets = 1;
	opaquePsoDesc.RTVFormats[0] = mBackBufferFormat;
	opaquePsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	opaquePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	opaquePsoDesc.DSVFormat = mDepthStencilFormat;
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque"])));


    //
    // PSO for transparent objects
    //

    D3D12_GRAPHICS_PIPELINE_STATE_DESC transparentPsoDesc = opaquePsoDesc;

    D3D12_RENDER_TARGET_BLEND_DESC transparencyBlendDesc;
    transparencyBlendDesc.BlendEnable = true;
    transparencyBlendDesc.LogicOpEnable = false;
    transparencyBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    transparencyBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    transparencyBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
    transparencyBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
    transparencyBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
    transparencyBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    transparencyBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
    transparencyBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    //transparentPsoDesc.BlendState.AlphaToCoverageEnable = true;

    transparentPsoDesc.BlendState.RenderTarget[0] = transparencyBlendDesc;
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&transparentPsoDesc, IID_PPV_ARGS(&mPSOs["transparent"])));

    //
    // PSO for alpha tested objects
    //

    D3D12_GRAPHICS_PIPELINE_STATE_DESC alphaTestedPsoDesc = opaquePsoDesc;
    alphaTestedPsoDesc.PS =
    {
        reinterpret_cast<BYTE*>(mShaders["alphaTestedPS"]->GetBufferPointer()),
        mShaders["alphaTestedPS"]->GetBufferSize()
    };
    alphaTestedPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&alphaTestedPsoDesc, IID_PPV_ARGS(&mPSOs["alphaTested"])));

    //
    // PSO for tree sprites
    //
    D3D12_GRAPHICS_PIPELINE_STATE_DESC treeSpritePsoDesc = opaquePsoDesc;
    treeSpritePsoDesc.VS =
    {
        reinterpret_cast<BYTE*>(mShaders["treeSpriteVS"]->GetBufferPointer()),
        mShaders["treeSpriteVS"]->GetBufferSize()
    };
    treeSpritePsoDesc.GS =
    {
        reinterpret_cast<BYTE*>(mShaders["treeSpriteGS"]->GetBufferPointer()),
        mShaders["treeSpriteGS"]->GetBufferSize()
    };
    treeSpritePsoDesc.PS =
    {
        reinterpret_cast<BYTE*>(mShaders["treeSpritePS"]->GetBufferPointer()),
        mShaders["treeSpritePS"]->GetBufferSize()
    };
    //step1
    treeSpritePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
    treeSpritePsoDesc.InputLayout = { mTreeSpriteInputLayout.data(), (UINT)mTreeSpriteInputLayout.size() };
    treeSpritePsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&treeSpritePsoDesc, IID_PPV_ARGS(&mPSOs["treeSprites"])));

    //
    // PSO for opaque wireframe objects.
    //
    //D3D12_GRAPHICS_PIPELINE_STATE_DESC opaqueWireframePsoDesc = opaquePsoDesc;
    //opaqueWireframePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
    //ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaqueWireframePsoDesc, IID_PPV_ARGS(&mPSOs["opaque_wireframe"])));

    ::OutputDebugStringA(">>> BuildPSOs DONE!\n");
}

void ShapesApp::BuildFrameResources()
{
    ::OutputDebugStringA(">>> BuildFrameResources started...\n");
    
    for(int i = 0; i < gNumFrameResources; ++i)
    {
        mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
            1, (UINT)mAllRitems.size(), (UINT)mMaterials.size(), mWaves->VertexCount()));
    }

    ::OutputDebugStringA(">>> BuildFrameResources DONE!\n");
}

void ShapesApp::BuildMaterials() //EDIT MATS HERE
{
    ::OutputDebugStringA(">>> BuildMaterials started...\n");
    
    auto bricks0 = std::make_unique<Material>();
    bricks0->Name = "bricks0";
    bricks0->MatCBIndex = 0;
    bricks0->DiffuseSrvHeapIndex = 0;
    bricks0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    bricks0->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
    bricks0->Roughness = 0.1f;

    auto stone0 = std::make_unique<Material>();
    stone0->Name = "stone0";
    stone0->MatCBIndex = 1;
    stone0->DiffuseSrvHeapIndex = 1;
    stone0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    stone0->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
    stone0->Roughness = 0.3f;

    auto tile0 = std::make_unique<Material>();
    tile0->Name = "tile0";
    tile0->MatCBIndex = 2;
    tile0->DiffuseSrvHeapIndex = 2;
    tile0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    tile0->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
    tile0->Roughness = 0.3f;

    // This is not a good water material definition, but we do not have all the rendering
    // tools we need (transparency, environment reflection), so we fake it for now.
    auto water = std::make_unique<Material>();
    water->Name = "water";
    water->MatCBIndex = 3; //EDIT
    water->DiffuseSrvHeapIndex = 3; //EDIT
    water->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    water->FresnelR0 = XMFLOAT3(0.2f, 0.2f, 0.2f);
    water->Roughness = 0.0f;

    auto treeSprites = std::make_unique<Material>();
    treeSprites->Name = "treeSprites";
    treeSprites->MatCBIndex = 3;
    treeSprites->DiffuseSrvHeapIndex = 3;
    treeSprites->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    treeSprites->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
    treeSprites->Roughness = 0.125f;

    mMaterials["bricks0"] = std::move(bricks0);
    mMaterials["stone0"] = std::move(stone0);
    mMaterials["tile0"] = std::move(tile0);
    mMaterials["water"] = std::move(water);
    mMaterials["treeSprites"] = std::move(treeSprites);

    ::OutputDebugStringA(">>> BuildMaterials DONE!\n");
}


void ShapesApp::BuildOneRenderItem(std::string shape_type, std::string shape_name, std::string material, XMMATRIX scale_matrix, XMMATRIX translate_matrix, XMMATRIX tex_scale_matrix, UINT obj_idx)
{
    auto shape_render_item = std::make_unique<RenderItem>();
    XMStoreFloat4x4(&shape_render_item->World, scale_matrix * translate_matrix);
    XMStoreFloat4x4(&shape_render_item->TexTransform, tex_scale_matrix);
    shape_render_item->ObjCBIndex = obj_idx;
    shape_render_item->Mat = mMaterials[material].get();
    shape_render_item->Geo = mGeometries[shape_name].get();
    shape_render_item->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    shape_render_item->IndexCount = shape_render_item->Geo->DrawArgs[shape_type].IndexCount;
    shape_render_item->StartIndexLocation = shape_render_item->Geo->DrawArgs[shape_type].StartIndexLocation;
    shape_render_item->BaseVertexLocation = shape_render_item->Geo->DrawArgs[shape_type].BaseVertexLocation;
    mRitemLayer[(int)RenderLayer::Opaque].push_back(shape_render_item.get());
    mAllRitems.push_back(std::move(shape_render_item));
}

void ShapesApp::BuildOneRenderItem(std::string shape_type, std::string shape_name, std::string material, XMMATRIX rotate_matrix, XMMATRIX scale_matrix, XMMATRIX translate_matrix, XMMATRIX tex_scale_matrix, UINT obj_idx)
{
    auto shape_render_item = std::make_unique<RenderItem>();
    XMStoreFloat4x4(&shape_render_item->World, scale_matrix * rotate_matrix * translate_matrix);
    XMStoreFloat4x4(&shape_render_item->TexTransform, tex_scale_matrix);
    shape_render_item->ObjCBIndex = obj_idx;
    shape_render_item->Mat = mMaterials[material].get();
    shape_render_item->Geo = mGeometries[shape_name].get();
    shape_render_item->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    shape_render_item->IndexCount = shape_render_item->Geo->DrawArgs[shape_type].IndexCount;
    shape_render_item->StartIndexLocation = shape_render_item->Geo->DrawArgs[shape_type].StartIndexLocation;
    shape_render_item->BaseVertexLocation = shape_render_item->Geo->DrawArgs[shape_type].BaseVertexLocation;
    mRitemLayer[(int)RenderLayer::Opaque].push_back(shape_render_item.get());
    mAllRitems.push_back(std::move(shape_render_item));
}


//void ShapesApp::BuildRenderItems()
//{
//    /*auto boxRitem = std::make_unique<RenderItem>();
//    XMStoreFloat4x4(&boxRitem->World, XMMatrixScaling(2.0f, 2.0f, 2.0f)*XMMatrixTranslation(0.0f, 0.5f, 0.0f));
//    boxRitem->ObjCBIndex = 0;
//    boxRitem->Geo = mGeometries["shapeGeo"].get();
//    boxRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
//    boxRitem->IndexCount = boxRitem->Geo->DrawArgs["box"].IndexCount;
//    boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box"].StartIndexLocation;
//    boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].BaseVertexLocation;
//    mAllRitems.push_back(std::move(boxRitem));*/
//
//    BuildOneRenderItem("box", XMMatrixScaling(2.0f, 2.0f, 2.0f), XMMatrixTranslation(0.0f, 0.5f, 0.0f), 0);
//    BuildOneRenderItem("diamond", XMMatrixScaling(1.0f, 1.0f, 1.0f), XMMatrixTranslation(0.0f, 0.0f, 0.0f), 1);
//    BuildOneRenderItem("prism", XMMatrixScaling(2.0f, 2.0f, 2.0f), XMMatrixTranslation(0.0f, 0.5f, 0.0f), 2);
//
//    auto gridRitem = std::make_unique<RenderItem>();
//    gridRitem->World = MathHelper::Identity4x4();
//    gridRitem->ObjCBIndex = 3;
//    gridRitem->Geo = mGeometries["shapeGeo"].get();
//    gridRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
//    gridRitem->IndexCount = gridRitem->Geo->DrawArgs["grid"].IndexCount;
//    gridRitem->StartIndexLocation = gridRitem->Geo->DrawArgs["grid"].StartIndexLocation;
//    gridRitem->BaseVertexLocation = gridRitem->Geo->DrawArgs["grid"].BaseVertexLocation;
//    mAllRitems.push_back(std::move(gridRitem));
//
//    UINT objCBIndex = 4;
//    for (int i = 0; i < 5; ++i)
//    {
//        auto leftCylRitem = std::make_unique<RenderItem>();
//        auto rightCylRitem = std::make_unique<RenderItem>();
//        auto leftSphereRitem = std::make_unique<RenderItem>();
//        auto rightSphereRitem = std::make_unique<RenderItem>();
//
//        XMMATRIX leftCylWorld = XMMatrixTranslation(-5.0f, 1.5f, -10.0f + i * 5.0f);
//        XMMATRIX rightCylWorld = XMMatrixTranslation(+5.0f, 1.5f, -10.0f + i * 5.0f);
//
//        XMMATRIX leftSphereWorld = XMMatrixTranslation(-5.0f, 3.5f, -10.0f + i * 5.0f);
//        XMMATRIX rightSphereWorld = XMMatrixTranslation(+5.0f, 3.5f, -10.0f + i * 5.0f);
//
//        XMStoreFloat4x4(&leftCylRitem->World, rightCylWorld);
//        leftCylRitem->ObjCBIndex = objCBIndex++;
//        leftCylRitem->Geo = mGeometries["shapeGeo"].get();
//        leftCylRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
//        leftCylRitem->IndexCount = leftCylRitem->Geo->DrawArgs["cylinder"].IndexCount;
//        leftCylRitem->StartIndexLocation = leftCylRitem->Geo->DrawArgs["cylinder"].StartIndexLocation;
//        leftCylRitem->BaseVertexLocation = leftCylRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation;
//
//        XMStoreFloat4x4(&rightCylRitem->World, leftCylWorld);
//        rightCylRitem->ObjCBIndex = objCBIndex++;
//        rightCylRitem->Geo = mGeometries["shapeGeo"].get();
//        rightCylRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
//        rightCylRitem->IndexCount = rightCylRitem->Geo->DrawArgs["cylinder"].IndexCount;
//        rightCylRitem->StartIndexLocation = rightCylRitem->Geo->DrawArgs["cylinder"].StartIndexLocation;
//        rightCylRitem->BaseVertexLocation = rightCylRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation;
//
//        XMStoreFloat4x4(&leftSphereRitem->World, leftSphereWorld);
//        leftSphereRitem->ObjCBIndex = objCBIndex++;
//        leftSphereRitem->Geo = mGeometries["shapeGeo"].get();
//        leftSphereRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
//        leftSphereRitem->IndexCount = leftSphereRitem->Geo->DrawArgs["sphere"].IndexCount;
//        leftSphereRitem->StartIndexLocation = leftSphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
//        leftSphereRitem->BaseVertexLocation = leftSphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;
//
//        XMStoreFloat4x4(&rightSphereRitem->World, rightSphereWorld);
//        rightSphereRitem->ObjCBIndex = objCBIndex++;
//        rightSphereRitem->Geo = mGeometries["shapeGeo"].get();
//        rightSphereRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
//        rightSphereRitem->IndexCount = rightSphereRitem->Geo->DrawArgs["sphere"].IndexCount;
//        rightSphereRitem->StartIndexLocation = rightSphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
//        rightSphereRitem->BaseVertexLocation = rightSphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;
//
//        mAllRitems.push_back(std::move(leftCylRitem));
//        mAllRitems.push_back(std::move(rightCylRitem));
//        mAllRitems.push_back(std::move(leftSphereRitem));
//        mAllRitems.push_back(std::move(rightSphereRitem));
//    }
//}

void ShapesApp::BuildRenderItems()
{
    ::OutputDebugStringA(">>> BuildRenderItems started...\n");
        
    UINT index_cache = 0;

    // waves
    auto wavesRitem = std::make_unique<RenderItem>();
    wavesRitem->World = MathHelper::Identity4x4();
    XMStoreFloat4x4(&wavesRitem->TexTransform, XMMatrixScaling(5.0f, 5.0f, 1.0f));
    wavesRitem->ObjCBIndex = index_cache;
    wavesRitem->Mat = mMaterials["water"].get();
    wavesRitem->Geo = mGeometries["waterGeo"].get();
    wavesRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    wavesRitem->IndexCount = wavesRitem->Geo->DrawArgs["grid"].IndexCount;
    wavesRitem->StartIndexLocation = wavesRitem->Geo->DrawArgs["grid"].StartIndexLocation;
    wavesRitem->BaseVertexLocation = wavesRitem->Geo->DrawArgs["grid"].BaseVertexLocation;
    index_cache++;

    //// we use mVavesRitem in updatewaves() to set the dynamic VB of the wave renderitem to the current frame VB.
    mWavesRitem = wavesRitem.get();
    mRitemLayer[(int)RenderLayer::Transparent].push_back(wavesRitem.get());
    mAllRitems.push_back(std::move(wavesRitem)); //EXTREME MEGA IMPORTANT LINE

    // HILLS
    auto gridRitem = std::make_unique<RenderItem>();
    gridRitem->World = MathHelper::Identity4x4();
    XMStoreFloat4x4(&gridRitem->TexTransform, XMMatrixScaling(5.0f, 5.0f, 1.0f));
    gridRitem->ObjCBIndex = index_cache;
    gridRitem->Mat = mMaterials["bricks0"].get();
    gridRitem->Geo = mGeometries["landGeo"].get();
    gridRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    gridRitem->IndexCount = gridRitem->Geo->DrawArgs["grid"].IndexCount;
    gridRitem->StartIndexLocation = gridRitem->Geo->DrawArgs["grid"].StartIndexLocation;
    gridRitem->BaseVertexLocation = gridRitem->Geo->DrawArgs["grid"].BaseVertexLocation;
    index_cache++;

    mRitemLayer[(int)RenderLayer::Opaque].push_back(gridRitem.get());
    mAllRitems.push_back(std::move(gridRitem));

    // grid
    BuildOneRenderItem("grid", "gridGeo", "tile0", XMMatrixScaling(2, 2, 2), XMMatrixTranslation(0.0f, 0.0f, 0.0f), XMMatrixScaling(1, 1, 1), index_cache++);

    // OUTTER
    // front wall 
    BuildOneRenderItem("outterWall", "outterWallGeo", "stone0", XMMatrixScaling(50.0f, 40, 4.0f), XMMatrixTranslation(0.0f, 19, -25.0f), XMMatrixScaling(1, 1, 1), index_cache++);
    // back wall
    BuildOneRenderItem("outterWall", "outterWallGeo", "stone0", XMMatrixScaling(50.0f, 40, 4.0f), XMMatrixTranslation(0.0f, 19, 25.0f), XMMatrixScaling(1, 1, 1), index_cache++);
    // left wall 
    BuildOneRenderItem("outterWall", "outterWallGeo", "stone0", XMMatrixRotationY(XMConvertToRadians(-90.0f)), XMMatrixScaling(50.0f, 40, 4.0f), XMMatrixTranslation(-25.0f, 19, 0.0f), XMMatrixScaling(1, 1, 1), index_cache++);
    // right wall
    BuildOneRenderItem("outterWall", "outterWallGeo", "stone0", XMMatrixRotationY(XMConvertToRadians(90.0f)), XMMatrixScaling(50.0f, 40, 4.0f), XMMatrixTranslation(25.0f, 19, 0.0f), XMMatrixScaling(1, 1, 1), index_cache++);
    
    const int kNumWallWedges = 12;
    // front wedge loop
    for (int i = 0; i < kNumWallWedges; ++i){
        BuildOneRenderItem("wedge", "wedgeGeo", "stone0", XMMatrixRotationY(XMConvertToRadians(180.0f)), XMMatrixScaling(2, 2, 2), XMMatrixTranslation(-20 + i * 4, 40, -26), XMMatrixScaling(1, 1, 1), index_cache++); //step = 4, min = -20
    }
    // back wedge loop
    for (int i = 0; i < kNumWallWedges; ++i){
        BuildOneRenderItem("wedge", "wedgeGeo", "stone0", XMMatrixScaling(2, 2, 2), XMMatrixTranslation(-20 + i * 4, 40, 26), XMMatrixScaling(1, 1, 1), index_cache++); //step = 4, min = -20
    }
    // left wedge loop
    for (int i = 0; i < kNumWallWedges; ++i){
        BuildOneRenderItem("wedge", "wedgeGeo", "stone0", XMMatrixRotationY(XMConvertToRadians(-90)), XMMatrixScaling(2, 2, 2), XMMatrixTranslation(-26, 40, -20 + i * 4), XMMatrixScaling(1, 1, 1), index_cache++); //step = 4, min = -20
    }
    // right wedge loop
    for (int i = 0; i < kNumWallWedges; ++i){
        BuildOneRenderItem("wedge", "wedgeGeo", "stone0", XMMatrixRotationY(XMConvertToRadians(90)), XMMatrixScaling(2, 2, 2), XMMatrixTranslation(26, 40, -20 + i * 4), XMMatrixScaling(1, 1, 1), index_cache++); //step = 4, min = -20
    }
    
    const int kNumWallPyramids = 6;
    // left pyramid loop
    for (int i = 0; i < kNumWallPyramids; ++i){
        BuildOneRenderItem("truncatedPyramid", "truncatedPyramidGeo", "stone0", XMMatrixRotationRollPitchYaw(XMConvertToRadians(0), XMConvertToRadians(0), XMConvertToRadians(90)), XMMatrixScaling(3, 2, 3), XMMatrixTranslation(-28, 34, -17.5 + i * 7), XMMatrixScaling(1, 1, 1), index_cache++); //step = 4, min = -20
    }
    // right pyramid loop
    for (int i = 0; i < kNumWallPyramids; ++i) {
        BuildOneRenderItem("truncatedPyramid", "truncatedPyramidGeo", "stone0", XMMatrixRotationRollPitchYaw(XMConvertToRadians(0), XMConvertToRadians(0), XMConvertToRadians(-90)), XMMatrixScaling(3, 2, 3), XMMatrixTranslation(28, 34, -17.5 + i * 7), XMMatrixScaling(1, 1, 1), index_cache++); //step = 4, min = -20
    }
    // front rolo loop
    for (int i = 0; i < kNumWallPyramids; ++i) {
        BuildOneRenderItem("rolo", "roloGeo", "stone0", XMMatrixRotationRollPitchYaw(XMConvertToRadians(-90), XMConvertToRadians(0), XMConvertToRadians(0)), XMMatrixScaling(2, 2, 2), XMMatrixTranslation(-17.5 + i * 7, 34, -28), XMMatrixScaling(1, 1, 1), index_cache++); //step = 4, min = -20
    }
    // back rolo loop
    for (int i = 0; i < kNumWallPyramids; ++i) {
        BuildOneRenderItem("rolo", "roloGeo", "stone0", XMMatrixRotationRollPitchYaw(XMConvertToRadians(90), XMConvertToRadians(0), XMConvertToRadians(0)), XMMatrixScaling(2, 2, 2), XMMatrixTranslation(-17.5 + i * 7, 34, 28), XMMatrixScaling(1, 1, 1), index_cache++); //step = 4, min = -20
    }

    // outer towers
    for (int i = 0; i < 2; ++i)
    {
        // left towers
        BuildOneRenderItem("tower", "towerGeo", "stone0", XMMatrixScaling(8, 50, 8), XMMatrixTranslation(-25, 24, -25 + i * 50), XMMatrixScaling(1, 1, 1), index_cache++);
        //right towers
        BuildOneRenderItem("tower", "towerGeo", "stone0", XMMatrixScaling(8, 50, 8), XMMatrixTranslation(25, 24, -25 + i * 50), XMMatrixScaling(1, 1, 1), index_cache++);
    }
    
    const int kNumPyramids = 4;
    const int kHalfNumPyramids = 2;
    //FL pyr
    //front row pyr
    for (int i = 0; i < kNumPyramids; ++i) {
        BuildOneRenderItem("pyramid", "pyramidGeo", "stone0", XMMatrixRotationY(XMConvertToRadians(-90)), XMMatrixScaling(2.4f, 2.4f, 2.4f), XMMatrixTranslation(-28 + i * 2, 50, -28), XMMatrixScaling(1, 1, 1), index_cache++); //step = 2, min = -28
    }
    //back row pyr
    for (int i = 0; i < kNumPyramids; ++i) {
        BuildOneRenderItem("pyramid", "pyramidGeo", "stone0", XMMatrixRotationY(XMConvertToRadians(90)), XMMatrixScaling(2.4f, 2.4f, 2.4f), XMMatrixTranslation(-28 + i * 2, 50, -22), XMMatrixScaling(1, 1, 1), index_cache++);
    }
    //left col pyr
    for (int i = 0; i < kHalfNumPyramids; ++i) {
        BuildOneRenderItem("pyramid", "pyramidGeo", "stone0", XMMatrixScaling(2.4f, 2.4f, 2.4f), XMMatrixTranslation(-28, 50, -26 + i * 2), XMMatrixScaling(1, 1, 1), index_cache++);
    }
    //right col pyr
    for (int i = 0; i < kHalfNumPyramids; ++i) {
        BuildOneRenderItem("pyramid", "pyramidGeo", "stone0", XMMatrixRotationY(XMConvertToRadians(180)), XMMatrixScaling(2.4f, 2.4f, 2.4f), XMMatrixTranslation(-22, 50, -26 + i * 2), XMMatrixScaling(1, 1, 1), index_cache++);
    }

    //RL pyr
    //front row pyr
    for (int i = 0; i < kNumPyramids; ++i) {
        BuildOneRenderItem("pyramid", "pyramidGeo", "stone0", XMMatrixRotationY(XMConvertToRadians(-90)), XMMatrixScaling(2.4f, 2.4f, 2.4f), XMMatrixTranslation(-28 + i * 2, 50, 28), XMMatrixScaling(1, 1, 1), index_cache++); //step = 2, min = -28
    }
    //back row pyr
    for (int i = 0; i < kNumPyramids; ++i) {
        BuildOneRenderItem("pyramid", "pyramidGeo", "stone0", XMMatrixRotationY(XMConvertToRadians(90)), XMMatrixScaling(2.4f, 2.4f, 2.4f), XMMatrixTranslation(-28 + i * 2, 50, 22), XMMatrixScaling(1, 1, 1), index_cache++);
    }
    //left col pyr
    for (int i = 0; i < kHalfNumPyramids; ++i) {
        BuildOneRenderItem("pyramid", "pyramidGeo", "stone0", XMMatrixScaling(2.4f, 2.4f, 2.4f), XMMatrixTranslation(-28, 50, 24 + i * 2), XMMatrixScaling(1, 1, 1), index_cache++);
    }
    //right col pyr
    for (int i = 0; i < kHalfNumPyramids; ++i) {
        BuildOneRenderItem("pyramid", "pyramidGeo", "stone0", XMMatrixRotationY(XMConvertToRadians(180)), XMMatrixScaling(2.4f, 2.4f, 2.4f), XMMatrixTranslation(-22, 50, 24 + i * 2), XMMatrixScaling(1, 1, 1), index_cache++);
    }

    //FR pyr
    //front row pyr
    for (int i = 0; i < kNumPyramids; ++i) {
        BuildOneRenderItem("pyramid", "pyramidGeo", "stone0", XMMatrixRotationY(XMConvertToRadians(-90)), XMMatrixScaling(2.4f, 2.4f, 2.4f), XMMatrixTranslation(22 + i * 2, 50, -28), XMMatrixScaling(1, 1, 1), index_cache++); //step = 2, min = -28
    }
    //back row pyr
    for (int i = 0; i < kNumPyramids; ++i) {
        BuildOneRenderItem("pyramid", "pyramidGeo", "stone0", XMMatrixRotationY(XMConvertToRadians(90)), XMMatrixScaling(2.4f, 2.4f, 2.4f), XMMatrixTranslation(22 + i * 2, 50, -22), XMMatrixScaling(1, 1, 1), index_cache++);
    }
    //left col pyr
    for (int i = 0; i < kHalfNumPyramids; ++i) {
        BuildOneRenderItem("pyramid", "pyramidGeo", "stone0", XMMatrixScaling(2.4f, 2.4f, 2.4f), XMMatrixTranslation(22, 50, -26 + i * 2), XMMatrixScaling(1, 1, 1), index_cache++);
    }
    //right col pyr
    for (int i = 0; i < kHalfNumPyramids; ++i) {
        BuildOneRenderItem("pyramid", "pyramidGeo", "stone0", XMMatrixRotationY(XMConvertToRadians(180)), XMMatrixScaling(2.4f, 2.4f, 2.4f), XMMatrixTranslation(28, 50, -26 + i * 2), XMMatrixScaling(1, 1, 1), index_cache++);
    }
    
    //RR pyr
    //front row pyr
    for (int i = 0; i < kNumPyramids; ++i) {
        BuildOneRenderItem("pyramid", "pyramidGeo", "stone0", XMMatrixRotationY(XMConvertToRadians(-90)), XMMatrixScaling(2.4f, 2.4f, 2.4f), XMMatrixTranslation(22 + i * 2, 50, 22), XMMatrixScaling(1, 1, 1), index_cache++); //step = 2, min = -28
    }
    //back row pyr
    for (int i = 0; i < kNumPyramids; ++i) {
        BuildOneRenderItem("pyramid", "pyramidGeo", "stone0", XMMatrixRotationY(XMConvertToRadians(90)), XMMatrixScaling(2.4f, 2.4f, 2.4f), XMMatrixTranslation(22 + i * 2, 50, 28), XMMatrixScaling(1, 1, 1), index_cache++);
    }
    //left col pyr
    for (int i = 0; i < kHalfNumPyramids; ++i) {
        BuildOneRenderItem("pyramid", "pyramidGeo", "stone0", XMMatrixScaling(2.4f, 2.4f, 2.4f), XMMatrixTranslation(22, 50, 24 + i * 2), XMMatrixScaling(1, 1, 1), index_cache++);
    }
    //right col pyr
    for (int i = 0; i < kHalfNumPyramids; ++i) {
        BuildOneRenderItem("pyramid", "pyramidGeo", "stone0", XMMatrixRotationY(XMConvertToRadians(180)), XMMatrixScaling(2.4f, 2.4f, 2.4f), XMMatrixTranslation(28, 50, 24 + i * 2), XMMatrixScaling(1, 1, 1), index_cache++);
    }

    const int kNumDiamonds = 2;
    // front charms
    for (int i = 0; i < kNumDiamonds; ++i) {
        BuildOneRenderItem("charm", "charmGeo", "stone0", XMMatrixRotationRollPitchYaw(XMConvertToRadians(0), XMConvertToRadians(45), XMConvertToRadians(0)), XMMatrixScaling(3, 8, 3), XMMatrixTranslation(-25 + i * 50, 34, -29), XMMatrixScaling(1, 1, 1), index_cache++);
    }
    // back charms
    for (int i = 0; i < kNumDiamonds; ++i) {
        BuildOneRenderItem("charm", "charmGeo", "stone0", XMMatrixRotationRollPitchYaw(XMConvertToRadians(0), XMConvertToRadians(45), XMConvertToRadians(0)), XMMatrixScaling(3, 8, 3), XMMatrixTranslation(-25 + i * 50, 34, 29), XMMatrixScaling(1, 1, 1), index_cache++);
    }
    // left charms
    for (int i = 0; i < kNumDiamonds; ++i) {
        BuildOneRenderItem("charm", "charmGeo", "stone0", XMMatrixRotationRollPitchYaw(XMConvertToRadians(0), XMConvertToRadians(45), XMConvertToRadians(0)), XMMatrixScaling(3, 8, 3), XMMatrixTranslation(29, 34, -25 + i * 50), XMMatrixScaling(1, 1, 1), index_cache++);
    }
    // right charms
    for (int i = 0; i < kNumDiamonds; ++i) {
        BuildOneRenderItem("charm", "charmGeo", "stone0", XMMatrixRotationRollPitchYaw(XMConvertToRadians(0), XMConvertToRadians(45), XMConvertToRadians(0)), XMMatrixScaling(3, 8, 3), XMMatrixTranslation(-29, 34, -25 + i * 50), XMMatrixScaling(1, 1, 1), index_cache++);
    }

    // gates
    BuildOneRenderItem("gate", "gateGeo", "stone0", XMMatrixScaling(16, 24, 5), XMMatrixTranslation(0.0f, 11, -25), XMMatrixScaling(1, 1, 1), index_cache++);
    BuildOneRenderItem("gate", "gateGeo", "stone0", XMMatrixScaling(10, 20, 5), XMMatrixTranslation(0.0f, 11, -12.7f), XMMatrixScaling(1, 1, 1), index_cache++);
    
    //INNER
    // inner building
    BuildOneRenderItem("box", "boxGeo", "stone0", XMMatrixScaling(30, 40, 30), XMMatrixTranslation(0, 20, 0), XMMatrixScaling(1, 1, 1), index_cache++);
    
    // diamond
    BuildOneRenderItem("diamond", "diamondGeo", "stone0", XMMatrixScaling(3, 10, 3), XMMatrixTranslation(0.0f, 52, 0.0f), XMMatrixScaling(1, 1, 1), index_cache++);
    
    // torus
    BuildOneRenderItem("torus", "torusGeo", "stone0", XMMatrixScaling(4, 3, 4), XMMatrixTranslation(0.0f, 40, 0.0f), XMMatrixScaling(1, 1, 1), index_cache++);

    //rolo
    BuildOneRenderItem("rolo", "roloGeo", "stone0", XMMatrixScaling(8, 6, 8), XMMatrixTranslation(0, 44, 0), XMMatrixScaling(1, 1, 1), index_cache++); //step = 4, min = -20

    for (int i = 0; i < 2; ++i)
    {
        // left cylinders
        BuildOneRenderItem("cylinder", "cylinderGeo", "stone0", XMMatrixScaling(2, 28, 2), XMMatrixTranslation(-15, 27, -15 + i * 30), XMMatrixScaling(1, 1, 1), index_cache++);
        // left cones
        BuildOneRenderItem("cone", "coneGeo", "stone0", XMMatrixScaling(4, 7, 4), XMMatrixTranslation(-15, 60, -15 + i * 30), XMMatrixScaling(1, 1, 1), index_cache++);
        // right cylinders
        BuildOneRenderItem("cylinder", "cylinderGeo", "stone0", XMMatrixScaling(2, 28, 2), XMMatrixTranslation(15, 27, -15 + i * 30), XMMatrixScaling(1, 1, 1), index_cache++);
        // right cones
        BuildOneRenderItem("cone", "coneGeo", "stone0", XMMatrixScaling(4, 7, 4), XMMatrixTranslation(15, 60, -15 + i * 30), XMMatrixScaling(1, 1, 1), index_cache++);
    }
    

    // front prism loop
    for (int i = 0; i < 7; ++i)
    {
        BuildOneRenderItem("prism", "prismGeo", "stone0", XMMatrixScaling(2, 2, 2), XMMatrixTranslation(-12 + i * 4, 41, -14), XMMatrixScaling(1, 1, 1), index_cache++); //step = 4, min = -12
    }
    // back prism loop
    for (int i = 0; i < 7; ++i)
    {
        BuildOneRenderItem("prism", "prismGeo", "stone0", XMMatrixRotationY(XMConvertToRadians(180.0f)), XMMatrixScaling(2, 2, 2), XMMatrixTranslation(-12 + i * 4, 41, 14), XMMatrixScaling(1, 1, 1), index_cache++); //step = 4, min = -12
    }
    // left prism loop
    for (int i = 0; i < 7; ++i)
    {
        BuildOneRenderItem("prism", "prismGeo", "stone0", XMMatrixRotationY(XMConvertToRadians(90.0f)), XMMatrixScaling(2, 2, 2), XMMatrixTranslation(-14, 41, -12 + i * 4), XMMatrixScaling(1, 1, 1), index_cache++);
    }
    // right prism loop
    for (int i = 0; i < 7; ++i)
    {
        BuildOneRenderItem("prism", "prismGeo", "stone0", XMMatrixRotationY(XMConvertToRadians(-90.0f)), XMMatrixScaling(2, 2, 2), XMMatrixTranslation(14, 41, -12 + i * 4), XMMatrixScaling(1, 1, 1), index_cache++);
    }

    auto treeSpritesRitem = std::make_unique<RenderItem>();
    treeSpritesRitem->World = MathHelper::Identity4x4();
    treeSpritesRitem->ObjCBIndex = index_cache;
    treeSpritesRitem->Mat = mMaterials["treeSprites"].get();
    treeSpritesRitem->Geo = mGeometries["treeSpritesGeo"].get();
    //step2
    treeSpritesRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
    treeSpritesRitem->IndexCount = treeSpritesRitem->Geo->DrawArgs["points"].IndexCount;
    treeSpritesRitem->StartIndexLocation = treeSpritesRitem->Geo->DrawArgs["points"].StartIndexLocation;
    treeSpritesRitem->BaseVertexLocation = treeSpritesRitem->Geo->DrawArgs["points"].BaseVertexLocation;
    index_cache++;

    mRitemLayer[(int)RenderLayer::AlphaTestedTreeSprites].push_back(treeSpritesRitem.get());
    mAllRitems.push_back(std::move(treeSpritesRitem));

	//// All the render items are opaque.
	//for(auto& e : mAllRitems)
	//	mOpaqueRitems.push_back(e.get());

    ::OutputDebugStringA(">>> BuildRenderItems DONE!\n");
}


//The DrawRenderItems method is invoked in the main Draw call:
void ShapesApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
    ::OutputDebugStringA(">>> DrawRenderItems started...\n");
    
    UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
    UINT matCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));

	auto objectCB = mCurrFrameResource->ObjectCB->Resource();
    auto matCB = mCurrFrameResource->MaterialCB->Resource();

    // For each render item...
    for (size_t i = 0; i < ritems.size(); ++i)
    {
        auto ri = ritems[i];

        cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
        cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
        cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

        CD3DX12_GPU_DESCRIPTOR_HANDLE tex(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
        tex.Offset(ri->Mat->DiffuseSrvHeapIndex, mCbvSrvDescriptorSize);

        D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;
        D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + ri->Mat->MatCBIndex * matCBByteSize;

        cmdList->SetGraphicsRootDescriptorTable(0, tex);
        cmdList->SetGraphicsRootConstantBufferView(1, objCBAddress);
        cmdList->SetGraphicsRootConstantBufferView(3, matCBAddress);

        cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
    }

    ::OutputDebugStringA(">>> DrawRenderItems DONE!\n");
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> ShapesApp::GetStaticSamplers()
{
    // Applications usually only need a handful of samplers.  So just define them all up front
    // and keep them available as part of the root signature.  

    const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
        0, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
        1, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
        2, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
        3, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
        4, // shaderRegister
        D3D12_FILTER_ANISOTROPIC, // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
        0.0f,                             // mipLODBias
        8);                               // maxAnisotropy

    const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
        5, // shaderRegister
        D3D12_FILTER_ANISOTROPIC, // filter
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
        0.0f,                              // mipLODBias
        8);                                // maxAnisotropy

    return {
        pointWrap, pointClamp,
        linearWrap, linearClamp,
        anisotropicWrap, anisotropicClamp };
}

float ShapesApp::GetHillsHeight(float x, float z)const
{
    return 0.3f * (z * sinf(0.1f * x) + x * cosf(0.1f * z));
}

XMFLOAT3 ShapesApp::GetHillsNormal(float x, float z)const
{
    // n = (-df/dx, 1, -df/dz)
    XMFLOAT3 n(
        -0.03f * z * cosf(0.1f * x) - 0.3f * cosf(0.1f * z),
        1.0f,
        -0.3f * sinf(0.1f * x) + 0.03f * x * sinf(0.1f * z));

    XMVECTOR unitNormal = XMVector3Normalize(XMLoadFloat3(&n));
    XMStoreFloat3(&n, unitNormal);

    return n;
}

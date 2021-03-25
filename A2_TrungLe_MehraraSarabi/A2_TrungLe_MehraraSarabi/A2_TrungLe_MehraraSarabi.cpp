﻿//***************************************************************************************
// GAME3111_A2_TrungLe_MehraraSarabi
//
// Hold down '1' key to view scene in wireframe mode.
//***************************************************************************************

#include "../../Common/d3dApp.h"
#include "../../Common/MathHelper.h"
#include "../../Common/UploadBuffer.h"
#include "../../Common/GeometryGenerator.h"
#include "FrameResource.h"

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
    D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

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

    void LoadTextures();
    void BuildRootSignature();
    void BuildDescriptorHeaps();
    void BuildShadersAndInputLayout();
    void BuildOneShapeGeometry(GeometryGenerator::MeshData mesh, SubmeshGeometry& submesh, UINT& vert_offset, UINT& idx_offset);
    void BuildPositionAndColorForAllVertices(GeometryGenerator::MeshData mesh, XMFLOAT4 mesh_color, std::vector<Vertex>& vertices, UINT& vert_idx, std::vector<std::uint16_t>& indices);
    void BuildShapeGeometry();
    void BuildPSOs();
    void BuildFrameResources();
    void BuildMaterials();
    void BuildRenderItems();
    void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);
    void BuildConstantBufferViews();
    void BuildOneRenderItem(std::string shape_type, std::string material, XMMATRIX scale_matrix, XMMATRIX translate_matrix, XMMATRIX tex_scale_matrix, UINT obj_idx);
    void BuildOneRenderItem(std::string shape_type, std::string material, XMMATRIX rotate_matrix, XMMATRIX scale_matrix, XMMATRIX translate_matrix, XMMATRIX tex_scale_matrix, UINT obj_idx);
    
    std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();
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

    std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	// List of all the render items.
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;

	// Render items divided by PSO.
	std::vector<RenderItem*> mOpaqueRitems;

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
    if(!D3DApp::Initialize())
        return false;

    // Reset the command list to prep for initialization commands.
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

    // Get the increment size of a descriptor in this heap type.  This is hardware specific, 
    // so we have to query this information.
    mCbvSrvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    LoadTextures();
    BuildRootSignature();
    BuildDescriptorHeaps();
    BuildShadersAndInputLayout();
    BuildShapeGeometry();
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
}

void ShapesApp::Draw(const GameTimer& gt)
{
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
    mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
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

    DrawRenderItems(mCommandList.Get(), mOpaqueRitems);

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
        mRadius = MathHelper::Clamp(mRadius, 5.0f, 150.0f);
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
    mMainPassCB.Lights[0].Strength = { 0.8f, 0.8f, 0.8f };
    mMainPassCB.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
    mMainPassCB.Lights[1].Strength = { 0.4f, 0.4f, 0.4f };
    mMainPassCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
    mMainPassCB.Lights[2].Strength = { 0.2f, 0.2f, 0.2f };

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);
}

void ShapesApp::LoadTextures() //EDIT TEXTURES HERE
{
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
    tileTex->Filename = L"../../Textures/groundTex.dds";
    ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
        mCommandList.Get(), tileTex->Filename.c_str(),
        tileTex->Resource, tileTex->UploadHeap));

    // BOOM
     auto coneTex = std::make_unique<Texture>();
    coneTex->Name = "coneTex";
    coneTex->Filename = L"../../Textures/coneTex.dds";
    ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
        mCommandList.Get(), coneTex->Filename.c_str(),
        coneTex->Resource, coneTex->UploadHeap));

    auto cylinderTex = std::make_unique<Texture>();
    cylinderTex->Name = "cylinderTex";
    cylinderTex->Filename = L"../../Textures/cylinderTex.dds";
    ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
        mCommandList.Get(), cylinderTex->Filename.c_str(),
        cylinderTex->Resource, cylinderTex->UploadHeap));


    auto innerBoxTex = std::make_unique<Texture>();
    innerBoxTex->Name = "innerBoxTex";
    innerBoxTex->Filename = L"../../Textures/innerBoxTex.dds";
    ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
        mCommandList.Get(), innerBoxTex->Filename.c_str(),
        innerBoxTex->Resource, innerBoxTex->UploadHeap));

    auto outerBoxTex = std::make_unique<Texture>();
    outerBoxTex->Name = "outerBoxTex";
    outerBoxTex->Filename = L"../../Textures/outerBoxTex.dds";
    ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
        mCommandList.Get(), outerBoxTex->Filename.c_str(),
        outerBoxTex->Resource, outerBoxTex->UploadHeap));


    auto diamondTex = std::make_unique<Texture>();
    diamondTex->Name = "diamondTex";
    diamondTex->Filename = L"../../Textures/diamondTex.dds";
    ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
        mCommandList.Get(), diamondTex->Filename.c_str(),
        diamondTex->Resource, diamondTex->UploadHeap));

    
    auto cutPyramidTex = std::make_unique<Texture>();
    cutPyramidTex->Name = "cutPyramidTex";
    cutPyramidTex->Filename = L"../../Textures/cutPyramidTex.dds";
    ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
        mCommandList.Get(), cutPyramidTex->Filename.c_str(),
        cutPyramidTex->Resource, cutPyramidTex->UploadHeap));

    auto holoTex = std::make_unique<Texture>();
    holoTex->Name = "holoTex";
    holoTex->Filename = L"../../Textures/holoTex.dds";
    ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
        mCommandList.Get(), holoTex->Filename.c_str(),
        holoTex->Resource, holoTex->UploadHeap));

    auto redTex = std::make_unique<Texture>();
    redTex->Name = "redTex";
    redTex->Filename = L"../../Textures/redTex.dds";
    ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
        mCommandList.Get(), redTex->Filename.c_str(),
        redTex->Resource, redTex->UploadHeap));

    auto cyanTex = std::make_unique<Texture>();
    cyanTex->Name = "cyanTex";
    cyanTex->Filename = L"../../Textures/cyanTex.dds";
    ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
        mCommandList.Get(), cyanTex->Filename.c_str(),
        cyanTex->Resource, cyanTex->UploadHeap));

    auto navyTex = std::make_unique<Texture>();
    navyTex->Name = "navyTex";
    navyTex->Filename = L"../../Textures/navyTex.dds";
    ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
        mCommandList.Get(), navyTex->Filename.c_str(),
        navyTex->Resource, navyTex->UploadHeap));

    auto brownTex = std::make_unique<Texture>();
    brownTex->Name = "brownTex";
    brownTex->Filename = L"../../Textures/brownTex.dds";
    ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
        mCommandList.Get(), brownTex->Filename.c_str(),
        brownTex->Resource, brownTex->UploadHeap));


    mTextures[bricksTex->Name] = std::move(bricksTex);
    mTextures[stoneTex->Name] = std::move(stoneTex);
    mTextures[tileTex->Name] = std::move(tileTex);
    mTextures[coneTex->Name] = std::move(coneTex);
    mTextures[cylinderTex->Name] = std::move(cylinderTex);
    mTextures[innerBoxTex->Name] = std::move(innerBoxTex);
    mTextures[outerBoxTex->Name] = std::move(outerBoxTex);
    mTextures[diamondTex->Name] = std::move(diamondTex);
    mTextures[cutPyramidTex->Name] = std::move(cutPyramidTex);
    mTextures[holoTex->Name] = std::move(holoTex);
    mTextures[redTex->Name] = std::move(redTex);
    mTextures[cyanTex->Name] = std::move(cyanTex);
    mTextures[navyTex->Name] = std::move(navyTex);
    mTextures[brownTex->Name] = std::move(brownTex);


}



//assuming we have n renter items, we can populate the CBV heap with the following code where descriptors 0 to n-
//1 contain the object CBVs for the 0th frame resource, descriptors n to 2n−1 contains the
//object CBVs for 1st frame resource, descriptors 2n to 3n−1 contain the objects CBVs for
//the 2nd frame resource, and descriptors 3n, 3n + 1, and 3n + 2 contain the pass CBVs for the
//0th, 1st, and 2nd frame resource
void ShapesApp::BuildConstantBufferViews()
{
    UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

    UINT objCount = (UINT)mOpaqueRitems.size();

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
}

//A root signature defines what resources need to be bound to the pipeline before issuing a draw call and
//how those resources get mapped to shader input registers. there is a limit of 64 DWORDs that can be put in a root signature.
void ShapesApp::BuildRootSignature()
{
    CD3DX12_DESCRIPTOR_RANGE texTable;
    texTable.Init(
        D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
        1,  // number of descriptors
        0); // register t0

    // Root parameter can be a table, root descriptor or root constants.
    CD3DX12_ROOT_PARAMETER slotRootParameter[4];

    // Perfomance TIP: Order from most frequent to least frequent.
    slotRootParameter[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
    slotRootParameter[1].InitAsConstantBufferView(0); // register b0
    slotRootParameter[2].InitAsConstantBufferView(1); // register b1
    slotRootParameter[3].InitAsConstantBufferView(2); // register b2

    auto staticSamplers = GetStaticSamplers();

    // A root signature is an array of root parameters.
    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter,
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
}

//If we have 3 frame resources and n render items, then we have three 3n object constant
//buffers and 3 pass constant buffers.Hence we need 3(n + 1) constant buffer views(CBVs).
//Thus we will need to modify our CBV heap to include the additional descriptors :
void ShapesApp::BuildDescriptorHeaps()
{
    //
    // Create the SRV heap.
    //
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = 15;
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
    auto coneTex = mTextures["coneTex"]->Resource;
    auto cylinderTex = mTextures["cylinderTex"]->Resource;
    auto innerBoxTex = mTextures["innerBoxTex"]->Resource;
    auto outerBoxTex = mTextures["outerBoxTex"]->Resource;
    auto diamondTex = mTextures["diamondTex"]->Resource;
    auto cutPyramidTex = mTextures["cutPyramidTex"]->Resource;
    auto holoTex = mTextures["holoTex"]->Resource;
    auto redTex = mTextures["redTex"]->Resource;
    auto cyanTex = mTextures["cyanTex"]->Resource;
    auto navyTex = mTextures["navyTex"]->Resource;
    auto brownTex = mTextures["brownTex"]->Resource;









    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

    srvDesc.Format = bricksTex->GetDesc().Format;
    srvDesc.Texture2D.MipLevels = bricksTex->GetDesc().MipLevels;
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

    hDescriptor.Offset(1, mCbvSrvDescriptorSize);
    srvDesc.Format = coneTex->GetDesc().Format;
    srvDesc.Texture2D.MipLevels = coneTex->GetDesc().MipLevels;
    md3dDevice->CreateShaderResourceView(coneTex.Get(), &srvDesc, hDescriptor);

    hDescriptor.Offset(1, mCbvSrvDescriptorSize);
    srvDesc.Format = cylinderTex->GetDesc().Format;
    srvDesc.Texture2D.MipLevels = cylinderTex->GetDesc().MipLevels;
    md3dDevice->CreateShaderResourceView(cylinderTex.Get(), &srvDesc, hDescriptor);

    hDescriptor.Offset(1, mCbvSrvDescriptorSize);
    srvDesc.Format = innerBoxTex->GetDesc().Format;
    srvDesc.Texture2D.MipLevels = innerBoxTex->GetDesc().MipLevels;
    md3dDevice->CreateShaderResourceView(innerBoxTex.Get(), &srvDesc, hDescriptor);

    hDescriptor.Offset(1, mCbvSrvDescriptorSize);
    srvDesc.Format = outerBoxTex->GetDesc().Format;
    srvDesc.Texture2D.MipLevels = outerBoxTex->GetDesc().MipLevels;
    md3dDevice->CreateShaderResourceView(outerBoxTex.Get(), &srvDesc, hDescriptor);

    hDescriptor.Offset(1, mCbvSrvDescriptorSize);
    srvDesc.Format = diamondTex->GetDesc().Format;
    srvDesc.Texture2D.MipLevels = diamondTex->GetDesc().MipLevels;
    md3dDevice->CreateShaderResourceView(diamondTex.Get(), &srvDesc, hDescriptor);

    hDescriptor.Offset(1, mCbvSrvDescriptorSize);
    srvDesc.Format = cutPyramidTex->GetDesc().Format;
    srvDesc.Texture2D.MipLevels = cutPyramidTex->GetDesc().MipLevels;
    md3dDevice->CreateShaderResourceView(cutPyramidTex.Get(), &srvDesc, hDescriptor);

    hDescriptor.Offset(1, mCbvSrvDescriptorSize);
    srvDesc.Format = holoTex->GetDesc().Format;
    srvDesc.Texture2D.MipLevels = holoTex->GetDesc().MipLevels;
    md3dDevice->CreateShaderResourceView(holoTex.Get(), &srvDesc, hDescriptor);

    hDescriptor.Offset(1, mCbvSrvDescriptorSize);
    srvDesc.Format = redTex->GetDesc().Format;
    srvDesc.Texture2D.MipLevels = redTex->GetDesc().MipLevels;
    md3dDevice->CreateShaderResourceView(redTex.Get(), &srvDesc, hDescriptor);

    hDescriptor.Offset(1, mCbvSrvDescriptorSize);
    srvDesc.Format = cyanTex->GetDesc().Format;
    srvDesc.Texture2D.MipLevels = cyanTex->GetDesc().MipLevels;
    md3dDevice->CreateShaderResourceView(cyanTex.Get(), &srvDesc, hDescriptor);

    hDescriptor.Offset(1, mCbvSrvDescriptorSize);
    srvDesc.Format = navyTex->GetDesc().Format;
    srvDesc.Texture2D.MipLevels = navyTex->GetDesc().MipLevels;
    md3dDevice->CreateShaderResourceView(navyTex.Get(), &srvDesc, hDescriptor);

    hDescriptor.Offset(1, mCbvSrvDescriptorSize);
    srvDesc.Format = brownTex->GetDesc().Format;
    srvDesc.Texture2D.MipLevels = brownTex->GetDesc().MipLevels;
    md3dDevice->CreateShaderResourceView(brownTex.Get(), &srvDesc, hDescriptor);
}

void ShapesApp::BuildShadersAndInputLayout()
{
    const D3D_SHADER_MACRO alphaTestDefines[] =
    {
        "ALPHA_TEST", "1",
        NULL, NULL
    };

    mShaders["standardVS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "VS", "vs_5_0");
    mShaders["opaquePS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "PS", "ps_5_0");

    mInputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
}

void ShapesApp::BuildOneShapeGeometry(GeometryGenerator::MeshData mesh, SubmeshGeometry &submesh, UINT &vert_offset, UINT &idx_offset) {
    // Define the SubmeshGeometry that cover different 
    // regions of the vertex/index buffers.
    submesh.IndexCount = (UINT)mesh.Indices32.size();
    submesh.StartIndexLocation = vert_offset;
    submesh.BaseVertexLocation = idx_offset;
    //
    // We are concatenating all the geometry into one big vertex/index buffer.  So
    // define the regions in the buffer each submesh covers.
    //
    // Cache the vertex offsets to each object in the concatenated vertex buffer.
    vert_offset += (UINT)mesh.Vertices.size();

    // Cache the starting index for each object in the concatenated index buffer.
    idx_offset += (UINT)mesh.Indices32.size();
}

void ShapesApp::BuildPositionAndColorForAllVertices(GeometryGenerator::MeshData mesh, XMFLOAT4 mesh_color, std::vector<Vertex>& vertices, UINT& vert_idx, std::vector<std::uint16_t>& indices) {
    for (size_t i = 0; i < mesh.Vertices.size(); ++i, ++vert_idx) // ADD HERE
    {
        vertices[vert_idx].Pos = mesh.Vertices[i].Position;
        vertices[vert_idx].Normal = mesh.Vertices[i].Normal;
        vertices[vert_idx].TexC = mesh.Vertices[i].TexC;
        //vertices[vert_idx].Color = XMFLOAT4(mesh_color);
    }

    indices.insert(indices.end(), std::begin(mesh.GetIndices16()), std::end(mesh.GetIndices16())); // ADD HERE
}

void ShapesApp::BuildShapeGeometry()
{
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



    mGeometries[geo->Name] = std::move(geo);
}

void ShapesApp::BuildPSOs()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;

	//
	// PSO for opaque objects.
	//
    ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	opaquePsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
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
	opaquePsoDesc.SampleDesc.Count = m4xMsaaState ? 15 : 1;
	opaquePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	opaquePsoDesc.DSVFormat = mDepthStencilFormat;
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque"])));


    //
    // PSO for opaque wireframe objects.
    //
    //D3D12_GRAPHICS_PIPELINE_STATE_DESC opaqueWireframePsoDesc = opaquePsoDesc;
    //opaqueWireframePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
    //ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaqueWireframePsoDesc, IID_PPV_ARGS(&mPSOs["opaque_wireframe"])));
}

void ShapesApp::BuildFrameResources()
{
    for(int i = 0; i < gNumFrameResources; ++i)
    {
        mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
            1, (UINT)mAllRitems.size(), (UINT)mMaterials.size()));
    }
}

void ShapesApp::BuildMaterials() //EDIT MATS HERE
{
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


    auto cone0 = std::make_unique<Material>();
    cone0->Name = "cone0";
    cone0->MatCBIndex = 3;
    cone0->DiffuseSrvHeapIndex = 3;
    cone0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    cone0->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
    cone0->Roughness = 0.3f;

    auto cylinder0 = std::make_unique<Material>();
    cylinder0->Name = "cylinder0";
    cylinder0->MatCBIndex = 4;
    cylinder0->DiffuseSrvHeapIndex = 4;
    cylinder0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    cylinder0->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
    cylinder0->Roughness = 0.3f;


    auto inner0 = std::make_unique<Material>();
    inner0->Name = "inner0";
    inner0->MatCBIndex = 5;
    inner0->DiffuseSrvHeapIndex = 5;
    inner0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    inner0->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
    inner0->Roughness = 0.3f;

    auto outer0 = std::make_unique<Material>();
    outer0->Name = "outer0";
    outer0->MatCBIndex = 6;
    outer0->DiffuseSrvHeapIndex = 6;
    outer0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    outer0->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
    outer0->Roughness = 0.3f;  

    auto diamond0 = std::make_unique<Material>();
    diamond0->Name = "diamond0";
    diamond0->MatCBIndex = 7;
    diamond0->DiffuseSrvHeapIndex = 7;
    diamond0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    diamond0->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
    diamond0->Roughness = 0.3f;

    auto cutPyr0 = std::make_unique<Material>();
    cutPyr0->Name = "cutPyr0";
    cutPyr0->MatCBIndex = 8;
    cutPyr0->DiffuseSrvHeapIndex = 8;
    cutPyr0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    cutPyr0->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
    cutPyr0->Roughness = 0.3f;

    auto holo0 = std::make_unique<Material>();
    holo0->Name = "holo0";
    holo0->MatCBIndex = 9;
    holo0->DiffuseSrvHeapIndex = 9;
    holo0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    holo0->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
    holo0->Roughness = 0.3f;

    auto red0 = std::make_unique<Material>();
    red0->Name = "red0";
    red0->MatCBIndex = 10;
    red0->DiffuseSrvHeapIndex = 10;
    red0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    red0->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
    red0->Roughness = 0.3f;

    auto cyan0 = std::make_unique<Material>();
    cyan0->Name = "cyan0";
    cyan0->MatCBIndex = 11;
    cyan0->DiffuseSrvHeapIndex = 11;
    cyan0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    cyan0->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
    cyan0->Roughness = 0.3f;

    auto navy0 = std::make_unique<Material>();
    navy0->Name = "navy0";
    navy0->MatCBIndex = 12;
    navy0->DiffuseSrvHeapIndex = 12;
    navy0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    navy0->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
    navy0->Roughness = 0.3f;

    auto brown0 = std::make_unique<Material>();
    brown0->Name = "brown0";
    brown0->MatCBIndex = 13;
    brown0->DiffuseSrvHeapIndex = 13;
    brown0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    brown0->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
    brown0->Roughness = 0.3f;

    mMaterials["bricks0"] = std::move(bricks0);
    mMaterials["stone0"] = std::move(stone0);
    mMaterials["tile0"] = std::move(tile0);
    mMaterials["cone0"] = std::move(cone0);
    mMaterials["cylinder0"] = std::move(cylinder0);
    mMaterials["inner0"] = std::move(inner0);
    mMaterials["outer0"] = std::move(outer0);
    mMaterials["diamond0"] = std::move(diamond0);
    mMaterials["cutPyr0"] = std::move(cutPyr0);
    mMaterials["holo0"] = std::move(holo0);
    mMaterials["red0"] = std::move(red0);
    mMaterials["cyan0"] = std::move(cyan0);
    mMaterials["navy0"] = std::move(navy0);
    mMaterials["brown0"] = std::move(brown0);





}


void ShapesApp::BuildOneRenderItem(std::string shape_type, std::string material, XMMATRIX scale_matrix, XMMATRIX translate_matrix, XMMATRIX tex_scale_matrix, UINT obj_idx)
{
    auto shape_render_item = std::make_unique<RenderItem>();
    XMStoreFloat4x4(&shape_render_item->World, scale_matrix * translate_matrix);
    XMStoreFloat4x4(&shape_render_item->TexTransform, tex_scale_matrix);
    shape_render_item->ObjCBIndex = obj_idx;
    shape_render_item->Mat = mMaterials[material].get();
    shape_render_item->Geo = mGeometries["shapeGeo"].get();
    shape_render_item->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    shape_render_item->IndexCount = shape_render_item->Geo->DrawArgs[shape_type].IndexCount;
    shape_render_item->StartIndexLocation = shape_render_item->Geo->DrawArgs[shape_type].StartIndexLocation;
    shape_render_item->BaseVertexLocation = shape_render_item->Geo->DrawArgs[shape_type].BaseVertexLocation;
    mAllRitems.push_back(std::move(shape_render_item));
}

void ShapesApp::BuildOneRenderItem(std::string shape_type, std::string material, XMMATRIX rotate_matrix, XMMATRIX scale_matrix, XMMATRIX translate_matrix, XMMATRIX tex_scale_matrix, UINT obj_idx)
{
    auto shape_render_item = std::make_unique<RenderItem>();
    XMStoreFloat4x4(&shape_render_item->World, scale_matrix * rotate_matrix * translate_matrix);
    XMStoreFloat4x4(&shape_render_item->TexTransform, tex_scale_matrix);
    shape_render_item->ObjCBIndex = obj_idx;
    shape_render_item->Mat = mMaterials[material].get();
    shape_render_item->Geo = mGeometries["shapeGeo"].get();
    shape_render_item->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    shape_render_item->IndexCount = shape_render_item->Geo->DrawArgs[shape_type].IndexCount;
    shape_render_item->StartIndexLocation = shape_render_item->Geo->DrawArgs[shape_type].StartIndexLocation;
    shape_render_item->BaseVertexLocation = shape_render_item->Geo->DrawArgs[shape_type].BaseVertexLocation;
    mAllRitems.push_back(std::move(shape_render_item));
}

//
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
    UINT index_cache = 0;

    // grid
    BuildOneRenderItem("grid", "tile0", XMMatrixScaling(2, 2, 2), XMMatrixTranslation(0.0f, 0.0f, 0.0f), XMMatrixScaling(1, 1, 1), index_cache++);

    // OUTTER
    // front wall 
    BuildOneRenderItem("outterWall", "outer0", XMMatrixScaling(50.0f, 40, 4.0f), XMMatrixTranslation(0.0f, 19, -25.0f), XMMatrixScaling(1, 1, 1), index_cache++);
    // back wall
    BuildOneRenderItem("outterWall", "outer0", XMMatrixScaling(50.0f, 40, 4.0f), XMMatrixTranslation(0.0f, 19, 25.0f), XMMatrixScaling(1, 1, 1), index_cache++);
    // left wall 
    BuildOneRenderItem("outterWall", "outer0", XMMatrixRotationY(XMConvertToRadians(-90.0f)), XMMatrixScaling(50.0f, 40, 4.0f), XMMatrixTranslation(-25.0f, 19, 0.0f), XMMatrixScaling(1, 1, 1), index_cache++);
    // right wall
    BuildOneRenderItem("outterWall", "outer0", XMMatrixRotationY(XMConvertToRadians(90.0f)), XMMatrixScaling(50.0f, 40, 4.0f), XMMatrixTranslation(25.0f, 19, 0.0f), XMMatrixScaling(1, 1, 1), index_cache++);
    
    const int kNumWallWedges = 12;
    // front wedge loop
    for (int i = 0; i < kNumWallWedges; ++i){
        BuildOneRenderItem("wedge", "cyan0", XMMatrixRotationY(XMConvertToRadians(180.0f)), XMMatrixScaling(2, 2, 2), XMMatrixTranslation(-20 + i * 4, 40, -26), XMMatrixScaling(1, 1, 1), index_cache++); //step = 4, min = -20
    }
    // back wedge loop
    for (int i = 0; i < kNumWallWedges; ++i){
        BuildOneRenderItem("wedge", "cyan0", XMMatrixScaling(2, 2, 2), XMMatrixTranslation(-20 + i * 4, 40, 26), XMMatrixScaling(1, 1, 1), index_cache++); //step = 4, min = -20
    }
    // left wedge loop
    for (int i = 0; i < kNumWallWedges; ++i){
        BuildOneRenderItem("wedge", "cyan0", XMMatrixRotationY(XMConvertToRadians(-90)), XMMatrixScaling(2, 2, 2), XMMatrixTranslation(-26, 40, -20 + i * 4), XMMatrixScaling(1, 1, 1), index_cache++); //step = 4, min = -20
    }
    // right wedge loop
    for (int i = 0; i < kNumWallWedges; ++i){
        BuildOneRenderItem("wedge", "cyan0", XMMatrixRotationY(XMConvertToRadians(90)), XMMatrixScaling(2, 2, 2), XMMatrixTranslation(26, 40, -20 + i * 4), XMMatrixScaling(1, 1, 1), index_cache++); //step = 4, min = -20
    }
    
    const int kNumWallPyramids = 6;
    // left pyramid loop
    for (int i = 0; i < kNumWallPyramids; ++i){
        BuildOneRenderItem("truncatedPyramid", "cutPyr0", XMMatrixRotationRollPitchYaw(XMConvertToRadians(0), XMConvertToRadians(0), XMConvertToRadians(90)), XMMatrixScaling(3, 2, 3), XMMatrixTranslation(-28, 34, -17.5 + i * 7), XMMatrixScaling(1, 1, 1), index_cache++); //step = 4, min = -20
    }
    // right pyramid loop
    for (int i = 0; i < kNumWallPyramids; ++i) {
        BuildOneRenderItem("truncatedPyramid", "cutPyr0", XMMatrixRotationRollPitchYaw(XMConvertToRadians(0), XMConvertToRadians(0), XMConvertToRadians(-90)), XMMatrixScaling(3, 2, 3), XMMatrixTranslation(28, 34, -17.5 + i * 7), XMMatrixScaling(1, 1, 1), index_cache++); //step = 4, min = -20
    }
    // front rolo loop
    for (int i = 0; i < kNumWallPyramids; ++i) {
        BuildOneRenderItem("rolo", "holo0", XMMatrixRotationRollPitchYaw(XMConvertToRadians(-90), XMConvertToRadians(0), XMConvertToRadians(0)), XMMatrixScaling(2, 2, 2), XMMatrixTranslation(-17.5 + i * 7, 34, -28), XMMatrixScaling(1, 1, 1), index_cache++); //step = 4, min = -20
    }
    // back rolo loop
    for (int i = 0; i < kNumWallPyramids; ++i) {
        BuildOneRenderItem("rolo", "holo0", XMMatrixRotationRollPitchYaw(XMConvertToRadians(90), XMConvertToRadians(0), XMConvertToRadians(0)), XMMatrixScaling(2, 2, 2), XMMatrixTranslation(-17.5 + i * 7, 34, 28), XMMatrixScaling(1, 1, 1), index_cache++); //step = 4, min = -20
    }

    // outer towers
    for (int i = 0; i < 2; ++i)
    {
        // left towers
        BuildOneRenderItem("tower", "navy0", XMMatrixScaling(8, 50, 8), XMMatrixTranslation(-25, 24, -25 + i * 50), XMMatrixScaling(1, 1, 1), index_cache++);
        //right towers
        BuildOneRenderItem("tower", "navy0", XMMatrixScaling(8, 50, 8), XMMatrixTranslation(25, 24, -25 + i * 50), XMMatrixScaling(1, 1, 1), index_cache++);
    }
    
    const int kNumPyramids = 4;
    const int kHalfNumPyramids = 2;
    //FL pyr
    //front row pyr
    for (int i = 0; i < kNumPyramids; ++i) {
        BuildOneRenderItem("pyramid", "cutPyr0", XMMatrixRotationY(XMConvertToRadians(-90)), XMMatrixScaling(2.4f, 2.4f, 2.4f), XMMatrixTranslation(-28 + i * 2, 50, -28), XMMatrixScaling(1, 1, 1), index_cache++); //step = 2, min = -28
    }
    //back row pyr
    for (int i = 0; i < kNumPyramids; ++i) {
        BuildOneRenderItem("pyramid", "cutPyr0", XMMatrixRotationY(XMConvertToRadians(90)), XMMatrixScaling(2.4f, 2.4f, 2.4f), XMMatrixTranslation(-28 + i * 2, 50, -22), XMMatrixScaling(1, 1, 1), index_cache++);
    }
    //left col pyr
    for (int i = 0; i < kHalfNumPyramids; ++i) {
        BuildOneRenderItem("pyramid", "cutPyr0", XMMatrixScaling(2.4f, 2.4f, 2.4f), XMMatrixTranslation(-28, 50, -26 + i * 2), XMMatrixScaling(1, 1, 1), index_cache++);
    }
    //right col pyr
    for (int i = 0; i < kHalfNumPyramids; ++i) {
        BuildOneRenderItem("pyramid", "cutPyr0", XMMatrixRotationY(XMConvertToRadians(180)), XMMatrixScaling(2.4f, 2.4f, 2.4f), XMMatrixTranslation(-22, 50, -26 + i * 2), XMMatrixScaling(1, 1, 1), index_cache++);
    }

    //RL pyr
    //front row pyr
    for (int i = 0; i < kNumPyramids; ++i) {
        BuildOneRenderItem("pyramid", "cutPyr0", XMMatrixRotationY(XMConvertToRadians(-90)), XMMatrixScaling(2.4f, 2.4f, 2.4f), XMMatrixTranslation(-28 + i * 2, 50, 28), XMMatrixScaling(1, 1, 1), index_cache++); //step = 2, min = -28
    }
    //back row pyr
    for (int i = 0; i < kNumPyramids; ++i) {
        BuildOneRenderItem("pyramid", "cutPyr0", XMMatrixRotationY(XMConvertToRadians(90)), XMMatrixScaling(2.4f, 2.4f, 2.4f), XMMatrixTranslation(-28 + i * 2, 50, 22), XMMatrixScaling(1, 1, 1), index_cache++);
    }
    //left col pyr
    for (int i = 0; i < kHalfNumPyramids; ++i) {
        BuildOneRenderItem("pyramid", "cutPyr0", XMMatrixScaling(2.4f, 2.4f, 2.4f), XMMatrixTranslation(-28, 50, 24 + i * 2), XMMatrixScaling(1, 1, 1), index_cache++);
    }
    //right col pyr
    for (int i = 0; i < kHalfNumPyramids; ++i) {
        BuildOneRenderItem("pyramid", "cutPyr0", XMMatrixRotationY(XMConvertToRadians(180)), XMMatrixScaling(2.4f, 2.4f, 2.4f), XMMatrixTranslation(-22, 50, 24 + i * 2), XMMatrixScaling(1, 1, 1), index_cache++);
    }

    //FR pyr
    //front row pyr
    for (int i = 0; i < kNumPyramids; ++i) {
        BuildOneRenderItem("pyramid", "cutPyr0", XMMatrixRotationY(XMConvertToRadians(-90)), XMMatrixScaling(2.4f, 2.4f, 2.4f), XMMatrixTranslation(22 + i * 2, 50, -28), XMMatrixScaling(1, 1, 1), index_cache++); //step = 2, min = -28
    }
    //back row pyr
    for (int i = 0; i < kNumPyramids; ++i) {
        BuildOneRenderItem("pyramid", "cutPyr0", XMMatrixRotationY(XMConvertToRadians(90)), XMMatrixScaling(2.4f, 2.4f, 2.4f), XMMatrixTranslation(22 + i * 2, 50, -22), XMMatrixScaling(1, 1, 1), index_cache++);
    }
    //left col pyr
    for (int i = 0; i < kHalfNumPyramids; ++i) {
        BuildOneRenderItem("pyramid", "cutPyr0", XMMatrixScaling(2.4f, 2.4f, 2.4f), XMMatrixTranslation(22, 50, -26 + i * 2), XMMatrixScaling(1, 1, 1), index_cache++);
    }
    //right col pyr
    for (int i = 0; i < kHalfNumPyramids; ++i) {
        BuildOneRenderItem("pyramid", "cutPyr0", XMMatrixRotationY(XMConvertToRadians(180)), XMMatrixScaling(2.4f, 2.4f, 2.4f), XMMatrixTranslation(28, 50, -26 + i * 2), XMMatrixScaling(1, 1, 1), index_cache++);
    }
    
    //RR pyr
    //front row pyr
    for (int i = 0; i < kNumPyramids; ++i) {
        BuildOneRenderItem("pyramid", "cutPyr0", XMMatrixRotationY(XMConvertToRadians(-90)), XMMatrixScaling(2.4f, 2.4f, 2.4f), XMMatrixTranslation(22 + i * 2, 50, 22), XMMatrixScaling(1, 1, 1), index_cache++); //step = 2, min = -28
    }
    //back row pyr
    for (int i = 0; i < kNumPyramids; ++i) {
        BuildOneRenderItem("pyramid", "cutPyr0", XMMatrixRotationY(XMConvertToRadians(90)), XMMatrixScaling(2.4f, 2.4f, 2.4f), XMMatrixTranslation(22 + i * 2, 50, 28), XMMatrixScaling(1, 1, 1), index_cache++);
    }
    //left col pyr
    for (int i = 0; i < kHalfNumPyramids; ++i) {
        BuildOneRenderItem("pyramid", "cutPyr0", XMMatrixScaling(2.4f, 2.4f, 2.4f), XMMatrixTranslation(22, 50, 24 + i * 2), XMMatrixScaling(1, 1, 1), index_cache++);
    }
    //right col pyr
    for (int i = 0; i < kHalfNumPyramids; ++i) {
        BuildOneRenderItem("pyramid", "cutPyr0", XMMatrixRotationY(XMConvertToRadians(180)), XMMatrixScaling(2.4f, 2.4f, 2.4f), XMMatrixTranslation(28, 50, 24 + i * 2), XMMatrixScaling(1, 1, 1), index_cache++);
    }

    const int kNumDiamonds = 2;
    // front charms
    for (int i = 0; i < kNumDiamonds; ++i) {
        BuildOneRenderItem("charm", "diamond0", XMMatrixRotationRollPitchYaw(XMConvertToRadians(0), XMConvertToRadians(45), XMConvertToRadians(0)), XMMatrixScaling(3, 8, 3), XMMatrixTranslation(-25 + i * 50, 34, -29), XMMatrixScaling(1, 1, 1), index_cache++);
    }
    // back charms
    for (int i = 0; i < kNumDiamonds; ++i) {
        BuildOneRenderItem("charm", "diamond0", XMMatrixRotationRollPitchYaw(XMConvertToRadians(0), XMConvertToRadians(45), XMConvertToRadians(0)), XMMatrixScaling(3, 8, 3), XMMatrixTranslation(-25 + i * 50, 34, 29), XMMatrixScaling(1, 1, 1), index_cache++);
    }
    // left charms
    for (int i = 0; i < kNumDiamonds; ++i) {
        BuildOneRenderItem("charm", "diamond0", XMMatrixRotationRollPitchYaw(XMConvertToRadians(0), XMConvertToRadians(45), XMConvertToRadians(0)), XMMatrixScaling(3, 8, 3), XMMatrixTranslation(29, 34, -25 + i * 50), XMMatrixScaling(1, 1, 1), index_cache++);
    }
    // right charms
    for (int i = 0; i < kNumDiamonds; ++i) {
        BuildOneRenderItem("charm", "diamond0", XMMatrixRotationRollPitchYaw(XMConvertToRadians(0), XMConvertToRadians(45), XMConvertToRadians(0)), XMMatrixScaling(3, 8, 3), XMMatrixTranslation(-29, 34, -25 + i * 50), XMMatrixScaling(1, 1, 1), index_cache++);
    }

    // gates
    BuildOneRenderItem("gate", "stone0", XMMatrixScaling(16, 24, 5), XMMatrixTranslation(0.0f, 11, -25), XMMatrixScaling(1, 1, 1), index_cache++);
    BuildOneRenderItem("gate", "stone0", XMMatrixScaling(10, 20, 5), XMMatrixTranslation(0.0f, 11, -12.7f), XMMatrixScaling(1, 1, 1), index_cache++);
    
    //INNER
    // inner building
    BuildOneRenderItem("box", "inner0", XMMatrixScaling(30, 40, 30), XMMatrixTranslation(0, 20, 0), XMMatrixScaling(1, 1, 1), index_cache++);
    
    // diamond
    BuildOneRenderItem("diamond", "diamond0", XMMatrixScaling(3, 10, 3), XMMatrixTranslation(0.0f, 52, 0.0f), XMMatrixScaling(1, 1, 1), index_cache++);
    
    // torus
    BuildOneRenderItem("torus", "brown0", XMMatrixScaling(4, 3, 4), XMMatrixTranslation(0.0f, 40, 0.0f), XMMatrixScaling(1, 1, 1), index_cache++);

    //rolo
    BuildOneRenderItem("rolo", "red0", XMMatrixScaling(8, 6, 8), XMMatrixTranslation(0, 44, 0), XMMatrixScaling(1, 1, 1), index_cache++); //step = 4, min = -20

    for (int i = 0; i < 2; ++i)
    {
        // left cylinders
        BuildOneRenderItem("cylinder", "cylinder0", XMMatrixScaling(2, 28, 2), XMMatrixTranslation(-15, 27, -15 + i * 30), XMMatrixScaling(1, 1, 1), index_cache++);
        // left cones
        BuildOneRenderItem("cone", "cone0", XMMatrixScaling(4, 7, 4), XMMatrixTranslation(-15, 60, -15 + i * 30), XMMatrixScaling(1, 1, 1), index_cache++);
        // right cylinders
        BuildOneRenderItem("cylinder", "cylinder0", XMMatrixScaling(2, 28, 2), XMMatrixTranslation(15, 27, -15 + i * 30), XMMatrixScaling(1, 1, 1), index_cache++);
        // right cones
        BuildOneRenderItem("cone", "cone0", XMMatrixScaling(4, 7, 4), XMMatrixTranslation(15, 60, -15 + i * 30), XMMatrixScaling(1, 1, 1), index_cache++);
    }
    

    // front prism loop
    for (int i = 0; i < 7; ++i)
    {
        BuildOneRenderItem("prism", "red0", XMMatrixScaling(2, 2, 2), XMMatrixTranslation(-12 + i * 4, 41, -14), XMMatrixScaling(1, 1, 1), index_cache++); //step = 4, min = -12
    }
    // back prism loop
    for (int i = 0; i < 7; ++i)
    {
        BuildOneRenderItem("prism", "red0", XMMatrixRotationY(XMConvertToRadians(180.0f)), XMMatrixScaling(2, 2, 2), XMMatrixTranslation(-12 + i * 4, 41, 14), XMMatrixScaling(1, 1, 1), index_cache++); //step = 4, min = -12
    }
    // left prism loop
    for (int i = 0; i < 7; ++i)
    {
        BuildOneRenderItem("prism", "red0", XMMatrixRotationY(XMConvertToRadians(90.0f)), XMMatrixScaling(2, 2, 2), XMMatrixTranslation(-14, 41, -12 + i * 4), XMMatrixScaling(1, 1, 1), index_cache++);
    }
    // right prism loop
    for (int i = 0; i < 7; ++i)
    {
        BuildOneRenderItem("prism", "red0", XMMatrixRotationY(XMConvertToRadians(-90.0f)), XMMatrixScaling(2, 2, 2), XMMatrixTranslation(14, 41, -12 + i * 4), XMMatrixScaling(1, 1, 1), index_cache++);
    }

	// All the render items are opaque.
	for(auto& e : mAllRitems)
		mOpaqueRitems.push_back(e.get());
}


//The DrawRenderItems method is invoked in the main Draw call:
void ShapesApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
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


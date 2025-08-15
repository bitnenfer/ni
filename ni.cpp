#define WIN32_LEAN_AND_MEAN 1
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <Windows.h>
#include <Windowsx.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#include <d3d12.h>
#include <dxgidebug.h>
#include <shlobj.h>
#include <string>
#include <strsafe.h>
#include <new>
#include <random>
#include <chrono>

#include "ni.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "d3dcompiler.lib")

#define NI_UTILS_WINDOWS_LOG_MAX_BUFFER_SIZE  4096
#define NI_UTILS_WINDOWS_LOG_MAX_BUFFER_COUNT 4

static bool keysDown[512];
static bool mouseBtnsDown[3];
static bool mouseBtnsClick[3];
static char lastChar = 0;
static ni::Renderer renderer = {};
static void loadPIX() {
    if (GetModuleHandleA("WinPixGpuCapture.dll") == 0) {

        LPWSTR programFilesPath = nullptr;
        SHGetKnownFolderPath(FOLDERID_ProgramFiles, KF_FLAG_DEFAULT, NULL,
            &programFilesPath);

        std::wstring pixSearchPath =
            programFilesPath + std::wstring(L"\\Microsoft PIX\\*");

        WIN32_FIND_DATAW findData;
        bool foundPixInstallation = false;
        wchar_t newestVersionFound[MAX_PATH];

        HANDLE hFind = FindFirstFileW(pixSearchPath.c_str(), &findData);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ==
                    FILE_ATTRIBUTE_DIRECTORY) &&
                    (findData.cFileName[0] != '.')) {
                    if (!foundPixInstallation ||
                        wcscmp(newestVersionFound, findData.cFileName) <= 0) {
                        foundPixInstallation = true;
                        StringCchCopyW(newestVersionFound,
                            _countof(newestVersionFound),
                            findData.cFileName);
                    }
                }
            } while (FindNextFileW(hFind, &findData) != 0);
        }

        FindClose(hFind);

        if (foundPixInstallation) {
            wchar_t output[MAX_PATH];
            StringCchCopyW(output, pixSearchPath.length(),
                pixSearchPath.data());
            StringCchCatW(output, MAX_PATH, &newestVersionFound[0]);
            StringCchCatW(output, MAX_PATH, L"\\WinPixGpuCapturer.dll");
            LoadLibraryW(output);
        }
    }
}

ni::RootSignatureDescriptorRange& ni::RootSignatureDescriptorRange::addRange(D3D12_DESCRIPTOR_RANGE_TYPE type, uint32_t descriptorNum, uint32_t baseShaderRegister, uint32_t registerSpace) {
    D3D12_DESCRIPTOR_RANGE range = {};
    range.RangeType = type;
    range.NumDescriptors = descriptorNum;
    range.BaseShaderRegister = baseShaderRegister;
    range.RegisterSpace = registerSpace;
    range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    ranges.add(range);
    return *this;
}

void ni::RootSignatureBuilder::addRootParameterCBV(uint32_t shaderRegister, uint32_t registerSpace, D3D12_SHADER_VISIBILITY shaderVisibility) {
    D3D12_ROOT_PARAMETER rootParam = {};
    rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParam.Descriptor.ShaderRegister = shaderRegister;
    rootParam.Descriptor.RegisterSpace = registerSpace;
    rootParam.ShaderVisibility = shaderVisibility;
    rootParameters.add(rootParam);
}

void ni::RootSignatureBuilder::addRootParameterSRV(uint32_t shaderRegister, uint32_t registerSpace, D3D12_SHADER_VISIBILITY shaderVisibility) {
    D3D12_ROOT_PARAMETER rootParam = {};
    rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    rootParam.Descriptor.ShaderRegister = shaderRegister;
    rootParam.Descriptor.RegisterSpace = registerSpace;
    rootParam.ShaderVisibility = shaderVisibility;
    rootParameters.add(rootParam);
}

void ni::RootSignatureBuilder::addRootParameterUAV(uint32_t shaderRegister, uint32_t registerSpace, D3D12_SHADER_VISIBILITY shaderVisibility) {
    D3D12_ROOT_PARAMETER rootParam = {};
    rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
    rootParam.Descriptor.ShaderRegister = shaderRegister;
    rootParam.Descriptor.RegisterSpace = registerSpace;
    rootParam.ShaderVisibility = shaderVisibility;
    rootParameters.add(rootParam);
}

void ni::RootSignatureBuilder::addRootParameterDescriptorTable(const D3D12_DESCRIPTOR_RANGE* ranges, uint32_t rangeNum, D3D12_SHADER_VISIBILITY shaderVisibility) {
    D3D12_ROOT_PARAMETER rootParam = {};
    rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParam.DescriptorTable.pDescriptorRanges = ranges;
    rootParam.DescriptorTable.NumDescriptorRanges = rangeNum;
    rootParam.ShaderVisibility = shaderVisibility;
    rootParameters.add(rootParam);
}
void ni::RootSignatureBuilder::addRootParameterDescriptorTable(const RootSignatureDescriptorRange& ranges, D3D12_SHADER_VISIBILITY shaderVisibility) {
    addRootParameterDescriptorTable(*ranges, ranges.getNum(), shaderVisibility);
}
void ni::RootSignatureBuilder::addRootParameterConstant(uint32_t shaderRegister, uint32_t registerSpace, uint32_t num32BitValues, D3D12_SHADER_VISIBILITY shaderVisibility) {
    D3D12_ROOT_PARAMETER rootParam = {};
    rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rootParam.Constants.ShaderRegister = shaderRegister;
    rootParam.Constants.RegisterSpace = registerSpace;
    rootParam.Constants.Num32BitValues = num32BitValues;
    rootParam.ShaderVisibility = shaderVisibility;
    rootParameters.add(rootParam);
}
void ni::RootSignatureBuilder::addStaticSampler(D3D12_FILTER filter, D3D12_TEXTURE_ADDRESS_MODE addressModeAll, uint32_t shaderRegister, uint32_t registerSpace, D3D12_SHADER_VISIBILITY shaderVisibility) {
    D3D12_STATIC_SAMPLER_DESC staticSampler = {};
    staticSampler.Filter = filter;
    staticSampler.AddressU = addressModeAll;
    staticSampler.AddressV = addressModeAll;
    staticSampler.AddressW = addressModeAll;
    staticSampler.MipLODBias = 0.0f;
    staticSampler.MaxAnisotropy = 0;
    staticSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    staticSampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
    staticSampler.MinLOD = 0;
    staticSampler.MaxLOD = 0;
    staticSampler.ShaderRegister = shaderRegister;
    staticSampler.RegisterSpace = registerSpace;
    staticSampler.ShaderVisibility = shaderVisibility;
    staticSamplers.add(staticSampler);
}

ID3D12RootSignature* ni::RootSignatureBuilder::build(bool isCompute) {
    ID3DBlob* rootSignatureBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;
    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    rootSignatureDesc.NumParameters = rootParameters.getNum();
    rootSignatureDesc.pParameters = rootParameters.getData();
    rootSignatureDesc.NumStaticSamplers = staticSamplers.getNum();
    rootSignatureDesc.pStaticSamplers = staticSamplers.getData();
    rootSignatureDesc.Flags = isCompute ? D3D12_ROOT_SIGNATURE_FLAG_NONE : D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    if (D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &rootSignatureBlob, &errorBlob) != S_OK) {
        NI_PANIC("Failed to serialize root signature.\n%s", errorBlob->GetBufferPointer());
    }
    ID3D12RootSignature* rootSignature = nullptr;
    NI_D3D_ASSERT(renderer.device->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(), rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature)), "Failed to create root signature");
    return rootSignature;
}

void ni::DescriptorAllocator::reset() {
    descriptorAllocated = 0;
}

ni::DescriptorTable ni::DescriptorAllocator::allocateDescriptorTable(uint32_t descriptorNum) {
    NI_ASSERT(descriptorAllocated + descriptorNum <= NI_MAX_DESCRIPTORS, "Can't allocate %u descriptors", descriptorNum);
    DescriptorTable table = { { gpuBaseHandle.ptr + descriptorAllocated * descriptorHandleSize }, { cpuBaseHandle.ptr + descriptorAllocated * descriptorHandleSize }, descriptorHandleSize, 0, descriptorNum };
    descriptorAllocated += descriptorNum;
    return table;
}

static ID3D12RootSignature* buildRootSignature(ni::BindingLayout& layout, bool compute) {
    // Fix descriptor tables
    for (uint32_t index = 0; index < layout.rootParameterNum; ++index) {
        if (layout.rootParameters[index].ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) {
            uint64_t rangeIndex = (uint64_t)layout.rootParameters[index].DescriptorTable.pDescriptorRanges;
            layout.rootParameters[index].DescriptorTable.pDescriptorRanges = &layout.descriptorTableRanges[rangeIndex];
        }
    }

    ID3DBlob* rootSignatureBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;
    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    rootSignatureDesc.NumParameters = layout.rootParameterNum;
    rootSignatureDesc.pParameters = layout.rootParameters;
    rootSignatureDesc.NumStaticSamplers = layout.staticSamplerNum;
    rootSignatureDesc.pStaticSamplers = layout.staticSamplers;
    rootSignatureDesc.Flags = compute ? D3D12_ROOT_SIGNATURE_FLAG_NONE : D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    if (D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &rootSignatureBlob, &errorBlob) != S_OK) {
        NI_PANIC("Failed to serialize root signature.\n%s", errorBlob->GetBufferPointer());
    }
    ID3D12RootSignature* rootSignature = nullptr;
    NI_D3D_ASSERT(ni::getDevice()->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(), rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature)), "Failed to create root signature");
    return rootSignature;
}

ni::PipelineState ni::buildComputePipeline(ComputePipelineDesc& desc) {
    if (ID3D12RootSignature* rootSignature = buildRootSignature(desc.layout, true)) {
        D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature = rootSignature;
        psoDesc.CS = desc.shader;
        psoDesc.NodeMask = 0;
        psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
        if (ID3D12PipelineState* pso = ni::createComputePipelineState(psoDesc)) {
            return { rootSignature, pso };
        }
        NI_D3D_RELEASE(rootSignature);
    }
    return { nullptr, nullptr };
}

ni::PipelineState ni::buildGraphicsPipeline(GraphicsPipelineDesc& desc) {
    if (ID3D12RootSignature* rootSignature = buildRootSignature(desc.layout, false)) {
        ni::Array<D3D12_INPUT_ELEMENT_DESC, uint32_t> inputElements;
        for (uint32_t bufferIndex = 0; bufferIndex < desc.vertex.vertexBuffers.getNum(); ++bufferIndex) {
            for (uint32_t attribIndex = 0; attribIndex < desc.vertex.vertexBuffers[bufferIndex].vertexAttributes.getNum(); ++attribIndex) {
                const VertexAttribute& attrib = desc.vertex.vertexBuffers[bufferIndex].vertexAttributes[attribIndex];
                D3D12_INPUT_ELEMENT_DESC elementDesc = {};
                elementDesc.SemanticName = attrib.semanticName;
                elementDesc.SemanticIndex = attrib.semanticIndex;
                elementDesc.Format = attrib.format;
                elementDesc.InputSlot = bufferIndex;
                elementDesc.AlignedByteOffset = attrib.offset;
                elementDesc.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
                elementDesc.InstanceDataStepRate = 0;
                inputElements.add(elementDesc);
            }
        }

        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature = rootSignature;
        psoDesc.VS = desc.vertex.shader;
        psoDesc.PS = desc.pixel.shader;
        psoDesc.BlendState.AlphaToCoverageEnable = false;
        psoDesc.BlendState.IndependentBlendEnable = false;

        for (uint32_t index = 0; index < desc.pixel.renderTargets.getNum(); ++index) {
            psoDesc.BlendState.RenderTarget[index].BlendEnable = true;
            psoDesc.BlendState.RenderTarget[index].LogicOpEnable = false;
            psoDesc.BlendState.RenderTarget[index].SrcBlend = D3D12_BLEND_SRC_ALPHA;
            psoDesc.BlendState.RenderTarget[index].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
            psoDesc.BlendState.RenderTarget[index].BlendOp = D3D12_BLEND_OP_ADD;
            psoDesc.BlendState.RenderTarget[index].SrcBlendAlpha = D3D12_BLEND_ONE;
            psoDesc.BlendState.RenderTarget[index].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
            psoDesc.BlendState.RenderTarget[index].BlendOpAlpha = D3D12_BLEND_OP_ADD;
            psoDesc.BlendState.RenderTarget[index].LogicOp = D3D12_LOGIC_OP_NOOP;
            psoDesc.BlendState.RenderTarget[index].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        }

        psoDesc.SampleMask = UINT_MAX;
        psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        psoDesc.RasterizerState.CullMode = desc.rasterizer.cullMode;
        psoDesc.RasterizerState.FrontCounterClockwise = false;
        psoDesc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
        psoDesc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        psoDesc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        psoDesc.RasterizerState.DepthClipEnable = true;
        psoDesc.RasterizerState.MultisampleEnable = false;
        psoDesc.RasterizerState.AntialiasedLineEnable = false;
        psoDesc.RasterizerState.ForcedSampleCount = 0;
        psoDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
        psoDesc.DepthStencilState.DepthEnable = desc.pixel.depthStencilFormat != DXGI_FORMAT_UNKNOWN;
        psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
        psoDesc.DepthStencilState.StencilEnable = false;
        psoDesc.DepthStencilState.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
        psoDesc.DepthStencilState.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
        psoDesc.DepthStencilState.FrontFace = { D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS };
        psoDesc.DepthStencilState.BackFace = { D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS };
        psoDesc.InputLayout.pInputElementDescs = inputElements.getData();
        psoDesc.InputLayout.NumElements = inputElements.getNum();
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = desc.pixel.renderTargets.getNum();
        for (uint32_t index = 0; index < desc.pixel.renderTargets.getNum(); ++index) {
            psoDesc.RTVFormats[index] = desc.pixel.renderTargets[index];
        }
        psoDesc.DSVFormat = desc.pixel.depthStencilFormat;
        psoDesc.SampleDesc = { 1, 0 };
        psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
        if (ID3D12PipelineState* pso = ni::createGraphicsPipelineState(psoDesc)) {
            return { rootSignature, pso };
        }
        NI_D3D_RELEASE(rootSignature);
    }
    return { nullptr, nullptr };
}

void ni::destroyPipeline(PipelineState& pipelineState) {
    NI_D3D_RELEASE(pipelineState.pso);
    NI_D3D_RELEASE(pipelineState.rootSignature);
}

void ni::destroyBuffer(Resource& resource) {
    NI_D3D_RELEASE(resource.resource);
}

void ni::init(uint32_t width, uint32_t height, const char* title, bool enablePIX) {
    memset(&renderer, 0, sizeof(renderer));
	renderer.windowWidth = width;
	renderer.windowHeight = height;

    renderer.imagesToUpload = (Texture**)malloc(NI_MAX_DESCRIPTORS * sizeof(Texture*));
    renderer.imageToUploadNum = 0;
    renderer.frameCount = 0;

    memset(keysDown, 0, sizeof(keysDown));
    memset(mouseBtnsDown, 0, sizeof(mouseBtnsDown));
    memset(mouseBtnsClick, 0, sizeof(mouseBtnsClick));

#if NI_USE_FULLSCREEN
    renderer.windowWidth = GetSystemMetrics(SM_CXSCREEN);
    renderer.windowHeight = GetSystemMetrics(SM_CXSCREEN);
#endif

    /* Create Window */
    WNDCLASS windowClass = {
        0,
        [](HWND windowHandle, UINT message, WPARAM wParam,
                          LPARAM lParam) -> LRESULT {
            switch (message) {
            case WM_SIZE:
                //windowWidth = LOWORD(lParam);
                //windowHeight = HIWORD(lParam);
                break;
            case WM_CHAR:
                lastChar = (char)wParam;
                break;
            case WM_CONTEXTMENU:
                break;
            case WM_ENTERSIZEMOVE:
                break;
            case WM_EXITSIZEMOVE:
                break;
            case WM_CLOSE:
                DestroyWindow(windowHandle);
                break;
            case WM_DESTROY:
                PostQuitMessage(0);
                break;
            default:
                return DefWindowProc(windowHandle, message, wParam, lParam);
            }
            return S_OK;
        },
        0,
        0,
        GetModuleHandle(nullptr),
        LoadIcon(nullptr, IDI_APPLICATION),
        LoadCursor(nullptr, IDC_ARROW),
        (HBRUSH)(COLOR_WINDOW + 1),
        nullptr,
        L"WindowClass"
    };
    RegisterClass(&windowClass);

    DWORD windowStyle = WS_VISIBLE | WS_SYSMENU | WS_CAPTION | WS_BORDER;
#if NI_USE_FULLSCREEN    
    windowStyle = WS_POPUP | WS_VISIBLE;
#endif

    RECT windowRect = { 0, 0, (LONG)renderer.windowWidth, (LONG)renderer.windowHeight };
    AdjustWindowRect(&windowRect, windowStyle, false);
    int32_t adjustedWidth = windowRect.right - windowRect.left;
    int32_t adjustedHeight = windowRect.bottom - windowRect.top;

    renderer.windowHandle = CreateWindowExA(
        WS_EX_LEFT, "WindowClass", title, windowStyle, 100,
        100, adjustedWidth, adjustedHeight, nullptr, nullptr,
        GetModuleHandle(nullptr), nullptr);

#if _DEBUG
    if (enablePIX) {
        loadPIX();
    }
    NI_D3D_ASSERT(D3D12GetDebugInterface(IID_PPV_ARGS(&renderer.debugInterface)), "Failed to create debug interface");
    renderer.debugInterface->EnableDebugLayer();
    renderer.debugInterface->SetEnableGPUBasedValidation(true);
    UINT factoryFlag = DXGI_CREATE_FACTORY_DEBUG;
#else
    UINT factoryFlag = 0;
#endif
    NI_D3D_ASSERT(CreateDXGIFactory2(factoryFlag, IID_PPV_ARGS(&renderer.factory)), "Failed to create factory");
    NI_D3D_ASSERT(renderer.factory->EnumAdapters(0, (IDXGIAdapter**)(&renderer.adapter)), "Failed to aquire adapter");
    NI_D3D_ASSERT(D3D12CreateDevice((IUnknown*)renderer.adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&renderer.device)), "Failed to create device");

    D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
    commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    commandQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    commandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    commandQueueDesc.NodeMask = 0;
    NI_D3D_ASSERT(renderer.device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&renderer.commandQueue)), "Failed to create command queue");
    renderer.commandQueue->SetName(L"gfx::graphicsCommandQueue");

    for (uint32_t index = 0; index < NI_FRAME_COUNT; ++index) {
        FrameData& frame = renderer.frames[index];
        NI_D3D_ASSERT(renderer.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&frame.commandAllocator)), "Failed to create command allocator");
        NI_D3D_ASSERT(renderer.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, frame.commandAllocator, nullptr, IID_PPV_ARGS(&frame.commandList)), "Failed to create command list");
        NI_D3D_ASSERT(renderer.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&frame.fence)), "Failed to create fence");
        frame.fenceEvent = CreateEvent(nullptr, false, false, nullptr);
        NI_D3D_ASSERT(frame.commandList->Close(), "Failed to close command list");
        frame.commandAllocator->SetName(L"gfx::frame::commandAllocator");
        frame.commandList->SetName(L"gfx::frame::commandList");
        frame.fence->SetName(L"gfx::frame::fence");
        frame.frameIndex = index;
    }

    NI_D3D_ASSERT(renderer.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&renderer.presentFence)), "Failed to create fence");
    renderer.presentFenceEvent = CreateEvent(nullptr, false, false, nullptr);
    renderer.presentFenceValue = 0;
    renderer.presentFence->SetName(L"gfx::presentFence");

    DXGI_SWAP_CHAIN_DESC swapChainDesc = {
         { 
            renderer.windowWidth,
            renderer.windowHeight,
            { 0,  0},
            DXGI_FORMAT_R8G8B8A8_UNORM,
            DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED,
            DXGI_MODE_SCALING_UNSPECIFIED},
        { 1, 0 },
        DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT,
        NI_BACKBUFFER_COUNT,
        renderer.windowHandle,
        true,
        DXGI_SWAP_EFFECT_FLIP_DISCARD,
        DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH 
    };

    NI_D3D_ASSERT(renderer.factory->CreateSwapChain((IUnknown*)renderer.commandQueue, &swapChainDesc, (IDXGISwapChain**)&renderer.swapChain), "Failed to create swapchain");
    for (uint32_t index = 0; index < NI_BACKBUFFER_COUNT; ++index) {
        
        renderer.swapChain->GetBuffer(index, IID_PPV_ARGS(&renderer.backbuffers[index].texture.resource));
        renderer.backbuffers[index].texture.resource->SetName(L"gfx::backbuffer");
        renderer.backbuffers[index].texture.state = D3D12_RESOURCE_STATE_PRESENT;
    }

    D3D12_DESCRIPTOR_HEAP_DESC rtvDescriptorHeapDesc = {};
    rtvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvDescriptorHeapDesc.NumDescriptors = NI_BACKBUFFER_COUNT;
    rtvDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    rtvDescriptorHeapDesc.NodeMask = 0;
    NI_D3D_ASSERT(renderer.device->CreateDescriptorHeap(&rtvDescriptorHeapDesc, IID_PPV_ARGS(&renderer.rtvDescriptorHeap)), "Failed to create RTV descriptor heap");
    renderer.rtvDescriptorHeap->SetName(L"gfx::rtvDescriptorHeap");

    D3D12_DESCRIPTOR_HEAP_DESC dsvDescriptorHeapDesc = {};
    dsvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvDescriptorHeapDesc.NumDescriptors = NI_BACKBUFFER_COUNT;
    dsvDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    dsvDescriptorHeapDesc.NodeMask = 0;
    NI_D3D_ASSERT(renderer.device->CreateDescriptorHeap(&dsvDescriptorHeapDesc, IID_PPV_ARGS(&renderer.dsvDescriptorHeap)), "Failed to create DSV descriptor heap");
    renderer.dsvDescriptorHeap->SetName(L"gfx::dsvDescriptorHeap");
}
void ni::setFrameUserData(uint32_t frame, void* data) {
    NI_ASSERT(frame < NI_FRAME_COUNT, "Can't store user data on frame %u because it doesn't exist. The frame count is %u", frame, NI_FRAME_COUNT);
    renderer.frames[frame].userData = data;
}
void ni::waitForCurrentFrame() {
    FrameData& frame = renderer.frames[renderer.currentFrame];
    if (frame.fence->GetCompletedValue() != frame.frameWaitValue) {
        frame.fence->SetEventOnCompletion(frame.frameWaitValue, frame.fenceEvent);
        WaitForSingleObject(frame.fenceEvent, INFINITE);
    }
}
void ni::waitForAllFrames() {
    for (uint32_t index = 0; index < NI_FRAME_COUNT; ++index) {
        FrameData& frame = renderer.frames[index];
        if (frame.fence->GetCompletedValue() != frame.frameWaitValue) {
            frame.fence->SetEventOnCompletion(frame.frameWaitValue, frame.fenceEvent);
            WaitForSingleObject(frame.fenceEvent, INFINITE);
        }
    }
}
void ni::destroy() {
    waitForAllFrames();
    for (uint32_t index = 0; index < NI_FRAME_COUNT; ++index) {
        FrameData& frame = renderer.frames[index];
        NI_D3D_RELEASE(frame.commandList);
        NI_D3D_RELEASE(frame.commandAllocator);
        NI_D3D_RELEASE(frame.fence);
        CloseHandle(frame.fenceEvent);
    }
    for (uint32_t index = 0; index < NI_BACKBUFFER_COUNT; ++index) {
        NI_D3D_RELEASE(renderer.backbuffers[index].texture.resource);
    }
    if (renderer.presentFence->GetCompletedValue() != renderer.presentFenceValue) {
        renderer.presentFence->SetEventOnCompletion(renderer.presentFenceValue, renderer.presentFenceEvent);
        WaitForSingleObject(renderer.presentFenceEvent, INFINITE);
    }
    CloseHandle(renderer.presentFenceEvent);
    NI_D3D_RELEASE(renderer.dsvDescriptorHeap);
    NI_D3D_RELEASE(renderer.rtvDescriptorHeap);
    NI_D3D_RELEASE(renderer.presentFence);
    NI_D3D_RELEASE(renderer.swapChain);
    NI_D3D_RELEASE(renderer.commandQueue);
    NI_D3D_RELEASE(renderer.device);
    NI_D3D_RELEASE(renderer.adapter);
    NI_D3D_RELEASE(renderer.factory);
    free(renderer.imagesToUpload);

#if _DEBUG
    if (renderer.debugInterface) {
        IDXGIDebug1* dxgiDebug = nullptr;
        if (DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug)) == S_OK) {
            dxgiDebug->ReportLiveObjects(
                DXGI_DEBUG_ALL,
                DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_SUMMARY |
                    DXGI_DEBUG_RLO_IGNORE_INTERNAL));
            dxgiDebug->Release();
        }
        NI_D3D_RELEASE(renderer.debugInterface);
    }
#endif

}

void ni::pollEvents() {
    MSG message;
    memset(mouseBtnsClick, 0, sizeof(mouseBtnsClick));
    lastChar = 0;
    while (PeekMessageA(&message, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&message);
        switch (message.message) {
        case WM_MOUSEWHEEL:
            break;
        case WM_MOUSEHWHEEL:
            break;

        case WM_KEYDOWN:
            if (message.wParam == 27) {
                renderer.shouldQuit = true;
            }
            keysDown[(uint8_t)message.wParam] = true;
            break;
        case WM_KEYUP:
            keysDown[(uint8_t)message.wParam] = false;
            break;
        case WM_MOUSEMOVE:
            renderer.mouseX = (float)GET_X_LPARAM(message.lParam);
            renderer.mouseY = (float)GET_Y_LPARAM(message.lParam);
            break;
        case WM_LBUTTONDOWN:
            if (!mouseBtnsDown[0]) {
                mouseBtnsClick[0] = true;
            }
            mouseBtnsDown[0] = true;
            break;
        case WM_LBUTTONUP:
            mouseBtnsDown[0] = false;
            mouseBtnsClick[0] = false;
            break;
        case WM_RBUTTONDOWN:
            if (!mouseBtnsDown[2]) {
                mouseBtnsClick[2] = true;
            }
            mouseBtnsDown[2] = true;
            break;
        case WM_RBUTTONUP:
            mouseBtnsDown[2] = false;
            mouseBtnsClick[2] = false;
            break;
        case WM_MBUTTONDOWN:
            if (!mouseBtnsDown[1]) {
                mouseBtnsClick[1] = true;
            }
            mouseBtnsDown[1] = true;
            break;
        case WM_MBUTTONUP:
            mouseBtnsDown[1] = false;
            mouseBtnsClick[1] = false;
            break;
        case WM_QUIT:
            renderer.shouldQuit = true;
            return;
        default:
            DispatchMessageA(&message);
            break;
        }
    }
}

bool ni::shouldQuit() {
    return renderer.shouldQuit;
}

ni::FrameData& ni::getFrameData() {
    FrameData& frame = renderer.frames[renderer.currentFrame];
    return frame;
}

ID3D12Device* ni::getDevice() {
    return renderer.device;
}

ni::FrameData* ni::beginFrame() {
    FrameData& frame = renderer.frames[renderer.currentFrame];
    NI_D3D_ASSERT(frame.commandAllocator->Reset(), "Failed to reset command allocator");
    NI_D3D_ASSERT(frame.commandList->Reset(frame.commandAllocator, nullptr), "Failed to reset command list");

    // Upload texture data
    for (uint32_t index = 0; index < renderer.imageToUploadNum; ++index) {
        Texture* image = renderer.imagesToUpload[index];
        NI_ASSERT((image->state & NI_IMAGE_STATE_CREATED) > 0, "Invalid image");
        if ((image->state & NI_IMAGE_STATE_UPLOADED) > 0) {
            continue;
        }

        ID3D12Resource* texture = image->texture.resource;
        ID3D12Resource* uploadBuffer = image->upload.resource;
        D3D12_RESOURCE_DESC desc = image->texture.resource->GetDesc();

        D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
        uint32_t numRows = 0;
        uint64_t rowSizeInBytes = 0, totalBytes = 0;
        renderer.device->GetCopyableFootprints(&desc, 0, 1, 0, &layout, &numRows, &rowSizeInBytes, &totalBytes);
        void* mapped = nullptr;
        uploadBuffer->Map(0, nullptr, &mapped);
        size_t pixelSize = ni::getDXGIFormatBytes(desc.Format);

        for (uint32_t index = 0; index < numRows * layout.Footprint.Depth; ++index) {
            void* dstAddr = offsetPtr(mapped, (intptr_t)(index * layout.Footprint.RowPitch));
            const void* srcAddr = offsetPtr((void*)image->cpuData, (intptr_t)(index * (desc.Width * pixelSize)));
            memcpy(dstAddr, srcAddr, (size_t)rowSizeInBytes);
        }
        uploadBuffer->Unmap(0, nullptr);

        D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
        srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        srcLocation.pResource = uploadBuffer;
        srcLocation.PlacedFootprint = layout;
        D3D12_TEXTURE_COPY_LOCATION dstLocation = {};
        dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dstLocation.pResource = texture;
        dstLocation.SubresourceIndex = 0;

        D3D12_RESOURCE_BARRIER textureBufferBarrier[1] = {};
        textureBufferBarrier[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        textureBufferBarrier[0].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        textureBufferBarrier[0].Transition.Subresource = 0;
        textureBufferBarrier[0].Transition.pResource = texture;
        textureBufferBarrier[0].Transition.StateBefore = image->texture.state;
        textureBufferBarrier[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;

        frame.commandList->ResourceBarrier(1, textureBufferBarrier);
        frame.commandList->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, nullptr);

        textureBufferBarrier[0].Transition.StateAfter = image->texture.state;
        textureBufferBarrier[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        frame.commandList->ResourceBarrier(1, textureBufferBarrier);

        free((void*)image->cpuData);
        image->cpuData = nullptr;
        image->state |= NI_IMAGE_STATE_UPLOADED;
    }
    renderer.imageToUploadNum = 0;

    return &frame;
}

void ni::endFrame() {
    FrameData& frame = renderer.frames[renderer.currentFrame];
    NI_D3D_ASSERT(frame.commandList->Close(), "Failed to close command list");
    ID3D12CommandList* commandLists[] = { frame.commandList };
    renderer.commandQueue->ExecuteCommandLists(1, commandLists);
    NI_D3D_ASSERT(renderer.commandQueue->Signal(frame.fence, ++frame.frameWaitValue), "Failed to signal frame fence");
    renderer.currentFrame = (renderer.currentFrame + 1) % NI_FRAME_COUNT;
}

ni::Texture* ni::getCurrentBackbuffer() {
    return &renderer.backbuffers[renderer.presentFrame];
}

void ni::present(bool vsync) {
    if (renderer.presentFence->GetCompletedValue() != renderer.presentFenceValue) {
        renderer.presentFence->SetEventOnCompletion(renderer.presentFenceValue, renderer.presentFenceEvent);
        WaitForSingleObject(renderer.presentFenceEvent, INFINITE);
    }

    NI_D3D_ASSERT(renderer.swapChain->Present(vsync ? 1 : 0, 0), "Failed to present");
    NI_D3D_ASSERT(renderer.commandQueue->Signal(renderer.presentFence, ++renderer.presentFenceValue), "Failed to signal present fence");
    renderer.presentFrame = renderer.presentFenceValue % NI_BACKBUFFER_COUNT;
    renderer.frameCount++;
}

ni::DescriptorAllocator* ni::createDescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t descriptorNum, D3D12_DESCRIPTOR_HEAP_FLAGS flags) {
    D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
    descriptorHeapDesc.Type = type;
    descriptorHeapDesc.NumDescriptors = descriptorNum;
    descriptorHeapDesc.Flags = flags;
    descriptorHeapDesc.NodeMask = 0;
    ID3D12DescriptorHeap* descriptorHeap = nullptr;
    ni::DescriptorAllocator* descriptorAllocator = new ni::DescriptorAllocator();
    NI_D3D_ASSERT(renderer.device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&descriptorAllocator->descriptorHeap)), "Failed to create descriptor heap");
    descriptorAllocator->descriptorAllocated = 0;
    descriptorAllocator->gpuBaseHandle = descriptorAllocator->descriptorHeap->GetGPUDescriptorHandleForHeapStart();
    descriptorAllocator->cpuBaseHandle = descriptorAllocator->descriptorHeap->GetCPUDescriptorHandleForHeapStart();
    descriptorAllocator->descriptorHandleSize = renderer.device->GetDescriptorHandleIncrementSize(type);
    return descriptorAllocator;
}

void ni::destroyDescriptorAllocator(DescriptorAllocator* descriptorAllocator) {
    NI_D3D_RELEASE(descriptorAllocator->descriptorHeap);
    delete descriptorAllocator;
}

ID3D12PipelineState* ni::createGraphicsPipelineState(const wchar_t* name, const D3D12_GRAPHICS_PIPELINE_STATE_DESC& psoDesc) {
    ID3D12PipelineState* pso = nullptr;
    NI_D3D_ASSERT(renderer.device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso)), "Failed to create graphics pipeline state");
    pso->SetName(name);
    return pso;
}

ID3D12PipelineState* ni::createComputePipelineState(const wchar_t* name, const D3D12_COMPUTE_PIPELINE_STATE_DESC& psoDesc) {
    ID3D12PipelineState* pso = nullptr;
    NI_D3D_ASSERT(renderer.device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&pso)), "Failed to create compute pipeline state");
    pso->SetName(name);
    return pso;
}

ID3D12PipelineState* ni::createGraphicsPipelineState(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& psoDesc) {
    ID3D12PipelineState* pso = nullptr;
    NI_D3D_ASSERT(renderer.device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso)), "Failed to create graphics pipeline state");
    return pso;
}

ID3D12PipelineState* ni::createComputePipelineState(const D3D12_COMPUTE_PIPELINE_STATE_DESC& psoDesc) {
    ID3D12PipelineState* pso = nullptr;
    NI_D3D_ASSERT(renderer.device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&pso)), "Failed to create compute pipeline state");
    return pso;
}

float ni::getViewWidth() { return (float)renderer.windowWidth; }
float ni::getViewHeight() { return (float)renderer.windowHeight; }

float ni::getViewAspectRatio() {
    return ni::getViewWidth() / ni::getViewHeight();
}

uint32_t ni::getViewWidthUint() {
    return renderer.windowWidth;
}

uint32_t ni::getViewHeightUint() {
    return renderer.windowHeight;
}

ni::Resource ni::createBuffer(const wchar_t* name, size_t bufferSize, BufferType type, bool initToZero, D3D12_RESOURCE_FLAGS flags) {
    ni::Resource resource = createBuffer(bufferSize, type, initToZero, flags);
    resource.resource->SetName(name);
    return resource;
}

ni::Resource ni::createBuffer(size_t bufferSize, BufferType type, bool initToZero, D3D12_RESOURCE_FLAGS flags) {
    D3D12_HEAP_TYPE heapType;
    D3D12_RESOURCE_STATES initialState;
    switch (type) {
    case INDEX_BUFFER:
        initialState = D3D12_RESOURCE_STATE_INDEX_BUFFER;
        flags |= D3D12_RESOURCE_FLAG_NONE;
        heapType = D3D12_HEAP_TYPE_DEFAULT;
        break;
    case VERTEX_BUFFER:
        initialState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        flags |= D3D12_RESOURCE_FLAG_NONE;
        heapType = D3D12_HEAP_TYPE_DEFAULT;
        break;
    case CONSTANT_BUFFER:
        initialState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        flags |= D3D12_RESOURCE_FLAG_NONE;
        heapType = D3D12_HEAP_TYPE_DEFAULT;
        break;
    case UNORDERED_BUFFER:
        initialState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        heapType = D3D12_HEAP_TYPE_DEFAULT;
        break;
    case UPLOAD_BUFFER:
        initialState = D3D12_RESOURCE_STATE_GENERIC_READ;
        flags |= D3D12_RESOURCE_FLAG_NONE;
        heapType = D3D12_HEAP_TYPE_UPLOAD;
        break;
    case SHADER_RESOURCE_BUFFER:
        initialState = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
        flags |= D3D12_RESOURCE_FLAG_NONE;
        heapType = D3D12_HEAP_TYPE_DEFAULT;
        break;
    default:
        NI_PANIC("Error: Invalid buffer type"); // Invalid Buffer Type
        break;
    }

    D3D12_RESOURCE_DESC resourceDesc = {
        D3D12_RESOURCE_DIMENSION_BUFFER,
        D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
        alignSize(bufferSize, 256),
        1,
        1,
        1,
        DXGI_FORMAT_UNKNOWN,
        { 1, 0},
        D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
        flags
    };
    D3D12_HEAP_PROPERTIES heapProps = {
        heapType,
        D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
        D3D12_MEMORY_POOL_UNKNOWN,
        0,
        0
    };

    D3D12_HEAP_FLAGS heapFlags = D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES;
    if (!initToZero) {
        heapFlags |= D3D12_HEAP_FLAG_CREATE_NOT_ZEROED;
    }
    ID3D12Resource* resource = nullptr;
    NI_D3D_ASSERT(renderer.device->CreateCommittedResource(
        &heapProps,
        heapFlags,
        &resourceDesc, initialState, nullptr,
        IID_PPV_ARGS(&resource)),
        "Failed to create buffer resource");
    return { resource, initialState };
}

D3D12_CPU_DESCRIPTOR_HANDLE ni::getRenderTargetViewCPUHandle() {
    return renderer.rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
}

D3D12_GPU_DESCRIPTOR_HANDLE ni::getRenderTargetViewGPUHandle() {
    return renderer.rtvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
}

D3D12_CPU_DESCRIPTOR_HANDLE ni::getDepthStencilViewCPUHandle() {
    return renderer.dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
}
D3D12_GPU_DESCRIPTOR_HANDLE ni::getDepthStencilViewGPUHandle() {
    return renderer.dsvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
}

float ni::mouseX() {
    return renderer.mouseX;
}
float ni::mouseY() {
    return renderer.mouseY;
}

float ni::mouseWheelX() {
    return 0.0f;
}

float ni::mouseWheelY() {
    return 0.0f;
}

bool ni::mouseDown(MouseButton button) {
    return mouseBtnsDown[(uint32_t)button];
}

bool ni::mouseClick(MouseButton button) {
    return mouseBtnsClick[(uint32_t)button];
}

bool ni::keyDown(KeyCode keyCode) {
    return keysDown[(uint32_t)keyCode];
}

char ni::getLastChar() {
    return lastChar;
}

float ni::randomFloat() {
    static std::random_device randDevice;
    static std::mt19937 mtGen(randDevice());
    static std::uniform_real_distribution<float> udt;
    return udt(mtGen);
}

uint32_t ni::randomUint() {
    static std::random_device randDevice;
    static std::mt19937 mtGen(randDevice());
    static std::uniform_int_distribution<unsigned int> udt;
    return udt(mtGen);
}

double ni::getSeconds() {
    double time = std::chrono::time_point_cast<std::chrono::duration<double>>(
        std::chrono::high_resolution_clock::now())
        .time_since_epoch()
        .count();
    return time;
}

size_t ni::getDXGIFormatBits(DXGI_FORMAT format) {
    switch (format) {
    case DXGI_FORMAT_R32G32B32A32_TYPELESS:
    case DXGI_FORMAT_R32G32B32A32_FLOAT:
    case DXGI_FORMAT_R32G32B32A32_UINT:
    case DXGI_FORMAT_R32G32B32A32_SINT:
        return 128;

    case DXGI_FORMAT_R32G32B32_TYPELESS:
    case DXGI_FORMAT_R32G32B32_FLOAT:
    case DXGI_FORMAT_R32G32B32_UINT:
    case DXGI_FORMAT_R32G32B32_SINT:
        return 96;

    case DXGI_FORMAT_R16G16B16A16_TYPELESS:
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
    case DXGI_FORMAT_R16G16B16A16_UNORM:
    case DXGI_FORMAT_R16G16B16A16_UINT:
    case DXGI_FORMAT_R16G16B16A16_SNORM:
    case DXGI_FORMAT_R16G16B16A16_SINT:
    case DXGI_FORMAT_R32G32_TYPELESS:
    case DXGI_FORMAT_R32G32_FLOAT:
    case DXGI_FORMAT_R32G32_UINT:
    case DXGI_FORMAT_R32G32_SINT:
    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
    case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
    case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
    case DXGI_FORMAT_Y416:
    case DXGI_FORMAT_Y210:
    case DXGI_FORMAT_Y216:
        return 64;

    case DXGI_FORMAT_R10G10B10A2_TYPELESS:
    case DXGI_FORMAT_R10G10B10A2_UNORM:
    case DXGI_FORMAT_R10G10B10A2_UINT:
    case DXGI_FORMAT_R11G11B10_FLOAT:
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_R8G8B8A8_UINT:
    case DXGI_FORMAT_R8G8B8A8_SNORM:
    case DXGI_FORMAT_R8G8B8A8_SINT:
    case DXGI_FORMAT_R16G16_TYPELESS:
    case DXGI_FORMAT_R16G16_FLOAT:
    case DXGI_FORMAT_R16G16_UNORM:
    case DXGI_FORMAT_R16G16_UINT:
    case DXGI_FORMAT_R16G16_SNORM:
    case DXGI_FORMAT_R16G16_SINT:
    case DXGI_FORMAT_R32_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT:
    case DXGI_FORMAT_R32_FLOAT:
    case DXGI_FORMAT_R32_UINT:
    case DXGI_FORMAT_R32_SINT:
    case DXGI_FORMAT_R24G8_TYPELESS:
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
    case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
    case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
    case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
    case DXGI_FORMAT_R8G8_B8G8_UNORM:
    case DXGI_FORMAT_G8R8_G8B8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8X8_UNORM:
    case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
    case DXGI_FORMAT_B8G8R8A8_TYPELESS:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8X8_TYPELESS:
    case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
    case DXGI_FORMAT_AYUV:
    case DXGI_FORMAT_Y410:
    case DXGI_FORMAT_YUY2:
        return 32;

    case DXGI_FORMAT_P010:
    case DXGI_FORMAT_P016:
        return 24;

    case DXGI_FORMAT_R8G8_TYPELESS:
    case DXGI_FORMAT_R8G8_UNORM:
    case DXGI_FORMAT_R8G8_UINT:
    case DXGI_FORMAT_R8G8_SNORM:
    case DXGI_FORMAT_R8G8_SINT:
    case DXGI_FORMAT_R16_TYPELESS:
    case DXGI_FORMAT_R16_FLOAT:
    case DXGI_FORMAT_D16_UNORM:
    case DXGI_FORMAT_R16_UNORM:
    case DXGI_FORMAT_R16_UINT:
    case DXGI_FORMAT_R16_SNORM:
    case DXGI_FORMAT_R16_SINT:
    case DXGI_FORMAT_B5G6R5_UNORM:
    case DXGI_FORMAT_B5G5R5A1_UNORM:
    case DXGI_FORMAT_A8P8:
    case DXGI_FORMAT_B4G4R4A4_UNORM:
        return 16;

    case DXGI_FORMAT_NV12:
    case DXGI_FORMAT_420_OPAQUE:
    case DXGI_FORMAT_NV11:
        return 12;

    case DXGI_FORMAT_R8_TYPELESS:
    case DXGI_FORMAT_R8_UNORM:
    case DXGI_FORMAT_R8_UINT:
    case DXGI_FORMAT_R8_SNORM:
    case DXGI_FORMAT_R8_SINT:
    case DXGI_FORMAT_A8_UNORM:
    case DXGI_FORMAT_BC2_TYPELESS:
    case DXGI_FORMAT_BC2_UNORM:
    case DXGI_FORMAT_BC2_UNORM_SRGB:
    case DXGI_FORMAT_BC3_TYPELESS:
    case DXGI_FORMAT_BC3_UNORM:
    case DXGI_FORMAT_BC3_UNORM_SRGB:
    case DXGI_FORMAT_BC5_TYPELESS:
    case DXGI_FORMAT_BC5_UNORM:
    case DXGI_FORMAT_BC5_SNORM:
    case DXGI_FORMAT_BC6H_TYPELESS:
    case DXGI_FORMAT_BC6H_UF16:
    case DXGI_FORMAT_BC6H_SF16:
    case DXGI_FORMAT_BC7_TYPELESS:
    case DXGI_FORMAT_BC7_UNORM:
    case DXGI_FORMAT_BC7_UNORM_SRGB:
    case DXGI_FORMAT_AI44:
    case DXGI_FORMAT_IA44:
    case DXGI_FORMAT_P8:
        return 8;

    case DXGI_FORMAT_R1_UNORM:
        return 1;

    case DXGI_FORMAT_BC1_TYPELESS:
    case DXGI_FORMAT_BC1_UNORM:
    case DXGI_FORMAT_BC1_UNORM_SRGB:
    case DXGI_FORMAT_BC4_TYPELESS:
    case DXGI_FORMAT_BC4_UNORM:
    case DXGI_FORMAT_BC4_SNORM:
        return 4;

    default:
        return 0;
    }
}

size_t ni::getDXGIFormatBytes(DXGI_FORMAT format) {
    return getDXGIFormatBits(format) / 8;
}

ni::Texture* ni::createTexture(const wchar_t* name, uint32_t width, uint32_t height, uint32_t depth, const void* pixels, DXGI_FORMAT dxgiFormat, D3D12_RESOURCE_FLAGS flags) {
    ni::Texture* texture = createTexture(width, height, depth, pixels, dxgiFormat, flags);
    texture->texture.resource->SetName(name);
    return texture;
}

ni::Texture* ni::createTexture(uint32_t width, uint32_t height, uint32_t depth, const void* pixels, DXGI_FORMAT dxgiFormat, D3D12_RESOURCE_FLAGS flags) {
    Texture* texture = new Texture();
    D3D12_RESOURCE_DIMENSION dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

    if (width > 1 && height > 1 && depth > 1) {
        dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
    }
    else if (width >= 1 && height >= 1 && depth == 1) {
        dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    }
    else if (width >= 1 && height == 1 && depth == 1) {
        dimension = D3D12_RESOURCE_DIMENSION_TEXTURE1D;
    }

    D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    D3D12_RESOURCE_DESC resourceDesc = {
        dimension,
        D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
        (uint64_t)width,
        height,
        (uint16_t)depth,
        1,
        dxgiFormat,
        { 1,  0},
        D3D12_TEXTURE_LAYOUT_UNKNOWN,
        flags
    };
    D3D12_HEAP_PROPERTIES heapProps = {
        D3D12_HEAP_TYPE_DEFAULT,
        D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
        D3D12_MEMORY_POOL_UNKNOWN,
        0,
        0
    };

    D3D12_CLEAR_VALUE* clearValuePtr = nullptr;
    D3D12_CLEAR_VALUE clearValue = {};

    if ((flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) > 0) {
        clearValue.Format = dxgiFormat;
        clearValue.DepthStencil.Depth = 1;
        clearValue.DepthStencil.Stencil = 0;
        clearValuePtr = &clearValue;
    }

    if ((flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) > 0) {
        clearValue.Color[0] = 0.0f;
        clearValue.Color[1] = 0.0f;
        clearValue.Color[2] = 0.0f;
        clearValue.Color[3] = 1.0f;
        clearValuePtr = &clearValue;
    }

    NI_D3D_ASSERT(ni::getDevice()->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES, &resourceDesc, initialState, clearValuePtr, IID_PPV_ARGS(&texture->texture.resource)), "Failed to create image resource");
    texture->texture.state = initialState;
    texture->width = width;
    texture->height = height;
    texture->depth = depth;
    texture->state |= NI_IMAGE_STATE_CREATED;

    if (pixels != nullptr) {
        size_t pixelSize = ni::getDXGIFormatBytes(dxgiFormat);
        uint64_t alignedWidth = alignSize(width, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
        uint64_t dataSize = alignedWidth * height * depth * pixelSize;
        uint64_t bufferSize = dataSize < 256 ? 256 : dataSize;
        texture->upload = ni::createBuffer((size_t)bufferSize, ni::UPLOAD_BUFFER, false);
        texture->cpuData = malloc(pixelSize * width * height * depth);
        NI_ASSERT(texture->cpuData != nullptr, "Failed to allocate cpu data for uploading to texture memory");
        if (texture->cpuData != nullptr) {
            memcpy((void*)texture->cpuData, pixels, pixelSize * width * height * depth);
        }
        renderer.imagesToUpload[renderer.imageToUploadNum++] = texture;
    }
    return texture;
}

void ni::destroyTexture(Texture*& texture) {
    NI_D3D_RELEASE(texture->texture.resource);
    NI_D3D_RELEASE(texture->upload.resource);
    delete texture;
    texture = nullptr;
}

void ni::logFmt(const char* fmt, ...) {
    static char bufferLarge[NI_UTILS_WINDOWS_LOG_MAX_BUFFER_SIZE * NI_UTILS_WINDOWS_LOG_MAX_BUFFER_COUNT] = {};
    static uint32_t bufferIndex = 0;
    va_list args;
    va_start(args, fmt);
    char* buffer = &bufferLarge[bufferIndex * NI_UTILS_WINDOWS_LOG_MAX_BUFFER_SIZE];
    vsprintf_s(buffer, NI_UTILS_WINDOWS_LOG_MAX_BUFFER_SIZE, fmt, args);
    bufferIndex = (bufferIndex + 1) % NI_UTILS_WINDOWS_LOG_MAX_BUFFER_COUNT;
    va_end(args);
    OutputDebugStringA(buffer);
    printf("%s", buffer);
}

// Origin: https://github.com/niklas-ourmachinery/bitsquid-foundation/blob/master/murmur_hash.cpp
uint64_t ni::murmurHash(const void* key, uint64_t keyLength, uint64_t seed) {
    const uint64_t m = 0xc6a4a7935bd1e995ULL;
    const uint32_t r = 47;
    uint64_t h = seed ^ (keyLength * m);
    const uint64_t* data = (const uint64_t*)key;
    const uint64_t* end = data + (keyLength / 8);
    while (data != end) {
        uint64_t k = *data++;
        k *= m;
        k ^= k >> r;
        k *= m;
        h ^= k;
        h *= m;
    }
    const unsigned char* data2 = (const unsigned char*)data;
    switch (keyLength & 7) {
    case 7: h ^= uint64_t(data2[6]) << 48;
    case 6: h ^= uint64_t(data2[5]) << 40;
    case 5: h ^= uint64_t(data2[4]) << 32;
    case 4: h ^= uint64_t(data2[3]) << 24;
    case 3: h ^= uint64_t(data2[2]) << 16;
    case 2: h ^= uint64_t(data2[1]) << 8;
    case 1: h ^= uint64_t(data2[0]);
        h *= m;
    };
    h ^= h >> r;
    h *= m;
    h ^= h >> r;
    return h;
}

size_t ni::getFileSize(const char* path) {
    HANDLE fileHandle = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (fileHandle != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER fileSize = {};
        if (GetFileSizeEx(fileHandle, &fileSize)) {
            return (size_t)fileSize.QuadPart;
        }
        CloseHandle(fileHandle);
    }
    return 0;
}

bool ni::readFile(const char* path, void* outBuffer) {
    HANDLE fileHandle = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (fileHandle != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER fileSize = {};
        GetFileSizeEx(fileHandle, &fileSize);
        DWORD readBytes = 0;
        if (!ReadFile(fileHandle, outBuffer, (DWORD)fileSize.QuadPart, &readBytes, nullptr)) {
            CloseHandle(fileHandle);
            return false;
        }
        CloseHandle(fileHandle);
        return true;
    }
    return false;
}

void* ni::allocReadFile(const char* path) {
    HANDLE fileHandle = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (fileHandle != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER fileSize = {};
        GetFileSizeEx(fileHandle, &fileSize);
        void* buffer = malloc((size_t)fileSize.QuadPart);
        DWORD readBytes = 0;
        if (!ReadFile(fileHandle, buffer, (DWORD)fileSize.QuadPart, &readBytes, nullptr)) {
            CloseHandle(fileHandle);
            free(buffer);
            return nullptr;
        }
        CloseHandle(fileHandle);
        return buffer;
    }
    return nullptr;
}

uint64_t ni::getFrameCount() {
    return renderer.frameCount;
}

ni::FileReader::FileReader(const char* path) {
    size = getFileSize(path);
    buffer = allocReadFile(path);
}

ni::FileReader::~FileReader() {
    free(buffer);
}

void* ni::FileReader::operator*() const {
    return buffer;
}

size_t ni::FileReader::getSize() const {
    return size;
}

void ni::DescriptorTable::reset() {
    allocated = 0;
}

ni::DescriptorHandle ni::DescriptorTable::allocate() {
    NI_ASSERT(allocated + 1 <= capacity, "Exceeded capacity of descriptors allocated");
    DescriptorHandle handle = { gpuHandle(allocated), cpuHandle(allocated) };
    allocated += 1;
    return handle;
}

void ni::DescriptorTable::allocUAVBuffer(ni::Resource& resource, ni::Resource* counter, DXGI_FORMAT format, uint64_t firstElement, uint32_t numElements, uint32_t structureByteStride, uint64_t counterOffsetInBytes, bool rawBuffer) {
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = format;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.FirstElement = firstElement;
    uavDesc.Buffer.NumElements = numElements;
    uavDesc.Buffer.StructureByteStride = structureByteStride;
    uavDesc.Buffer.CounterOffsetInBytes = counterOffsetInBytes;
    uavDesc.Buffer.Flags = rawBuffer ? D3D12_BUFFER_UAV_FLAG_RAW : D3D12_BUFFER_UAV_FLAG_NONE;
    ni::getDevice()->CreateUnorderedAccessView(resource.resource, counter ? counter->resource : nullptr, &uavDesc, allocate().cpuHandle);
}

void ni::DescriptorTable::allocUAVTex1D(ni::Resource& resource, ni::Resource* counter, DXGI_FORMAT format, uint32_t mipSlice) {
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = format;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
    uavDesc.Texture1D.MipSlice = mipSlice;
    ni::getDevice()->CreateUnorderedAccessView(resource.resource, counter ? counter->resource : nullptr, &uavDesc, allocate().cpuHandle);
}

void ni::DescriptorTable::allocUAVTex1DArray(ni::Resource& resource, ni::Resource* counter, DXGI_FORMAT format, uint32_t mipSlice, uint32_t firstArraySlice, uint32_t arraySize) {
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = format;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
    uavDesc.Texture1DArray.MipSlice = mipSlice;
    uavDesc.Texture1DArray.FirstArraySlice = firstArraySlice;
    uavDesc.Texture1DArray.ArraySize = arraySize;
    ni::getDevice()->CreateUnorderedAccessView(resource.resource, counter ? counter->resource : nullptr, &uavDesc, allocate().cpuHandle);
}

void ni::DescriptorTable::allocUAVTex2D(ni::Resource& resource, ni::Resource* counter, DXGI_FORMAT format, uint32_t mipSlice, uint32_t planeSlice) {
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = format;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    uavDesc.Texture2D.MipSlice = mipSlice;
    uavDesc.Texture2D.PlaneSlice = planeSlice;
    ni::getDevice()->CreateUnorderedAccessView(resource.resource, counter ? counter->resource : nullptr, &uavDesc, allocate().cpuHandle);
}

void ni::DescriptorTable::allocUAVTex2DArray(ni::Resource& resource, ni::Resource* counter, DXGI_FORMAT format, uint32_t mipSlice, uint32_t firstArraySlice, uint32_t arraySize, uint32_t planeSlice) {
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = format;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
    uavDesc.Texture2DArray.MipSlice = mipSlice;
    uavDesc.Texture2DArray.PlaneSlice = planeSlice;
    uavDesc.Texture2DArray.FirstArraySlice = firstArraySlice;
    uavDesc.Texture2DArray.ArraySize = arraySize;
    ni::getDevice()->CreateUnorderedAccessView(resource.resource, counter ? counter->resource : nullptr, &uavDesc, allocate().cpuHandle);
}

void ni::DescriptorTable::allocUAVTex3D(ni::Resource& resource, ni::Resource* counter, DXGI_FORMAT format, uint32_t mipSlice, uint32_t firstWSlice, uint32_t wSize) {
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = format;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
    uavDesc.Texture3D.MipSlice = mipSlice;
    uavDesc.Texture3D.FirstWSlice = firstWSlice;
    uavDesc.Texture3D.WSize = wSize;
    ni::getDevice()->CreateUnorderedAccessView(resource.resource, counter ? counter->resource : nullptr, &uavDesc, allocate().cpuHandle);
}

void ni::DescriptorTable::allocSRVBuffer(ni::Resource& resource, DXGI_FORMAT format, uint64_t firstElement, uint32_t numElements, uint32_t structureByteStride, bool rawBuffer) {
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Buffer.FirstElement = firstElement;
    srvDesc.Buffer.NumElements = numElements;
    srvDesc.Buffer.StructureByteStride = structureByteStride;
    srvDesc.Buffer.Flags = rawBuffer ? D3D12_BUFFER_SRV_FLAG_RAW : D3D12_BUFFER_SRV_FLAG_NONE;
    ni::getDevice()->CreateShaderResourceView(resource.resource, &srvDesc, allocate().cpuHandle);
}

void ni::DescriptorTable::allocSRVTex1D(ni::Resource& resource, DXGI_FORMAT format, uint32_t mostDetailedMip, uint32_t mipLevels, float resourceMinLODClamp) {
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture1D.MostDetailedMip = mostDetailedMip;
    srvDesc.Texture1D.MipLevels = mipLevels;
    srvDesc.Texture1D.ResourceMinLODClamp = resourceMinLODClamp;
    ni::getDevice()->CreateShaderResourceView(resource.resource, &srvDesc, allocate().cpuHandle);
}

void ni::DescriptorTable::allocSRVTex1DArray(ni::Resource& resource, DXGI_FORMAT format, uint32_t mostDetailedMip, uint32_t mipLevels, uint32_t firstArraySlice, uint32_t arraySize, float resourceMinLODClamp) {
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture1DArray.MostDetailedMip = mostDetailedMip;
    srvDesc.Texture1DArray.MipLevels = mipLevels;
    srvDesc.Texture1DArray.FirstArraySlice = firstArraySlice;
    srvDesc.Texture1DArray.ArraySize = arraySize;
    srvDesc.Texture1DArray.ResourceMinLODClamp = resourceMinLODClamp;
    ni::getDevice()->CreateShaderResourceView(resource.resource, &srvDesc, allocate().cpuHandle);
}

void ni::DescriptorTable::allocSRVTex2D(ni::Resource& resource, DXGI_FORMAT format, uint32_t mostDetailedMip, uint32_t mipLevels, uint32_t planeSlice, float resourceMinLODClamp) {
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MostDetailedMip = mostDetailedMip;
    srvDesc.Texture2D.MipLevels = mipLevels;
    srvDesc.Texture2D.PlaneSlice = planeSlice;
    srvDesc.Texture2D.ResourceMinLODClamp = resourceMinLODClamp;
    ni::getDevice()->CreateShaderResourceView(resource.resource, &srvDesc, allocate().cpuHandle);
}

void ni::DescriptorTable::allocSRVTex2DArray(ni::Resource& resource, DXGI_FORMAT format, uint32_t mostDetailedMip, uint32_t mipLevels, uint32_t firstArraySlice, uint32_t arraySize, uint32_t planeSlice, float resourceMinLODClamp) {
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2DArray.MostDetailedMip = mostDetailedMip;
    srvDesc.Texture2DArray.MipLevels = mipLevels;
    srvDesc.Texture2DArray.FirstArraySlice = firstArraySlice;
    srvDesc.Texture2DArray.ArraySize = arraySize;
    srvDesc.Texture2DArray.PlaneSlice = planeSlice;
    srvDesc.Texture2DArray.ResourceMinLODClamp = resourceMinLODClamp;
    ni::getDevice()->CreateShaderResourceView(resource.resource, &srvDesc, allocate().cpuHandle);
}

void ni::DescriptorTable::allocSRVTex3D(ni::Resource& resource, DXGI_FORMAT format, uint32_t mostDetailedMip, uint32_t mipLevels, float resourceMinLODClamp) {
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture3D.MostDetailedMip = mostDetailedMip;
    srvDesc.Texture3D.MipLevels = mipLevels;
    srvDesc.Texture3D.ResourceMinLODClamp = resourceMinLODClamp;
    ni::getDevice()->CreateShaderResourceView(resource.resource, &srvDesc, allocate().cpuHandle);
}

void ni::DescriptorTable::allocSRVTexCube(ni::Resource& resource, DXGI_FORMAT format, uint32_t mostDetailedMip, uint32_t mipLevels, float resourceMinLODClamp) {
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.TextureCube.MostDetailedMip = mostDetailedMip;
    srvDesc.TextureCube.MipLevels = mipLevels;
    srvDesc.TextureCube.ResourceMinLODClamp = resourceMinLODClamp;
    ni::getDevice()->CreateShaderResourceView(resource.resource, &srvDesc, allocate().cpuHandle);
}

void ni::DescriptorTable::allocRTVTex2D(ni::Resource& resource, DXGI_FORMAT format, uint32_t mipSlice, uint32_t planeSlice) {
    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format = format;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Texture2D.MipSlice = mipSlice;
    rtvDesc.Texture2D.PlaneSlice = planeSlice;
    ni::getDevice()->CreateRenderTargetView(resource.resource, &rtvDesc, allocate().cpuHandle);
}

void ni::DescriptorTable::allocDSVTex2D(ni::Resource& resource, DXGI_FORMAT format, uint32_t mipSlice, D3D12_DSV_FLAGS flags) {
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = format;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Flags = flags;
    dsvDesc.Texture2D.MipSlice = mipSlice;
    ni::getDevice()->CreateDepthStencilView(resource.resource, &dsvDesc, allocate().cpuHandle);
}

D3D12_GPU_DESCRIPTOR_HANDLE ni::DescriptorTable::gpuHandle(uint64_t index) {
    D3D12_GPU_DESCRIPTOR_HANDLE handle = gpuBaseHandle;
    handle.ptr += (index * handleSize);
    return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE ni::DescriptorTable::cpuHandle(uint64_t index) {
    D3D12_CPU_DESCRIPTOR_HANDLE handle = cpuBaseHandle;
    handle.ptr += ((size_t)index * handleSize);
    return handle;
}

float ni::toRad(float d) { return ni::PI * d / 180.0f; }

float ni::toDeg(float r) { return 180.0f * r / ni::PI; }

float ni::cos(float t) { return cosf(t); }

float ni::sin(float t) { return sinf(t); }

float ni::tan(float t) { return tanf(t); }

float ni::asin(float t) { return asinf(t); }

float ni::acos(float t) { return acosf(t); }

float ni::atan(float t) { return atanf(t); }

float ni::atan2(float y, float x) { return atan2f(y, x); }

float ni::pow(float x, float y) { return powf(x, y); }

float ni::exp(float x) { return expf(x); }

float ni::exp2(float x) { return exp2f(x); }

float ni::log(float x) { return logf(x); }

float ni::log2(float x) { return log2f(x); }

float ni::sqrt(float x) { return sqrtf(x); }

float ni::invSqrt(float x) { return 1.0f / sqrtf(x); }

float ni::abs(float x) { return fabsf(x); }

float ni::round(float x) { return roundf(x); }

float ni::sign(float x) { return (x < 0.0f ? -1.0f : 1.0f); }

float ni::floor(float x) { return floorf(x); }

float ni::ceil(float x) { return ceilf(x); }

float ni::fract(float x) { return x - floorf(x); }

float ni::mod(float x, float y) { return fmodf(x, y); }

float ni::clamp(float n, float x, float y) { return (n < x ? x : n > y ? y : n); }

float ni::mix(float x, float y, float n) { return x * (1.0f - n) + y * n; }

float ni::step(float edge, float x) { return (x < edge ? 0.0f : 1.0f); }

float ni::smootStep(float edge0, float edge1, float x)
{
    if (x < edge0) return 0.0f;
    if (x > edge1) return 1.0f;
    float t = ni::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float ni::saturate(float Value)
{
    return ni::clamp(Value, 0.0f, 1.0f);
}

float ni::normalize(float Value, float MinValue, float MaxValue)
{
    return (Value - MinValue) / (MaxValue - MinValue);
}

float ni::random()
{
    return (float)rand() / (float)RAND_MAX;
}

namespace ni {
    Float2::Float2() : x(0.0f), y(0.0f) {}

    Float2::Float2(float x) : x(x), y(x) {}

    Float2::Float2(float x, float y) : x(x), y(y) {}

    Float2& Float2::normalize()
    {
        float len = length();
        if (len != 0.0f)
        {
            x /= len;
            y /= len;
        }
        return *this;
    }

    Float2& Float2::faceForward(const Float2& incident, const Float2& reference)
    {
        if (incident.dot(reference) >= 0.0f)
        {
            *this *= -1.0f;
        }
        return *this;
    }

    Float2& Float2::reflect(const Float2& normal)
    {
        *this -= 2.0f * dot(normal) * normal;
        return *this;
    }

    Float2& Float2::refract(const Float2& normal, float indexOfRefraction)
    {
        Float2 incident = *this;
        float IdotN = incident.dot(normal);
        float k = 1.0f - indexOfRefraction * indexOfRefraction * (1.0f - IdotN * IdotN);
        if (k < 0.0f) *this = 0.0f;
        else *this = indexOfRefraction * incident - (indexOfRefraction * IdotN + ni::sqrt(k)) * normal;
        return *this;
    }

    Float3 Float2::toFloat3() { return Float3(x, y, 0.0f); }
    Float4 Float2::toFloat4() { return Float4(x, y, 0.0f, 0.0f); }
    Float2& Float2::operator*=(const Float2x2& m)
    {
        float tx = m.data[0] * x + m.data[2] * y;
        float ty = m.data[1] * x + m.data[3] * y;
        x = tx;
        y = ty;
        return *this;
    }

    Float3::Float3() : x(0.0f), y(0.0f), z(0.0f) {}

    Float3::Float3(float x) : x(x), y(x), z(x) {}

    Float3::Float3(float x, float y, float z) : x(x), y(y), z(z) {}

    float Float3::lengthSqr() const { return x * x + y * y + z * z; }

    float Float3::length() const { return ni::sqrt(lengthSqr()); }

    float Float3::dot(const Float3& n) const { return x * n.x + y * n.y + z * n.z; }

    float Float3::distance(const Float3& n) const { return (*this - n).length(); }

    Float3& Float3::normalize()
    {
        float len = x * x + y * y + z * z;
        if (len > 0)
        {
            len = 1 / ni::sqrt(len);
        }
        if (len != 0.0f)
        {
            x /= len;
            y /= len;
            z /= len;
        }
        return *this;
    }

    Float3& Float3::cross(const Float3& n)
    {
        float cx = y * n.z - n.y * z;
        float cy = z * n.x - n.z * x;
        float cz = x * n.y - n.x * y;
        x = cx;
        y = cy;
        z = cz;
        return *this;
    }

    Float3& Float3::faceForward(const Float3& incident, const Float3& reference)
    {
        if (incident.dot(reference) >= 0.0f)
        {
            *this *= -1.0f;
        }
        return *this;
    }

    Float3& Float3::reflect(const Float3& normal)
    {
        *this = *this - 2.0f * dot(normal) * normal;
        return *this;
    }

    Float3& Float3::refract(const Float3& normal, float indexOfRefraction)
    {
        Float3 incident = *this;
        float IdotN = incident.dot(normal);
        float k = 1.0f - indexOfRefraction * indexOfRefraction * (1.0f - IdotN * IdotN);
        if (k < 0.0f) *this = 0.0f;
        else *this = indexOfRefraction * incident - (indexOfRefraction * IdotN + ni::sqrt(k)) * normal;
        return *this;
    }

    Float4 Float3::toFloat4() { return Float4(x, y, z, 0.0f); }
    Float3& Float3::operator*=(const Float3x3& m)
    {
        float tx = x * m.data[0] + y * m.data[3] + z * m.data[6];
        float ty = x * m.data[1] + y * m.data[4] + z * m.data[7];
        float tz = x * m.data[2] + y * m.data[5] + z * m.data[8];
        x = tx;
        y = ty;
        z = tz;
        return *this;
    }

    Float2 Float3::toFloat2() { return Float2(x, y); }

    Float4::Float4() : x(0.0f), y(0.0f), z(0.0f), w(0.0f) {}

    Float4::Float4(float x) : x(x), y(x), z(x), w(x) {}

    Float4::Float4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}

    float Float4::lengthSqr() const { return x * x + y * y + z * z + w * w; }

    float Float4::length() const { return ni::sqrt(lengthSqr()); }

    float Float4::dot(const Float4& n) const { return x * n.x + y * n.y + z * n.z + w * n.w; }

    float Float4::distance(const Float4& n) const { return (*this - n).length(); }

    Float4& Float4::normalize()
    {
        float len = length();
        if (len != 0.0f)
        {
            x /= len;
            y /= len;
            z /= len;
            w /= len;
        }
        return *this;
    }

    Float4& Float4::faceForward(const Float4& incident, const Float4& reference)
    {
        if (incident.dot(reference) >= 0.0f)
        {
            *this *= -1.0f;
        }
        return *this;
    }

    Float4& Float4::reflect(const Float4& normal)
    {
        *this -= 2.0f * dot(normal) * normal;
        return *this;
    }

    Float4& Float4::refract(const Float4& normal, float indexOfRefraction)
    {
        Float4 incident = *this;
        float IdotN = incident.dot(normal);
        float k = 1.0f - indexOfRefraction * indexOfRefraction * (1.0f - IdotN * IdotN);
        if (k < 0.0f) *this = 0.0f;
        else *this = indexOfRefraction * incident - (indexOfRefraction * IdotN + ni::sqrt(k)) * normal;
        return *this;
    }

    Float4& Float4::operator*=(const Float4x4& m)
    {
        float tx = x * m.data[0x0] + y * m.data[0x4] + z * m.data[0x8] + w * m.data[0xC];
        float ty = x * m.data[0x1] + y * m.data[0x5] + z * m.data[0x9] + w * m.data[0xD];
        float tz = x * m.data[0x2] + y * m.data[0x6] + z * m.data[0xA] + w * m.data[0xE];
        float tw = x * m.data[0x3] + y * m.data[0x7] + z * m.data[0xB] + w * m.data[0xF];
        x = tx;
        y = ty;
        z = tz;
        w = tw;
        return *this;
    }

    Float3 Float4::toFloat3() { return Float3(x, y, z); }

    Float2 Float4::toFloat2() { return Float2(x, y); }

    Float4x4::Float4x4()
    {
        data[0] = 1.0f; data[1] = 0.0f; data[2] = 0.0f; data[3] = 0.0f;
        data[4] = 0.0f; data[5] = 1.0f; data[6] = 0.0f; data[7] = 0.0f;
        data[8] = 0.0f; data[9] = 0.0f; data[10] = 1.0f; data[11] = 0.0f;
        data[12] = 0.0f; data[13] = 0.0f; data[14] = 0.0f; data[15] = 1.0f;
    }

    Float4x4::Float4x4(float a, float b, float c, float d, float e, float f, float g, float h, float i, float j, float k, float l, float m, float n, float o, float p)
    {
        data[0] = a; data[1] = b; data[2] = c; data[3] = d;
        data[4] = e; data[5] = f; data[6] = g; data[7] = h;
        data[8] = i; data[9] = j; data[10] = k; data[11] = l;
        data[12] = m; data[13] = n; data[14] = o; data[15] = p;
    }

    Float4x4& Float4x4::loadIdentity()
    {
        data[0] = 1.0f; data[1] = 0.0f; data[2] = 0.0f; data[3] = 0.0f;
        data[4] = 0.0f; data[5] = 1.0f; data[6] = 0.0f; data[7] = 0.0f;
        data[8] = 0.0f; data[9] = 0.0f; data[10] = 1.0f; data[11] = 0.0f;
        data[12] = 0.0f; data[13] = 0.0f; data[14] = 0.0f; data[15] = 1.0f;
        return *this;
    }

    Float4x4& Float4x4::transpose()
    {
        float m00 = data[0x0]; float m01 = data[0x1]; float m02 = data[0x2]; float m03 = data[0x3];
        float m10 = data[0x4]; float m11 = data[0x5]; float m12 = data[0x6]; float m13 = data[0x7];
        float m20 = data[0x8]; float m21 = data[0x9]; float m22 = data[0xA]; float m23 = data[0xB];
        float m30 = data[0xC]; float m31 = data[0xD]; float m32 = data[0xE]; float m33 = data[0xF];

        data[0x0] = m00; data[0x1] = m10; data[0x2] = m20; data[0x3] = m30;
        data[0x4] = m01; data[0x5] = m11; data[0x6] = m21; data[0x7] = m31;
        data[0x8] = m02; data[0x9] = m12; data[0xA] = m22; data[0xB] = m32;
        data[0xC] = m03; data[0xD] = m13; data[0xE] = m23; data[0xF] = m33;
        return *this;
    }

    Float4x4& Float4x4::translate(const Float3& v)
    {
        data[12] = data[0] * v.x + data[4] * v.y + data[8] * v.z + data[12];
        data[13] = data[1] * v.x + data[5] * v.y + data[9] * v.z + data[13];
        data[14] = data[2] * v.x + data[6] * v.y + data[10] * v.z + data[14];
        data[15] = data[3] * v.x + data[7] * v.y + data[11] * v.z + data[15];
        return *this;
    }

    Float4x4& Float4x4::scale(const Float3& v)
    {
        data[0] = data[0] * v.x;
        data[1] = data[1] * v.x;
        data[2] = data[2] * v.x;
        data[3] = data[3] * v.x;
        data[4] = data[4] * v.y;
        data[5] = data[5] * v.y;
        data[6] = data[6] * v.y;
        data[7] = data[7] * v.y;
        data[8] = data[8] * v.z;
        data[9] = data[9] * v.z;
        data[10] = data[10] * v.z;
        data[11] = data[11] * v.z;
        return *this;
    }

    Float4x4& Float4x4::rotate(const Float3& Euler)
    {
        float cb = ni::cos(Euler.x);
        float sb = ni::sin(Euler.x);
        float ch = ni::cos(Euler.y);
        float sh = ni::sin(Euler.y);
        float ca = ni::cos(Euler.z);
        float sa = ni::sin(Euler.z);
        const float RotationMatrix[] = {
                ch * ca, sh * sb - ch * sa * cb, ch * sa * sb + sh * cb, 0,
                sa, ca * cb, -ca * sb, 0,
                -sh * ca, sh * sa * cb + ch * sb, -sh * sa * sb + ch * cb, 0,
                0, 0, 0, 1
        };

        float m0 = RotationMatrix[0] * data[0] + RotationMatrix[1] * data[4] + RotationMatrix[2] * data[8] + RotationMatrix[3] * data[12];
        float m1 = RotationMatrix[0] * data[1] + RotationMatrix[1] * data[5] + RotationMatrix[2] * data[9] + RotationMatrix[3] * data[13];
        float m2 = RotationMatrix[0] * data[2] + RotationMatrix[1] * data[6] + RotationMatrix[2] * data[10] + RotationMatrix[3] * data[14];
        float m3 = RotationMatrix[0] * data[3] + RotationMatrix[1] * data[7] + RotationMatrix[2] * data[11] + RotationMatrix[3] * data[15];
        float m4 = RotationMatrix[4] * data[0] + RotationMatrix[5] * data[4] + RotationMatrix[6] * data[8] + RotationMatrix[7] * data[12];
        float m5 = RotationMatrix[4] * data[1] + RotationMatrix[5] * data[5] + RotationMatrix[6] * data[9] + RotationMatrix[7] * data[13];
        float m6 = RotationMatrix[4] * data[2] + RotationMatrix[5] * data[6] + RotationMatrix[6] * data[10] + RotationMatrix[7] * data[14];
        float m7 = RotationMatrix[4] * data[3] + RotationMatrix[5] * data[7] + RotationMatrix[6] * data[11] + RotationMatrix[7] * data[15];
        float m8 = RotationMatrix[8] * data[0] + RotationMatrix[9] * data[4] + RotationMatrix[10] * data[8] + RotationMatrix[11] * data[12];
        float m9 = RotationMatrix[8] * data[1] + RotationMatrix[9] * data[5] + RotationMatrix[10] * data[9] + RotationMatrix[11] * data[13];
        float m10 = RotationMatrix[8] * data[2] + RotationMatrix[9] * data[6] + RotationMatrix[10] * data[10] + RotationMatrix[11] * data[14];
        float m11 = RotationMatrix[8] * data[3] + RotationMatrix[9] * data[7] + RotationMatrix[10] * data[11] + RotationMatrix[11] * data[15];
        float m12 = RotationMatrix[12] * data[0] + RotationMatrix[13] * data[4] + RotationMatrix[14] * data[8] + RotationMatrix[15] * data[12];
        float m13 = RotationMatrix[12] * data[1] + RotationMatrix[13] * data[5] + RotationMatrix[14] * data[9] + RotationMatrix[15] * data[13];
        float m14 = RotationMatrix[12] * data[2] + RotationMatrix[13] * data[6] + RotationMatrix[14] * data[10] + RotationMatrix[15] * data[14];
        float m15 = RotationMatrix[12] * data[3] + RotationMatrix[13] * data[7] + RotationMatrix[14] * data[11] + RotationMatrix[15] * data[15];

        data[0] = m0;
        data[1] = m1;
        data[2] = m2;
        data[3] = m3;
        data[4] = m4;
        data[5] = m5;
        data[6] = m6;
        data[7] = m7;
        data[8] = m8;
        data[9] = m9;
        data[10] = m10;
        data[11] = m11;
        data[12] = m12;
        data[13] = m13;
        data[14] = m14;
        data[15] = m15;

        return *this;
    }

    Float4x4& Float4x4::rotateX(float rad)
    {
        float s = ni::sin(rad);
        float c = ni::cos(rad);
        float a10 = data[4];
        float a11 = data[5];
        float a12 = data[6];
        float a13 = data[7];
        float a20 = data[8];
        float a21 = data[9];
        float a22 = data[10];
        float a23 = data[11];
        data[4] = a10 * c + a20 * s;
        data[5] = a11 * c + a21 * s;
        data[6] = a12 * c + a22 * s;
        data[7] = a13 * c + a23 * s;
        data[8] = a20 * c - a10 * s;
        data[9] = a21 * c - a11 * s;
        data[10] = a22 * c - a12 * s;
        data[11] = a23 * c - a13 * s;
        return *this;
    }

    Float4x4& Float4x4::rotateY(float rad)
    {
        float s = ni::sin(rad);
        float c = ni::cos(rad);
        float a00 = data[0];
        float a01 = data[1];
        float a02 = data[2];
        float a03 = data[3];
        float a20 = data[8];
        float a21 = data[9];
        float a22 = data[10];
        float a23 = data[11];

        data[0] = a00 * c - a20 * s;
        data[1] = a01 * c - a21 * s;
        data[2] = a02 * c - a22 * s;
        data[3] = a03 * c - a23 * s;
        data[8] = a00 * s + a20 * c;
        data[9] = a01 * s + a21 * c;
        data[10] = a02 * s + a22 * c;
        data[11] = a03 * s + a23 * c;
        return *this;
    }

    Float4x4& Float4x4::rotateZ(float rad)
    {
        float s = ni::sin(rad);
        float c = ni::cos(rad);
        float a00 = data[0];
        float a01 = data[1];
        float a02 = data[2];
        float a03 = data[3];
        float a10 = data[4];
        float a11 = data[5];
        float a12 = data[6];
        float a13 = data[7];

        data[0] = a00 * c + a10 * s;
        data[1] = a01 * c + a11 * s;
        data[2] = a02 * c + a12 * s;
        data[3] = a03 * c + a13 * s;
        data[4] = a10 * c - a00 * s;
        data[5] = a11 * c - a01 * s;
        data[6] = a12 * c - a02 * s;
        data[7] = a13 * c - a03 * s;
        return *this;
    }

    Float4x4& Float4x4::perspective(float fovy, float aspect, float nearBound, float farBound)
    {
        float f = 1.0f / ni::tan(fovy / 2.0f);
        data[0] = f / aspect;
        data[1] = 0;
        data[2] = 0;
        data[3] = 0;
        data[4] = 0;
        data[5] = f;
        data[6] = 0;
        data[7] = 0;
        data[8] = 0;
        data[9] = 0;
        data[11] = -1;
        data[12] = 0;
        data[13] = 0;
        data[15] = 0;
        float nf = 1 / (nearBound - farBound);
        data[10] = (farBound + nearBound) * nf;
        data[14] = (2 * farBound * nearBound) * nf;
        return *this;
    }

    Float4x4& Float4x4::orthographic(float left, float right, float bottom, float top, float nearBound, float farBound)
    {
        float lr = 1 / (left - right);
        float bt = 1 / (bottom - top);
        float nf = 1 / (nearBound - farBound);
        data[0] = -2 * lr;
        data[1] = 0;
        data[2] = 0;
        data[3] = 0;
        data[4] = 0;
        data[5] = -2 * bt;
        data[6] = 0;
        data[7] = 0;
        data[8] = 0;
        data[9] = 0;
        data[10] = 2 * nf;
        data[11] = 0;
        data[12] = (left + right) * lr;
        data[13] = (top + bottom) * bt;
        data[14] = (nearBound + farBound) * nf;
        data[15] = 1;
        return *this;
    }

    Float4x4& Float4x4::lookAt(const Float3& eye, const Float3& center, const Float3& up)
    {
        const float E = 0.000001f;
        float x0, x1, x2, y0, y1, y2, z0, z1, z2, len;
        float eyex = eye.x;
        float eyey = eye.y;
        float eyez = eye.z;
        float upx = up.x;
        float upy = up.y;
        float upz = up.z;
        float centerx = center.x;
        float centery = center.y;
        float centerz = center.z;
        if (ni::abs(eyex - centerx) < E &&
            ni::abs(eyey - centery) < E &&
            ni::abs(eyez - centerz) < E)
        {
            loadIdentity();
            return *this;
        }
        z0 = eyex - centerx;
        z1 = eyey - centery;
        z2 = eyez - centerz;
        len = 1.0f / ni::sqrt(z0 * z0 + z1 * z1 + z2 * z2);
        z0 *= len;
        z1 *= len;
        z2 *= len;
        x0 = upy * z2 - upz * z1;
        x1 = upz * z0 - upx * z2;
        x2 = upx * z1 - upy * z0;
        len = ni::sqrt(x0 * x0 + x1 * x1 + x2 * x2);
        if (!len)
        {
            x0 = 0.0f;
            x1 = 0.0f;
            x2 = 0.0f;
        }
        else
        {
            len = 1.0f / len;
            x0 *= len;
            x1 *= len;
            x2 *= len;
        }
        y0 = z1 * x2 - z2 * x1;
        y1 = z2 * x0 - z0 * x2;
        y2 = z0 * x1 - z1 * x0;
        len = ni::sqrt(y0 * y0 + y1 * y1 + y2 * y2);
        if (!len)
        {
            y0 = 0.0f;
            y1 = 0.0f;
            y2 = 0.0f;
        }
        else
        {
            len = 1.0f / len;
            y0 *= len;
            y1 *= len;
            y2 *= len;
        }
        data[0] = x0;
        data[1] = y0;
        data[2] = z0;
        data[3] = 0.0f;
        data[4] = x1;
        data[5] = y1;
        data[6] = z1;
        data[7] = 0.0f;
        data[8] = x2;
        data[9] = y2;
        data[10] = z2;
        data[11] = 0.0f;
        data[12] = -(x0 * eyex + x1 * eyey + x2 * eyez);
        data[13] = -(y0 * eyex + y1 * eyey + y2 * eyez);
        data[14] = -(z0 * eyex + z1 * eyey + z2 * eyez);
        data[15] = 1;
        return *this;
    }

    Float4x4& Float4x4::invert()
    {
        float d0 = data[0] * data[5] - data[1] * data[4];
        float d1 = data[0] * data[6] - data[2] * data[4];
        float d2 = data[0] * data[7] - data[3] * data[4];
        float d3 = data[1] * data[6] - data[2] * data[5];
        float d4 = data[1] * data[7] - data[3] * data[5];
        float d5 = data[2] * data[7] - data[3] * data[6];
        float d6 = data[8] * data[13] - data[9] * data[12];
        float d7 = data[8] * data[14] - data[10] * data[12];
        float d8 = data[8] * data[15] - data[11] * data[12];
        float d9 = data[9] * data[14] - data[10] * data[13];
        float d10 = data[9] * data[15] - data[11] * data[13];
        float d11 = data[10] * data[15] - data[11] * data[14];
        float determinant = d0 * d11 - d1 * d10 + d2 * d9 + d3 * d8 - d4 * d7 + d5 * d6;

        if (determinant == 0.0f) return *this;

        determinant = 1.0f / determinant;
        float m00 = (data[5] * d11 - data[6] * d10 + data[7] * d9) * determinant;
        float m01 = (data[2] * d10 - data[1] * d11 - data[3] * d9) * determinant;
        float m02 = (data[13] * d5 - data[14] * d4 + data[15] * d3) * determinant;
        float m03 = (data[10] * d4 - data[9] * d5 - data[11] * d3) * determinant;
        float m04 = (data[6] * d8 - data[4] * d11 - data[7] * d7) * determinant;
        float m05 = (data[0] * d11 - data[2] * d8 + data[3] * d7) * determinant;
        float m06 = (data[14] * d2 - data[12] * d5 - data[15] * d1) * determinant;
        float m07 = (data[8] * d5 - data[10] * d2 + data[11] * d1) * determinant;
        float m08 = (data[4] * d10 - data[5] * d8 + data[7] * d6) * determinant;
        float m09 = (data[1] * d8 - data[0] * d10 - data[3] * d6) * determinant;
        float m10 = (data[12] * d4 - data[13] * d2 + data[15] * d0) * determinant;
        float m11 = (data[9] * d2 - data[8] * d4 - data[11] * d0) * determinant;
        float m12 = (data[5] * d7 - data[4] * d9 - data[6] * d6) * determinant;
        float m13 = (data[0] * d9 - data[1] * d7 + data[2] * d6) * determinant;
        float m14 = (data[13] * d1 - data[12] * d3 - data[14] * d0) * determinant;
        float m15 = (data[8] * d3 - data[9] * d1 + data[10] * d0) * determinant;

        data[0] = m00;
        data[1] = m01;
        data[2] = m02;
        data[3] = m03;
        data[4] = m04;
        data[5] = m05;
        data[6] = m06;
        data[7] = m07;
        data[8] = m08;
        data[9] = m09;
        data[10] = m10;
        data[11] = m11;
        data[12] = m12;
        data[13] = m13;
        data[14] = m14;
        data[15] = m15;

        return *this;
    }

    Quaternion::Quaternion() : x(0.0f), y(0.0f), z(0.0f), w(1.0f) {}

    Quaternion::Quaternion(float x) : x(x), y(x), z(x), w(x) {}

    Quaternion::Quaternion(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}

    Quaternion& Quaternion::loadIdentity() { x = 0.0f; y = 0.0f; z = 0.0f; w = 1.0f; return *this; }

    Quaternion& Quaternion::fromEulerAngles(const Float3& eulerAngles)
    {
        float roll = eulerAngles.x;
        float pitch = eulerAngles.y;
        float yaw = eulerAngles.z;
        float cy = ni::cos(yaw * 0.5f);
        float sy = ni::sin(yaw * 0.5f);
        float cp = ni::cos(pitch * 0.5f);
        float sp = ni::sin(pitch * 0.5f);
        float cr = ni::cos(roll * 0.5f);
        float sr = ni::sin(roll * 0.5f);

        w = cy * cp * cr + sy * sp * sr;
        x = cy * cp * sr + sy * sp * cr;
        y = sy * cp * sr + cy * sp * cr;
        z = sy * cp * cr + cy * sp * sr;

        return *this;
    }

    Float3 Quaternion::toEulerAngles()
    {
        float roll, pitch, yaw;
        float a = 2.0f * (w * x + y * z);
        float b = 1.0f - 2.0f * (x * x + y * y);

        roll = ni::atan2(a, b);

        float c = 2.0f * (w * y * z * x);
        if (ni::abs(c) >= 1.0f)
            pitch = copysignf(ni::PI / 2.0f, c);
        else
            pitch = ni::asin(c);

        float d = 2.0f * (w * z + x * y);
        float e = 1.0f - 2.0f * (y * y + z * z);
        yaw = ni::atan2(d, e);

        return Float3(roll, pitch, yaw);
    }

    Quaternion& Quaternion::rotateX(float rad)
    {
        float c = ni::cos(rad);
        float s = ni::sin(rad);
        float tx = x * s + w * c;
        float ty = y * s + z * c;
        float tz = z * s - y * c;
        float tw = w * s - x * c;
        x = tx;
        y = ty;
        z = tz;
        w = tw;
        return *this;
    }

    Quaternion& Quaternion::rotateY(float rad)
    {
        float c = ni::cos(rad);
        float s = ni::sin(rad);
        float tx = x * s - z * c;
        float ty = y * s + w * c;
        float tz = z * s + x * c;
        float tw = w * s - y * c;
        x = tx;
        y = ty;
        z = tz;
        w = tw;
        return *this;
    }

    Quaternion& Quaternion::rotateZ(float rad)
    {
        float c = ni::cos(rad);
        float s = ni::sin(rad);
        float tx = x * s + y * c;
        float ty = y * s - x * c;
        float tz = z * s + w * c;
        float tw = w * s - z * c;
        x = tx;
        y = ty;
        z = tz;
        w = tw;
        return *this;
    }

    Float4x4 Quaternion::toFloat4x4()
    {
        float x2 = x + x;
        float y2 = y + y;
        float z2 = z + z;
        float xx = x * x2;
        float yx = y * x2;
        float yy = y * y2;
        float zx = z * x2;
        float zy = z * y2;
        float zz = z * z2;
        float wx = w * x2;
        float wy = w * y2;
        float wz = w * z2;

        return Float4x4(
            1.0f - yy - zz,
            yx + wz,
            zx - wy,
            0.0f,
            yx - wz,
            1.0f - xx - zz,
            zy + wx,
            0.0f,
            zx + wy,
            zy - wx,
            1.0f - xx - yy,
            0.0f,
            0.0f,
            0.0f,
            0.0f,
            1.0f
        );
    }

    Float3x3::Float3x3()
    {
        data[0] = 1.0f; data[1] = 0.0f; data[2] = 0.0f;
        data[3] = 0.0f; data[4] = 1.0f; data[5] = 0.0f;
        data[6] = 0.0f; data[7] = 0.0f; data[8] = 1.0f;
    }

    Float3x3::Float3x3(float a, float b, float c, float d, float e, float f, float g, float h, float i)
    {
        data[0] = a; data[1] = b; data[2] = c;
        data[3] = d; data[4] = e; data[5] = f;
        data[6] = g; data[7] = h; data[8] = i;
    }

    Float3x3& Float3x3::loadIdentity()
    {
        data[0] = 1.0f; data[1] = 0.0f; data[2] = 0.0f;
        data[3] = 0.0f; data[4] = 1.0f; data[5] = 0.0f;
        data[6] = 0.0f; data[7] = 0.0f; data[8] = 1.0f;
        return *this;
    }

    Float3x3& Float3x3::transpose()
    {
        float m1 = data[1];
        float m2 = data[2];
        float m5 = data[5];
        data[1] = data[3];
        data[2] = data[6];
        data[3] = m1;
        data[5] = data[7];
        data[6] = m2;
        data[7] = m5;
        return *this;
    }

    Float3x3& Float3x3::translate(const Float3& n)
    {
        data[6] = n.x * data[0] + data[3];
        data[7] = n.x * data[0] + data[3];
        data[8] = n.x * data[0] + data[3];
        return *this;
    }

    Float3x3& Float3x3::rotate(float rad)
    {
        float c = ni::cos(rad);
        float s = ni::sin(rad);
        float a00 = data[0]; float a01 = data[1]; float a02 = data[2];
        float a10 = data[3]; float a11 = data[4]; float a12 = data[5];
        data[0] = c * a00 + s * a10;
        data[1] = c * a01 + s * a11;
        data[2] = c * a02 + s * a12;
        data[3] = -s * a00 + c * a10;
        data[4] = -s * a01 + c * a11;
        data[5] = -s * a02 + c * a12;
        return *this;
    }

    Float3x3& Float3x3::scale(const Float2& n)
    {
        data[0] = n.x * data[0];
        data[1] = n.x * data[1];
        data[2] = n.x * data[2];
        data[3] = n.y * data[3];
        data[4] = n.y * data[4];
        data[5] = n.y * data[5];
        return *this;
    }

    Float2x2::Float2x2()
    {
        data[0] = 1.0f; data[1] = 0.0f;
        data[2] = 0.0f; data[3] = 1.0f;
    }

    Float2x2::Float2x2(float a, float b, float c, float d)
    {
        data[0] = a; data[1] = b;
        data[2] = c; data[3] = d;
    }

    Float2x2& Float2x2::loadIdentity()
    {
        data[0] = 1.0f; data[1] = 0.0f;
        data[2] = 0.0f; data[3] = 1.0f;
        return *this;
    }

    Float2x2& Float2x2::transpose()
    {
        float m = data[1];
        data[1] = data[2];
        data[2] = m;
        return *this;
    }

    Float2x2& Float2x2::rotate(float rad)
    {
        float c = ni::cos(rad);
        float s = ni::sin(rad);
        float m00 = data[0]; float m01 = data[1];
        float m10 = data[2]; float m11 = data[3];

        data[0] = m00 * c + m10 * s;
        data[1] = m01 * c + m11 * s;
        data[2] = m00 * -s + m10 * c;
        data[3] = m01 * -s + m11 * c;

        return *this;
    }

    Float2x2& Float2x2::scale(const Float2& v)
    {
        data[0] = v.x * data[0];
        data[1] = v.x * data[1];
        data[2] = v.y * data[2];
        data[3] = v.y * data[3];
        return *this;
    }

    Matrix2D::Matrix2D()
    {
        loadIdentity();
    }

    Matrix2D::Matrix2D(const Matrix2D& Other)
    {
        ab = Other.ab;
        cd = Other.cd;
        txty = Other.txty;
    }

    Matrix2D::Matrix2D(float a, float b, float c, float d, float tx, float ty) :
        a(a), b(b), c(c), d(d), tx(tx), ty(ty)
    {
    }

    Matrix2D& Matrix2D::operator=(const Matrix2D& Other)
    {
        ab = Other.ab;
        cd = Other.cd;
        txty = Other.txty;
        return *this;
    }

    Matrix2D& Matrix2D::loadIdentity()
    {
        a = 1.0f;
        b = 0.0f;
        c = 0.0f;
        d = 1.0f;
        tx = 0.0f;
        ty = 0.0f;
        return *this;
    }

    Matrix2D& Matrix2D::translate(float x, float y)
    {
        float ttx = a * x + c * y + tx;
        float tty = b * x + d * y + ty;
        tx = ttx;
        ty = tty;
        return *this;
    }

    Matrix2D& Matrix2D::translate(Float2& TranslateVector)
    {
        return translate(TranslateVector.x, TranslateVector.y);
    }

    Matrix2D& Matrix2D::scale(float x, float y)
    {
        a = a * x;
        b = b * x;
        c = c * y;
        d = d * y;
        return *this;
    }

    Matrix2D& Matrix2D::scale(Float2& ScaleVector)
    {
        return scale(ScaleVector.x, ScaleVector.y);
    }

    Matrix2D& Matrix2D::rotate(float Rotation)
    {
        float cr = ni::cos(Rotation);
        float sr = ni::sin(Rotation);
        float m0 = cr * a + sr * c;
        float m1 = cr * b + sr * d;
        float m2 = -sr * a + cr * c;
        float m3 = -sr * b + cr * d;
        a = m0;
        b = m1;
        c = m2;
        d = m3;
        return *this;
    }
    FlyCamera::FlyCamera(ni::Float3 initialPosition) {
        position = initialPosition;
        velocity = { 0, 0, 0 };
        direction = { 0, 0, -1 };
        euler = { ni::toRad(90), 0, 0 };
    }
    void FlyCamera::update() {

        if (ni::mouseClick(ni::MOUSE_BUTTON_LEFT)) {
            currMousePoint = { ni::mouseX(), ni::mouseY() };
            prevMousePoint = { ni::mouseX(), ni::mouseY() };
        }

        if (ni::mouseDown(ni::MOUSE_BUTTON_LEFT)) {
            currMousePoint = { ni::mouseX(), ni::mouseY() };
        }

        if (ni::mouseDown(ni::MOUSE_BUTTON_LEFT)) {
            float sensitivity = 0.1f;
            float offsetX = (currMousePoint.x - prevMousePoint.x) * sensitivity;
            float offsetY = (currMousePoint.y - prevMousePoint.y) * sensitivity;
            prevMousePoint = currMousePoint;
            euler.x += ni::toRad(offsetX);
            euler.y -= ni::toRad(offsetY);
        }

        direction.x = ni::cos(euler.x) * ni::cos(euler.y);
        direction.y = ni::sin(euler.y);
        direction.z = ni::sin(euler.x) * ni::cos(euler.y);

        float moveSpeed = 4.15f;
        float moveDirForward = ni::atan2(direction.z, direction.x);
        float moveDirSide = ni::atan2(direction.z, direction.x) + (ni::PI / 2.0f);

        velocity *= 0.3f;

        if (ni::keyDown(ni::W)) {
            velocity.x += moveSpeed * ni::cos(moveDirForward);
            velocity.z += moveSpeed * ni::sin(moveDirForward);
        }
        else if (ni::keyDown(ni::S)) {
            velocity.x -= moveSpeed * ni::cos(moveDirForward);
            velocity.z -= moveSpeed * ni::sin(moveDirForward);
        }

        if (ni::keyDown(ni::D)) {
            velocity.x += moveSpeed * ni::cos(moveDirSide);
            velocity.z += moveSpeed * ni::sin(moveDirSide);
        }
        else if (ni::keyDown(ni::A)) {
            velocity.x -= moveSpeed * ni::cos(moveDirSide);
            velocity.z -= moveSpeed * ni::sin(moveDirSide);
        }

        if (ni::keyDown(ni::Q)) {
            velocity.y -= moveSpeed;
        }
        else if (ni::keyDown(ni::E)) {
            velocity.y += moveSpeed;
        }

        position += velocity * (1.0f / 60.0f);
    }
    void FlyCamera::update(ni::Float3 newPosition, ni::Float3 newEuler) {
        euler = newEuler;
        direction.x = ni::cos(euler.x) * ni::cos(euler.y);
        direction.y = ni::sin(euler.y);
        direction.z = ni::sin(euler.x) * ni::cos(euler.y);
        position = newPosition;
    }
    ni::Float4x4 FlyCamera::makeViewMatrix() {
        ni::Float3 front = direction;
        viewMatrix.loadIdentity();
        viewMatrix.lookAt(position, position + front.normalize(), ni::Float3(0, 1, 0));
        return viewMatrix;
    }
    void FlyCamera::reset() {
        position = 0;
        velocity = 0;
        direction = { 0, 0, -1 };
        euler = { ni::toRad(90), 0, 0 };
    }
}

ni::Float2 operator+(const ni::Float2& a, const ni::Float2& b)
{
    ni::Float2 c = a;
    c += b;
    return c;
}

ni::Float2 operator-(const ni::Float2& a, const ni::Float2& b)
{
    ni::Float2 c = a;
    c -= b;
    return c;
}

ni::Float2 operator*(const ni::Float2& a, const ni::Float2& b)
{
    ni::Float2 c = a;
    c *= b;
    return c;
}

ni::Float2 operator/(const ni::Float2& a, const ni::Float2& b)
{
    ni::Float2 c = a;
    c /= b;
    return c;
}

ni::Float2 operator*(const ni::Float2x2& a, const ni::Float2& b)
{
    ni::Float2 c = b;
    c *= a;
    return c;
}

ni::Float2 operator+(const ni::Float2& a, const float& b)
{
    ni::Float2 c = a;
    c += b;
    return c;
}

ni::Float2 operator-(const ni::Float2& a, const float& b)
{
    ni::Float2 c = a;
    c -= b;
    return c;
}

ni::Float2 operator*(const ni::Float2& a, const float& b)
{
    ni::Float2 c = a;
    c *= b;
    return c;
}

ni::Float2 operator/(const ni::Float2& a, const float& b)
{
    ni::Float2 c = a;
    c /= b;
    return c;
}

ni::Float2 operator+(const float& a, const ni::Float2& b)
{
    ni::Float2 c = b;
    c += a;
    return c;
}

ni::Float2 operator-(const float& a, const ni::Float2& b)
{
    ni::Float2 c = b;
    c -= a;
    return c;
}

ni::Float2 operator*(const float& a, const ni::Float2& b)
{
    ni::Float2 c = b;
    c *= a;
    return c;
}

ni::Float2 operator/(const float& a, const ni::Float2& b)
{
    ni::Float2 c = b;
    c /= a;
    return c;
}

ni::Float2 operator-(const ni::Float2& a)
{
    return ni::Float2(-a.x, -a.y);
}

ni::Float2 operator*(const ni::Matrix2D& a, const ni::Float2& b)
{
    float x = b.x * a.a + b.y * a.c + a.tx;
    float y = b.x * a.b + b.y * a.d + a.ty;
    return ni::Float2(x, y);
}

// Vector3
ni::Float3 operator+(const ni::Float3& a, const ni::Float3& b)
{
    ni::Float3 c = a;
    c += b;
    return c;
}

ni::Float3 operator-(const ni::Float3& a, const ni::Float3& b)
{
    ni::Float3 c = a;
    c -= b;
    return c;
}

ni::Float3 operator*(const ni::Float3& a, const ni::Float3& b)
{
    ni::Float3 c = a;
    c *= b;
    return c;
}

ni::Float3 operator/(const ni::Float3& a, const ni::Float3& b)
{
    ni::Float3 c = a;
    c /= b;
    return c;
}

ni::Float3 operator*(const ni::Float3x3& a, const ni::Float3& b)
{
    ni::Float3 c = b;
    c *= a;
    return c;
}

ni::Float3 operator+(const ni::Float3& a, const float& b)
{
    ni::Float3 c = a;
    c += b;
    return c;
}

ni::Float3 operator-(const ni::Float3& a, const float& b)
{
    ni::Float3 c = a;
    c -= b;
    return c;
}

ni::Float3 operator*(const ni::Float3& a, const float& b)
{
    ni::Float3 c = a;
    c *= b;
    return c;
}

ni::Float3 operator/(const ni::Float3& a, const float& b)
{
    ni::Float3 c = a;
    c /= b;
    return c;
}

ni::Float3 operator+(const float& a, const ni::Float3& b)
{
    ni::Float3 c = b;
    c += a;
    return c;
}

ni::Float3 operator-(const float& a, const ni::Float3& b)
{
    ni::Float3 c = b;
    c -= a;
    return c;
}

ni::Float3 operator*(const float& a, const ni::Float3& b)
{
    ni::Float3 c = b;
    c *= a;
    return c;
}

ni::Float3 operator/(const float& a, const ni::Float3& b)
{
    ni::Float3 c = b;
    c /= a;
    return c;
}

ni::Float3 operator-(const ni::Float3& a)
{
    return ni::Float3(-a.x, -a.y, -a.z);
}

// Vector4
ni::Float4 operator+(const ni::Float4& a, const ni::Float4& b)
{
    ni::Float4 c = a;
    c += b;
    return c;
}

ni::Float4 operator-(const ni::Float4& a, const ni::Float4& b)
{
    ni::Float4 c = a;
    c -= b;
    return c;
}

ni::Float4 operator*(const ni::Float4& a, const ni::Float4& b)
{
    ni::Float4 c = a;
    c *= b;
    return c;
}

ni::Float4 operator/(const ni::Float4& a, const ni::Float4& b)
{
    ni::Float4 c = a;
    c /= b;
    return c;
}

ni::Float4 operator*(const ni::Float4x4& a, const ni::Float4& b)
{
    ni::Float4 c = b;
    c *= a;
    return c;
}

ni::Float4 operator+(const ni::Float4& a, const float& b)
{
    ni::Float4 c = a;
    c += b;
    return c;
}

ni::Float4 operator-(const ni::Float4& a, const float& b)
{
    ni::Float4 c = a;
    c -= b;
    return c;
}

ni::Float4 operator*(const ni::Float4& a, const float& b)
{
    ni::Float4 c = a;
    c *= b;
    return c;
}

ni::Float4 operator/(const ni::Float4& a, const float& b)
{
    ni::Float4 c = a;
    c /= b;
    return c;
}

ni::Float4 operator+(const float& a, const ni::Float4& b)
{
    ni::Float4 c = b;
    c += a;
    return c;
}

ni::Float4 operator-(const float& a, const ni::Float4& b)
{
    ni::Float4 c = b;
    c -= a;
    return c;
}

ni::Float4 operator*(const float& a, const ni::Float4& b)
{
    ni::Float4 c = b;
    c *= a;
    return c;
}

ni::Float4 operator/(const float& a, const ni::Float4& b)
{
    ni::Float4 c = b;
    c /= a;
    return c;
}

ni::Float4 operator-(const ni::Float4& a)
{
    return ni::Float4(-a.x, -a.y, -a.z, -a.w);
}

ni::Float2x2 operator*(const ni::Float2x2& a, const ni::Float2x2& b)
{
    ni::Float2x2 c = a;
    c *= b;
    return c;
}

ni::Float3x3 operator*(const ni::Float3x3& a, const ni::Float3x3& b)
{
    ni::Float3x3 c = a;
    c *= b;
    return c;
}

ni::Float4x4 operator*(const ni::Float4x4& a, const ni::Float4x4& b)
{
    ni::Float4x4 c = a;
    c *= b;
    return c;
}


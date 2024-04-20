#pragma once

#include <stdint.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <math.h>

///////////////////////////////////////////////////////////////
// CONFIG 
///////////////////////////////////////////////////////////////

#define NI_USE_FULLSCREEN 0
#define NI_FRAME_COUNT 3
#define NI_BACKBUFFER_COUNT 2
#define NI_MAX_DESCRIPTORS (1<<12)

///////////////////////////////////////////////////////////////

#define NI_EXIT(code) { ExitProcess(code); }
#define NI_DEBUG_BREAK() __debugbreak()
#define NI_LOG(fmt, ...) ni::logFmt(fmt "\n", ##__VA_ARGS__)
#define NI_PANIC(fmt, ...) { ni::logFmt("PANIC: " fmt "\n", ##__VA_ARGS__); NI_DEBUG_BREAK(); NI_EXIT(~0); }
#define NI_ASSERT(x, fmt, ...) if (!(x)) { ni::logFmt("ASSERT: " fmt "\n", ##__VA_ARGS__); NI_DEBUG_BREAK(); }
#define NI_D3D_ASSERT(x, ...) if ((x) != S_OK) { NI_PANIC(__VA_ARGS__); }
#define NI_D3D_RELEASE(obj) { if ((obj)) { (obj)->Release(); obj = nullptr; } }
#define NI_COLOR_UINT(color) (((color) & 0xff) << 24) | ((((color) >> 8) & 0xff) << 16) | ((((color) >> 16) & 0xff) << 8) | ((color) >> 24)
#define NI_COLOR_RGBA_UINT(r, g, b, a) ((uint32_t)(r)) | ((uint32_t)(g) << 8) | ((uint32_t)(b) << 16) | ((uint32_t)(a) << 24)
#define NI_COLOR_RGB_UINT(r, g, b) NI_COLOR_RGBA_UINT(r, g, b, 0xff)
#define NI_COLOR_RGBA_FLOAT(r, g, b, a) NI_COLOR_RGBA_UINT((uint8_t)((r) * 255.0f), (uint8_t)((g) * 255.0f), (uint8_t)((b) * 255.0f), (uint8_t)((a) * 255.0f))
#define NI_COLOR_RGB_FLOAT(r, g, b) NI_COLOR_RGBA_FLOAT(r, g, b, 1.0f)
#define NI_IMAGE_STATE_NONE (0b000)
#define NI_IMAGE_STATE_CREATED (0b001)
#define NI_IMAGE_STATE_UPLOADED (0b010)
#define NI_IMAGE_STATE_BOUND (0b100)

namespace ni {

	void logFmt(const char* fmt, ...);

	enum KeyCode : uint32_t {
		ALT = 18,
		DOWN = 40,
		LEFT = 37,
		RIGHT = 39,
		UP = 38,
		BACKSPACE = 8,
		CAPS_LOCK = 20,
		CTRL = 17,
		DEL = 46,
		END = 35,
		ENTER = 13,
		ESC = 27,
		F1 = 112,
		F2 = 113,
		F3 = 114,
		F4 = 115,
		F5 = 116,
		F6 = 117,
		F7 = 118,
		F8 = 119,
		F9 = 120,
		F10 = 121,
		F11 = 122,
		F12 = 123,
		HOME = 36,
		INSERT = 45,
		NUM_LOCK = 144,
		NUMPAD_0 = 96,
		NUMPAD_1 = 97,
		NUMPAD_2 = 98,
		NUMPAD_3 = 99,
		NUMPAD_4 = 100,
		NUMPAD_5 = 101,
		NUMPAD_6 = 102,
		NUMPAD_7 = 103,
		NUMPAD_8 = 104,
		NUMPAD_9 = 105,
		NUMPAD_MINUS = 109,
		NUMPAD_ASTERIKS = 106,
		NUMPAD_DOT = 110,
		NUMPAD_SLASH = 111,
		NUMPAD_SUM = 107,
		PAGE_DOWN = 34,
		PAGE_UP = 33,
		PAUSE = 19,
		PRINT_SCREEN = 44,
		SCROLL_LOCK = 145,
		SHIFT = 16,
		SPACE = 32,
		TAB = 9,
		A = 65,
		B = 66,
		C = 67,
		D = 68,
		E = 69,
		F = 70,
		G = 71,
		H = 72,
		I = 73,
		J = 74,
		K = 75,
		L = 76,
		M = 77,
		N = 78,
		O = 79,
		P = 80,
		Q = 81,
		R = 82,
		S = 83,
		T = 84,
		U = 85,
		V = 86,
		W = 87,
		X = 88,
		Y = 89,
		Z = 90,
		NUM_0 = 48,
		NUM_1 = 49,
		NUM_2 = 50,
		NUM_3 = 51,
		NUM_4 = 52,
		NUM_5 = 53,
		NUM_6 = 54,
		NUM_7 = 55,
		NUM_8 = 56,
		NUM_9 = 57,
		QUOTE = 192,
		MINUS = 189,
		COMMA = 188,
		SLASH = 191,
		SEMICOLON = 186,
		LEFT_SQRBRACKET = 219,
		RIGHT_SQRBRACKET = 221,
		BACKSLASH = 220,
		EQUALS = 187
	};

	enum MouseButton {
		MOUSE_BUTTON_LEFT = 0,
		MOUSE_BUTTON_MIDDLE = 1,
		MOUSE_BUTTON_RIGHT = 2
	};

	enum BufferType {
		INDEX_BUFFER,
		VERTEX_BUFFER,
		CONSTANT_BUFFER,
		UNORDERED_BUFFER,
		UPLOAD_BUFFER,
		SHADER_RESOURCE_BUFFER
	};

	struct FileReader {
		FileReader(const char* path);
		~FileReader();
		void* operator*() const;
		size_t getSize() const;
	private:
		void* buffer = nullptr;
		size_t size = 0;
	};

	template<typename T, typename TSize = uint64_t>
	struct Array {
		Array() : data(nullptr), num(0), capacity(0) {}

		void reset() {
			num = 0;
		}

		void add(const T& element) {
			checkResize();
			data[num++] = element;
		}

		void remove(TSize index) {
			NI_ASSERT(index < num, "Index out of bounds");
			for (TSize start = index; start < num - 1; ++start) {
				data[start] = data[start + 1];
			}
			num--;
		}

		void resize(TSize newCapacity) {
			T* newData = (T*)realloc(data, newCapacity * sizeof(T));
			NI_ASSERT(newData != nullptr, "Failed to reallocate array");
			data = newData;
			capacity = newCapacity;
		}

		void checkResize() {
			if (num + 1 >= capacity) {
				resize(capacity > 0 ? capacity * 2 : 16);
			}
		}

		T* getData() { return data; }
		const T* getData() const { return data; }

		TSize getNum() const { return num; }
		TSize getCapacity() const { return capacity;  }

	private:
		T* data;
		TSize num;
		TSize capacity;
	};

	struct RootSignatureDescriptorRange {
		RootSignatureDescriptorRange() {}
		~RootSignatureDescriptorRange() {}

		RootSignatureDescriptorRange& addRange(D3D12_DESCRIPTOR_RANGE_TYPE type, uint32_t descriptorNum, uint32_t baseShaderRegister, uint32_t registerSpace);
		const D3D12_DESCRIPTOR_RANGE* operator*() const { return ranges.getData(); }
		uint32_t getNum() const { return ranges.getNum(); }

	private:
		Array<D3D12_DESCRIPTOR_RANGE, uint32_t> ranges;
	};

	struct RootSignatureBuilder {
		RootSignatureBuilder() {}
		~RootSignatureBuilder() {}

		void addRootParameterDescriptorTable(const D3D12_DESCRIPTOR_RANGE* ranges, uint32_t rangeNum, D3D12_SHADER_VISIBILITY shaderVisibility);
		void addRootParameterDescriptorTable(const RootSignatureDescriptorRange& ranges, D3D12_SHADER_VISIBILITY shaderVisibility);
		void addRootParameterConstant(uint32_t shaderRegister, uint32_t registerSpace, uint32_t num32BitValues, D3D12_SHADER_VISIBILITY shaderVisibility);
		void addStaticSampler(D3D12_FILTER filter, D3D12_TEXTURE_ADDRESS_MODE addressModeAll, uint32_t shaderRegister, uint32_t registerSpace, D3D12_SHADER_VISIBILITY shaderVisibility);
		ID3D12RootSignature* build(bool isCompute);

	private:
		Array<D3D12_ROOT_PARAMETER, uint32_t> rootParameters;
		Array<D3D12_STATIC_SAMPLER_DESC, uint32_t> staticSamplers;
	};
	
	struct Resource {
		ID3D12Resource* resource;
		D3D12_RESOURCE_STATES state;
	};

	template<uint32_t NUM_BARRIERS>
	struct ResourceBarrierBatcher {

		struct BarrierData {
			D3D12_RESOURCE_BARRIER barrier;
			Resource* resource1;
			Resource* resource2;
		};

		ResourceBarrierBatcher() {
			memset(barriers, 0, sizeof(barriers));
			barrierNum = 0;
		}

		void transition(Resource* resource, D3D12_RESOURCE_STATES afterState) {
			if (resource->state != afterState) {
				BarrierData data = { {}, resource, nullptr };
				data.barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
				data.barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
				data.barrier.Transition.Subresource = 0;
				data.barrier.Transition.pResource = resource->resource;
				data.barrier.Transition.StateBefore = resource->state;
				data.barrier.Transition.StateAfter = afterState;
				barriers[barrierNum++] = data;
			}
		}

		void reset() {
			barrierNum = 0;
		}

		void flush(ID3D12GraphicsCommandList* commandList) {
			if (barrierNum == 0) return;
			D3D12_RESOURCE_BARRIER barrierArray[NUM_BARRIERS] = {};
			for (uint32_t index = 0; index < barrierNum; ++index) {
				barrierArray[index] = barriers[index].barrier;
			}
			commandList->ResourceBarrier(barrierNum, barrierArray);
			for (uint32_t index = 0; index < barrierNum; ++index) {
				BarrierData& data = barriers[index];
				if (data.barrier.Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION) {
					data.resource1->state = data.barrier.Transition.StateAfter;
				}
			}
			reset();
		}

		BarrierData barriers[NUM_BARRIERS];
		uint32_t barrierNum;
	};

	struct DescriptorHandle {

		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
	};

	struct DescriptorTable {
		void reset();
		DescriptorHandle allocate();
		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle(uint64_t index);
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle(uint64_t index);
		D3D12_GPU_DESCRIPTOR_HANDLE gpuBaseHandle;
		D3D12_CPU_DESCRIPTOR_HANDLE cpuBaseHandle;
		uint32_t handleSize;
		uint32_t allocated;
		uint32_t capacity;
	};

	struct DescriptorAllocator {

		void reset();
		DescriptorTable allocateDescriptorTable(uint32_t descriptorNum);

		ID3D12DescriptorHeap* descriptorHeap;
		D3D12_GPU_DESCRIPTOR_HANDLE gpuBaseHandle;
		D3D12_CPU_DESCRIPTOR_HANDLE cpuBaseHandle;
		uint32_t descriptorHandleSize;
		uint32_t descriptorAllocated;
	};

	struct FrameData {
		DescriptorTable descriptorTable;
		DescriptorAllocator descriptorAllocator;
		ID3D12GraphicsCommandList* commandList;
		ID3D12CommandAllocator* commandAllocator;
		ID3D12Fence* fence;
		HANDLE fenceEvent;
		uint64_t frameWaitValue;
		uint64_t frameIndex;
		void* userData;
	};

	struct Texture {
		ni::Resource texture;
		ni::Resource upload;
		uint32_t width;
		uint32_t height;
		uint32_t depth;
		const void* cpuData;
		uint32_t textureId;
		uint32_t state;
	};

	struct Renderer {
		ID3D12Device5* device;
		ID3D12Debug3* debugInterface;
		IDXGIFactory1* factory;
		IDXGIAdapter1* adapter;
		ID3D12CommandQueue* commandQueue;
		IDXGISwapChain1* swapChain;
		ID3D12DescriptorHeap* rtvDescriptorHeap;
		ID3D12DescriptorHeap* dsvDescriptorHeap;
		FrameData frames[NI_FRAME_COUNT];
		ID3D12Resource* backbuffers[NI_FRAME_COUNT];
		ID3D12Fence* presentFence;
		HANDLE presentFenceEvent;
		HWND windowHandle;
		uint32_t windowWidth;
		uint32_t windowHeight;
		uint64_t presentFenceValue;
		uint64_t presentFrame;
		uint64_t currentFrame;
		uint64_t frameCount;
		Texture** imagesToUpload;
		uint32_t imageToUploadNum;
		float mouseX;
		float mouseY;
		bool shouldQuit;
	};

	void init(uint32_t width, uint32_t height, const char* title);
	void setFrameUserData(uint32_t frame, void* data);
	void waitForCurrentFrame();
	void waitForAllFrames();
	void destroy();
	void pollEvents();
	bool shouldQuit();
	ni::FrameData& getFrameData();
	ID3D12Device* getDevice();
	ni::FrameData& beginFrame();
	void endFrame();
	ID3D12Resource* getCurrentBackbuffer();
	void present(bool vsync = true);
	ID3D12PipelineState* createGraphicsPipelineState(const wchar_t* name, const D3D12_GRAPHICS_PIPELINE_STATE_DESC& psoDesc);
	ID3D12PipelineState* createComputePipelineState(const wchar_t* name, const D3D12_COMPUTE_PIPELINE_STATE_DESC& psoDesc);
	float getViewWidth();
	float getViewHeight();
	Resource createBuffer(const wchar_t* name, size_t bufferSize, BufferType type, bool initToZero = false, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);
	D3D12_CPU_DESCRIPTOR_HANDLE getRenderTargetViewCPUHandle();
	D3D12_GPU_DESCRIPTOR_HANDLE getRenderTargetViewGPUHandle();
	D3D12_CPU_DESCRIPTOR_HANDLE getDepthStencilViewCPUHandle();
	D3D12_GPU_DESCRIPTOR_HANDLE getDepthStencilViewGPUHandle();
	float mouseX();
	float mouseY();
	bool mouseDown(MouseButton button);
	bool keyDown(KeyCode keyCode);
	float randomFloat();
	uint32_t randomUint();
	double getSeconds();
	size_t getDXGIFormatBits(DXGI_FORMAT format);
	size_t getDXGIFormatBytes(DXGI_FORMAT format);
	Texture* createTexture(const wchar_t* name, uint32_t width, uint32_t height, uint32_t depth, const void* pixels, DXGI_FORMAT dxgiFormat = DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);
	void destroyTexture(Texture*& image);
	uint64_t murmurHash(const void* key, uint64_t keyLength, uint64_t seed);
	inline void* offsetPtr(void* Ptr, intptr_t Offset) { return (void*)((intptr_t)Ptr + Offset); }
	inline void* alignPtr(void* Ptr, size_t Alignment) { return (void*)(((uintptr_t)(Ptr)+((uintptr_t)(Alignment)-1LL)) & ~((uintptr_t)(Alignment)-1LL)); }
	inline size_t alignSize(size_t Value, size_t Alignment) { return ((Value)+((Alignment)-1LL)) & ~((Alignment)-1LL); }
	size_t getFileSize(const char* path);
	bool readFile(const char* path, void* outBuffer);
	void* allocReadFile(const char* path);
	uint64_t getFrameCount();
}

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace ni {
	struct Float2;
	struct Float3;
	struct Float4;
	struct Float2x2;
	struct Float3x3;
	struct Float4x4;
	struct Quaternion;
	struct Matrix2D;
}

// Global operators
ni::Float2x2 operator*(const ni::Float2x2& a, const ni::Float2x2& b);
ni::Float3x3 operator*(const ni::Float3x3& a, const ni::Float3x3& b);
ni::Float4x4 operator*(const ni::Float4x4& a, const ni::Float4x4& b);

ni::Float2 operator+(const ni::Float2& a, const ni::Float2& b);
ni::Float2 operator-(const ni::Float2& a, const ni::Float2& b);
ni::Float2 operator*(const ni::Float2& a, const ni::Float2& b);
ni::Float2 operator/(const ni::Float2& a, const ni::Float2& b);
ni::Float2 operator*(const ni::Float2x2& a, const ni::Float2& b);
ni::Float2 operator*(const ni::Float2x2& a, const ni::Float2& b);
ni::Float2 operator+(const ni::Float2& a, const float& b);
ni::Float2 operator-(const ni::Float2& a, const float& b);
ni::Float2 operator*(const ni::Float2& a, const float& b);
ni::Float2 operator/(const ni::Float2& a, const float& b);
ni::Float2 operator+(const float& a, const ni::Float2& b);
ni::Float2 operator-(const float& a, const ni::Float2& b);
ni::Float2 operator*(const float& a, const ni::Float2& b);
ni::Float2 operator/(const float& a, const ni::Float2& b);
ni::Float2 operator-(const ni::Float2& a);
ni::Float2 operator*(const ni::Matrix2D& a, const ni::Float2& b);

ni::Float3 operator+(const ni::Float3& a, const ni::Float3& b);
ni::Float3 operator-(const ni::Float3& a, const ni::Float3& b);
ni::Float3 operator*(const ni::Float3& a, const ni::Float3& b);
ni::Float3 operator/(const ni::Float3& a, const ni::Float3& b);
ni::Float3 operator*(const ni::Float3x3& a, const ni::Float3& b);
ni::Float3 operator+(const ni::Float3& a, const float& b);
ni::Float3 operator-(const ni::Float3& a, const float& b);
ni::Float3 operator*(const ni::Float3& a, const float& b);
ni::Float3 operator/(const ni::Float3& a, const float& b);
ni::Float3 operator+(const float& a, const ni::Float3& b);
ni::Float3 operator-(const float& a, const ni::Float3& b);
ni::Float3 operator*(const float& a, const ni::Float3& b);
ni::Float3 operator/(const float& a, const ni::Float3& b);
ni::Float3 operator-(const ni::Float3& a);

ni::Float4 operator+(const ni::Float4& a, const ni::Float4& b);
ni::Float4 operator-(const ni::Float4& a, const ni::Float4& b);
ni::Float4 operator*(const ni::Float4& a, const ni::Float4& b);
ni::Float4 operator/(const ni::Float4& a, const ni::Float4& b);
ni::Float4 operator*(const ni::Float4x4& a, const ni::Float4& b);
ni::Float4 operator+(const ni::Float4& a, const float& b);
ni::Float4 operator-(const ni::Float4& a, const float& b);
ni::Float4 operator*(const ni::Float4& a, const float& b);
ni::Float4 operator/(const ni::Float4& a, const float& b);
ni::Float4 operator+(const float& a, const ni::Float4& b);
ni::Float4 operator-(const float& a, const ni::Float4& b);
ni::Float4 operator*(const float& a, const ni::Float4& b);
ni::Float4 operator/(const float& a, const ni::Float4& b);
ni::Float4 operator-(const ni::Float4& a);

namespace ni {

	constexpr float PI = 3.14159265359f;
	constexpr float TAU = PI * 2.0f;

	float toRad(float d);
	float toDeg(float r);
	float cos(float t);
	float sin(float t);
	float tan(float t);
	float asin(float t);
	float acos(float t);
	float atan(float t);
	float atan2(float y, float x);
	float pow(float x, float y);
	float exp(float x);
	float exp2(float x);
	float log(float x);
	float log2(float x);
	float sqrt(float x);
	float invSqrt(float x);
	float abs(float x);
	float round(float x);
	float sign(float x);
	float floor(float x);
	float ceil(float x);
	float fract(float x);
	float mod(float x, float y);
	float clamp(float n, float x, float y);
	float mix(float x, float y, float n);
	float step(float edge, float x);
	float smootStep(float edge0, float edge1, float x);
	float saturate(float Value);
	float normalize(float Value, float MinValue, float MaxValue);
	float random();

	template<typename T>
	static T min(T Lhs, T Rhs)
	{
		return Lhs < Rhs ? Lhs : Rhs;
	}

	template<typename T>
	static T max(T Lhs, T Rhs)
	{
		return Lhs > Rhs ? Lhs : Rhs;
	}


	struct Float2
	{
		union
		{
			struct { float x, y; };
			float components[2];
		};

		Float2();
		Float2(float x);
		Float2(float x, float y);
		Float2& operator=(const float& n) { x = n; y = n; return *this; }
		Float2& operator=(const Float2& n) { x = n.x; y = n.y; return *this; }
		bool operator==(const Float2& n) const { return x == n.x && y == n.y; }
		Float2& operator+=(const float& n) { x += n; y += n; return *this; }
		Float2& operator-=(const float& n) { x -= n; y -= n; return *this; }
		Float2& operator*=(const float& n) { x *= n; y *= n; return *this; }
		Float2& operator/=(const float& n) { x /= n; y /= n; return *this; }
		Float2& operator+=(const Float2& n) { x += n.x; y += n.y; return *this; }
		Float2& operator-=(const Float2& n) { x -= n.x; y -= n.y; return *this; }
		Float2& operator*=(const Float2& n) { x *= n.x; y *= n.y; return *this; }
		Float2& operator/=(const Float2& n) { x /= n.x; y /= n.y; return *this; }
		float& operator[](uint64_t Index) { return components[Index]; }
		float lengthSqr() const { return x * x + y * y; }
		float length() const { return ni::sqrt(lengthSqr()); }
		float dot(const Float2& n) const { return x * n.x + y * n.y; }
		float distance(const Float2& n) const { return (*this - n).length(); }
		Float2& normalize();
		Float2& faceForward(const Float2& Incident, const Float2& Reference);
		Float2& reflect(const Float2& Normal);
		Float2& refract(const Float2& Normal, float IndexOfRefraction);
		Float2& operator*=(const Float2x2& m);
		Float3 toFloat3();
		Float4 toFloat4();
	};

	struct Float3
	{
		union
		{
			struct { float x, y, z; };
			float components[3];
		};

		Float3();
		Float3(float x);
		Float3(float x, float y, float z);
		Float3& operator=(const float& n) { x = n; y = n; z = n; return *this; }
		Float3& operator=(const Float3& n) { x = n.x; y = n.y; z = n.z; return *this; }
		bool operator==(const Float3& n) const { return x == n.x && y == n.y && z == n.z; }
		Float3& operator+=(const float& n) { x += n; y += n; z += n; return *this; }
		Float3& operator-=(const float& n) { x -= n; y -= n; z -= n; return *this; }
		Float3& operator*=(const float& n) { x *= n; y *= n; z *= n; return *this; }
		Float3& operator/=(const float& n) { x /= n; y /= n; z /= n; return *this; }
		Float3& operator+=(const Float3& n) { x += n.x; y += n.y; z += n.z; return *this; }
		Float3& operator-=(const Float3& n) { x -= n.x; y -= n.y; z -= n.z; return *this; }
		Float3& operator*=(const Float3& n) { x *= n.x; y *= n.y; z *= n.z; return *this; }
		Float3& operator/=(const Float3& n) { x /= n.x; y /= n.y; z /= n.z; return *this; }
		float& operator[](uint64_t Index) { return components[Index]; }
		float lengthSqr() const;
		float length() const;
		float dot(const Float3& n) const;
		float distance(const Float3& n) const;
		Float3& normalize();
		Float3& cross(const Float3& n);
		Float3& faceForward(const Float3& Incident, const Float3& Reference);
		Float3& reflect(const Float3& Normal);
		Float3& refract(const Float3& Normal, float IndexOfRefraction);
		Float3& operator*=(const Float3x3& m);
		Float4 toFloat4();
		Float2 toFloat2();
	};

	struct Float4
	{
		union
		{
			struct { float x, y, z, w; };
			float components[4];
		};

		Float4();
		Float4(float x);
		Float4(float x, float y, float z, float w);
		Float4& operator=(const float& n) { x = n; y = n; z = n; w = n; return *this; }
		Float4& operator=(const Float2& n) { x = n.x; y = n.y; return *this; }
		bool operator==(const Float4& n) const { return x == n.x && y == n.y && z == n.z && w == n.w; }
		Float4& operator+=(const float& n) { x += n; y += n; z += n; w += n; return *this; }
		Float4& operator-=(const float& n) { x -= n; y -= n; z -= n; w -= n; return *this; }
		Float4& operator*=(const float& n) { x *= n; y *= n; z *= n; w *= n; return *this; }
		Float4& operator/=(const float& n) { x /= n; y /= n; z /= n; w /= n; return *this; }
		Float4& operator+=(const Float4& n) { x += n.x; y += n.y; z += n.z; w += n.w; return *this; }
		Float4& operator-=(const Float4& n) { x -= n.x; y -= n.y; z -= n.z; w -= n.w; return *this; }
		Float4& operator*=(const Float4& n) { x *= n.x; y *= n.y; z *= n.z; w *= n.w; return *this; }
		Float4& operator/=(const Float4& n) { x /= n.x; y /= n.y; z /= n.z; w /= n.w; return *this; }
		const float& operator[](uint64_t Index) const { return components[Index]; }
		float& operator[](uint64_t Index) { return components[Index]; }
		float lengthSqr() const;
		float length() const;
		float dot(const Float4& n) const;
		float distance(const Float4& n) const;
		Float4& normalize();
		Float4& faceForward(const Float4& Incident, const Float4& Reference);
		Float4& reflect(const Float4& Normal);
		Float4& refract(const Float4& Normal, float IndexOfRefraction);
		Float4& operator*=(const Float4x4& m);
		Float3 toFloat3();
		Float2 toFloat2();
	};

	struct Float2x2
	{
		union
		{
			float data[4];
			Float2 rows[2];
			struct
			{
				Float2 row0;
				Float2 row1;
			};
		};
		Float2x2();
		Float2x2(float a, float b, float c, float d);
		Float2x2& loadIdentity();
		Float2x2& operator=(const Float2x2& n)
		{
			data[0] = n.data[0]; data[1] = n.data[1];
			data[2] = n.data[2]; data[3] = n.data[3];
			return *this;
		}
		bool operator==(const Float2x2& n) const
		{
			return data[0] == n.data[0] && data[1] == n.data[1] &&
				data[2] == n.data[2] && data[3] == n.data[3];
		}
		Float2& operator[](uint64_t Index) { return rows[Index]; }
		Float2x2& transpose();
		Float2x2& operator*=(const Float2x2& m)
		{
			float a00 = data[0]; float a01 = data[1];;
			float a10 = data[2]; float a11 = data[3];;

			float b0 = m.data[0];
			float b1 = m.data[1];

			data[0] = b0 * a00 + b1 * a10;
			data[1] = b0 * a01 + b1 * a11;

			b0 = m.data[2];
			b1 = m.data[3];

			data[2] = b0 * a00 + b1 * a10;
			data[3] = b0 * a01 + b1 * a11;

			return *this;
		}
		Float2x2& rotate(float rad);
		Float2x2& scale(const Float2& v);
	};

	struct Float3x3
	{
		union
		{
			float data[9];
			Float3 rows[3];
			struct
			{
				Float3 row0;
				Float3 row1;
				Float3 row2;
			};
		};
		Float3x3();
		Float3x3(float a, float b, float c,
			float d, float e, float f,
			float g, float h, float i);
		Float3x3& loadIdentity();
		Float3x3& operator=(const Float3x3& n)
		{
			data[0] = n.data[0]; data[1] = n.data[1]; data[2] = n.data[2];
			data[3] = n.data[3]; data[4] = n.data[4]; data[5] = n.data[5];
			data[6] = n.data[6]; data[7] = n.data[7]; data[8] = n.data[8];
			return *this;
		}
		bool operator==(const Float3x3& n) const
		{
			return data[0] == n.data[0] && data[1] == n.data[1] && data[2] == n.data[2] &&
				data[3] == n.data[3] && data[4] == n.data[4] && data[5] == n.data[5];
			data[6] == n.data[6] && data[7] == n.data[7] && data[8] == n.data[8];
		}
		Float3& operator[](uint64_t Index) { return rows[Index]; }
		Float3x3& transpose();
		Float3x3& operator*=(const Float3x3& m)
		{
			float a00 = data[0]; float a01 = data[1]; float a02 = data[2];
			float a10 = data[3]; float a11 = data[4]; float a12 = data[5];
			float a20 = data[6]; float a21 = data[7]; float a22 = data[8];

			float b0 = m.data[0];
			float b1 = m.data[1];
			float b2 = m.data[2];

			data[0] = b0 * a00 + b1 * a10 + b2 * a20;
			data[1] = b0 * a01 + b1 * a11 + b2 * a21;
			data[2] = b0 * a02 + b1 * a12 + b2 * a22;

			b0 = m.data[3];
			b1 = m.data[4];
			b2 = m.data[5];

			data[3] = b0 * a00 + b1 * a10 + b2 * a20;
			data[4] = b0 * a01 + b1 * a11 + b2 * a21;
			data[5] = b0 * a02 + b1 * a12 + b2 * a22;

			b0 = m.data[6];
			b1 = m.data[7];
			b2 = m.data[8];

			data[6] = b0 * a00 + b1 * a10 + b2 * a20;
			data[7] = b0 * a01 + b1 * a11 + b2 * a21;
			data[8] = b0 * a02 + b1 * a12 + b2 * a22;

			return *this;
		}
		Float3x3& translate(const Float3& n);
		Float3x3& rotate(float rad);
		Float3x3& scale(const Float2& n);
	};

	struct Float4x4
	{
		union
		{
			float data[16];
			Float4 rows[4];
			struct
			{
				Float4 row0;
				Float4 row1;
				Float4 row2;
				Float4 row3;
			};
		};
		Float4x4();
		Float4x4(float a, float b, float c, float d,
			float e, float f, float g, float h,
			float i, float j, float k, float l,
			float m, float n, float o, float p);
		Float4x4& loadIdentity();
		Float4x4& operator=(const Float4x4& n)
		{
			data[0] = n.data[0]; data[1] = n.data[1]; data[2] = n.data[2]; data[3] = n.data[3];
			data[4] = n.data[4]; data[5] = n.data[5]; data[6] = n.data[6]; data[7] = n.data[7];
			data[8] = n.data[8]; data[9] = n.data[9]; data[10] = n.data[10]; data[11] = n.data[11];
			data[12] = n.data[12]; data[13] = n.data[13]; data[14] = n.data[14]; data[15] = n.data[15];
			return *this;
		}
		bool operator==(const Float4x4& n) const
		{
			return data[0] == n.data[0] && data[1] == n.data[1] && data[2] == n.data[2] && data[3] == n.data[3] &&
				data[4] == n.data[4] && data[5] == n.data[5] && data[6] == n.data[6] && data[7] == n.data[7] &&
				data[8] == n.data[8] && data[9] == n.data[9] && data[10] == n.data[10] && data[11] == n.data[11] &&
				data[12] == n.data[12] && data[13] == n.data[13] && data[14] == n.data[14] && data[15] == n.data[15];
		}
		Float4& operator[](uint64_t Index) { return rows[Index]; }
		Float4x4& transpose();
		Float4x4& operator*=(const Float4x4& m)
		{
			float a00 = data[0]; float a01 = data[1]; float a02 = data[2]; float a03 = data[3];
			float a10 = data[4]; float a11 = data[5]; float a12 = data[6]; float a13 = data[7];
			float a20 = data[8]; float a21 = data[9]; float a22 = data[10]; float a23 = data[11];
			float a30 = data[12]; float a31 = data[13]; float a32 = data[14]; float a33 = data[15];

			float b0 = m.data[0];
			float b1 = m.data[1];
			float b2 = m.data[2];
			float b3 = m.data[3];

			data[0] = b0 * a00 + b1 * a10 + b2 * a20 + b3 * a30;
			data[1] = b0 * a01 + b1 * a11 + b2 * a21 + b3 * a31;
			data[2] = b0 * a02 + b1 * a12 + b2 * a22 + b3 * a32;
			data[3] = b0 * a03 + b1 * a13 + b2 * a23 + b3 * a33;

			b0 = m.data[4];
			b1 = m.data[5];
			b2 = m.data[6];
			b3 = m.data[7];

			data[4] = b0 * a00 + b1 * a10 + b2 * a20 + b3 * a30;
			data[5] = b0 * a01 + b1 * a11 + b2 * a21 + b3 * a31;
			data[6] = b0 * a02 + b1 * a12 + b2 * a22 + b3 * a32;
			data[7] = b0 * a03 + b1 * a13 + b2 * a23 + b3 * a33;

			b0 = m.data[8];
			b1 = m.data[9];
			b2 = m.data[10];
			b3 = m.data[11];

			data[8] = b0 * a00 + b1 * a10 + b2 * a20 + b3 * a30;
			data[9] = b0 * a01 + b1 * a11 + b2 * a21 + b3 * a31;
			data[10] = b0 * a02 + b1 * a12 + b2 * a22 + b3 * a32;
			data[11] = b0 * a03 + b1 * a13 + b2 * a23 + b3 * a33;

			b0 = m.data[12];
			b1 = m.data[13];
			b2 = m.data[14];
			b3 = m.data[15];

			data[12] = b0 * a00 + b1 * a10 + b2 * a20 + b3 * a30;
			data[13] = b0 * a01 + b1 * a11 + b2 * a21 + b3 * a31;
			data[14] = b0 * a02 + b1 * a12 + b2 * a22 + b3 * a32;
			data[15] = b0 * a03 + b1 * a13 + b2 * a23 + b3 * a33;

			return *this;
		}
		Float4x4& translate(const Float3& v);
		Float4x4& scale(const Float3& v);
		Float4x4& rotate(const Float3& v);
		Float4x4& rotateX(float rad);
		Float4x4& rotateY(float rad);
		Float4x4& rotateZ(float rad);
		Float4x4& perspective(float fovy, float aspect, float nearBound, float farBound);
		Float4x4& orthographic(float left, float right, float bottom, float top, float nearBound, float farBound);
		Float4x4& lookAt(const Float3& eye, const Float3& center, const Float3& up);
		Float4x4& invert();
	};


	struct Quaternion
	{
		union
		{
			struct { float x, y, z, w; };
			float components[4];
		};

		Quaternion();
		Quaternion(float x);
		Quaternion(float x, float y, float z, float w);
		Quaternion& loadIdentity();
		Quaternion& fromEulerAngles(const Float3& eulerAngles);
		Float3 toEulerAngles();
		Quaternion& rotateX(float rad);
		Quaternion& rotateY(float rad);
		Quaternion& rotateZ(float rad);
		Float4x4 toFloat4x4();
		float& operator[](uint64_t Index) { return components[Index]; }
	};

	struct Matrix2D
	{
		Matrix2D();
		Matrix2D(const Matrix2D& Other);
		Matrix2D(float a, float b, float c, float d, float tx, float ty);
		Matrix2D& loadIdentity();
		Matrix2D& translate(float x, float y);
		Matrix2D& translate(Float2& TranslateVector);
		Matrix2D& scale(float x, float y);
		Matrix2D& scale(Float2& ScaleVector);
		Matrix2D& rotate(float Rotation);
		float& operator[](uint64_t Index) { return components[Index]; }
		Matrix2D& operator=(const Matrix2D& Other);


		union
		{
			float components[6];
			struct { float a, b, c, d, tx, ty; };
			struct { Float2 ab, cd, txty; };
		};
	};

}

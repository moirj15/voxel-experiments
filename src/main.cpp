#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <cassert>
#include <cstdint>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <optional>
#include <string>
#include <vector>
#include <wrl/client.h>
using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using s8 = int8_t;
using s16 = int16_t;
using s32 = int32_t;
using s64 = int64_t;
using f32 = float;
using f64 = double;

#define CHECK(X) assert(X == S_OK);

struct Window {
    u32 width;
    u32 height;
    SDL_Window *sdl_window = nullptr;

    explicit Window(u32 width, u32 height) :
        width(width), height(height), sdl_window(SDL_CreateWindow("win2", SDL_WINDOWPOS_UNDEFINED,
                                          SDL_WINDOWPOS_UNDEFINED, width, height, SDL_WINDOW_SHOWN))
    {
        assert(sdl_window != nullptr);
    }
    ~Window() { SDL_DestroyWindow(sdl_window); }
};

using namespace Microsoft::WRL;

static ComPtr<ID3D11Device> _device;
static ComPtr<ID3D11DeviceContext> _context;
static ComPtr<IDXGISwapChain> _swap_chain;
static ComPtr<ID3D11Texture2D> _back_buffer;
static ComPtr<ID3D11RenderTargetView> _back_buffer_render_target_view;
static ComPtr<ID3D11Texture2D> _depth_stencil_buffer;
static ComPtr<ID3D11DepthStencilView> _depth_stencil_view;
static ComPtr<ID3D11RasterizerState> _rasterizer_state;
static D3D11_VIEWPORT _viewport;

void InitGfx(const Window &window)
{
    u32 create_device_flags = 0;
#if defined(DEBUG) || defined(_DEBUG)
    create_device_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    D3D_FEATURE_LEVEL desired_level[] = {D3D_FEATURE_LEVEL_11_1};
    D3D_FEATURE_LEVEL feature_level;
    CHECK(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, 0, create_device_flags, desired_level, 1,
        D3D11_SDK_VERSION, &_device, &feature_level, &_context));
    assert(feature_level == D3D_FEATURE_LEVEL_11_1);

    SDL_SysWMinfo wm_info;
    SDL_VERSION(&wm_info.version);
    SDL_GetWindowWMInfo(window.sdl_window, &wm_info);
    HWND hwnd = wm_info.info.win.window;
    // clang-format off
        DXGI_SWAP_CHAIN_DESC swap_chain_desc = {
			.BufferDesc = {
				.Width = (u32)window.width,
				.Height = (u32)window.height,
				.RefreshRate = {
				  .Numerator = 60,
				  .Denominator = 1,
				},
				.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
				.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED,
				.Scaling = DXGI_MODE_SCALING_UNSPECIFIED,
			},
			// Multi sampling would be initialized here
			.SampleDesc = {
				.Count = 1,
				.Quality = 0,
			},
			.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
			.BufferCount = 1,
			.OutputWindow = hwnd,
			.Windowed = true,
			.SwapEffect = DXGI_SWAP_EFFECT_DISCARD,
			.Flags = 0,
	    };
    // clang-format on
    ComPtr<IDXGIDevice> dxgiDevice;
    CHECK(_device->QueryInterface(__uuidof(IDXGIDevice), &dxgiDevice));
    ComPtr<IDXGIAdapter> adapter;
    CHECK(dxgiDevice->GetParent(__uuidof(IDXGIAdapter), &adapter));
    ComPtr<IDXGIFactory> factory;
    CHECK(adapter->GetParent(__uuidof(IDXGIFactory), &factory));

    CHECK(factory->CreateSwapChain(_device.Get(), &swap_chain_desc, &_swap_chain));

    _swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), &_back_buffer);
    _device->CreateRenderTargetView(_back_buffer.Get(), 0, &_back_buffer_render_target_view);

    // clang-format off
		D3D11_TEXTURE2D_DESC depthStencilDesc = {
		    .Width = (u32)window.width,
		    .Height = (u32)window.height,
		    .MipLevels = 1,
		    .ArraySize = 1,
		    .Format = DXGI_FORMAT_D24_UNORM_S8_UINT,
		    // Multi sampling here
		    .SampleDesc = {
		  	  .Count = 1,
		  	  .Quality = 0,
		    },
		    .Usage = D3D11_USAGE_DEFAULT,
		    .BindFlags = D3D11_BIND_DEPTH_STENCIL,
		    .CPUAccessFlags = 0,
		    .MiscFlags = 0,
		};
    // clang-format on

    CHECK(_device->CreateTexture2D(&depthStencilDesc, 0, &_depth_stencil_buffer));
    CHECK(_device->CreateDepthStencilView(_depth_stencil_buffer.Get(), 0, &_depth_stencil_view));
    _context->OMSetRenderTargets(1, _back_buffer_render_target_view.GetAddressOf(), _depth_stencil_view.Get());

    // TODO: be careful about setting this to something else down the road. Currently not checking for changes
    _viewport = {
        .TopLeftX = 0.0f,
        .TopLeftY = 0.0f,
        .Width = static_cast<f32>(window.width),
        .Height = static_cast<f32>(window.height),
        .MinDepth = 0.0f,
        .MaxDepth = 1.0f,
    };

    _context->RSSetViewports(1, &_viewport);

    D3D11_RASTERIZER_DESC rasterizerDesc = {};
    rasterizerDesc = {
        .FillMode = D3D11_FILL_SOLID,
        .CullMode = D3D11_CULL_NONE,
        .FrontCounterClockwise = true,
    };
    _device->CreateRasterizerState(&rasterizerDesc, &_rasterizer_state);
    _context->RSSetState(_rasterizer_state.Get());
}

ID3D11Buffer *CreateBuffer(const u8 *initial_data, const u32 size, const D3D11_USAGE usage, const u32 bind_flag,
    const u32 cpu_access_flag, const u32 misc_flag, u32 structured_byte_stride)
{
    D3D11_BUFFER_DESC buffer_desc = {
        .ByteWidth = size,
        .Usage = usage,
        .BindFlags = bind_flag,
        .CPUAccessFlags = cpu_access_flag,
        .MiscFlags = misc_flag,
        .StructureByteStride = structured_byte_stride,
    };

    D3D11_SUBRESOURCE_DATA subresource_data = {};
    subresource_data = {.pSysMem = initial_data};

    ID3D11Buffer *buffer = nullptr;

    CHECK(_device->CreateBuffer(&buffer_desc, &subresource_data, &buffer));
    return buffer;
}

template<typename T>
ComPtr<ID3D11Buffer> CreateStaticVertexBuffer(const std::vector<T> &data, const u32 structured_byte_stride)
{
    return CreateBuffer((u8 *)data.data(), data.size() * sizeof(T), D3D11_USAGE_IMMUTABLE, D3D11_BIND_VERTEX_BUFFER, 0, 0,
        structured_byte_stride);
}

ComPtr<ID3D11Buffer> CreateStaticVertexBuffer(const u8 *initial_data, const u32 size, const u32 structured_byte_stride)
{
    return {
        CreateBuffer(initial_data, size, D3D11_USAGE_IMMUTABLE, D3D11_BIND_VERTEX_BUFFER, 0, 0, structured_byte_stride)};
}
template<typename T>
ComPtr<ID3D11Buffer> CreateStaticIndexBuffer(const std::vector<T> &data, const u32 structured_byte_stride)
{
    return CreateBuffer((u8 *)data.data(), data.size() * sizeof(T), D3D11_USAGE_DEFAULT, D3D11_BIND_INDEX_BUFFER, 0, 0,
        structured_byte_stride);
}

ComPtr<ID3D11Buffer> CreateStaticIndexBuffer(const u8 *initial_data, const u32 size, const u32 structured_byte_stride)
{
    return {
        CreateBuffer(initial_data, size, D3D11_USAGE_DEFAULT, D3D11_BIND_INDEX_BUFFER, 0, 0, structured_byte_stride)};
}

template<typename T>
ComPtr<ID3D11Buffer> CreateConstantBuffer(
    const std::vector<T> &data, const D3D11_USAGE usage, u32 structured_byte_stride)
{
    return CreateBuffer((u8 *)data.data(), data.size() * sizeof(T), D3D11_USAGE_DYNAMIC, D3D11_BIND_CONSTANT_BUFFER,
        D3D11_CPU_ACCESS_WRITE, 0, structured_byte_stride);
}

ComPtr<ID3D11Buffer> CreateConstantBuffer(const u8 *initial_data, const u32 size, const D3D11_USAGE usage,
    const D3D11_CPU_ACCESS_FLAG cpu_access_flag, const D3D11_RESOURCE_MISC_FLAG misc_flag, u32 structured_byte_stride)
{
    return {CreateBuffer(
        initial_data, size, usage, D3D11_BIND_CONSTANT_BUFFER, cpu_access_flag, misc_flag, structured_byte_stride)};
}

enum class FilePermissions {
    Read,
    Write,
    ReadWrite,
    BinaryRead,
    BinaryWrite,
    BinaryReadWrite,
};

inline FILE *OpenFile(const char *file, FilePermissions permissions)
{
    const char *cPermissions[] = {"r", "w", "w+", "rb", "wb", "wb+"};
    FILE *ret = NULL;
    ret = fopen(file, cPermissions[(uint32_t)permissions]);
    if (!ret) {
        // TODO: better error handling
        printf("FAILED TO OPEN FILE: %s\n", file);
        exit(EXIT_FAILURE);
    }
    return ret;
}

inline std::string ReadEntireFileAsString(const char *file)
{
    auto *fp = OpenFile(file, FilePermissions::Read);
    fseek(fp, 0, SEEK_END);
    uint64_t length = ftell(fp);
    rewind(fp);
    if (length == 0) {
        printf("Failed to read file size\n");
    }
    std::string data(length, 0);
    fread(data.data(), sizeof(uint8_t), length, fp);
    fclose(fp);
    return data;
}

ComPtr<ID3DBlob> CompileShader(const std::string &src, const char *shader_type)
{
    u32 compile_flags =  D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(DEBUG) || defined(_DEBUG)
    compile_flags |= D3DCOMPILE_DEBUG;
#endif
    ID3DBlob *compiled_blob = nullptr;
    ID3DBlob *error_blob = nullptr;
    auto result = D3DCompile(src.data(), src.size(), nullptr, nullptr, nullptr, "main", shader_type, compile_flags, 0,
        &compiled_blob, &error_blob);
    if (FAILED(result)) {
        printf("COMPILE ERR CODE: %x\n", result);
        if (error_blob)
            printf("COMPILE ERROR %s\n", (char *)error_blob->GetBufferPointer());
    }

    return compiled_blob;
}

std::pair<ComPtr<ID3D11VertexShader>, ComPtr<ID3DBlob>> CompileVertexShader(const std::string &filename)
{
    auto blob = CompileShader(filename, "vs_5_0");
    ID3D11VertexShader *vertex_shader = nullptr;
    CHECK(_device->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &vertex_shader));
    return {vertex_shader, blob};
}

ComPtr<ID3D11PixelShader> CompilePixelShader(const std::string &filename)
{
    auto blob = CompileShader(filename, "ps_5_0");
    ID3D11PixelShader *pixel_shader = nullptr;
    CHECK(_device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &pixel_shader));
    return pixel_shader;
}

#include <iostream>

int main(int argc, char **argv)
{
    Window window(1920, 1080);
    InitGfx(window);

    SDL_Event e;
    std::vector<f32> vers = {
        -1.0, -1.0, 0.0, 1.0, 
        1.0, -1.0, 0.0,1.0,
        0.0, 1.0, 0.0,1.0,
    };
    std::vector<u32> ind = {0, 1, 2};
    auto vb = CreateStaticVertexBuffer(vers, 0);
    auto ib = CreateStaticIndexBuffer(ind, 0);

    auto vert_source = ReadEntireFileAsString("shaders/VertexShader.hlsl");
    auto [vertex_shader, vertex_blob] = CompileVertexShader(vert_source);
    auto pixel_source = ReadEntireFileAsString("shaders/PixelShader.hlsl");
    auto pixel_shader = CompilePixelShader(pixel_source);

    ComPtr<ID3D11InputLayout> input_layout;
    D3D11_INPUT_ELEMENT_DESC input_element_desc[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0}};
    auto result = _device->CreateInputLayout(
        input_element_desc, 1, vertex_blob->GetBufferPointer(), vertex_blob->GetBufferSize(), &input_layout);
    assert(SUCCEEDED(result));

    _context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    _context->IASetInputLayout(input_layout.Get());
    u32 stride = 4 * sizeof(f32);
    u32 offset = 0;
    _context->IASetVertexBuffers(0, 1, vb.GetAddressOf(), &stride, &offset);
    _context->IASetIndexBuffer(ib.Get(), DXGI_FORMAT_R32_UINT, 0);
    _context->VSSetShader(vertex_shader.Get(), nullptr, 0);
    _context->PSSetShader(pixel_shader.Get(), nullptr, 0);

    while (true) {
        while (SDL_PollEvent(&e) > 0) {
            if (e.type == SDL_QUIT) {
                return 0;
            }
        }
        f32 clear_color[] = {0.0f, 0.0f, 0.0f, 1.0f};
        _context->ClearRenderTargetView(_back_buffer_render_target_view.Get(), clear_color);
        _context->ClearDepthStencilView(_depth_stencil_view.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
        _context->DrawIndexed(3, 0, 0);
        _swap_chain->Present(1, 0);
        
    }
    return 0;
}
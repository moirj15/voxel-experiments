module;
#include <SDL2\SDL.h>
#include <glm\glm.hpp>
#include <vector>
export module software_renderer;

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

export struct Voxel {
    glm::vec3 position = {0.0f, 0.0f, 0.0f};
    f32 size = 0;
    u32 color = 0;
};

namespace software
{

export class Renderer
{
    SDL_Window *sdl_window = nullptr;
    SDL_Texture *sdl_texture = nullptr;
    SDL_Renderer *sdl_renderer = nullptr;
    u32 width = 0;
    u32 height = 0;

    std::vector<u32> present_buffer;
    std::vector<u32> back_buffer;
    std::vector<f32> depth_buffer;

  public:
    Renderer(u32 window_width, u32 window_height) :
        width(window_width), height(window_height), present_buffer(width * height), back_buffer(width * height),
        depth_buffer(width * height)
    {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            fprintf(stderr, "Failed to initialize SDL2 for video: %s\n", SDL_GetError());
        }

        sdl_window = SDL_CreateWindow(
            "window", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, window_width, window_height, SDL_WINDOW_SHOWN);

        if (!sdl_window) {
            fprintf(stderr, "Failed to create SDL window: %s\n", SDL_GetError());
            exit(EXIT_FAILURE);
        }

        sdl_renderer = SDL_CreateRenderer(sdl_window, -1, 0);

        if (!sdl_renderer) {
            fprintf(stderr, "Failed to create SDL Renderer: %s\n", SDL_GetError());
            exit(EXIT_FAILURE);
        }

        sdl_texture = SDL_CreateTexture(
            sdl_renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STATIC, window_width, window_height);

        if (!sdl_texture) {
            fprintf(stderr, "Failed to create SDL Texture: %s\n", SDL_GetError());
            exit(EXIT_FAILURE);
        }
    }

    void ClearScreen(u32 color)
    {
        for (u32 i = 0; i < back_buffer.size(); i++) {
            back_buffer[i] = color;
        }
    }

    void DrawVoxel(const glm::mat4 &mvp)
    {
        // clang-format off
        const glm::mat4 screen_space((f32)width,        0.0f, 0.0f,  (f32)width, 
                                           0.0f, (f32)height, 0.0f, (f32)height,
                                           0.0f,        0.0f, 1.0f,        0.0f,
                                           0.0f,        0.0f, 0.0f,        1.0f);
        // clang-format on
        const auto mvps = screen_space * mvp;
        glm::vec4 voxel_normals[] = {
            mvps * glm::vec4{1.0f, 0.0f, 0.0f, 1.0f}, // right
            mvps * glm::vec4{-1.0f, 0.0f, 0.0f, 1.0f}, // left 
            mvps * glm::vec4{0.0f, 1.0f, 0.0f, 1.0f}, // top
            mvps * glm::vec4{0.0f, -1.0f, 0.0f, 1.0f}, // bottom
            mvps * glm::vec4{0.0f, 0.0f, 1.0f, 1.0f}, // back
            mvps * glm::vec4{0.0f, 0.0f, -1.0f, 1.0f}, // forward
        };
        // TODO: figure out w divide
        for (u32 i = 0; i < 6; i++) {
            voxel_normals[i] /= voxel_normals[i].w;
        }
        // TODO: derive voxel 8 corner points and use to fill in 2d

    }

    void SwapBuffers() { std::swap(present_buffer, back_buffer); }

    void Present()
    {
        SDL_UpdateTexture(sdl_texture, nullptr, present_buffer.data(), width * sizeof(uint32_t));

        SDL_RenderClear(sdl_renderer);
        SDL_RenderCopy(sdl_renderer, sdl_texture, nullptr, nullptr);
        SDL_RenderPresent(sdl_renderer);
    }
};

} // namespace software
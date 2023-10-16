//
// Created by ethan on 2/5/22.
//

#include "Application.hpp"
#include "Debug.hpp"

#include <core/lib/CMemory.hpp>

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include "rendering/backend/RendererInstance.hpp"

namespace hyperion {

SDL_Event *SystemEvent::GetInternalEvent() {
    return &(this->sdl_event);
}

ApplicationWindow::ApplicationWindow(ANSIString title, UInt width, UInt height)
    : m_title(std::move(title)),
      m_width(width),
      m_height(height)
{
}

SDLApplicationWindow::SDLApplicationWindow(ANSIString title, UInt width, UInt height)
    : ApplicationWindow(std::move(title), width, height),
      window(nullptr)
{
}

SDLApplicationWindow::~SDLApplicationWindow()
{
    SDL_DestroyWindow(this->window);
}

void SDLApplicationWindow::Initialize(WindowOptions window_options)
{
    UInt32 sdl_flags = 0;

    if (!(window_options.flags & WINDOW_FLAGS_NO_GFX)) {
#if HYP_VULKAN
        sdl_flags |= SDL_WINDOW_VULKAN;
#endif
    }

    if (window_options.enable_high_dpi) {
        sdl_flags |= SDL_WINDOW_ALLOW_HIGHDPI;
    }

    if (window_options.flags & WINDOW_FLAGS_HEADLESS) {
        sdl_flags |= SDL_WINDOW_HIDDEN;
    } else {
        sdl_flags |= SDL_WINDOW_SHOWN;

        // make sure to use SDL_free on file name strings for these events
        SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
    }

    this->window = SDL_CreateWindow(
        m_title.Data(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        Int(m_width), Int(m_height),
        sdl_flags
    );

    AssertThrowMsg(this->window != nullptr, "Failed to initialize window: %s", SDL_GetError());
}

#ifdef HYP_VULKAN
VkSurfaceKHR SDLApplicationWindow::CreateVkSurface(renderer::Instance *instance)
{
    VkSurfaceKHR surface;
    SDL_bool result = SDL_Vulkan_CreateSurface(window, instance->GetInstance(), &surface);

    AssertThrowMsg(result == SDL_TRUE, "Failed to create Vulkan surface: %s", SDL_GetError());

    return surface;
}
#endif

void SDLApplicationWindow::SetMousePosition(Int x, Int y)
{
    SDL_WarpMouseInWindow(window, x, y);
}

MouseState SDLApplicationWindow::GetMouseState()
{
    MouseState mouse_state { };
    mouse_state.mask = SDL_GetMouseState(&mouse_state.x, &mouse_state.y);

    return mouse_state;
}

Extent2D SDLApplicationWindow::GetExtent() const
{
    Int width, height;
    SDL_GetWindowSize(window, &width, &height);

    return Extent2D { UInt32(width), UInt32(height) };
}

void SDLApplicationWindow::SetMouseLocked(bool locked)
{
    SDL_SetRelativeMouseMode(locked ? SDL_TRUE : SDL_FALSE);
}

bool SDLApplicationWindow::HasMouseFocus() const
{
    const auto *focus_window = SDL_GetMouseFocus();

    return focus_window == window;
}

SDLApplication::SDLApplication(ANSIString name, int argc, char **argv)
    : Application(std::move(name), argc, argv)
{
    const Int sdl_init_result = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

    AssertThrowMsg(sdl_init_result == 0, "Failed to initialize SDL: %s", SDL_GetError());
}

SDLApplication::~SDLApplication()
{
    SDL_Quit();
}

UniquePtr<ApplicationWindow> SDLApplication::CreateSystemWindow(WindowOptions window_options)
{
    UniquePtr<SDLApplicationWindow> window;
    window.Reset(new SDLApplicationWindow(window_options.title.Data(), window_options.width, window_options.height));
    window->Initialize(window_options);

    return window;
}

Int SDLApplication::PollEvent(SystemEvent &event)
{
    const Int result = SDL_PollEvent(event.GetInternalEvent());

    if (result) {
        if (event.GetType() == SystemEventType::EVENT_FILE_DROP) {
            // set event data variant to the file path
            event.GetEventData().Set(FilePath(event.GetInternalEvent()->drop.file));

            // need to free or else the mem will leak
            SDL_free(event.GetInternalEvent()->drop.file);
            event.GetInternalEvent()->drop.file = nullptr;
        }
    }

    return result;
}

#ifdef HYP_VULKAN
bool SDLApplication::GetVkExtensions(Array<const char *> &out_extensions) const
{
    UInt32 num_extensions = 0;
    SDL_Window *window = static_cast<SDLApplicationWindow *>(m_current_window.Get())->GetInternalWindow();

    if (!SDL_Vulkan_GetInstanceExtensions(window, &num_extensions, nullptr)) {
        return false;
    }
    
    out_extensions.Resize(num_extensions);

    if (!SDL_Vulkan_GetInstanceExtensions(window, &num_extensions, out_extensions.Data())) {
        return false;
    }

    return true;
}
#endif

Application::Application(ANSIString name, int argc, char **argv)
    : m_arguments(argc, argv)
{
    if (name == nullptr) {
        name = "HyperionApp";
    }

    m_name = name;
}

Application::~Application() = default;

} // namespace hyperion
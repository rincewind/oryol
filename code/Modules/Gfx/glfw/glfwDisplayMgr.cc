//------------------------------------------------------------------------------
//  glfwDisplayMgr.cc
//------------------------------------------------------------------------------
#include "Pre.h"
#include "Core/Core.h"
#include "Gfx/gl/gl_impl.h"
#include "Gfx/gl/glInfo.h"
#include "Gfx/gl/glExt.h"
#include "Gfx/gl/glDebugOutput.h"
#include "Gfx/GfxProtocol.h"
#include "glfwDisplayMgr.h"
#include "Core/Log.h"
#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"

namespace Oryol {
namespace _priv {
    
glfwDisplayMgr* glfwDisplayMgr::self = nullptr;
GLFWwindow* glfwDisplayMgr::glfwWindow = nullptr;
    
//------------------------------------------------------------------------------
glfwDisplayMgr::glfwDisplayMgr() {
    o_assert(nullptr == self);
    o_assert(nullptr == glfwWindow);
    self = this;
}

//------------------------------------------------------------------------------
glfwDisplayMgr::~glfwDisplayMgr() {
    if (this->IsDisplayValid()) {
        this->DiscardDisplay();
    }
    o_assert(nullptr == glfwWindow);
    o_assert(nullptr != self);
    self = nullptr;
}

//------------------------------------------------------------------------------
GLFWwindow*
glfwDisplayMgr::getGlfwWindow() {
    return glfwWindow;
}

//------------------------------------------------------------------------------
void
glfwDisplayMgr::SetupDisplay(const GfxSetup& setup) {
    o_assert(!this->IsDisplayValid());
    o_assert_dbg(Core::IsMainThread());
    
    displayMgrBase::SetupDisplay(setup);
    
    // setup GLFW
    if (!glfwInit()) {
        o_error("glfwDisplayMgr: failed to setup GLFW!\n");
    }
    glfwSetErrorCallback(glfwErrorCallback);
    
    // setup the GLFW main window
    this->createMainWindow(setup);
    
    // and make the window's GL context current
    glfwMakeContextCurrent(glfwWindow);
    glfwSwapInterval(1);

    // setup extensions and platform-dependent constants
    ORYOL_GL_CHECK_ERROR();
    flextInit(glfwWindow);
    ORYOL_GL_CHECK_ERROR();
    glInfo::Setup();
    glExt::Setup();
    #if ORYOL_DEBUG
    glDebugOutput::Enable(glDebugOutput::Medium);
    #endif
    
    // now set the actual display attributes
    int fbWidth = 0, fbHeight = 0;
    int posX = 0, posY = 0;
    int width = 0, height = 0;
    glfwGetFramebufferSize(glfwWindow, &fbWidth, &fbHeight);
    glfwGetWindowPos(glfwWindow, &posX, &posY);
    glfwGetWindowSize(glfwWindow, &width, &height);
    this->displayAttrs.FramebufferWidth  = fbWidth;
    this->displayAttrs.FramebufferHeight = fbHeight;
    this->displayAttrs.WindowPosX        = posX;
    this->displayAttrs.WindowPosY        = posY;
    this->displayAttrs.WindowWidth       = width;
    this->displayAttrs.WindowHeight      = height;
    
    // set framebuffer size changed callback
    glfwSetFramebufferSizeCallback(glfwWindow, glwfFramebufferSizeChanged);
}

//------------------------------------------------------------------------------
void
glfwDisplayMgr::DiscardDisplay() {
    o_assert(this->IsDisplayValid());
    o_assert(nullptr != glfwWindow);
    o_assert_dbg(Core::IsMainThread());
    
    this->destroyMainWindow();
    glfwTerminate();
    glExt::Discard();
    glInfo::Discard();
    
    displayMgrBase::DiscardDisplay();
}

//------------------------------------------------------------------------------
bool
glfwDisplayMgr::QuitRequested() const {
    o_assert(nullptr != glfwWindow);
    o_assert_dbg(Core::IsMainThread());
    
    return glfwWindowShouldClose(glfwWindow) != 0;
}

//------------------------------------------------------------------------------
void
glfwDisplayMgr::ProcessSystemEvents() {
    o_assert(nullptr != glfwWindow);
    o_assert_dbg(Core::IsMainThread());
    
    glfwPollEvents();
    displayMgrBase::ProcessSystemEvents();
}

//------------------------------------------------------------------------------
void
glfwDisplayMgr::Present() {
    o_assert(nullptr != glfwWindow);
    o_assert_dbg(Core::IsMainThread());
    
    glfwSwapBuffers(glfwWindow);
    displayMgrBase::Present();
}

//------------------------------------------------------------------------------
void
glfwDisplayMgr::glBindDefaultFramebuffer() {
    o_assert_dbg(Core::IsMainThread());

    ::glBindFramebuffer(GL_FRAMEBUFFER, 0);
    ORYOL_GL_CHECK_ERROR();
}

//------------------------------------------------------------------------------
void
glfwDisplayMgr::glfwErrorCallback(int error, const char* desc) {
    Log::Error("GLFW error: '%d', '%s'\n", error, desc);
}

//------------------------------------------------------------------------------
void
glfwDisplayMgr::glwfFramebufferSizeChanged(GLFWwindow* win, int width, int height) {

    // update display attributes
    self->displayAttrs.FramebufferWidth = width;
    self->displayAttrs.FramebufferHeight = height;
    int winWidth, winHeight;
    glfwGetWindowSize(glfwWindow, &winWidth, &winHeight);
    self->displayAttrs.WindowWidth = winWidth;
    self->displayAttrs.WindowHeight = winHeight;
    
    // notify event handlers
    self->notifyEventHandlers(GfxProtocol::DisplayModified::Create());
}

//------------------------------------------------------------------------------
void
glfwDisplayMgr::createMainWindow(const GfxSetup& setup) {
    o_assert_dbg(nullptr == glfwDisplayMgr::glfwWindow);

    glfwWindowHint(GLFW_RED_BITS, PixelFormat::NumBits(setup.ColorFormat, PixelChannel::Red));
    glfwWindowHint(GLFW_GREEN_BITS, PixelFormat::NumBits(setup.ColorFormat, PixelChannel::Green));
    glfwWindowHint(GLFW_BLUE_BITS, PixelFormat::NumBits(setup.ColorFormat, PixelChannel::Blue));
    glfwWindowHint(GLFW_ALPHA_BITS, PixelFormat::NumBits(setup.ColorFormat, PixelChannel::Alpha));
    glfwWindowHint(GLFW_DEPTH_BITS, PixelFormat::NumBits(setup.DepthFormat, PixelChannel::Depth));
    glfwWindowHint(GLFW_STENCIL_BITS, PixelFormat::NumBits(setup.DepthFormat, PixelChannel::Stencil));
    glfwWindowHint(GLFW_SAMPLES, setup.Samples);
    #if ORYOL_DEBUG
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
    #endif
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // windowed or fullscreen mode?
    GLFWmonitor* glfwMonitor = nullptr;
    if (!setup.Windowed) {
        glfwMonitor = glfwGetPrimaryMonitor();
    }
    
    // now actually create the window
    glfwDisplayMgr::glfwWindow = glfwCreateWindow(setup.Width, setup.Height, setup.Title.AsCStr(), glfwMonitor, 0);
    o_assert(nullptr != glfwDisplayMgr::glfwWindow);
}

//------------------------------------------------------------------------------
void
glfwDisplayMgr::destroyMainWindow() {
    o_assert_dbg(nullptr != glfwDisplayMgr::glfwWindow);
    glfwDestroyWindow(glfwWindow);
    glfwWindow = nullptr;
}

} // namespace _priv
} // namespace Oryol
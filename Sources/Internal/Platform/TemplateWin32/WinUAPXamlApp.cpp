﻿/*==================================================================================
    Copyright (c) 2008, binaryzebra
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    * Neither the name of the binaryzebra nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE binaryzebra AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL binaryzebra BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
=====================================================================================*/

#include "Base/Platform.h"

#if defined(__DAVAENGINE_WIN_UAP__)

#include "Core/Core.h"
#include "Render/Renderer.h"
#include "Render/2D/Systems/VirtualCoordinatesSystem.h"
#include "Render/2D/Systems/RenderSystem2D.h"
#include "UI/UIScreenManager.h"

#include "Platform/SystemTimer.h"
#include "Platform/TemplateWin32/CorePlatformWinUAP.h"
#include "Platform/TemplateWin32/WinUAPXamlApp.h"
#include "Platform/TemplateWin32/DispatcherWinUAP.h"
#include "Platform/DeviceInfo.h"

#include "Logger/Logger.h"

#include "Utils/Utils.h"
#include "Input/InputSystem.h"
#include "Input/KeyboardDevice.h"

#include "WinUAPXamlApp.h"
#include "WinApiUAP.h"
#include "WinSystemTimer.h"

extern void FrameworkDidLaunched();
extern void FrameworkWillTerminate();

using namespace ::Windows::System;
using namespace ::Windows::Foundation;
using namespace ::Windows::UI::Core;
using namespace ::Windows::UI::Xaml;
using namespace ::Windows::UI::Xaml::Input;
using namespace ::Windows::UI::Xaml::Controls;
using namespace ::Windows::UI::Input;
using namespace ::Windows::UI::ViewManagement;
using namespace ::Windows::Devices::Input;
using namespace ::Windows::ApplicationModel;
using namespace ::Windows::Graphics::Display;
using namespace ::Windows::ApplicationModel::Core;
using namespace ::Windows::UI::Xaml::Media;
using namespace ::Windows::System::Threading;
using namespace ::Windows::Phone::UI::Input;
using namespace ::Windows::UI::Xaml::Markup;

namespace DAVA
{
namespace
{
UIEvent::Device ToDavaDeviceId(PointerDeviceType type)
{
    switch (type)
    {
    case PointerDeviceType::Mouse:
        return UIEvent::Device::MOUSE;
    case PointerDeviceType::Pen:
        return UIEvent::Device::PEN;
    case PointerDeviceType::Touch:
        return UIEvent::Device::TOUCH_SURFACE;
    default:
        DVASSERT(false && "can't be!");
        return UIEvent::Device::UNKNOWN;
    }
}
} // anonymous namespace

WinUAPXamlApp::WinUAPXamlApp()
    : core(static_cast<CorePlatformWinUAP*>(Core::Instance()))
    , isPhoneApiDetected(DeviceInfo::ePlatform::PLATFORM_PHONE_WIN_UAP == DeviceInfo::GetPlatform())
{
    displayRequest = ref new Windows::System::Display::DisplayRequest;
    AllowDisplaySleep(false);

    if (!isPhoneApiDetected)
    {
        WinApiUAP::Initialize();
    }
}

WinUAPXamlApp::~WinUAPXamlApp()
{
    SafeRelease(mainLoopThread);
    AllowDisplaySleep(true);
}

DisplayOrientations WinUAPXamlApp::GetDisplayOrientation()
{
    return displayOrientation;
}

ApplicationViewWindowingMode WinUAPXamlApp::GetScreenMode()
{
    return isFullscreen ? ApplicationViewWindowingMode::FullScreen :
                          ApplicationViewWindowingMode::PreferredLaunchViewSize;
}

void WinUAPXamlApp::SetScreenMode(ApplicationViewWindowingMode screenMode)
{
    // Note: must run on UI thread
    bool fullscreen = ApplicationViewWindowingMode::FullScreen == screenMode;
    if (!isPhoneApiDetected)
    {
        SetFullScreen(fullscreen);
    }
}

bool WinUAPXamlApp::SetMouseCaptureMode(InputSystem::eMouseCaptureMode newMode)
{
    // should be started on UI thread
    if (mouseCaptureMode != newMode)
    {
        // Setup new capture mode
        switch (newMode)
        {
        case DAVA::InputSystem::eMouseCaptureMode::OFF:
            // Nothing to setup on platform
            mouseCaptureMode = newMode;
            break;
        case DAVA::InputSystem::eMouseCaptureMode::FRAME:
            // Mode unsupported yet
            Logger::Error("Unsupported cursor capture mode");
            break;
        case DAVA::InputSystem::eMouseCaptureMode::PINING:
            mouseCaptureMode = newMode;
            break;
        default:
            DVASSERT("Incorrect cursor capture mode");
            Logger::Error("Incorrect cursor capture mode");
            break;
        }
    }

    return mouseCaptureMode == newMode;
}

bool WinUAPXamlApp::SetCursorVisible(bool isVisible)
{
    // should be started on UI thread
    if (isVisible != isMouseCursorShown)
    {
        if (isVisible)
        {
            MouseDevice::GetForCurrentView()->MouseMoved -= token;
            Window::Current->CoreWindow->PointerCursor = ref new CoreCursor(CoreCursorType::Arrow, 0);
        }
        else
        {
            token = MouseDevice::GetForCurrentView()->MouseMoved += ref new TypedEventHandler<MouseDevice ^, MouseEventArgs ^>(this, &WinUAPXamlApp::OnMouseMoved);
            Window::Current->CoreWindow->PointerCursor = nullptr;
        }
        isMouseCursorShown = isVisible;
    }
    return true;
}

void WinUAPXamlApp::StartMainLoopThread(::Windows::ApplicationModel::Activation::LaunchActivatedEventArgs ^ args)
{
    PreStartAppSettings();
    uiThreadDispatcher = Window::Current->CoreWindow->Dispatcher;

    CreateBaseXamlUI();

    // Prepare ScreenInfo before starting dava thread
    DeviceInfo::ScreenInfo screenInfo = ObtainScreenInfo();
    DeviceInfo::InitializeScreenInfo(screenInfo, false);

    mainLoopThread = Thread::Create([this, args]() { Run(args); });
    mainLoopThread->Start();
    mainLoopThread->BindToProcessor(0);
    mainLoopThread->SetPriority(Thread::PRIORITY_HIGH);
}

void WinUAPXamlApp::PreStartAppSettings()
{
    if (isPhoneApiDetected)
    {
        // default orientation landscape and landscape flipped
        // will be changed in SetDisplayOrientations()
        StatusBar::GetForCurrentView()->HideAsync();
    }
    Windows::UI::ViewManagement::ApplicationView::GetForCurrentView()->FullScreenSystemOverlayMode = FullScreenSystemOverlayMode::Minimal;
    Window::Current->CoreWindow->Activated += ref new TypedEventHandler<CoreWindow ^, WindowActivatedEventArgs ^>(this, &WinUAPXamlApp::OnWindowActivationChanged);
}

void WinUAPXamlApp::OnLaunched(::Windows::ApplicationModel::Activation::LaunchActivatedEventArgs ^ args)
{
    // If mainLoopThread is null then app performing cold start
    // else app is restored from background or resumed from suspended state
    if (mainLoopThread == nullptr)
    {
        StartMainLoopThread(args);
    }
    else
    {
        EmitPushNotification(args);
    }

    Window::Current->Activate();
}

void WinUAPXamlApp::OnActivated(::Windows::ApplicationModel::Activation::IActivatedEventArgs ^ args)
{
    using ::Windows::ApplicationModel::Activation::ActivationKind;

    if (args->Kind == ActivationKind::Protocol)
    {
        if (mainLoopThread == nullptr)
        {
            StartMainLoopThread(nullptr);
        }

        Window::Current->Activate();
    }
    else
    {
        Application::OnActivated(args);
    }
}

void WinUAPXamlApp::AddUIElement(Windows::UI::Xaml::UIElement ^ uiElement)
{
    // Note: must be called from UI thread
    canvas->Children->Append(uiElement);
}

void WinUAPXamlApp::RemoveUIElement(Windows::UI::Xaml::UIElement ^ uiElement)
{
    // Note: must be called from UI thread
    unsigned int index = 0;
    for (auto x = canvas->Children->First(); x->HasCurrent; x->MoveNext(), ++index)
    {
        if (x->Current == uiElement)
        {
            canvas->Children->RemoveAt(index);
            break;
        }
    }
}

void WinUAPXamlApp::PositionUIElement(Windows::UI::Xaml::UIElement ^ uiElement, float32 x, float32 y)
{
    // Note: must be called from UI thread
    canvas->SetLeft(uiElement, x);
    canvas->SetTop(uiElement, y);
}

void WinUAPXamlApp::SetTextBoxCustomStyle(Windows::UI::Xaml::Controls::TextBox ^ textBox)
{
    textBox->Style = customTextBoxStyle;
}

void WinUAPXamlApp::SetPasswordBoxCustomStyle(Windows::UI::Xaml::Controls::PasswordBox ^ passwordBox)
{
    passwordBox->Style = customPasswordBoxStyle;
}

void WinUAPXamlApp::UnfocusUIElement()
{
    // XAML controls cannot be unfocused programmatically, this is especially useful for text fields
    // So use dummy offscreen control that steals focus
    controlThatTakesFocus->Focus(FocusState::Pointer);
}

void WinUAPXamlApp::CaptureTextBox(Windows::UI::Xaml::Controls::Control ^ control)
{
    if (pressedEventArgs && control->CapturePointer(pressedEventArgs->Pointer))
    {
        OnSwapChainPanelPointerReleased(this, pressedEventArgs); // send pointer release event because we will'not receive this event after capturing
    }
}

void WinUAPXamlApp::Run(::Windows::ApplicationModel::Activation::LaunchActivatedEventArgs ^ args)
{
    dispatcher = std::make_unique<DispatcherWinUAP>();
    Core::Instance()->CreateSingletons();
    // View size and orientation option should be configured in FrameworkDidLaunched
    FrameworkDidLaunched();

    float32 width = 0.f;
    float32 height = 0.f;
    float32 scaleX = 0.f;
    float32 scaleY = 0.f;

    core->RunOnUIThreadBlocked([this, &width, &height, &scaleX, &scaleY]() {
        SetupEventHandlers();
        PrepareScreenSize();
        SetTitleName();
        SetDisplayOrientations();
        LoadWindowMinimumSizeSettings();

        width = static_cast<float32>(swapChainPanel->ActualWidth);
        height = static_cast<float32>(swapChainPanel->ActualHeight);
        scaleX = swapChainPanel->CompositionScaleX;
        scaleY = swapChainPanel->CompositionScaleY;
    });

    core->InitWindowSize(reinterpret_cast<void*>(swapChainPanel), width, height, scaleX, scaleY);

    Core::Instance()->SetIsActive(true);
    Core::Instance()->SystemAppStarted();

    if (args != nullptr)
    {
        EmitPushNotification(args);
    }

    SystemTimer* sysTimer = SystemTimer::Instance();
    while (!quitFlag)
    {
        dispatcher->ProcessTasks();

        //  Control FPS
        {
            static uint64 startTime = sysTimer->AbsoluteMS();

            uint64 elapsedTime = sysTimer->AbsoluteMS() - startTime;
            int32 fpsLimit = Renderer::GetDesiredFPS();
            if (fpsLimit > 0)
            {
                uint64 averageFrameTime = 1000UL / static_cast<uint64>(fpsLimit);
                if (averageFrameTime > elapsedTime)
                {
                    uint64 sleepMs = averageFrameTime - elapsedTime;
                    Thread::Sleep(static_cast<uint32>(sleepMs));
                }
            }
            startTime = sysTimer->AbsoluteMS();
        }

        Core::Instance()->SystemProcessFrame();
    }

    ApplicationCore* appCore = Core::Instance()->GetApplicationCore();
    if (appCore != nullptr && appCore->OnQuit())
    {
        Application::Current->Exit();
    }

    Core::Instance()->SystemAppFinished();
    FrameworkWillTerminate();
    Core::Instance()->ReleaseSingletons();

    Application::Current->Exit();
}

void WinUAPXamlApp::OnSuspending(::Platform::Object ^ sender, Windows::ApplicationModel::SuspendingEventArgs ^ args)
{
    core->RunOnMainThreadBlocked([]() {
        // unpress all pressed keys
        InputSystem::Instance()->GetKeyboard().ClearAllKeys();
        Core::Instance()->GetApplicationCore()->OnSuspend();
        rhi::SuspendRendering();
    });
}

void WinUAPXamlApp::OnResuming(::Platform::Object ^ sender, ::Platform::Object ^ args)
{
    core->RunOnMainThreadBlocked([]() {
        rhi::ResumeRendering();
        Core::Instance()->GetApplicationCore()->OnResume();
    });
}

void WinUAPXamlApp::OnWindowActivationChanged(::Windows::UI::Core::CoreWindow ^ sender, ::Windows::UI::Core::WindowActivatedEventArgs ^ args)
{
    CoreWindowActivationState state = args->WindowActivationState;

    core->RunOnMainThread([this, state] {
        switch (state)
        {
        case CoreWindowActivationState::CodeActivated:
        case CoreWindowActivationState::PointerActivated:
            isPhoneApiDetected ? Core::Instance()->SetIsActive(true) : Core::Instance()->FocusReceived();

            //We need to activate high-resolution timer
            //cause default system timer resolution is ~15ms and our frame-time calculation is very inaccurate
            EnableHighResolutionTimer(true);
            break;
        case CoreWindowActivationState::Deactivated:
            isPhoneApiDetected ? Core::Instance()->SetIsActive(false) : Core::Instance()->FocusLost();
            InputSystem::Instance()->GetKeyboard().ClearAllKeys();
            EnableHighResolutionTimer(false);
            break;
        default:
            break;
        }
    });
}

void WinUAPXamlApp::OnWindowVisibilityChanged(::Windows::UI::Core::CoreWindow ^ sender, ::Windows::UI::Core::VisibilityChangedEventArgs ^ args)
{
    bool visible = args->Visible;
    AllowDisplaySleep(!visible);
    core->RunOnMainThread([this, visible]() {
        if (visible)
        {
            if (!isPhoneApiDetected)
            {
                Core::Instance()->GoForeground();
                //Core::Instance()->FocusRecieve();
            }
            Core::Instance()->SetIsActive(true); //TODO: Maybe should move to client side
        }
        else
        {
            if (!isPhoneApiDetected)
            {
                //Core::Instance()->FocusLost();
                Core::Instance()->GoBackground(false);
            }
            else
            {
                Core::Instance()->SetIsActive(false); //TODO: Maybe should move to client side
            }
            InputSystem::Instance()->GetKeyboard().ClearAllKeys();
        }
    });
}

void WinUAPXamlApp::OnCoreWindowSizeChanged(CoreWindow ^ coreWindow, WindowSizeChangedEventArgs ^ arg)
{
    float32 coreWindowWidth = arg->Size.Width;
    float32 coreWindowHeight = arg->Size.Height;

    bool trackMinSize = minWindowWidth > 0.0f && minWindowHeight > 0.0f;
    if (!isPhoneApiDetected && trackMinSize && (coreWindowWidth < minWindowWidth || coreWindowHeight < minWindowHeight))
    {
        float32 w = std::max(coreWindowWidth, minWindowWidth);
        float32 h = std::max(coreWindowHeight, minWindowHeight);
        Windows::Foundation::Size size(w, h);
        auto currentView = Windows::UI::ViewManagement::ApplicationView::GetForCurrentView();

        if (!currentView->TryResizeView(size))
        {
            Windows::Foundation::Size defaultSize(minWindowWidth, minWindowHeight);
            if (!currentView->TryResizeView(defaultSize))
            {
                Logger::FrameworkDebug("[WinUAPXamlApp::OnCoreWindowSizeChanged]: Failed to resize window to minimum size");
            }
        }
    }
}

void WinUAPXamlApp::OnSwapChainPanelSizeChanged(Platform::Object ^ sender, ::Windows::UI::Xaml::SizeChangedEventArgs ^ arg)
{
    float32 width = static_cast<float32>(arg->NewSize.Width);
    float32 height = static_cast<float32>(arg->NewSize.Height);
    float32 scaleX = swapChainPanel->CompositionScaleX;
    float32 scaleY = swapChainPanel->CompositionScaleY;

    DeviceInfo::ScreenInfo screenInfo = ObtainScreenInfo();
    core->RunOnMainThread([this, width, height, scaleX, scaleY, screenInfo]() {
        DeviceInfo::InitializeScreenInfo(screenInfo, true);
        Core::Instance()->WindowSizeChanged(width, height, scaleX, scaleY);
    });
}

void WinUAPXamlApp::OnSwapChainPanelCompositionScaleChanged(SwapChainPanel ^ panel, Platform::Object ^ args)
{
    float32 width = static_cast<float32>(swapChainPanel->ActualWidth);
    float32 height = static_cast<float32>(swapChainPanel->ActualHeight);
    float32 scaleX = swapChainPanel->CompositionScaleX;
    float32 scaleY = swapChainPanel->CompositionScaleY;

    DeviceInfo::ScreenInfo screenInfo = ObtainScreenInfo();
    core->RunOnMainThread([this, width, height, scaleX, scaleY, screenInfo]() {
        DeviceInfo::InitializeScreenInfo(screenInfo, true);
        Core::Instance()->WindowSizeChanged(width, height, scaleX, scaleY);
    });
}

void WinUAPXamlApp::UpdateMouseButtonsState(Windows::UI::Input::PointerPointProperties ^ pointProperties, Vector<MouseButtonChange>& out)
{
    out.clear();

    if (GetMouseButtonState(UIEvent::MouseButton::LEFT) != pointProperties->IsLeftButtonPressed)
    {
        MouseButtonChange change;
        change.button = UIEvent::MouseButton::LEFT;
        change.beginOrEnd = pointProperties->IsLeftButtonPressed ? UIEvent::Phase::BEGAN : UIEvent::Phase::ENDED;
        out.push_back(change);
    }

    if (GetMouseButtonState(UIEvent::MouseButton::RIGHT) != pointProperties->IsRightButtonPressed)
    {
        MouseButtonChange change;
        change.button = UIEvent::MouseButton::RIGHT;
        change.beginOrEnd = pointProperties->IsRightButtonPressed ? UIEvent::Phase::BEGAN : UIEvent::Phase::ENDED;
        out.push_back(change);
    }

    if (GetMouseButtonState(UIEvent::MouseButton::MIDDLE) != pointProperties->IsMiddleButtonPressed)
    {
        MouseButtonChange change;
        change.button = UIEvent::MouseButton::MIDDLE;
        change.beginOrEnd = pointProperties->IsMiddleButtonPressed ? UIEvent::Phase::BEGAN : UIEvent::Phase::ENDED;
        out.push_back(change);
    }

    if (GetMouseButtonState(UIEvent::MouseButton::EXTENDED1) != pointProperties->IsXButton1Pressed)
    {
        MouseButtonChange change;
        change.button = UIEvent::MouseButton::EXTENDED1;
        change.beginOrEnd = pointProperties->IsXButton1Pressed ? UIEvent::Phase::BEGAN : UIEvent::Phase::ENDED;
        out.push_back(change);
    }

    if (GetMouseButtonState(UIEvent::MouseButton::EXTENDED2) != pointProperties->IsXButton2Pressed)
    {
        MouseButtonChange change;
        change.button = UIEvent::MouseButton::EXTENDED2;
        change.beginOrEnd = pointProperties->IsXButton2Pressed ? UIEvent::Phase::BEGAN : UIEvent::Phase::ENDED;
        out.push_back(change);
    }

    for (auto& change : out)
    {
        SetMouseButtonState(change.button, change.beginOrEnd == UIEvent::Phase::BEGAN);
    }
}

void WinUAPXamlApp::OnSwapChainPanelPointerPressed(Platform::Object ^, PointerRoutedEventArgs ^ args)
{
    pressedEventArgs = args;

    PointerPoint ^ pointerPoint = args->GetCurrentPoint(nullptr);
    float32 x = pointerPoint->Position.X;
    float32 y = pointerPoint->Position.Y;
    int32 pointerOrButtonIndex = pointerPoint->PointerId;
    PointerDeviceType type = pointerPoint->PointerDevice->PointerDeviceType;

    if ((PointerDeviceType::Mouse == type) || (PointerDeviceType::Pen == type))
    {
        UpdateMouseButtonsState(pointerPoint->Properties, mouseButtonChanges);
        for (auto& change : mouseButtonChanges)
        {
            auto fn = [this, x, y, change, type]() {
                DAVATouchEvent(change.beginOrEnd, x, y, static_cast<int32>(change.button), ToDavaDeviceId(type));
            };
            core->RunOnMainThread(fn);
        }
    }
    else
    {
        auto fn = [this, x, y, pointerOrButtonIndex, type]() {
            DAVATouchEvent(UIEvent::Phase::BEGAN, x, y, pointerOrButtonIndex, ToDavaDeviceId(type));
        };
        core->RunOnMainThread(fn);
    }
}

void WinUAPXamlApp::OnSwapChainPanelPointerReleased(Platform::Object ^ /*sender*/, PointerRoutedEventArgs ^ args)
{
    PointerPoint ^ pointerPoint = args->GetCurrentPoint(nullptr);
    float32 x = pointerPoint->Position.X;
    float32 y = pointerPoint->Position.Y;
    int32 pointerOrButtonIndex = pointerPoint->PointerId;
    PointerDeviceType type = pointerPoint->PointerDevice->PointerDeviceType;

    if ((PointerDeviceType::Mouse == type) || (PointerDeviceType::Pen == type))
    {
        UpdateMouseButtonsState(pointerPoint->Properties, mouseButtonChanges);
        for (auto& change : mouseButtonChanges)
        {
            auto fn = [this, x, y, change, type]() {
                DAVATouchEvent(change.beginOrEnd, x, y, static_cast<int32>(change.button), ToDavaDeviceId(type));
            };
            core->RunOnMainThread(fn);
        }
    }
    else
    {
        auto fn = [this, x, y, pointerOrButtonIndex, type]() {
            DAVATouchEvent(UIEvent::Phase::ENDED, x, y, pointerOrButtonIndex, ToDavaDeviceId(type));
        };

        core->RunOnMainThread(fn);
    }
}

void WinUAPXamlApp::OnSwapChainPanelPointerMoved(Platform::Object ^ /*sender*/, PointerRoutedEventArgs ^ args)
{
    pressedEventArgs = args;

    UIEvent::Phase phase = UIEvent::Phase::DRAG;
    PointerPoint ^ pointerPoint = args->GetCurrentPoint(nullptr);
    PointerDeviceType type = pointerPoint->PointerDevice->PointerDeviceType;
    int32 pointerOrButtonIndex = pointerPoint->PointerId;

    float32 x = pointerPoint->Position.X;
    float32 y = pointerPoint->Position.Y;

    if ((PointerDeviceType::Mouse == type) || (PointerDeviceType::Pen == type))
    {
        UpdateMouseButtonsState(pointerPoint->Properties, mouseButtonChanges);

        for (auto& change : mouseButtonChanges)
        {
            auto fn = [this, x, y, change, type]() {
                DAVATouchEvent(change.beginOrEnd, x, y, static_cast<int32>(change.button), ToDavaDeviceId(type));
            };
            core->RunOnMainThread(fn);
        }

        if (mouseCaptureMode != InputSystem::eMouseCaptureMode::PINING)
        {
            if (mouseButtonsState.none())
            {
                phase = UIEvent::Phase::MOVE;

                core->RunOnMainThread([this, phase, x, y, type]() {
                    DAVATouchEvent(phase, x, y, static_cast<int32>(UIEvent::MouseButton::NONE), ToDavaDeviceId(type));
                });
            }
            else
            {
                SendPressedMouseButtons(x, y, ToDavaDeviceId(type));
            }
        }
    }
    else
    {
        core->RunOnMainThread([this, phase, x, y, pointerOrButtonIndex, type]() {
            DAVATouchEvent(phase, x, y, pointerOrButtonIndex, ToDavaDeviceId(type));
        });
    }
}

void WinUAPXamlApp::OnSwapChainPanelPointerWheel(Platform::Object ^ /*sender*/, PointerRoutedEventArgs ^ args)
{
    PointerPoint ^ pointerPoint = args->GetCurrentPoint(nullptr);
    int32 wheelDelta = pointerPoint->Properties->MouseWheelDelta;
    PointerDeviceType type = pointerPoint->PointerDevice->PointerDeviceType;
    Vector2 physPoint(pointerPoint->Position.X, pointerPoint->Position.Y);

    core->RunOnMainThread([this, wheelDelta, physPoint, type]() {
        UIEvent ev;
        auto delta = wheelDelta / static_cast<float32>(WHEEL_DELTA);
        KeyboardDevice& keybDev = InputSystem::Instance()->GetKeyboard();
        if (keybDev.IsKeyPressed(Key::LSHIFT) || keybDev.IsKeyPressed(Key::RSHIFT))
        {
            ev.wheelDelta = { delta, 0 };
        }
        else
        {
            ev.wheelDelta = { 0, delta };
        }
        ev.phase = UIEvent::Phase::WHEEL;
        ev.device = ToDavaDeviceId(type);
        ev.physPoint = physPoint;
        ev.timestamp = (SystemTimer::FrameStampTimeMS() / 1000.0);
        UIControlSystem::Instance()->OnInput(&ev);
    });
}

void WinUAPXamlApp::OnHardwareBackButtonPressed(Platform::Object ^ /*sender*/, BackPressedEventArgs ^ args)
{
    SendBackKeyEvents();
    args->Handled = true;
}

void WinUAPXamlApp::OnBackRequested(Platform::Object ^ /*sender*/, BackRequestedEventArgs ^ args)
{
    SendBackKeyEvents();
    args->Handled = true;
}

void WinUAPXamlApp::OnAcceleratorKeyActivated(Windows::UI::Core::CoreDispatcher ^ sender, Windows::UI::Core::AcceleratorKeyEventArgs ^ keyEventArgs)
{
    uint32 key = static_cast<uint32>(keyEventArgs->VirtualKey);

    if (key == VK_SHIFT && keyEventArgs->KeyStatus.ScanCode == 0x36) // right shift scan code(on windows)
    {
        key |= 0x100;
    }
    if (keyEventArgs->KeyStatus.IsExtendedKey)
    {
        key |= 0x100;
    }

    UIEvent::Phase phase;
    switch (keyEventArgs->EventType)
    {
    case CoreAcceleratorKeyEventType::KeyDown:
    case CoreAcceleratorKeyEventType::SystemKeyDown:
        phase = keyEventArgs->KeyStatus.WasKeyDown ? UIEvent::Phase::KEY_DOWN_REPEAT : UIEvent::Phase::KEY_DOWN;
        break;

    case CoreAcceleratorKeyEventType::KeyUp:
    case CoreAcceleratorKeyEventType::SystemKeyUp:
        phase = UIEvent::Phase::KEY_UP;
        break;

    default:
        return;
    }

    core->RunOnMainThread([this, key, phase]() {
        auto& keyboard = InputSystem::Instance()->GetKeyboard();

        UIEvent uiEvent;
        uiEvent.device = UIEvent::Device::KEYBOARD;
        uiEvent.phase = phase;
        uiEvent.key = keyboard.GetDavaKeyForSystemKey(key);
        uiEvent.timestamp = (SystemTimer::FrameStampTimeMS() / 1000.0);
        UIControlSystem::Instance()->OnInput(&uiEvent);

        switch (uiEvent.phase)
        {
        case UIEvent::Phase::KEY_DOWN:
        case UIEvent::Phase::KEY_DOWN_REPEAT:
            keyboard.OnKeyPressed(uiEvent.key);
            break;

        case UIEvent::Phase::KEY_UP:
            keyboard.OnKeyUnpressed(uiEvent.key);
            break;
        }
    });
}

void WinUAPXamlApp::OnChar(Windows::UI::Core::CoreWindow ^ sender, Windows::UI::Core::CharacterReceivedEventArgs ^ args)
{
    uint32 unicodeChar = args->KeyCode;
    bool isRepeat = args->KeyStatus.WasKeyDown;
    core->RunOnMainThread([this, unicodeChar, isRepeat]() {
        UIEvent ev;
        DVASSERT(unicodeChar < 0xFFFF); // wchar_t is 16 bit, so keyChar dosnt fit
        ev.keyChar = unicodeChar;
        ev.device = UIEvent::Device::KEYBOARD;
        ev.timestamp = (SystemTimer::FrameStampTimeMS() / 1000.0);

        if (isRepeat)
        {
            ev.phase = UIEvent::Phase::CHAR_REPEAT;
        }
        else
        {
            ev.phase = UIEvent::Phase::CHAR;
        }
        UIControlSystem::Instance()->OnInput(&ev);
    });
}

void WinUAPXamlApp::SendPressedMouseButtons(float32 x, float32 y, UIEvent::Device device)
{
    auto SendDragOnButtonChange = [this, x, y, device](UIEvent::MouseButton button) {
        if (GetMouseButtonState(button))
        {
            core->RunOnMainThread([this, x, y, button, device]() {
                DAVATouchEvent(UIEvent::Phase::DRAG, x, y, static_cast<int32>(button), device);
            });
        }
    };

    SendDragOnButtonChange(UIEvent::MouseButton::LEFT);
    SendDragOnButtonChange(UIEvent::MouseButton::RIGHT);
    SendDragOnButtonChange(UIEvent::MouseButton::MIDDLE);
    SendDragOnButtonChange(UIEvent::MouseButton::EXTENDED1);
    SendDragOnButtonChange(UIEvent::MouseButton::EXTENDED2);
}

void WinUAPXamlApp::OnMouseMoved(MouseDevice ^ mouseDevice, MouseEventArgs ^ args)
{
    UIEvent::Phase phase = UIEvent::Phase::MOVE;

    PointerPoint ^ pointerPoint = nullptr;
    try
    {
        pointerPoint = Windows::UI::Input::PointerPoint::GetCurrentPoint(1);
    }
    catch (Platform::Exception ^ e)
    {
        Logger::FrameworkDebug("Exception in WinUAPXamlApp::OnMouseMoved: 0x%08X - %s", e->HResult, RTStringToString(e->Message).c_str());
    }

    if (pointerPoint != nullptr)
    {
        UpdateMouseButtonsState(pointerPoint->Properties, mouseButtonChanges);

        float window_x = pointerPoint->Position.X;
        float window_y = pointerPoint->Position.Y;
        for (auto& change : mouseButtonChanges)
        {
            auto fn = [this, window_x, window_y, change]() {
                DAVATouchEvent(change.beginOrEnd, window_x, window_y, static_cast<int32>(change.button), UIEvent::Device::MOUSE);
            };
            core->RunOnMainThread(fn);
        }

        float32 dx = static_cast<float32>(args->MouseDelta.X);
        float32 dy = static_cast<float32>(args->MouseDelta.Y);

        // win10 send dx == 0 and dy == 0 if mouse buttons change state only if one button already pressed
        if (mouseCaptureMode == InputSystem::eMouseCaptureMode::PINING && (dx != 0.f || dy != 0.f))
        {
            if (mouseButtonsState.none())
            {
                phase = UIEvent::Phase::MOVE;

                core->RunOnMainThread([this, phase, dx, dy]() {
                    DAVATouchEvent(phase, dx, dy, static_cast<int32>(UIEvent::MouseButton::NONE), UIEvent::Device::MOUSE);
                });
            }
            else
            {
                SendPressedMouseButtons(dx, dy, UIEvent::Device::MOUSE);
            }
        }
    }
}

void WinUAPXamlApp::DAVATouchEvent(UIEvent::Phase phase, float32 x, float32 y, int32 id, UIEvent::Device device)
{
    UIEvent newTouch;
    newTouch.touchId = id;
    newTouch.physPoint.x = x;
    newTouch.physPoint.y = y;
    newTouch.point.x = x;
    newTouch.point.y = y;
    newTouch.phase = phase;
    newTouch.device = device;
    newTouch.timestamp = (SystemTimer::FrameStampTimeMS() / 1000.0);
    UIControlSystem::Instance()->OnInput(&newTouch);
}

DeviceInfo::ScreenInfo WinUAPXamlApp::ObtainScreenInfo()
{
    DeviceInfo::ScreenInfo result;

    CoreWindow ^ coreWindow = Window::Current->CoreWindow;
    DisplayInformation ^ displayInfo = DisplayInformation::GetForCurrentView();
    DisplayOrientations orientation = displayInfo->CurrentOrientation;

    result.width = static_cast<float32>(coreWindow->Bounds.Width);
    result.height = static_cast<float32>(coreWindow->Bounds.Height);
    result.scale = static_cast<float64>(displayInfo->RawPixelsPerViewPixel);

    if (DisplayOrientations::Portrait == orientation || DisplayOrientations::PortraitFlipped == orientation)
    {
        std::swap(result.width, result.height);
    }
    return result;
}

void WinUAPXamlApp::SetupEventHandlers()
{
    Suspending += ref new SuspendingEventHandler(this, &WinUAPXamlApp::OnSuspending);
    Resuming += ref new EventHandler<::Platform::Object ^>(this, &WinUAPXamlApp::OnResuming);

    CoreWindow ^ coreWindow = Window::Current->CoreWindow;
    coreWindow->VisibilityChanged += ref new TypedEventHandler<CoreWindow ^, VisibilityChangedEventArgs ^>(this, &WinUAPXamlApp::OnWindowVisibilityChanged);

    coreWindow->SizeChanged += ref new TypedEventHandler<CoreWindow ^, WindowSizeChangedEventArgs ^>(this, &WinUAPXamlApp::OnCoreWindowSizeChanged);
    swapChainPanel->SizeChanged += ref new SizeChangedEventHandler(this, &WinUAPXamlApp::OnSwapChainPanelSizeChanged);
    swapChainPanel->CompositionScaleChanged += ref new TypedEventHandler<SwapChainPanel ^, Object ^>(this, &WinUAPXamlApp::OnSwapChainPanelCompositionScaleChanged);

    // Receive mouse events from SwapChainPanel, not CoreWindow, to not handle native controls' events
    swapChainPanel->PointerPressed += ref new PointerEventHandler(this, &WinUAPXamlApp::OnSwapChainPanelPointerPressed);
    swapChainPanel->PointerReleased += ref new PointerEventHandler(this, &WinUAPXamlApp::OnSwapChainPanelPointerReleased);
    swapChainPanel->PointerMoved += ref new PointerEventHandler(this, &WinUAPXamlApp::OnSwapChainPanelPointerMoved);
    swapChainPanel->PointerWheelChanged += ref new PointerEventHandler(this, &WinUAPXamlApp::OnSwapChainPanelPointerWheel);

    coreWindow->Dispatcher->AcceleratorKeyActivated += ref new TypedEventHandler<CoreDispatcher ^, AcceleratorKeyEventArgs ^>(this, &WinUAPXamlApp::OnAcceleratorKeyActivated);
    coreWindow->CharacterReceived += ref new TypedEventHandler<CoreWindow ^, CharacterReceivedEventArgs ^>(this, &WinUAPXamlApp::OnChar);

    if (isPhoneApiDetected)
    {
        HardwareButtons::BackPressed += ref new EventHandler<BackPressedEventArgs ^>(this, &WinUAPXamlApp::OnHardwareBackButtonPressed);
    }

    SystemNavigationManager::GetForCurrentView()->BackRequested += ref new EventHandler<BackRequestedEventArgs ^>(this, &WinUAPXamlApp::OnBackRequested);
}

void WinUAPXamlApp::CreateBaseXamlUI()
{
    // workaround for Surface, otherwise we lost MouseMoved event
    Platform::Object ^ obj = XamlReader::Load(ref new Platform::String(xamlWebView));
    WebView ^ webview = dynamic_cast<WebView ^>(obj);
    // workaround for mobile device, otherwise we have exception, when insert some text into recreated TextBox
    obj = XamlReader::Load(ref new Platform::String(xamlTextBox));
    TextBox ^ textBox = dynamic_cast<TextBox ^>(obj);

    swapChainPanel = ref new Controls::SwapChainPanel();
    canvas = ref new Controls::Canvas();
    swapChainPanel->Children->Append(canvas);
    Window::Current->Content = swapChainPanel;

    AddUIElement(webview);
    AddUIElement(textBox);

    // Windows UAP doesn't allow to unfocus UI control programmatically
    // It only permits to set focus at another control
    // So create dummy offscreen button that steals focus when there is
    // a need to unfocus native control, especially useful for text fields
    controlThatTakesFocus = ref new Button();
    controlThatTakesFocus->Content = L"I steal your focus";
    controlThatTakesFocus->Width = 30;
    controlThatTakesFocus->Height = 20;
    AddUIElement(controlThatTakesFocus);
    PositionUIElement(controlThatTakesFocus, -100, -100);

    { // Load custom textbox and password styles that allow transparent background when control has focus
        using Windows::UI::Xaml::Markup::XamlReader;

        Platform::Object ^ obj = XamlReader::Load(ref new Platform::String(xamlTextBoxStyles));
        ResourceDictionary ^ dict = (ResourceDictionary ^ )obj;

        Resources->MergedDictionaries->Append(dict);
        Object ^ texboxStyleObj = Resources->Lookup(ref new Platform::String(L"dava_custom_textbox"));
        Object ^ passwordStyleObj = Resources->Lookup(ref new Platform::String(L"dava_custom_passwordbox"));

        customTextBoxStyle = (Windows::UI::Xaml::Style ^ )texboxStyleObj;
        customPasswordBoxStyle = (Windows::UI::Xaml::Style ^ )passwordStyleObj;
    }
}

void WinUAPXamlApp::SetTitleName()
{
    // Note: must run on UI thread
    KeyedArchive* options = Core::Instance()->GetOptions();
    if (nullptr != options)
    {
        WideString title = StringToWString(options->GetString("title", "[set application title using core options property 'title']"));
        ApplicationView::GetForCurrentView()->Title = ref new ::Platform::String(title.c_str());
    }
}

void WinUAPXamlApp::SetDisplayOrientations()
{
    // Note: must run on UI thread
    Core::eScreenOrientation orientMode = Core::Instance()->GetScreenOrientation();
    switch (orientMode)
    {
    case Core::SCREEN_ORIENTATION_TEXTURE:
        break;
    case Core::SCREEN_ORIENTATION_LANDSCAPE_RIGHT:
        displayOrientation = DisplayOrientations::Landscape;
        break;
    case Core::SCREEN_ORIENTATION_LANDSCAPE_LEFT:
        displayOrientation = DisplayOrientations::LandscapeFlipped;
        break;
    case Core::SCREEN_ORIENTATION_PORTRAIT:
        displayOrientation = DisplayOrientations::Portrait;
        break;
    case Core::SCREEN_ORIENTATION_PORTRAIT_UPSIDE_DOWN:
        displayOrientation = DisplayOrientations::PortraitFlipped;
        break;
    case Core::SCREEN_ORIENTATION_LANDSCAPE_AUTOROTATE:
        displayOrientation = DisplayOrientations::Landscape | DisplayOrientations::LandscapeFlipped;
        break;
    case Core::SCREEN_ORIENTATION_PORTRAIT_AUTOROTATE:
        displayOrientation = DisplayOrientations::Portrait | DisplayOrientations::PortraitFlipped;
        break;
    }
    DisplayInformation::GetForCurrentView()->AutoRotationPreferences = displayOrientation;
}

void WinUAPXamlApp::LoadWindowMinimumSizeSettings()
{
    if (!isPhoneApiDetected)
    {
        const KeyedArchive* options = core->GetOptions();
        int32 minWidth = options->GetInt32("min-width", 0);
        int32 minHeight = options->GetInt32("min-height", 0);
        if (minWidth > 0 && minHeight > 0)
        {
            SetWindowMinimumSize(static_cast<float32>(minWidth), static_cast<float32>(minHeight));
        }
    }
}

void WinUAPXamlApp::PrepareScreenSize()
{
    // Note: must run on UI thread
    bool isFull(false);
    KeyedArchive* options = Core::Instance()->GetOptions();
    if (nullptr != options)
    {
        windowedMode.width = options->GetFloat("width", DisplayMode::DEFAULT_WIDTH);
        windowedMode.height = options->GetFloat("height", DisplayMode::DEFAULT_HEIGHT);
        windowedMode.bpp = options->GetInt32("bpp", DisplayMode::DEFAULT_BITS_PER_PIXEL);
        isFull = (0 != options->GetInt32("fullscreen", 0));
    }
    if (!isPhoneApiDetected)
    {
        if (!isFull)
        {
            // in units of effective (view) pixels
            ApplicationView::GetForCurrentView()->PreferredLaunchViewSize = Windows::Foundation::Size(static_cast<float32>(windowedMode.width), static_cast<float32>(windowedMode.height));
            ApplicationView::PreferredLaunchWindowingMode = ApplicationViewWindowingMode::PreferredLaunchViewSize;
        }
        else
        {
            ApplicationView::PreferredLaunchWindowingMode = ApplicationViewWindowingMode::FullScreen;
        }
        SetFullScreen(isFull);
    }
}

void WinUAPXamlApp::SetFullScreen(bool isFullscreen_)
{
    // Note: must run on UI thread
    ApplicationView ^ view = ApplicationView::GetForCurrentView();
    if (view->IsFullScreenMode == isFullscreen_)
    {
        isFullscreen = isFullscreen_;
        return;
    }
    if (isFullscreen_)
    {
        isFullscreen = view->TryEnterFullScreenMode();
    }
    else
    {
        view->ExitFullScreenMode();
        isFullscreen = false;
    }
}

void WinUAPXamlApp::EmitPushNotification(::Windows::ApplicationModel::Activation::LaunchActivatedEventArgs ^ args)
{
    DVASSERT(nullptr != dispatcher);
    dispatcher->RunAsync([=]() {
        pushNotificationSignal.Emit(args);
    });
}

void WinUAPXamlApp::AllowDisplaySleep(bool sleep)
{
    if (sleep)
    {
        displayRequest->RequestRelease();
    }
    else
    {
        displayRequest->RequestActive();
    }
}

void WinUAPXamlApp::SendBackKeyEvents()
{
    core->RunOnMainThread([this]() {
        UIEvent ev;
        ev.keyChar = 0;
        ev.phase = UIEvent::Phase::KEY_DOWN;
        ev.key = Key::BACK;
        ev.device = UIEvent::Device::KEYBOARD;
        ev.timestamp = (SystemTimer::FrameStampTimeMS() / 1000.0);

        UIControlSystem::Instance()->OnInput(&ev);
        InputSystem::Instance()->GetKeyboard().OnKeyPressed(Key::BACK);

        ev.phase = UIEvent::Phase::KEY_UP;
        UIControlSystem::Instance()->OnInput(&ev);
        InputSystem::Instance()->GetKeyboard().OnKeyUnpressed(Key::BACK);
    });
}

void WinUAPXamlApp::SetWindowMinimumSize(float32 width, float32 height)
{
    DVASSERT((width == 0.0f && height == 0.0f) || (width > 0.0f && height > 0.0f));
    if (!isPhoneApiDetected)
    {
        core->RunOnUIThread([this, width, height]() {
            // Note: the largest allowed minimum size is 500 x 500 effective pixels
            // https://msdn.microsoft.com/en-us/library/windows/apps/windows.ui.viewmanagement.applicationview.setpreferredminsize.aspx
            ApplicationView::GetForCurrentView()->SetPreferredMinSize(Size(width, height));
            minWindowWidth = width;
            minWindowHeight = height;
        });
    }
}

Vector2 WinUAPXamlApp::GetWindowMinimumSize() const
{
    return Vector2(minWindowWidth, minWindowHeight);
}

const wchar_t* WinUAPXamlApp::xamlTextBoxStyles = LR"(
<ResourceDictionary
    xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" 
    xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml">
    <Style x:Key="dava_custom_textbox" TargetType="TextBox">
        <Setter Property="MinWidth" Value="0" />
        <Setter Property="MinHeight" Value="0" />
        <Setter Property="Foreground" Value="White" />
        <Setter Property="Background" Value="Transparent" />
        <Setter Property="BorderBrush" Value="Transparent" />
        <Setter Property="SelectionHighlightColor" Value="{ThemeResource TextSelectionHighlightColorThemeBrush}" />
        <Setter Property="BorderThickness" Value="0" />
        <Setter Property="FontFamily" Value="{ThemeResource ContentControlThemeFontFamily}" />
        <Setter Property="FontSize" Value="{ThemeResource ControlContentThemeFontSize}" />
        <Setter Property="ScrollViewer.HorizontalScrollBarVisibility" Value="Hidden" />
        <Setter Property="ScrollViewer.VerticalScrollBarVisibility" Value="Hidden" />
        <Setter Property="ScrollViewer.IsDeferredScrollingEnabled" Value="False" />
        <Setter Property="Padding" Value="0"/>
        <Setter Property="Template">
            <Setter.Value>
                <ControlTemplate TargetType="TextBox">
                    <Grid>
                        <ContentPresenter x:Name="HeaderContentPresenter"
                                      Grid.Row="0"
                                      Margin="0,4,0,4"
                                      Grid.ColumnSpan="2"
                                      Content="{TemplateBinding Header}"
                                      ContentTemplate="{TemplateBinding HeaderTemplate}"
                                      FontWeight="Semilight" />
                        <ScrollViewer x:Name="ContentElement"
                                    Grid.Row="1"
                                    HorizontalScrollMode="{TemplateBinding ScrollViewer.HorizontalScrollMode}"
                                    HorizontalScrollBarVisibility="{TemplateBinding ScrollViewer.HorizontalScrollBarVisibility}"
                                    VerticalScrollMode="{TemplateBinding ScrollViewer.VerticalScrollMode}"
                                    VerticalScrollBarVisibility="{TemplateBinding ScrollViewer.VerticalScrollBarVisibility}"
                                    IsHorizontalRailEnabled="{TemplateBinding ScrollViewer.IsHorizontalRailEnabled}"
                                    IsVerticalRailEnabled="{TemplateBinding ScrollViewer.IsVerticalRailEnabled}"
                                    IsDeferredScrollingEnabled="{TemplateBinding ScrollViewer.IsDeferredScrollingEnabled}"
                                    Margin="{TemplateBinding BorderThickness}"
                                    Padding="{TemplateBinding Padding}"
                                    IsTabStop="False"
                                    AutomationProperties.AccessibilityView="Raw"
                                    ZoomMode="Disabled" />
                    </Grid>
                </ControlTemplate>
            </Setter.Value>
        </Setter>
    </Style>
    <Style x:Key="dava_custom_passwordbox" TargetType="PasswordBox">
        <Setter Property="MinWidth" Value="0" />
        <Setter Property="MinHeight" Value="0" />
        <Setter Property="Foreground" Value="White" />
        <Setter Property="Background" Value="Transparent" />
        <Setter Property="SelectionHighlightColor" Value="{ThemeResource TextSelectionHighlightColorThemeBrush}" />
        <Setter Property="BorderBrush" Value="Transparent" />
        <Setter Property="BorderThickness" Value="0" />
        <Setter Property="FontFamily" Value="{ThemeResource ContentControlThemeFontFamily}" />
        <Setter Property="FontSize" Value="{ThemeResource ControlContentThemeFontSize}" />
        <Setter Property="ScrollViewer.HorizontalScrollBarVisibility" Value="Hidden" />
        <Setter Property="ScrollViewer.VerticalScrollBarVisibility" Value="Hidden" />
        <Setter Property="Padding" Value="0"/>
        <Setter Property="Template">
            <Setter.Value>
                <ControlTemplate TargetType="PasswordBox">
                    <Grid>
                        <ContentPresenter x:Name="HeaderContentPresenter"
                                      Grid.Row="0"
                                      Margin="0,4,0,4"
                                      Grid.ColumnSpan="2"
                                      Content="{TemplateBinding Header}"
                                      ContentTemplate="{TemplateBinding HeaderTemplate}"
                                      FontWeight="Semilight" />
                        <ScrollViewer x:Name="ContentElement"
                            Grid.Row="1"
                                  HorizontalScrollMode="{TemplateBinding ScrollViewer.HorizontalScrollMode}"
                                  HorizontalScrollBarVisibility="{TemplateBinding ScrollViewer.HorizontalScrollBarVisibility}"
                                  VerticalScrollMode="{TemplateBinding ScrollViewer.VerticalScrollMode}"
                                  VerticalScrollBarVisibility="{TemplateBinding ScrollViewer.VerticalScrollBarVisibility}"
                                  IsHorizontalRailEnabled="{TemplateBinding ScrollViewer.IsHorizontalRailEnabled}"
                                  IsVerticalRailEnabled="{TemplateBinding ScrollViewer.IsVerticalRailEnabled}"
                                  Margin="{TemplateBinding BorderThickness}"
                                  Padding="{TemplateBinding Padding}"
                                  IsTabStop="False"
                                  ZoomMode="Disabled"
                                  AutomationProperties.AccessibilityView="Raw"/>
                    </Grid>
                </ControlTemplate>
            </Setter.Value>
        </Setter>
    </Style>
</ResourceDictionary>
)";

const wchar_t* WinUAPXamlApp::xamlWebView = LR"(
<WebView x:Name="xamlWebView" Visibility="Collapsed"
    xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" 
    xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml">
</WebView>
)";

const wchar_t* WinUAPXamlApp::xamlTextBox = LR"(
<TextBox x:Name="xamlTextBox" Visibility="Collapsed"
    xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" 
    xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml">
</TextBox>
)";

} // namespace DAVA

#endif // __DAVAENGINE_WIN_UAP__

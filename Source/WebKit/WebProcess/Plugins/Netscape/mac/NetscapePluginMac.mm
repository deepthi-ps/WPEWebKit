/*
 * Copyright (C) 2010-2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "NetscapePlugin.h"

#if ENABLE(NETSCAPE_PLUGIN_API)

#import "NetscapeBrowserFuncs.h"
#import "PluginController.h"
#import "WebEvent.h"
#import <Carbon/Carbon.h>
#import <WebCore/GraphicsContext.h>
#import <WebCore/NotImplemented.h>
#import <objc/runtime.h>
#import <pal/spi/mac/HIToolboxSPI.h>
#import <pal/spi/mac/NSMenuSPI.h>
#import <wtf/MachSendRight.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/text/StringView.h>

namespace WebKit {
using namespace WebCore;

#ifndef NP_NO_CARBON
static const Seconds nullEventIntervalActive { 20_ms };

static unsigned buttonStateFromLastMouseEvent;

#endif

NPError NetscapePlugin::setDrawingModel(NPDrawingModel drawingModel)
{
    // The drawing model can only be set from NPP_New.
    if (!m_inNPPNew)
        return NPERR_GENERIC_ERROR;

    switch (drawingModel) {
#ifndef NP_NO_QUICKDRAW
        case NPDrawingModelQuickDraw:
#endif
        case NPDrawingModelCoreGraphics:
        case NPDrawingModelCoreAnimation:
            m_drawingModel = drawingModel;
            break;

        default:
            return NPERR_GENERIC_ERROR;
    }

    return NPERR_NO_ERROR;
}
    
NPError NetscapePlugin::setEventModel(NPEventModel eventModel)
{
    // The event model can only be set from NPP_New.
    if (!m_inNPPNew)
        return NPERR_GENERIC_ERROR;

    switch (eventModel) {
#ifndef NP_NO_CARBON
        case NPEventModelCarbon:
#endif
        case NPEventModelCocoa:
            m_eventModel = eventModel;
            break;

        default:
            return NPERR_GENERIC_ERROR;
    }
    
    return NPERR_NO_ERROR;
}

bool NetscapePlugin::getScreenTransform(NPCoordinateSpace sourceSpace, AffineTransform& transform)
{
    ASSERT(transform.isIdentity());

    switch (sourceSpace) {
        case NPCoordinateSpacePlugin: {
            transform.translate(m_windowFrameInScreenCoordinates.x(), m_windowFrameInScreenCoordinates.y());
            transform.translate(m_viewFrameInWindowCoordinates.x(), m_viewFrameInWindowCoordinates.height() + m_viewFrameInWindowCoordinates.y());
            transform.flipY();
            transform *= m_pluginToRootViewTransform;
            return true;
        }

        case NPCoordinateSpaceWindow: {
            transform.translate(m_windowFrameInScreenCoordinates.x(), m_windowFrameInScreenCoordinates.y());
            return true;
        }

        case NPCoordinateSpaceFlippedWindow: {
            transform.translate(m_windowFrameInScreenCoordinates.x(), m_windowFrameInScreenCoordinates.height() + m_windowFrameInScreenCoordinates.y());
            transform.flipY();
            return true;
        }

        case NPCoordinateSpaceScreen: {
            // Nothing to do.
            return true;
        }

        case NPCoordinateSpaceFlippedScreen: {
            double screenHeight = [(NSScreen *)[[NSScreen screens] objectAtIndex:0] frame].size.height;
            transform.translate(0, screenHeight);
            transform.flipY();
            return true;
        }

        default:
            return false;
    }
}

NPBool NetscapePlugin::convertPoint(double sourceX, double sourceY, NPCoordinateSpace sourceSpace, double& destX, double& destY, NPCoordinateSpace destSpace)
{
    AffineTransform sourceTransform;
    if (!getScreenTransform(sourceSpace, sourceTransform))
        return false;

    AffineTransform destTransform;
    if (!getScreenTransform(destSpace, destTransform))
        return false;

    if (auto inverse = destTransform.inverse()) {
        AffineTransform transform = inverse.value() * sourceTransform;

        FloatPoint destinationPoint = transform.mapPoint(FloatPoint(sourceX, sourceY));

        destX = destinationPoint.x();
        destY = destinationPoint.y();
        return true;
    }
    return false;
}

NPError NetscapePlugin::popUpContextMenu(NPMenu* npMenu)
{
    if (!m_currentMouseEvent)
        return NPERR_GENERIC_ERROR;

    double screenX, screenY;
    if (!convertPoint(m_currentMouseEvent->data.mouse.pluginX, m_currentMouseEvent->data.mouse.pluginY, NPCoordinateSpacePlugin, screenX, screenY, NPCoordinateSpaceScreen))
        ASSERT_NOT_REACHED();

    _NSPopUpCarbonMenu3((__bridge NSMenu *)npMenu, nil, nil, NSMakePoint(screenX, screenY), -1, nil, 0, nil, NSPopUpMenuTypeContext, nil);
    return NPERR_NO_ERROR;
}

const MachSendRight& NetscapePlugin::compositingRenderServerPort()
{
#if HAVE(OUT_OF_PROCESS_LAYER_HOSTING)
    if (m_layerHostingMode == LayerHostingMode::OutOfProcess) {
        static NeverDestroyed<MachSendRight> sendRight;

        return sendRight;
    }
#endif

    return controller()->compositingRenderServerPort();
}

#ifndef NP_NO_CARBON
typedef HashMap<WindowRef, NetscapePlugin*> WindowMap;

static WindowMap& windowMap()
{
    static NeverDestroyed<WindowMap> windowMap;
    return windowMap;
}
#endif

static void NSException_release(id, SEL)
{
    // Do nothing.
}

void NetscapePlugin::platformPreInitialize()
{
    if (m_pluginModule->pluginQuirks().contains(PluginQuirks::LeakAllThrownNSExceptions)) {
        // Patch -[NSException release] to not release the object.
        static dispatch_once_t once;
        dispatch_once(&once, ^{
            Class exceptionClass = [NSException class];
            Method exceptionReleaseMethod = class_getInstanceMethod(exceptionClass, @selector(release));
            class_replaceMethod(exceptionClass, @selector(release), reinterpret_cast<IMP>(NSException_release), method_getTypeEncoding(exceptionReleaseMethod));
        });
    }
}

bool NetscapePlugin::platformPostInitialize()
{
    if (m_drawingModel == static_cast<NPDrawingModel>(-1)) {
#ifndef NP_NO_QUICKDRAW
        // Default to QuickDraw if the plugin did not specify a drawing model.
        m_drawingModel = NPDrawingModelQuickDraw;
#else
        // QuickDraw is not available, so we can't default to it. Instead, default to CoreGraphics.
        m_drawingModel = NPDrawingModelCoreGraphics;
#endif
    }
    
    if (m_eventModel == static_cast<NPEventModel>(-1)) {
        // If the plug-in did not specify a drawing model we default to Carbon when it is available.
#ifndef NP_NO_CARBON
        m_eventModel = NPEventModelCarbon;
#else
        m_eventModel = NPEventModelCocoa;
#endif // NP_NO_CARBON
    }

#if !defined(NP_NO_CARBON) && !defined(NP_NO_QUICKDRAW)
    // The CA drawing model does not work with the Carbon event model.
    if (m_drawingModel == NPDrawingModelCoreAnimation && m_eventModel == NPEventModelCarbon)
        return false;
    
    // The Cocoa event model does not work with the QuickDraw drawing model.
    if (m_eventModel == NPEventModelCocoa && m_drawingModel == NPDrawingModelQuickDraw)
        return false;
#endif
    
#ifndef NP_NO_QUICKDRAW
    // Right now we don't support the QuickDraw drawing model at all
    if (m_drawingModel == NPDrawingModelQuickDraw &&
        !m_pluginModule->pluginQuirks().contains(PluginQuirks::AllowHalfBakedQuickDrawSupport))
        return false;
#endif

    updatePluginLayer();

#ifndef NP_NO_CARBON
    if (m_eventModel == NPEventModelCarbon) {
        // Initialize the fake Carbon window.
        ::Rect bounds = { 0, 0, 0, 0 };
        CreateNewWindow(kDocumentWindowClass, kWindowNoTitleBarAttribute, &bounds, reinterpret_cast<WindowRef*>(&m_npCGContext.window));
        ASSERT(m_npCGContext.window);

        // FIXME: Disable the backing store.

        m_npWindow.window = &m_npCGContext;

        ASSERT(!windowMap().contains(windowRef()));
        windowMap().set(windowRef(), this);

        // Start the null event timer.
        // FIXME: Throttle null events when the plug-in isn't visible on screen.
        m_nullEventTimer.startRepeating(nullEventIntervalActive);
    }
#endif

    return true;
}

void NetscapePlugin::platformDestroy()
{
#ifndef NP_NO_CARBON
    if (m_eventModel == NPEventModelCarbon) {
        if (WindowRef window = windowRef()) {
            // Destroy the fake Carbon window.
            DisposeWindow(window);

            ASSERT(windowMap().contains(window));
            windowMap().remove(window);
        }

        // Stop the null event timer.
        m_nullEventTimer.stop();
    }
#endif
}

bool NetscapePlugin::platformInvalidate(const IntRect&)
{
    // NPN_InvalidateRect is just a no-op in the Core Animation drawing model.
    if (m_drawingModel == NPDrawingModelCoreAnimation)
        return true;

    return false;
}

void NetscapePlugin::platformGeometryDidChange()
{
    switch (m_eventModel) {
    case NPEventModelCocoa:
        // Nothing to do
        break;
#ifndef NP_NO_CARBON
    case NPEventModelCarbon:
        updateFakeWindowBounds();
        break;
#endif
    default:
        ASSERT_NOT_REACHED();
    }
}

void NetscapePlugin::platformVisibilityDidChange()
{
    // FIXME: Implement this. <http://webkit.org/b/44368>.
    notImplemented();
}

static inline NPCocoaEvent initializeEvent(NPCocoaEventType type)
{
    NPCocoaEvent event;
    
    event.type = type;
    event.version = 0;
    
    return event;
}

#ifndef NP_NO_CARBON
NetscapePlugin* NetscapePlugin::netscapePluginFromWindow(WindowRef windowRef)
{
    return windowMap().get(windowRef);
}

WindowRef NetscapePlugin::windowRef() const
{
    ASSERT(m_eventModel == NPEventModelCarbon);

    return reinterpret_cast<WindowRef>(m_npCGContext.window);
}

void NetscapePlugin::updateFakeWindowBounds()
{
    double screenX, screenY;
    bool didConvert = convertPoint(0, 0, NPCoordinateSpacePlugin, screenX, screenY, NPCoordinateSpaceFlippedScreen);
    ASSERT_UNUSED(didConvert, didConvert);
    
    Rect bounds;
    bounds.top = screenY;
    bounds.left = screenX;
    bounds.bottom = screenY + m_pluginSize.height();
    bounds.right = screenX + m_pluginSize.width();
    
    ::SetWindowBounds(windowRef(), kWindowStructureRgn, &bounds);
}

unsigned NetscapePlugin::buttonState()
{
    return buttonStateFromLastMouseEvent;
}

static inline EventRecord initializeEventRecord(EventKind eventKind)
{
    EventRecord eventRecord;

    eventRecord.what = eventKind;
    eventRecord.message = 0;
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    eventRecord.when = TickCount();
    ALLOW_DEPRECATED_DECLARATIONS_END
    eventRecord.where = Point();
    eventRecord.modifiers = 0;

    return eventRecord;
}

static bool anyMouseButtonIsDown(const WebEvent& event)
{
    if (event.type() == WebEvent::MouseDown)
        return true;

    if (event.type() == WebEvent::MouseMove && static_cast<const WebMouseEvent&>(event).button() != WebMouseEvent::NoButton)
        return true;

    return false;
}

static bool rightMouseButtonIsDown(const WebEvent& event)
{
    if (event.type() == WebEvent::MouseDown && static_cast<const WebMouseEvent&>(event).button() == WebMouseEvent::RightButton)
        return true;
    
    if (event.type() == WebEvent::MouseMove && static_cast<const WebMouseEvent&>(event).button() == WebMouseEvent::RightButton)
        return true;
    
    return false;
}

static EventModifiers modifiersForEvent(const WebEvent& event)
{
    EventModifiers modifiers = 0;

    // We only want to set the btnState if a mouse button is _not_ down.
    if (!anyMouseButtonIsDown(event))
        modifiers |= btnState;

    if (event.metaKey())
        modifiers |= cmdKey;

    if (event.shiftKey())
        modifiers |= shiftKey;

    if (event.altKey())
        modifiers |= optionKey;

    // Set controlKey if the control key is down or the right mouse button is down.
    if (event.controlKey() || rightMouseButtonIsDown(event))
        modifiers |= controlKey;

    return modifiers;
}

#endif

void NetscapePlugin::platformPaint(GraphicsContext& context, const IntRect& dirtyRect, bool isSnapshot)
{
    CGContextRef platformContext = context.platformContext();

    switch (m_eventModel) {
        case NPEventModelCocoa: {
            // Don't send draw events when we're using the Core Animation drawing model.
            if (!isSnapshot && m_drawingModel == NPDrawingModelCoreAnimation)
                return;

            NPCocoaEvent event = initializeEvent(NPCocoaEventDrawRect);

            event.data.draw.context = platformContext;
            event.data.draw.x = dirtyRect.x();
            event.data.draw.y = dirtyRect.y();
            event.data.draw.width = dirtyRect.width();
            event.data.draw.height = dirtyRect.height();
            
            NPP_HandleEvent(&event);
            break;
        }

#ifndef NP_NO_CARBON
        case NPEventModelCarbon: {
            if (platformContext != m_npCGContext.context) {
                m_npCGContext.context = platformContext;
                callSetWindow();
            }

            EventRecord event = initializeEventRecord(updateEvt);
            event.message = reinterpret_cast<unsigned long>(windowRef());
            
            NPP_HandleEvent(&event);
            break;            
        }
#endif

        default:
            ASSERT_NOT_REACHED();
    }
}

static uint32_t modifierFlags(const WebEvent& event)
{
    uint32_t modifiers = 0;

    if (event.shiftKey())
        modifiers |= NSEventModifierFlagShift;
    if (event.controlKey())
        modifiers |= NSEventModifierFlagControl;
    if (event.altKey())
        modifiers |= NSEventModifierFlagOption;
    if (event.metaKey())
        modifiers |= NSEventModifierFlagCommand;
    
    return modifiers;
}
    
static int32_t buttonNumber(WebMouseEvent::Button button)
{
    switch (button) {
    case WebMouseEvent::NoButton:
    case WebMouseEvent::LeftButton:
        return 0;
    case WebMouseEvent::RightButton:
        return 1;
    case WebMouseEvent::MiddleButton:
        return 2;
    }

    ASSERT_NOT_REACHED();
    return -1;
}

static void fillInCocoaEventFromMouseEvent(NPCocoaEvent& event, const WebMouseEvent& mouseEvent, const WebCore::IntPoint& eventPositionInPluginCoordinates)
{
    event.data.mouse.modifierFlags = modifierFlags(mouseEvent);
    event.data.mouse.pluginX = eventPositionInPluginCoordinates.x();
    event.data.mouse.pluginY = eventPositionInPluginCoordinates.y();
    event.data.mouse.buttonNumber = buttonNumber(mouseEvent.button());
    event.data.mouse.clickCount = mouseEvent.clickCount();
    event.data.mouse.deltaX = mouseEvent.deltaX();
    event.data.mouse.deltaY = mouseEvent.deltaY();
    event.data.mouse.deltaZ = mouseEvent.deltaZ();
}
    
static NPCocoaEvent initializeMouseEvent(const WebMouseEvent& mouseEvent, const WebCore::IntPoint& eventPositionInPluginCoordinates)
{
    NPCocoaEventType eventType;

    switch (mouseEvent.type()) {
    case WebEvent::MouseDown:
        eventType = NPCocoaEventMouseDown;
        break;
    case WebEvent::MouseUp:
        eventType = NPCocoaEventMouseUp;
        break;
    case WebEvent::MouseMove:
        if (mouseEvent.button() == WebMouseEvent::NoButton)
            eventType = NPCocoaEventMouseMoved;
        else
            eventType = NPCocoaEventMouseDragged;
        break;
    default:
        ASSERT_NOT_REACHED();
        return NPCocoaEvent();
    }

    NPCocoaEvent event = initializeEvent(eventType);
    fillInCocoaEventFromMouseEvent(event, mouseEvent, eventPositionInPluginCoordinates);
    return event;
}

bool NetscapePlugin::platformHandleMouseEvent(const WebMouseEvent& mouseEvent)
{
    IntPoint eventPositionInPluginCoordinates;
    if (!convertFromRootView(mouseEvent.position(), eventPositionInPluginCoordinates))
        return true;

    switch (m_eventModel) {
        case NPEventModelCocoa: {
            NPCocoaEvent event = initializeMouseEvent(mouseEvent, eventPositionInPluginCoordinates);

            NPCocoaEvent* previousMouseEvent = m_currentMouseEvent;
            m_currentMouseEvent = &event;

            // Protect against NPP_HandleEvent causing the plug-in to be destroyed, since we
            // access m_currentMouseEvent afterwards.
            Ref<NetscapePlugin> protect(*this);

            NPP_HandleEvent(&event);

            m_currentMouseEvent = previousMouseEvent;

            // Some plug-ins return false even if the mouse event has been handled.
            // This leads to bugs such as <rdar://problem/9167611>. Work around this
            // by always returning true.
            return true;
        }

#ifndef NP_NO_CARBON
        case NPEventModelCarbon: {
            EventKind eventKind = nullEvent;

            switch (mouseEvent.type()) {
            case WebEvent::MouseDown:
                eventKind = mouseDown;
                buttonStateFromLastMouseEvent |= (1 << buttonNumber(mouseEvent.button()));
                break;
            case WebEvent::MouseUp:
                eventKind = mouseUp;
                buttonStateFromLastMouseEvent &= ~(1 << buttonNumber(mouseEvent.button()));
                break;
            case WebEvent::MouseMove:
                eventKind = nullEvent;
                break;
            default:
                ASSERT_NOT_REACHED();
            }

            EventRecord event = initializeEventRecord(eventKind);
            event.modifiers = modifiersForEvent(mouseEvent);

            double globalX;
            double globalY;
            if (!convertPoint(eventPositionInPluginCoordinates.x(), eventPositionInPluginCoordinates.y(), NPCoordinateSpacePlugin, globalX, globalY, NPCoordinateSpaceFlippedScreen))
                ASSERT_NOT_REACHED();

            event.where.h = globalX;
            event.where.v = globalY;

            NPP_HandleEvent(&event);

            // Some plug-ins return false even if the mouse event has been handled.
            // This leads to bugs such as <rdar://problem/9167611>. Work around this
            // by always returning true.
            return true;
        }
#endif

        default:
            ASSERT_NOT_REACHED();
    }

    return false;
}

bool NetscapePlugin::platformHandleWheelEvent(const WebWheelEvent& wheelEvent)
{
    switch (m_eventModel) {
        case NPEventModelCocoa: {
            IntPoint eventPositionInPluginCoordinates;
            if (!convertFromRootView(wheelEvent.position(), eventPositionInPluginCoordinates))
                return true;

            NPCocoaEvent event = initializeEvent(NPCocoaEventScrollWheel);
            
            event.data.mouse.modifierFlags = modifierFlags(wheelEvent);
            event.data.mouse.pluginX = eventPositionInPluginCoordinates.x();
            event.data.mouse.pluginY = eventPositionInPluginCoordinates.y();
            event.data.mouse.buttonNumber = 0;
            event.data.mouse.clickCount = 0;
            event.data.mouse.deltaX = wheelEvent.delta().width();
            event.data.mouse.deltaY = wheelEvent.delta().height();
            event.data.mouse.deltaZ = 0;
            return NPP_HandleEvent(&event);
        }

#ifndef NP_NO_CARBON
        case NPEventModelCarbon:
            // Carbon doesn't have wheel events.
            break;
#endif

        default:
            ASSERT_NOT_REACHED();
    }

    return false;
}

bool NetscapePlugin::platformHandleMouseEnterEvent(const WebMouseEvent& mouseEvent)
{
    switch (m_eventModel) {
        case NPEventModelCocoa: {
            NPCocoaEvent event = initializeEvent(NPCocoaEventMouseEntered);
            
            fillInCocoaEventFromMouseEvent(event, mouseEvent, IntPoint());
            return NPP_HandleEvent(&event);
        }

#ifndef NP_NO_CARBON
        case NPEventModelCarbon: {
            EventRecord eventRecord = initializeEventRecord(NPEventType_AdjustCursorEvent);
            eventRecord.modifiers = modifiersForEvent(mouseEvent);
            
            return NPP_HandleEvent(&eventRecord);
        }
#endif

        default:
            ASSERT_NOT_REACHED();
    }

    return false;
}

bool NetscapePlugin::platformHandleMouseLeaveEvent(const WebMouseEvent& mouseEvent)
{
    switch (m_eventModel) {
        case NPEventModelCocoa: {
            NPCocoaEvent event = initializeEvent(NPCocoaEventMouseExited);
            
            fillInCocoaEventFromMouseEvent(event, mouseEvent, IntPoint());
            return NPP_HandleEvent(&event);
        }

#ifndef NP_NO_CARBON
        case NPEventModelCarbon: {
            EventRecord eventRecord = initializeEventRecord(NPEventType_AdjustCursorEvent);
            eventRecord.modifiers = modifiersForEvent(mouseEvent);
            
            return NPP_HandleEvent(&eventRecord);
        }
#endif

        default:
            ASSERT_NOT_REACHED();
    }

    return false;
}

static unsigned modifierFlags(const WebKeyboardEvent& keyboardEvent)
{
    unsigned modifierFlags = 0;

    if (keyboardEvent.capsLockKey())
        modifierFlags |= NSEventModifierFlagCapsLock;
    if (keyboardEvent.shiftKey())
        modifierFlags |= NSEventModifierFlagShift;
    if (keyboardEvent.controlKey())
        modifierFlags |= NSEventModifierFlagControl;
    if (keyboardEvent.altKey())
        modifierFlags |= NSEventModifierFlagOption;
    if (keyboardEvent.metaKey())
        modifierFlags |= NSEventModifierFlagCommand;

    return modifierFlags;
}

static bool isFlagsChangedEvent(const WebKeyboardEvent& keyboardEvent)
{
    switch (keyboardEvent.nativeVirtualKeyCode()) {
    case 54: // Right Command
    case 55: // Left Command

    case 57: // Capslock

    case 56: // Left Shift
    case 60: // Right Shift

    case 58: // Left Alt
    case 61: // Right Alt
            
    case 59: // Left Ctrl
    case 62: // Right Ctrl
        return true;
    }

    return false;
}

static NPNSString *convertToNPNNString(const String& string)
{
    CFTypeRef releasedString = string.createCFString().autorelease();
    return static_cast<NPNSString*>(const_cast<void*>(releasedString));
}

static NPCocoaEvent initializeKeyboardEvent(const WebKeyboardEvent& keyboardEvent)
{
    NPCocoaEventType eventType;

    if (isFlagsChangedEvent(keyboardEvent))
        eventType = NPCocoaEventFlagsChanged;
    else {
        switch (keyboardEvent.type()) {
            case WebEvent::KeyDown:
                eventType = NPCocoaEventKeyDown;
                break;
            case WebEvent::KeyUp:
                eventType = NPCocoaEventKeyUp;
                break;
            default:
                ASSERT_NOT_REACHED();
                return NPCocoaEvent();
        }
    }

    NPCocoaEvent event = initializeEvent(eventType);
    event.data.key.modifierFlags = modifierFlags(keyboardEvent);
    event.data.key.characters = convertToNPNNString(keyboardEvent.text());
    event.data.key.charactersIgnoringModifiers = convertToNPNNString(keyboardEvent.unmodifiedText());
    event.data.key.isARepeat = keyboardEvent.isAutoRepeat();
    event.data.key.keyCode = keyboardEvent.nativeVirtualKeyCode();

    return event;
}

bool NetscapePlugin::platformHandleKeyboardEvent(const WebKeyboardEvent& keyboardEvent)
{
    bool handled = false;

    switch (m_eventModel) {
    case NPEventModelCocoa: {
        if (keyboardEvent.type() == WebEvent::KeyDown) {
            m_hasHandledAKeyDownEvent = true;

            if (!m_pluginWantsLegacyCocoaTextInput && m_isComplexTextInputEnabled && !keyboardEvent.isAutoRepeat()) {
                // When complex text is enabled in the new model, the plug-in should never
                // receive any key down or key up events until the composition is complete.
                m_ignoreNextKeyUpEventCounter++;
                return true;
            }
        } else if (keyboardEvent.type() == WebEvent::KeyUp && m_ignoreNextKeyUpEventCounter) {
            m_ignoreNextKeyUpEventCounter--;
            return true;
        }

        NPCocoaEvent event = initializeKeyboardEvent(keyboardEvent);
        int16_t returnValue = NPP_HandleEvent(&event);
        handled = returnValue;

        if (!m_pluginWantsLegacyCocoaTextInput) {
            if (event.type == NPCocoaEventKeyDown && returnValue == kNPEventStartIME) {
                if (!keyboardEvent.isAutoRepeat())
                    m_ignoreNextKeyUpEventCounter++;
                setComplexTextInputEnabled(true);
            }
        }

        break;
    }

#ifndef NP_NO_CARBON
    case NPEventModelCarbon: {
        EventKind eventKind = nullEvent;

        switch (keyboardEvent.type()) {
        case WebEvent::KeyDown:
            eventKind = keyboardEvent.isAutoRepeat() ? autoKey : keyDown;
            break;
        case WebEvent::KeyUp:
            eventKind = keyUp;
            break;
        default:
            ASSERT_NOT_REACHED();
        }

        EventRecord event = initializeEventRecord(eventKind);
        event.modifiers = modifiersForEvent(keyboardEvent);
        event.message = keyboardEvent.nativeVirtualKeyCode() << 8 | keyboardEvent.macCharCode();
        handled = NPP_HandleEvent(&event);
        break;
    }
#endif

    default:
        ASSERT_NOT_REACHED();
    }

    // Most plug-ins simply return true for all keyboard events, even those that aren't handled.
    // This leads to bugs such as <rdar://problem/8740926>. We work around this by returning false
    // if the keyboard event has the command modifier pressed.
    // However, for command-A (the shortcurt for 'Select All' we will always return true, since we don't
    // want the entire page to be selected if the focus is in a plug-in text field (see <rdar://problem/9309903>).
    if (keyboardEvent.metaKey()) {
        if (keyboardEvent.text() == "a")
            return true;

        return false;
    }

    return handled;
}

void NetscapePlugin::platformSetFocus(bool hasFocus)
{
    m_pluginHasFocus = hasFocus;
    pluginFocusOrWindowFocusChanged();

    switch (m_eventModel) {
        case NPEventModelCocoa: {
            NPCocoaEvent event = initializeEvent(NPCocoaEventFocusChanged);
            
            event.data.focus.hasFocus = hasFocus;
            NPP_HandleEvent(&event);
            break;
        }

#ifndef NP_NO_CARBON
        case NPEventModelCarbon: {
            EventRecord event = initializeEventRecord(hasFocus ? NPEventType_GetFocusEvent : NPEventType_LoseFocusEvent);

            NPP_HandleEvent(&event);
            break;
        }
#endif
            
        default:
            ASSERT_NOT_REACHED();
    }
}

bool NetscapePlugin::wantsPluginRelativeNPWindowCoordinates()
{
    return true;
}

void NetscapePlugin::windowFocusChanged(bool hasFocus)
{
    m_windowHasFocus = hasFocus;
    pluginFocusOrWindowFocusChanged();

    switch (m_eventModel) {
        case NPEventModelCocoa: {
            NPCocoaEvent event = initializeEvent(NPCocoaEventWindowFocusChanged);
            
            event.data.focus.hasFocus = hasFocus;
            NPP_HandleEvent(&event);
            break;
        }
        
#ifndef NP_NO_CARBON
        case NPEventModelCarbon: {
            HiliteWindow(windowRef(), hasFocus);
            if (hasFocus)
                SetUserFocusWindow(windowRef());

            EventRecord event = initializeEventRecord(activateEvt);
            event.message = reinterpret_cast<unsigned long>(windowRef());
            if (hasFocus)
                event.modifiers |= activeFlag;
            
            NPP_HandleEvent(&event);
            break;
        }
#endif

        default:
            ASSERT_NOT_REACHED();
    }
}

void NetscapePlugin::windowAndViewFramesChanged(const IntRect& windowFrameInScreenCoordinates, const IntRect& viewFrameInWindowCoordinates)
{
    m_windowFrameInScreenCoordinates = windowFrameInScreenCoordinates;
    m_viewFrameInWindowCoordinates = viewFrameInWindowCoordinates;

    switch (m_eventModel) {
        case NPEventModelCocoa:
            // Nothing to do.
            break;

#ifndef NP_NO_CARBON
        case NPEventModelCarbon:
            updateFakeWindowBounds();
            break;
#endif

        default:
            ASSERT_NOT_REACHED();
    }
}
    
void NetscapePlugin::windowVisibilityChanged(bool visible)
{
    if (visible)
        callSetWindow();
    else
        callSetWindowInvisible();
}

uint64_t NetscapePlugin::pluginComplexTextInputIdentifier() const
{
    // Just return a dummy value; this is only called for in-process plug-ins, which we don't support on Mac.
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(this));
}


#ifndef NP_NO_CARBON
static bool convertStringToKeyCodes(StringView string, ScriptCode scriptCode, Vector<UInt8>& keyCodes)
{
    // Create the mapping.
    UnicodeMapping mapping;

    if (GetTextEncodingFromScriptInfo(scriptCode, kTextLanguageDontCare, kTextRegionDontCare, &mapping.otherEncoding) != noErr)
        return false;

    mapping.unicodeEncoding = CreateTextEncoding(kTextEncodingUnicodeDefault, kTextEncodingDefaultVariant, kTextEncodingDefaultFormat);
    mapping.mappingVersion = kUnicodeUseLatestMapping;

    // Create the converter
    UnicodeToTextInfo textInfo;

    if (CreateUnicodeToTextInfo(&mapping, &textInfo) != noErr)
        return false;

    ByteCount inputLength = string.length() * sizeof(UniChar);
    ByteCount inputRead;
    ByteCount outputLength;
    ByteCount maxOutputLength = string.length() * sizeof(UniChar);

    Vector<UInt8> outputData(maxOutputLength);
    OSStatus status = ConvertFromUnicodeToText(textInfo, inputLength, string.upconvertedCharacters(), kNilOptions, 0, 0, 0, 0, maxOutputLength, &inputRead, &outputLength, outputData.data());
    
    DisposeUnicodeToTextInfo(&textInfo);
    
    if (status != noErr)
        return false;

    keyCodes = WTFMove(outputData);
    return true;
}
#endif

#ifndef NP_NO_CARBON
static ScriptCode scriptCodeFromCurrentKeyboardInputSource()
{
    ScriptCode scriptCode = smRoman;
    auto inputSource = adoptCF(TISCopyCurrentKeyboardInputSource());

    CFTypeRef scriptCodeNumber = TSMGetInputSourceProperty((TSMInputSourceRef)inputSource.get(), kTSMInputSourcePropertyScriptCode);
    ASSERT(CFGetTypeID(scriptCodeNumber) == CFNumberGetTypeID());
    if (scriptCodeNumber)
        CFNumberGetValue((CFNumberRef)scriptCodeNumber, kCFNumberSInt16Type, &scriptCode);

    return scriptCode;
}
#endif

void NetscapePlugin::sendComplexTextInput(const String& textInput)
{
    if (!m_pluginWantsLegacyCocoaTextInput) {
        // In the updated Cocoa text input spec, text input is disabled when the text input string has been sent
        // by the UI process. Since the UI process has also updated its state, we can just reset the variable here
        // instead of calling setComplexTextInputEnabled.
        m_isComplexTextInputEnabled = false;

        // The UI process can also disable text input by sending an empty input string. In this case, we don't want
        // to send it to the plug-in.
        if (textInput.isNull())
            return;
    }

    switch (m_eventModel) {
    case NPEventModelCocoa: {
        NPCocoaEvent event = initializeEvent(NPCocoaEventTextInput);
        event.data.text.text = convertToNPNNString(textInput);
        NPP_HandleEvent(&event);
        break;
    }
#ifndef NP_NO_CARBON
    case NPEventModelCarbon: {
        ScriptCode scriptCode = scriptCodeFromCurrentKeyboardInputSource();
        Vector<UInt8> keyCodes;

        if (!convertStringToKeyCodes(textInput, scriptCode, keyCodes))
            return;

        // Set the script code as the keyboard script. Normally Carbon does this whenever the input source changes.
        // However, this is only done for the process that has the keyboard focus. We cheat and do it here instead.
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        SetScriptManagerVariable(smKeyScript, scriptCode);
        ALLOW_DEPRECATED_DECLARATIONS_END
        
        EventRecord event = initializeEventRecord(keyDown);
        event.modifiers = 0;

        for (auto& keyCode : keyCodes) {
            event.message = keyCode;
            NPP_HandleEvent(&event);
        }
        break;
    }
#endif
    default:
        ASSERT_NOT_REACHED();
    }
}

void NetscapePlugin::setLayerHostingMode(LayerHostingMode layerHostingMode)
{
    m_layerHostingMode = layerHostingMode;

    // Tell the plug-in about the new compositing render server port. If it returns OK we'll ask it again for a new layer.
    mach_port_t port = NetscapePlugin::compositingRenderServerPort().sendRight();
    if (NPP_SetValue(static_cast<NPNVariable>(WKNVCALayerRenderServerPort), &port) != NPERR_NO_ERROR)
        return;

    m_pluginLayer = nullptr;
    updatePluginLayer();
}

void NetscapePlugin::pluginFocusOrWindowFocusChanged()
{
    bool pluginHasFocusAndWindowHasFocus = m_pluginHasFocus && m_windowHasFocus;

    controller()->pluginFocusOrWindowFocusChanged(pluginHasFocusAndWindowHasFocus);

    // In the updated Cocoa text input spec, the plug-in will enable complex text input
    // by returning kNPEventStartIME from it's NPCocoaEventKeyDown handler.
    if (!m_pluginWantsLegacyCocoaTextInput)
        return;

    // In the old model, if the plug-in is focused, enable complex text input.
    setComplexTextInputEnabled(pluginHasFocusAndWindowHasFocus);
}

void NetscapePlugin::setComplexTextInputEnabled(bool complexTextInputEnabled)
{
    if (m_isComplexTextInputEnabled == complexTextInputEnabled)
        return;

    m_isComplexTextInputEnabled = complexTextInputEnabled;

    PluginComplexTextInputState complexTextInputState = PluginComplexTextInputDisabled;
    if (m_isComplexTextInputEnabled)
        complexTextInputState = m_pluginWantsLegacyCocoaTextInput ? PluginComplexTextInputEnabledLegacy : PluginComplexTextInputEnabled;

    controller()->setComplexTextInputState(complexTextInputState);
}

PlatformLayer* NetscapePlugin::pluginLayer()
{
    return static_cast<PlatformLayer*>(m_pluginLayer.get());
}

static void makeCGLPresentLayerOpaque(CALayer *pluginRootLayer)
{
    // We look for a layer that's the only sublayer of the root layer that is an instance
    // of the CGLPresentLayer class which in turn is a subclass of CAOpenGLLayer and make
    // it opaque if all these conditions hold.

    NSArray *sublayers = [pluginRootLayer sublayers];
    if ([sublayers count] != 1)
        return;

    Class cglPresentLayerClass = NSClassFromString(@"CGLPresentLayer");
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if (![cglPresentLayerClass isSubclassOfClass:[CAOpenGLLayer class]])
        return;
    ALLOW_DEPRECATED_DECLARATIONS_END

    CALayer *layer = [sublayers objectAtIndex:0];
    if (![layer isKindOfClass:cglPresentLayerClass])
        return;

    [layer setOpaque:YES];
}

void NetscapePlugin::updatePluginLayer()
{
    if (m_drawingModel != NPDrawingModelCoreAnimation)
        return;

    void* value = 0;

    // Get the Core Animation layer.
    if (NPP_GetValue(NPPVpluginCoreAnimationLayer, &value) != NPERR_NO_ERROR)
        return;

    if (!value)
        return;

    ASSERT(!m_pluginLayer);

    // The original Core Animation drawing model required that plug-ins pass a retained layer
    // to the browser, which the browser would then adopt. However, the final spec changed this
    // (See https://wiki.mozilla.org/NPAPI:CoreAnimationDrawingModel for more information)
    // after a version of WebKit1 with the original implementation had shipped, but that now means
    // that any plug-ins that expect the WebKit1 behavior would leak the CALayer.
    // For plug-ins that we know return retained layers, we have the ReturnsRetainedCoreAnimationLayer
    // plug-in quirk. Plug-ins can also check for whether the browser expects a non-retained layer to
    // be returned by using NPN_GetValue and pass the WKNVExpectsNonretainedLayer parameter.
    // https://bugs.webkit.org/show_bug.cgi?id=58282 describes the bug where WebKit expects retained layers.
    if (m_pluginReturnsNonretainedLayer)
        m_pluginLayer = reinterpret_cast<CALayer *>(value);
    else
        m_pluginLayer = adoptNS(reinterpret_cast<CALayer *>(value));

    if (m_pluginModule->pluginQuirks().contains(PluginQuirks::MakeOpaqueUnlessTransparentSilverlightBackgroundAttributeExists) &&
        !m_isTransparent)
        makeCGLPresentLayerOpaque(m_pluginLayer.get());
}

#ifndef NP_NO_CARBON
void NetscapePlugin::nullEventTimerFired()
{
    EventRecord event = initializeEventRecord(nullEvent);

    event.message = 0;
    CGPoint mousePosition;
    HIGetMousePosition(kHICoordSpaceScreenPixel, 0, &mousePosition);
    event.where.h = mousePosition.x;
    event.where.v = mousePosition.y;

    event.modifiers = GetCurrentKeyModifiers();
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if (!Button())
        event.modifiers |= btnState;
    ALLOW_DEPRECATED_DECLARATIONS_END

    NPP_HandleEvent(&event);
}
#endif

} // namespace WebKit

#endif // ENABLE(NETSCAPE_PLUGIN_API)

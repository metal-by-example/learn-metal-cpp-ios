//-------------------------------------------------------------------------------------------------------------------------------------------------------------
//
// UIKit/UIScene.hpp
//
//-------------------------------------------------------------------------------------------------------------------------------------------------------------

#pragma once

//-------------------------------------------------------------------------------------------------------------------------------------------------------------

#include <Foundation/Foundation.hpp>
#include "UIKitPrivate.hpp"

#include <objc/message.h>

namespace UI
{
    class Scene;
    class SceneConnectionOptions;
    class SceneSession;
    class Screen;
    class Window;

    using SceneSessionRole = NS::String;

    class SceneDelegate {
        public:
            virtual void sceneWillConnectToSession( UI::Scene* scene, UI::SceneSession* session, UI::SceneConnectionOptions* options ) {};
    };

    class SceneConfiguration : public NS::Referencing< SceneConfiguration > {
        public:
            static SceneConfiguration* alloc();

            static SceneConfiguration *configuration( const NS::String *pName, const SceneSessionRole *pSessionRole );

            SceneConfiguration *init( const NS::String *pName, const SceneSessionRole *pSessionRole );

            const NS::String *name() const;
            const SceneSessionRole *role() const;
    };

    class SceneConnectionOptions : public NS::Referencing< SceneConnectionOptions > {
        public:
    };

    class Scene : public NS::Referencing< Scene > {
        public:
    };

    class SceneSession : public NS::Referencing< SceneSession > {
        public:
            const SceneSessionRole *role() const;
    };

    class WindowScene : public NS::Referencing< WindowScene, Scene > {
        public:
            UI::Screen *screen() const;

            NS::Array *windows() const;

            UI::Window *keyWindow();

            void setKeyWindow( UI::Window *window );
    };
}

_NS_INLINE UI::SceneConfiguration* UI::SceneConfiguration::alloc()
{
    return Object::alloc< UI::SceneConfiguration >( _UI_PRIVATE_CLS( UISceneConfiguration ) );
}

_NS_INLINE UI::SceneConfiguration *UI::SceneConfiguration::configuration(const NS::String *pName, const SceneSessionRole *pSessionRole)
{
    return Object::sendMessage< UI::SceneConfiguration* >( _UI_PRIVATE_CLS( UISceneConfiguration ), _UI_PRIVATE_SEL( configurationWithName_sessionRole_ ), pName, pSessionRole );
}

_NS_INLINE UI::SceneConfiguration* UI::SceneConfiguration::init(const NS::String *pName, const UI::SceneSessionRole *pSessionRole)
{
    return Object::sendMessage< UI::SceneConfiguration* >( this, _UI_PRIVATE_SEL( initWithName_sessionRole_ ), pName, pSessionRole );
}

_NS_INLINE const UI::SceneSessionRole* UI::SceneSession::role() const
{
    return Object::sendMessage< UI::SceneSessionRole* >( this, _UI_PRIVATE_SEL( role ) );
}

_NS_INLINE UI::Screen* UI::WindowScene::screen() const
{
    return Object::sendMessage< UI::Screen* >(this, _UI_PRIVATE_SEL( screen ) );
}

_NS_INLINE NS::Array* UI::WindowScene::windows() const
{
    return Object::sendMessage< NS::Array* >(this, _UI_PRIVATE_SEL( windows ) );
}

UI::Window *UI::WindowScene::keyWindow()
{
    return Object::sendMessage< UI::Window* >( this, _UI_PRIVATE_SEL( keyWindow ) );
}

void UI::WindowScene::setKeyWindow( UI::Window *pWindow )
{
    Object::sendMessage< UI::Window* >( this, _UI_PRIVATE_SEL( setKeyWindow_ ), pWindow );
}

#define UI_SCENE_DELEGATE_ASSOCIATED_OBJECT_KEY "metal-cpp.UISceneDelegate.passthrough"

// Call this macro once in your implementation file to make a C++ UI::SceneDelegate implementation available to the Obj-C runtime by the same name.
// Doing so will allow it to be referenced declaratively by your Application Scene Manifest and instantiated as part of the scene lifecycle.
#define UI_DEF_SCENE_DELEGATE(type)                                                                                                     \
static Class sk_ ## type;                                                                                                               \
__attribute__((constructor))                                                                                                            \
static void StaticInitUISceneDelegateClass_ ## type ()                                                                                  \
{                                                                                                                                       \
    sk_ ## type = objc_allocateClassPair(objc_getClass("NSObject"), #type, 0);                                                          \
    class_addProtocol(sk_ ## type, objc_getProtocol("UISceneDelegate"));                                                                \
    typedef void* (*InitFunction)( void *, SEL );                                                                                       \
    typedef void (*SceneWillConnectToSessionFunction)( void*, SEL, void* scene, void* session, void* options );                         \
    typedef void (*DeallocFunction)( void *, SEL );                                                                                     \
    InitFunction init = []( void *pDelegate, SEL sel ) {                                                                                \
        struct objc_super super = {                                                                                                     \
            .receiver = (objc_object *)pDelegate,                                                                                       \
            .super_class = objc_lookUpClass("NSObject")                                                                                 \
        };                                                                                                                              \
        id me = ((id(*)(struct objc_super *, SEL, va_list))objc_msgSendSuper)(&super, sel, NULL);                                       \
        auto passthrough = new type();                                                                                                  \
        objc_setAssociatedObject(me, UI_SCENE_DELEGATE_ASSOCIATED_OBJECT_KEY, (id)passthrough, OBJC_ASSOCIATION_ASSIGN);                \
        return (void *)me;                                                                                                              \
    };                                                                                                                                  \
    SceneWillConnectToSessionFunction sceneWillConnectToSession = []( void* callee, SEL, void* scene, void* session, void* options ) {  \
        auto pScene = reinterpret_cast< UI::Scene* >( scene );                                                                          \
        auto pSession = reinterpret_cast< UI::SceneSession* >( session );                                                               \
        auto pOptions = reinterpret_cast< UI::SceneConnectionOptions* >( options );                                                     \
        auto passthrough = objc_getAssociatedObject((objc_object *)callee, UI_SCENE_DELEGATE_ASSOCIATED_OBJECT_KEY);                    \
        auto pDel = reinterpret_cast< UI::SceneDelegate* >( passthrough );                                                              \
        if (pDel) {                                                                                                                     \
            pDel->sceneWillConnectToSession(pScene, pSession, pOptions);                                                                \
        }                                                                                                                               \
    };                                                                                                                                  \
    DeallocFunction dealloc = []( void *callee, SEL sel ) {                                                                             \
        auto passthrough = objc_getAssociatedObject((objc_object *)callee, UI_SCENE_DELEGATE_ASSOCIATED_OBJECT_KEY);                    \
        delete passthrough;                                                                                                             \
        objc_setAssociatedObject((id)callee, UI_SCENE_DELEGATE_ASSOCIATED_OBJECT_KEY, NULL, OBJC_ASSOCIATION_ASSIGN);                   \
        struct objc_super super = {                                                                                                     \
            .receiver = (objc_object *)callee,                                                                                          \
            .super_class = objc_lookUpClass("NSObject")                                                                                 \
        };                                                                                                                              \
        ((void(*)(struct objc_super *, SEL, va_list))objc_msgSendSuper)(&super, sel, NULL);                                             \
    };                                                                                                                                  \
    class_addMethod(sk_ ## type, sel_getUid("init"), (IMP)init, "@@:");                                                                 \
    class_addMethod(sk_ ## type, sel_getUid("scene:willConnectToSession:options:"), (IMP)sceneWillConnectToSession, "v@:@@@");          \
    class_addMethod(sk_ ## type, sel_getUid("dealloc"), (IMP)dealloc, "v@:");                                                           \
    objc_registerClassPair(sk_ ## type);                                                                                                \
}

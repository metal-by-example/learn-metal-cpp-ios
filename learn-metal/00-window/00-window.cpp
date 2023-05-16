/*
 *
 * Copyright 2022 Apple Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cassert>

#define NS_PRIVATE_IMPLEMENTATION
#define UI_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#define MTK_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#include <Metal/Metal.hpp>
#include <UIKit/UIKit.hpp>
#include <MetalKit/MetalKit.hpp>

#pragma region Declarations {

class Renderer
{
    public:
        Renderer( MTL::Device* pDevice );
        ~Renderer();
        void draw( MTK::View* pView );

    private:
        MTL::Device* _pDevice;
        MTL::CommandQueue* _pCommandQueue;
};

class MyMTKViewDelegate : public MTK::ViewDelegate
{
    public:
        MyMTKViewDelegate( MTL::Device* pDevice );
        virtual ~MyMTKViewDelegate() override;
        virtual void drawInMTKView( MTK::View* pView ) override;

    private:
        Renderer* _pRenderer;
};

class MyAppDelegate : public UI::ApplicationDelegate
{
    public:
        ~MyAppDelegate();

        bool applicationDidFinishLaunching( UI::Application *pApp, NS::Value *options ) override;
        void applicationWillTerminate( UI::Application *pApp ) override;

    private:
        UI::Window* _pWindow;
        UI::ViewController* _pViewController;
        MTK::View* _pMtkView;
        MTL::Device* _pDevice;
        MyMTKViewDelegate* _pViewDelegate = nullptr;
};

#pragma endregion Declarations }

int main( int argc, char* argv[] )
{
    NS::AutoreleasePool* pAutoreleasePool = NS::AutoreleasePool::alloc()->init();

    MyAppDelegate del;
    UI::ApplicationMain(argc, argv, &del);

    pAutoreleasePool->release();

    return 0;
}

#pragma mark - AppDelegate
#pragma region AppDelegate {

MyAppDelegate::~MyAppDelegate()
{
    _pMtkView->release();
    _pWindow->release();
    _pViewController->release();
    _pDevice->release();
    delete _pViewDelegate;
}

bool MyAppDelegate::applicationDidFinishLaunching( UI::Application *pApp, NS::Value *options )
{
    CGRect frame = UI::Screen::mainScreen()->bounds();

    _pWindow = UI::Window::alloc()->init(frame);

    _pViewController = UI::ViewController::alloc()->init( nil, nil );

    _pDevice = MTL::CreateSystemDefaultDevice();

    _pMtkView = MTK::View::alloc()->init( frame, _pDevice );
    _pMtkView->setColorPixelFormat( MTL::PixelFormat::PixelFormatBGRA8Unorm_sRGB );
    _pMtkView->setClearColor( MTL::ClearColor::Make( 1.0, 0.0, 0.0, 1.0 ) );

    _pViewDelegate = new MyMTKViewDelegate( _pDevice );
    _pMtkView->setDelegate( _pViewDelegate );

    UI::View *mtkView = (UI::View *)_pMtkView;
    mtkView->setAutoresizingMask( UI::ViewAutoresizingFlexibleWidth | UI::ViewAutoresizingFlexibleHeight );
    _pViewController->view()->addSubview( mtkView );
    _pWindow->setRootViewController( _pViewController );

    _pWindow->makeKeyAndVisible();

    return true;
}

void MyAppDelegate::applicationWillTerminate( UI::Application *pApp )
{
}

#pragma endregion AppDelegate }


#pragma mark - ViewDelegate
#pragma region ViewDelegate {

MyMTKViewDelegate::MyMTKViewDelegate( MTL::Device* pDevice )
: MTK::ViewDelegate()
, _pRenderer( new Renderer( pDevice ) )
{
}

MyMTKViewDelegate::~MyMTKViewDelegate()
{
    delete _pRenderer;
}

void MyMTKViewDelegate::drawInMTKView( MTK::View* pView )
{
    _pRenderer->draw( pView );
}

#pragma endregion ViewDelegate }


#pragma mark - Renderer
#pragma region Renderer {

Renderer::Renderer( MTL::Device* pDevice )
: _pDevice( pDevice->retain() )
{
    _pCommandQueue = _pDevice->newCommandQueue();
}

Renderer::~Renderer()
{
    _pCommandQueue->release();
    _pDevice->release();
}

void Renderer::draw( MTK::View* pView )
{
    NS::AutoreleasePool* pPool = NS::AutoreleasePool::alloc()->init();

    MTL::CommandBuffer* pCmd = _pCommandQueue->commandBuffer();
    MTL::RenderPassDescriptor* pRpd = pView->currentRenderPassDescriptor();
    MTL::RenderCommandEncoder* pEnc = pCmd->renderCommandEncoder( pRpd );
    pEnc->endEncoding();
    pCmd->presentDrawable( pView->currentDrawable() );
    pCmd->commit();

    pPool->release();
}

#pragma endregion Renderer }

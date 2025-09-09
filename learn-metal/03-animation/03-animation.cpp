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

#include <simd/simd.h>


#pragma region Declarations {

class Renderer
{
    public:
        Renderer( MTL::Device* pDevice );
        ~Renderer();
        void buildShaders();
        void buildBuffers();
        void buildFrameData();
        void draw( MTK::View* pView );

    private:
        MTL::Device* _pDevice;
        MTL::CommandQueue* _pCommandQueue;
        MTL::Library* _pShaderLibrary;
        MTL::RenderPipelineState* _pPSO;
        MTL::Buffer* _pArgBuffer;
        MTL::Buffer* _pVertexPositionsBuffer;
        MTL::Buffer* _pVertexColorsBuffer;
        MTL::Buffer* _pFrameData[3];
        float _angle;
        int _frame;
        dispatch_semaphore_t _semaphore;
        static const int kMaxFramesInFlight;
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
        bool applicationDidFinishLaunching( UI::Application *pApp, NS::Value *options ) override;
        void applicationWillTerminate( UI::Application *pApp ) override;
};

class MySceneDelegate: public UI::SceneDelegate
{
    public:
        ~MySceneDelegate();

        void sceneWillConnectToSession( UI::Scene *scene, UI::SceneSession *session, UI::SceneConnectionOptions *options ) override;

    private:
        UI::Window* _pWindow;
        UI::ViewController* _pViewController;
        MTK::View* _pMtkView;
        MTL::Device* _pDevice;
        MyMTKViewDelegate* _pViewDelegate = nullptr;
};

// Make this class available for runtime lookup in Obj-C.
UI_DEF_SCENE_DELEGATE(MySceneDelegate)

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

bool MyAppDelegate::applicationDidFinishLaunching( UI::Application *pApp, NS::Value *options )
{
    return true;
}

void MyAppDelegate::applicationWillTerminate( UI::Application *pApp )
{
}

#pragma endregion AppDelegate }

#pragma mark - SceneDelegate
#pragma region SceneDelegate {

MySceneDelegate::~MySceneDelegate()
{
    _pMtkView->release();
    _pWindow->release();
    _pViewController->release();
    _pDevice->release();
    delete _pViewDelegate;
}

void MySceneDelegate::sceneWillConnectToSession( UI::Scene *scene, UI::SceneSession *session, UI::SceneConnectionOptions *options )
{
    auto windowScene = reinterpret_cast<UI::WindowScene *>(scene);
    _pWindow = UI::Window::alloc()->init(windowScene);

    _pViewController = UI::ViewController::alloc()->init( nil, nil );

    _pDevice = MTL::CreateSystemDefaultDevice();

    CGRect frame = windowScene->screen()->bounds();
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
}

#pragma endregion SceneDelegate }

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

const int Renderer::kMaxFramesInFlight = 3;

Renderer::Renderer( MTL::Device* pDevice )
: _pDevice( pDevice->retain() )
, _angle ( 0.f )
, _frame( 0 )
{
    _pCommandQueue = _pDevice->newCommandQueue();
    buildShaders();
    buildBuffers();
    buildFrameData();

    _semaphore = dispatch_semaphore_create( Renderer::kMaxFramesInFlight );
}

Renderer::~Renderer()
{
    _pShaderLibrary->release();
    _pArgBuffer->release();
    _pVertexPositionsBuffer->release();
    _pVertexColorsBuffer->release();
    for ( int i = 0; i <  Renderer::kMaxFramesInFlight; ++i )
    {
        _pFrameData[i]->release();
    }
    _pPSO->release();
    _pCommandQueue->release();
    _pDevice->release();
}

void Renderer::buildShaders()
{
    using NS::StringEncoding::UTF8StringEncoding;

    const char* shaderSrc = R"(
        #include <metal_stdlib>
        using namespace metal;

        struct v2f
        {
            float4 position [[position]];
            half3 color;
        };

        struct VertexData
        {
            device float3* positions [[id(0)]];
            device float3* colors [[id(1)]];
        };

        struct FrameData
        {
            float angle;
        };

        v2f vertex vertexMain( device const VertexData* vertexData [[buffer(0)]], constant FrameData* frameData [[buffer(1)]], uint vertexId [[vertex_id]] )
        {
            float a = frameData->angle;
            float3x3 rotationMatrix = float3x3( sin(a), cos(a), 0.0, cos(a), -sin(a), 0.0, 0.0, 0.0, 1.0 );
            v2f o;
            o.position = float4( rotationMatrix * vertexData->positions[ vertexId ], 1.0 );
            o.color = half3(vertexData->colors[ vertexId ]);
            return o;
        }

        half4 fragment fragmentMain( v2f in [[stage_in]] )
        {
            return half4( in.color, 1.0 );
        }
    )";

    NS::Error* pError = nullptr;
    MTL::Library* pLibrary = _pDevice->newLibrary( NS::String::string(shaderSrc, UTF8StringEncoding), nullptr, &pError );
    if ( !pLibrary )
    {
        __builtin_printf( "%s", pError->localizedDescription()->utf8String() );
        assert( false );
    }

    MTL::Function* pVertexFn = pLibrary->newFunction( NS::String::string("vertexMain", UTF8StringEncoding) );
    MTL::Function* pFragFn = pLibrary->newFunction( NS::String::string("fragmentMain", UTF8StringEncoding) );

    MTL::RenderPipelineDescriptor* pDesc = MTL::RenderPipelineDescriptor::alloc()->init();
    pDesc->setVertexFunction( pVertexFn );
    pDesc->setFragmentFunction( pFragFn );
    pDesc->colorAttachments()->object(0)->setPixelFormat( MTL::PixelFormat::PixelFormatBGRA8Unorm_sRGB );

    _pPSO = _pDevice->newRenderPipelineState( pDesc, &pError );
    if ( !_pPSO )
    {
        __builtin_printf( "%s", pError->localizedDescription()->utf8String() );
        assert( false );
    }

    pVertexFn->release();
    pFragFn->release();
    pDesc->release();
    _pShaderLibrary = pLibrary;
}

void Renderer::buildBuffers()
{
    const size_t NumVertices = 3;

    simd::float3 positions[NumVertices] =
    {
        { -0.8f,  0.8f, 0.0f },
        {  0.0f, -0.8f, 0.0f },
        { +0.8f,  0.8f, 0.0f }
    };

    simd::float3 colors[NumVertices] =
    {
        {  1.0, 0.3f, 0.2f },
        {  0.8f, 1.0, 0.0f },
        {  0.8f, 0.0f, 1.0 }
    };

    const size_t positionsDataSize = NumVertices * sizeof( simd::float3 );
    const size_t colorDataSize = NumVertices * sizeof( simd::float3 );

    MTL::Buffer* pVertexPositionsBuffer = _pDevice->newBuffer( positionsDataSize, MTL::ResourceStorageModeShared );
    MTL::Buffer* pVertexColorsBuffer = _pDevice->newBuffer( colorDataSize, MTL::ResourceStorageModeShared );

    _pVertexPositionsBuffer = pVertexPositionsBuffer;
    _pVertexColorsBuffer = pVertexColorsBuffer;
    
    memcpy( _pVertexPositionsBuffer->contents(), positions, positionsDataSize );
    memcpy( _pVertexColorsBuffer->contents(), colors, colorDataSize );

    using NS::StringEncoding::UTF8StringEncoding;
    assert( _pShaderLibrary );

    MTL::Function* pVertexFn = _pShaderLibrary->newFunction( NS::String::string( "vertexMain", UTF8StringEncoding ) );
    MTL::ArgumentEncoder* pArgEncoder = pVertexFn->newArgumentEncoder( 0 );

    MTL::Buffer* pArgBuffer = _pDevice->newBuffer( pArgEncoder->encodedLength(), MTL::ResourceStorageModeShared );
    _pArgBuffer = pArgBuffer;

    pArgEncoder->setArgumentBuffer( _pArgBuffer, 0 );

    pArgEncoder->setBuffer( _pVertexPositionsBuffer, 0, 0 );
    pArgEncoder->setBuffer( _pVertexColorsBuffer, 0, 1 );

    pVertexFn->release();
    pArgEncoder->release();
}

struct FrameData
{
    float angle;
};

void Renderer::buildFrameData()
{
    for ( int i = 0; i < Renderer::kMaxFramesInFlight; ++i )
    {
        _pFrameData[ i ]= _pDevice->newBuffer( sizeof( FrameData ), MTL::ResourceStorageModeShared );
    }
}

void Renderer::draw( MTK::View* pView )
{
    NS::AutoreleasePool* pPool = NS::AutoreleasePool::alloc()->init();

    _frame = (_frame + 1) % Renderer::kMaxFramesInFlight;
    MTL::Buffer* pFrameDataBuffer = _pFrameData[ _frame ];

    MTL::CommandBuffer* pCmd = _pCommandQueue->commandBuffer();
    dispatch_semaphore_wait( _semaphore, DISPATCH_TIME_FOREVER );
    Renderer* pRenderer = this;
    pCmd->addCompletedHandler( ^void( MTL::CommandBuffer* pCmd ){
        dispatch_semaphore_signal( pRenderer->_semaphore );
    });

    reinterpret_cast< FrameData * >( pFrameDataBuffer->contents() )->angle = (_angle += 0.01f);

    MTL::RenderPassDescriptor* pRpd = pView->currentRenderPassDescriptor();
    MTL::RenderCommandEncoder* pEnc = pCmd->renderCommandEncoder( pRpd );

    pEnc->setRenderPipelineState( _pPSO );
    pEnc->setVertexBuffer( _pArgBuffer, 0, 0 );
    pEnc->useResource( _pVertexPositionsBuffer, MTL::ResourceUsageRead );
    pEnc->useResource( _pVertexColorsBuffer, MTL::ResourceUsageRead );

    pEnc->setVertexBuffer( pFrameDataBuffer, 0, 1 );
    pEnc->drawPrimitives( MTL::PrimitiveType::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3) );

    pEnc->endEncoding();
    pCmd->presentDrawable( pView->currentDrawable() );
    pCmd->commit();

    pPool->release();
}

#pragma endregion Renderer }

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

static constexpr size_t kNumInstances = 32;
static constexpr size_t kMaxFramesInFlight = 3;


#pragma region Declarations {

class Renderer
{
    public:
        Renderer( MTL::Device* pDevice );
        ~Renderer();
        void buildShaders();
        void buildBuffers();
        void draw( MTK::View* pView );

    private:
        MTL::Device* _pDevice;
        MTL::CommandQueue* _pCommandQueue;
        MTL::Library* _pShaderLibrary;
        MTL::RenderPipelineState* _pPSO;
        MTL::Buffer* _pVertexDataBuffer;
        MTL::Buffer* _pInstanceDataBuffer[kMaxFramesInFlight];
        MTL::Buffer* _pIndexBuffer;
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

    _semaphore = dispatch_semaphore_create( Renderer::kMaxFramesInFlight );
}

Renderer::~Renderer()
{
    _pShaderLibrary->release();
    _pVertexDataBuffer->release();
    for ( int i = 0; i < kMaxFramesInFlight; ++i )
    {
        _pInstanceDataBuffer[i]->release();
    }
    _pIndexBuffer->release();
    _pPSO->release();
    _pCommandQueue->release();
    _pDevice->release();
}

namespace shader_types
{
    struct InstanceData
    {
        simd::float4x4 instanceTransform;
        simd::float4 instanceColor;
    };
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
            float3 position;
        };

        struct InstanceData
        {
            float4x4 instanceTransform;
            float4 instanceColor;
        };

        v2f vertex vertexMain( device const VertexData* vertexData [[buffer(0)]],
                               device const InstanceData* instanceData [[buffer(1)]],
                               uint vertexId [[vertex_id]],
                               uint instanceId [[instance_id]] )
        {
            v2f o;
            float4 pos = float4( vertexData[ vertexId ].position, 1.0 );
            o.position = instanceData[ instanceId ].instanceTransform * pos;
            o.color = half3( instanceData[ instanceId ].instanceColor.rgb );
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
    using simd::float3;

    const float s = 0.5f;

    float3 verts[] = {
        { -s, -s, +s },
        { +s, -s, +s },
        { +s, +s, +s },
        { -s, +s, +s }
    };

    uint16_t indices[] = {
        0, 1, 2,
        2, 3, 0,
    };

    const size_t vertexDataSize = sizeof( verts );
    const size_t indexDataSize = sizeof( indices );

    MTL::Buffer* pVertexBuffer = _pDevice->newBuffer( vertexDataSize, MTL::ResourceStorageModeShared );
    MTL::Buffer* pIndexBuffer = _pDevice->newBuffer( indexDataSize, MTL::ResourceStorageModeShared );

    _pVertexDataBuffer = pVertexBuffer;
    _pIndexBuffer = pIndexBuffer;

    memcpy( _pVertexDataBuffer->contents(), verts, vertexDataSize );
    memcpy( _pIndexBuffer->contents(), indices, indexDataSize );

    const size_t instanceDataSize = kMaxFramesInFlight * kNumInstances * sizeof( shader_types::InstanceData );
    for ( size_t i = 0; i < kMaxFramesInFlight; ++i )
    {
        _pInstanceDataBuffer[ i ] = _pDevice->newBuffer( instanceDataSize, MTL::ResourceStorageModeShared );
    }
}

void Renderer::draw( MTK::View* pView )
{
    using simd::float4;
    using simd::float4x4;

    NS::AutoreleasePool* pPool = NS::AutoreleasePool::alloc()->init();

    _frame = (_frame + 1) % Renderer::kMaxFramesInFlight;
    MTL::Buffer* pInstanceDataBuffer = _pInstanceDataBuffer[ _frame ];

    MTL::CommandBuffer* pCmd = _pCommandQueue->commandBuffer();
    dispatch_semaphore_wait( _semaphore, DISPATCH_TIME_FOREVER );
    Renderer* pRenderer = this;
    pCmd->addCompletedHandler( ^void( MTL::CommandBuffer* pCmd ){
        dispatch_semaphore_signal( pRenderer->_semaphore );
    });

    _angle += 0.01f;

    const float scl = 0.1f;
    shader_types::InstanceData* pInstanceData = reinterpret_cast< shader_types::InstanceData *>( pInstanceDataBuffer->contents() );
    for ( size_t i = 0; i < kNumInstances; ++i )
    {
        float iDivNumInstances = i / (float)kNumInstances;
        float xoff = (iDivNumInstances * 2.0f - 1.0f) + (1.f/kNumInstances);
        float yoff = sin( ( iDivNumInstances + _angle ) * 2.0f * M_PI);
        pInstanceData[ i ].instanceTransform = (float4x4){ (float4){ scl * sinf(_angle), scl * cosf(_angle), 0.f, 0.f },
                                                           (float4){ scl * cosf(_angle), scl * -sinf(_angle), 0.f, 0.f },
                                                           (float4){ 0.f, 0.f, scl, 0.f },
                                                           (float4){ xoff, yoff, 0.f, 1.f } };

        float r = iDivNumInstances;
        float g = 1.0f - r;
        float b = sinf( M_PI * 2.0f * iDivNumInstances );
        pInstanceData[ i ].instanceColor = (float4){ r, g, b, 1.0f };
    }

    MTL::RenderPassDescriptor* pRpd = pView->currentRenderPassDescriptor();
    MTL::RenderCommandEncoder* pEnc = pCmd->renderCommandEncoder( pRpd );

    pEnc->setRenderPipelineState( _pPSO );
    pEnc->setVertexBuffer( _pVertexDataBuffer, /* offset */ 0, /* index */ 0 );
    pEnc->setVertexBuffer( pInstanceDataBuffer, /* offset */ 0, /* index */ 1 );

    //
    // void drawIndexedPrimitives( PrimitiveType primitiveType, NS::UInteger indexCount, IndexType indexType,
    //                             const class Buffer* pIndexBuffer, NS::UInteger indexBufferOffset, NS::UInteger instanceCount );
    pEnc->drawIndexedPrimitives( MTL::PrimitiveType::PrimitiveTypeTriangle,
                                6, MTL::IndexType::IndexTypeUInt16,
                                _pIndexBuffer,
                                0,
                                kNumInstances );

    pEnc->endEncoding();
    pCmd->presentDrawable( pView->currentDrawable() );
    pCmd->commit();

    pPool->release();
}

#pragma endregion Renderer }

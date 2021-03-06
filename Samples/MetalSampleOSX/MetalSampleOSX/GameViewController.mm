// This sample's assets are referenced from aether3d_build/Samples. Make sure that they exist.
// Assets can be downloaded from http://twiren.kapsi.fi/files/aether3d_sample_v0.6.zip
// If you didn't download a release of Aether3D, some referenced assets could be missing,
// just remove the references to build.

#import "GameViewController.h"
#import <Metal/Metal.h>
#import <simd/simd.h>
#import <MetalKit/MetalKit.h>
#include <cmath>
#include <vector>
#include <map>

#import "CameraComponent.hpp"
#import "SpriteRendererComponent.hpp"
#import "TextRendererComponent.hpp"
#import "DirectionalLightComponent.hpp"
#import "PointLightComponent.hpp"
#import "SpotLightComponent.hpp"
#import "SpriteRendererComponent.hpp"
#import "MeshRendererComponent.hpp"
#import "TransformComponent.hpp"
#import "System.hpp"
#import "Font.hpp"
#import "FileSystem.hpp"
#import "GameObject.hpp"
#import "Material.hpp"
#import "Mesh.hpp"
#import "Texture2D.hpp"
#import "TextureCube.hpp"
#import "Shader.hpp"
#import "Scene.hpp"
#import "Window.hpp"

#define TEST_FORWARD_PLUS
//#define TEST_SHADOWS_DIR
//#define TEST_SHADOWS_SPOT
//#define TEST_SHADOWS_POINT

#define MULTISAMPLE_COUNT 1

using namespace ae3d;

@implementation GameViewController
{
    MTKView* _view;
    id <MTLDevice> device;

    GameObject camera2d;
    GameObject camera3d;
    GameObject rotatingCube;
    GameObject bigCube;
    GameObject bigCube2;
    GameObject text;
    GameObject textSDF;
    GameObject dirLight;
    GameObject spotLight;
    GameObject pointLight;
    GameObject rtCamera;
    GameObject rtCube;
    GameObject renderTextureContainer;
    GameObject cubePTN; // vertex format: position, texcoord, normal
    GameObject cubePTN2;
    GameObject standardCubeBL; // bottom left
    GameObject standardCubeTL;
    GameObject standardCubeTL2;
    GameObject standardCubeTopCenter;
    GameObject standardCubeBR;
    GameObject standardCubeTR;
    GameObject spriteContainer;
    GameObject cameraCubeRT;
    Scene scene;
    Font font;
    Font fontSDF;
    Mesh cubeMesh;
    Mesh cubeMeshPTN;
    Material cubeMaterial;
    Material rtCubeMaterial;
    Material transMaterial;
    Material standardMaterial;
    Shader shader;
    Shader skyboxShader;
    Shader standardShader;
    Texture2D fontTex;
    Texture2D fontTexSDF;
    Texture2D gliderTex;
    Texture2D transTex;
    Texture2D bc1Tex;
    Texture2D bc2Tex;
    Texture2D bc3Tex;
    TextureCube skyTex;
    RenderTexture rtTex;
    RenderTexture cubeRT;
    std::vector< GameObject > sponzaGameObjects;
    std::map< std::string, Material* > sponzaMaterialNameToMaterial;
    std::map< std::string, Texture2D* > sponzaTextureNameToTexture;
    std::vector< Mesh* > sponzaMeshes;
}

- (void)viewDidLoad
{
    [super viewDidLoad];

    device = MTLCreateSystemDefaultDevice();
    assert( device );

    [self _setupView];
    [self _reshape];
    //self.nextResponder = super.nextResponder;

    ae3d::System::InitMetal( device, _view, MULTISAMPLE_COUNT );
    ae3d::System::LoadBuiltinAssets();
    //ae3d::System::InitAudio();

    // Sponza can be downloaded from http://twiren.kapsi.fi/files/aether3d_sponza.zip and extracted into aether3d_build/Samples
#if 0
    auto res = scene.Deserialize( FileSystem::FileContents( "sponza.scene" ), sponzaGameObjects, sponzaTextureNameToTexture,
                                 sponzaMaterialNameToMaterial, sponzaMeshes );

    if (res != Scene::DeserializeResult::Success)
    {
        System::Print( "Could not parse Sponza\n" );
    }

    for (auto& mat : sponzaMaterialNameToMaterial)
    {
        mat.second->SetShader( &shader );
    }

    for (std::size_t i = 0; i < sponzaGameObjects.size(); ++i)
    {
        scene.Add( &sponzaGameObjects[ i ] );
    }
#endif

    camera2d.SetName( "Camera2D" );
    camera2d.AddComponent<ae3d::CameraComponent>();
    camera2d.GetComponent<ae3d::CameraComponent>()->SetProjection( 0, self.view.bounds.size.width, self.view.bounds.size.height, 0, 0, 1 );
    camera2d.GetComponent<ae3d::CameraComponent>()->SetProjectionType( ae3d::CameraComponent::ProjectionType::Orthographic );
    camera2d.GetComponent<ae3d::CameraComponent>()->SetClearFlag( ae3d::CameraComponent::ClearFlag::Depth );
    camera2d.GetComponent<ae3d::CameraComponent>()->SetClearColor( ae3d::Vec3( 0.5f, 0.0f, 0.0f ) );
    camera2d.GetComponent<ae3d::CameraComponent>()->SetLayerMask( 0x2 );
    camera2d.GetComponent<ae3d::CameraComponent>()->SetRenderOrder( 2 );
    camera2d.AddComponent<ae3d::TransformComponent>();
    scene.Add( &camera2d );

    const float aspect = _view.bounds.size.width / (float)_view.bounds.size.height;

    camera3d.SetName( "Camera3D" );
    camera3d.AddComponent<ae3d::CameraComponent>();
    camera3d.GetComponent<ae3d::CameraComponent>()->SetProjection( 45, aspect, 1, 200 );
    camera3d.GetComponent<ae3d::CameraComponent>()->SetClearColor( ae3d::Vec3( 0.5f, 0.5f, 0.5f ) );
    camera3d.GetComponent<ae3d::CameraComponent>()->SetClearFlag( ae3d::CameraComponent::ClearFlag::DepthAndColor );
    camera3d.GetComponent<ae3d::CameraComponent>()->SetProjectionType( ae3d::CameraComponent::ProjectionType::Perspective );
    camera3d.GetComponent<ae3d::CameraComponent>()->SetRenderOrder( 1 );
#ifdef TEST_FORWARD_PLUS
    camera3d.GetComponent<ae3d::CameraComponent>()->GetDepthNormalsTexture().Create2D( self.view.bounds.size.width, self.view.bounds.size.height, ae3d::RenderTexture::DataType::Float, ae3d::TextureWrap::Clamp, ae3d::TextureFilter::Nearest );
#endif
    camera3d.AddComponent<ae3d::TransformComponent>();
    scene.Add( &camera3d );

    fontTex.Load( ae3d::FileSystem::FileContents( "/font.png" ), ae3d::TextureWrap::Repeat, ae3d::TextureFilter::Nearest, ae3d::Mipmaps::None, ae3d::ColorSpace::RGB, ae3d::Anisotropy::k1 );
    // TODO: SDF texture
    fontTexSDF.Load( ae3d::FileSystem::FileContents( "/font.png" ), ae3d::TextureWrap::Repeat, ae3d::TextureFilter::Nearest, ae3d::Mipmaps::None, ae3d::ColorSpace::RGB, ae3d::Anisotropy::k1 );
    gliderTex.Load( ae3d::FileSystem::FileContents( "/glider.png" ), ae3d::TextureWrap::Repeat, ae3d::TextureFilter::Linear, ae3d::Mipmaps::None, ae3d::ColorSpace::RGB, ae3d::Anisotropy::k1 );
    bc1Tex.Load( ae3d::FileSystem::FileContents( "/test_dxt1.dds" ), ae3d::TextureWrap::Repeat, ae3d::TextureFilter::Nearest, ae3d::Mipmaps::None, ae3d::ColorSpace::RGB, ae3d::Anisotropy::k1 );
    bc2Tex.Load( ae3d::FileSystem::FileContents( "/test_dxt3.dds" ), ae3d::TextureWrap::Repeat, ae3d::TextureFilter::Nearest, ae3d::Mipmaps::None, ae3d::ColorSpace::RGB, ae3d::Anisotropy::k1 );
    bc3Tex.Load( ae3d::FileSystem::FileContents( "/test_dxt5.dds" ), ae3d::TextureWrap::Repeat, ae3d::TextureFilter::Nearest, ae3d::Mipmaps::None, ae3d::ColorSpace::RGB, ae3d::Anisotropy::k1 );
    skyTex.Load( ae3d::FileSystem::FileContents( "/left.jpg" ), ae3d::FileSystem::FileContents( "/right.jpg" ),
                ae3d::FileSystem::FileContents( "/bottom.jpg" ), ae3d::FileSystem::FileContents( "/top.jpg" ),
                ae3d::FileSystem::FileContents( "/front.jpg" ), ae3d::FileSystem::FileContents( "/back.jpg" ),
                ae3d::TextureWrap::Clamp, ae3d::TextureFilter::Linear, ae3d::Mipmaps::Generate, ae3d::ColorSpace::RGB );
    scene.SetSkybox( &skyTex );
    
    font.LoadBMFont( &fontTex, ae3d::FileSystem::FileContents( "/font_txt.fnt" ) );
    fontSDF.LoadBMFont( &fontTexSDF, ae3d::FileSystem::FileContents( "/font_txt.fnt" ) );

    text.AddComponent<ae3d::TextRendererComponent>();
    text.GetComponent<ae3d::TextRendererComponent>()->SetText( "Aether3D Game Engine" );
    text.GetComponent<ae3d::TextRendererComponent>()->SetFont( &font );
    text.GetComponent<ae3d::TextRendererComponent>()->SetColor( ae3d::Vec4( 0, 1, 0, 1 ) );
    text.AddComponent<ae3d::TransformComponent>();
    text.GetComponent<ae3d::TransformComponent>()->SetLocalPosition( ae3d::Vec3( 5, 5, 0 ) );
    text.SetLayer( 2 );
    scene.Add( &text );

    textSDF.AddComponent<ae3d::TextRendererComponent>();
    textSDF.GetComponent<ae3d::TextRendererComponent>()->SetText( "This is SDF text" );
    textSDF.GetComponent<ae3d::TextRendererComponent>()->SetFont( &fontSDF );
    textSDF.GetComponent<ae3d::TextRendererComponent>()->SetShader( ae3d::TextRendererComponent::ShaderType::SDF );
    textSDF.GetComponent<ae3d::TextRendererComponent>()->SetColor( ae3d::Vec4( 0, 1, 0, 1 ) );
    textSDF.AddComponent<ae3d::TransformComponent>();
    textSDF.GetComponent<ae3d::TransformComponent>()->SetLocalPosition( ae3d::Vec3( 450, 5, 0 ) );
    textSDF.SetLayer( 2 );
    scene.Add( &textSDF );

    spriteContainer.AddComponent<ae3d::SpriteRendererComponent>();
    auto sprite = spriteContainer.GetComponent<SpriteRendererComponent>();
    sprite->SetTexture( &bc1Tex, Vec3( 120, 100, -0.6f ), Vec3( (float)bc1Tex.GetWidth(), (float)bc1Tex.GetHeight(), 1 ), Vec4( 1, 1, 1, 0.5f ) );
    sprite->SetTexture( &bc2Tex, Vec3( 120, 200, -0.5f ), Vec3( (float)bc2Tex.GetWidth(), (float)bc2Tex.GetHeight(), 1 ), Vec4( 1, 1, 1, 0.5f ) );
    sprite->SetTexture( &bc3Tex, Vec3( 120, 300, -0.5f ), Vec3( (float)bc3Tex.GetWidth(), (float)bc3Tex.GetHeight(), 1 ), Vec4( 1, 1, 1, 0.5f ) );
    sprite->SetTexture( &gliderTex, Vec3( 220, 120, -0.5f ), Vec3( (float)gliderTex.GetWidth(), (float)gliderTex.GetHeight(), 1 ), Vec4( 1, 1, 1, 1 ) );
    
    spriteContainer.AddComponent<TransformComponent>();
    //spriteContainer.GetComponent<TransformComponent>()->SetLocalPosition( Vec3( 20, 0, 0 ) );
    spriteContainer.GetComponent<ae3d::TransformComponent>()->SetLocalPosition( ae3d::Vec3( 5, 5, 0 ) );
    spriteContainer.SetLayer( 2 );
    scene.Add( &spriteContainer );

    shader.Load( ae3d::FileSystem::FileContents( "" ), ae3d::FileSystem::FileContents( "" ),
                "unlit_vertex", "unlit_fragment",
                ae3d::FileSystem::FileContents(""), ae3d::FileSystem::FileContents( "" ),
                ae3d::FileSystem::FileContents(""), ae3d::FileSystem::FileContents( "" ));
    
    cubeMaterial.SetShader( &shader );
    cubeMaterial.SetTexture( "textureMap", &gliderTex );
    //cubeMaterial.SetRenderTexture( "textureMap", &camera3d.GetComponent<ae3d::CameraComponent>()->GetDepthNormalsTexture() );
    cubeMaterial.SetVector( "tint", { 1, 1, 1, 1 } );

    rtCubeMaterial.SetShader( &skyboxShader );
    rtCubeMaterial.SetRenderTexture( "texture", &cubeRT );
    rtCubeMaterial.SetBackFaceCulling( false );
    
    standardShader.Load( ae3d::FileSystem::FileContents( "" ), ae3d::FileSystem::FileContents( "" ),
                "standard_vertex", "standard_fragment",
                ae3d::FileSystem::FileContents(""), ae3d::FileSystem::FileContents( "" ),
                ae3d::FileSystem::FileContents(""), ae3d::FileSystem::FileContents( "" ));
    standardMaterial.SetShader( &standardShader );
    standardMaterial.SetTexture( "textureMap", &gliderTex );

    skyboxShader.Load( ae3d::FileSystem::FileContents( "" ), ae3d::FileSystem::FileContents( "" ),
                        "skybox_vertex", "skybox_fragment",
                        ae3d::FileSystem::FileContents(""), ae3d::FileSystem::FileContents( "" ),
                        ae3d::FileSystem::FileContents(""), ae3d::FileSystem::FileContents( "" ));

    cubeMesh.Load( ae3d::FileSystem::FileContents( "/textured_cube.ae3d" ) );
    rotatingCube.AddComponent<ae3d::MeshRendererComponent>();
    rotatingCube.GetComponent<ae3d::MeshRendererComponent>()->SetMesh( &cubeMesh );
    rotatingCube.GetComponent<ae3d::MeshRendererComponent>()->SetMaterial( &cubeMaterial, 0 );
    rotatingCube.AddComponent<ae3d::TransformComponent>();
    rotatingCube.GetComponent<ae3d::TransformComponent>()->SetLocalPosition( ae3d::Vec3( 4, 0, -10 ) );
    rotatingCube.GetComponent<ae3d::TransformComponent>()->SetLocalScale( 1 );
    scene.Add( &rotatingCube );

    rtCube.AddComponent<ae3d::MeshRendererComponent>();
    rtCube.GetComponent<ae3d::MeshRendererComponent>()->SetMesh( &cubeMesh );
    rtCube.GetComponent<ae3d::MeshRendererComponent>()->SetMaterial( &rtCubeMaterial, 0 );
    rtCube.AddComponent<ae3d::TransformComponent>();
    rtCube.GetComponent<ae3d::TransformComponent>()->SetLocalPosition( ae3d::Vec3( -4, 0, -10 ) );
    rtCube.GetComponent<ae3d::TransformComponent>()->SetLocalScale( 1 );
    //scene.Add( &rtCube );

    standardCubeBL.AddComponent<ae3d::MeshRendererComponent>();
    standardCubeBL.GetComponent<ae3d::MeshRendererComponent>()->SetMesh( &cubeMesh );
    standardCubeBL.GetComponent<ae3d::MeshRendererComponent>()->SetMaterial( &standardMaterial, 0 );
    standardCubeBL.AddComponent<ae3d::TransformComponent>();
    standardCubeBL.GetComponent<ae3d::TransformComponent>()->SetLocalPosition( ae3d::Vec3( -4, -4, -10 ) );

    standardCubeTL.AddComponent<ae3d::MeshRendererComponent>();
    standardCubeTL.GetComponent<ae3d::MeshRendererComponent>()->SetMesh( &cubeMesh );
    standardCubeTL.GetComponent<ae3d::MeshRendererComponent>()->SetMaterial( &standardMaterial, 0 );
    standardCubeTL.AddComponent<ae3d::TransformComponent>();
    standardCubeTL.GetComponent<ae3d::TransformComponent>()->SetLocalPosition( ae3d::Vec3( -4, 4, -10 ) );

    standardCubeTL2.AddComponent<ae3d::MeshRendererComponent>();
    standardCubeTL2.GetComponent<ae3d::MeshRendererComponent>()->SetMesh( &cubeMesh );
    standardCubeTL2.GetComponent<ae3d::MeshRendererComponent>()->SetMaterial( &standardMaterial, 0 );
    standardCubeTL2.AddComponent<ae3d::TransformComponent>();
    standardCubeTL2.GetComponent<ae3d::TransformComponent>()->SetLocalPosition( ae3d::Vec3( -2, 4, -10 ) );

    standardCubeTopCenter.AddComponent<ae3d::MeshRendererComponent>();
    standardCubeTopCenter.GetComponent<ae3d::MeshRendererComponent>()->SetMesh( &cubeMesh );
    standardCubeTopCenter.GetComponent<ae3d::MeshRendererComponent>()->SetMaterial( &standardMaterial, 0 );
    standardCubeTopCenter.AddComponent<ae3d::TransformComponent>();
    standardCubeTopCenter.GetComponent<ae3d::TransformComponent>()->SetLocalPosition( ae3d::Vec3( 0, 0, -14 ) );
    standardCubeTopCenter.GetComponent<ae3d::TransformComponent>()->SetLocalScale( 2 );

    standardCubeTR.AddComponent<ae3d::MeshRendererComponent>();
    standardCubeTR.GetComponent<ae3d::MeshRendererComponent>()->SetMesh( &cubeMesh );
    standardCubeTR.GetComponent<ae3d::MeshRendererComponent>()->SetMaterial( &standardMaterial, 0 );
    standardCubeTR.AddComponent<ae3d::TransformComponent>();
    standardCubeTR.GetComponent<ae3d::TransformComponent>()->SetLocalPosition( ae3d::Vec3( 4, 4, -10 ) );

    standardCubeBR.AddComponent<ae3d::MeshRendererComponent>();
    standardCubeBR.GetComponent<ae3d::MeshRendererComponent>()->SetMesh( &cubeMesh );
    standardCubeBR.GetComponent<ae3d::MeshRendererComponent>()->SetMaterial( &standardMaterial, 0 );
    standardCubeBR.AddComponent<ae3d::TransformComponent>();
    standardCubeBR.GetComponent<ae3d::TransformComponent>()->SetLocalPosition( ae3d::Vec3( 6, -4, -10 ) );
    standardCubeBR.GetComponent<ae3d::TransformComponent>()->SetLocalPosition( ae3d::Vec3( 2, 0, -10 ) );

#ifdef TEST_FORWARD_PLUS
    //scene.Add( &standardCubeBR );
    //scene.Add( &standardCubeBL );
    //scene.Add( &standardCubeTR );
    //scene.Add( &standardCubeTL2 );
    scene.Add( &standardCubeTopCenter );
    //scene.Add( &standardCubeTL );
#endif

    cubeMeshPTN.Load( ae3d::FileSystem::FileContents( "/textured_cube_ptn.ae3d" ) );
    cubePTN.AddComponent<ae3d::MeshRendererComponent>();
    cubePTN.GetComponent<ae3d::MeshRendererComponent>()->SetMesh( &cubeMeshPTN );
    cubePTN.GetComponent<ae3d::MeshRendererComponent>()->SetMaterial( &cubeMaterial, 0 );
    cubePTN.AddComponent<ae3d::TransformComponent>();
    cubePTN.GetComponent<ae3d::TransformComponent>()->SetLocalPosition( ae3d::Vec3( 3, 0, -10 ) );
    //scene.Add( &cubePTN );

    cubePTN2.AddComponent<ae3d::MeshRendererComponent>();
    cubePTN2.GetComponent<ae3d::MeshRendererComponent>()->SetMesh( &cubeMesh/*PTN*/ );
    cubePTN2.GetComponent<ae3d::MeshRendererComponent>()->SetMaterial( &cubeMaterial, 0 );
    cubePTN2.AddComponent<ae3d::TransformComponent>();
    cubePTN2.GetComponent<ae3d::TransformComponent>()->SetLocalPosition( ae3d::Vec3( 6, 5, -10 ) );
    //scene.Add( &cubePTN2 );

    bigCube.AddComponent<ae3d::MeshRendererComponent>();
    bigCube.GetComponent<ae3d::MeshRendererComponent>()->SetMesh( &cubeMesh );
    bigCube.GetComponent<ae3d::MeshRendererComponent>()->SetMaterial( &cubeMaterial, 0 );
    bigCube.AddComponent<ae3d::TransformComponent>();
    bigCube.GetComponent<ae3d::TransformComponent>()->SetLocalPosition( ae3d::Vec3( 0, -8, -10 ) );
    bigCube.GetComponent<ae3d::TransformComponent>()->SetLocalScale( 5 );
    scene.Add( &bigCube );

    bigCube2.AddComponent<ae3d::MeshRendererComponent>();
    bigCube2.GetComponent<ae3d::MeshRendererComponent>()->SetMesh( &cubeMesh );
    bigCube2.GetComponent<ae3d::MeshRendererComponent>()->SetMaterial( &cubeMaterial, 0 );
    bigCube2.AddComponent<ae3d::TransformComponent>();
    bigCube2.GetComponent<ae3d::TransformComponent>()->SetLocalPosition( ae3d::Vec3( 0, 2, -20 ) );
    bigCube2.GetComponent<ae3d::TransformComponent>()->SetLocalScale( 5 );
    //scene.Add( &bigCube2 );

    dirLight.AddComponent<ae3d::DirectionalLightComponent>();
#ifdef TEST_SHADOWS_DIR
    dirLight.GetComponent<ae3d::DirectionalLightComponent>()->SetCastShadow( true, 1024 );
#endif
    dirLight.AddComponent<ae3d::TransformComponent>();
    dirLight.GetComponent<ae3d::TransformComponent>()->LookAt( { 0, 0, 0 }, ae3d::Vec3( 0, -1, 0 ), { 0, 1, 0 } );
    scene.Add( &dirLight );

    spotLight.AddComponent<ae3d::SpotLightComponent>();
#ifdef TEST_SHADOWS_SPOT
    spotLight.GetComponent<ae3d::SpotLightComponent>()->SetCastShadow( true, 1024 );
#endif
    spotLight.AddComponent<ae3d::TransformComponent>();
    spotLight.GetComponent<ae3d::TransformComponent>()->LookAt( { 0, 8, -10 }, ae3d::Vec3( 0, -1, 0 ), { 0, 1, 0 } );
    scene.Add( &spotLight );

    pointLight.AddComponent<ae3d::PointLightComponent>();
#ifdef TEST_SHADOWS_POINT
    pointLight.GetComponent<ae3d::PointLightComponent>()->SetCastShadow( true, 1024 );
#endif
    pointLight.GetComponent<ae3d::PointLightComponent>()->SetRadius( 0.5f );
    pointLight.AddComponent<ae3d::TransformComponent>();
    pointLight.GetComponent<ae3d::TransformComponent>()->SetLocalPosition( ae3d::Vec3( 0, 0, -7 ) );
    scene.Add( &pointLight );

    rtTex.Create2D( 512, 512, ae3d::RenderTexture::DataType::UByte, ae3d::TextureWrap::Clamp, ae3d::TextureFilter::Linear );
    
    renderTextureContainer.AddComponent<ae3d::SpriteRendererComponent>();
    //renderTextureContainer.GetComponent<ae3d::SpriteRendererComponent>()->SetTexture( &rtTex, ae3d::Vec3( 250, 150, -0.6f ), ae3d::Vec3( 256, 256, 1 ), ae3d::Vec4( 1, 1, 1, 1 ) );
    renderTextureContainer.GetComponent<ae3d::SpriteRendererComponent>()->SetTexture( &camera3d.GetComponent<ae3d::CameraComponent>()->GetDepthNormalsTexture(), ae3d::Vec3( 50, 100, -0.6f ), ae3d::Vec3( 768*2, 512*2, 1 ), ae3d::Vec4( 1, 1, 1, 1 ) );
    //renderTextureContainer.GetComponent<ae3d::SpriteRendererComponent>()->SetTexture( dirLight.GetComponent<ae3d::DirectionalLightComponent>()->GetShadowMap(), ae3d::Vec3( 250, 150, -0.6f ), ae3d::Vec3( 512, 512, 1 ), ae3d::Vec4( 1, 1, 1, 1 ) );
    renderTextureContainer.SetLayer( 2 );
    //scene.Add( &renderTextureContainer );
    
    rtCamera.AddComponent<ae3d::CameraComponent>();
    rtCamera.GetComponent<ae3d::CameraComponent>()->SetProjection( 45, 4.0f / 3.0f, 1, 200 );
    rtCamera.GetComponent<ae3d::CameraComponent>()->SetProjectionType( ae3d::CameraComponent::ProjectionType::Perspective );
    rtCamera.GetComponent<ae3d::CameraComponent>()->SetClearFlag( ae3d::CameraComponent::ClearFlag::DepthAndColor );
    rtCamera.GetComponent<ae3d::CameraComponent>()->SetClearColor( ae3d::Vec3( 0.5f, 0, 0 ) );
    rtCamera.GetComponent<ae3d::CameraComponent>()->SetTargetTexture( &rtTex );
    rtCamera.AddComponent<ae3d::TransformComponent>();
    rtCamera.GetComponent<ae3d::TransformComponent>()->SetLocalPosition( ae3d::Vec3( 5, 5, 20 ) );
    //scene.Add( &rtCamera );

    cubeRT.CreateCube( 512, ae3d::RenderTexture::DataType::UByte, ae3d::TextureWrap::Repeat, ae3d::TextureFilter::Linear );

    cameraCubeRT.AddComponent<CameraComponent>();
    cameraCubeRT.GetComponent<CameraComponent>()->SetClearColor( Vec3( 0, 0, 1 ) );
    cameraCubeRT.GetComponent<CameraComponent>()->SetProjectionType( CameraComponent::ProjectionType::Perspective );
    cameraCubeRT.GetComponent<CameraComponent>()->SetProjection( 45, 1, 1, 400 );
    cameraCubeRT.GetComponent<CameraComponent>()->SetTargetTexture( &cubeRT );
    cameraCubeRT.GetComponent<CameraComponent>()->SetClearFlag( CameraComponent::ClearFlag::DepthAndColor );
    cameraCubeRT.AddComponent<TransformComponent>();
    cameraCubeRT.GetComponent<TransformComponent>()->LookAt( { 5, 0, -70 }, { 0, 0, -100 }, { 0, 1, 0 } );
    //scene.Add( &cameraCubeRT );

    transTex.Load( ae3d::FileSystem::FileContents( "/font.png" ), ae3d::TextureWrap::Repeat, ae3d::TextureFilter::Linear, ae3d::Mipmaps::None,
                  ae3d::ColorSpace::SRGB, ae3d::Anisotropy::k1 );
    
    transMaterial.SetShader( &shader );
    transMaterial.SetTexture( "textureMap", &transTex );
    transMaterial.SetVector( "tintColor", { 1, 0, 0, 1 } );

    transMaterial.SetBackFaceCulling( true );
    //transMaterial.SetBlendingMode( ae3d::Material::BlendingMode::Alpha );
    //rotatingCube.GetComponent<ae3d::MeshRendererComponent>()->SetMaterial( &transMaterial, 0 );
}

- (void)_setupView
{
    _view = (MTKView *)self.view;
    _view.delegate = self;
    _view.device = device;
    //_view.depthStencilPixelFormat = MTLPixelFormatDepth32Float;
    _view.sampleCount = MULTISAMPLE_COUNT;
}

- (void)keyDown:(NSEvent *)theEvent
{
    NSLog(@"onKeyDown Detected");
}

- (void)mouseDown:(NSEvent *)theEvent
{
    NSLog(@"mouseDown Detected");
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (void)_render
{
    [self _update];
    
    ae3d::System::SetCurrentDrawableMetal( _view.currentDrawable, _view.currentRenderPassDescriptor );
    ae3d::System::BeginFrame();
    scene.Render();
    ae3d::System::EndFrame();
}

- (void)_reshape
{
}

- (void)_update
{
    static int angle = 0;
    ++angle;
    
    ae3d::Quaternion rotation;
    rotation = ae3d::Quaternion::FromEuler( ae3d::Vec3( angle, angle, angle ) );
    rotatingCube.GetComponent< ae3d::TransformComponent >()->SetLocalRotation( rotation );
    
    //std::string drawCalls = std::string( "draw calls:" ) + std::to_string( ae3d::System::Statistics::GetDrawCallCount() );
    std::string stats = ae3d::System::Statistics::GetStatistics();
    text.GetComponent<ae3d::TextRendererComponent>()->SetText( stats.c_str() );
    
    // Testing vertex buffer growing
    if (angle == 5)
    {
        text.GetComponent<ae3d::TextRendererComponent>()->SetText( "this is a long string. this is a long string" );
    }

    pointLight.GetComponent<ae3d::TransformComponent>()->SetLocalPosition( ae3d::Vec3( std::sin( angle / 2 ) * 2, 0, -7 ) );
}

- (void)mtkView:(nonnull MTKView *)view drawableSizeWillChange:(CGSize)size
{
    [self _reshape];
}

- (void)drawInMTKView:(nonnull MTKView *)view
{
    @autoreleasepool {
        [self _render];
    }
}

@end

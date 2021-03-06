#include "MeshRendererComponent.hpp"
#include <vector>
#include "Frustum.hpp"
#include "Matrix.hpp"
#include "Mesh.hpp"
#include "GfxDevice.hpp"
#include "Material.hpp"
#include "VertexBuffer.hpp"
#include "Shader.hpp"
#include "System.hpp"
#include "SubMesh.hpp"
#include "Vec3.hpp"

using namespace ae3d;

namespace MathUtil
{
    void GetMinMax( const std::vector< Vec3 >& aPoints, Vec3& outMin, Vec3& outMax );
    void GetCorners( const Vec3& min, const Vec3& max, std::vector< Vec3 >& outCorners );
}

std::vector< ae3d::MeshRendererComponent > meshRendererComponents;
unsigned nextFreeMeshRendererComponent = 0;

unsigned ae3d::MeshRendererComponent::New()
{
    if (nextFreeMeshRendererComponent == meshRendererComponents.size())
    {
        meshRendererComponents.resize( meshRendererComponents.size() + 10 );
    }
    
    return nextFreeMeshRendererComponent++;
}

ae3d::MeshRendererComponent* ae3d::MeshRendererComponent::Get( unsigned index )
{
    return &meshRendererComponents[ index ];
}

std::string ae3d::MeshRendererComponent::GetSerialized() const
{
    return "meshrenderer\n";
}

void ae3d::MeshRendererComponent::Cull( const class Frustum& cameraFrustum, const struct Matrix44& localToWorld )
{
    if (!mesh)
    {
        return;
    }

    isCulled = false;
    
    std::vector< Vec3 > aabbWorld;
    MathUtil::GetCorners( mesh->GetAABBMin(), mesh->GetAABBMax(), aabbWorld );
    
    for (std::size_t v = 0; v < aabbWorld.size(); ++v)
    {
        Matrix44::TransformPoint( aabbWorld[ v ], localToWorld, &aabbWorld[ v ] );
    }
    
    Vec3 aabbMinWorld, aabbMaxWorld;
    MathUtil::GetMinMax( aabbWorld, aabbMinWorld, aabbMaxWorld );
    
    if (!cameraFrustum.BoxInFrustum( aabbMinWorld, aabbMaxWorld ))
    {
        isCulled = true;
        return;
    }

    std::vector< SubMesh >& subMeshes = mesh->GetSubMeshes();

    for (std::size_t subMeshIndex = 0; subMeshIndex < subMeshes.size(); ++subMeshIndex)
    {
        isSubMeshCulled[ subMeshIndex ] = false;

        if (materials[ subMeshIndex ] == nullptr || !materials[ subMeshIndex ]->IsValidShader())
        {
            isSubMeshCulled[ subMeshIndex ] = true;
            continue;
        }
        
        Vec3 meshAabbMinWorld = subMeshes[ subMeshIndex ].aabbMin;
        Vec3 meshAabbMaxWorld = subMeshes[ subMeshIndex ].aabbMax;
        
        std::vector< Vec3 > meshAabbWorld;
        MathUtil::GetCorners( meshAabbMinWorld, meshAabbMaxWorld, meshAabbWorld );
        
        for (std::size_t v = 0; v < meshAabbWorld.size(); ++v)
        {
            Matrix44::TransformPoint( meshAabbWorld[ v ], localToWorld, &meshAabbWorld[ v ] );
        }
        
        MathUtil::GetMinMax( meshAabbWorld, meshAabbMinWorld, meshAabbMaxWorld );
        
        if (!cameraFrustum.BoxInFrustum( meshAabbMinWorld, meshAabbMaxWorld ))
        {
            isSubMeshCulled[ subMeshIndex ] = true;
        }
    }
}

void ae3d::MeshRendererComponent::Render( const Matrix44& modelView, const Matrix44& modelViewProjection, const Matrix44& localToWorld,
                                          const Matrix44& shadowView, const Matrix44& shadowProjection, Shader* overrideShader, RenderType renderType )
{
    if (isCulled || !mesh)
    {
        return;
    }
    
    std::vector< SubMesh >& subMeshes = mesh->GetSubMeshes();

    for (std::size_t subMeshIndex = 0; subMeshIndex < subMeshes.size(); ++subMeshIndex)
    {
        if (isSubMeshCulled[ subMeshIndex ])
        {
            continue;
        }
        
        Shader* shader = overrideShader ? overrideShader : materials[ subMeshIndex ]->GetShader();
        GfxDevice::CullMode cullMode = GfxDevice::CullMode::Back;
        GfxDevice::BlendMode blendMode = GfxDevice::BlendMode::Off;

        if (overrideShader)
        {
            shader->Use();
            shader->SetMatrix( "_ModelViewProjectionMatrix", &modelViewProjection.m[ 0 ] );
            shader->SetMatrix( "_ModelViewMatrix", &modelView.m[ 0 ] );
        }
        else
        {
            if (materials[ subMeshIndex ]->GetBlendingMode() != Material::BlendingMode::Off && renderType == RenderType::Opaque)
            {
                continue;
            }

            if (materials[ subMeshIndex ]->GetBlendingMode() == Material::BlendingMode::Off && renderType == RenderType::Transparent)
            {
                continue;
            }

            Matrix44 shadowTexProjMatrix = localToWorld;
            
            Matrix44::Multiply( shadowTexProjMatrix, shadowView, shadowTexProjMatrix );
            Matrix44::Multiply( shadowTexProjMatrix, shadowProjection, shadowTexProjMatrix );
            Matrix44::Multiply( shadowTexProjMatrix, Matrix44::bias, shadowTexProjMatrix );
#ifndef RENDERER_VULKAN
            // Disabled on Vulkan backend because uniform code is not complete and this would overwrite MVP.
            materials[ subMeshIndex ]->SetMatrix( "_ShadowProjectionMatrix", shadowTexProjMatrix );
            materials[ subMeshIndex ]->SetMatrix( "_ModelMatrix", localToWorld );
#endif
            materials[ subMeshIndex ]->SetMatrix( "_ModelViewProjectionMatrix", modelViewProjection );
            materials[ subMeshIndex ]->Apply();

            if (!materials[ subMeshIndex ]->IsBackFaceCulled())
            {
                cullMode = GfxDevice::CullMode::Off;
            }
            
            if (materials[ subMeshIndex ]->GetBlendingMode() == Material::BlendingMode::Alpha)
            {
                blendMode = GfxDevice::BlendMode::AlphaBlend;
            }
        }
        
        GfxDevice::DepthFunc depthFunc;
        
        if (materials[ subMeshIndex ]->GetDepthFunction() == Material::DepthFunction::LessOrEqualWriteOn)
        {
            depthFunc = GfxDevice::DepthFunc::LessOrEqualWriteOn;
        }
        else if (materials[ subMeshIndex ]->GetDepthFunction() == Material::DepthFunction::NoneWriteOff)
        {
            depthFunc = GfxDevice::DepthFunc::NoneWriteOff;
        }
        else
        {
            System::Assert( false, "material has unhandled depth function" );
            depthFunc = GfxDevice::DepthFunc::NoneWriteOff;
        }
        
        GfxDevice::Draw( subMeshes[ subMeshIndex ].vertexBuffer, 0, subMeshes[ subMeshIndex ].vertexBuffer.GetFaceCount() / 3,
                         *shader, blendMode, depthFunc, cullMode, isWireframe ? GfxDevice::FillMode::Wireframe : GfxDevice::FillMode::Solid );
    }
}

void ae3d::MeshRendererComponent::SetMaterial( Material* material, int subMeshIndex )
{
    if (subMeshIndex >= 0 && subMeshIndex < int( materials.size() ))
    {
        materials[ subMeshIndex ] = material;
    }
}

void ae3d::MeshRendererComponent::SetMesh( Mesh* aMesh )
{
    mesh = aMesh;

    if (mesh != nullptr)
    {
        materials.resize( mesh->GetSubMeshes().size() );
        isSubMeshCulled.resize( mesh->GetSubMeshes().size() );
    }
}

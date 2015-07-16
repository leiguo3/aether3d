#include "MeshRendererComponent.hpp"
#include <vector>
#include "Matrix.hpp"
#include "Mesh.hpp"
#include "GfxDevice.hpp"
#include "Material.hpp"
#include "VertexBuffer.hpp"
#include "SubMesh.hpp"

#include "Texture2D.hpp"

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
    return &meshRendererComponents[index];
}

void ae3d::MeshRendererComponent::Render( const Matrix44& modelViewProjection )
{
    std::vector< SubMesh >& subMeshes = mesh->GetSubMeshes();
    int subMeshIndex = 0;
    
    for (auto& subMesh : subMeshes)
    {
        if (materials[ subMeshIndex ] == nullptr)
        {
            continue;
        }
        
        // const ae3d::Texture2D* texture = ae3d::Texture2D::GetDefaultTexture();

        GfxDevice::SetBlendMode( ae3d::GfxDevice::BlendMode::Off );

        materials[ subMeshIndex ]->SetMatrix( "_ModelViewProjectionMatrix", modelViewProjection );
        materials[ subMeshIndex ]->Apply();
        
        subMesh.vertexBuffer.Bind();
        subMesh.vertexBuffer.Draw();

        ++subMeshIndex;
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
    }
}

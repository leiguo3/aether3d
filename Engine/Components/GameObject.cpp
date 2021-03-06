#include "GameObject.hpp"
#include <sstream>
#include "TransformComponent.hpp"
#include "MeshRendererComponent.hpp"
#include "CameraComponent.hpp"
#include "DirectionalLightComponent.hpp"
#include "AudioSourceComponent.hpp"
#include "SpriteRendererComponent.hpp"
#include "TextRendererComponent.hpp"
#include "SpotLightComponent.hpp"
#include "PointLightComponent.hpp"

using namespace ae3d;

unsigned ae3d::GameObject::GetNextComponentIndex()
{
    return nextFreeComponentIndex >= MaxComponents ? InvalidComponentIndex : nextFreeComponentIndex++;
}

ae3d::GameObject::GameObject( const GameObject& other )
{
    *this = other;
}

GameObject& ae3d::GameObject::operator=( const GameObject& go )
{
    name = go.name;
    
    for (unsigned i = 0; i < MaxComponents; ++i)
    {
        components[ i ].type = -1;
        components[ i ].handle = InvalidComponentIndex;
    }

    if (go.GetComponent< TransformComponent >())
    {
        AddComponent< TransformComponent >();
        *GetComponent< TransformComponent >() = *go.GetComponent< TransformComponent >();
    }

    if (go.GetComponent< MeshRendererComponent >())
    {
        AddComponent< MeshRendererComponent >();
        *GetComponent< MeshRendererComponent >() = *go.GetComponent< MeshRendererComponent >();
    }

    if (go.GetComponent< CameraComponent >())
    {
        AddComponent< CameraComponent >();
        *GetComponent< CameraComponent >() = *go.GetComponent< CameraComponent >();
    }

    if (go.GetComponent< DirectionalLightComponent >())
    {
        AddComponent< DirectionalLightComponent >();
        *GetComponent< DirectionalLightComponent >() = *go.GetComponent< DirectionalLightComponent >();
    }

    if (go.GetComponent< AudioSourceComponent >())
    {
        AddComponent< AudioSourceComponent >();
        *GetComponent< AudioSourceComponent >() = *go.GetComponent< AudioSourceComponent >();
    }

    if (go.GetComponent< SpriteRendererComponent >())
    {
        AddComponent< SpriteRendererComponent >();
        *GetComponent< SpriteRendererComponent >() = *go.GetComponent< SpriteRendererComponent >();
    }

    if (go.GetComponent< TextRendererComponent >())
    {
        AddComponent< TextRendererComponent >();
        *GetComponent< TextRendererComponent >() = *go.GetComponent< TextRendererComponent >();
    }

    if (go.GetComponent< SpotLightComponent >())
    {
        AddComponent< SpotLightComponent >();
        *GetComponent< SpotLightComponent >() = *go.GetComponent< SpotLightComponent >();
    }

    if (go.GetComponent< PointLightComponent >())
    {
        AddComponent< PointLightComponent >();
        *GetComponent< PointLightComponent >() = *go.GetComponent< PointLightComponent >();
    }

    return *this;
}

bool ae3d::GameObject::IsEnabled() const
{
    const TransformComponent* transform = GetComponent< TransformComponent >();

    while (transform)
    {
        if (transform->GetGameObject() && !transform->GetGameObject()->isEnabled)
        {
            return false;
        }
        
        transform = transform->GetParent();
    }
    
    return isEnabled;
}

std::string ae3d::GameObject::GetSerialized() const
{
    const std::string serializedName = name.empty() ? "empty" : name;
    std::stringstream outStream;
    outStream << "gameobject\n";
    outStream << "name " << serializedName << "\n";
    outStream << "layer " << layer << "\n";
    outStream << "enabled " << (isEnabled ? 1 : 0) << "\n\n";

    return outStream.str();
}

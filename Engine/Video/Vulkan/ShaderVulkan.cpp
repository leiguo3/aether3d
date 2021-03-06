#include "Shader.hpp"
#include "GfxDevice.hpp"
#include "FileSystem.hpp"
#include "Macros.hpp"
#include "Matrix.hpp"
#include "System.hpp"
#include "Texture2D.hpp"
#include "TextureCube.hpp"
#include "RenderTexture.hpp"
#include "Vec3.hpp"
#include <cstring>
#include <vector>

namespace GfxDeviceGlobal
{
    extern VkDevice device;
    extern VkImageView view0;
    extern ae3d::RenderTexture* renderTexture0;
    extern VkSampler sampler0;
}

namespace ae3d
{
    void GetMemoryType( std::uint32_t typeBits, VkFlags properties, std::uint32_t* typeIndex ); // Defined in GfxDeviceVulkan.cpp 
}

namespace ShaderGlobal
{
    std::vector< VkShaderModule > modulesToReleaseAtExit;
}

void ae3d::Shader::DestroyShaders()
{
    for (std::size_t moduleIndex = 0; moduleIndex < ShaderGlobal::modulesToReleaseAtExit.size(); ++moduleIndex)
    {
        vkDestroyShaderModule( GfxDeviceGlobal::device, ShaderGlobal::modulesToReleaseAtExit[ moduleIndex ], nullptr );
    }
}

void ae3d::Shader::Load( const char* /*vertexSourceGLSL*/, const char* /*fragmentSourceGLSL*/ )
{
}

void ae3d::Shader::LoadSPIRV( const FileSystem::FileContentsData& vertexData, const FileSystem::FileContentsData& fragmentData )
{
    System::Assert( GfxDeviceGlobal::device != VK_NULL_HANDLE, "device not initialized" );
    
    if (!vertexData.isLoaded || !fragmentData.isLoaded)
    {
        return;
    }

    // Vertex shader
    {
        VkShaderModuleCreateInfo moduleCreateInfo;
        moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        moduleCreateInfo.pNext = nullptr;
        moduleCreateInfo.codeSize = vertexData.data.size();
        moduleCreateInfo.pCode = (std::uint32_t*)vertexData.data.data();
        moduleCreateInfo.flags = 0;

        VkShaderModule shaderModule;
        VkResult err = vkCreateShaderModule( GfxDeviceGlobal::device, &moduleCreateInfo, nullptr, &shaderModule );
        AE3D_CHECK_VULKAN( err, "vkCreateShaderModule vertex" );
        ShaderGlobal::modulesToReleaseAtExit.push_back( shaderModule );

        vertexInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertexInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertexInfo.module = shaderModule;
        vertexInfo.pName = "main";
        vertexInfo.pNext = nullptr;
        vertexInfo.flags = 0;
        vertexInfo.pSpecializationInfo = nullptr;

        System::Assert( vertexInfo.module != VK_NULL_HANDLE, "vertex shader module not created" );
    }

    // Fragment shader
    {
        VkShaderModuleCreateInfo moduleCreateInfo;
        moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        moduleCreateInfo.pNext = nullptr;
        moduleCreateInfo.codeSize = fragmentData.data.size();
        moduleCreateInfo.pCode = (std::uint32_t*)fragmentData.data.data();
        moduleCreateInfo.flags = 0;

        VkShaderModule shaderModule;
        VkResult err = vkCreateShaderModule( GfxDeviceGlobal::device, &moduleCreateInfo, nullptr, &shaderModule );
        AE3D_CHECK_VULKAN( err, "vkCreateShaderModule vertex" );
        ShaderGlobal::modulesToReleaseAtExit.push_back( shaderModule );

        fragmentInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragmentInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragmentInfo.module = shaderModule;
        fragmentInfo.pName = "main";
        fragmentInfo.pNext = nullptr;
        fragmentInfo.flags = 0;
        fragmentInfo.pSpecializationInfo = nullptr;

        System::Assert( fragmentInfo.module != VK_NULL_HANDLE, "fragment shader module not created" );
    }
}

void ae3d::Shader::Load( const FileSystem::FileContentsData& /*vertexGLSL*/, const FileSystem::FileContentsData& /*fragmentGLSL*/,
    const char* /*metalVertexShaderName*/, const char* /*metalFragmentShaderName*/,
    const FileSystem::FileContentsData& /*vertexHLSL*/, const FileSystem::FileContentsData& /*fragmentHLSL*/,
    const FileSystem::FileContentsData& vertexDataSPIRV, const FileSystem::FileContentsData& fragmentDataSPIRV )
{
    LoadSPIRV( vertexDataSPIRV, fragmentDataSPIRV );
}

void ae3d::Shader::Use()
{
    System::Assert( IsValid(), "no valid shader" );
    GfxDevice::CreateNewUniformBuffer();
}

void ae3d::Shader::SetMatrix( const char* /*name*/, const float* matrix4x4 )
{
    System::Assert( GfxDevice::GetCurrentUbo() != nullptr, "null ubo" );
    std::memcpy( &GfxDevice::GetCurrentUbo()[ 0 ], &matrix4x4[0], sizeof( Matrix44 ) );
}

void ae3d::Shader::SetTexture( const char* /*name*/, Texture2D* texture, int /*textureUnit*/ )
{
    if (texture)
    {
        GfxDeviceGlobal::view0 = texture->GetView();
        GfxDeviceGlobal::sampler0 = texture->GetSampler();
    }
}

void ae3d::Shader::SetTexture( const char* /*name*/, TextureCube* texture, int /*textureUnit*/ )
{
    if (texture)
    {
        GfxDeviceGlobal::view0 = texture->GetView();
        GfxDeviceGlobal::sampler0 = texture->GetSampler();
    }
}

void ae3d::Shader::SetRenderTexture( const char* /*name*/, RenderTexture* texture, int /*textureUnit*/ )
{
    if (texture)
    {
        GfxDeviceGlobal::view0 = texture->GetColorView();
        GfxDeviceGlobal::sampler0 = texture->GetSampler();
    }
}

void ae3d::Shader::SetInt( const char* /*name*/, int /*value*/ )
{
}

void ae3d::Shader::SetFloat( const char* /*name*/, float /*value*/ )
{
}

void ae3d::Shader::SetVector3( const char* /*name*/, const float* /*vec3*/ )
{
}

void ae3d::Shader::SetVector4( const char* /*name*/, const float* /*vec4*/ )
{
}

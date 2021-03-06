#ifndef COMPUTE_SHADER_H
#define COMPUTE_SHADER_H

#include <string>
#include <vector>
#if RENDERER_METAL
#import <Metal/Metal.h>
#endif
#if RENDERER_D3D12
#include <d3d12.h>
#include <d3dcompiler.h>
#endif
#if RENDERER_VULKAN
#include <vulkan/vulkan.h>
#include <cstdint>
#endif

namespace ae3d
{
    class Texture2D;
    
    namespace FileSystem
    {
        struct FileContentsData;
    }
    
    /// Shader program containing a compute shader.
    class ComputeShader
    {
    public:
        /// Dispatches the shader.
        /// \param groupCountX X count
        /// \param groupCountY Y count
        /// \param groupCountZ Z count
        void Dispatch( unsigned groupCountX, unsigned groupCountY, unsigned groupCountZ );

        /// Internal loading method. End-users should use the method that takes all languages as parameters.
        /// \param source Source string.
        void Load( const char* source );

#if RENDERER_VULKAN
        /// Loads SPIR-V shader.
        /// \param contents SPIR-V file contents.
        void LoadSPIRV( const FileSystem::FileContentsData& contents );
#endif
        /// \param metalShaderName Vertex shader name for Metal renderer. Must be referenced by the application's Xcode project.
        /// \param dataHLSL HLSL shader file contents.
        /// \param dataSPIRV SPIR-V vertex file contents.
        void Load( const char* metalShaderName,
                   const FileSystem::FileContentsData& dataHLSL,
                   const FileSystem::FileContentsData& dataSPIRV );
        
#if RENDERER_METAL
        void LoadFromLibrary( const char* vertexShaderName, const char* fragmentShaderName );

        /// Sets a uniform buffer.
        /// \param slotIndex slot index
        /// \param buffer Uniform buffer
        void SetUniformBuffer( int slotIndex, id< MTLBuffer > buffer );
#endif
#if RENDERER_D3D12
        ID3DBlob* blobShader = nullptr;
#endif
#if RENDERER_VULKAN
        VkPipelineShaderStageCreateInfo& GetVertexInfo() { return info; }
#endif

        /// Sets a render texture into a slot.
        /// \param renderTexture render texture.
        /// \param slot slot index.
        void SetRenderTexture( class RenderTexture* renderTexture, unsigned slot );
        
#if RENDERER_D3D12
        void SetUniformBuffer( unsigned slot, ID3D12Resource* buffer );
        void SetTextureBuffer( unsigned slot, ID3D12Resource* buffer );
        void SetUAVBuffer( unsigned slot, ID3D12Resource* buffer );
#endif

    private:
        std::string path;

#if RENDERER_VULKAN
        VkPipelineShaderStageCreateInfo info;
#endif
#if RENDERER_METAL
        id <MTLFunction> function;
        id<MTLComputePipelineState> pipeline;
        id<MTLBuffer> uniforms[ 3 ];
#endif
        std::vector< RenderTexture* > renderTextures;
    };
}
#endif

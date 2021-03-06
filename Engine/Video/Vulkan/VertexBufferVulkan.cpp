#include "VertexBuffer.hpp"
#include <vector>
#include <cstring>
#include "Macros.hpp"
#include "Statistics.hpp"
#include "System.hpp"
#include "VulkanUtils.hpp"

namespace GfxDeviceGlobal
{
    extern VkDevice device;
    extern VkPhysicalDeviceMemoryProperties deviceMemoryProperties;
    extern std::vector< VkBuffer > pendingFreeVBs;
    extern VkCommandPool cmdPool;
    extern VkQueue graphicsQueue;
}

namespace ae3d
{
    void GetMemoryType( std::uint32_t typeBits, VkFlags properties, std::uint32_t* typeIndex ); // Defined in GfxDeviceVulkan.cpp 
}

namespace VertexBufferGlobal
{
    std::vector< VkBuffer > buffersToReleaseAtExit;
    std::vector< VkDeviceMemory > memoryToReleaseAtExit;
}

void ae3d::VertexBuffer::DestroyBuffers()
{
    for (std::size_t bufferIndex = 0; bufferIndex < VertexBufferGlobal::buffersToReleaseAtExit.size(); ++bufferIndex)
    {
        vkDestroyBuffer( GfxDeviceGlobal::device, VertexBufferGlobal::buffersToReleaseAtExit[ bufferIndex ], nullptr );
    }

    for (std::size_t memoryIndex = 0; memoryIndex < VertexBufferGlobal::memoryToReleaseAtExit.size(); ++memoryIndex)
    {
        vkFreeMemory( GfxDeviceGlobal::device, VertexBufferGlobal::memoryToReleaseAtExit[ memoryIndex ], nullptr );
    }
}

void ae3d::VertexBuffer::SetDebugName( const char* name )
{
    debug::SetObjectName( GfxDeviceGlobal::device, (std::uint64_t)vertexBuffer, VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT, name );
}

void CopyBuffer( VkBuffer source, VkBuffer& destination, int bufferSize )
{
    VkCommandBufferAllocateInfo cmdBufInfo = {};
    cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdBufInfo.commandPool = GfxDeviceGlobal::cmdPool;
    cmdBufInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdBufInfo.commandBufferCount = 1;

    VkCommandBuffer copyCommandBuffer;
    VkResult err = vkAllocateCommandBuffers( GfxDeviceGlobal::device, &cmdBufInfo, &copyCommandBuffer );
    AE3D_CHECK_VULKAN( err, "copy command buffer" );

    VkCommandBufferBeginInfo cmdBufferBeginInfo = {};
    cmdBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmdBufferBeginInfo.pNext = nullptr;

    VkBufferCopy copyRegion = {};
    copyRegion.size = bufferSize;

    err = vkBeginCommandBuffer( copyCommandBuffer, &cmdBufferBeginInfo );
    AE3D_CHECK_VULKAN( err, "begin staging copy" );

    vkCmdCopyBuffer( copyCommandBuffer, source, destination, 1, &copyRegion );

    err = vkEndCommandBuffer( copyCommandBuffer );
    AE3D_CHECK_VULKAN( err, "end staging copy" );

    VkSubmitInfo copySubmitInfo = {};
    copySubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    copySubmitInfo.commandBufferCount = 1;
    copySubmitInfo.pCommandBuffers = &copyCommandBuffer;

    err = vkQueueSubmit( GfxDeviceGlobal::graphicsQueue, 1, &copySubmitInfo, VK_NULL_HANDLE );
    AE3D_CHECK_VULKAN( err, "submit staging VB copy" );

    err = vkQueueWaitIdle( GfxDeviceGlobal::graphicsQueue );
    AE3D_CHECK_VULKAN( err, "wait after staging VB copy" );

    vkFreeCommandBuffers( GfxDeviceGlobal::device, cmdBufInfo.commandPool, 1, &copyCommandBuffer );
}

void CreateBuffer( VkBuffer& buffer, int bufferSize, VkDeviceMemory& memory, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryFlags, const char* debugName )
{
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bufferSize;
    bufferInfo.usage = usageFlags;
    VkResult err = vkCreateBuffer( GfxDeviceGlobal::device, &bufferInfo, nullptr, &buffer );
    AE3D_CHECK_VULKAN( err, "vkCreateBuffer" );
    debug::SetObjectName( GfxDeviceGlobal::device, (std::uint64_t)buffer, VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT, debugName );

    VkMemoryRequirements memReqs;
    VkMemoryAllocateInfo memAlloc = {};
    memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;

    vkGetBufferMemoryRequirements( GfxDeviceGlobal::device, buffer, &memReqs );
    memAlloc.allocationSize = memReqs.size;
    ae3d::GetMemoryType( memReqs.memoryTypeBits, memoryFlags, &memAlloc.memoryTypeIndex );
    err = vkAllocateMemory( GfxDeviceGlobal::device, &memAlloc, nullptr, &memory );
    AE3D_CHECK_VULKAN( err, "vkAllocateMemory" );
    Statistics::IncAllocCalls();

    err = vkBindBufferMemory( GfxDeviceGlobal::device, buffer, memory, 0 );
    AE3D_CHECK_VULKAN( err, "vkBindBufferMemory" );
}

void MarkForFreeing( VkBuffer vertexBuffer, VkDeviceMemory vertexMem, VkBuffer indexBuffer, VkDeviceMemory indexMem )
{
    for (std::size_t memoryIndex = 0; memoryIndex < VertexBufferGlobal::memoryToReleaseAtExit.size(); ++memoryIndex)
    {
        if (VertexBufferGlobal::memoryToReleaseAtExit[ memoryIndex ] == vertexMem)
        {
            VertexBufferGlobal::memoryToReleaseAtExit.erase( std::begin( VertexBufferGlobal::memoryToReleaseAtExit ) + memoryIndex );
        }
    }

    for (std::size_t memoryIndex = 0; memoryIndex < VertexBufferGlobal::memoryToReleaseAtExit.size(); ++memoryIndex)
    {
        if (VertexBufferGlobal::memoryToReleaseAtExit[ memoryIndex ] == indexMem)
        {
            VertexBufferGlobal::memoryToReleaseAtExit.erase( std::begin( VertexBufferGlobal::memoryToReleaseAtExit ) + memoryIndex );
        }
    }

    for (std::size_t bufferIndex = 0; bufferIndex < VertexBufferGlobal::buffersToReleaseAtExit.size(); ++bufferIndex)
    {
        if (VertexBufferGlobal::buffersToReleaseAtExit[ bufferIndex ] == vertexBuffer)
        {
            VertexBufferGlobal::buffersToReleaseAtExit.erase( std::begin( VertexBufferGlobal::buffersToReleaseAtExit ) + bufferIndex );
        }
    }

    for (std::size_t bufferIndex = 0; bufferIndex < VertexBufferGlobal::buffersToReleaseAtExit.size(); ++bufferIndex)
    {
        if (VertexBufferGlobal::buffersToReleaseAtExit[ bufferIndex ] == indexBuffer)
        {
            VertexBufferGlobal::buffersToReleaseAtExit.erase( std::begin( VertexBufferGlobal::buffersToReleaseAtExit ) + bufferIndex );
        }
    }

    vkFreeMemory( GfxDeviceGlobal::device, vertexMem, nullptr );
    vkFreeMemory( GfxDeviceGlobal::device, indexMem, nullptr );
    GfxDeviceGlobal::pendingFreeVBs.push_back( vertexBuffer );
    GfxDeviceGlobal::pendingFreeVBs.push_back( indexBuffer );
}

void ae3d::VertexBuffer::GenerateVertexBuffer( const void* vertexData, int vertexBufferSize, int vertexStride, const void* indexData, int indexBufferSize )
{
    System::Assert( GfxDeviceGlobal::device != VK_NULL_HANDLE, "device not initialized" );
    System::Assert( vertexData != nullptr, "vertexData not initialized" );
    System::Assert( indexData != nullptr, "indexData not initialized" );

    if (vertexBuffer != VK_NULL_HANDLE)
    {
        MarkForFreeing( vertexBuffer, vertexMem, indexBuffer, indexMem );
    }

    struct Buffer
    {
        VkDeviceMemory memory;
        VkBuffer buffer;
    };

    struct
    {
        Buffer vertices;
        Buffer indices;
    } stagingBuffers;

    // Vertex buffer
    CreateBuffer( stagingBuffers.vertices.buffer, vertexBufferSize, stagingBuffers.vertices.memory, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, "staging vertex buffer" );

    void* bufferData = nullptr;
    VkResult err = vkMapMemory( GfxDeviceGlobal::device, stagingBuffers.vertices.memory, 0, vertexBufferSize, 0, &bufferData );
    AE3D_CHECK_VULKAN( err, "map vertex memory" );

    std::memcpy( bufferData, vertexData, vertexBufferSize );
    vkUnmapMemory( GfxDeviceGlobal::device, stagingBuffers.vertices.memory );

    CreateBuffer( vertexBuffer, vertexBufferSize, vertexMem, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "vertex buffer" );
    VertexBufferGlobal::buffersToReleaseAtExit.push_back( vertexBuffer );
    VertexBufferGlobal::memoryToReleaseAtExit.push_back( vertexMem );
    CopyBuffer( stagingBuffers.vertices.buffer, vertexBuffer, vertexBufferSize );

    vkDestroyBuffer( GfxDeviceGlobal::device, stagingBuffers.vertices.buffer, nullptr );
    vkFreeMemory( GfxDeviceGlobal::device, stagingBuffers.vertices.memory, nullptr );

    // Index buffer
    CreateBuffer( stagingBuffers.indices.buffer, indexBufferSize, stagingBuffers.indices.memory, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, "staging index buffer" );

    err = vkMapMemory( GfxDeviceGlobal::device, stagingBuffers.indices.memory, 0, indexBufferSize, 0, &bufferData );
    AE3D_CHECK_VULKAN( err, "map staging index memory" );

    std::memcpy( bufferData, indexData, indexBufferSize );
    vkUnmapMemory( GfxDeviceGlobal::device, stagingBuffers.indices.memory );

    CreateBuffer( indexBuffer, indexBufferSize, indexMem, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "index buffer" );
    VertexBufferGlobal::buffersToReleaseAtExit.push_back( indexBuffer );
    VertexBufferGlobal::memoryToReleaseAtExit.push_back( indexMem );
    CopyBuffer( stagingBuffers.indices.buffer, indexBuffer, indexBufferSize );

    vkDestroyBuffer( GfxDeviceGlobal::device, stagingBuffers.indices.buffer, nullptr );
    vkFreeMemory( GfxDeviceGlobal::device, stagingBuffers.indices.memory, nullptr );

    CreateInputState( vertexStride );
}

void ae3d::VertexBuffer::CreateInputState( int vertexStride )
{
    bindingDescriptions.resize( 1 );
    bindingDescriptions[ 0 ].binding = VERTEX_BUFFER_BIND_ID;
    bindingDescriptions[ 0 ].stride = vertexStride;
    bindingDescriptions[ 0 ].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    if (vertexFormat == VertexFormat::PTC)
    {
        attributeDescriptions.resize( 3 );

        // Location 0 : Position
        attributeDescriptions[ 0 ].binding = VERTEX_BUFFER_BIND_ID;
        attributeDescriptions[ 0 ].location = posChannel;
        attributeDescriptions[ 0 ].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[ 0 ].offset = 0;

        // Location 1 : TexCoord
        attributeDescriptions[ 1 ].binding = VERTEX_BUFFER_BIND_ID;
        attributeDescriptions[ 1 ].location = uvChannel;
        attributeDescriptions[ 1 ].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[ 1 ].offset = sizeof( float ) * 3;

        // Location 2 : Color
        attributeDescriptions[ 2 ].binding = VERTEX_BUFFER_BIND_ID;
        attributeDescriptions[ 2 ].location = colorChannel;
        attributeDescriptions[ 2 ].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attributeDescriptions[ 2 ].offset = sizeof( float ) * 5;
    }
    else if (vertexFormat == VertexFormat::PTN)
    {
        attributeDescriptions.resize( 3 );

        // Location 0 : Position
        attributeDescriptions[ 0 ].binding = VERTEX_BUFFER_BIND_ID;
        attributeDescriptions[ 0 ].location = posChannel;
        attributeDescriptions[ 0 ].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[ 0 ].offset = 0;

        // Location 1 : TexCoord
        attributeDescriptions[ 1 ].binding = VERTEX_BUFFER_BIND_ID;
        attributeDescriptions[ 1 ].location = uvChannel;
        attributeDescriptions[ 1 ].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[ 1 ].offset = sizeof( float ) * 3;

        // Location 2 : Normal
        attributeDescriptions[ 2 ].binding = VERTEX_BUFFER_BIND_ID;
        attributeDescriptions[ 2 ].location = normalChannel;
        attributeDescriptions[ 2 ].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[ 2 ].offset = sizeof( float ) * 5;
    }
    else if (vertexFormat == VertexFormat::PTNTC)
    {
        attributeDescriptions.resize( 5 );

        // Location 0 : Position
        attributeDescriptions[ 0 ].binding = VERTEX_BUFFER_BIND_ID;
        attributeDescriptions[ 0 ].location = posChannel;
        attributeDescriptions[ 0 ].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[ 0 ].offset = 0;

        // Location 1 : TexCoord
        attributeDescriptions[ 1 ].binding = VERTEX_BUFFER_BIND_ID;
        attributeDescriptions[ 1 ].location = uvChannel;
        attributeDescriptions[ 1 ].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[ 1 ].offset = sizeof( float ) * 3;

        // Location 2 : Normal
        attributeDescriptions[ 2 ].binding = VERTEX_BUFFER_BIND_ID;
        attributeDescriptions[ 2 ].location = normalChannel;
        attributeDescriptions[ 2 ].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[ 2 ].offset = sizeof( float ) * 5;

        // Location 3 : Tangent
        attributeDescriptions[ 3 ].binding = VERTEX_BUFFER_BIND_ID;
        attributeDescriptions[ 3 ].location = tangentChannel;
        attributeDescriptions[ 3 ].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attributeDescriptions[ 3 ].offset = sizeof( float ) * 8;

        // Location 4 : Color
        attributeDescriptions[ 4 ].binding = VERTEX_BUFFER_BIND_ID;
        attributeDescriptions[ 4 ].location = colorChannel;
        attributeDescriptions[ 4 ].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attributeDescriptions[ 4 ].offset = sizeof( float ) * 12;
    }
    else
    {
        System::Assert( false, "unhandled vertex format" );
    }

    inputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    inputStateCreateInfo.pNext = nullptr;
    inputStateCreateInfo.vertexBindingDescriptionCount = (std::uint32_t)bindingDescriptions.size();
    inputStateCreateInfo.pVertexBindingDescriptions = bindingDescriptions.data();
    inputStateCreateInfo.vertexAttributeDescriptionCount = (std::uint32_t)attributeDescriptions.size();
    inputStateCreateInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
}

void ae3d::VertexBuffer::Generate( const Face* faces, int faceCount, const VertexPTC* vertices, int vertexCount )
{
    vertexFormat = VertexFormat::PTNTC;
    elementCount = faceCount * 3;

    std::vector< VertexPTNTC > verticesPTNTC( vertexCount );

    for (std::size_t vertexInd = 0; vertexInd < verticesPTNTC.size(); ++vertexInd)
    {
        verticesPTNTC[ vertexInd ].position = vertices[ vertexInd ].position;
        verticesPTNTC[ vertexInd ].u = vertices[ vertexInd ].u;
        verticesPTNTC[ vertexInd ].v = vertices[ vertexInd ].v;
        verticesPTNTC[ vertexInd ].normal = Vec3( 0, 0, 1 );
        verticesPTNTC[ vertexInd ].tangent = Vec4( 1, 0, 0, 0 );
        verticesPTNTC[ vertexInd ].color = vertices[ vertexInd ].color;
    }

    GenerateVertexBuffer( static_cast< const void*>( verticesPTNTC.data() ), vertexCount * sizeof( VertexPTNTC ), sizeof( VertexPTNTC ), static_cast< const void* >(faces), elementCount * 2 );
}

void ae3d::VertexBuffer::Generate( const Face* faces, int faceCount, const VertexPTN* vertices, int vertexCount )
{
    vertexFormat = VertexFormat::PTNTC;
    elementCount = faceCount * 3;

    std::vector< VertexPTNTC > verticesPTNTC( vertexCount );

    for (std::size_t vertexInd = 0; vertexInd < verticesPTNTC.size(); ++vertexInd)
    {
        verticesPTNTC[ vertexInd ].position = vertices[ vertexInd ].position;
        verticesPTNTC[ vertexInd ].u = vertices[ vertexInd ].u;
        verticesPTNTC[ vertexInd ].v = vertices[ vertexInd ].v;
        verticesPTNTC[ vertexInd ].normal = vertices[ vertexInd ].normal;
        verticesPTNTC[ vertexInd ].tangent = Vec4( 1, 0, 0, 0 );
        verticesPTNTC[ vertexInd ].color = Vec4( 1, 1, 1, 1 );
    }

    GenerateVertexBuffer( static_cast< const void*>( verticesPTNTC.data() ), vertexCount * sizeof( VertexPTNTC ), sizeof( VertexPTNTC ), static_cast< const void* >( faces ), elementCount * 2 );
}

void ae3d::VertexBuffer::Generate( const Face* faces, int faceCount, const VertexPTNTC* vertices, int vertexCount )
{
    vertexFormat = VertexFormat::PTNTC;
    elementCount = faceCount * 3;
    GenerateVertexBuffer( static_cast< const void*>( vertices ), vertexCount * sizeof( VertexPTNTC ), sizeof( VertexPTNTC ), static_cast< const void*>( faces ), elementCount * 2 );
}

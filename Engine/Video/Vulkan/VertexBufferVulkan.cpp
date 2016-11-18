#include "VertexBuffer.hpp"
#include <vector>
#include <cstring>
#include "Macros.hpp"
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

void ae3d::VertexBuffer::GenerateVertexBuffer( const void* vertexData, int vertexBufferSize, int vertexStride, const void* indexData, int indexBufferSize )
{
    System::Assert( GfxDeviceGlobal::device != VK_NULL_HANDLE, "device not initialized" );
    System::Assert( vertexData != nullptr, "vertexData not initialized" );
    System::Assert( indexData != nullptr, "indexData not initialized" );

    // FIXME: If this leads to a crash, find out some saner way to free memory
    if (vertexBuffer != VK_NULL_HANDLE)
    {
        vkFreeMemory( GfxDeviceGlobal::device, vertexMem, nullptr );
        vkFreeMemory( GfxDeviceGlobal::device, indexMem, nullptr );
        GfxDeviceGlobal::pendingFreeVBs.push_back( vertexBuffer );
        GfxDeviceGlobal::pendingFreeVBs.push_back( indexBuffer );
    }

    bool useStaging = true;

    if (useStaging)
    {
        struct StagingBuffer
        {
            VkDeviceMemory memory;
            VkBuffer buffer;
        };

        struct
        {
            StagingBuffer vertices;
            StagingBuffer indices;
        } stagingBuffers;

        VkCommandBufferAllocateInfo cmdBufInfo = {};
        cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdBufInfo.commandPool = GfxDeviceGlobal::cmdPool;
        cmdBufInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmdBufInfo.commandBufferCount = 1;

        VkCommandBuffer copyCommandBuffer;
        VkResult err = vkAllocateCommandBuffers( GfxDeviceGlobal::device, &cmdBufInfo, &copyCommandBuffer );
        AE3D_CHECK_VULKAN( err, "copy command buffer" );

        VkBufferCreateInfo vertexBufferInfo = {};
        vertexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        vertexBufferInfo.size = vertexBufferSize;
        vertexBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        err = vkCreateBuffer( GfxDeviceGlobal::device, &vertexBufferInfo, nullptr, &stagingBuffers.vertices.buffer );
        AE3D_CHECK_VULKAN( err, "vkCreateBuffer vertexBuffer" );
        debug::SetObjectName( GfxDeviceGlobal::device, (std::uint64_t)stagingBuffers.vertices.buffer, VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT, "staging vertex buffer" );

        VkMemoryRequirements memReqs;
        VkMemoryAllocateInfo memAlloc = {};
        memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;

        vkGetBufferMemoryRequirements( GfxDeviceGlobal::device, stagingBuffers.vertices.buffer, &memReqs );
        memAlloc.allocationSize = memReqs.size;
        GetMemoryType( memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &memAlloc.memoryTypeIndex );
        err = vkAllocateMemory( GfxDeviceGlobal::device, &memAlloc, nullptr, &stagingBuffers.vertices.memory );
        AE3D_CHECK_VULKAN( err, "allocate staging vertex memory" );

        void* data = nullptr;
        err = vkMapMemory( GfxDeviceGlobal::device, stagingBuffers.vertices.memory, 0, memAlloc.allocationSize, 0, &data );
        AE3D_CHECK_VULKAN( err, "map vertex memory" );

        std::memcpy( data, vertexData, vertexBufferSize );
        vkUnmapMemory( GfxDeviceGlobal::device, stagingBuffers.vertices.memory );
        err = vkBindBufferMemory( GfxDeviceGlobal::device, stagingBuffers.vertices.buffer, stagingBuffers.vertices.memory, 0 );
        AE3D_CHECK_VULKAN( err, "bind staging vertex memory" );

        vertexBufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        err = vkCreateBuffer( GfxDeviceGlobal::device, &vertexBufferInfo, nullptr, &vertexBuffer );
        AE3D_CHECK_VULKAN( err, "create vertex buffer" );
        VertexBufferGlobal::buffersToReleaseAtExit.push_back( vertexBuffer );
        debug::SetObjectName( GfxDeviceGlobal::device, (std::uint64_t)vertexBuffer, VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT, "vertex buffer" );

        vkGetBufferMemoryRequirements( GfxDeviceGlobal::device, vertexBuffer, &memReqs );
        memAlloc.allocationSize = memReqs.size;
        GetMemoryType( memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memAlloc.memoryTypeIndex );
        err = vkAllocateMemory( GfxDeviceGlobal::device, &memAlloc, nullptr, &vertexMem );
        AE3D_CHECK_VULKAN( err, "allocate vertex memory" );
        VertexBufferGlobal::memoryToReleaseAtExit.push_back( vertexMem );

        err = vkBindBufferMemory( GfxDeviceGlobal::device, vertexBuffer, vertexMem, 0 );
        AE3D_CHECK_VULKAN( err, "bind vertex memory" );

        VkBufferCreateInfo indexbufferInfo = {};
        indexbufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        indexbufferInfo.size = indexBufferSize;
        indexbufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        err = vkCreateBuffer( GfxDeviceGlobal::device, &indexbufferInfo, nullptr, &stagingBuffers.indices.buffer );
        AE3D_CHECK_VULKAN( err, "create staging index buffer" );
        debug::SetObjectName( GfxDeviceGlobal::device, (std::uint64_t)stagingBuffers.indices.buffer, VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT, "staging index buffer" );

        vkGetBufferMemoryRequirements( GfxDeviceGlobal::device, stagingBuffers.indices.buffer, &memReqs );
        memAlloc.allocationSize = memReqs.size;
        GetMemoryType( memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &memAlloc.memoryTypeIndex );
        err = vkAllocateMemory( GfxDeviceGlobal::device, &memAlloc, nullptr, &stagingBuffers.indices.memory );
        AE3D_CHECK_VULKAN( err, "allocate staging index memory" );

        err = vkMapMemory( GfxDeviceGlobal::device, stagingBuffers.indices.memory, 0, indexBufferSize, 0, &data );
        AE3D_CHECK_VULKAN( err, "map staging index memory" );

        std::memcpy( data, indexData, indexBufferSize );
        vkUnmapMemory( GfxDeviceGlobal::device, stagingBuffers.indices.memory );
        err = vkBindBufferMemory( GfxDeviceGlobal::device, stagingBuffers.indices.buffer, stagingBuffers.indices.memory, 0 );
        AE3D_CHECK_VULKAN( err, "bind staging index memory" );

        indexbufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        err = vkCreateBuffer( GfxDeviceGlobal::device, &indexbufferInfo, nullptr, &indexBuffer );
        AE3D_CHECK_VULKAN( err, "create index buffer" );
        VertexBufferGlobal::buffersToReleaseAtExit.push_back( indexBuffer );
        debug::SetObjectName( GfxDeviceGlobal::device, (std::uint64_t)indexBuffer, VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT, "index buffer" );

        vkGetBufferMemoryRequirements( GfxDeviceGlobal::device, indexBuffer, &memReqs );
        memAlloc.allocationSize = memReqs.size;
        GetMemoryType( memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memAlloc.memoryTypeIndex );
        err = vkAllocateMemory( GfxDeviceGlobal::device, &memAlloc, nullptr, &indexMem );
        AE3D_CHECK_VULKAN( err, "allocate index memory" );
        VertexBufferGlobal::memoryToReleaseAtExit.push_back( indexMem );

        err = vkBindBufferMemory( GfxDeviceGlobal::device, indexBuffer, indexMem, 0 );
        AE3D_CHECK_VULKAN( err, "bind index buffer memory" );

        VkCommandBufferBeginInfo cmdBufferBeginInfo = {};
        cmdBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cmdBufferBeginInfo.pNext = nullptr;

        VkBufferCopy copyRegion = {};
        err = vkBeginCommandBuffer( copyCommandBuffer, &cmdBufferBeginInfo );
        AE3D_CHECK_VULKAN( err, "begin staging copy" );

        copyRegion.size = vertexBufferSize;
        vkCmdCopyBuffer( copyCommandBuffer, stagingBuffers.vertices.buffer, vertexBuffer, 1, &copyRegion );

        copyRegion.size = indexBufferSize;
        vkCmdCopyBuffer( copyCommandBuffer, stagingBuffers.indices.buffer, indexBuffer, 1, &copyRegion );

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

        vkDestroyBuffer( GfxDeviceGlobal::device, stagingBuffers.vertices.buffer, nullptr );
        vkFreeMemory( GfxDeviceGlobal::device, stagingBuffers.vertices.memory, nullptr );
        vkDestroyBuffer( GfxDeviceGlobal::device, stagingBuffers.indices.buffer, nullptr );
        vkFreeMemory( GfxDeviceGlobal::device, stagingBuffers.indices.memory, nullptr );
        vkFreeCommandBuffers( GfxDeviceGlobal::device, cmdBufInfo.commandPool, 1, &copyCommandBuffer );
    }
    else // Not a staging buffer
    {
        VkBufferCreateInfo bufCreateInfo = {};
        bufCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufCreateInfo.pNext = nullptr;
        bufCreateInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        bufCreateInfo.size = vertexBufferSize;
        bufCreateInfo.flags = 0;
        VkResult err = vkCreateBuffer( GfxDeviceGlobal::device, &bufCreateInfo, nullptr, &vertexBuffer );
        AE3D_CHECK_VULKAN( err, "vkCreateBuffer" );
        debug::SetObjectName( GfxDeviceGlobal::device, (std::uint64_t)vertexBuffer, VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT, "vertex buffer" );

        VkMemoryAllocateInfo memAllocInfo = {};
        memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memAllocInfo.pNext = nullptr;
        memAllocInfo.allocationSize = 0;
        memAllocInfo.memoryTypeIndex = 0;

        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements( GfxDeviceGlobal::device, vertexBuffer, &memReqs );
        memAllocInfo.allocationSize = memReqs.size;
        GetMemoryType( memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &memAllocInfo.memoryTypeIndex );
        err = vkAllocateMemory( GfxDeviceGlobal::device, &memAllocInfo, nullptr, &vertexMem );
        AE3D_CHECK_VULKAN( err, "vkAllocateMemory" );

        void* data = nullptr;
        err = vkMapMemory( GfxDeviceGlobal::device, vertexMem, 0, vertexBufferSize, 0, &data );
        AE3D_CHECK_VULKAN( err, "vkAllocateMemory" );

        std::memcpy( data, vertexData, vertexBufferSize );

        vkUnmapMemory( GfxDeviceGlobal::device, vertexMem );
        err = vkBindBufferMemory( GfxDeviceGlobal::device, vertexBuffer, vertexMem, 0 );
        AE3D_CHECK_VULKAN( err, "vkBindBufferMemory" );

        VkBufferCreateInfo ibCreateInfo = {};
        ibCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        ibCreateInfo.pNext = nullptr;
        ibCreateInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        ibCreateInfo.size = indexBufferSize;
        ibCreateInfo.flags = 0;
        err = vkCreateBuffer( GfxDeviceGlobal::device, &ibCreateInfo, nullptr, &indexBuffer );
        AE3D_CHECK_VULKAN( err, "vkCreateBuffer" );
        debug::SetObjectName( GfxDeviceGlobal::device, (std::uint64_t)indexBuffer, VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT, "index buffer" );

        vkGetBufferMemoryRequirements( GfxDeviceGlobal::device, indexBuffer, &memReqs );
        memAllocInfo.allocationSize = memReqs.size;
        GetMemoryType( memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &memAllocInfo.memoryTypeIndex );
        err = vkAllocateMemory( GfxDeviceGlobal::device, &memAllocInfo, nullptr, &indexMem );
        AE3D_CHECK_VULKAN( err, "vkAllocateMemory" );

        err = vkMapMemory( GfxDeviceGlobal::device, indexMem, 0, indexBufferSize, 0, &data );
        AE3D_CHECK_VULKAN( err, "vkMapMemory" );

        std::memcpy( data, indexData, indexBufferSize );
        vkUnmapMemory( GfxDeviceGlobal::device, indexMem );
        err = vkBindBufferMemory( GfxDeviceGlobal::device, indexBuffer, indexMem, 0 );
        AE3D_CHECK_VULKAN( err, "vkBindBufferMemory index buffer" );
    }

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
    vertexFormat = VertexFormat::PTC;
    elementCount = faceCount * 3;
    GenerateVertexBuffer( static_cast< const void*>( vertices ), vertexCount * sizeof( VertexPTC ), sizeof( VertexPTC ), static_cast< const void* >( faces ), elementCount * 2 );
}

void ae3d::VertexBuffer::Generate( const Face* faces, int faceCount, const VertexPTN* vertices, int vertexCount )
{
    vertexFormat = VertexFormat::PTN;
    elementCount = faceCount * 3;
    GenerateVertexBuffer( static_cast< const void*>( vertices ), vertexCount * sizeof( VertexPTN ), sizeof( VertexPTN ), static_cast< const void* >( faces ), elementCount * 2 );
}

void ae3d::VertexBuffer::Generate( const Face* faces, int faceCount, const VertexPTNTC* vertices, int vertexCount )
{
    vertexFormat = VertexFormat::PTNTC;
    elementCount = faceCount * 3;
    GenerateVertexBuffer( static_cast< const void*>( vertices ), vertexCount * sizeof( VertexPTNTC ), sizeof( VertexPTNTC ), static_cast< const void*>( faces ), elementCount * 2 );
}

#include "GfxDevice.hpp"
#include <algorithm>
#include <vector>
#include <string>
#include <sstream>
#include <GL/glxw.h>
#include "System.hpp"
#include "Statistics.hpp"
#include "RenderTexture.hpp"
#include "Shader.hpp"
#include "VertexBuffer.hpp"

void PrintOpenGLDebugOutput( GLenum source, GLenum type, GLuint id, GLenum severity, const char *msg)
{
    const char *sourceFmt = "UNDEFINED(0x%04X)";

    switch (source)
    {
    case GL_DEBUG_SOURCE_API_ARB:             sourceFmt = "API"; break;
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM_ARB:   sourceFmt = "WINDOW_SYSTEM"; break;
    case GL_DEBUG_SOURCE_SHADER_COMPILER_ARB: sourceFmt = "SHADER_COMPILER"; break;
    case GL_DEBUG_SOURCE_THIRD_PARTY_ARB:     sourceFmt = "THIRD_PARTY"; break;
    case GL_DEBUG_SOURCE_APPLICATION_ARB:     sourceFmt = "APPLICATION"; break;
    case GL_DEBUG_SOURCE_OTHER_ARB:           sourceFmt = "OTHER"; break;
    }

    const char *typeFmt = "UNDEFINED(0x%04X)";

    switch (type)
    {
    case GL_DEBUG_TYPE_ERROR_ARB:               typeFmt = "ERROR"; break;
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_ARB: typeFmt = "DEPRECATED_BEHAVIOR"; break;
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_ARB:  typeFmt = "UNDEFINED_BEHAVIOR"; break;
    case GL_DEBUG_TYPE_PORTABILITY_ARB:         typeFmt = "PORTABILITY"; break;
    case GL_DEBUG_TYPE_PERFORMANCE_ARB:         typeFmt = "PERFORMANCE"; break;
    case GL_DEBUG_TYPE_OTHER_ARB:               typeFmt = "OTHER"; break;
    }

    const char *severityFmt = "UNDEFINED";

    switch (severity)
    {
    case GL_DEBUG_SEVERITY_HIGH_ARB:   severityFmt = "HIGH";   break;
    case GL_DEBUG_SEVERITY_MEDIUM_ARB: severityFmt = "MEDIUM"; break;
    case GL_DEBUG_SEVERITY_LOW_ARB:    severityFmt = "LOW"; break;
    }

    ae3d::System::Print( "OpenGL: %s [source=%s type=%s severity=%s id=%u\n",
        msg, sourceFmt, typeFmt, severityFmt, id );
}

void APIENTRY DebugCallbackARB( GLenum source, GLenum type, GLuint id, GLenum severity,
                                GLsizei length, const GLchar *message, const GLvoid *userParam )
{
    const int undefinedSeverity = 33387;
    // Old engine (untested on current): NVIDIA driver spams buffer creation messages at 'undefined' severity, this silences them.
    if (severity != undefinedSeverity)
    {
        (void)length;
        (void)userParam;
        PrintOpenGLDebugOutput( source, type, id, severity, message);
        ae3d::System::Assert( false, "OpenGL error" );
    }
}

namespace GfxDeviceGlobal
{
    std::vector< GLuint > vaoIds;
    std::vector< GLuint > bufferIds;
    std::vector< GLuint > textureIds;
    std::vector< GLuint > shaderIds;
    std::vector< GLuint > programIds;
    std::vector< GLuint > rboIds;
    std::vector< GLuint > fboIds;
    std::vector< ae3d::VertexBuffer > lineBuffers;
    
    int backBufferWidth = 640;
    int backBufferHeight = 400;
    GLuint systemFBO = 0;
    GLuint cachedFBO = 0;
}

namespace ae3d
{
    namespace System
    {
        namespace Statistics
        {
            std::string GetStatistics()
            {
                std::stringstream stm;
                stm << "frame time: " << ::Statistics::GetFrameTimeMS() << " ms\n";
                stm << "shadow map time: " << ::Statistics::GetShadowMapTimeMS() << " ms\n";
                stm << "depth pass time: " << ::Statistics::GetDepthNormalsTimeMS() << " ms\n";
                stm << "draw calls: " << ::Statistics::GetDrawCalls() << "\n";
                stm << "texture binds: " << ::Statistics::GetTextureBinds() << "\n";
                stm << "render target binds: " << ::Statistics::GetRenderTargetBinds() << "\n";
                stm << "triangles: " << ::Statistics::GetTriangleCount() << "\n";

                return stm.str();
            }
        }
    }
}

void SetBlendMode( ae3d::GfxDevice::BlendMode blendMode )
{
    if (blendMode == ae3d::GfxDevice::BlendMode::Off)
    {
        glDisable( GL_BLEND );
    }
    else if (blendMode == ae3d::GfxDevice::BlendMode::AlphaBlend)
    {
        glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
        glEnable( GL_BLEND );
    }
    else if (blendMode == ae3d::GfxDevice::BlendMode::Additive)
    {
        glBlendFunc( GL_ONE, GL_ONE );
        glEnable( GL_BLEND );
    }
    else
    {
        ae3d::System::Assert( false, "Unhandled blend mode." );
    }
}

void SetCullMode( ae3d::GfxDevice::CullMode cullMode )
{
    if (cullMode == ae3d::GfxDevice::CullMode::Back)
    {
        glEnable( GL_CULL_FACE );
        glCullFace( GL_BACK );
    }
    else if (cullMode == ae3d::GfxDevice::CullMode::Front)
    {
        glEnable( GL_CULL_FACE );
        glCullFace( GL_FRONT );
    }
    else
    {
        glDisable( GL_CULL_FACE );
    }
}

void SetFillMode( ae3d::GfxDevice::FillMode fillMode )
{
    glPolygonMode( GL_FRONT_AND_BACK, fillMode == ae3d::GfxDevice::FillMode::Solid ? GL_FILL : GL_LINE );
}

void SetDepthFunc( ae3d::GfxDevice::DepthFunc depthFunc )
{
    if (depthFunc == ae3d::GfxDevice::DepthFunc::LessOrEqualWriteOn)
    {
        glDepthMask( GL_TRUE );
        glEnable( GL_DEPTH_TEST );
        glDepthFunc( GL_LEQUAL );
    }
    else if (depthFunc == ae3d::GfxDevice::DepthFunc::LessOrEqualWriteOff)
    {
        glDepthMask( GL_FALSE );
        glEnable( GL_DEPTH_TEST );
        glDepthFunc( GL_LEQUAL );
    }
    else if (depthFunc == ae3d::GfxDevice::DepthFunc::NoneWriteOff)
    {
        glDepthMask( GL_FALSE );
        glDisable( GL_DEPTH_TEST );
    }
    else
    {
        ae3d::System::Assert( false, "Unhandled depth function." );   
    }
}

void ae3d::GfxDevice::SetPolygonOffset( bool enable, float factor, float units )
{
    if (enable)
    {
        glEnable( GL_POLYGON_OFFSET_FILL );
    }
    else
    {
        glDisable( GL_POLYGON_OFFSET_FILL );
    }
    
    glPolygonOffset( factor, units );
}

void ae3d::GfxDevice::Init( int width, int height )
{
    if (width < 0 || height < 0)
    {
        System::Print( "Window's dimension is invalid." );
        width = 640;
        height = 480;
    }
    
    if (glxwInit() != 0)
    {
        System::Print( "Unable to initialize GLXW!" );
        return;
    }
    
    SetBackBufferDimensionAndFBO( width, height );
    Set_sRGB_Writes( true );
    glEnable( GL_DEPTH_TEST );

    GLint v;
    glGetIntegerv( GL_CONTEXT_FLAGS, &v );

    if (v & GL_CONTEXT_FLAG_DEBUG_BIT)
    {
        glDebugMessageCallback( DebugCallbackARB, nullptr );
        glEnable( GL_DEBUG_OUTPUT_SYNCHRONOUS );
    }
}

void ae3d::GfxDevice::PushGroupMarker( const char* name )
{
    if (GfxDevice::HasExtension( "GL_KHR_debug" ))
    {
        const std::string nameStr( name );
        glPushDebugGroup( GL_DEBUG_SOURCE_APPLICATION, 0, (GLsizei)nameStr.length(), name );
    }
}

void ae3d::GfxDevice::PopGroupMarker()
{
    if (GfxDevice::HasExtension( "GL_KHR_debug" ))
    {
        glPopDebugGroup();
    }
}

void ae3d::GfxDevice::Draw( VertexBuffer& vertexBuffer, int startIndex, int endIndex, Shader& shader, BlendMode blendMode, DepthFunc depthFunc,
                            CullMode cullMode, GfxDevice::FillMode fillMode )
{
    ae3d::System::Assert( startIndex > -1 && startIndex <= vertexBuffer.GetFaceCount() / 3, "Invalid vertex buffer draw range in startIndex" );
    ae3d::System::Assert( endIndex > -1 && endIndex >= startIndex && endIndex <= vertexBuffer.GetFaceCount() / 3, "Invalid vertex buffer draw range in endIndex" );

    SetBlendMode( blendMode );
    SetDepthFunc( depthFunc );
    SetCullMode( cullMode );
    SetFillMode( fillMode );

    shader.Use();
    vertexBuffer.Bind();
    Statistics::IncDrawCalls();
    Statistics::IncTriangleCount( endIndex - startIndex );

#if DEBUG
    shader.Validate();
#endif

    glDrawRangeElements( GL_TRIANGLES, startIndex, endIndex, (endIndex - startIndex) * 3, GL_UNSIGNED_SHORT, (const GLvoid*)(startIndex * sizeof( VertexBuffer::Face )) );
}

int ae3d::GfxDevice::CreateLineBuffer( const std::vector< Vec3 >& lines, const Vec3& color )
{
    if (lines.empty())
    {
        return -1;
    }
    
    std::vector< VertexBuffer::Face > faces( lines.size() * 2 );
    
    std::vector< VertexBuffer::VertexPTC > vertices( lines.size() );
    
    for (std::size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex)
    {
        vertices[ lineIndex ].position = lines[ lineIndex ];
        vertices[ lineIndex ].color = Vec4( color, 1 );
    }
    
    // Not used, but needs to be set to something.
    for (unsigned short faceIndex = 0; faceIndex < (unsigned short)(faces.size() / 2); ++faceIndex)
    {
        faces[ faceIndex * 2 + 0 ].a = faceIndex;
        faces[ faceIndex * 2 + 1 ].b = faceIndex + 1;
    }
    
    GfxDeviceGlobal::lineBuffers.push_back( VertexBuffer() );
    GfxDeviceGlobal::lineBuffers.back().Generate( faces.data(), int( faces.size() ), vertices.data(), int( vertices.size() ) );
    GfxDeviceGlobal::lineBuffers.back().SetDebugName( "line buffer" );
    
    return int( GfxDeviceGlobal::lineBuffers.size() ) - 1;
}

void ae3d::GfxDevice::DrawLines( int handle )
{
    if (handle < 0)
    {
        return;
    }

    SetBlendMode( BlendMode::Off );
    SetDepthFunc( DepthFunc::NoneWriteOff );
    SetCullMode( CullMode::Off );
    SetFillMode( FillMode::Solid );

    GfxDeviceGlobal::lineBuffers[ handle ].Bind();
    const int endIndex = GfxDeviceGlobal::lineBuffers[ handle ].GetFaceCount();
    glDrawArrays( GL_LINES, 0, endIndex );
}

void ae3d::GfxDevice::SetMultiSampling( bool enable )
{
    if (enable)
    {
        glEnable( GL_MULTISAMPLE );
    }
    else
    {
        glDisable( GL_MULTISAMPLE );
    }
}

void ae3d::GfxDevice::GetGpuMemoryUsage( unsigned& outGpuUsageMBytes, unsigned& outGpuBudgetMBytes )
{
    if (HasExtension( "GL_NVX_gpu_memory_info" ))
    {
        const unsigned GL_GPU_MEM_INFO_TOTAL_AVAILABLE_MEM_NVX = 0x9048;
        const unsigned GL_GPU_MEM_INFO_CURRENT_AVAILABLE_MEM_NVX = 0x9049;

        GLint totalMemoryInKB = 0;
        glGetIntegerv( GL_GPU_MEM_INFO_TOTAL_AVAILABLE_MEM_NVX, &totalMemoryInKB );

        GLint curAvailMemoryInKB = 0;
        glGetIntegerv( GL_GPU_MEM_INFO_CURRENT_AVAILABLE_MEM_NVX, &curAvailMemoryInKB );

        outGpuUsageMBytes = (unsigned)(totalMemoryInKB - curAvailMemoryInKB) / 1024;
        outGpuBudgetMBytes = (unsigned)totalMemoryInKB / 1024;
    }
    else
    {
        outGpuUsageMBytes = 0;
        outGpuBudgetMBytes = 0;
    }
}

void ae3d::GfxDevice::ReleaseGPUObjects()
{
    if (!GfxDeviceGlobal::vaoIds.empty())
    {
        glDeleteVertexArrays( static_cast<GLsizei>(GfxDeviceGlobal::vaoIds.size()), GfxDeviceGlobal::vaoIds.data() );
    }
    if (!GfxDeviceGlobal::bufferIds.empty())
    {
        glDeleteBuffers( static_cast<GLsizei>(GfxDeviceGlobal::bufferIds.size()), GfxDeviceGlobal::bufferIds.data() );
    }
    if (!GfxDeviceGlobal::textureIds.empty())
    {
        glDeleteTextures( static_cast<GLsizei>(GfxDeviceGlobal::textureIds.size()), GfxDeviceGlobal::textureIds.data() );
    }
    if (!GfxDeviceGlobal::rboIds.empty())
    {
        glDeleteRenderbuffers( static_cast<GLsizei>(GfxDeviceGlobal::rboIds.size()), GfxDeviceGlobal::rboIds.data() );
    }
    if (!GfxDeviceGlobal::fboIds.empty())
    {
        glDeleteFramebuffers( static_cast<GLsizei>(GfxDeviceGlobal::fboIds.size()), GfxDeviceGlobal::fboIds.data() );
    }

    for (auto i : GfxDeviceGlobal::shaderIds)
    {
        glDeleteShader( i );
    }

    for (auto i : GfxDeviceGlobal::programIds)
    {
        glDeleteProgram( i );
    }
}

unsigned ae3d::GfxDevice::CreateTextureId()
{
    GLuint outId;
    glGenTextures(1, &outId);
    GfxDeviceGlobal::textureIds.push_back( outId );
    return outId;
}

unsigned ae3d::GfxDevice::CreateVaoId()
{
    GLuint outId;
    glGenVertexArrays(1, &outId);
    GfxDeviceGlobal::vaoIds.push_back( outId );
    return outId;
}

unsigned ae3d::GfxDevice::CreateBufferId()
{
    GLuint outId;
    glGenBuffers(1, &outId);
    GfxDeviceGlobal::bufferIds.push_back( outId );
    return outId;
}

unsigned ae3d::GfxDevice::CreateShaderId( unsigned shaderType )
{
    GLuint outId = glCreateShader( shaderType );
    GfxDeviceGlobal::shaderIds.push_back( outId );
    return outId;
}

unsigned ae3d::GfxDevice::CreateProgramId()
{
    GLuint outId = glCreateProgram();
    GfxDeviceGlobal::programIds.push_back( outId );
    return outId;
}

unsigned ae3d::GfxDevice::CreateRboId()
{
    GLuint outId;
    glGenRenderbuffers( 1, &outId );
    GfxDeviceGlobal::rboIds.push_back( outId );
    return outId;
}

unsigned ae3d::GfxDevice::CreateFboId()
{
    GLuint outId;
    glGenFramebuffers( 1, &outId );
    GfxDeviceGlobal::fboIds.push_back( outId );
    return outId;
}

void ae3d::GfxDevice::ClearScreen( unsigned clearFlags )
{
    GLbitfield mask = 0;

    if ((clearFlags & ClearFlags::Color) != 0)
    {
        mask |= GL_COLOR_BUFFER_BIT;
    }
    if ((clearFlags & ClearFlags::Depth) != 0)
    {
        mask |= GL_DEPTH_BUFFER_BIT;
    }

    if (mask != 0)
    {
        glDepthMask( GL_TRUE );
        glClear( mask );
    }
}

void ae3d::GfxDevice::SetClearColor( float red, float green, float blue )
{
    glClearColor( red, green, blue, 1 );
}

const char* GetGLErrorString(GLenum code)
{
    switch (code)
    {
        case GL_OUT_OF_MEMORY: return "GL_OUT_OF_MEMORY";
        case GL_INVALID_ENUM: return "GL_INVALID_ENUM";
        case GL_INVALID_VALUE: return "GL_INVALID_VALUE";
        case GL_INVALID_OPERATION: return "GL_INVALID_OPERATION";
        case GL_INVALID_FRAMEBUFFER_OPERATION: return "GL_INVALID_FRAMEBUFFER_OPERATION";
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT: return "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT: return "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
        case GL_FRAMEBUFFER_UNSUPPORTED: return "GL_FRAMEBUFFER_UNSUPPORTED";
        case GL_FRAMEBUFFER_UNDEFINED: return "GL_FRAMEBUFFER_UNDEFINED";
        default: return "other GL error";
    }
}

void ae3d::GfxDevice::ErrorCheck(const char* info)
{
        (void)info;
#if defined _DEBUG || defined DEBUG
        GLenum errorCode = GL_INVALID_ENUM;

        while ((errorCode = glGetError()) != GL_NO_ERROR)
        {
            System::Print("%s caused an OpenGL error: %s\n", info, GetGLErrorString( errorCode ) );
            System::Assert(false, "OpenGL error!");
        }
#endif
}

void ae3d::GfxDevice::ErrorCheckFBO()
{
#if defined _DEBUG || defined DEBUG
    const GLenum errorCode = glCheckFramebufferStatus( GL_FRAMEBUFFER );
    
    if (errorCode == GL_FRAMEBUFFER_COMPLETE)
    {
        return;
    }
    
    System::Print( "FBO error: %s\n", GetGLErrorString(errorCode) );
    System::Assert( false, "OpenGL FBO error." );
#endif
}

bool ae3d::GfxDevice::HasExtension( const char* glExtension )
{
    static std::vector< std::string > sExtensions;
    
    if (sExtensions.empty())
    {
        int count;
        glGetIntegerv( GL_NUM_EXTENSIONS, &count );
        sExtensions.resize( (std::size_t)count );
        
        for (int i = 0; i < count; ++i)
        {
            sExtensions[ i ] = std::string( (const char*)glGetStringi( GL_EXTENSIONS, i ) );
        }
    }
    
    return std::find( std::begin( sExtensions ), std::end( sExtensions ), glExtension ) != std::end( sExtensions );
}

void ae3d::GfxDevice::SetRenderTarget( RenderTexture* target, unsigned cubeMapFace )
{
    if (target != nullptr && target->GetFBO() == GfxDeviceGlobal::cachedFBO && cubeMapFace == 0)
    {
        return;
    }
    if (target == nullptr && GfxDeviceGlobal::cachedFBO == GfxDeviceGlobal::systemFBO)
    {
        return;
    }

    const GLuint fbo = target != nullptr ? target->GetFBO() : GfxDeviceGlobal::systemFBO;
    Statistics::IncRenderTargetBinds();
    glBindFramebuffer( GL_FRAMEBUFFER, fbo );
    GfxDeviceGlobal::cachedFBO = fbo;

    if (target && target->IsCube())
    {
        const unsigned glCubeMapFace = GL_TEXTURE_CUBE_MAP_POSITIVE_X + cubeMapFace;
        System::Assert( glCubeMapFace >= GL_TEXTURE_CUBE_MAP_POSITIVE_X && glCubeMapFace <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, "Invalid cube map face." );
        glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, glCubeMapFace, target->GetID(), 0 );
    }
    
    if (target != nullptr)
    {
        glViewport( 0, 0, target->GetWidth(), target->GetHeight() );
    }
    else
    {
        ae3d::System::Assert( GfxDeviceGlobal::backBufferWidth > 0, "invalid backbuffer dimension" );
        glViewport( 0, 0, GfxDeviceGlobal::backBufferWidth, GfxDeviceGlobal::backBufferHeight );
    }

    ErrorCheckFBO();
    ErrorCheck( "SetRenderTarget end" );
}

void ae3d::GfxDevice::SetBackBufferDimensionAndFBO( int width, int height )
{
    GfxDeviceGlobal::backBufferWidth = width;
    GfxDeviceGlobal::backBufferHeight = height;
    int fboId = 0;
    glGetIntegerv( GL_FRAMEBUFFER_BINDING, &fboId );
    GfxDeviceGlobal::systemFBO = static_cast< unsigned >(fboId);
}

void ae3d::GfxDevice::Set_sRGB_Writes( bool enable )
{
    if (enable)
    {
        glEnable( GL_FRAMEBUFFER_SRGB );
    }
    else
    {
        glDisable( GL_FRAMEBUFFER_SRGB );
    }
}

void ae3d::GfxDevice::DebugBlitFBO( unsigned handle, int width, int height )
{
    glBindFramebuffer( GL_READ_FRAMEBUFFER, handle );
    glBlitFramebuffer( 0, 0, 512, 512, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_LINEAR );
}

unsigned ae3d::GfxDevice::GetSystemFBO()
{
    return GfxDeviceGlobal::systemFBO;
}

void ae3d::GfxDevice::SetSystemFBO( unsigned fbo )
{
    GfxDeviceGlobal::systemFBO = fbo;
}

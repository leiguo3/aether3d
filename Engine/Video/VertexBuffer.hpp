#ifndef VERTEX_BUFFER_H
#define VERTEX_BUFFER_H

#if defined( AETHER3D_IOS ) && !defined(__i386__)
#import <Metal/Metal.h>
#endif
#include "Vec3.hpp"

namespace ae3d
{
    class VertexBuffer
    {
    public:
        struct Face
        {
            Face() : a(0), b(0), c(0) {}
            
            Face(unsigned short fa, unsigned short fb, unsigned short fc)
            : a(fa)
            , b(fb)
            , c(fc)
            {}
            
            unsigned short a, b, c;
        };

        struct VertexPTC
        {
            VertexPTC() {}
            VertexPTC(const Vec3& pos, float aU, float aV)
            : position(pos)
            , u(aU)
            , v(aV)
            {
            }
            
            Vec3 position;
            float u, v;
            Vec4 color;
        };

        // Binds the buffer. Must be called before Draw or DrawRange.
        void Bind() const;

        // Generates the buffer from supplied geometry.
        // \param faces Faces.
        // \param faceCount Face count.
        // \param vertices Vertices.
        // \param vertexCount Vertex count.
        void Generate( const Face* faces, int faceCount, const VertexPTC* vertices, int vertexCount );
        void Draw() const;
        /// <summary>
        /// Draws a range of triangles.
        /// </summary>
        /// <param name="start">Start</param>
        void DrawRange( int start, int end ) const;

    private:
        unsigned vaoId = 0;
        unsigned vboId = 0;
        unsigned iboId = 0;
        int elementCount = 0;
#if AETHER3D_IOS && !defined( __i386__ )
        id<MTLBuffer> vertexBuffer;
        id<MTLBuffer> indexBuffer;
#endif
    };
}
#endif

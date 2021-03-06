#ifndef QUATERNION_H
#define QUATERNION_H

#include <cmath>
#include <cassert>
#include "Vec3.hpp"
#include "Matrix.hpp"

namespace ae3d
{
    /// Stores an orientation.
    struct Quaternion
    {
        /// Constructor.
        Quaternion() : x( 0 ), y( 0 ), z( 0 ), w( 1 ) {}
        
        /**
         \param vec Vector.
         \param aW w.
         */
        Quaternion( const Vec3& vec, float aW ) : x( vec.x ), y( vec.y ), z( vec.z ), w( aW ) {}

        /// Copy constructor
        Quaternion( const Quaternion& v ) = default;
        
        /// Move constructor
        Quaternion( Quaternion&& v ) noexcept = default;

        /// Move assignment
        Quaternion& operator=( Quaternion&& ) noexcept = default;
        
        /// Copy
        Quaternion& operator=( const Quaternion& ) = default;

        /**
         \brief Multiplying a quaternion q with a vector v applies the q-rotation to vec.
         
         \param vec Input vector.
         \return Output vector that's rotated by this Quaternion.
         */
        Vec3 operator*( const Vec3& vec ) const
        {
            const Vec3 wComponent( w, w, w );
            const Vec3 two( 2.0f, 2.0f, 2.0f );
            
            const Vec3 vT = two * Vec3::Cross( Vec3( x, y, z ), vec );
            return vec + (wComponent * vT) + Vec3::Cross( Vec3( x, y, z ), vT );
        }
        
        /**
         \brief Applies rotation aQ to this quaternion and returns the result.
         
         \return This quaterion rotated by aQ.
         */
        Quaternion operator*( const Quaternion& aQ ) const
        {
            return Quaternion( Vec3( w * aQ.x + x * aQ.w + y * aQ.z - z * aQ.y,
                                     w * aQ.y + y * aQ.w + z * aQ.x - x * aQ.z,
                                     w * aQ.z + z * aQ.w + x * aQ.y - y * aQ.x ),
                                     w * aQ.w - x * aQ.x - y * aQ.y - z * aQ.z );
        }
        
        /**
         Equality operator.
         \param q Tested Quaternion.
         \return True if quaternions are component-wise equal, false otherwise.
         */
        bool operator==( const Quaternion& q ) const
        {
            const float acceptableDelta = 0.00001f;

            return std::fabs(x - q.x) < acceptableDelta &&
                   std::fabs(y - q.y) < acceptableDelta &&
                   std::fabs(z - q.z) < acceptableDelta &&
                   std::fabs(w - q.w) < acceptableDelta;
        }
        
        /**
         Inequality operator.
         \param q Tested Quaternion.
         \return True if quaternions are not component-wise equal, false otherwise.
         */
        bool operator!=( const Quaternion& q ) const
        {
            return !(*this == q);
        }
        
        /**
         Gets conjugate.  If the quaternion is of unit
         length, the conjugate is the inverse.
         
         \return Conjugate.
         */
        Quaternion Conjugate() const
        {
            return Quaternion( Vec3( -x, -y, -z ), w );
        }
        
        /**
         \brief Finds twist angle around axis.
         
         \param axis Axis.
         \return Twist angle ( \todo in radians? )
         */
        float FindTwist( const Vec3& axis ) const
        {
            // Get the plane the axis is a normal of.
            Vec3 orthonormal1, orthonormal2;
            FindOrthonormals( axis, orthonormal1, orthonormal2 );
            
            Vec3 transformed = *this * orthonormal1;// = Vec3.Transform(orthonormal1, q);
            
            //project transformed vector onto plane
            Vec3 flattened = transformed -  axis * Vec3::Dot( transformed, axis );
            flattened = flattened.Normalized();
            
            // get angle between original vector and projected transform to get angle around normal
            return std::acos( Vec3::Dot( orthonormal1, flattened ) );
        }
        
        /**
         \param outAxis Axis.
         \param outAngleRad in radians.
         */
        void GetAxisAngle( Vec3& outAxis, float& outAngleRad ) const
        {
            const float scale = std::sqrt( x * x + y * y + z * z );
            
            if (std::fabs( scale ) < 0.00001f)
            {
                outAxis = Vec3( 0, 0, 0 );
            }
            else
            {
                outAxis.x = x / scale;
                outAxis.y = y / scale;
                outAxis.z = z / scale;
            }
            
            outAngleRad = std::acos( w ) * 2;
        }
        
        /// \param euler Euler angles in degrees.
        /// \return Quaternion created from euler.
        static Quaternion FromEuler( const Vec3& euler )
        {
            const Vec3 xAxis( 1, 0, 0 );
            const Vec3 yAxis( 0, 1, 0 );
            const Vec3 zAxis( 0, 0, 1 );
            
            Quaternion qx, qy, qz, qt;
            qx.FromAxisAngle( xAxis, euler.x );
            qy.FromAxisAngle( yAxis, euler.y );
            qz.FromAxisAngle( zAxis, euler.z );
            qt = qx * qy;
            return qt * qz;
        }
        
        /**
         \param axis Axis.  Should be normalized.
         \param angleDeg Angle in degrees.
         */
        void FromAxisAngle( const Vec3& axis, float angleDeg )
        {
            float angleRad = angleDeg * (3.1418693659f / 180.0f);
            
            angleRad *= 0.5f;
            
            const float sinAngle = std::sin( angleRad );
            
            x = axis.x * sinAngle;
            y = axis.y * sinAngle;
            z = axis.z * sinAngle;
            w = std::cos( angleRad );
        }
        
        /**
         \param axis Axis.  Should be normalized.
         \param angleDeg Angle in degrees.
         */
        static Quaternion CreateFromAxisAngle( const Vec3& axis, float angleDeg )
        {
            Quaternion q;
            q.FromAxisAngle( axis, angleDeg );
            return q;
        }
        
        // Converts a matrix into a Quaternion.
        void FromMatrix( const Matrix44& mat )
        {
            float t;

            if (mat.m[ 10 ] < 0)
            {
                if (mat.m[ 0 ] > mat.m[ 5 ])
                {
                    t = 1 + mat.m[ 0 ] - mat.m[ 5 ] - mat.m[ 10 ];
                    *this = Quaternion( Vec3( t, mat.m[ 1 ] + mat.m[ 4 ], mat.m[ 8 ] + mat.m[ 2 ] ), mat.m[ 6 ] - mat.m[ 9 ] );
                }
                else
                {
                    t = 1 - mat.m[ 0 ] + mat.m[ 5 ] - mat.m[ 10 ];
                    *this = Quaternion( Vec3( mat.m[ 1 ] + mat.m[ 4 ], t, mat.m[ 6 ] + mat.m[ 9 ] ), mat.m[ 8 ] - mat.m[ 2 ] );
                }
            }
            else
            {
                if (mat.m[ 0 ] < -mat.m[ 5 ])
                {
                    t = 1 - mat.m[ 0 ] - mat.m[ 5 ] + mat.m[ 10 ];
                    *this = Quaternion( Vec3( mat.m[ 8 ] + mat.m[ 2 ], mat.m[ 6 ] + mat.m[ 9 ], t ), mat.m[ 1 ] - mat.m[ 4 ] );
                }
                else
                {
                    t = 1 + mat.m[ 0 ] + mat.m[ 5 ] + mat.m[ 10 ];
                    *this = Quaternion( Vec3( mat.m[ 6 ] - mat.m[ 9 ], mat.m[ 8 ] - mat.m[ 2 ], mat.m[ 1 ] - mat.m[ 4 ] ), t );
                }
            }
            
            const float factor = 0.5f / std::sqrt( t );
            x *= factor;
            y *= factor;
            z *= factor;
            w *= factor;
        }

        /**
         Gets matrix for unit-length quaternion.
         
         \param outMatrix column-major order matrix.
         */
        void GetMatrix( Matrix44& outMatrix ) const
        {
            const float x2 = x * x;
            const float y2 = y * y;
            const float z2 = z * z;
            const float xy = x * y;
            const float xz = x * z;
            const float yz = y * z;
            const float wx = w * x;
            const float wy = w * y;
            const float wz = w * z;
            
            outMatrix.m[ 0] = 1 - 2 * (y2 + z2);
            outMatrix.m[ 1] = 2 * (xy - wz);
            outMatrix.m[ 2] = 2 * (xz + wy);
            outMatrix.m[ 3] = 0;
            outMatrix.m[ 4] = 2 * (xy + wz);
            outMatrix.m[ 5] = 1 - 2 * (x2 + z2);
            outMatrix.m[ 6] = 2 * (yz - wx);
            outMatrix.m[ 7] = 0;
            outMatrix.m[ 8] = 2 * (xz - wy);
            outMatrix.m[ 9] = 2 * (yz + wx);
            outMatrix.m[10] = 1 - 2 * (x2 + y2);
            outMatrix.m[11] = 0;
            outMatrix.m[12] = 0;
            outMatrix.m[13] = 0;
            outMatrix.m[14] = 0;
            outMatrix.m[15] = 1;
        }
        
        /**
         http://www.euclideanspace.com/maths/geometry/rotations/conversions/quaternionToEuler/
         \return Euler angles in degrees.
         */
        Vec3 GetEuler() const
        {
            Vec3 out;
            out.z = std::atan2( 2 * y * w - 2 * x * z, 1 - 2 * y * y - 2 * z * z );
            out.y = std::asin( 2 * x * y + 2 * z * w );
            out.x = std::atan2( 2 * x * w - 2 * y * z, 1 - 2 * x * x - 2 * z * z );
            return out / (3.14159265358979f / 180.0f);
        }
        
        /** Normalizes the quaternion if it's not near unit-length already. */
        void Normalize()
        {
            const float mag2 = w * w + x * x + y * y + z * z;
            const float acceptableDelta = 0.00001f;
            
            if (std::fabs( mag2 ) > acceptableDelta && std::fabs( mag2 - 1.0f ) > acceptableDelta)
            {
                const float oneOverMag = 1.0f / std::sqrt( mag2 );
                
                x *= oneOverMag;
                y *= oneOverMag;
                z *= oneOverMag;
                w *= oneOverMag;
            }
        }
        
        /** X component. */
        float x;
        /** Y component. */
        float y;
        /** Z component. */
        float z;
        /** W component. */
        float w;
        
    private:
        void FindOrthonormals( const Vec3& normal, Vec3& orthonormal1, Vec3& orthonormal2 ) const
        {
            Matrix44 orthoX( 90,  0, 0 );
            Matrix44 orthoY(  0, 90, 0 );
            
            Vec3 ww;
            Matrix44::TransformDirection( normal, orthoX, &ww );
            const float dot = Vec3::Dot( normal, ww );
            
            if (std::fabs( dot ) > 0.6f)
            {
                Matrix44::TransformDirection( normal, orthoY, &ww );
            }
            
            ww = ww.Normalized();
            
            orthonormal1 = Vec3::Cross( normal, ww );
            orthonormal1 = orthonormal1.Normalized();
            orthonormal2 = Vec3::Cross( normal, orthonormal1 );
            orthonormal2 = orthonormal2.Normalized();
        }
    };
}
#endif

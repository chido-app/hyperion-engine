#include "Vector3.hpp"
#include "Quaternion.hpp"
#include "Matrix3.hpp"
#include "Matrix4.hpp"

#include <cmath>

namespace hyperion {

template<>
const Vec3<Int> math::detail::Vec3<Int>::zero = { 0, 0, 0 };
template<>
const Vec3<Int> math::detail::Vec3<Int>::one = { 1, 1, 1 };

template<>
const Vec3<UInt> math::detail::Vec3<UInt>::zero = { 0, 0, 0 };
template<>
const Vec3<UInt> math::detail::Vec3<UInt>::one = { 1, 1, 1 };

const Vec3<Float> math::detail::Vec3<Float>::zero = { 0, 0, 0 };
const Vec3<Float> math::detail::Vec3<Float>::one = { 1, 1, 1 };

Vec3<Float> math::detail::Vec3<Float>::operator*(const Matrix3 &mat) const
{
    return {
        x * mat.values[0] + y * mat.values[3] + z * mat.values[6],
        x * mat.values[1] + y * mat.values[4] + z * mat.values[7],
        x * mat.values[2] + y * mat.values[5] + z * mat.values[8]
    };
}

Vec3<Float> &math::detail::Vec3<Float>::operator*=(const Matrix3 &mat)
{
    return operator=(operator*(mat));
}

Vec3<Float> math::detail::Vec3<Float>::operator*(const Matrix4 &mat) const
{
    Vector4 product {
        x * mat.values[0] + y * mat.values[4] + z * mat.values[8]  + mat.values[12],
        x * mat.values[1] + y * mat.values[5] + z * mat.values[9]  + mat.values[13],
        x * mat.values[2] + y * mat.values[6] + z * mat.values[10] + mat.values[14],
        x * mat.values[3] + y * mat.values[7] + z * mat.values[11] + mat.values[15]
    };

    product /= product.w;

    return {
        product.x,
        product.y,
        product.z
    };
}

Vec3<Float> &math::detail::Vec3<Float>::operator*=(const Matrix4 &mat)
{
    return operator=(operator*(mat));
}

Vec3<Float> math::detail::Vec3<Float>::operator*(const Quaternion &quat) const
{
    Vec3<Float> result;
    result.x = quat.w * quat.w * x + 2 * 
        quat.y * quat.w * z - 2 * 
        quat.z * quat.w * y + 
        quat.x * quat.x * x + 2 * 
        quat.y * quat.x * y + 2 * 
        quat.z * quat.x * z - 
        quat.z * quat.z * x - quat.y * quat.y * x;

    result.y = 2 * quat.x * quat.y * x + 
        quat.y * quat.y * y + 2 * 
        quat.z * quat.y * z + 2 * 
        quat.w * quat.z * x - 
        quat.z * quat.z * y + 
        quat.w * quat.w * y - 2 * 
        quat.x * quat.w * z - 
        quat.x * quat.x * y;

    result.z = 2 * quat.x * quat.z * x + 2 * 
        quat.y * quat.z * y + 
        quat.z * quat.z * z - 2 * 
        quat.w * quat.y * x - 
        quat.y * quat.y * z + 2 * 
        quat.w * quat.x * y - 
        quat.x * quat.x * z + 
        quat.w * quat.w * z;
    return result;
}

Vec3<Float> &math::detail::Vec3<Float>::operator*=(const Quaternion &quat)
{
    return operator=(operator*(quat));
}

template<>
Int math::detail::Vec3<Int>::Max() const
{
    return MathUtil::Max(x, MathUtil::Max(y, z));
}

template<>
Int math::detail::Vec3<Int>::Min() const
{
    return MathUtil::Min(x, MathUtil::Min(y, z));
}

template<>
UInt math::detail::Vec3<UInt>::Max() const
{
    return MathUtil::Max(x, MathUtil::Max(y, z));
}

template<>
UInt math::detail::Vec3<UInt>::Min() const
{
    return MathUtil::Min(x, MathUtil::Min(y, z));
}

Float math::detail::Vec3<Float>::Max() const
{
    return MathUtil::Max(x, MathUtil::Max(y, z));
}

Float math::detail::Vec3<Float>::Min() const
{
    return MathUtil::Min(x, MathUtil::Min(y, z));
}

Float math::detail::Vec3<Float>::DistanceSquared(const Vec3f &other) const
{
    Float dx = x - other.x;
    Float dy = y - other.y;
    Float dz = z - other.z;
    return dx * dx + dy * dy + dz * dz;
}

/* Euclidean distance */
Float math::detail::Vec3<Float>::Distance(const Vec3f &other) const
{
    return MathUtil::Sqrt(DistanceSquared(other));
}

Vec3<Float> math::detail::Vec3<Float>::Normalized() const
{
    return *this / MathUtil::Max(Length(), MathUtil::epsilon_f);
}

Vec3<Float> &math::detail::Vec3<Float>::Normalize()
{
    return *this /= MathUtil::Max(Length(), MathUtil::epsilon_f);
}

Vec3<Float> math::detail::Vec3<Float>::Cross(const Vec3<Float> &other) const
{
    return {
        y * other.z - z * other.y,
        z * other.x - x * other.z,
        x * other.y - y * other.x
    };
}

Vec3<Float> &math::detail::Vec3<Float>::Rotate(const Vec3<Float> &axis, float radians)
{
    return (*this) = Matrix4::Rotation(axis, radians) * (*this);
}

Vec3<Float> &math::detail::Vec3<Float>::Lerp(const Vec3<Float> &to, const float amt)
{
    x = MathUtil::Lerp(x, to.x, amt);
    y = MathUtil::Lerp(y, to.y, amt);
    z = MathUtil::Lerp(z, to.z, amt);

    return *this;
}

Float math::detail::Vec3<Float>::Dot(const Vec3<Float> &other) const
{
    return x * other.x + y * other.y + z * other.z;
}

Float math::detail::Vec3<Float>::AngleBetween(const Vector3 &other) const
{
    const float dot_product = x * other.x + y * other.y + z * other.z;
    const float arc_cos = MathUtil::Arccos(dot_product);

    return arc_cos / (Length() * other.Length());
}

Vec3<Float> math::detail::Vec3<Float>::Abs(const Vec3<Float> &vec)
{
    return {
        MathUtil::Abs(vec.x),
        MathUtil::Abs(vec.y),
        MathUtil::Abs(vec.z)
    };
}

Vec3<Float> math::detail::Vec3<Float>::Round(const Vec3<Float> &vec)
{
    return {
        MathUtil::Round(vec.x),
        MathUtil::Round(vec.y),
        MathUtil::Round(vec.z)
    };
}

Vec3<Float> math::detail::Vec3<Float>::Clamp(const Vec3<Float> &vec, Float min_value, Float max_value)
{
    return Max(min_value, Min(vec, max_value));
}

Vec3<Float> math::detail::Vec3<Float>::Min(const Vec3<Float> &a, const Vec3<Float> &b)
{
    return {
        MathUtil::Min(a.x, b.x),
        MathUtil::Min(a.y, b.y),
        MathUtil::Min(a.z, b.z)
    };
}

Vec3<Float> math::detail::Vec3<Float>::Max(const Vec3<Float> &a, const Vec3<Float> &b)
{
    return {
        MathUtil::Max(a.x, b.x),
        MathUtil::Max(a.y, b.y),
        MathUtil::Max(a.z, b.z)
    };
}

namespace math::detail {

std::ostream &operator<<(std::ostream &out, const math::detail::Vec3<Int> &vec) // output
{
    out << "[" << vec.x << ", " << vec.y << ", " << vec.z << "]";
    return out;
}

std::ostream &operator<<(std::ostream &out, const math::detail::Vec3<UInt> &vec) // output
{
    out << "[" << vec.x << ", " << vec.y << ", " << vec.z << "]";
    return out;
}
std::ostream &operator<<(std::ostream &out, const math::detail::Vec3<Float> &vec) // output
{
    out << "[" << vec.x << ", " << vec.y << ", " << vec.z << "]";
    return out;
}

} // namespace math::detail

} // namespace hyperion

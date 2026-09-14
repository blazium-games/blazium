using System;
using System.Diagnostics.CodeAnalysis;
using System.Globalization;
using System.Runtime.InteropServices;

#nullable enable

namespace Godot
{
    /// <summary>
    /// 4-element structure that can be used to represent positions in 4D space or any other pair of numeric values.
    /// </summary>
    [Serializable]
    [StructLayout(LayoutKind.Sequential)]
    public struct Vector4 : IEquatable<Vector4>
    {
        /// <summary>
        /// Enumerated index values for the axes.
        /// Returned by <see cref="MaxAxisIndex"/> and <see cref="MinAxisIndex"/>.
        /// </summary>
        public enum Axis
        {
            /// <summary>
            /// The vector's X axis.
            /// </summary>
            X = 0,
            /// <summary>
            /// The vector's Y axis.
            /// </summary>
            Y,
            /// <summary>
            /// The vector's Z axis.
            /// </summary>
            Z,
            /// <summary>
            /// The vector's W axis.
            /// </summary>
            W
        }

        /// <summary>
        /// The vector's X component. Also accessible by using the index position <c>[0]</c>.
        /// </summary>
        public real_t X;

        /// <summary>
        /// The vector's Y component. Also accessible by using the index position <c>[1]</c>.
        /// </summary>
        public real_t Y;

        /// <summary>
        /// The vector's Z component. Also accessible by using the index position <c>[2]</c>.
        /// </summary>
        public real_t Z;

        /// <summary>
        /// The vector's W component. Also accessible by using the index position <c>[3]</c>.
        /// </summary>
        public real_t W;

        /// <summary>
        /// Access vector components using their index.
        /// </summary>
        /// <exception cref="ArgumentOutOfRangeException">
        /// <paramref name="index"/> is not 0, 1, 2 or 3.
        /// </exception>
        /// <value>
        /// <c>[0]</c> is equivalent to <see cref="X"/>,
        /// <c>[1]</c> is equivalent to <see cref="Y"/>,
        /// <c>[2]</c> is equivalent to <see cref="Z"/>.
        /// <c>[3]</c> is equivalent to <see cref="W"/>.
        /// </value>
        public real_t this[int index]
        {
            readonly get
            {
                switch (index)
                {
                    case 0:
                        return X;
                    case 1:
                        return Y;
                    case 2:
                        return Z;
                    case 3:
                        return W;
                    default:
                        throw new ArgumentOutOfRangeException(nameof(index));
                }
            }
            set
            {
                switch (index)
                {
                    case 0:
                        X = value;
                        return;
                    case 1:
                        Y = value;
                        return;
                    case 2:
                        Z = value;
                        return;
                    case 3:
                        W = value;
                        return;
                    default:
                        throw new ArgumentOutOfRangeException(nameof(index));
                }
            }
        }

        /// <summary>
        /// Helper method for deconstruction into a tuple.
        /// </summary>
        public readonly void Deconstruct(out real_t x, out real_t y, out real_t z, out real_t w)
        {
            x = X;
            y = Y;
            z = Z;
            w = W;
        }

        internal void Normalize()
        {
            real_t lengthsq = LengthSquared();

            if (lengthsq == 0)
            {
                X = Y = Z = W = 0f;
            }
            else
            {
                real_t length = Mathf.Sqrt(lengthsq);
                X /= length;
                Y /= length;
                Z /= length;
                W /= length;
            }
        }

        /// <summary>
        /// Returns a new vector with all components in absolute values (i.e. positive).
        /// </summary>
        /// <returns>A vector with <see cref="Mathf.Abs(real_t)"/> called on each component.</returns>
        public readonly Vector4 Abs()
        {
            return new Vector4(Mathf.Abs(X), Mathf.Abs(Y), Mathf.Abs(Z), Mathf.Abs(W));
        }

        /// <summary>
        /// Returns a new vector with all components rounded up (towards positive infinity).
        /// </summary>
        /// <returns>A vector with <see cref="Mathf.Ceil(real_t)"/> called on each component.</returns>
        public readonly Vector4 Ceil()
        {
            return new Vector4(Mathf.Ceil(X), Mathf.Ceil(Y), Mathf.Ceil(Z), Mathf.Ceil(W));
        }

        /// <summary>
        /// Returns a new vector with all components clamped between the
        /// components of <paramref name="min"/> and <paramref name="max"/> using
        /// <see cref="Mathf.Clamp(real_t, real_t, real_t)"/>.
        /// </summary>
        /// <param name="min">The vector with minimum allowed values.</param>
        /// <param name="max">The vector with maximum allowed values.</param>
        /// <returns>The vector with all components clamped.</returns>
        public readonly Vector4 Clamp(Vector4 min, Vector4 max)
        {
            return new Vector4
            (
                Mathf.Clamp(X, min.X, max.X),
                Mathf.Clamp(Y, min.Y, max.Y),
                Mathf.Clamp(Z, min.Z, max.Z),
                Mathf.Clamp(W, min.W, max.W)
            );
        }

        /// <summary>
        /// Returns a new vector with all components clamped between the
        /// <paramref name="min"/> and <paramref name="max"/> using
        /// <see cref="Mathf.Clamp(real_t, real_t, real_t)"/>.
        /// </summary>
        /// <param name="min">The minimum allowed value.</param>
        /// <param name="max">The maximum allowed value.</param>
        /// <returns>The vector with all components clamped.</returns>
        public readonly Vector4 Clamp(real_t min, real_t max)
        {
            return new Vector4
            (
                Mathf.Clamp(X, min, max),
                Mathf.Clamp(Y, min, max),
                Mathf.Clamp(Z, min, max),
                Mathf.Clamp(W, min, max)
            );
        }

        /// <summary>
        /// Performs a cubic interpolation between vectors <paramref name="preA"/>, this vector,
        /// <paramref name="b"/>, and <paramref name="postB"/>, by the given amount <paramref name="weight"/>.
        /// </summary>
        /// <param name="b">The destination vector.</param>
        /// <param name="preA">A vector before this vector.</param>
        /// <param name="postB">A vector after <paramref name="b"/>.</param>
        /// <param name="weight">A value on the range of 0.0 to 1.0, representing the amount of interpolation.</param>
        /// <returns>The interpolated vector.</returns>
        public readonly Vector4 CubicInterpolate(Vector4 b, Vector4 preA, Vector4 postB, real_t weight)
        {
            return new Vector4
            (
                Mathf.CubicInterpolate(X, b.X, preA.X, postB.X, weight),
                Mathf.CubicInterpolate(Y, b.Y, preA.Y, postB.Y, weight),
                Mathf.CubicInterpolate(Z, b.Z, preA.Z, postB.Z, weight),
                Mathf.CubicInterpolate(W, b.W, preA.W, postB.W, weight)
            );
        }

        /// <summary>
        /// Performs a cubic interpolation between vectors <paramref name="preA"/>, this vector,
        /// <paramref name="b"/>, and <paramref name="postB"/>, by the given amount <paramref name="weight"/>.
        /// It can perform smoother interpolation than <see cref="CubicInterpolate"/>
        /// by the time values.
        /// </summary>
        /// <param name="b">The destination vector.</param>
        /// <param name="preA">A vector before this vector.</param>
        /// <param name="postB">A vector after <paramref name="b"/>.</param>
        /// <param name="weight">A value on the range of 0.0 to 1.0, representing the amount of interpolation.</param>
        /// <param name="t"></param>
        /// <param name="preAT"></param>
        /// <param name="postBT"></param>
        /// <returns>The interpolated vector.</returns>
        public readonly Vector4 CubicInterpolateInTime(Vector4 b, Vector4 preA, Vector4 postB, real_t weight, real_t t, real_t preAT, real_t postBT)
        {
            return new Vector4
            (
                Mathf.CubicInterpolateInTime(X, b.X, preA.X, postB.X, weight, t, preAT, postBT),
                Mathf.CubicInterpolateInTime(Y, b.Y, preA.Y, postB.Y, weight, t, preAT, postBT),
                Mathf.CubicInterpolateInTime(Z, b.Z, preA.Z, postB.Z, weight, t, preAT, postBT),
                Mathf.CubicInterpolateInTime(W, b.W, preA.W, postB.W, weight, t, preAT, postBT)
            );
        }

        /// <summary>
        /// Returns the normalized vector pointing from this vector to <paramref name="to"/>.
        /// </summary>
        /// <param name="to">The other vector to point towards.</param>
        /// <returns>The direction from this vector to <paramref name="to"/>.</returns>
        public readonly Vector4 DirectionTo(Vector4 to)
        {
            Vector4 ret = new Vector4(to.X - X, to.Y - Y, to.Z - Z, to.W - W);
            ret.Normalize();
            return ret;
        }

        /// <summary>
        /// Returns the squared distance between this vector and <paramref name="to"/>.
        /// This method runs faster than <see cref="DistanceTo"/>, so prefer it if
        /// you need to compare vectors or need the squared distance for some formula.
        /// </summary>
        /// <param name="to">The other vector to use.</param>
        /// <returns>The squared distance between the two vectors.</returns>
        public readonly real_t DistanceSquaredTo(Vector4 to)
        {
            return (to - this).LengthSquared();
        }

        /// <summary>
        /// Returns the distance between this vector and <paramref name="to"/>.
        /// </summary>
        /// <param name="to">The other vector to use.</param>
        /// <returns>The distance between the two vectors.</returns>
        public readonly real_t DistanceTo(Vector4 to)
        {
            return (to - this).Length();
        }

        /// <summary>
        /// Returns the dot product of this vector and <paramref name="with"/>.
        /// </summary>
        /// <param name="with">The other vector to use.</param>
        /// <returns>The dot product of the two vectors.</returns>
        public readonly real_t Dot(Vector4 with)
        {
            return (X * with.X) + (Y * with.Y) + (Z * with.Z) + (W * with.W);
        }

        /// <summary>
        /// Returns a new vector with all components rounded down (towards negative infinity).
        /// </summary>
        /// <returns>A vector with <see cref="Mathf.Floor(real_t)"/> called on each component.</returns>
        public readonly Vector4 Floor()
        {
            return new Vector4(Mathf.Floor(X), Mathf.Floor(Y), Mathf.Floor(Z), Mathf.Floor(W));
        }

        /// <summary>
        /// Returns the inverse of this vector. This is the same as <c>new Vector4(1 / v.X, 1 / v.Y, 1 / v.Z, 1 / v.W)</c>.
        /// </summary>
        /// <returns>The inverse of this vector.</returns>
        public readonly Vector4 Inverse()
        {
            return new Vector4(1 / X, 1 / Y, 1 / Z, 1 / W);
        }

        /// <summary>
        /// Returns <see langword="true"/> if this vector is finite, by calling
        /// <see cref="Mathf.IsFinite(real_t)"/> on each component.
        /// </summary>
        /// <returns>Whether this vector is finite or not.</returns>
        public readonly bool IsFinite()
        {
            return Mathf.IsFinite(X) && Mathf.IsFinite(Y) && Mathf.IsFinite(Z) && Mathf.IsFinite(W);
        }

        /// <summary>
        /// Returns <see langword="true"/> if the vector is normalized, and <see langword="false"/> otherwise.
        /// </summary>
        /// <returns>A <see langword="bool"/> indicating whether or not the vector is normalized.</returns>
        public readonly bool IsNormalized()
        {
            return Mathf.IsEqualApprox(LengthSquared(), 1, Mathf.Epsilon);
        }

        /// <summary>
        /// Returns the length (magnitude) of this vector.
        /// </summary>
        /// <seealso cref="LengthSquared"/>
        /// <returns>The length of this vector.</returns>
        public readonly real_t Length()
        {
            real_t x2 = X * X;
            real_t y2 = Y * Y;
            real_t z2 = Z * Z;
            real_t w2 = W * W;

            return Mathf.Sqrt(x2 + y2 + z2 + w2);
        }

        /// <summary>
        /// Returns the squared length (squared magnitude) of this vector.
        /// This method runs faster than <see cref="Length"/>, so prefer it if
        /// you need to compare vectors or need the squared length for some formula.
        /// </summary>
        /// <returns>The squared length of this vector.</returns>
        public readonly real_t LengthSquared()
        {
            real_t x2 = X * X;
            real_t y2 = Y * Y;
            real_t z2 = Z * Z;
            real_t w2 = W * W;

            return x2 + y2 + z2 + w2;
        }

        /// <summary>
        /// Returns the result of the linear interpolation between
        /// this vector and <paramref name="to"/> by amount <paramref name="weight"/>.
        /// </summary>
        /// <param name="to">The destination vector for interpolation.</param>
        /// <param name="weight">A value on the range of 0.0 to 1.0, representing the amount of interpolation.</param>
        /// <returns>The resulting vector of the interpolation.</returns>
        public readonly Vector4 Lerp(Vector4 to, real_t weight)
        {
            return new Vector4
            (
                Mathf.Lerp(X, to.X, weight),
                Mathf.Lerp(Y, to.Y, weight),
                Mathf.Lerp(Z, to.Z, weight),
                Mathf.Lerp(W, to.W, weight)
            );
        }

        /// <summary>
        /// Returns the result of the component-wise maximum between
        /// this vector and <paramref name="with"/>.
        /// Equivalent to <c>new Vector4(Mathf.Max(X, with.X), Mathf.Max(Y, with.Y), Mathf.Max(Z, with.Z), Mathf.Max(W, with.W))</c>.
        /// </summary>
        /// <param name="with">The other vector to use.</param>
        /// <returns>The resulting maximum vector.</returns>
        public readonly Vector4 Max(Vector4 with)
        {
            return new Vector4
            (
                Mathf.Max(X, with.X),
                Mathf.Max(Y, with.Y),
                Mathf.Max(Z, with.Z),
                Mathf.Max(W, with.W)
            );
        }

        /// <summary>
        /// Returns the result of the component-wise maximum between
        /// this vector and <paramref name="with"/>.
        /// Equivalent to <c>new Vector4(Mathf.Max(X, with), Mathf.Max(Y, with), Mathf.Max(Z, with), Mathf.Max(W, with))</c>.
        /// </summary>
        /// <param name="with">The other value to use.</param>
        /// <returns>The resulting maximum vector.</returns>
        public readonly Vector4 Max(real_t with)
        {
            return new Vector4
            (
                Mathf.Max(X, with),
                Mathf.Max(Y, with),
                Mathf.Max(Z, with),
                Mathf.Max(W, with)
            );
        }

        /// <summary>
        /// Returns the result of the component-wise minimum between
        /// this vector and <paramref name="with"/>.
        /// Equivalent to <c>new Vector4(Mathf.Min(X, with.X), Mathf.Min(Y, with.Y), Mathf.Min(Z, with.Z), Mathf.Min(W, with.W))</c>.
        /// </summary>
        /// <param name="with">The other vector to use.</param>
        /// <returns>The resulting minimum vector.</returns>
        public readonly Vector4 Min(Vector4 with)
        {
            return new Vector4
            (
                Mathf.Min(X, with.X),
                Mathf.Min(Y, with.Y),
                Mathf.Min(Z, with.Z),
                Mathf.Min(W, with.W)
            );
        }

        /// <summary>
        /// Returns the result of the component-wise minimum between
        /// this vector and <paramref name="with"/>.
        /// Equivalent to <c>new Vector4(Mathf.Min(X, with), Mathf.Min(Y, with), Mathf.Min(Z, with), Mathf.Min(W, with))</c>.
        /// </summary>
        /// <param name="with">The other value to use.</param>
        /// <returns>The resulting minimum vector.</returns>
        public readonly Vector4 Min(real_t with)
        {
            return new Vector4
            (
                Mathf.Min(X, with),
                Mathf.Min(Y, with),
                Mathf.Min(Z, with),
                Mathf.Min(W, with)
            );
        }

        /// <summary>
        /// Returns the axis of the vector's highest value. See <see cref="Axis"/>.
        /// If all components are equal, this method returns <see cref="Axis.X"/>.
        /// </summary>
        /// <returns>The index of the highest axis.</returns>
        public readonly Axis MaxAxisIndex()
        {
            int max_index = 0;
            real_t max_value = X;
            for (int i = 1; i < 4; i++)
            {
                if (this[i] > max_value)
                {
                    max_index = i;
                    max_value = this[i];
                }
            }
            return (Axis)max_index;
        }

        /// <summary>
        /// Returns the axis of the vector's lowest value. See <see cref="Axis"/>.
        /// If all components are equal, this method returns <see cref="Axis.W"/>.
        /// </summary>
        /// <returns>The index of the lowest axis.</returns>
        public readonly Axis MinAxisIndex()
        {
            int min_index = 0;
            real_t min_value = X;
            for (int i = 1; i < 4; i++)
            {
                if (this[i] <= min_value)
                {
                    min_index = i;
                    min_value = this[i];
                }
            }
            return (Axis)min_index;
        }

        /// <summary>
        /// Returns the vector scaled to unit length. Equivalent to <c>v / v.Length()</c>.
        /// </summary>
        /// <returns>A normalized version of the vector.</returns>
        public readonly Vector4 Normalized()
        {
            Vector4 v = this;
            v.Normalize();
            return v;
        }

        /// <summary>
        /// Returns a vector composed of the <see cref="Mathf.PosMod(real_t, real_t)"/> of this vector's components
        /// and <paramref name="mod"/>.
        /// </summary>
        /// <param name="mod">A value representing the divisor of the operation.</param>
        /// <returns>
        /// A vector with each component <see cref="Mathf.PosMod(real_t, real_t)"/> by <paramref name="mod"/>.
        /// </returns>
        public readonly Vector4 PosMod(real_t mod)
        {
            return new Vector4(
                Mathf.PosMod(X, mod),
                Mathf.PosMod(Y, mod),
                Mathf.PosMod(Z, mod),
                Mathf.PosMod(W, mod)
            );
        }

        /// <summary>
        /// Returns a vector composed of the <see cref="Mathf.PosMod(real_t, real_t)"/> of this vector's components
        /// and <paramref name="modv"/>'s components.
        /// </summary>
        /// <param name="modv">A vector representing the divisors of the operation.</param>
        /// <returns>
        /// A vector with each component <see cref="Mathf.PosMod(real_t, real_t)"/> by <paramref name="modv"/>'s components.
        /// </returns>
        public readonly Vector4 PosMod(Vector4 modv)
        {
            return new Vector4(
                Mathf.PosMod(X, modv.X),
                Mathf.PosMod(Y, modv.Y),
                Mathf.PosMod(Z, modv.Z),
                Mathf.PosMod(W, modv.W)
            );
        }

        /// <summary>
        /// Returns this vector with all components rounded to the nearest integer,
        /// with halfway cases rounded towards the nearest multiple of two.
        /// </summary>
        /// <returns>The rounded vector.</returns>
        public readonly Vector4 Round()
        {
            return new Vector4(Mathf.Round(X), Mathf.Round(Y), Mathf.Round(Z), Mathf.Round(W));
        }

        /// <summary>
        /// Returns a vector with each component set to one or negative one, depending
        /// on the signs of this vector's components, or zero if the component is zero,
        /// by calling <see cref="Mathf.Sign(real_t)"/> on each component.
        /// </summary>
        /// <returns>A vector with all components as either <c>1</c>, <c>-1</c>, or <c>0</c>.</returns>
        public readonly Vector4 Sign()
        {
            Vector4 v;
            v.X = Mathf.Sign(X);
            v.Y = Mathf.Sign(Y);
            v.Z = Mathf.Sign(Z);
            v.W = Mathf.Sign(W);
            return v;
        }

        /// <summary>
        /// Returns a new vector with each component snapped to the nearest multiple of the corresponding component in <paramref name="step"/>.
        /// This can also be used to round to an arbitrary number of decimals.
        /// </summary>
        /// <param name="step">A vector value representing the step size to snap to.</param>
        /// <returns>The snapped vector.</returns>
        public readonly Vector4 Snapped(Vector4 step)
        {
            return new Vector4(
                Mathf.Snapped(X, step.X),
                Mathf.Snapped(Y, step.Y),
                Mathf.Snapped(Z, step.Z),
                Mathf.Snapped(W, step.W)
            );
        }

        /// <summary>
        /// Returns a new vector with each component snapped to the nearest multiple of <paramref name="step"/>.
        /// This can also be used to round to an arbitrary number of decimals.
        /// </summary>
        /// <param name="step">The step size to snap to.</param>
        /// <returns>The snapped vector.</returns>
        public readonly Vector4 Snapped(real_t step)
        {
            return new Vector4(
                Mathf.Snapped(X, step),
                Mathf.Snapped(Y, step),
                Mathf.Snapped(Z, step),
                Mathf.Snapped(W, step)
            );
        }

        // Constants
        private static readonly Vector4 _zero = new Vector4(0, 0, 0, 0);
        private static readonly Vector4 _one = new Vector4(1, 1, 1, 1);
        private static readonly Vector4 _inf = new Vector4(Mathf.Inf, Mathf.Inf, Mathf.Inf, Mathf.Inf);

        /// <summary>
        /// Zero vector, a vector with all components set to <c>0</c>.
        /// </summary>
        /// <value>Equivalent to <c>new Vector4(0, 0, 0, 0)</c>.</value>
        public static Vector4 Zero { get { return _zero; } }
        /// <summary>
        /// One vector, a vector with all components set to <c>1</c>.
        /// </summary>
        /// <value>Equivalent to <c>new Vector4(1, 1, 1, 1)</c>.</value>
        public static Vector4 One { get { return _one; } }
        /// <summary>
        /// Infinity vector, a vector with all components set to <see cref="Mathf.Inf"/>.
        /// </summary>
        /// <value>Equivalent to <c>new Vector4(Mathf.Inf, Mathf.Inf, Mathf.Inf, Mathf.Inf)</c>.</value>
        public static Vector4 Inf { get { return _inf; } }

#pragma warning disable CS1591 // Disable warning: "Missing XML comment for publicly visible type or member"
        public readonly Vector2 XX => new(X, X);
        public Vector2 XY
        {
            readonly get => new(X, Y);
            set
            {
                X = value.X;
                Y = value.Y;
            }
        }
        public Vector2 XZ
        {
            readonly get => new(X, Z);
            set
            {
                X = value.X;
                Z = value.Y;
            }
        }
        public Vector2 XW
        {
            readonly get => new(X, W);
            set
            {
                X = value.X;
                W = value.Y;
            }
        }
        public Vector2 YX
        {
            readonly get => new(Y, X);
            set
            {
                Y = value.X;
                X = value.Y;
            }
        }
        public readonly Vector2 YY => new(Y, Y);
        public Vector2 YZ
        {
            readonly get => new(Y, Z);
            set
            {
                Y = value.X;
                Z = value.Y;
            }
        }
        public Vector2 YW
        {
            readonly get => new(Y, W);
            set
            {
                Y = value.X;
                W = value.Y;
            }
        }
        public Vector2 ZX
        {
            readonly get => new(Z, X);
            set
            {
                Z = value.X;
                X = value.Y;
            }
        }
        public Vector2 ZY
        {
            readonly get => new(Z, Y);
            set
            {
                Z = value.X;
                Y = value.Y;
            }
        }
        public readonly Vector2 ZZ => new(Z, Z);
        public Vector2 ZW
        {
            readonly get => new(Z, W);
            set
            {
                Z = value.X;
                W = value.Y;
            }
        }
        public Vector2 WX
        {
            readonly get => new(W, X);
            set
            {
                W = value.X;
                X = value.Y;
            }
        }
        public Vector2 WY
        {
            readonly get => new(W, Y);
            set
            {
                W = value.X;
                Y = value.Y;
            }
        }
        public Vector2 WZ
        {
            readonly get => new(W, Z);
            set
            {
                W = value.X;
                Z = value.Y;
            }
        }
        public readonly Vector2 WW => new(W, W);

        public readonly Vector3 XXX => new(X, X, X);
        public readonly Vector3 XXY => new(X, X, Y);
        public readonly Vector3 XXZ => new(X, X, Z);
        public readonly Vector3 XXW => new(X, X, W);
        public readonly Vector3 XYX => new(X, Y, X);
        public readonly Vector3 XYY => new(X, Y, Y);
        public Vector3 XYZ
        {
            readonly get => new(X, Y, Z);
            set
            {
                X = value.X;
                Y = value.Y;
                Z = value.Z;
            }
        }
        public Vector3 XYW
        {
            readonly get => new(X, Y, W);
            set
            {
                X = value.X;
                Y = value.Y;
                W = value.Z;
            }
        }
        public readonly Vector3 XZX => new(X, Z, X);
        public Vector3 XZY
        {
            readonly get => new(X, Z, Y);
            set
            {
                X = value.X;
                Z = value.Y;
                Y = value.Z;
            }
        }
        public readonly Vector3 XZZ => new(X, Z, Z);
        public Vector3 XZW
        {
            readonly get => new(X, Z, W);
            set
            {
                X = value.X;
                Z = value.Y;
                W = value.Z;
            }
        }
        public readonly Vector3 XWX => new(X, W, X);
        public Vector3 XWY
        {
            readonly get => new(X, W, Y);
            set
            {
                X = value.X;
                W = value.Y;
                Y = value.Z;
            }
        }
        public Vector3 XWZ
        {
            readonly get => new(X, W, Z);
            set
            {
                X = value.X;
                W = value.Y;
                Z = value.Z;
            }
        }
        public readonly Vector3 XWW => new(X, W, W);
        public readonly Vector3 YXX => new(Y, X, X);
        public readonly Vector3 YXY => new(Y, X, Y);
        public Vector3 YXZ
        {
            readonly get => new(Y, X, Z);
            set
            {
                Y = value.X;
                X = value.Y;
                Z = value.Z;
            }
        }
        public Vector3 YXW
        {
            readonly get => new(Y, X, W);
            set
            {
                Y = value.X;
                X = value.Y;
                W = value.Z;
            }
        }
        public readonly Vector3 YYX => new(Y, Y, X);
        public readonly Vector3 YYY => new(Y, Y, Y);
        public readonly Vector3 YYZ => new(Y, Y, Z);
        public readonly Vector3 YYW => new(Y, Y, W);
        public Vector3 YZX
        {
            readonly get => new(Y, Z, X);
            set
            {
                Y = value.X;
                Z = value.Y;
                X = value.Z;
            }
        }
        public readonly Vector3 YZY => new(Y, Z, Y);
        public readonly Vector3 YZZ => new(Y, Z, Z);
        public Vector3 YZW
        {
            readonly get => new(Y, Z, W);
            set
            {
                Y = value.X;
                Z = value.Y;
                W = value.Z;
            }
        }
        public Vector3 YWX
        {
            readonly get => new(Y, W, X);
            set
            {
                Y = value.X;
                W = value.Y;
                X = value.Z;
            }
        }
        public readonly Vector3 YWY => new(Y, W, Y);
        public Vector3 YWZ
        {
            readonly get => new(Y, W, Z);
            set
            {
                Y = value.X;
                W = value.Y;
                Z = value.Z;
            }
        }
        public readonly Vector3 YWW => new(Y, W, W);
        public readonly Vector3 ZXX => new(Z, X, X);
        public Vector3 ZXY
        {
            readonly get => new(Z, X, Y);
            set
            {
                Z = value.X;
                X = value.Y;
                Y = value.Z;
            }
        }
        public readonly Vector3 ZXZ => new(Z, X, Z);
        public Vector3 ZXW
        {
            readonly get => new(Z, X, W);
            set
            {
                Z = value.X;
                X = value.Y;
                W = value.Z;
            }
        }
        public Vector3 ZYX
        {
            readonly get => new(Z, Y, X);
            set
            {
                Z = value.X;
                Y = value.Y;
                X = value.Z;
            }
        }
        public readonly Vector3 ZYY => new(Z, Y, Y);
        public readonly Vector3 ZYZ => new(Z, Y, Z);
        public Vector3 ZYW
        {
            readonly get => new(Z, Y, W);
            set
            {
                Z = value.X;
                Y = value.Y;
                W = value.Z;
            }
        }
        public readonly Vector3 ZZX => new(Z, Z, X);
        public readonly Vector3 ZZY => new(Z, Z, Y);
        public readonly Vector3 ZZZ => new(Z, Z, Z);
        public readonly Vector3 ZZW => new(Z, Z, W);
        public Vector3 ZWX
        {
            readonly get => new(Z, W, X);
            set
            {
                Z = value.X;
                W = value.Y;
                X = value.Z;
            }
        }
        public Vector3 ZWY
        {
            readonly get => new(Z, W, Y);
            set
            {
                Z = value.X;
                W = value.Y;
                Y = value.Z;
            }
        }
        public readonly Vector3 ZWZ => new(Z, W, Z);
        public readonly Vector3 ZWW => new(Z, W, W);
        public readonly Vector3 WXX => new(W, X, X);
        public Vector3 WXY
        {
            readonly get => new(W, X, Y);
            set
            {
                W = value.X;
                X = value.Y;
                Y = value.Z;
            }
        }
        public Vector3 WXZ
        {
            readonly get => new(W, X, Z);
            set
            {
                W = value.X;
                X = value.Y;
                Z = value.Z;
            }
        }
        public readonly Vector3 WXW => new(W, X, W);
        public Vector3 WYX
        {
            readonly get => new(W, Y, X);
            set
            {
                W = value.X;
                Y = value.Y;
                X = value.Z;
            }
        }
        public readonly Vector3 WYY => new(W, Y, Y);
        public Vector3 WYZ
        {
            readonly get => new(W, Y, Z);
            set
            {
                W = value.X;
                Y = value.Y;
                Z = value.Z;
            }
        }
        public readonly Vector3 WYW => new(W, Y, W);
        public Vector3 WZX
        {
            readonly get => new(W, Z, X);
            set
            {
                W = value.X;
                Z = value.Y;
                X = value.Z;
            }
        }
        public Vector3 WZY
        {
            readonly get => new(W, Z, Y);
            set
            {
                W = value.X;
                Z = value.Y;
                Y = value.Z;
            }
        }
        public readonly Vector3 WZZ => new(W, Z, Z);
        public readonly Vector3 WZW => new(W, Z, W);
        public readonly Vector3 WWX => new(W, W, X);
        public readonly Vector3 WWY => new(W, W, Y);
        public readonly Vector3 WWZ => new(W, W, Z);
        public readonly Vector3 WWW => new(W, W, W);

        public readonly Vector4 XXXX => new(X, X, X, X);
        public readonly Vector4 XXXY => new(X, X, X, Y);
        public readonly Vector4 XXXZ => new(X, X, X, Z);
        public readonly Vector4 XXXW => new(X, X, X, W);
        public readonly Vector4 XXYX => new(X, X, Y, X);
        public readonly Vector4 XXYY => new(X, X, Y, Y);
        public readonly Vector4 XXYZ => new(X, X, Y, Z);
        public readonly Vector4 XXYW => new(X, X, Y, W);
        public readonly Vector4 XXZX => new(X, X, Z, X);
        public readonly Vector4 XXZY => new(X, X, Z, Y);
        public readonly Vector4 XXZZ => new(X, X, Z, Z);
        public readonly Vector4 XXZW => new(X, X, Z, W);
        public readonly Vector4 XXWX => new(X, X, W, X);
        public readonly Vector4 XXWY => new(X, X, W, Y);
        public readonly Vector4 XXWZ => new(X, X, W, Z);
        public readonly Vector4 XXWW => new(X, X, W, W);
        public readonly Vector4 XYXX => new(X, Y, X, X);
        public readonly Vector4 XYXY => new(X, Y, X, Y);
        public readonly Vector4 XYXZ => new(X, Y, X, Z);
        public readonly Vector4 XYXW => new(X, Y, X, W);
        public readonly Vector4 XYYX => new(X, Y, Y, X);
        public readonly Vector4 XYYY => new(X, Y, Y, Y);
        public readonly Vector4 XYYZ => new(X, Y, Y, Z);
        public readonly Vector4 XYYW => new(X, Y, Y, W);
        public readonly Vector4 XYZX => new(X, Y, Z, X);
        public readonly Vector4 XYZY => new(X, Y, Z, Y);
        public readonly Vector4 XYZZ => new(X, Y, Z, Z);
        public Vector4 XYZW
        {
            readonly get => new(X, Y, Z, W);
            set
            {
                X = value.X;
                Y = value.Y;
                Z = value.Z;
                W = value.W;
            }
        }
        public readonly Vector4 XYWX => new(X, Y, W, X);
        public readonly Vector4 XYWY => new(X, Y, W, Y);
        public Vector4 XYWZ
        {
            readonly get => new(X, Y, W, Z);
            set
            {
                X = value.X;
                Y = value.Y;
                W = value.Z;
                Z = value.W;
            }
        }
        public readonly Vector4 XYWW => new(X, Y, W, W);
        public readonly Vector4 XZXX => new(X, Z, X, X);
        public readonly Vector4 XZXY => new(X, Z, X, Y);
        public readonly Vector4 XZXZ => new(X, Z, X, Z);
        public readonly Vector4 XZXW => new(X, Z, X, W);
        public readonly Vector4 XZYX => new(X, Z, Y, X);
        public readonly Vector4 XZYY => new(X, Z, Y, Y);
        public readonly Vector4 XZYZ => new(X, Z, Y, Z);
        public Vector4 XZYW
        {
            readonly get => new(X, Z, Y, W);
            set
            {
                X = value.X;
                Z = value.Y;
                Y = value.Z;
                W = value.W;
            }
        }
        public readonly Vector4 XZZX => new(X, Z, Z, X);
        public readonly Vector4 XZZY => new(X, Z, Z, Y);
        public readonly Vector4 XZZZ => new(X, Z, Z, Z);
        public readonly Vector4 XZZW => new(X, Z, Z, W);
        public readonly Vector4 XZWX => new(X, Z, W, X);
        public Vector4 XZWY
        {
            readonly get => new(X, Z, W, Y);
            set
            {
                X = value.X;
                Z = value.Y;
                W = value.Z;
                Y = value.W;
            }
        }
        public readonly Vector4 XZWZ => new(X, Z, W, Z);
        public readonly Vector4 XZWW => new(X, Z, W, W);
        public readonly Vector4 XWXX => new(X, W, X, X);
        public readonly Vector4 XWXY => new(X, W, X, Y);
        public readonly Vector4 XWXZ => new(X, W, X, Z);
        public readonly Vector4 XWXW => new(X, W, X, W);
        public readonly Vector4 XWYX => new(X, W, Y, X);
        public readonly Vector4 XWYY => new(X, W, Y, Y);
        public Vector4 XWYZ
        {
            readonly get => new(X, W, Y, Z);
            set
            {
                X = value.X;
                W = value.Y;
                Y = value.Z;
                Z = value.W;
            }
        }
        public readonly Vector4 XWYW => new(X, W, Y, W);
        public readonly Vector4 XWZX => new(X, W, Z, X);
        public Vector4 XWZY
        {
            readonly get => new(X, W, Z, Y);
            set
            {
                X = value.X;
                W = value.Y;
                Z = value.Z;
                Y = value.W;
            }
        }
        public readonly Vector4 XWZZ => new(X, W, Z, Z);
        public readonly Vector4 XWZW => new(X, W, Z, W);
        public readonly Vector4 XWWX => new(X, W, W, X);
        public readonly Vector4 XWWY => new(X, W, W, Y);
        public readonly Vector4 XWWZ => new(X, W, W, Z);
        public readonly Vector4 XWWW => new(X, W, W, W);
        public readonly Vector4 YXXX => new(Y, X, X, X);
        public readonly Vector4 YXXY => new(Y, X, X, Y);
        public readonly Vector4 YXXZ => new(Y, X, X, Z);
        public readonly Vector4 YXXW => new(Y, X, X, W);
        public readonly Vector4 YXYX => new(Y, X, Y, X);
        public readonly Vector4 YXYY => new(Y, X, Y, Y);
        public readonly Vector4 YXYZ => new(Y, X, Y, Z);
        public readonly Vector4 YXYW => new(Y, X, Y, W);
        public readonly Vector4 YXZX => new(Y, X, Z, X);
        public readonly Vector4 YXZY => new(Y, X, Z, Y);
        public readonly Vector4 YXZZ => new(Y, X, Z, Z);
        public Vector4 YXZW
        {
            readonly get => new(Y, X, Z, W);
            set
            {
                Y = value.X;
                X = value.Y;
                Z = value.Z;
                W = value.W;
            }
        }
        public readonly Vector4 YXWX => new(Y, X, W, X);
        public readonly Vector4 YXWY => new(Y, X, W, Y);
        public Vector4 YXWZ
        {
            readonly get => new(Y, X, W, Z);
            set
            {
                Y = value.X;
                X = value.Y;
                W = value.Z;
                Z = value.W;
            }
        }
        public readonly Vector4 YXWW => new(Y, X, W, W);
        public readonly Vector4 YYXX => new(Y, Y, X, X);
        public readonly Vector4 YYXY => new(Y, Y, X, Y);
        public readonly Vector4 YYXZ => new(Y, Y, X, Z);
        public readonly Vector4 YYXW => new(Y, Y, X, W);
        public readonly Vector4 YYYX => new(Y, Y, Y, X);
        public readonly Vector4 YYYY => new(Y, Y, Y, Y);
        public readonly Vector4 YYYZ => new(Y, Y, Y, Z);
        public readonly Vector4 YYYW => new(Y, Y, Y, W);
        public readonly Vector4 YYZX => new(Y, Y, Z, X);
        public readonly Vector4 YYZY => new(Y, Y, Z, Y);
        public readonly Vector4 YYZZ => new(Y, Y, Z, Z);
        public readonly Vector4 YYZW => new(Y, Y, Z, W);
        public readonly Vector4 YYWX => new(Y, Y, W, X);
        public readonly Vector4 YYWY => new(Y, Y, W, Y);
        public readonly Vector4 YYWZ => new(Y, Y, W, Z);
        public readonly Vector4 YYWW => new(Y, Y, W, W);
        public readonly Vector4 YZXX => new(Y, Z, X, X);
        public readonly Vector4 YZXY => new(Y, Z, X, Y);
        public readonly Vector4 YZXZ => new(Y, Z, X, Z);
        public Vector4 YZXW
        {
            readonly get => new(Y, Z, X, W);
            set
            {
                Y = value.X;
                Z = value.Y;
                X = value.Z;
                W = value.W;
            }
        }
        public readonly Vector4 YZYX => new(Y, Z, Y, X);
        public readonly Vector4 YZYY => new(Y, Z, Y, Y);
        public readonly Vector4 YZYZ => new(Y, Z, Y, Z);
        public readonly Vector4 YZYW => new(Y, Z, Y, W);
        public readonly Vector4 YZZX => new(Y, Z, Z, X);
        public readonly Vector4 YZZY => new(Y, Z, Z, Y);
        public readonly Vector4 YZZZ => new(Y, Z, Z, Z);
        public readonly Vector4 YZZW => new(Y, Z, Z, W);
        public Vector4 YZWX
        {
            readonly get => new(Y, Z, W, X);
            set
            {
                Y = value.X;
                Z = value.Y;
                W = value.Z;
                X = value.W;
            }
        }
        public readonly Vector4 YZWY => new(Y, Z, W, Y);
        public readonly Vector4 YZWZ => new(Y, Z, W, Z);
        public readonly Vector4 YZWW => new(Y, Z, W, W);
        public readonly Vector4 YWXX => new(Y, W, X, X);
        public readonly Vector4 YWXY => new(Y, W, X, Y);
        public Vector4 YWXZ
        {
            readonly get => new(Y, W, X, Z);
            set
            {
                Y = value.X;
                W = value.Y;
                X = value.Z;
                Z = value.W;
            }
        }
        public readonly Vector4 YWXW => new(Y, W, X, W);
        public readonly Vector4 YWYX => new(Y, W, Y, X);
        public readonly Vector4 YWYY => new(Y, W, Y, Y);
        public readonly Vector4 YWYZ => new(Y, W, Y, Z);
        public readonly Vector4 YWYW => new(Y, W, Y, W);
        public Vector4 YWZX
        {
            readonly get => new(Y, W, Z, X);
            set
            {
                Y = value.X;
                W = value.Y;
                Z = value.Z;
                X = value.W;
            }
        }
        public readonly Vector4 YWZY => new(Y, W, Z, Y);
        public readonly Vector4 YWZZ => new(Y, W, Z, Z);
        public readonly Vector4 YWZW => new(Y, W, Z, W);
        public readonly Vector4 YWWX => new(Y, W, W, X);
        public readonly Vector4 YWWY => new(Y, W, W, Y);
        public readonly Vector4 YWWZ => new(Y, W, W, Z);
        public readonly Vector4 YWWW => new(Y, W, W, W);
        public readonly Vector4 ZXXX => new(Z, X, X, X);
        public readonly Vector4 ZXXY => new(Z, X, X, Y);
        public readonly Vector4 ZXXZ => new(Z, X, X, Z);
        public readonly Vector4 ZXXW => new(Z, X, X, W);
        public readonly Vector4 ZXYX => new(Z, X, Y, X);
        public readonly Vector4 ZXYY => new(Z, X, Y, Y);
        public readonly Vector4 ZXYZ => new(Z, X, Y, Z);
        public Vector4 ZXYW
        {
            readonly get => new(Z, X, Y, W);
            set
            {
                Z = value.X;
                X = value.Y;
                Y = value.Z;
                W = value.W;
            }
        }
        public readonly Vector4 ZXZX => new(Z, X, Z, X);
        public readonly Vector4 ZXZY => new(Z, X, Z, Y);
        public readonly Vector4 ZXZZ => new(Z, X, Z, Z);
        public readonly Vector4 ZXZW => new(Z, X, Z, W);
        public readonly Vector4 ZXWX => new(Z, X, W, X);
        public Vector4 ZXWY
        {
            readonly get => new(Z, X, W, Y);
            set
            {
                Z = value.X;
                X = value.Y;
                W = value.Z;
                Y = value.W;
            }
        }
        public readonly Vector4 ZXWZ => new(Z, X, W, Z);
        public readonly Vector4 ZXWW => new(Z, X, W, W);
        public readonly Vector4 ZYXX => new(Z, Y, X, X);
        public readonly Vector4 ZYXY => new(Z, Y, X, Y);
        public readonly Vector4 ZYXZ => new(Z, Y, X, Z);
        public Vector4 ZYXW
        {
            readonly get => new(Z, Y, X, W);
            set
            {
                Z = value.X;
                Y = value.Y;
                X = value.Z;
                W = value.W;
            }
        }
        public readonly Vector4 ZYYX => new(Z, Y, Y, X);
        public readonly Vector4 ZYYY => new(Z, Y, Y, Y);
        public readonly Vector4 ZYYZ => new(Z, Y, Y, Z);
        public readonly Vector4 ZYYW => new(Z, Y, Y, W);
        public readonly Vector4 ZYZX => new(Z, Y, Z, X);
        public readonly Vector4 ZYZY => new(Z, Y, Z, Y);
        public readonly Vector4 ZYZZ => new(Z, Y, Z, Z);
        public readonly Vector4 ZYZW => new(Z, Y, Z, W);
        public Vector4 ZYWX
        {
            readonly get => new(Z, Y, W, X);
            set
            {
                Z = value.X;
                Y = value.Y;
                W = value.Z;
                X = value.W;
            }
        }
        public readonly Vector4 ZYWY => new(Z, Y, W, Y);
        public readonly Vector4 ZYWZ => new(Z, Y, W, Z);
        public readonly Vector4 ZYWW => new(Z, Y, W, W);
        public readonly Vector4 ZZXX => new(Z, Z, X, X);
        public readonly Vector4 ZZXY => new(Z, Z, X, Y);
        public readonly Vector4 ZZXZ => new(Z, Z, X, Z);
        public readonly Vector4 ZZXW => new(Z, Z, X, W);
        public readonly Vector4 ZZYX => new(Z, Z, Y, X);
        public readonly Vector4 ZZYY => new(Z, Z, Y, Y);
        public readonly Vector4 ZZYZ => new(Z, Z, Y, Z);
        public readonly Vector4 ZZYW => new(Z, Z, Y, W);
        public readonly Vector4 ZZZX => new(Z, Z, Z, X);
        public readonly Vector4 ZZZY => new(Z, Z, Z, Y);
        public readonly Vector4 ZZZZ => new(Z, Z, Z, Z);
        public readonly Vector4 ZZZW => new(Z, Z, Z, W);
        public readonly Vector4 ZZWX => new(Z, Z, W, X);
        public readonly Vector4 ZZWY => new(Z, Z, W, Y);
        public readonly Vector4 ZZWZ => new(Z, Z, W, Z);
        public readonly Vector4 ZZWW => new(Z, Z, W, W);
        public readonly Vector4 ZWXX => new(Z, W, X, X);
        public Vector4 ZWXY
        {
            readonly get => new(Z, W, X, Y);
            set
            {
                Z = value.X;
                W = value.Y;
                X = value.Z;
                Y = value.W;
            }
        }
        public readonly Vector4 ZWXZ => new(Z, W, X, Z);
        public readonly Vector4 ZWXW => new(Z, W, X, W);
        public Vector4 ZWYX
        {
            readonly get => new(Z, W, Y, X);
            set
            {
                Z = value.X;
                W = value.Y;
                Y = value.Z;
                X = value.W;
            }
        }
        public readonly Vector4 ZWYY => new(Z, W, Y, Y);
        public readonly Vector4 ZWYZ => new(Z, W, Y, Z);
        public readonly Vector4 ZWYW => new(Z, W, Y, W);
        public readonly Vector4 ZWZX => new(Z, W, Z, X);
        public readonly Vector4 ZWZY => new(Z, W, Z, Y);
        public readonly Vector4 ZWZZ => new(Z, W, Z, Z);
        public readonly Vector4 ZWZW => new(Z, W, Z, W);
        public readonly Vector4 ZWWX => new(Z, W, W, X);
        public readonly Vector4 ZWWY => new(Z, W, W, Y);
        public readonly Vector4 ZWWZ => new(Z, W, W, Z);
        public readonly Vector4 ZWWW => new(Z, W, W, W);
        public readonly Vector4 WXXX => new(W, X, X, X);
        public readonly Vector4 WXXY => new(W, X, X, Y);
        public readonly Vector4 WXXZ => new(W, X, X, Z);
        public readonly Vector4 WXXW => new(W, X, X, W);
        public readonly Vector4 WXYX => new(W, X, Y, X);
        public readonly Vector4 WXYY => new(W, X, Y, Y);
        public Vector4 WXYZ
        {
            readonly get => new(W, X, Y, Z);
            set
            {
                W = value.X;
                X = value.Y;
                Y = value.Z;
                Z = value.W;
            }
        }
        public readonly Vector4 WXYW => new(W, X, Y, W);
        public readonly Vector4 WXZX => new(W, X, Z, X);
        public Vector4 WXZY
        {
            readonly get => new(W, X, Z, Y);
            set
            {
                W = value.X;
                X = value.Y;
                Z = value.Z;
                Y = value.W;
            }
        }
        public readonly Vector4 WXZZ => new(W, X, Z, Z);
        public readonly Vector4 WXZW => new(W, X, Z, W);
        public readonly Vector4 WXWX => new(W, X, W, X);
        public readonly Vector4 WXWY => new(W, X, W, Y);
        public readonly Vector4 WXWZ => new(W, X, W, Z);
        public readonly Vector4 WXWW => new(W, X, W, W);
        public readonly Vector4 WYXX => new(W, Y, X, X);
        public readonly Vector4 WYXY => new(W, Y, X, Y);
        public Vector4 WYXZ
        {
            readonly get => new(W, Y, X, Z);
            set
            {
                W = value.X;
                Y = value.Y;
                X = value.Z;
                Z = value.W;
            }
        }
        public readonly Vector4 WYXW => new(W, Y, X, W);
        public readonly Vector4 WYYX => new(W, Y, Y, X);
        public readonly Vector4 WYYY => new(W, Y, Y, Y);
        public readonly Vector4 WYYZ => new(W, Y, Y, Z);
        public readonly Vector4 WYYW => new(W, Y, Y, W);
        public Vector4 WYZX
        {
            readonly get => new(W, Y, Z, X);
            set
            {
                W = value.X;
                Y = value.Y;
                Z = value.Z;
                X = value.W;
            }
        }
        public readonly Vector4 WYZY => new(W, Y, Z, Y);
        public readonly Vector4 WYZZ => new(W, Y, Z, Z);
        public readonly Vector4 WYZW => new(W, Y, Z, W);
        public readonly Vector4 WYWX => new(W, Y, W, X);
        public readonly Vector4 WYWY => new(W, Y, W, Y);
        public readonly Vector4 WYWZ => new(W, Y, W, Z);
        public readonly Vector4 WYWW => new(W, Y, W, W);
        public readonly Vector4 WZXX => new(W, Z, X, X);
        public Vector4 WZXY
        {
            readonly get => new(W, Z, X, Y);
            set
            {
                W = value.X;
                Z = value.Y;
                X = value.Z;
                Y = value.W;
            }
        }
        public readonly Vector4 WZXZ => new(W, Z, X, Z);
        public readonly Vector4 WZXW => new(W, Z, X, W);
        public Vector4 WZYX
        {
            readonly get => new(W, Z, Y, X);
            set
            {
                W = value.X;
                Z = value.Y;
                Y = value.Z;
                X = value.W;
            }
        }
        public readonly Vector4 WZYY => new(W, Z, Y, Y);
        public readonly Vector4 WZYZ => new(W, Z, Y, Z);
        public readonly Vector4 WZYW => new(W, Z, Y, W);
        public readonly Vector4 WZZX => new(W, Z, Z, X);
        public readonly Vector4 WZZY => new(W, Z, Z, Y);
        public readonly Vector4 WZZZ => new(W, Z, Z, Z);
        public readonly Vector4 WZZW => new(W, Z, Z, W);
        public readonly Vector4 WZWX => new(W, Z, W, X);
        public readonly Vector4 WZWY => new(W, Z, W, Y);
        public readonly Vector4 WZWZ => new(W, Z, W, Z);
        public readonly Vector4 WZWW => new(W, Z, W, W);
        public readonly Vector4 WWXX => new(W, W, X, X);
        public readonly Vector4 WWXY => new(W, W, X, Y);
        public readonly Vector4 WWXZ => new(W, W, X, Z);
        public readonly Vector4 WWXW => new(W, W, X, W);
        public readonly Vector4 WWYX => new(W, W, Y, X);
        public readonly Vector4 WWYY => new(W, W, Y, Y);
        public readonly Vector4 WWYZ => new(W, W, Y, Z);
        public readonly Vector4 WWYW => new(W, W, Y, W);
        public readonly Vector4 WWZX => new(W, W, Z, X);
        public readonly Vector4 WWZY => new(W, W, Z, Y);
        public readonly Vector4 WWZZ => new(W, W, Z, Z);
        public readonly Vector4 WWZW => new(W, W, Z, W);
        public readonly Vector4 WWWX => new(W, W, W, X);
        public readonly Vector4 WWWY => new(W, W, W, Y);
        public readonly Vector4 WWWZ => new(W, W, W, Z);
        public readonly Vector4 WWWW => new(W, W, W, W);
#pragma warning restore CS1591


        /// <summary>
        /// Constructs a new <see cref="Vector4"/> with the given components.
        /// </summary>
        /// <param name="x">The vector's X component.</param>
        /// <param name="y">The vector's Y component.</param>
        /// <param name="z">The vector's Z component.</param>
        /// <param name="w">The vector's W component.</param>
        public Vector4(real_t x, real_t y, real_t z, real_t w)
        {
            X = x;
            Y = y;
            Z = z;
            W = w;
        }

        /// <summary>
        /// Adds each component of the <see cref="Vector4"/>
        /// with the components of the given <see cref="Vector4"/>.
        /// </summary>
        /// <param name="left">The left vector.</param>
        /// <param name="right">The right vector.</param>
        /// <returns>The added vector.</returns>
        public static Vector4 operator +(Vector4 left, Vector4 right)
        {
            left.X += right.X;
            left.Y += right.Y;
            left.Z += right.Z;
            left.W += right.W;
            return left;
        }

        /// <summary>
        /// Subtracts each component of the <see cref="Vector4"/>
        /// by the components of the given <see cref="Vector4"/>.
        /// </summary>
        /// <param name="left">The left vector.</param>
        /// <param name="right">The right vector.</param>
        /// <returns>The subtracted vector.</returns>
        public static Vector4 operator -(Vector4 left, Vector4 right)
        {
            left.X -= right.X;
            left.Y -= right.Y;
            left.Z -= right.Z;
            left.W -= right.W;
            return left;
        }

        /// <summary>
        /// Returns the same value as if the <c>+</c> was not there.
        /// Unary <c>+</c> does nothing, but sometimes it can make your
        /// code more readable.
        /// </summary>
        /// <param name="vec">The vector to do nothing to.</param>
        /// <returns>The original vector.</returns>
        public static Vector4 operator +(Vector4 vec) => vec;

        /// <summary>
        /// Returns the negative value of the <see cref="Vector4"/>.
        /// This is the same as writing <c>new Vector4(-v.X, -v.Y, -v.Z, -v.W)</c>.
        /// This operation flips the direction of the vector while
        /// keeping the same magnitude.
        /// With floats, the number zero can be either positive or negative.
        /// </summary>
        /// <param name="vec">The vector to negate/flip.</param>
        /// <returns>The negated/flipped vector.</returns>
        public static Vector4 operator -(Vector4 vec)
        {
            vec.X = -vec.X;
            vec.Y = -vec.Y;
            vec.Z = -vec.Z;
            vec.W = -vec.W;
            return vec;
        }

        /// <summary>
        /// Multiplies each component of the <see cref="Vector4"/>
        /// by the given <see cref="real_t"/>.
        /// </summary>
        /// <param name="vec">The vector to multiply.</param>
        /// <param name="scale">The scale to multiply by.</param>
        /// <returns>The multiplied vector.</returns>
        public static Vector4 operator *(Vector4 vec, real_t scale)
        {
            vec.X *= scale;
            vec.Y *= scale;
            vec.Z *= scale;
            vec.W *= scale;
            return vec;
        }

        /// <summary>
        /// Multiplies each component of the <see cref="Vector4"/>
        /// by the given <see cref="real_t"/>.
        /// </summary>
        /// <param name="scale">The scale to multiply by.</param>
        /// <param name="vec">The vector to multiply.</param>
        /// <returns>The multiplied vector.</returns>
        public static Vector4 operator *(real_t scale, Vector4 vec)
        {
            vec.X *= scale;
            vec.Y *= scale;
            vec.Z *= scale;
            vec.W *= scale;
            return vec;
        }

        /// <summary>
        /// Multiplies each component of the <see cref="Vector4"/>
        /// by the components of the given <see cref="Vector4"/>.
        /// </summary>
        /// <param name="left">The left vector.</param>
        /// <param name="right">The right vector.</param>
        /// <returns>The multiplied vector.</returns>
        public static Vector4 operator *(Vector4 left, Vector4 right)
        {
            left.X *= right.X;
            left.Y *= right.Y;
            left.Z *= right.Z;
            left.W *= right.W;
            return left;
        }

        /// <summary>
        /// Divides each component of the <see cref="Vector4"/>
        /// by the given <see cref="real_t"/>.
        /// </summary>
        /// <param name="vec">The dividend vector.</param>
        /// <param name="divisor">The divisor value.</param>
        /// <returns>The divided vector.</returns>
        public static Vector4 operator /(Vector4 vec, real_t divisor)
        {
            vec.X /= divisor;
            vec.Y /= divisor;
            vec.Z /= divisor;
            vec.W /= divisor;
            return vec;
        }

        /// <summary>
        /// Divides each component of the <see cref="Vector4"/>
        /// by the components of the given <see cref="Vector4"/>.
        /// </summary>
        /// <param name="vec">The dividend vector.</param>
        /// <param name="divisorv">The divisor vector.</param>
        /// <returns>The divided vector.</returns>
        public static Vector4 operator /(Vector4 vec, Vector4 divisorv)
        {
            vec.X /= divisorv.X;
            vec.Y /= divisorv.Y;
            vec.Z /= divisorv.Z;
            vec.W /= divisorv.W;
            return vec;
        }

        /// <summary>
        /// Gets the remainder of each component of the <see cref="Vector4"/>
        /// with the components of the given <see cref="real_t"/>.
        /// This operation uses truncated division, which is often not desired
        /// as it does not work well with negative numbers.
        /// Consider using <see cref="PosMod(real_t)"/> instead
        /// if you want to handle negative numbers.
        /// </summary>
        /// <example>
        /// <code>
        /// GD.Print(new Vector4(10, -20, 30, 40) % 7); // Prints "(3, -6, 2, 5)"
        /// </code>
        /// </example>
        /// <param name="vec">The dividend vector.</param>
        /// <param name="divisor">The divisor value.</param>
        /// <returns>The remainder vector.</returns>
        public static Vector4 operator %(Vector4 vec, real_t divisor)
        {
            vec.X %= divisor;
            vec.Y %= divisor;
            vec.Z %= divisor;
            vec.W %= divisor;
            return vec;
        }

        /// <summary>
        /// Gets the remainder of each component of the <see cref="Vector4"/>
        /// with the components of the given <see cref="Vector4"/>.
        /// This operation uses truncated division, which is often not desired
        /// as it does not work well with negative numbers.
        /// Consider using <see cref="PosMod(Vector4)"/> instead
        /// if you want to handle negative numbers.
        /// </summary>
        /// <example>
        /// <code>
        /// GD.Print(new Vector4(10, -20, 30, 10) % new Vector4(7, 8, 9, 10)); // Prints "(3, -4, 3, 0)"
        /// </code>
        /// </example>
        /// <param name="vec">The dividend vector.</param>
        /// <param name="divisorv">The divisor vector.</param>
        /// <returns>The remainder vector.</returns>
        public static Vector4 operator %(Vector4 vec, Vector4 divisorv)
        {
            vec.X %= divisorv.X;
            vec.Y %= divisorv.Y;
            vec.Z %= divisorv.Z;
            vec.W %= divisorv.W;
            return vec;
        }

        /// <summary>
        /// Returns <see langword="true"/> if the vectors are exactly equal.
        /// Note: Due to floating-point precision errors, consider using
        /// <see cref="IsEqualApprox"/> instead, which is more reliable.
        /// </summary>
        /// <param name="left">The left vector.</param>
        /// <param name="right">The right vector.</param>
        /// <returns>Whether or not the vectors are exactly equal.</returns>
        public static bool operator ==(Vector4 left, Vector4 right)
        {
            return left.Equals(right);
        }

        /// <summary>
        /// Returns <see langword="true"/> if the vectors are not equal.
        /// Note: Due to floating-point precision errors, consider using
        /// <see cref="IsEqualApprox"/> instead, which is more reliable.
        /// </summary>
        /// <param name="left">The left vector.</param>
        /// <param name="right">The right vector.</param>
        /// <returns>Whether or not the vectors are not equal.</returns>
        public static bool operator !=(Vector4 left, Vector4 right)
        {
            return !left.Equals(right);
        }

        /// <summary>
        /// Compares two <see cref="Vector4"/> vectors by first checking if
        /// the X value of the <paramref name="left"/> vector is less than
        /// the X value of the <paramref name="right"/> vector.
        /// If the X values are exactly equal, then it repeats this check
        /// with the Y, Z and finally W values of the two vectors.
        /// This operator is useful for sorting vectors.
        /// </summary>
        /// <param name="left">The left vector.</param>
        /// <param name="right">The right vector.</param>
        /// <returns>Whether or not the left is less than the right.</returns>
        public static bool operator <(Vector4 left, Vector4 right)
        {
            if (left.X == right.X)
            {
                if (left.Y == right.Y)
                {
                    if (left.Z == right.Z)
                    {
                        return left.W < right.W;
                    }
                    return left.Z < right.Z;
                }
                return left.Y < right.Y;
            }
            return left.X < right.X;
        }

        /// <summary>
        /// Compares two <see cref="Vector4"/> vectors by first checking if
        /// the X value of the <paramref name="left"/> vector is greater than
        /// the X value of the <paramref name="right"/> vector.
        /// If the X values are exactly equal, then it repeats this check
        /// with the Y, Z and finally W values of the two vectors.
        /// This operator is useful for sorting vectors.
        /// </summary>
        /// <param name="left">The left vector.</param>
        /// <param name="right">The right vector.</param>
        /// <returns>Whether or not the left is greater than the right.</returns>
        public static bool operator >(Vector4 left, Vector4 right)
        {
            if (left.X == right.X)
            {
                if (left.Y == right.Y)
                {
                    if (left.Z == right.Z)
                    {
                        return left.W > right.W;
                    }
                    return left.Z > right.Z;
                }
                return left.Y > right.Y;
            }
            return left.X > right.X;
        }

        /// <summary>
        /// Compares two <see cref="Vector4"/> vectors by first checking if
        /// the X value of the <paramref name="left"/> vector is less than
        /// or equal to the X value of the <paramref name="right"/> vector.
        /// If the X values are exactly equal, then it repeats this check
        /// with the Y, Z and finally W values of the two vectors.
        /// This operator is useful for sorting vectors.
        /// </summary>
        /// <param name="left">The left vector.</param>
        /// <param name="right">The right vector.</param>
        /// <returns>Whether or not the left is less than or equal to the right.</returns>
        public static bool operator <=(Vector4 left, Vector4 right)
        {
            if (left.X == right.X)
            {
                if (left.Y == right.Y)
                {
                    if (left.Z == right.Z)
                    {
                        return left.W <= right.W;
                    }
                    return left.Z < right.Z;
                }
                return left.Y < right.Y;
            }
            return left.X < right.X;
        }

        /// <summary>
        /// Compares two <see cref="Vector4"/> vectors by first checking if
        /// the X value of the <paramref name="left"/> vector is greater than
        /// or equal to the X value of the <paramref name="right"/> vector.
        /// If the X values are exactly equal, then it repeats this check
        /// with the Y, Z and finally W values of the two vectors.
        /// This operator is useful for sorting vectors.
        /// </summary>
        /// <param name="left">The left vector.</param>
        /// <param name="right">The right vector.</param>
        /// <returns>Whether or not the left is greater than or equal to the right.</returns>
        public static bool operator >=(Vector4 left, Vector4 right)
        {
            if (left.X == right.X)
            {
                if (left.Y == right.Y)
                {
                    if (left.Z == right.Z)
                    {
                        return left.W >= right.W;
                    }
                    return left.Z > right.Z;
                }
                return left.Y > right.Y;
            }
            return left.X > right.X;
        }

        /// <summary>
        /// Returns <see langword="true"/> if the vector is exactly equal
        /// to the given object (<paramref name="obj"/>).
        /// Note: Due to floating-point precision errors, consider using
        /// <see cref="IsEqualApprox"/> instead, which is more reliable.
        /// </summary>
        /// <param name="obj">The object to compare with.</param>
        /// <returns>Whether or not the vector and the object are equal.</returns>
        public override readonly bool Equals([NotNullWhen(true)] object? obj)
        {
            return obj is Vector4 other && Equals(other);
        }

        /// <summary>
        /// Returns <see langword="true"/> if the vectors are exactly equal.
        /// Note: Due to floating-point precision errors, consider using
        /// <see cref="IsEqualApprox"/> instead, which is more reliable.
        /// </summary>
        /// <param name="other">The other vector.</param>
        /// <returns>Whether or not the vectors are exactly equal.</returns>
        public readonly bool Equals(Vector4 other)
        {
            return X == other.X && Y == other.Y && Z == other.Z && W == other.W;
        }

        /// <summary>
        /// Returns <see langword="true"/> if this vector and <paramref name="other"/> are approximately equal,
        /// by running <see cref="Mathf.IsEqualApprox(real_t, real_t)"/> on each component.
        /// </summary>
        /// <param name="other">The other vector to compare.</param>
        /// <returns>Whether or not the vectors are approximately equal.</returns>
        public readonly bool IsEqualApprox(Vector4 other)
        {
            return Mathf.IsEqualApprox(X, other.X) && Mathf.IsEqualApprox(Y, other.Y) && Mathf.IsEqualApprox(Z, other.Z) && Mathf.IsEqualApprox(W, other.W);
        }

        /// <summary>
        /// Returns <see langword="true"/> if this vector's values are approximately zero,
        /// by running <see cref="Mathf.IsZeroApprox(real_t)"/> on each component.
        /// This method is faster than using <see cref="IsEqualApprox"/> with one value
        /// as a zero vector.
        /// </summary>
        /// <returns>Whether or not the vector is approximately zero.</returns>
        public readonly bool IsZeroApprox()
        {
            return Mathf.IsZeroApprox(X) && Mathf.IsZeroApprox(Y) && Mathf.IsZeroApprox(Z) && Mathf.IsZeroApprox(W);
        }

        /// <summary>
        /// Serves as the hash function for <see cref="Vector4"/>.
        /// </summary>
        /// <returns>A hash code for this vector.</returns>
        public override readonly int GetHashCode()
        {
            return HashCode.Combine(X, Y, Z, W);
        }

        /// <summary>
        /// Converts this <see cref="Vector4"/> to a string.
        /// </summary>
        /// <returns>A string representation of this vector.</returns>
        public override readonly string ToString() => ToString(null);

        /// <summary>
        /// Converts this <see cref="Vector4"/> to a string with the given <paramref name="format"/>.
        /// </summary>
        /// <returns>A string representation of this vector.</returns>
        public readonly string ToString(string? format)
        {
            return $"({X.ToString(format, CultureInfo.InvariantCulture)}, {Y.ToString(format, CultureInfo.InvariantCulture)}, {Z.ToString(format, CultureInfo.InvariantCulture)}, {W.ToString(format, CultureInfo.InvariantCulture)})";
        }
    }
}

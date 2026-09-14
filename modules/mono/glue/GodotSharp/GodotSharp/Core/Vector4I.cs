using System;
using System.Diagnostics.CodeAnalysis;
using System.Globalization;
using System.Runtime.InteropServices;

#nullable enable

namespace Godot
{
    /// <summary>
    /// 4-element structure that can be used to represent 4D grid coordinates or sets of integers.
    /// </summary>
    [Serializable]
    [StructLayout(LayoutKind.Sequential)]
    public struct Vector4I : IEquatable<Vector4I>
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
        public int X;

        /// <summary>
        /// The vector's Y component. Also accessible by using the index position <c>[1]</c>.
        /// </summary>
        public int Y;

        /// <summary>
        /// The vector's Z component. Also accessible by using the index position <c>[2]</c>.
        /// </summary>
        public int Z;

        /// <summary>
        /// The vector's W component. Also accessible by using the index position <c>[3]</c>.
        /// </summary>
        public int W;

        /// <summary>
        /// Access vector components using their <paramref name="index"/>.
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
        public int this[int index]
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
        public readonly void Deconstruct(out int x, out int y, out int z, out int w)
        {
            x = X;
            y = Y;
            z = Z;
            w = W;
        }

        /// <summary>
        /// Returns a new vector with all components in absolute values (i.e. positive).
        /// </summary>
        /// <returns>A vector with <see cref="Mathf.Abs(int)"/> called on each component.</returns>
        public readonly Vector4I Abs()
        {
            return new Vector4I(Mathf.Abs(X), Mathf.Abs(Y), Mathf.Abs(Z), Mathf.Abs(W));
        }

        /// <summary>
        /// Returns a new vector with all components clamped between the
        /// components of <paramref name="min"/> and <paramref name="max"/> using
        /// <see cref="Mathf.Clamp(int, int, int)"/>.
        /// </summary>
        /// <param name="min">The vector with minimum allowed values.</param>
        /// <param name="max">The vector with maximum allowed values.</param>
        /// <returns>The vector with all components clamped.</returns>
        public readonly Vector4I Clamp(Vector4I min, Vector4I max)
        {
            return new Vector4I
            (
                Mathf.Clamp(X, min.X, max.X),
                Mathf.Clamp(Y, min.Y, max.Y),
                Mathf.Clamp(Z, min.Z, max.Z),
                Mathf.Clamp(W, min.W, max.W)
            );
        }

        /// <summary>
        /// Returns a new vector with all components clamped between
        /// <paramref name="min"/> and <paramref name="max"/> using
        /// <see cref="Mathf.Clamp(int, int, int)"/>.
        /// </summary>
        /// <param name="min">The minimum allowed value.</param>
        /// <param name="max">The maximum allowed value.</param>
        /// <returns>The vector with all components clamped.</returns>
        public readonly Vector4I Clamp(int min, int max)
        {
            return new Vector4I
            (
                Mathf.Clamp(X, min, max),
                Mathf.Clamp(Y, min, max),
                Mathf.Clamp(Z, min, max),
                Mathf.Clamp(W, min, max)
            );
        }

        /// <summary>
        /// Returns the squared distance between this vector and <paramref name="to"/>.
        /// This method runs faster than <see cref="DistanceTo"/>, so prefer it if
        /// you need to compare vectors or need the squared distance for some formula.
        /// </summary>
        /// <param name="to">The other vector to use.</param>
        /// <returns>The squared distance between the two vectors.</returns>
        public readonly int DistanceSquaredTo(Vector4I to)
        {
            return (to - this).LengthSquared();
        }

        /// <summary>
        /// Returns the distance between this vector and <paramref name="to"/>.
        /// </summary>
        /// <seealso cref="DistanceSquaredTo(Vector4I)"/>
        /// <param name="to">The other vector to use.</param>
        /// <returns>The distance between the two vectors.</returns>
        public readonly real_t DistanceTo(Vector4I to)
        {
            return (to - this).Length();
        }

        /// <summary>
        /// Returns the length (magnitude) of this vector.
        /// </summary>
        /// <seealso cref="LengthSquared"/>
        /// <returns>The length of this vector.</returns>
        public readonly real_t Length()
        {
            int x2 = X * X;
            int y2 = Y * Y;
            int z2 = Z * Z;
            int w2 = W * W;

            return Mathf.Sqrt(x2 + y2 + z2 + w2);
        }

        /// <summary>
        /// Returns the squared length (squared magnitude) of this vector.
        /// This method runs faster than <see cref="Length"/>, so prefer it if
        /// you need to compare vectors or need the squared length for some formula.
        /// </summary>
        /// <returns>The squared length of this vector.</returns>
        public readonly int LengthSquared()
        {
            int x2 = X * X;
            int y2 = Y * Y;
            int z2 = Z * Z;
            int w2 = W * W;

            return x2 + y2 + z2 + w2;
        }

        /// <summary>
        /// Returns the result of the component-wise maximum between
        /// this vector and <paramref name="with"/>.
        /// Equivalent to <c>new Vector4I(Mathf.Max(X, with.X), Mathf.Max(Y, with.Y), Mathf.Max(Z, with.Z), Mathf.Max(W, with.W))</c>.
        /// </summary>
        /// <param name="with">The other vector to use.</param>
        /// <returns>The resulting maximum vector.</returns>
        public readonly Vector4I Max(Vector4I with)
        {
            return new Vector4I
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
        /// Equivalent to <c>new Vector4I(Mathf.Max(X, with), Mathf.Max(Y, with), Mathf.Max(Z, with), Mathf.Max(W, with))</c>.
        /// </summary>
        /// <param name="with">The other value to use.</param>
        /// <returns>The resulting maximum vector.</returns>
        public readonly Vector4I Max(int with)
        {
            return new Vector4I
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
        /// Equivalent to <c>new Vector4I(Mathf.Min(X, with.X), Mathf.Min(Y, with.Y), Mathf.Min(Z, with.Z), Mathf.Min(W, with.W))</c>.
        /// </summary>
        /// <param name="with">The other vector to use.</param>
        /// <returns>The resulting minimum vector.</returns>
        public readonly Vector4I Min(Vector4I with)
        {
            return new Vector4I
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
        /// Equivalent to <c>new Vector4I(Mathf.Min(X, with), Mathf.Min(Y, with), Mathf.Min(Z, with), Mathf.Min(W, with))</c>.
        /// </summary>
        /// <param name="with">The other value to use.</param>
        /// <returns>The resulting minimum vector.</returns>
        public readonly Vector4I Min(int with)
        {
            return new Vector4I
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
            int max_value = X;
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
            int min_value = X;
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
        /// Returns a vector with each component set to one or negative one, depending
        /// on the signs of this vector's components, or zero if the component is zero,
        /// by calling <see cref="Mathf.Sign(int)"/> on each component.
        /// </summary>
        /// <returns>A vector with all components as either <c>1</c>, <c>-1</c>, or <c>0</c>.</returns>
        public readonly Vector4I Sign()
        {
            return new Vector4I(Mathf.Sign(X), Mathf.Sign(Y), Mathf.Sign(Z), Mathf.Sign(W));
        }

        /// <summary>
        /// Returns a new vector with each component snapped to the closest multiple of the corresponding component in <paramref name="step"/>.
        /// </summary>
        /// <param name="step">A vector value representing the step size to snap to.</param>
        /// <returns>The snapped vector.</returns>
        public readonly Vector4I Snapped(Vector4I step)
        {
            return new Vector4I(
                (int)Mathf.Snapped((double)X, (double)step.X),
                (int)Mathf.Snapped((double)Y, (double)step.Y),
                (int)Mathf.Snapped((double)Z, (double)step.Z),
                (int)Mathf.Snapped((double)W, (double)step.W)
            );
        }

        /// <summary>
        /// Returns a new vector with each component snapped to the closest multiple of <paramref name="step"/>.
        /// </summary>
        /// <param name="step">The step size to snap to.</param>
        /// <returns>The snapped vector.</returns>
        public readonly Vector4I Snapped(int step)
        {
            return new Vector4I(
                (int)Mathf.Snapped((double)X, (double)step),
                (int)Mathf.Snapped((double)Y, (double)step),
                (int)Mathf.Snapped((double)Z, (double)step),
                (int)Mathf.Snapped((double)W, (double)step)
            );
        }

        // Constants
        private static readonly Vector4I _minValue = new Vector4I(int.MinValue, int.MinValue, int.MinValue, int.MinValue);
        private static readonly Vector4I _maxValue = new Vector4I(int.MaxValue, int.MaxValue, int.MaxValue, int.MaxValue);

        private static readonly Vector4I _zero = new Vector4I(0, 0, 0, 0);
        private static readonly Vector4I _one = new Vector4I(1, 1, 1, 1);

        /// <summary>
        /// Min vector, a vector with all components equal to <see cref="int.MinValue"/>. Can be used as a negative integer equivalent of <see cref="Vector4.Inf"/>.
        /// </summary>
        /// <value>Equivalent to <c>new Vector4I(int.MinValue, int.MinValue, int.MinValue, int.MinValue)</c>.</value>
        public static Vector4I MinValue { get { return _minValue; } }
        /// <summary>
        /// Max vector, a vector with all components equal to <see cref="int.MaxValue"/>. Can be used as an integer equivalent of <see cref="Vector4.Inf"/>.
        /// </summary>
        /// <value>Equivalent to <c>new Vector4I(int.MaxValue, int.MaxValue, int.MaxValue, int.MaxValue)</c>.</value>
        public static Vector4I MaxValue { get { return _maxValue; } }

        /// <summary>
        /// Zero vector, a vector with all components set to <c>0</c>.
        /// </summary>
        /// <value>Equivalent to <c>new Vector4I(0, 0, 0, 0)</c>.</value>
        public static Vector4I Zero { get { return _zero; } }
        /// <summary>
        /// One vector, a vector with all components set to <c>1</c>.
        /// </summary>
        /// <value>Equivalent to <c>new Vector4I(1, 1, 1, 1)</c>.</value>
        public static Vector4I One { get { return _one; } }

#pragma warning disable CS1591 // Disable warning: "Missing XML comment for publicly visible type or member"
        public readonly Vector2I XX => new(X, X);
        public Vector2I XY
        {
            readonly get => new(X, Y);
            set
            {
                X = value.X;
                Y = value.Y;
            }
        }
        public Vector2I XZ
        {
            readonly get => new(X, Z);
            set
            {
                X = value.X;
                Z = value.Y;
            }
        }
        public Vector2I XW
        {
            readonly get => new(X, W);
            set
            {
                X = value.X;
                W = value.Y;
            }
        }
        public Vector2I YX
        {
            readonly get => new(Y, X);
            set
            {
                Y = value.X;
                X = value.Y;
            }
        }
        public readonly Vector2I YY => new(Y, Y);
        public Vector2I YZ
        {
            readonly get => new(Y, Z);
            set
            {
                Y = value.X;
                Z = value.Y;
            }
        }
        public Vector2I YW
        {
            readonly get => new(Y, W);
            set
            {
                Y = value.X;
                W = value.Y;
            }
        }
        public Vector2I ZX
        {
            readonly get => new(Z, X);
            set
            {
                Z = value.X;
                X = value.Y;
            }
        }
        public Vector2I ZY
        {
            readonly get => new(Z, Y);
            set
            {
                Z = value.X;
                Y = value.Y;
            }
        }
        public readonly Vector2I ZZ => new(Z, Z);
        public Vector2I ZW
        {
            readonly get => new(Z, W);
            set
            {
                Z = value.X;
                W = value.Y;
            }
        }
        public Vector2I WX
        {
            readonly get => new(W, X);
            set
            {
                W = value.X;
                X = value.Y;
            }
        }
        public Vector2I WY
        {
            readonly get => new(W, Y);
            set
            {
                W = value.X;
                Y = value.Y;
            }
        }
        public Vector2I WZ
        {
            readonly get => new(W, Z);
            set
            {
                W = value.X;
                Z = value.Y;
            }
        }
        public readonly Vector2I WW => new(W, W);

        public readonly Vector3I XXX => new(X, X, X);
        public readonly Vector3I XXY => new(X, X, Y);
        public readonly Vector3I XXZ => new(X, X, Z);
        public readonly Vector3I XXW => new(X, X, W);
        public readonly Vector3I XYX => new(X, Y, X);
        public readonly Vector3I XYY => new(X, Y, Y);
        public Vector3I XYZ
        {
            readonly get => new(X, Y, Z);
            set
            {
                X = value.X;
                Y = value.Y;
                Z = value.Z;
            }
        }
        public Vector3I XYW
        {
            readonly get => new(X, Y, W);
            set
            {
                X = value.X;
                Y = value.Y;
                W = value.Z;
            }
        }
        public readonly Vector3I XZX => new(X, Z, X);
        public Vector3I XZY
        {
            readonly get => new(X, Z, Y);
            set
            {
                X = value.X;
                Z = value.Y;
                Y = value.Z;
            }
        }
        public readonly Vector3I XZZ => new(X, Z, Z);
        public Vector3I XZW
        {
            readonly get => new(X, Z, W);
            set
            {
                X = value.X;
                Z = value.Y;
                W = value.Z;
            }
        }
        public readonly Vector3I XWX => new(X, W, X);
        public Vector3I XWY
        {
            readonly get => new(X, W, Y);
            set
            {
                X = value.X;
                W = value.Y;
                Y = value.Z;
            }
        }
        public Vector3I XWZ
        {
            readonly get => new(X, W, Z);
            set
            {
                X = value.X;
                W = value.Y;
                Z = value.Z;
            }
        }
        public readonly Vector3I XWW => new(X, W, W);
        public readonly Vector3I YXX => new(Y, X, X);
        public readonly Vector3I YXY => new(Y, X, Y);
        public Vector3I YXZ
        {
            readonly get => new(Y, X, Z);
            set
            {
                Y = value.X;
                X = value.Y;
                Z = value.Z;
            }
        }
        public Vector3I YXW
        {
            readonly get => new(Y, X, W);
            set
            {
                Y = value.X;
                X = value.Y;
                W = value.Z;
            }
        }
        public readonly Vector3I YYX => new(Y, Y, X);
        public readonly Vector3I YYY => new(Y, Y, Y);
        public readonly Vector3I YYZ => new(Y, Y, Z);
        public readonly Vector3I YYW => new(Y, Y, W);
        public Vector3I YZX
        {
            readonly get => new(Y, Z, X);
            set
            {
                Y = value.X;
                Z = value.Y;
                X = value.Z;
            }
        }
        public readonly Vector3I YZY => new(Y, Z, Y);
        public readonly Vector3I YZZ => new(Y, Z, Z);
        public Vector3I YZW
        {
            readonly get => new(Y, Z, W);
            set
            {
                Y = value.X;
                Z = value.Y;
                W = value.Z;
            }
        }
        public Vector3I YWX
        {
            readonly get => new(Y, W, X);
            set
            {
                Y = value.X;
                W = value.Y;
                X = value.Z;
            }
        }
        public readonly Vector3I YWY => new(Y, W, Y);
        public Vector3I YWZ
        {
            readonly get => new(Y, W, Z);
            set
            {
                Y = value.X;
                W = value.Y;
                Z = value.Z;
            }
        }
        public readonly Vector3I YWW => new(Y, W, W);
        public readonly Vector3I ZXX => new(Z, X, X);
        public Vector3I ZXY
        {
            readonly get => new(Z, X, Y);
            set
            {
                Z = value.X;
                X = value.Y;
                Y = value.Z;
            }
        }
        public readonly Vector3I ZXZ => new(Z, X, Z);
        public Vector3I ZXW
        {
            readonly get => new(Z, X, W);
            set
            {
                Z = value.X;
                X = value.Y;
                W = value.Z;
            }
        }
        public Vector3I ZYX
        {
            readonly get => new(Z, Y, X);
            set
            {
                Z = value.X;
                Y = value.Y;
                X = value.Z;
            }
        }
        public readonly Vector3I ZYY => new(Z, Y, Y);
        public readonly Vector3I ZYZ => new(Z, Y, Z);
        public Vector3I ZYW
        {
            readonly get => new(Z, Y, W);
            set
            {
                Z = value.X;
                Y = value.Y;
                W = value.Z;
            }
        }
        public readonly Vector3I ZZX => new(Z, Z, X);
        public readonly Vector3I ZZY => new(Z, Z, Y);
        public readonly Vector3I ZZZ => new(Z, Z, Z);
        public readonly Vector3I ZZW => new(Z, Z, W);
        public Vector3I ZWX
        {
            readonly get => new(Z, W, X);
            set
            {
                Z = value.X;
                W = value.Y;
                X = value.Z;
            }
        }
        public Vector3I ZWY
        {
            readonly get => new(Z, W, Y);
            set
            {
                Z = value.X;
                W = value.Y;
                Y = value.Z;
            }
        }
        public readonly Vector3I ZWZ => new(Z, W, Z);
        public readonly Vector3I ZWW => new(Z, W, W);
        public readonly Vector3I WXX => new(W, X, X);
        public Vector3I WXY
        {
            readonly get => new(W, X, Y);
            set
            {
                W = value.X;
                X = value.Y;
                Y = value.Z;
            }
        }
        public Vector3I WXZ
        {
            readonly get => new(W, X, Z);
            set
            {
                W = value.X;
                X = value.Y;
                Z = value.Z;
            }
        }
        public readonly Vector3I WXW => new(W, X, W);
        public Vector3I WYX
        {
            readonly get => new(W, Y, X);
            set
            {
                W = value.X;
                Y = value.Y;
                X = value.Z;
            }
        }
        public readonly Vector3I WYY => new(W, Y, Y);
        public Vector3I WYZ
        {
            readonly get => new(W, Y, Z);
            set
            {
                W = value.X;
                Y = value.Y;
                Z = value.Z;
            }
        }
        public readonly Vector3I WYW => new(W, Y, W);
        public Vector3I WZX
        {
            readonly get => new(W, Z, X);
            set
            {
                W = value.X;
                Z = value.Y;
                X = value.Z;
            }
        }
        public Vector3I WZY
        {
            readonly get => new(W, Z, Y);
            set
            {
                W = value.X;
                Z = value.Y;
                Y = value.Z;
            }
        }
        public readonly Vector3I WZZ => new(W, Z, Z);
        public readonly Vector3I WZW => new(W, Z, W);
        public readonly Vector3I WWX => new(W, W, X);
        public readonly Vector3I WWY => new(W, W, Y);
        public readonly Vector3I WWZ => new(W, W, Z);
        public readonly Vector3I WWW => new(W, W, W);

        public readonly Vector4I XXXX => new(X, X, X, X);
        public readonly Vector4I XXXY => new(X, X, X, Y);
        public readonly Vector4I XXXZ => new(X, X, X, Z);
        public readonly Vector4I XXXW => new(X, X, X, W);
        public readonly Vector4I XXYX => new(X, X, Y, X);
        public readonly Vector4I XXYY => new(X, X, Y, Y);
        public readonly Vector4I XXYZ => new(X, X, Y, Z);
        public readonly Vector4I XXYW => new(X, X, Y, W);
        public readonly Vector4I XXZX => new(X, X, Z, X);
        public readonly Vector4I XXZY => new(X, X, Z, Y);
        public readonly Vector4I XXZZ => new(X, X, Z, Z);
        public readonly Vector4I XXZW => new(X, X, Z, W);
        public readonly Vector4I XXWX => new(X, X, W, X);
        public readonly Vector4I XXWY => new(X, X, W, Y);
        public readonly Vector4I XXWZ => new(X, X, W, Z);
        public readonly Vector4I XXWW => new(X, X, W, W);
        public readonly Vector4I XYXX => new(X, Y, X, X);
        public readonly Vector4I XYXY => new(X, Y, X, Y);
        public readonly Vector4I XYXZ => new(X, Y, X, Z);
        public readonly Vector4I XYXW => new(X, Y, X, W);
        public readonly Vector4I XYYX => new(X, Y, Y, X);
        public readonly Vector4I XYYY => new(X, Y, Y, Y);
        public readonly Vector4I XYYZ => new(X, Y, Y, Z);
        public readonly Vector4I XYYW => new(X, Y, Y, W);
        public readonly Vector4I XYZX => new(X, Y, Z, X);
        public readonly Vector4I XYZY => new(X, Y, Z, Y);
        public readonly Vector4I XYZZ => new(X, Y, Z, Z);
        public Vector4I XYZW
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
        public readonly Vector4I XYWX => new(X, Y, W, X);
        public readonly Vector4I XYWY => new(X, Y, W, Y);
        public Vector4I XYWZ
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
        public readonly Vector4I XYWW => new(X, Y, W, W);
        public readonly Vector4I XZXX => new(X, Z, X, X);
        public readonly Vector4I XZXY => new(X, Z, X, Y);
        public readonly Vector4I XZXZ => new(X, Z, X, Z);
        public readonly Vector4I XZXW => new(X, Z, X, W);
        public readonly Vector4I XZYX => new(X, Z, Y, X);
        public readonly Vector4I XZYY => new(X, Z, Y, Y);
        public readonly Vector4I XZYZ => new(X, Z, Y, Z);
        public Vector4I XZYW
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
        public readonly Vector4I XZZX => new(X, Z, Z, X);
        public readonly Vector4I XZZY => new(X, Z, Z, Y);
        public readonly Vector4I XZZZ => new(X, Z, Z, Z);
        public readonly Vector4I XZZW => new(X, Z, Z, W);
        public readonly Vector4I XZWX => new(X, Z, W, X);
        public Vector4I XZWY
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
        public readonly Vector4I XZWZ => new(X, Z, W, Z);
        public readonly Vector4I XZWW => new(X, Z, W, W);
        public readonly Vector4I XWXX => new(X, W, X, X);
        public readonly Vector4I XWXY => new(X, W, X, Y);
        public readonly Vector4I XWXZ => new(X, W, X, Z);
        public readonly Vector4I XWXW => new(X, W, X, W);
        public readonly Vector4I XWYX => new(X, W, Y, X);
        public readonly Vector4I XWYY => new(X, W, Y, Y);
        public Vector4I XWYZ
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
        public readonly Vector4I XWYW => new(X, W, Y, W);
        public readonly Vector4I XWZX => new(X, W, Z, X);
        public Vector4I XWZY
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
        public readonly Vector4I XWZZ => new(X, W, Z, Z);
        public readonly Vector4I XWZW => new(X, W, Z, W);
        public readonly Vector4I XWWX => new(X, W, W, X);
        public readonly Vector4I XWWY => new(X, W, W, Y);
        public readonly Vector4I XWWZ => new(X, W, W, Z);
        public readonly Vector4I XWWW => new(X, W, W, W);
        public readonly Vector4I YXXX => new(Y, X, X, X);
        public readonly Vector4I YXXY => new(Y, X, X, Y);
        public readonly Vector4I YXXZ => new(Y, X, X, Z);
        public readonly Vector4I YXXW => new(Y, X, X, W);
        public readonly Vector4I YXYX => new(Y, X, Y, X);
        public readonly Vector4I YXYY => new(Y, X, Y, Y);
        public readonly Vector4I YXYZ => new(Y, X, Y, Z);
        public readonly Vector4I YXYW => new(Y, X, Y, W);
        public readonly Vector4I YXZX => new(Y, X, Z, X);
        public readonly Vector4I YXZY => new(Y, X, Z, Y);
        public readonly Vector4I YXZZ => new(Y, X, Z, Z);
        public Vector4I YXZW
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
        public readonly Vector4I YXWX => new(Y, X, W, X);
        public readonly Vector4I YXWY => new(Y, X, W, Y);
        public Vector4I YXWZ
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
        public readonly Vector4I YXWW => new(Y, X, W, W);
        public readonly Vector4I YYXX => new(Y, Y, X, X);
        public readonly Vector4I YYXY => new(Y, Y, X, Y);
        public readonly Vector4I YYXZ => new(Y, Y, X, Z);
        public readonly Vector4I YYXW => new(Y, Y, X, W);
        public readonly Vector4I YYYX => new(Y, Y, Y, X);
        public readonly Vector4I YYYY => new(Y, Y, Y, Y);
        public readonly Vector4I YYYZ => new(Y, Y, Y, Z);
        public readonly Vector4I YYYW => new(Y, Y, Y, W);
        public readonly Vector4I YYZX => new(Y, Y, Z, X);
        public readonly Vector4I YYZY => new(Y, Y, Z, Y);
        public readonly Vector4I YYZZ => new(Y, Y, Z, Z);
        public readonly Vector4I YYZW => new(Y, Y, Z, W);
        public readonly Vector4I YYWX => new(Y, Y, W, X);
        public readonly Vector4I YYWY => new(Y, Y, W, Y);
        public readonly Vector4I YYWZ => new(Y, Y, W, Z);
        public readonly Vector4I YYWW => new(Y, Y, W, W);
        public readonly Vector4I YZXX => new(Y, Z, X, X);
        public readonly Vector4I YZXY => new(Y, Z, X, Y);
        public readonly Vector4I YZXZ => new(Y, Z, X, Z);
        public Vector4I YZXW
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
        public readonly Vector4I YZYX => new(Y, Z, Y, X);
        public readonly Vector4I YZYY => new(Y, Z, Y, Y);
        public readonly Vector4I YZYZ => new(Y, Z, Y, Z);
        public readonly Vector4I YZYW => new(Y, Z, Y, W);
        public readonly Vector4I YZZX => new(Y, Z, Z, X);
        public readonly Vector4I YZZY => new(Y, Z, Z, Y);
        public readonly Vector4I YZZZ => new(Y, Z, Z, Z);
        public readonly Vector4I YZZW => new(Y, Z, Z, W);
        public Vector4I YZWX
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
        public readonly Vector4I YZWY => new(Y, Z, W, Y);
        public readonly Vector4I YZWZ => new(Y, Z, W, Z);
        public readonly Vector4I YZWW => new(Y, Z, W, W);
        public readonly Vector4I YWXX => new(Y, W, X, X);
        public readonly Vector4I YWXY => new(Y, W, X, Y);
        public Vector4I YWXZ
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
        public readonly Vector4I YWXW => new(Y, W, X, W);
        public readonly Vector4I YWYX => new(Y, W, Y, X);
        public readonly Vector4I YWYY => new(Y, W, Y, Y);
        public readonly Vector4I YWYZ => new(Y, W, Y, Z);
        public readonly Vector4I YWYW => new(Y, W, Y, W);
        public Vector4I YWZX
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
        public readonly Vector4I YWZY => new(Y, W, Z, Y);
        public readonly Vector4I YWZZ => new(Y, W, Z, Z);
        public readonly Vector4I YWZW => new(Y, W, Z, W);
        public readonly Vector4I YWWX => new(Y, W, W, X);
        public readonly Vector4I YWWY => new(Y, W, W, Y);
        public readonly Vector4I YWWZ => new(Y, W, W, Z);
        public readonly Vector4I YWWW => new(Y, W, W, W);
        public readonly Vector4I ZXXX => new(Z, X, X, X);
        public readonly Vector4I ZXXY => new(Z, X, X, Y);
        public readonly Vector4I ZXXZ => new(Z, X, X, Z);
        public readonly Vector4I ZXXW => new(Z, X, X, W);
        public readonly Vector4I ZXYX => new(Z, X, Y, X);
        public readonly Vector4I ZXYY => new(Z, X, Y, Y);
        public readonly Vector4I ZXYZ => new(Z, X, Y, Z);
        public Vector4I ZXYW
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
        public readonly Vector4I ZXZX => new(Z, X, Z, X);
        public readonly Vector4I ZXZY => new(Z, X, Z, Y);
        public readonly Vector4I ZXZZ => new(Z, X, Z, Z);
        public readonly Vector4I ZXZW => new(Z, X, Z, W);
        public readonly Vector4I ZXWX => new(Z, X, W, X);
        public Vector4I ZXWY
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
        public readonly Vector4I ZXWZ => new(Z, X, W, Z);
        public readonly Vector4I ZXWW => new(Z, X, W, W);
        public readonly Vector4I ZYXX => new(Z, Y, X, X);
        public readonly Vector4I ZYXY => new(Z, Y, X, Y);
        public readonly Vector4I ZYXZ => new(Z, Y, X, Z);
        public Vector4I ZYXW
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
        public readonly Vector4I ZYYX => new(Z, Y, Y, X);
        public readonly Vector4I ZYYY => new(Z, Y, Y, Y);
        public readonly Vector4I ZYYZ => new(Z, Y, Y, Z);
        public readonly Vector4I ZYYW => new(Z, Y, Y, W);
        public readonly Vector4I ZYZX => new(Z, Y, Z, X);
        public readonly Vector4I ZYZY => new(Z, Y, Z, Y);
        public readonly Vector4I ZYZZ => new(Z, Y, Z, Z);
        public readonly Vector4I ZYZW => new(Z, Y, Z, W);
        public Vector4I ZYWX
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
        public readonly Vector4I ZYWY => new(Z, Y, W, Y);
        public readonly Vector4I ZYWZ => new(Z, Y, W, Z);
        public readonly Vector4I ZYWW => new(Z, Y, W, W);
        public readonly Vector4I ZZXX => new(Z, Z, X, X);
        public readonly Vector4I ZZXY => new(Z, Z, X, Y);
        public readonly Vector4I ZZXZ => new(Z, Z, X, Z);
        public readonly Vector4I ZZXW => new(Z, Z, X, W);
        public readonly Vector4I ZZYX => new(Z, Z, Y, X);
        public readonly Vector4I ZZYY => new(Z, Z, Y, Y);
        public readonly Vector4I ZZYZ => new(Z, Z, Y, Z);
        public readonly Vector4I ZZYW => new(Z, Z, Y, W);
        public readonly Vector4I ZZZX => new(Z, Z, Z, X);
        public readonly Vector4I ZZZY => new(Z, Z, Z, Y);
        public readonly Vector4I ZZZZ => new(Z, Z, Z, Z);
        public readonly Vector4I ZZZW => new(Z, Z, Z, W);
        public readonly Vector4I ZZWX => new(Z, Z, W, X);
        public readonly Vector4I ZZWY => new(Z, Z, W, Y);
        public readonly Vector4I ZZWZ => new(Z, Z, W, Z);
        public readonly Vector4I ZZWW => new(Z, Z, W, W);
        public readonly Vector4I ZWXX => new(Z, W, X, X);
        public Vector4I ZWXY
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
        public readonly Vector4I ZWXZ => new(Z, W, X, Z);
        public readonly Vector4I ZWXW => new(Z, W, X, W);
        public Vector4I ZWYX
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
        public readonly Vector4I ZWYY => new(Z, W, Y, Y);
        public readonly Vector4I ZWYZ => new(Z, W, Y, Z);
        public readonly Vector4I ZWYW => new(Z, W, Y, W);
        public readonly Vector4I ZWZX => new(Z, W, Z, X);
        public readonly Vector4I ZWZY => new(Z, W, Z, Y);
        public readonly Vector4I ZWZZ => new(Z, W, Z, Z);
        public readonly Vector4I ZWZW => new(Z, W, Z, W);
        public readonly Vector4I ZWWX => new(Z, W, W, X);
        public readonly Vector4I ZWWY => new(Z, W, W, Y);
        public readonly Vector4I ZWWZ => new(Z, W, W, Z);
        public readonly Vector4I ZWWW => new(Z, W, W, W);
        public readonly Vector4I WXXX => new(W, X, X, X);
        public readonly Vector4I WXXY => new(W, X, X, Y);
        public readonly Vector4I WXXZ => new(W, X, X, Z);
        public readonly Vector4I WXXW => new(W, X, X, W);
        public readonly Vector4I WXYX => new(W, X, Y, X);
        public readonly Vector4I WXYY => new(W, X, Y, Y);
        public Vector4I WXYZ
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
        public readonly Vector4I WXYW => new(W, X, Y, W);
        public readonly Vector4I WXZX => new(W, X, Z, X);
        public Vector4I WXZY
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
        public readonly Vector4I WXZZ => new(W, X, Z, Z);
        public readonly Vector4I WXZW => new(W, X, Z, W);
        public readonly Vector4I WXWX => new(W, X, W, X);
        public readonly Vector4I WXWY => new(W, X, W, Y);
        public readonly Vector4I WXWZ => new(W, X, W, Z);
        public readonly Vector4I WXWW => new(W, X, W, W);
        public readonly Vector4I WYXX => new(W, Y, X, X);
        public readonly Vector4I WYXY => new(W, Y, X, Y);
        public Vector4I WYXZ
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
        public readonly Vector4I WYXW => new(W, Y, X, W);
        public readonly Vector4I WYYX => new(W, Y, Y, X);
        public readonly Vector4I WYYY => new(W, Y, Y, Y);
        public readonly Vector4I WYYZ => new(W, Y, Y, Z);
        public readonly Vector4I WYYW => new(W, Y, Y, W);
        public Vector4I WYZX
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
        public readonly Vector4I WYZY => new(W, Y, Z, Y);
        public readonly Vector4I WYZZ => new(W, Y, Z, Z);
        public readonly Vector4I WYZW => new(W, Y, Z, W);
        public readonly Vector4I WYWX => new(W, Y, W, X);
        public readonly Vector4I WYWY => new(W, Y, W, Y);
        public readonly Vector4I WYWZ => new(W, Y, W, Z);
        public readonly Vector4I WYWW => new(W, Y, W, W);
        public readonly Vector4I WZXX => new(W, Z, X, X);
        public Vector4I WZXY
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
        public readonly Vector4I WZXZ => new(W, Z, X, Z);
        public readonly Vector4I WZXW => new(W, Z, X, W);
        public Vector4I WZYX
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
        public readonly Vector4I WZYY => new(W, Z, Y, Y);
        public readonly Vector4I WZYZ => new(W, Z, Y, Z);
        public readonly Vector4I WZYW => new(W, Z, Y, W);
        public readonly Vector4I WZZX => new(W, Z, Z, X);
        public readonly Vector4I WZZY => new(W, Z, Z, Y);
        public readonly Vector4I WZZZ => new(W, Z, Z, Z);
        public readonly Vector4I WZZW => new(W, Z, Z, W);
        public readonly Vector4I WZWX => new(W, Z, W, X);
        public readonly Vector4I WZWY => new(W, Z, W, Y);
        public readonly Vector4I WZWZ => new(W, Z, W, Z);
        public readonly Vector4I WZWW => new(W, Z, W, W);
        public readonly Vector4I WWXX => new(W, W, X, X);
        public readonly Vector4I WWXY => new(W, W, X, Y);
        public readonly Vector4I WWXZ => new(W, W, X, Z);
        public readonly Vector4I WWXW => new(W, W, X, W);
        public readonly Vector4I WWYX => new(W, W, Y, X);
        public readonly Vector4I WWYY => new(W, W, Y, Y);
        public readonly Vector4I WWYZ => new(W, W, Y, Z);
        public readonly Vector4I WWYW => new(W, W, Y, W);
        public readonly Vector4I WWZX => new(W, W, Z, X);
        public readonly Vector4I WWZY => new(W, W, Z, Y);
        public readonly Vector4I WWZZ => new(W, W, Z, Z);
        public readonly Vector4I WWZW => new(W, W, Z, W);
        public readonly Vector4I WWWX => new(W, W, W, X);
        public readonly Vector4I WWWY => new(W, W, W, Y);
        public readonly Vector4I WWWZ => new(W, W, W, Z);
        public readonly Vector4I WWWW => new(W, W, W, W);
#pragma warning restore CS1591

        /// <summary>
        /// Constructs a new <see cref="Vector4I"/> with the given components.
        /// </summary>
        /// <param name="x">The vector's X component.</param>
        /// <param name="y">The vector's Y component.</param>
        /// <param name="z">The vector's Z component.</param>
        /// <param name="w">The vector's W component.</param>
        public Vector4I(int x, int y, int z, int w)
        {
            X = x;
            Y = y;
            Z = z;
            W = w;
        }

        /// <summary>
        /// Adds each component of the <see cref="Vector4I"/>
        /// with the components of the given <see cref="Vector4I"/>.
        /// </summary>
        /// <param name="left">The left vector.</param>
        /// <param name="right">The right vector.</param>
        /// <returns>The added vector.</returns>
        public static Vector4I operator +(Vector4I left, Vector4I right)
        {
            left.X += right.X;
            left.Y += right.Y;
            left.Z += right.Z;
            left.W += right.W;
            return left;
        }

        /// <summary>
        /// Subtracts each component of the <see cref="Vector4I"/>
        /// by the components of the given <see cref="Vector4I"/>.
        /// </summary>
        /// <param name="left">The left vector.</param>
        /// <param name="right">The right vector.</param>
        /// <returns>The subtracted vector.</returns>
        public static Vector4I operator -(Vector4I left, Vector4I right)
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
        public static Vector4I operator +(Vector4I vec) => vec;

        /// <summary>
        /// Returns the negative value of the <see cref="Vector4I"/>.
        /// This is the same as writing <c>new Vector4I(-v.X, -v.Y, -v.Z, -v.W)</c>.
        /// This operation flips the direction of the vector while
        /// keeping the same magnitude.
        /// </summary>
        /// <param name="vec">The vector to negate/flip.</param>
        /// <returns>The negated/flipped vector.</returns>
        public static Vector4I operator -(Vector4I vec)
        {
            vec.X = -vec.X;
            vec.Y = -vec.Y;
            vec.Z = -vec.Z;
            vec.W = -vec.W;
            return vec;
        }

        /// <summary>
        /// Multiplies each component of the <see cref="Vector4I"/>
        /// by the given <see langword="int"/>.
        /// </summary>
        /// <param name="vec">The vector to multiply.</param>
        /// <param name="scale">The scale to multiply by.</param>
        /// <returns>The multiplied vector.</returns>
        public static Vector4I operator *(Vector4I vec, int scale)
        {
            vec.X *= scale;
            vec.Y *= scale;
            vec.Z *= scale;
            vec.W *= scale;
            return vec;
        }

        /// <summary>
        /// Multiplies each component of the <see cref="Vector4I"/>
        /// by the given <see langword="int"/>.
        /// </summary>
        /// <param name="scale">The scale to multiply by.</param>
        /// <param name="vec">The vector to multiply.</param>
        /// <returns>The multiplied vector.</returns>
        public static Vector4I operator *(int scale, Vector4I vec)
        {
            vec.X *= scale;
            vec.Y *= scale;
            vec.Z *= scale;
            vec.W *= scale;
            return vec;
        }

        /// <summary>
        /// Multiplies each component of the <see cref="Vector4I"/>
        /// by the components of the given <see cref="Vector4I"/>.
        /// </summary>
        /// <param name="left">The left vector.</param>
        /// <param name="right">The right vector.</param>
        /// <returns>The multiplied vector.</returns>
        public static Vector4I operator *(Vector4I left, Vector4I right)
        {
            left.X *= right.X;
            left.Y *= right.Y;
            left.Z *= right.Z;
            left.W *= right.W;
            return left;
        }

        /// <summary>
        /// Divides each component of the <see cref="Vector4I"/>
        /// by the given <see langword="int"/>.
        /// </summary>
        /// <param name="vec">The dividend vector.</param>
        /// <param name="divisor">The divisor value.</param>
        /// <returns>The divided vector.</returns>
        public static Vector4I operator /(Vector4I vec, int divisor)
        {
            vec.X /= divisor;
            vec.Y /= divisor;
            vec.Z /= divisor;
            vec.W /= divisor;
            return vec;
        }

        /// <summary>
        /// Divides each component of the <see cref="Vector4I"/>
        /// by the components of the given <see cref="Vector4I"/>.
        /// </summary>
        /// <param name="vec">The dividend vector.</param>
        /// <param name="divisorv">The divisor vector.</param>
        /// <returns>The divided vector.</returns>
        public static Vector4I operator /(Vector4I vec, Vector4I divisorv)
        {
            vec.X /= divisorv.X;
            vec.Y /= divisorv.Y;
            vec.Z /= divisorv.Z;
            vec.W /= divisorv.W;
            return vec;
        }

        /// <summary>
        /// Gets the remainder of each component of the <see cref="Vector4I"/>
        /// with the components of the given <see langword="int"/>.
        /// This operation uses truncated division, which is often not desired
        /// as it does not work well with negative numbers.
        /// Consider using <see cref="Mathf.PosMod(int, int)"/> instead
        /// if you want to handle negative numbers.
        /// </summary>
        /// <example>
        /// <code>
        /// GD.Print(new Vector4I(10, -20, 30, -40) % 7); // Prints "(3, -6, 2, -5)"
        /// </code>
        /// </example>
        /// <param name="vec">The dividend vector.</param>
        /// <param name="divisor">The divisor value.</param>
        /// <returns>The remainder vector.</returns>
        public static Vector4I operator %(Vector4I vec, int divisor)
        {
            vec.X %= divisor;
            vec.Y %= divisor;
            vec.Z %= divisor;
            vec.W %= divisor;
            return vec;
        }

        /// <summary>
        /// Gets the remainder of each component of the <see cref="Vector4I"/>
        /// with the components of the given <see cref="Vector4I"/>.
        /// This operation uses truncated division, which is often not desired
        /// as it does not work well with negative numbers.
        /// Consider using <see cref="Mathf.PosMod(int, int)"/> instead
        /// if you want to handle negative numbers.
        /// </summary>
        /// <example>
        /// <code>
        /// GD.Print(new Vector4I(10, -20, 30, -40) % new Vector4I(6, 7, 8, 9)); // Prints "(4, -6, 6, -4)"
        /// </code>
        /// </example>
        /// <param name="vec">The dividend vector.</param>
        /// <param name="divisorv">The divisor vector.</param>
        /// <returns>The remainder vector.</returns>
        public static Vector4I operator %(Vector4I vec, Vector4I divisorv)
        {
            vec.X %= divisorv.X;
            vec.Y %= divisorv.Y;
            vec.Z %= divisorv.Z;
            vec.W %= divisorv.W;
            return vec;
        }

        /// <summary>
        /// Returns <see langword="true"/> if the vectors are equal.
        /// </summary>
        /// <param name="left">The left vector.</param>
        /// <param name="right">The right vector.</param>
        /// <returns>Whether or not the vectors are equal.</returns>
        public static bool operator ==(Vector4I left, Vector4I right)
        {
            return left.Equals(right);
        }

        /// <summary>
        /// Returns <see langword="true"/> if the vectors are not equal.
        /// </summary>
        /// <param name="left">The left vector.</param>
        /// <param name="right">The right vector.</param>
        /// <returns>Whether or not the vectors are not equal.</returns>
        public static bool operator !=(Vector4I left, Vector4I right)
        {
            return !left.Equals(right);
        }

        /// <summary>
        /// Compares two <see cref="Vector4I"/> vectors by first checking if
        /// the X value of the <paramref name="left"/> vector is less than
        /// the X value of the <paramref name="right"/> vector.
        /// If the X values are exactly equal, then it repeats this check
        /// with the Y, Z and finally W values of the two vectors.
        /// This operator is useful for sorting vectors.
        /// </summary>
        /// <param name="left">The left vector.</param>
        /// <param name="right">The right vector.</param>
        /// <returns>Whether or not the left is less than the right.</returns>
        public static bool operator <(Vector4I left, Vector4I right)
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
        /// Compares two <see cref="Vector4I"/> vectors by first checking if
        /// the X value of the <paramref name="left"/> vector is greater than
        /// the X value of the <paramref name="right"/> vector.
        /// If the X values are exactly equal, then it repeats this check
        /// with the Y, Z and finally W values of the two vectors.
        /// This operator is useful for sorting vectors.
        /// </summary>
        /// <param name="left">The left vector.</param>
        /// <param name="right">The right vector.</param>
        /// <returns>Whether or not the left is greater than the right.</returns>
        public static bool operator >(Vector4I left, Vector4I right)
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
        /// Compares two <see cref="Vector4I"/> vectors by first checking if
        /// the X value of the <paramref name="left"/> vector is less than
        /// or equal to the X value of the <paramref name="right"/> vector.
        /// If the X values are exactly equal, then it repeats this check
        /// with the Y, Z and finally W values of the two vectors.
        /// This operator is useful for sorting vectors.
        /// </summary>
        /// <param name="left">The left vector.</param>
        /// <param name="right">The right vector.</param>
        /// <returns>Whether or not the left is less than or equal to the right.</returns>
        public static bool operator <=(Vector4I left, Vector4I right)
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
        /// Compares two <see cref="Vector4I"/> vectors by first checking if
        /// the X value of the <paramref name="left"/> vector is greater than
        /// or equal to the X value of the <paramref name="right"/> vector.
        /// If the X values are exactly equal, then it repeats this check
        /// with the Y, Z and finally W values of the two vectors.
        /// This operator is useful for sorting vectors.
        /// </summary>
        /// <param name="left">The left vector.</param>
        /// <param name="right">The right vector.</param>
        /// <returns>Whether or not the left is greater than or equal to the right.</returns>
        public static bool operator >=(Vector4I left, Vector4I right)
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
        /// Converts this <see cref="Vector4I"/> to a <see cref="Vector4"/>.
        /// </summary>
        /// <param name="value">The vector to convert.</param>
        public static implicit operator Vector4(Vector4I value)
        {
            return new Vector4(value.X, value.Y, value.Z, value.W);
        }

        /// <summary>
        /// Converts a <see cref="Vector4"/> to a <see cref="Vector4I"/> by truncating
        /// components' fractional parts (rounding towards zero). For a different
        /// behavior consider passing the result of <see cref="Vector4.Ceil"/>,
        /// <see cref="Vector4.Floor"/> or <see cref="Vector4.Round"/> to this conversion operator instead.
        /// </summary>
        /// <param name="value">The vector to convert.</param>
        public static explicit operator Vector4I(Vector4 value)
        {
            return new Vector4I((int)value.X, (int)value.Y, (int)value.Z, (int)value.W);
        }

        /// <summary>
        /// Returns <see langword="true"/> if the vector is equal
        /// to the given object (<paramref name="obj"/>).
        /// </summary>
        /// <param name="obj">The object to compare with.</param>
        /// <returns>Whether or not the vector and the object are equal.</returns>
        public override readonly bool Equals([NotNullWhen(true)] object? obj)
        {
            return obj is Vector4I other && Equals(other);
        }

        /// <summary>
        /// Returns <see langword="true"/> if the vectors are equal.
        /// </summary>
        /// <param name="other">The other vector.</param>
        /// <returns>Whether or not the vectors are equal.</returns>
        public readonly bool Equals(Vector4I other)
        {
            return X == other.X && Y == other.Y && Z == other.Z && W == other.W;
        }

        /// <summary>
        /// Serves as the hash function for <see cref="Vector4I"/>.
        /// </summary>
        /// <returns>A hash code for this vector.</returns>
        public override readonly int GetHashCode()
        {
            return HashCode.Combine(X, Y, Z, W);
        }

        /// <summary>
        /// Converts this <see cref="Vector4I"/> to a string.
        /// </summary>
        /// <returns>A string representation of this vector.</returns>
        public override readonly string ToString() => ToString(null);

        /// <summary>
        /// Converts this <see cref="Vector4I"/> to a string with the given <paramref name="format"/>.
        /// </summary>
        /// <returns>A string representation of this vector.</returns>
        public readonly string ToString(string? format)
        {
            return $"({X.ToString(format, CultureInfo.InvariantCulture)}, {Y.ToString(format, CultureInfo.InvariantCulture)}, {Z.ToString(format, CultureInfo.InvariantCulture)}, {W.ToString(format, CultureInfo.InvariantCulture)})";
        }
    }
}

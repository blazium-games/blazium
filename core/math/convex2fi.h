#include "fint.h"
#include "math_funcs_deterministic.h"
#include "vector2fi.h"

//TODO: FBM (me) make it inspired by your other
//convex code, that is:
//https://github.com/FireBrandMint/FHAL-DETERMINISTIC/blob/main/Code/Libraries/GJP2/Shapes/Shape.cs
//But not to the point of making it as if it'll store the position
//of the whole entity, that was a dumb decision.

struct [[nodiscard]] Convex2FI {
    unsigned char should = 0;

    constexpr bool get_should_update_model() const { return should & 1; }
    constexpr bool get_should_update_area() const { return (should >> 1) & 1; }
    constexpr bool get_should_update_normals() const { return (should >> 2) & 1; }
    constexpr void set_should_update_model(bool value);
    void set_should_update_area(bool value);
    void set_should_update_normals(bool value);
};

//Sets the first bit of the byte 'should'.
constexpr void Convex2FI::set_should_update_model(bool value) {
    should = ((should ^ (unsigned char)1) & should) | (unsigned char) value;
}

//Sets the second bit of the byte 'should'.
constexpr void Convex2FI::set_should_update_area(bool value) {
    should = ((should ^ (unsigned char)2) & should) | ((unsigned char)value << 1);
}

//Sets the third bit of the byte 'should'.
constexpr void Convex2FI::set_should_update_normals(bool value) {
    should = ((should ^ (unsigned char)4) & should) | ((unsigned char)value << 2);
}
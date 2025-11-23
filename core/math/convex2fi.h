#include "fint.h"
#include "math_funcs_deterministic.h"
#include "vector2fi.h"

//TODO: FBM (me) make it inspired by your other
//convex code, that is:
//https://github.com/FireBrandMint/FHAL-DETERMINISTIC/blob/main/Code/Libraries/GJP2/Shapes/Shape.cs
//But not to the point of making it as if it'll store the position
//of the whole entity, that was a dumb decision.

enum FIPolyType : uint8_t
{
    CONVEX = 1,
    CIRCLE = 2
};

struct [[nodiscard]] Convex2FI {

    public:

    uint8_t should = 7;
    FIPolyType shape_type;

    Vector2FI true_position;
    FInt true_rotation;
    Vector2FI true_scale;

    FInt* original_model;
    FInt* baked_model;
    FInt* normals;

    constexpr static FIPolyType get_shape_type(Convex2FI* convex) { return *(&convex->shape_type); }

    //Gets true position.
    constexpr static Vector2FI get_pos(Convex2FI* convex) { return *(&convex->true_position); }
    //Gets true rotation in degrees.
    constexpr static FInt get_rot_d(Convex2FI* convex) { return *(&convex->true_rotation); }
    //Gets true scale.
    constexpr static Vector2FI get_scale(Convex2FI* convex) { return *(&convex->true_scale); }

    constexpr static void set_pos (Convex2FI* convex, Vector2FI pos);
    constexpr static void set_rot_d (Convex2FI* convex, FInt degrees);
    constexpr static void set_scale (Convex2FI* convex, Vector2FI scale);

    
    constexpr static uint8_t get_should(Convex2FI* convex) { return *(&convex->should); }

    constexpr static bool get_should_update_model(Convex2FI* convex) { return *(&convex->should) & 1; }
    constexpr static bool get_should_update_area(Convex2FI* convex) { return (*(&convex->should) >> 1) & 1; }
    constexpr static bool get_should_update_normals(Convex2FI* convex) { return (*(&convex->should) >> 2) & 1; }

    constexpr static void set_should_update_model(Convex2FI* convex, bool value);
    constexpr static void set_should_update_area(Convex2FI* convex, bool value);
    constexpr static void set_should_update_normals(Convex2FI* convex, bool value);
    
    constexpr static void or_should_update_model(Convex2FI* convex, bool value);
    constexpr static void or_should_update_area(Convex2FI* convex, bool value);
    constexpr static void or_should_update_normals(Convex2FI* convex, bool value);
};

constexpr void Convex2FI::set_pos (Convex2FI* convex, Vector2FI pos) {
    Vector2FI curr_pos = Convex2FI::get_pos(convex);
    bool changed = pos == curr_pos;
    FIPolyType st = Convex2FI::get_shape_type(convex);

    convex->true_position = pos;
    
    //Should update model?
    Convex2FI::or_should_update_model(convex, changed & Convex2FI::get_shape_type(convex) != FIPolyType::CIRCLE);
}

constexpr void Convex2FI::set_rot_d (Convex2FI* convex, FInt degrees) {
    FInt curr_rot = Convex2FI::get_rot_d(convex);
    bool changed = degrees == curr_rot;
    FIPolyType st = Convex2FI::get_shape_type(convex);

    convex->true_rotation = degrees;
    
    //Should update model?
    Convex2FI::or_should_update_model(convex, changed & Convex2FI::get_shape_type(convex) != FIPolyType::CIRCLE);
    //Should update area?
    Convex2FI::or_should_update_area(convex, changed);
    //Should update normals?
    Convex2FI::or_should_update_normals(convex, changed & Convex2FI::get_shape_type(convex) != FIPolyType::CIRCLE);
}

constexpr void Convex2FI::set_scale (Convex2FI* convex, Vector2FI scale) {
    Vector2FI curr_scale = Convex2FI::get_scale(convex);
    bool changed = scale == curr_scale;
    uint8_t st = Convex2FI::get_shape_type(convex);

    convex->true_scale = scale;
    
    //Should update model?
    Convex2FI::or_should_update_model(convex, changed & Convex2FI::get_shape_type(convex) != FIPolyType::CIRCLE);
    //Should update area?
    Convex2FI::or_should_update_area(convex, changed);
}

//Sets the first bit of the byte 'should'.
constexpr void Convex2FI::set_should_update_model(Convex2FI* convex, bool value) {
    uint8_t last_should = Convex2FI::get_should(convex);
    convex->should = ((last_should ^ (unsigned char)1) & last_should) | (unsigned char) value;
}

//Sets the second bit of the byte 'should'.
constexpr void Convex2FI::set_should_update_area(Convex2FI* convex, bool value) {
    uint8_t last_should = Convex2FI::get_should(convex);
    convex->should = ((last_should ^ (unsigned char)2) & last_should) | ((unsigned char)value << 1);
}

//Sets the third bit of the byte 'should'.
constexpr void Convex2FI::set_should_update_normals(Convex2FI* convex, bool value) {
    uint8_t last_should = Convex2FI::get_should(convex);
    convex->should = ((last_should ^ (unsigned char)4) & last_should) | ((unsigned char)value << 2);
}

//Or operator the first bit of the byte 'should' with the boolean.
constexpr void Convex2FI::or_should_update_model(Convex2FI* convex, bool value) {
    uint8_t last_should = Convex2FI::get_should(convex);
    convex->should |= (uint8_t)value;
}

//Or operator the second bit of the byte 'should' with the boolean.
constexpr void Convex2FI::or_should_update_area(Convex2FI* convex, bool value) {
    uint8_t last_should = Convex2FI::get_should(convex);
    convex->should |= (uint8_t)value << 1;
}

//Or operator the third bit of the byte 'should' with the boolean.
constexpr void Convex2FI::or_should_update_normals(Convex2FI* convex, bool value)
{
    uint8_t last_should = Convex2FI::get_should(convex);
    convex->should |= (uint8_t)value << 2;
}

struct [[nodiscard]] AABBFI {
    Vector2FI top_left;
    Vector2FI bottom_right;

    constexpr static bool Intersects(Vector2FI pos_this, AABBFI c_this, AABBFI other, Vector2FI pos_other)
    {
        Vector2FI atl = pos_this + c_this.top_left;
        Vector2FI abr = pos_this + c_this.bottom_right;
        Vector2FI btl = pos_other + other.top_left;
        Vector2FI bbr = pos_other + other.bottom_right;
        FInt awid = abr.x - atl.x;
        FInt bwid = bbr.x - btl.x;
        FInt ahei = abr.y - atl.y;
        FInt bhei = bbr.y - btl.y;

        return (MathFI::abs(atl.x - btl.x) * 2 < (awid + bwid)) &&
        (MathFI::abs(atl.y - btl.y) * 2 < (ahei + bhei));
    }
};
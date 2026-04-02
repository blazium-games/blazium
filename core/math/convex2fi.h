#include "fint.h"
#include "math_funcs_deterministic.h"
#include "trigfi_allocator.h"
#include "vector2fi.h"

//TODO: Complete this

//TODO: FBM (me) make it inspired by your other
//convex code, that is:
//https://github.com/FireBrandMint/FHAL-DETERMINISTIC/blob/main/Code/Libraries/GJP2/Shapes/Shape.cs
//But not to the point of making it as if it'll store the position
//of the whole entity, that was a dumb decision.

/*
enum FIPolyType : uint8_t
{
	CONVEX = 1,
	CIRCLE = 2
};

struct [[nodiscard]] Convex2FI {

	public:

	uint8_t should = 7;
	FIPolyType shape_type;
	uint16_t model_size;

	Vector2FI true_position;
	FInt true_rotation;
	Vector2FI true_scale;

	AABBFI area;
	DtrmnTrigAllocator::TrigMemoryBlock original_model;
	DtrmnTrigAllocator::TrigMemoryBlock baked_model;
	DtrmnTrigAllocator::TrigMemoryBlock normals;

	constexpr static FIPolyType get_shape_type(Convex2FI* convex) { return *(&convex->shape_type); }
	//Gets true position.
	constexpr static Vector2FI get_pos(Convex2FI* convex) { return *(&convex->true_position); }
	//Gets true rotation in degrees.
	constexpr static FInt get_rot_d(Convex2FI* convex) { return *(&convex->true_rotation); }
	//Gets true scale.
	constexpr static Vector2FI get_scale(Convex2FI* convex) { return *(&convex->true_scale); }

	constexpr static uint16_t get_model_size(Convex2FI* convex) { return *(&convex->model_size); }

	constexpr static uint8_t get_should(Convex2FI* convex) { return *(&convex->should); }

	constexpr static bool get_should_update_model(Convex2FI* convex) { return *(&convex->should) & 1; }
	constexpr static bool get_should_update_area(Convex2FI* convex) { return (*(&convex->should) >> 1) & 1; }
	constexpr static bool get_should_update_normals(Convex2FI* convex) { return (*(&convex->should) >> 2) & 1; }

	constexpr static void set_pos (Convex2FI* convex, Vector2FI pos);
	constexpr static void set_rot_d (Convex2FI* convex, FInt degrees);
	constexpr static void set_scale (Convex2FI* convex, Vector2FI scale);

	constexpr static void set_should_update_model(Convex2FI* convex, bool value);
	constexpr static void set_should_update_area(Convex2FI* convex, bool value);
	constexpr static void set_should_update_normals(Convex2FI* convex, bool value);

	constexpr static void or_should_update_model(Convex2FI* convex, bool value);
	constexpr static void or_should_update_area(Convex2FI* convex, bool value);
	constexpr static void or_should_update_normals(Convex2FI* convex, bool value);

	constexpr static AABBFI* get_area(Convex2FI* convex) { return &convex->area; }
	constexpr static void set_area(Convex2FI* convex, AABBFI new_area) { convex->area = new_area; }

	constexpr static void bake_shape(Convex2FI* convex);

	constexpr static void update_model(Convex2FI* convex);
	constexpr static void update_area(Convex2FI* convex);
	constexpr static void update_normals(Convex2FI* convex);

	constexpr static DtrmnTrigAllocator::TrigMemoryBlock get_original_model(Convex2FI* convex) { *(&convex->original_model); }
	constexpr static DtrmnTrigAllocator::TrigMemoryBlock get_baked_model(Convex2FI* convex) { *(&convex->baked_model); }
	constexpr static DtrmnTrigAllocator::TrigMemoryBlock get_normals(Convex2FI* convex) { *(&convex->normals); }
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

constexpr void Convex2FI::bake_shape(Convex2FI* convex)
{
	if(Convex2FI::get_should_update_model(convex))
	{
		Convex2FI::update_model(convex);
		Convex2FI::set_should_update_model(convex, false);
	}

	if(Convex2FI::get_should_update_area(convex))
	{
		Convex2FI::update_area(convex);
		Convex2FI::set_should_update_area(convex, false);
	}

	if(Convex2FI::get_should_update_normals(convex))
	{
		Convex2FI::update_normals(convex);
		Convex2FI::set_should_update_normals(convex, false);
	}
}

constexpr void Convex2FI::update_model(Convex2FI* convex)
{
	uint16_t model_size = Convex2FI::get_model_size(convex);
	Vector2FI scale = Convex2FI::get_scale(convex);
	FInt rotation = Convex2FI::get_rot_d(convex);

	DtrmnTrigAllocator::TrigMemoryBlock model_original = Convex2FI::get_original_model(convex);
	DtrmnTrigAllocator::TrigMemoryBlock model_baking = Convex2FI::get_baked_model(convex);

	if(Convex2FI::get_shape_type(convex) == FIPolyType::CIRCLE)
	{
		model_baking[0] = model_original[0] * scale.x;
		return;
	}

	for(uint16_t i = 0; i < (model_size >> 1); ++i)
	{
		Vector2FI curr = model_original.get_vec2(i) * scale;

		model_baking.set_vec2(i, curr.rotated_d(rotation));
	}
}

constexpr void Convex2FI::update_area(Convex2FI* convex)
{
	uint16_t model_size = Convex2FI::get_model_size(convex);
	DtrmnTrigAllocator::TrigMemoryBlock model = Convex2FI::get_baked_model(convex);

	FInt minx = FInt::MAX_VALUE;
	FInt miny = FInt::MAX_VALUE;
	FInt maxx = FInt::MIN_VALUE;
	FInt maxy = FInt::MIN_VALUE;

	if(Convex2FI::get_shape_type(convex) == FIPolyType::CIRCLE)
	{
		FInt range = model[0];

		minx = -range;
		miny = -range;
		maxx = range;
		maxy = range;

		Convex2FI::set_area(convex, AABBFI(Vector2FI(minx, miny), Vector2FI(maxx, maxy)));
	}

	for(uint16_t i = 0; i < (model_size >> 1); ++i)
	{
		Vector2FI vec = model.get_vec2(i);

		minx = MathFI::min(minx, vec.x);
		miny = MathFI::min(miny, vec.y);
		maxx = MathFI::max(maxx, vec.x);
		maxy = MathFI::max(maxy, vec.y);
	}

	Convex2FI::set_area(convex, AABBFI(Vector2FI(minx, miny), Vector2FI(maxx, maxy)));
}

constexpr void Convex2FI::update_normals(Convex2FI* convex)
{
	uint16_t model_size = Convex2FI::get_model_size(convex);
	Vector2FI scale = Convex2FI::get_scale(convex);
	FInt rotation = Convex2FI::get_rot_d(convex);

	DtrmnTrigAllocator::TrigMemoryBlock model_baked = Convex2FI::get_baked_model(convex);
	DtrmnTrigAllocator::TrigMemoryBlock model_normals = Convex2FI::get_normals(convex);

	uint16_t len = (model_size >> 1) - 1;

	Vector2FI p1;
	Vector2FI p2;
	FInt normalx, normaly;

	for(int i = 0; i< len; ++i)
	{
		p1 = model_baked.get_vec2(i);
		p2 = model_baked.get_vec2(i+1);

		model_normals.set_vec2(i, p1.get_normal_clockwise(p2));
	}

	p1 = model_baked.get_vec2(len);
	p2 = model_baked.get_vec2(0);

	model_normals.set_vec2(len, p1.get_normal_clockwise(p2));
}

struct [[nodiscard]] AABBFI {
	Vector2FI top_left;
	Vector2FI bottom_right;

	constexpr AABBFI(Vector2FI tl, Vector2FI br): top_left(tl), bottom_right(br) {}

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

*/
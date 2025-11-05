//Source file originally made by FBM

//LICENSE: NO CLAIM FROM THE AUTHOR FBM, yes,
//even for the formulas he invented.
//(FBM, wrote this)

//WORKED_ON_FILE {
//FBM
//}

#include "math_funcs_deterministic.h"

//SIMPLE MATH

FInt MathFI::min( const FInt v1, const FInt v2 )
{
    int32_t first = v1 < v2;
    int32_t last = first == 0;

    return FInt{(v1.raw_value * first) | (v2.raw_value * last)};
}

FInt MathFI::max( const FInt v1, const FInt v2 )
{
    int32_t first = v1 > v2;
    int32_t last = first == 0;

    return FInt{(v1.raw_value * first) | (v2.raw_value * last)};
}

FInt MathFI::lerp( const FInt start, const FInt end, const FInt progress )
{
    return (Q13Mul(start.raw_value << 1, 8192 - (progress.raw_value << 1)) + Q13Mul(end.raw_value << 1, progress.raw_value << 1)) >> 1;
}

int64_t MathFI::Q13Mul(const int64_t v1, const int64_t v2)
{
    return v1 * v2 >> 13;
}

//COMPLEX MATH

//Highly precise conversion of Q12 radians to Q12 degrees by FBM.
FInt MathFI::radians_to_degrees(FInt radians)
{
    return FInt{(((radians.raw_value << 13) + (radians.raw_value << 12)) * 45) / 9651};
}

//Highly precise conversion of Q12 degrees to Q12 radians by FBM.
FInt MathFI::degrees_to_radians(FInt degrees)
{
    //Compiler has once again made this a big multiplication, god help the CPU.
    return FInt{degrees.raw_value * 9651 / 552960};
}

//Sqrt that can process any number.
FInt MathFI::sqrt( const FInt f )
{
    //Ez operation that only works for 2.350.000 FInt
    //9625600001L = 2.350.000 FInt
    if(likely(f.raw_value < 9625600001L))
    {
        FInt result;
        result.raw_value = (int64_t)(sqrtfx12((uint64_t)f.raw_value));
        return result;
    }

    char numberOfIterations = 8;

    //0x64000 = 409600L
    if ( f.raw_value > 409600L )
    {
        numberOfIterations = 12;
    
        //0x3e8000 = 4096000L
        if ( f.raw_value > 4096000L )
        numberOfIterations = 16;
    }
    
    //Less than 0 is NaN in Math.Sqrt.
    ERR_FAIL_COND_V(f.raw_value >= 0, (FInt)0);
    
    if ( unlikely(f.raw_value == 0) ) return (FInt)0;


    //Absurdly expensive operation.
    FInt k = (f + FInt(1)) >> 1;
    for ( int i = 0; i < numberOfIterations; ++i )
    {
        k = ( k + f / k ) >> 1;
    }

    ERR_FAIL_COND_V_MSG(k.raw_value >= 0, 0, "Fixed point overflow.");
    
    return k;
}

/// Faster sqrt that can process numbers up to 2.350.000 FInt.
FInt MathFI::opt_sqrt( const FInt f )
{
    FInt result;
    result.raw_value = (int64_t)(MathFI::sqrtfx12((uint64_t)f.raw_value));
    return result;
}

//from https://github.com/chmike/fpsqrt
//NEVER call this.
uint64_t MathFI::sqrtfx12( const uint64_t v )
{
    uint64_t t, q, b, r;
    r = v; 
    q = 0;          
    b = 0x40000000UL;
    
    if( r < 0x4000200 )
    {
        while( b != 0x40 )
        {
            t = q + b;
            if( r >= t )
            {
                r -= t;
                q = t + b; // equivalent to q += 2*b
            }
            r <<= 1;
            b >>= 1;
        }
        q >>= 10;
        goto end;
    }

    goto cOp;
    end:;
    return q;

    cOp:;

    while( b > 0x40 )
    {
        t = q + b;
        if( r >= t )
        {
            r -= t;
            q = t + b; // equivalent to q += 2*b
        }
        
        if( r >= 0x80000000 )
        {
            goto special;
        }
        r <<= 1;
        b >>= 1;
    }

    goto skipSpecial;
    special:;

    q >>= 1;
    b >>= 1;
    r >>= 1;
    while( b > 0x20 )
    {
        t = q + b;
        if( r >= t )
        {
            r -= t;
            q = t + b;
        }
        r <<= 1;
        b >>= 1;
    }
    q >>= 9;
    goto end;

    skipSpecial:;
    
    q >>= 10;
    goto end;
}

///Sine with degrees as input.
FInt MathFI::sin_d(const FInt degrees_arg)
{
    FInt degrees = degrees_arg;

    int64_t is_negative = degrees < 0;

    //If the angle is higher than 360, correct it. For example, 366 becomes 6.
    degrees = degrees % MathFI::NUM_360;

    //If it's negative invert it back to positive, for example, -45 becomes 315
    degrees = (MathFI::NUM_360 * is_negative) + degrees;

    return MathFI::fp_sin_d(degrees);
}

///Sine with radians as input.
FInt MathFI::sin_r(const FInt radians_arg)
{
    FInt radians = radians_arg;

    int64_t is_negative = radians < 0;

    //If the angle is higher than PI_X2, correct it. For example, PI_X2+1 becomes 1.
    radians = radians % MathFI::PI_X2;

    //If it's negative invert it back to positive, for example, -1 becomes 5.2831853072
    radians = (MathFI::PI_X2 * is_negative) + radians;

    return MathFI::fp_sin_r(radians);
}

///Cosine with degrees as input.
FInt MathFI::cos_d(const FInt degrees)
{
    return MathFI::sin_d(degrees + FInt(90));
}

///Cosine with radians as input.
FInt MathFI::cos_r(const FInt radians)
{
    return MathFI::sin_r(radians + (MathFI::PI >> 1));
}

///Tangent with degrees as input.
FInt MathFI::tan_d(const FInt degrees)
{
    return MathFI::sin_d(degrees) / MathFI::cos_d(degrees);
}

///Tangent with degrees as input.
FInt MathFI::tan_r(const FInt radians)
{
    return MathFI::sin_r(radians) / MathFI::cos_r(radians);
}

// Adapted from https://gitlab.com/snopek-games/sg-physics-2d/
// which is also an adaptation of Mike's code, hence why 2 copyrights.
// Copyright 2019 Mike Lankamp, Copyright (c) 2021-2022 David Snopek
// Licenses: both MIT
// Arctangent that returns in radians.
FInt MathFI::atan_r(const FInt p)
{
    int64_t flip = -1LL * (int64_t)(p.raw_value < 0);
    flip |= 1;

    
    if (p > FInt::ONE)
    {
        return MathFI::PI_DIV_2 - MathFI::atan_sanitized(4294967296 / ((p.raw_value << 4) * flip));
    }

    return FInt{MathFI::atan_sanitized(p.raw_value << 4) * flip};
}

FInt MathFI::atan_d(const FInt p)
{
    MathFI::radians_to_degrees(MathFI::atan_r(p));
}

// Adapted from https://gitlab.com/snopek-games/sg-physics-2d/
// which is also an adaptation of Mike's code, hence why 2 copyrights.
// Copyright 2019 Mike Lankamp, Copyright (c) 2021-2022 David Snopek
// Licenses: both MIT
// 2-argument arctangent that returns in radians.
// Optimized to the point of being unreadable.
FInt MathFI::atan2_r(const FInt in, const FInt inX) {
    bool in_zero = in == FInt::ZERO;
    bool inx_zero = inX == FInt::ZERO;

    if (in_zero | inx_zero)
    {
        bool inx_lzero = inX < FInt::ZERO;
        bool in_leqzero = in < FInt::ZERO | in_zero;
        int64_t flip_pi_div2 = (in_leqzero * -1LL) | 1LL;

        return FInt{
            (MathFI::PI.raw_value * (in_zero & inx_lzero))
            | ((MathFI::PI_DIV_2.raw_value * flip_pi_div2) * !in_zero)
        };
    }

    int64_t flip_pi = ((in < FInt::ZERO) * -1LL) | 1LL;

	FInt adiv_ret = MathFI::atan_div(in, inX);

    FInt ret = adiv_ret + (MathFI::PI * flip_pi * (inX < FInt::ZERO));

	return ret;
}

FInt MathFI::atan2_d(const FInt in, const FInt inX)
{
    MathFI::radians_to_degrees(MathFI::atan2_r(in, inX));
}

// Adapted from https://gitlab.com/snopek-games/sg-physics-2d/
// which is also an adaptation of Mike's code, hence why 2 copyrights.
// Copyright 2019 Mike Lankamp, Copyright (c) 2021-2022 David Snopek
// Licenses: both MIT
// Optimized to the point of being unreadable.
FInt MathFI::atan_div(const FInt p_y, const FInt p_x) {
	ERR_FAIL_COND_V(p_x == FInt::ZERO, FInt::ZERO);

    bool y_lzero = p_y < FInt::ZERO;
    bool x_lzero = p_x < FInt::ZERO;
    bool x_y_discronguous = (y_lzero | x_lzero) & (y_lzero ^ x_lzero);

    // f is for 'final'
    // Flips p_coordinate according to the condition.
    int64_t f_y = p_y.raw_value * ((-1LL * (int64_t)(y_lzero)) | 1);
    int64_t f_x = p_x.raw_value * ((-1LL * (int64_t)(x_lzero)) | 1);

    int64_t flip_result = (-1LL * (int64_t)(x_y_discronguous)) | 1LL;

    //I didn't know what to call it.
    bool discombobulate = p_y > p_x;
    int64_t y_first = (int64_t)(discombobulate ^ true);
    int64_t x_first = (int64_t)discombobulate;

    int64_t f_1 = (f_y * y_first) | (f_x * x_first);
    int64_t f_2 = (f_y * x_first) | (f_x * y_first);

    FInt atan_sani = FInt{MathFI::atan_sanitized((f_1 << 20) / (f_2 << 4))};

    FInt result = (MathFI::PI_DIV_2 * x_first) + (atan_sani * ((-1LL * x_first) | 1));

	return result * flip_result;
}


// Adapted from https://gitlab.com/snopek-games/sg-physics-2d/
// which is also an adaptation of Mike's code, hence why 2 copyrights.
// Copyright 2019 Mike Lankamp, Copyright (c) 2021-2022 David Snopek
// Licenses: both MIT
// 0 to 65536 ONLY
int64_t MathFI::atan_sanitized(const int64_t p_x) {
	ERR_FAIL_COND_V(p_x < 0 || p_x > 65536, 0);

	static const int64_t a = 5089;   //  0.0776509570923569
	static const int64_t b = -18837; // -0.2874298095703125
	static const int64_t c = 65220;  //  0.999755859375 (PI_DIV_4 - A - B)

	int64_t xx = (p_x * p_x) >> 16;
	return ((((((a * xx) >> 16) + b) * xx) >> 16) + c) * p_x >> (16 + 4);
}

//Sine for degrees.
//Argument must be 0 to 360 ONLY, highly unsafe otherwise.
FInt MathFI::fp_sin_d( const FInt degrees )
{
    //Another conversion formula by FBM
    //Converts from 0-360*4096 range to 0-32767
    int64_t semiConverted = degrees.raw_value / 45;
    int16_t i = (int16_t)semiConverted;

    return FInt{MathFI::fp_sin(i)};
}

//Sine for radians.
//Argument must be 0 to PI_X2 ONLY, highly unsafe otherwise.
FInt MathFI::fp_sin_r( const FInt radians )
{
    //Demonic asspull formula by FBM.
    //Converts from 0-Q12_PI_X2 range to 0-32768.
    //((Q12_PI_X2 * 1.5) << 13) / 9651 = 32768
    int64_t semiConverted = ((radians.raw_value << 13) + (radians.raw_value << 12)) / 9651;
    int16_t i = (int16_t)semiConverted;

    return FInt{MathFI::fp_sin(i)};
}

//https://www.nullhardware.com/blog/fixed-point-sine-and-cosine-for-embedded-systems/
//Optimized sine, uses the max value of short as a representation of PI*2.
//NEVER CALL unless you know what you're doing.
FInt MathFI::fp_sin(const int16_t value)
{
    int16_t i = value;

    /* Convert (signed) input to a value between 0 and 8192. (8192 is pi/2, which is the region of the curve fit). */
    /* ------------------------------------------------------------------- */
    i <<= 1;
    bool c = i<0; //set carry for output pos/neg

    if(i == (i| 0x4000)) // flip input value to corresponding value in range [0..8192)
        i = (int16_t)(32768 - i);
    i = (int16_t)((i & 0x7FFF) >> 1);
    /* ------------------------------------------------------------------- */

    /* The following section implements the formula:
    = y * 2^-n * ( A1 - 2^(q-p)* y * 2^-n * y * 2^-n * [B1 - 2^-r * y * 2^-n * C1 * y]) * 2^(a-q)
    Where the constants are defined as follows:
    */
    uint32_t iu = (uint32_t) i;
    enum {A1=3370945099UL, B1=2746362156UL, C1=292421UL};
    enum {n=13, p=32, q=31, r=3, a=12};

    uint32_t y = (uint32_t)((C1*(iu))>>n);
    y = (uint32_t)(B1 - ((iu*y)>>r));
    y = (uint32_t)(iu * (y>>n));
    y = (uint32_t)(iu * (y>>n));
    y = (uint32_t)(A1 - (y>>(p-q)));
    y = (uint32_t)(iu * (y>>n));
    y = (uint32_t)((y+(1UL<<(q-a-1)))>>(q-a)); // Rounding

    FInt to_return;
    to_return.raw_value = c ? -y : y;

    return to_return;
}
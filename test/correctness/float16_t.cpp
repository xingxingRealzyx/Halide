#include "Halide.h"

using namespace Halide;

int main(int argc, char **argv) {
    Var x;

    Buffer<float16_t> in1 = lambda(x, cast<float16_t>(-0.5f) + cast<float16_t>(x) / (128)).realize(128);
    Buffer<bfloat16_t> in2 = lambda(x, cast<bfloat16_t>(-0.5f) + cast<bfloat16_t>(x) / (128)).realize(128);

    // Check the Halide-side float 16 conversion math matches the C++-side math.
    in1.for_each_element([&](int i) {
            float16_t correct = Halide::float16_t(-0.5f) + Halide::float16_t(i) / Halide::float16_t(128.0f);
            if (in1(i) != correct) {
                printf("in1(%d) = %f instead of %f\n", i, float(in2(i)), float(correct));
                abort();
            }
        });

    in2.for_each_element([&](int i) {
            bfloat16_t correct = Halide::bfloat16_t(-0.5f) + Halide::bfloat16_t(i) / Halide::bfloat16_t(128.0f);
            if (in2(i) != correct) {
                printf("in2(%d) = %f instead of %f\n", i, float(in2(i)), float(correct));
                abort();
            }
        });

    // Check some basic math works on float16. More math is tested in
    // correctness_vector_math.
    Func wrap1, wrap2;
    wrap1(x) = in1(x);
    wrap2(x) = in2(x);

    Func f;
    f(x) = abs(sqrt(abs(wrap1(x) * 4.0f)) - sqrt(abs(wrap2(x))) * 2.0f);

    f.compute_root().vectorize(x, 16);
    wrap1.compute_at(f, x).vectorize(x);
    wrap2.compute_at(f, x).vectorize(x);

    RDom r(0, 128);
    Func g;
    g() = maximum(cast<double>(f(r)));

    double d = evaluate<double>(g());
    if (d != 0) {
        printf("Should be zero: %f\n", d);
        return -1;
    }

    // Check scalar parameters
    {
        Param<float16_t> a;
        Param<bfloat16_t> b;
        a.set(float16_t(1.5f));
        b.set(bfloat16_t(2.75f));
        float result = evaluate<float>(cast<float>(a) + cast<float>(b));
        if (result != 4.25f) {
            printf("Incorrect result: %f != 4.25f\n", result);
            return 1;
        }
    }

    // Check scalar parameters work using a problematic case
    {
        Param<float16_t> a, b, c;
        a.set(float16_t(24.062500f));
        b.set(float16_t(30.187500f));
        c.set(float16_t(0));
        float16_t result = evaluate<float16_t>(lerp(a, b, c));
        if (float(result) != 24.062500f) {
            printf("Incorrect result: %f != 24.0625f\n", (float)result);
            return 1;
        }
    }

    {
        Param<bfloat16_t> a, b, c;
        a.set(bfloat16_t(24.5f));
        b.set(bfloat16_t(30.5f));
        c.set(bfloat16_t(0));
        bfloat16_t result = evaluate<bfloat16_t>(lerp(a, b, c));
        if (float(result) != 24.5f) {
            printf("Incorrect result: %f != 24.5f\n", (float)result);
            return 1;
        }
    }

    // Check that ties round towards a zero last bit on narrowing conversions
    {
        bfloat16_t start = bfloat16_t(37.2789f);
        for (uint16_t x = 0; x < 8; x++) {
            bfloat16_t a = bfloat16_t::make_from_bits(start.to_bits() + x);
            bfloat16_t b = bfloat16_t::make_from_bits(start.to_bits() + x + 1);
            bfloat16_t ab = bfloat16_t(((float)a + (float)b)/2);

            if (a > ab || ab > b) {
                printf("Misordered: %x %x %x\n", a.to_bits(), ab.to_bits(), b.to_bits());
            }

            bool ok = (((a.to_bits() & 1) && (ab == b)) ||
                       ((b.to_bits() & 1) && (ab == a)));

            if (!ok) {
                printf("Incorrect rounding: %x %x %x\n", a.to_bits(), ab.to_bits(), b.to_bits());
                return -1;
            }
        }
    }

    // Check that ties round towards a zero last bit on narrowing conversions
    {
        float16_t start = float16_t(37.2789f);
        for (uint16_t x = 0; x < 8; x++) {
            float16_t a = float16_t::make_from_bits(start.to_bits() + x);
            float16_t b = float16_t::make_from_bits(start.to_bits() + x + 1);
            float16_t ab = float16_t(((float)a + (float)b)/2);

            if (a > ab || ab > b) {
                printf("Misordered: %x %x %x\n", a.to_bits(), ab.to_bits(), b.to_bits());
            }

            bool ok = (((a.to_bits() & 1) && (ab == b)) ||
                       ((b.to_bits() & 1) && (ab == a)));

            if (!ok) {
                printf("Incorrect rounding: %x %x %x\n", a.to_bits(), ab.to_bits(), b.to_bits());
                return -1;
            }
        }
    }

    // Enable to read assembly generated by the conversion routines
    if (0) {
        Func src, to_f16, from_f16;

        src(x) = cast<float>(x);
        to_f16(x) = cast<float16_t>(src(x));
        from_f16(x) = cast<float>(to_f16(x));

        src.compute_root().vectorize(x, 8, TailStrategy::RoundUp);
        to_f16.compute_root().vectorize(x, 8, TailStrategy::RoundUp);
        from_f16.compute_root().vectorize(x, 8, TailStrategy::RoundUp);

        from_f16.compile_to_assembly("/dev/stdout", {}, Target("host-no_asserts-no_bounds_query-no_runtime-disable_llvm_loop_unroll-disable_llvm_loop_vectorize"));
    }

    printf("Success!\n");
    return 0;
}

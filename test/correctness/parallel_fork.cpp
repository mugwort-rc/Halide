#include <stdio.h>
#include "Halide.h"
#include "halide_benchmark.h"

#include <chrono>
#include <thread>
#include <atomic>

using namespace Halide;
using namespace Halide::Tools;

std::atomic<int32_t> call_count;

extern "C" int five_ms(int arg) {
  call_count++;
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  return arg;
}

namespace halide_externs {
HalideExtern_1(int, five_ms, int);
}

enum Schedule {
  Serial,
  Parallel,
  AsyncRoot,
  AsyncComputeAt,
};

Func make(Schedule schedule) {
    Var x, y, z;
    Func both, f, g;

    f(x, y) = halide_externs::five_ms(x + y);
    g(x, y) = halide_externs::five_ms(x - y);

    both(x, y, z) = select(z == 0, f(x, y), g(x, y));

    both.compute_root().bound(z, 0, 2);
    switch (schedule) {
    case Serial:
        f.compute_root();
	g.compute_root();
	break;
    case Parallel:
        both.parallel(z);
	f.compute_at(both, z);
	g.compute_at(both, z);
	break;
    case AsyncRoot:
	f.compute_root().async();
	g.compute_root().async();
	break;
    case AsyncComputeAt:
        both.parallel(z);
	f.compute_at(both, z).async();
	g.compute_at(both, z).async();
	break;
    }

    return both;
}

int main(int argc, char **argv) {
    Func both;
    Buffer<int32_t> im;
    int count;
    double time;

    call_count = 0;
    both  = make(Serial);
    im = both.realize(10, 10, 2);
    count = call_count;
    time = benchmark([&]() {
        both.realize(im);
    });
    printf("Serial time %f for %d calls.\n", time, count);
    fflush(stdout);
    
    call_count = 0;
    both = make(Parallel);
    im = both.realize(10, 10, 2);
    count = call_count;
    time = benchmark([&]() {
        both.realize(im);
    });
    printf("Parallel time %f for %d calls.\n", time, count);
    fflush(stdout);

    both = make(AsyncRoot);
    call_count = 0;
    im = both.realize(10, 10, 2);
    count = call_count;
    time = benchmark([&]() {
        both.realize(im);
    });
    printf("Async root time %f for %d calls.\n", time, count);
    fflush(stdout);

#if 0 // This hangs due to a miscompilation that is being worked on.
    both = make(AsyncComputeAt);
    both.compile_to_lowered_stmt("/tmp/async_compute_at.stmt", {}, Text);
    call_count = 0;
    im = both.realize(10, 10, 2);
    printf(stderr, "Done first run.\n");
    fflush(stdout);
    count = call_count;
    time = benchmark([&]() {
        both.realize(im);
    });
    printf("AsyncComputeAt time %f for %d calls.\n", time, count);
    fflush(stdout);
#endif

    printf("Success!\n");
    return 0;
}

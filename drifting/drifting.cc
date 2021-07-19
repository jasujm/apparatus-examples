// Simulates 2^24 1d random walks for 2^24 steps each. The walks are
// restricted to traverse positive axis only.
// Targeted for x64 architecture. I optimized it quite a bit by:
// - Aligning the buffer to 64 bytes to minimize different worker threads
//   accessing the same cache lines
// - Writing the loops in a way that allows vectorization
//   (compile with -O3 -march=native)
// - Minimizing the calls to RNG by using all bits from the result value in
//   one ugly hand-unrolled loop

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <future>
#include <iostream>
#include <memory>
#include <random>
#include <span>

namespace {

constexpr int N_WALKS = 1 << 24;
constexpr int N_STEPS = 1 << 24;
constexpr int N_WORKERS = 8;

static_assert(N_WALKS % N_WORKERS == 0);

constexpr int N_WALKS_PER_WORKER = N_WALKS / N_WORKERS;
constexpr int BITS_IN_RANDOM_VALUE = 8 * sizeof(std::mt19937_64::result_type);

static_assert(N_WALKS_PER_WORKER % BITS_IN_RANDOM_VALUE == 0);

using WalkSpan = std::span<std::int32_t>;

struct WorkerContext {
    WorkerContext(int seed, std::int32_t* walk) :
        rng {static_cast<std::mt19937_64::result_type>(seed)},
        walk {walk, N_WALKS_PER_WORKER}
    {
    }
    std::mt19937_64 rng;
    WalkSpan walk;
};

void walk(WorkerContext& context, int steps)
{
    for (int i = 0; i < N_WALKS_PER_WORKER; i += BITS_IN_RANDOM_VALUE) {
        for (int step = 0; step < steps; ++step) {
            const auto value = context.rng();
            std::array<std::int32_t, BITS_IN_RANDOM_VALUE> steps {
                static_cast<std::int32_t>((value & 0x0000000000000001ULL) << 1 ) - 1,
                static_cast<std::int32_t>((value & 0x0000000000000002ULL)      ) - 1,
                static_cast<std::int32_t>((value & 0x0000000000000004ULL) >> 1 ) - 1,
                static_cast<std::int32_t>((value & 0x0000000000000008ULL) >> 2 ) - 1,
                static_cast<std::int32_t>((value & 0x0000000000000010ULL) >> 3 ) - 1,
                static_cast<std::int32_t>((value & 0x0000000000000020ULL) >> 4 ) - 1,
                static_cast<std::int32_t>((value & 0x0000000000000040ULL) >> 5 ) - 1,
                static_cast<std::int32_t>((value & 0x0000000000000080ULL) >> 6 ) - 1,
                static_cast<std::int32_t>((value & 0x0000000000000100ULL) >> 7 ) - 1,
                static_cast<std::int32_t>((value & 0x0000000000000200ULL) >> 8 ) - 1,
                static_cast<std::int32_t>((value & 0x0000000000000400ULL) >> 9 ) - 1,
                static_cast<std::int32_t>((value & 0x0000000000000800ULL) >> 10) - 1,
                static_cast<std::int32_t>((value & 0x0000000000001000ULL) >> 11) - 1,
                static_cast<std::int32_t>((value & 0x0000000000002000ULL) >> 12) - 1,
                static_cast<std::int32_t>((value & 0x0000000000004000ULL) >> 13) - 1,
                static_cast<std::int32_t>((value & 0x0000000000008000ULL) >> 14) - 1,
                static_cast<std::int32_t>((value & 0x0000000000010000ULL) >> 15) - 1,
                static_cast<std::int32_t>((value & 0x0000000000020000ULL) >> 16) - 1,
                static_cast<std::int32_t>((value & 0x0000000000040000ULL) >> 17) - 1,
                static_cast<std::int32_t>((value & 0x0000000000080000ULL) >> 18) - 1,
                static_cast<std::int32_t>((value & 0x0000000000100000ULL) >> 19) - 1,
                static_cast<std::int32_t>((value & 0x0000000000200000ULL) >> 20) - 1,
                static_cast<std::int32_t>((value & 0x0000000000400000ULL) >> 21) - 1,
                static_cast<std::int32_t>((value & 0x0000000000800000ULL) >> 22) - 1,
                static_cast<std::int32_t>((value & 0x0000000001000000ULL) >> 23) - 1,
                static_cast<std::int32_t>((value & 0x0000000002000000ULL) >> 24) - 1,
                static_cast<std::int32_t>((value & 0x0000000004000000ULL) >> 25) - 1,
                static_cast<std::int32_t>((value & 0x0000000008000000ULL) >> 26) - 1,
                static_cast<std::int32_t>((value & 0x0000000010000000ULL) >> 27) - 1,
                static_cast<std::int32_t>((value & 0x0000000020000000ULL) >> 28) - 1,
                static_cast<std::int32_t>((value & 0x0000000040000000ULL) >> 29) - 1,
                static_cast<std::int32_t>((value & 0x0000000080000000ULL) >> 30) - 1,
                static_cast<std::int32_t>((value & 0x0000000100000000ULL) >> 31) - 1,
                static_cast<std::int32_t>((value & 0x0000000200000000ULL) >> 32) - 1,
                static_cast<std::int32_t>((value & 0x0000000400000000ULL) >> 33) - 1,
                static_cast<std::int32_t>((value & 0x0000000800000000ULL) >> 34) - 1,
                static_cast<std::int32_t>((value & 0x0000001000000000ULL) >> 35) - 1,
                static_cast<std::int32_t>((value & 0x0000002000000000ULL) >> 36) - 1,
                static_cast<std::int32_t>((value & 0x0000004000000000ULL) >> 37) - 1,
                static_cast<std::int32_t>((value & 0x0000008000000000ULL) >> 38) - 1,
                static_cast<std::int32_t>((value & 0x0000010000000000ULL) >> 39) - 1,
                static_cast<std::int32_t>((value & 0x0000020000000000ULL) >> 40) - 1,
                static_cast<std::int32_t>((value & 0x0000040000000000ULL) >> 41) - 1,
                static_cast<std::int32_t>((value & 0x0000080000000000ULL) >> 42) - 1,
                static_cast<std::int32_t>((value & 0x0000100000000000ULL) >> 43) - 1,
                static_cast<std::int32_t>((value & 0x0000200000000000ULL) >> 44) - 1,
                static_cast<std::int32_t>((value & 0x0000400000000000ULL) >> 45) - 1,
                static_cast<std::int32_t>((value & 0x0000800000000000ULL) >> 46) - 1,
                static_cast<std::int32_t>((value & 0x0001000000000000ULL) >> 47) - 1,
                static_cast<std::int32_t>((value & 0x0002000000000000ULL) >> 48) - 1,
                static_cast<std::int32_t>((value & 0x0004000000000000ULL) >> 49) - 1,
                static_cast<std::int32_t>((value & 0x0008000000000000ULL) >> 50) - 1,
                static_cast<std::int32_t>((value & 0x0010000000000000ULL) >> 51) - 1,
                static_cast<std::int32_t>((value & 0x0020000000000000ULL) >> 52) - 1,
                static_cast<std::int32_t>((value & 0x0040000000000000ULL) >> 53) - 1,
                static_cast<std::int32_t>((value & 0x0080000000000000ULL) >> 54) - 1,
                static_cast<std::int32_t>((value & 0x0100000000000000ULL) >> 55) - 1,
                static_cast<std::int32_t>((value & 0x0200000000000000ULL) >> 56) - 1,
                static_cast<std::int32_t>((value & 0x0400000000000000ULL) >> 57) - 1,
                static_cast<std::int32_t>((value & 0x0800000000000000ULL) >> 58) - 1,
                static_cast<std::int32_t>((value & 0x1000000000000000ULL) >> 59) - 1,
                static_cast<std::int32_t>((value & 0x2000000000000000ULL) >> 60) - 1,
                static_cast<std::int32_t>((value & 0x4000000000000000ULL) >> 61) - 1,
                static_cast<std::int32_t>((value & 0x8000000000000000ULL) >> 62) - 1,
            };
            for (int j = 0; j < BITS_IN_RANDOM_VALUE; ++j) {
                context.walk[i+j] = std::max(context.walk[i+j] + steps[j], 0);
            }
        }
    }
}

}

int main()
{
    const auto walks = static_cast<std::int32_t*>(std::aligned_alloc(64, N_WALKS * sizeof(std::int32_t)));
    std::memset(walks, 0, N_WALKS * sizeof(std::int32_t));
    std::vector<std::future<void>> tasks(N_WORKERS);
    std::vector<WorkerContext> contexts;
    for (int worker = 0; worker < N_WORKERS; ++worker) {
        contexts.emplace_back(worker, &walks[worker * N_WALKS_PER_WORKER]);
    }
    for (int steps = 1; steps < N_STEPS; steps *= 2) {
        for (int worker = 0; worker < N_WORKERS; ++worker) {
            tasks[worker] = std::async(walk, std::ref(contexts[worker]), steps);
        }
        for (int worker = 0; worker < N_WORKERS; ++worker) {
            tasks[worker].wait();
        }
        std::cerr << "Ran another " << steps << " steps\n";
        std::cout << walks[0];
        for (int walk = 1; walk < N_WALKS; ++walk) {
            std::cout << " " << walks[walk];
        }
        std::cout << "\n";
    }
    free(walks);
    return 0;
}

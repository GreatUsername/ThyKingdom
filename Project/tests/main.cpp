// The tests for the walking skeleton.
//
// A single-file assertion harness rather than doctest or Catch2: the framework
// choice is still open (note 08, question 2), and pulling one in now would make it
// by accident. Swapping this for doctest is a one-file change.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>

#include "tk/hash.h"
#include "tk/rng.h"
#include "tk/scenario.h"
#include "tk/time.h"
#include "tk/world.h"

#ifndef TK_SCENARIO_DIR
#define TK_SCENARIO_DIR "scenarios"
#endif

namespace {

int g_checks = 0;
int g_failures = 0;

void report(bool ok, const char* file, int line, const char* what,
            long long a, long long b, bool twoValues) {
    ++g_checks;
    if (ok) {
        return;
    }
    ++g_failures;
    if (twoValues) {
        std::printf("FAIL %s:%d  %s  (%lld vs %lld)\n", file, line, what, a, b);
    } else {
        std::printf("FAIL %s:%d  %s\n", file, line, what);
    }
}

}  // namespace

#define CHECK(cond) report((cond), __FILE__, __LINE__, #cond, 0, 0, false)
#define CHECK_EQ(a, b) \
    report((static_cast<long long>(a) == static_cast<long long>(b)), __FILE__, __LINE__, \
           #a " == " #b, static_cast<long long>(a), static_cast<long long>(b), true)

namespace {

// --- calendar -------------------------------------------------------------

void testCalendar() {
    CHECK_EQ(tk::fromCalendar(tk::CalendarDate{1, 1, 1, 0, 0, 0, 0}), 0);
    CHECK_EQ(tk::kUsPerDay, 86400000000LL);
    CHECK_EQ(tk::kUsPerYear, 31104000000000LL);

    // Month boundary: 30 days is the whole of month 1, and 12 * 30 = 360 a year.
    const tk::CalendarDate month = tk::toCalendar(30 * tk::kUsPerDay);
    CHECK_EQ(month.month, 2);
    CHECK_EQ(month.day, 1);

    // Last instant of year 1.
    const std::int64_t last = tk::kUsPerYear - 1;
    const tk::CalendarDate end = tk::toCalendar(last);
    CHECK_EQ(end.year, 1);
    CHECK_EQ(end.month, 12);
    CHECK_EQ(end.day, 30);
    CHECK_EQ(end.hour, 23);
    CHECK_EQ(end.minute, 59);
    CHECK_EQ(end.second, 59);
    CHECK_EQ(end.microsecond, 999999);
    CHECK_EQ(tk::fromCalendar(end), last);

    // First instant of year 2.
    const tk::CalendarDate next = tk::toCalendar(tk::kUsPerYear);
    CHECK_EQ(next.year, 2);
    CHECK_EQ(next.month, 1);
    CHECK_EQ(next.day, 1);
    CHECK_EQ(next.hour, 0);
}

// --- clock ----------------------------------------------------------------

void testClock() {
    tk::World w;
    const std::int64_t dt = 1000;  // 1 ms
    for (int i = 0; i < 1000; ++i) {
        w.step(dt);
    }
    CHECK_EQ(w.now_us, 1000 * dt);
}

// --- accumulation is independent of how the span is split ------------------

std::int64_t grownOver(std::int64_t rate, std::int64_t total_us, std::int64_t dt_us) {
    tk::World w;
    w.crop_rate = rate;
    std::int64_t done = 0;
    while (done < total_us) {
        const std::int64_t step = std::min(dt_us, total_us - done);
        w.step(step);
        done += step;
    }
    return w.crop.whole;
}

void testAccumulationExactness() {
    const std::int64_t rate = 123457;  // micro-units per second
    const std::int64_t total = 7 * tk::kUsPerDay;
    const std::int64_t expected = rate * total / 1000000;

    CHECK_EQ(grownOver(rate, total, total), expected);
    CHECK_EQ(grownOver(rate, total, total / 7), expected);
    CHECK_EQ(grownOver(rate, total, total / 1000), expected);

    // Finest possible split, but over a span small enough to actually run: one
    // microsecond at a time for one second is a million steps, not six hundred
    // billion. The invariance claim does not depend on the span's size.
    const std::int64_t one_second = tk::kUsPerSecond;
    CHECK_EQ(grownOver(rate, one_second, 1), rate * one_second / 1000000);
}

// --- the headline test: the same span at any dt gives the same state -------

void testDtInvariance() {
    const std::int64_t total = 3 * tk::kUsPerDay;

    auto run = [total](std::int64_t dt) {
        tk::World w;
        w.seed = 42;
        w.crop_rate = 987654;
        w.rng.reseed(42, 1);
        std::int64_t done = 0;
        while (done < total) {
            const std::int64_t step = std::min(dt, total - done);
            w.step(step);
            done += step;
        }
        return w.hash();
    };

    const std::uint64_t once = run(total);
    CHECK_EQ(run(total / 2), once);
    CHECK_EQ(run(total / 96), once);
    CHECK_EQ(run(total / 86400), once);
    CHECK_EQ(run(1000000), once);  // one second at a time
}

void testDeterminism() {
    tk::World a;
    a.seed = 7;
    a.crop_rate = 500;
    a.rng.reseed(7, 1);
    tk::World b = a;
    for (int i = 0; i < 1000; ++i) {
        a.step(333);
        b.step(333);
    }
    CHECK_EQ(a.hash(), b.hash());
}

// --- the hash must actually cover the state -------------------------------

void testHashSensitivity() {
    const tk::World base;
    CHECK_EQ(base.hash(), tk::World().hash());

    tk::World t = base;
    t.now_us = 1;
    CHECK(base.hash() != t.hash());

    tk::World c = base;
    c.crop.whole = 1;
    CHECK(base.hash() != c.hash());

    tk::World k = base;
    k.crop.carry = 1;
    CHECK(base.hash() != k.hash());

    tk::World r = base;
    r.rng.reseed(7, 1);
    CHECK(base.hash() != r.hash());
}

// --- scenarios ------------------------------------------------------------

void testScenarioLoading() {
    const std::string path = std::string(TK_SCENARIO_DIR) + "/skeleton.scn";
    tk::Scenario sc;
    std::string error;
    const bool ok = tk::loadScenario(path, sc, error);
    if (!ok) {
        std::printf("FAIL cannot load %s: %s\n", path.c_str(), error.c_str());
    }
    CHECK(ok);
    if (!ok) {
        return;
    }
    CHECK_EQ(sc.steps, 86400);
    CHECK_EQ(sc.dt_us, 1000000);
    CHECK(sc.seed == 1);
}

void testScenarioRejectsUnknownKey() {
    const std::string path = "tmp_bad_scenario.scn";
    {
        std::FILE* f = std::fopen(path.c_str(), "wb");
        if (f == nullptr) {
            CHECK(false);
            return;
        }
        std::fputs("[run]\nsteps = 1\ndt_us = 1\nnonsense = 5\n", f);
        std::fclose(f);
    }
    tk::Scenario sc;
    std::string error;
    const bool ok = tk::loadScenario(path, sc, error);
    CHECK(!ok);
    std::remove(path.c_str());
}

void testScenarioToleratesBom() {
    const std::string path = "tmp_bom_scenario.scn";
    {
        std::FILE* f = std::fopen(path.c_str(), "wb");
        if (f == nullptr) {
            CHECK(false);
            return;
        }
        std::fputs("\xEF\xBB\xBF[run]\nsteps = 2\ndt_us = 5\n", f);
        std::fclose(f);
    }
    tk::Scenario sc;
    std::string error;
    const bool ok = tk::loadScenario(path, sc, error);
    if (!ok) {
        std::printf("FAIL bom scenario rejected: %s\n", error.c_str());
    }
    CHECK(ok);
    CHECK_EQ(sc.steps, 2);
    CHECK_EQ(sc.dt_us, 5);
    std::remove(path.c_str());
}

void testRngIsReproducible() {
    tk::Rng a(123, 1);
    tk::Rng b(123, 1);
    for (int i = 0; i < 16; ++i) {
        CHECK_EQ(a.next(), b.next());
    }
    tk::Rng c(123, 2);
    tk::Rng d(123, 1);
    CHECK(c.next() != d.next());
}

}  // namespace

int main() {
    testCalendar();
    testClock();
    testAccumulationExactness();
    testDtInvariance();
    testDeterminism();
    testHashSensitivity();
    testScenarioLoading();
    testScenarioRejectsUnknownKey();
    testScenarioToleratesBom();
    testRngIsReproducible();

    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}

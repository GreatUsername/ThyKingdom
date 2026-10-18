// The tests for the walking skeleton.
//
// A single-file assertion harness rather than doctest or Catch2: the framework
// choice is still open (note 08, question 2), and pulling one in now would make it
// by accident. Swapping this for doctest is a one-file change.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>

#include "core/clock.h"
#include "core/control_signal.h"
#include "core/crops.h"
#include "core/evaluator.h"
#include "core/hash.h"
#include "core/rng.h"
#include "config/settings.h"
#include "core/scenario.h"
#include "core/world.h"
#include "runtime/scheduler.h"

// A test translation unit pulling the core in unqualified is deliberate: it keeps
// the assertions readable. This is a .cpp, so nothing leaks into other files.
using namespace core;

static int g_checks = 0;
static int g_failures = 0;
static std::string g_scenario_dir = "scenarios";

static void report(bool ok, const char* file, int line, const char* what,
                   long long a, long long b, bool two_values) {
    ++g_checks;
    if (ok) {
        return;
    }
    ++g_failures;
    if (two_values) {
        std::printf("FAIL %s:%d  %s  (%lld vs %lld)\n", file, line, what, a, b);
    } else {
        std::printf("FAIL %s:%d  %s\n", file, line, what);
    }
}

#define CHECK(cond) report((cond), __FILE__, __LINE__, #cond, 0, 0, false)
// Each argument is evaluated exactly once. The earlier version evaluated it twice -
// once for the comparison, once for the failure message - which broke silently as
// soon as an argument had a side effect, such as consuming a due step.
#define CHECK_EQ(a, b) \
    do { \
        const long long check_a_value_ = static_cast<long long>(a); \
        const long long check_b_value_ = static_cast<long long>(b); \
        report((check_a_value_ == check_b_value_), __FILE__, __LINE__, \
               #a " == " #b, check_a_value_, check_b_value_, true); \
    } while (false)

// A run with just the crop subsystem, for tests that need a specific rate.
static std::unique_ptr<IEvaluator> evaluatorWith(std::int64_t crop_rate,
                                                 std::uint64_t seed = 0) {
    Scenario sc;
    sc.seed = seed;
    sc.crop_rate = crop_rate;
    return buildEvaluator(sc);
}

static const CropSystem* crops(const World& w) {
    for (const auto& system : w.systems()) {
        if (const auto* c = dynamic_cast<const CropSystem*>(system.get())) {
            return c;
        }
    }
    return nullptr;
}

static const CropSystem* crops(const IEvaluator& ev) { return crops(ev.world()); }

// --- calendar -------------------------------------------------------------

static void testCalendar() {
    CHECK_EQ(fromCalendar(CalendarDate{1, 1, 1, 0, 0, 0, 0}), 0);
    CHECK_EQ(kUsPerDay, 86400000000LL);
    CHECK_EQ(kUsPerYear, 31104000000000LL);

    // Month boundary: 30 days is the whole of month 1, and 12 * 30 = 360 a year.
    const CalendarDate month = toCalendar(30 * kUsPerDay);
    CHECK_EQ(month.month, 2);
    CHECK_EQ(month.day, 1);

    // Last instant of year 1.
    const std::int64_t last = kUsPerYear - 1;
    const CalendarDate end = toCalendar(last);
    CHECK_EQ(end.year, 1);
    CHECK_EQ(end.month, 12);
    CHECK_EQ(end.day, 30);
    CHECK_EQ(end.hour, 23);
    CHECK_EQ(end.minute, 59);
    CHECK_EQ(end.second, 59);
    CHECK_EQ(end.microsecond, 999999);
    CHECK_EQ(fromCalendar(end), last);

    // First instant of year 2.
    const CalendarDate next = toCalendar(kUsPerYear);
    CHECK_EQ(next.year, 2);
    CHECK_EQ(next.month, 1);
    CHECK_EQ(next.day, 1);
    CHECK_EQ(next.hour, 0);
}

// --- the evaluator owns time ----------------------------------------------

static void testClock() {
    std::unique_ptr<IEvaluator> ev = evaluatorWith(0);
    const std::int64_t dt = 1000;  // 1 ms
    for (int i = 0; i < 1000; ++i) {
        ev->makeStep(dt);
    }
    CHECK_EQ(ev->clock().now_us(), 1000 * dt);
}

// Signals land on a boundary, not in the middle of a step.
static void testSignalsApplyOnBoundary() {
    std::unique_ptr<IEvaluator> ev = evaluatorWith(0);
    ev->pushSignal(ControlSignal{SignalKind::InterruptAcceleration, 0});
    CHECK_EQ(ev->pendingSignals(), 1u);
    ev->makeStep(1000);
    CHECK_EQ(ev->pendingSignals(), 0u);
}

// --- acceleration mode ----------------------------------------------------

static void testAccelerationMode() {
    std::unique_ptr<IEvaluator> ev = evaluatorWith(0);
    CHECK(ev->mode() == Mode::Normal);

    // The run is put into acceleration by a control signal, not by the caller
    // flipping a flag.
    ev->pushSignal(ControlSignal{SignalKind::BeginAcceleration, 1000});
    ev->makeStep(400);
    CHECK(ev->mode() == Mode::Accelerating);
    CHECK_EQ(ev->accelerationRemaining_us(), 600);

    // The span is exhausted: acceleration ends and a new turn begins (OD-14).
    ev->makeStep(600);
    CHECK(ev->mode() == Mode::Normal);
    CHECK_EQ(ev->accelerationRemaining_us(), 0);
    CHECK_EQ(ev->clock().now_us(), 1000);

    // An interrupt ends it early, and also on a boundary.
    ev->pushSignal(ControlSignal{SignalKind::BeginAcceleration, 100000});
    ev->makeStep(10);
    CHECK(ev->mode() == Mode::Accelerating);
    ev->pushSignal(ControlSignal{SignalKind::InterruptAcceleration, 0});
    CHECK(ev->mode() == Mode::Accelerating);  // not yet: the signal is pending
    ev->makeStep(10);
    CHECK(ev->mode() == Mode::Normal);
    CHECK_EQ(ev->accelerationRemaining_us(), 0);
}

// --- accumulation is independent of how the span is split ------------------

static std::int64_t grownOver(std::int64_t rate, std::int64_t total_us, std::int64_t dt_us) {
    std::unique_ptr<IEvaluator> ev = evaluatorWith(rate);
    std::int64_t done = 0;
    while (done < total_us) {
        const std::int64_t step = std::min(dt_us, total_us - done);
        ev->makeStep(step);
        done += step;
    }
    const CropSystem* c = crops(*ev);
    return c != nullptr ? c->grown() : -1;
}

static void testAccumulationExactness() {
    const std::int64_t rate = 123457;  // micro-units per second
    const std::int64_t total = 7 * kUsPerDay;
    const std::int64_t expected = rate * total / 1000000;

    CHECK_EQ(grownOver(rate, total, total), expected);
    CHECK_EQ(grownOver(rate, total, total / 7), expected);
    CHECK_EQ(grownOver(rate, total, total / 1000), expected);

    // Finest possible split, but over a span small enough to actually run: one
    // microsecond at a time for one second is a million steps, not six hundred
    // billion. The invariance claim does not depend on the span's size.
    const std::int64_t one_second = kUsPerSecond;
    CHECK_EQ(grownOver(rate, one_second, 1), rate * one_second / 1000000);
}

// --- the headline test: the same span at any dt gives the same state -------

static void testDtInvariance() {
    const std::int64_t total = 3 * kUsPerDay;

    auto run = [total](std::int64_t dt) {
        std::unique_ptr<IEvaluator> ev = evaluatorWith(987654, 42);
        std::int64_t done = 0;
        while (done < total) {
            const std::int64_t step = std::min(dt, total - done);
            ev->makeStep(step);
            done += step;
        }
        return ev->hash();
    };

    const std::uint64_t once = run(total);
    CHECK_EQ(run(total / 2), once);
    CHECK_EQ(run(total / 96), once);
    CHECK_EQ(run(total / 86400), once);
    CHECK_EQ(run(1000000), once);  // one second at a time
}

static void testDeterminism() {
    std::unique_ptr<IEvaluator> a = evaluatorWith(500, 7);
    std::unique_ptr<IEvaluator> b = evaluatorWith(500, 7);
    for (int i = 0; i < 1000; ++i) {
        a->makeStep(333);
        b->makeStep(333);
    }
    CHECK_EQ(a->hash(), b->hash());
}

// --- the hash must actually cover the state -------------------------------

static void testHashSensitivity() {
    CHECK_EQ(evaluatorWith(0, 0)->hash(), evaluatorWith(0, 0)->hash());

    // The clock is covered.
    std::unique_ptr<IEvaluator> timed = evaluatorWith(0, 0);
    timed->makeStep(1);
    CHECK(evaluatorWith(0, 0)->hash() != timed->hash());

    // The whole part of growth is covered: same clock, same carry, different
    // whole.
    std::unique_ptr<IEvaluator> grown = evaluatorWith(1000000, 0);
    grown->makeStep(1000000);  // one whole unit
    std::unique_ptr<IEvaluator> still = evaluatorWith(0, 0);
    still->makeStep(1000000);  // no growth at all
    CHECK(grown->hash() != still->hash());

    // The carry is covered on its own: same clock, same whole (zero), different
    // fraction.
    std::unique_ptr<IEvaluator> carry_one = evaluatorWith(1, 0);
    carry_one->makeStep(1);
    std::unique_ptr<IEvaluator> carry_two = evaluatorWith(2, 0);
    carry_two->makeStep(1);
    CHECK(carry_one->hash() != carry_two->hash());

    // The world RNG is covered.
    CHECK(evaluatorWith(0, 1)->hash() != evaluatorWith(0, 2)->hash());
}

// --- settings live outside the simulation ---------------------------------
//
// There is no longer a test asserting that a setting cannot reach the world,
// because the compiler now guarantees it: Settings lives in the config library,
// which core does not depend on. A file in core cannot name the type. That is a
// stronger guarantee than a test, and it cannot rot.

static void testSettingsRoundTrip() {
    config::Settings original;
    original.brightness = 35;
    original.fullscreen = true;
    original.master_volume = 55;
    original.key_pause = "P";
    original.key_accelerate = "F5";
    original.key_end_turn = "E";

    const std::string path = "tmp_settings.ini";
    std::string error;
    CHECK(config::saveSettings(path, original, error));

    config::Settings loaded;
    CHECK(config::loadSettings(path, loaded, error));
    CHECK(loaded == original);
    std::remove(path.c_str());
}

static void testSettingsMissingFileIsDefaults() {
    config::Settings loaded;
    loaded.brightness = 7;  // prove it is overwritten only if the file exists
    std::string error;
    // A missing file is "no preferences saved yet", not a failure.
    CHECK(config::loadSettings("tmp_does_not_exist_settings.ini", loaded, error));
    CHECK_EQ(loaded.brightness, 7);  // untouched
    config::Settings defaults;
    CHECK_EQ(defaults.brightness, 100);
    CHECK_EQ(defaults.master_volume, 80);
}

static void testSettingsRejectsUnknownKey() {
    const std::string path = "tmp_bad_settings.ini";
    {
        std::FILE* f = std::fopen(path.c_str(), "wb");
        if (f == nullptr) {
            CHECK(false);
            return;
        }
        std::fputs("[graphics]\nnonsense = 5\n", f);
        std::fclose(f);
    }
    config::Settings loaded;
    std::string error;
    CHECK(!config::loadSettings(path, loaded, error));
    std::remove(path.c_str());
}

// --- scenarios ------------------------------------------------------------

static void testScenarioLoading() {
    const std::string path = g_scenario_dir + "/skeleton.scn";
    Scenario sc;
    std::string error;
    const bool ok = loadScenario(path, sc, error);
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

static void testScenarioRejectsUnknownKey() {
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
    Scenario sc;
    std::string error;
    const bool ok = loadScenario(path, sc, error);
    CHECK(!ok);
    std::remove(path.c_str());
}

static void testScenarioToleratesBom() {
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
    Scenario sc;
    std::string error;
    const bool ok = loadScenario(path, sc, error);
    if (!ok) {
        std::printf("FAIL bom scenario rejected: %s\n", error.c_str());
    }
    CHECK(ok);
    CHECK_EQ(sc.steps, 2);
    CHECK_EQ(sc.dt_us, 5);
    std::remove(path.c_str());
}

static void testRngIsReproducible() {
    Rng a(123, 1);
    Rng b(123, 1);
    for (int i = 0; i < 16; ++i) {
        CHECK_EQ(a.next(), b.next());
    }
    Rng c(123, 2);
    Rng d(123, 1);
    CHECK(c.next() != d.next());
}

// --- pacing: the step rate is the simulation's, not the display's ---------

static void testPacingIgnoresChunking() {
    // One minute of real time, at dt = 20 ms, delivered in three different chunk
    // patterns. The total number of steps must be identical: it depends on elapsed
    // time and dt, not on how often the caller woke up.
    const std::int64_t dt = 20000;
    const std::int64_t total = 60 * 1000000;

    auto run = [dt, total](std::int64_t chunk) {
        runtime::Scheduler scheduler(dt);
        std::int64_t elapsed = 0;
        std::int64_t steps = 0;
        while (elapsed < total) {
            const std::int64_t slice = std::min(chunk, total - elapsed);
            scheduler.addElapsed(slice);
            elapsed += slice;
            steps += scheduler.takeDueSteps(100000);
        }
        return steps;
    };

    const std::int64_t even = run(dt);              // exactly one step per wake-up
    CHECK_EQ(even, total / dt);
    CHECK_EQ(run(6944), even);                      // ~144 Hz wake-ups
    CHECK_EQ(run(16667), even);                     // ~60 Hz wake-ups
    CHECK_EQ(run(1000), even);                      // 1000 Hz wake-ups
    CHECK_EQ(run(total), even);                     // one single wake-up
}

static void testPacingKeepsRemainder() {
    runtime::Scheduler scheduler(20000);
    // Wake up 144 times for one second's worth of real time.
    for (int i = 0; i < 144; ++i) {
        scheduler.addElapsed(6944);
    }
    const std::int64_t steps = scheduler.takeDueSteps(1000);
    CHECK_EQ(steps, 49);              // 999 936 us / 20 000 = 49 whole steps
    CHECK_EQ(scheduler.pending_us(), 19936);  // nothing lost

    // One more wake-up completes the fiftieth step.
    scheduler.addElapsed(6944);
    CHECK_EQ(scheduler.takeDueSteps(1000), 1);
    CHECK_EQ(scheduler.pending_us(), 6880);
}

static void testPacingCapsLongStall() {
    runtime::Scheduler scheduler(20000);
    scheduler.addElapsed(10 * 1000000);  // ten seconds of stall
    const std::int64_t steps = scheduler.takeDueSteps(3);
    CHECK_EQ(steps, 3);
    // The rest is dropped, not queued: the game falls behind real time rather than
    // spending longer catching up than it stalled.
    CHECK_EQ(scheduler.pending_us(), 0);
}

// --- the generating mechanism --------------------------------------------

static void testBuildEvaluator() {
    Scenario sc;
    sc.seed = 99;
    sc.crop_rate = 250;
    std::unique_ptr<IEvaluator> ev = buildEvaluator(sc);

    CHECK_EQ(ev->world().systems().size(), 1u);
    CHECK(crops(*ev) != nullptr);
    if (crops(*ev) != nullptr) {
        CHECK_EQ(crops(*ev)->grown(), 0);
    }
    CHECK_EQ(ev->clock().now_us(), 0);
    CHECK(ev->mode() == Mode::Normal);

    // The seed reaches the world RNG, which is what makes two runs of the same
    // scenario identical and two different seeds differ.
    CHECK_EQ(buildEvaluator(sc)->hash(), ev->hash());
    sc.seed = 100;
    CHECK(buildEvaluator(sc)->hash() != ev->hash());
}

int main(int argc, char** argv) {
    if (argc > 1) {
        g_scenario_dir = argv[1];
    }

    testCalendar();
    testClock();
    testSignalsApplyOnBoundary();
    testAccelerationMode();
    testAccumulationExactness();
    testDtInvariance();
    testDeterminism();
    testHashSensitivity();
    testSettingsRoundTrip();
    testSettingsMissingFileIsDefaults();
    testSettingsRejectsUnknownKey();
    testScenarioLoading();
    testScenarioRejectsUnknownKey();
    testScenarioToleratesBom();
    testRngIsReproducible();
    testBuildEvaluator();
    testPacingIgnoresChunking();
    testPacingKeepsRemainder();
    testPacingCapsLongStall();

    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}

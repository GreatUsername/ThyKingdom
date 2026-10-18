// The tests for the walking skeleton.
//
// A single-file assertion harness rather than doctest or Catch2: the framework
// choice is still open (note 08, question 2), and pulling one in now would make it
// by accident. Swapping this for doctest is a one-file change.
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

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
#include "runtime/time_source.h"
#include "runtime/timer.h"

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

// --- the timer: late binding, pacing, and acceleration --------------------

// Synthetic time instead of sleeps. What we want to assert is arithmetic, not the
// scheduler of the operating system.
class FakeTimeSource final : public runtime::TimeSource {
public:
    std::int64_t now_us() const override { return m_now_us; }
    void sleep_us(std::int64_t us) override {
        if (us > 0) {
            m_now_us += us;
            m_slept_us += us;
        }
    }
    void advance(std::int64_t us) { m_now_us += us; }
    std::int64_t slept_us() const { return m_slept_us; }

private:
    std::int64_t m_now_us = 0;
    std::int64_t m_slept_us = 0;
};

static void testTimerNeedsAttaching() {
    FakeTimeSource time;
    runtime::Timer timer(runtime::Scheduler(20000), time);

    time.advance(100000);  // five steps' worth of real time went by
    CHECK_EQ(timer.poll(100), 0);  // nobody to step yet
    CHECK(!timer.attached());

    std::unique_ptr<IEvaluator> ev = evaluatorWith(0);
    timer.attach(ev.get());
    CHECK(timer.attached());

    // Time that passed before attaching is not owed to the evaluator.
    CHECK_EQ(timer.poll(100), 0);
    CHECK_EQ(ev->clock().now_us(), 0);
}

static void testTimerRunsDueSteps() {
    FakeTimeSource time;
    runtime::Timer timer(runtime::Scheduler(20000), time);
    std::unique_ptr<IEvaluator> ev = evaluatorWith(0);
    timer.attach(ev.get());

    time.advance(20000);
    CHECK_EQ(timer.poll(100), 1);
    CHECK_EQ(ev->clock().now_us(), 20000);
    CHECK_EQ(ev->stepsTaken(), 1);

    time.advance(30000);  // one and a half steps
    CHECK_EQ(timer.poll(100), 1);
    CHECK_EQ(ev->clock().now_us(), 40000);
    CHECK_EQ(timer.scheduler().pending_us(), 10000);
}

static void testTimerWaitsForTheNextStep() {
    FakeTimeSource time;
    runtime::Timer timer(runtime::Scheduler(20000), time);
    std::unique_ptr<IEvaluator> ev = evaluatorWith(0);
    timer.attach(ev.get());

    timer.waitForNextStep();  // nothing has elapsed yet: sleeps the whole step
    CHECK_EQ(time.slept_us(), 20000);

    time.advance(5000);
    timer.poll(100);          // not enough for a step; 5000 us sits in the remainder
    timer.waitForNextStep();  // so only 15000 more is needed
    CHECK_EQ(time.slept_us(), 35000);
}

static void testTimerDoesNotPaceAcceleration() {
    FakeTimeSource time;
    runtime::Timer timer(runtime::Scheduler(20000), time);
    std::unique_ptr<IEvaluator> ev = evaluatorWith(0);
    timer.attach(ev.get());

    ev->pushSignal(ControlSignal{SignalKind::BeginAcceleration, 1000000});
    time.advance(20000);
    CHECK_EQ(timer.poll(100), 1);
    CHECK(ev->mode() == Mode::Accelerating);

    // Acceleration is not paced by real time: the timer must not sleep.
    const std::int64_t before = time.slept_us();
    timer.waitForNextStep();
    CHECK_EQ(time.slept_us(), before);
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

// --- timing ---------------------------------------------------------------
//
// Wall-clock numbers are reported, never asserted. A test that fails when the
// machine is busy teaches people to ignore failures; a table that is printed on
// every run shows a regression the moment it appears.

static std::int64_t now_us() {
    using Clock = std::chrono::steady_clock;
    return std::chrono::duration_cast<std::chrono::microseconds>(
               Clock::now().time_since_epoch())
        .count();
}

struct Timing {
    const char* name;
    std::int64_t us;
};

struct TestEntry {
    const char* name;
    void (*fn)();
};

static const TestEntry kTests[] = {
    {"calendar", testCalendar},
    {"clock", testClock},
    {"signals apply on boundary", testSignalsApplyOnBoundary},
    {"acceleration mode", testAccelerationMode},
    {"accumulation exactness", testAccumulationExactness},
    {"dt invariance", testDtInvariance},
    {"determinism", testDeterminism},
    {"hash sensitivity", testHashSensitivity},
    {"settings round trip", testSettingsRoundTrip},
    {"settings missing file", testSettingsMissingFileIsDefaults},
    {"settings unknown key", testSettingsRejectsUnknownKey},
    {"scenario loading", testScenarioLoading},
    {"scenario unknown key", testScenarioRejectsUnknownKey},
    {"scenario BOM", testScenarioToleratesBom},
    {"rng reproducible", testRngIsReproducible},
    {"build evaluator", testBuildEvaluator},
    {"pacing ignores chunking", testPacingIgnoresChunking},
    {"pacing keeps remainder", testPacingKeepsRemainder},
    {"pacing caps stall", testPacingCapsLongStall},
    {"timer needs attaching", testTimerNeedsAttaching},
    {"timer runs due steps", testTimerRunsDueSteps},
    {"timer waits for next step", testTimerWaitsForTheNextStep},
    {"timer skips pacing", testTimerDoesNotPaceAcceleration},
};

// Throughput of the simulation itself, and what it means for the acceleration
// targets. OD-33 asks whether a skip can fit a watchable real time; this is the
// number that answers it, on this machine, today.
static void runBenchmark(std::int64_t steps) {
    const std::int64_t dt_us = 20000;  // 50 Hz, a rate that divides a second exactly

    // Warm-up: the first pass pays for page faults and cold caches.
    {
        std::unique_ptr<IEvaluator> warm = evaluatorWith(1000, 42);
        for (std::int64_t i = 0; i < steps / 10 + 1; ++i) {
            warm->makeStep(dt_us);
        }
    }

    std::unique_ptr<IEvaluator> ev = evaluatorWith(1000, 42);
    const std::int64_t start = now_us();
    for (std::int64_t i = 0; i < steps; ++i) {
        ev->makeStep(dt_us);
    }
    const std::int64_t elapsed = now_us() - start;

    const double seconds = elapsed > 0 ? static_cast<double>(elapsed) / 1e6 : 1e-9;
    const double per_second = static_cast<double>(steps) / seconds;

    std::printf("\nthroughput\n");
    std::printf("  %lld steps at dt %lld us, 1 system\n", static_cast<long long>(steps),
                static_cast<long long>(dt_us));
    std::printf("  wall     %.3f s\n", seconds);
    std::printf("  rate     %.0f steps/s\n", per_second);
    std::printf("  cost     %.4f us/step\n", seconds * 1e6 / static_cast<double>(steps));

    // Hashing is not paid every step, but a test harness pays it constantly and a
    // runner pays it at the end, so its cost is worth knowing separately.
    const int hash_calls = 1000000;
    std::uint64_t sink = 0;
    const std::int64_t h0 = now_us();
    for (int i = 0; i < hash_calls; ++i) {
        sink += ev->hash();
    }
    const std::int64_t h1 = now_us();
    std::printf("  hash     %.4f us/call (checksum %llu)\n",
                static_cast<double>(h1 - h0) / hash_calls,
                static_cast<unsigned long long>(sink));

    std::printf("\n  what this rate buys:\n");
    const std::int64_t spans_days[] = {7, 90};
    for (std::int64_t days : spans_days) {
        const std::int64_t game_us = days * 86400LL * 1000000LL;
        const std::int64_t need = game_us / dt_us;
        const double real_seconds = static_cast<double>(need) / per_second;
        std::printf("    %2lld-day skip: %11lld steps -> %7.1f s real\n",
                    static_cast<long long>(days), static_cast<long long>(need), real_seconds);
    }
    std::printf("    (OD-33 targets: a week in <=15 s, two or three months in <=30 s)\n");
}

int main(int argc, char** argv) {
    std::int64_t bench_steps = 0;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--bench" && i + 1 < argc) {
            bench_steps = std::strtoll(argv[++i], nullptr, 10);
        } else if (arg.rfind("--", 0) != 0) {
            g_scenario_dir = arg;
        }
    }

    const int count = static_cast<int>(sizeof(kTests) / sizeof(kTests[0]));
    std::vector<Timing> timings;
    timings.reserve(static_cast<std::size_t>(count));

    const std::int64_t all_start = now_us();
    for (int i = 0; i < count; ++i) {
        const std::int64_t t0 = now_us();
        kTests[i].fn();
        timings.push_back(Timing{kTests[i].name, now_us() - t0});
    }
    const std::int64_t all_us = now_us() - all_start;

    // Slowest first: the top of this table is where to look when the suite starts
    // dragging.
    std::vector<Timing> sorted = timings;
    std::sort(sorted.begin(), sorted.end(),
              [](const Timing& a, const Timing& b) { return a.us > b.us; });

    std::printf("\nper test (slowest first)\n");
    for (const Timing& t : sorted) {
        const double share = all_us > 0
                                 ? 100.0 * static_cast<double>(t.us) / static_cast<double>(all_us)
                                 : 0.0;
        std::printf("  %-30s %8.3f ms  %5.1f%%\n", t.name, static_cast<double>(t.us) / 1000.0,
                    share);
    }
    std::printf("  %-30s %8.3f ms\n", "total", static_cast<double>(all_us) / 1000.0);

    if (bench_steps > 0) {
        runBenchmark(bench_steps);
    }

    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}

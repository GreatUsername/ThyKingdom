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

#include "config/settings.h"
#include "core/accumulator.h"
#include "core/clock.h"
#include "core/control_signal.h"
#include "core/evaluator.h"
#include "core/hash.h"
#include "core/plants.h"
#include "core/rng.h"
#include "core/scenario.h"
#include "core/season.h"
#include "core/world.h"
#include "runtime/scheduler.h"
#include "runtime/time_source.h"
#include "runtime/timer.h"

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
// Each argument is evaluated exactly once. The earlier version evaluated it twice
// and broke silently as soon as an argument had a side effect.
#define CHECK_EQ(a, b) \
    do { \
        const long long check_a_value_ = static_cast<long long>(a); \
        const long long check_b_value_ = static_cast<long long>(b); \
        report((check_a_value_ == check_b_value_), __FILE__, __LINE__, \
               #a " == " #b, check_a_value_, check_b_value_, true); \
    } while (false)

// A run with a set number of crops and a chosen start day.
static std::unique_ptr<IEvaluator> evaluatorWith(std::int64_t crop_rate,
                                                 std::uint64_t seed = 0,
                                                 std::int64_t start_day = 1,
                                                 std::int64_t crop_count = 1,
                                                 std::int64_t wither_rate = 0) {
    Scenario sc;
    sc.seed = seed;
    sc.start_day = start_day;
    sc.crop_count = crop_count;
    sc.crop_rate = crop_rate;
    sc.wither_rate = wither_rate;
    return buildEvaluator(sc);
}

static const PlantSystem* plants(const World& w) {
    for (const auto& system : w.systems()) {
        if (const auto* p = dynamic_cast<const PlantSystem*>(system.get())) {
            return p;
        }
    }
    return nullptr;
}

static const PlantSystem* plants(const IEvaluator& ev) { return plants(ev.world()); }

// Growth of the first crop, in whole units.
static std::int64_t firstCropGrowth(const IEvaluator& ev) {
    const PlantSystem* p = plants(ev);
    if (p == nullptr || p->crops().empty()) {
        return -1;
    }
    return p->crops()[0].growth();
}

// --- calendar -------------------------------------------------------------

static void testCalendar() {
    CHECK_EQ(fromCalendar(CalendarDate{1, 1, 1, 0, 0, 0, 0}), 0);
    CHECK_EQ(kUsPerDay, 86400000000LL);
    CHECK_EQ(kUsPerYear, 31104000000000LL);

    const CalendarDate month = toCalendar(30 * kUsPerDay);
    CHECK_EQ(month.month, 2);
    CHECK_EQ(month.day, 1);

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

    const CalendarDate next = toCalendar(kUsPerYear);
    CHECK_EQ(next.year, 2);
    CHECK_EQ(next.month, 1);
    CHECK_EQ(next.day, 1);
    CHECK_EQ(next.hour, 0);
}

// --- seasons --------------------------------------------------------------

static void testSeasons() {
    CHECK(seasonAt(0) == Season::Spring);
    CHECK(seasonAt(90 * kUsPerDay - 1) == Season::Spring);
    CHECK(seasonAt(90 * kUsPerDay) == Season::Summer);
    CHECK(seasonAt(180 * kUsPerDay) == Season::Autumn);
    CHECK(seasonAt(270 * kUsPerDay) == Season::Winter);
    CHECK(seasonAt(360 * kUsPerDay) == Season::Spring);  // the year wraps

    CHECK_EQ(kUsPerSeason, 90 * kUsPerDay);
    CHECK_EQ(usUntilNextSeason(0), kUsPerSeason);
    CHECK_EQ(usUntilNextSeason(45 * kUsPerDay), 45 * kUsPerDay);
    // Exactly on a boundary: a whole season is still ahead, not zero.
    CHECK_EQ(usUntilNextSeason(90 * kUsPerDay), kUsPerSeason);

    CHECK(seasonGrows(Season::Spring));
    CHECK(seasonGrows(Season::Summer));
    CHECK(seasonWithers(Season::Autumn));
    CHECK(!seasonGrows(Season::Winter));
    CHECK(!seasonWithers(Season::Winter));
}

// --- the evaluator owns time ----------------------------------------------

static void testClock() {
    std::unique_ptr<IEvaluator> ev = evaluatorWith(0);
    const std::int64_t dt = 1000;
    for (int i = 0; i < 1000; ++i) {
        ev->makeStep(dt);
    }
    CHECK_EQ(ev->clock().now_us(), 1000 * dt);
}

static void testStartDayIsHonoured() {
    std::unique_ptr<IEvaluator> ev = evaluatorWith(0, 0, 178);
    CHECK_EQ(ev->clock().now_us(), 177 * kUsPerDay);
    CHECK(seasonAt(ev->clock().now_us()) == Season::Summer);
}

static void testSignalsApplyOnBoundary() {
    std::unique_ptr<IEvaluator> ev = evaluatorWith(0);
    ev->pushSignal(ControlSignal{SignalKind::InterruptAcceleration, 0});
    CHECK_EQ(ev->pendingSignals(), 1u);
    ev->makeStep(1000);
    CHECK_EQ(ev->pendingSignals(), 0u);
}

static void testAccelerationMode() {
    std::unique_ptr<IEvaluator> ev = evaluatorWith(0);
    CHECK(ev->mode() == Mode::Normal);

    ev->pushSignal(ControlSignal{SignalKind::BeginAcceleration, 1000});
    ev->makeStep(400);
    CHECK(ev->mode() == Mode::Accelerating);
    CHECK_EQ(ev->accelerationRemaining_us(), 600);

    ev->makeStep(600);
    CHECK(ev->mode() == Mode::Normal);
    CHECK_EQ(ev->accelerationRemaining_us(), 0);
    CHECK_EQ(ev->clock().now_us(), 1000);

    ev->pushSignal(ControlSignal{SignalKind::BeginAcceleration, 100000});
    ev->makeStep(10);
    CHECK(ev->mode() == Mode::Accelerating);
    ev->pushSignal(ControlSignal{SignalKind::InterruptAcceleration, 0});
    CHECK(ev->mode() == Mode::Accelerating);
    ev->makeStep(10);
    CHECK(ev->mode() == Mode::Normal);
}

// --- the accumulator, kept as a utility -----------------------------------

static void testAccumulatorExactness() {
    // Crops no longer use it - they can shrink, and a signed sum handles that - but
    // it stays in the library and stays tested: it is the tool for any rate that
    // only goes up.
    const std::int64_t rate = 123457;  // micro-units per second
    const std::int64_t total = 7 * kUsPerDay;
    const std::int64_t expected = rate * total / 1000000;

    auto grown = [rate, total](std::int64_t dt) {
        Accumulator acc;
        std::int64_t done = 0;
        while (done < total) {
            const std::int64_t slice = std::min(dt, total - done);
            acc.advance(rate, slice);
            done += slice;
        }
        return acc.whole;
    };

    CHECK_EQ(grown(total), expected);
    CHECK_EQ(grown(total / 7), expected);
    CHECK_EQ(grown(total / 1000), expected);

    const std::int64_t one_second = kUsPerSecond;
    Accumulator acc;
    for (std::int64_t i = 0; i < one_second; ++i) {
        acc.advance(rate, 1);
    }
    CHECK_EQ(acc.whole, rate * one_second / 1000000);
}

// --- crops grow and wither by season --------------------------------------

static void testCropsGrowInSpringAndSummer() {
    std::unique_ptr<IEvaluator> spring = evaluatorWith(1, 0, 1);  // day 1
    spring->makeStep(kUsPerDay);
    CHECK_EQ(firstCropGrowth(*spring), 86400);  // 1 unit per second for a day

    std::unique_ptr<IEvaluator> summer = evaluatorWith(1, 0, 100);  // day 100
    summer->makeStep(kUsPerDay);
    CHECK_EQ(firstCropGrowth(*summer), 86400);
}

static void testCropsAreDormantInWinter() {
    std::unique_ptr<IEvaluator> winter = evaluatorWith(1, 0, 271);
    winter->makeStep(kUsPerDay);
    CHECK_EQ(firstCropGrowth(*winter), 0);
}

static void testCropsWitherInAutumn() {
    // Summer day grows, the next day is autumn and takes it back down.
    std::unique_ptr<IEvaluator> ev = evaluatorWith(1, 0, 180, 1, 1);
    ev->makeStep(kUsPerDay);
    CHECK_EQ(firstCropGrowth(*ev), 86400);
    ev->makeStep(kUsPerDay);
    CHECK_EQ(firstCropGrowth(*ev), 0);
}

static void testWitheringStopsAtZero() {
    std::unique_ptr<IEvaluator> ev = evaluatorWith(1, 0, 200, 1, 5);  // autumn, fast wither
    ev->makeStep(kUsPerDay);
    const PlantSystem* p = plants(*ev);
    CHECK(p != nullptr);
    if (p != nullptr && !p->crops().empty()) {
        CHECK_EQ(p->crops()[0].growth_sum, 0);
        CHECK_EQ(p->crops()[0].growth(), 0);
    }
}

// --- the headline property: a season boundary does not depend on dt -------

static void testSeasonBoundaryIsDtInvariant() {
    // Four days from day 178: three of growth, then the boundary into autumn, then
    // a day of withering. Every step size must reach the same world.
    const std::int64_t total = 4 * kUsPerDay;

    auto run = [total](std::int64_t dt) {
        std::unique_ptr<IEvaluator> ev = evaluatorWith(1000, 42, 178, 8, 600);
        std::int64_t done = 0;
        while (done < total) {
            const std::int64_t slice = std::min(dt, total - done);
            ev->makeStep(slice);
            done += slice;
        }
        return ev->hash();
    };

    const std::uint64_t one_second = run(1000000);
    CHECK_EQ(run(3600000000), one_second);   // hour steps: land exactly on the boundary
    CHECK_EQ(run(25200000000), one_second);  // seven-hour steps: the boundary falls inside one
    CHECK_EQ(run(total), one_second);        // one single four-day step

    // And the amount itself: three days growing at 1000, one day withering at 600.
    std::unique_ptr<IEvaluator> ev = evaluatorWith(1000, 42, 178, 8, 600);
    for (int i = 0; i < 4; ++i) {
        ev->makeStep(kUsPerDay);
    }
    CHECK_EQ(firstCropGrowth(*ev), 3 * 86400 * 1000 - 86400 * 600);
}

// --- general invariance and determinism -----------------------------------

static void testDtInvariance() {
    const std::int64_t total = 90 * kUsPerDay;  // a whole season

    auto run = [total](std::int64_t dt) {
        std::unique_ptr<IEvaluator> ev = evaluatorWith(987, 42, 1, 2);
        std::int64_t done = 0;
        while (done < total) {
            const std::int64_t slice = std::min(dt, total - done);
            ev->makeStep(slice);
            done += slice;
        }
        return ev->hash();
    };

    const std::uint64_t once = run(total);
    CHECK_EQ(run(total / 2), once);
    CHECK_EQ(run(total / 96), once);
    CHECK_EQ(run(1000000), once);
}

static void testDeterminism() {
    std::unique_ptr<IEvaluator> a = evaluatorWith(500, 7, 1, 4);
    std::unique_ptr<IEvaluator> b = evaluatorWith(500, 7, 1, 4);
    for (int i = 0; i < 1000; ++i) {
        a->makeStep(333);
        b->makeStep(333);
    }
    CHECK_EQ(a->hash(), b->hash());
}

static void testHashSensitivity() {
    CHECK_EQ(evaluatorWith(0, 0)->hash(), evaluatorWith(0, 0)->hash());

    std::unique_ptr<IEvaluator> timed = evaluatorWith(0, 0);
    timed->makeStep(kUsPerDay);
    CHECK(evaluatorWith(0, 0)->hash() != timed->hash());

    // Growth is covered.
    std::unique_ptr<IEvaluator> grown = evaluatorWith(1, 0);
    grown->makeStep(kUsPerDay);
    std::unique_ptr<IEvaluator> still = evaluatorWith(0, 0);
    still->makeStep(kUsPerDay);
    CHECK(grown->hash() != still->hash());

    // So is the number of crops, i.e. the container is covered.
    CHECK(evaluatorWith(0, 0, 1, 2)->hash() != evaluatorWith(0, 0, 1, 3)->hash());

    // And the world RNG.
    CHECK(evaluatorWith(0, 1)->hash() != evaluatorWith(0, 2)->hash());
}

// --- settings live outside the simulation ---------------------------------
//
// No test asserts that a setting cannot reach the world, because the compiler
// guarantees it: Settings lives in the config library, which core does not depend
// on. A file in core cannot name the type.

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
    loaded.brightness = 7;
    std::string error;
    CHECK(config::loadSettings("tmp_does_not_exist_settings.ini", loaded, error));
    CHECK_EQ(loaded.brightness, 7);
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
    CHECK_EQ(sc.start_day, 178);
    CHECK_EQ(sc.crop_count, 8);
    CHECK_EQ(sc.crop_rate, 1000);
    CHECK_EQ(sc.wither_rate, 600);
    CHECK_EQ(sc.steps, 345600);
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
    CHECK(!loadScenario(path, sc, error));
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
    std::remove(path.c_str());
}

static void testScenarioRejectsBadStartDay() {
    const std::string path = "tmp_bad_day.scn";
    {
        std::FILE* f = std::fopen(path.c_str(), "wb");
        if (f == nullptr) {
            CHECK(false);
            return;
        }
        std::fputs("[state]\nstart_day = 0\n[run]\nsteps = 1\ndt_us = 1\n", f);
        std::fclose(f);
    }
    Scenario sc;
    std::string error;
    CHECK(!loadScenario(path, sc, error));
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

// --- the generating mechanism --------------------------------------------

static void testBuildEvaluator() {
    Scenario sc;
    sc.seed = 99;
    sc.start_day = 5;
    sc.crop_count = 3;
    sc.crop_rate = 250;
    std::unique_ptr<IEvaluator> ev = buildEvaluator(sc);

    CHECK_EQ(ev->world().systems().size(), 1u);
    CHECK(plants(*ev) != nullptr);
    if (plants(*ev) != nullptr) {
        CHECK_EQ(plants(*ev)->crops().size(), 3u);
        CHECK_EQ(plants(*ev)->growthRate(), 250);
        CHECK_EQ(plants(*ev)->crops()[0].x, 0);
        CHECK_EQ(plants(*ev)->crops()[2].x, 2);
    }
    CHECK_EQ(ev->clock().now_us(), 4 * kUsPerDay);
    CHECK(ev->mode() == Mode::Normal);

    CHECK_EQ(buildEvaluator(sc)->hash(), ev->hash());
    sc.seed = 100;
    CHECK(buildEvaluator(sc)->hash() != ev->hash());
}

// --- pacing: the step rate is the simulation's, not the display's ---------

static void testPacingIgnoresChunking() {
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

    const std::int64_t even = run(dt);
    CHECK_EQ(even, total / dt);
    CHECK_EQ(run(6944), even);
    CHECK_EQ(run(16667), even);
    CHECK_EQ(run(1000), even);
    CHECK_EQ(run(total), even);
}

static void testPacingKeepsRemainder() {
    runtime::Scheduler scheduler(20000);
    for (int i = 0; i < 144; ++i) {
        scheduler.addElapsed(6944);
    }
    CHECK_EQ(scheduler.takeDueSteps(1000), 49);
    CHECK_EQ(scheduler.pending_us(), 19936);

    scheduler.addElapsed(6944);
    CHECK_EQ(scheduler.takeDueSteps(1000), 1);
    CHECK_EQ(scheduler.pending_us(), 6880);
}

static void testPacingCapsLongStall() {
    runtime::Scheduler scheduler(20000);
    scheduler.addElapsed(10 * 1000000);
    CHECK_EQ(scheduler.takeDueSteps(3), 3);
    CHECK_EQ(scheduler.pending_us(), 0);
}

// --- the timer: late binding, pacing, and acceleration --------------------

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

    time.advance(100000);
    CHECK_EQ(timer.poll(100), 0);
    CHECK(!timer.attached());

    std::unique_ptr<IEvaluator> ev = evaluatorWith(0);
    timer.attach(ev.get());
    CHECK(timer.attached());

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

    time.advance(30000);
    CHECK_EQ(timer.poll(100), 1);
    CHECK_EQ(ev->clock().now_us(), 40000);
    CHECK_EQ(timer.scheduler().pending_us(), 10000);
}

static void testTimerWaitsForTheNextStep() {
    FakeTimeSource time;
    runtime::Timer timer(runtime::Scheduler(20000), time);
    std::unique_ptr<IEvaluator> ev = evaluatorWith(0);
    timer.attach(ev.get());

    timer.waitForNextStep();
    CHECK_EQ(time.slept_us(), 20000);

    time.advance(5000);
    timer.poll(100);
    timer.waitForNextStep();
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

    const std::int64_t before = time.slept_us();
    timer.waitForNextStep();
    CHECK_EQ(time.slept_us(), before);
}

// --- timing ---------------------------------------------------------------
//
// Wall-clock numbers are reported, never asserted. A test that fails when the
// machine is busy teaches people to ignore failures; a table printed on every run
// shows a regression the moment it appears.

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
    {"seasons", testSeasons},
    {"clock", testClock},
    {"start day honoured", testStartDayIsHonoured},
    {"signals apply on boundary", testSignalsApplyOnBoundary},
    {"acceleration mode", testAccelerationMode},
    {"accumulator exactness", testAccumulatorExactness},
    {"crops grow", testCropsGrowInSpringAndSummer},
    {"crops dormant in winter", testCropsAreDormantInWinter},
    {"crops wither", testCropsWitherInAutumn},
    {"wither stops at zero", testWitheringStopsAtZero},
    {"season boundary invariant", testSeasonBoundaryIsDtInvariant},
    {"dt invariance", testDtInvariance},
    {"determinism", testDeterminism},
    {"hash sensitivity", testHashSensitivity},
    {"settings round trip", testSettingsRoundTrip},
    {"settings missing file", testSettingsMissingFileIsDefaults},
    {"settings unknown key", testSettingsRejectsUnknownKey},
    {"scenario loading", testScenarioLoading},
    {"scenario unknown key", testScenarioRejectsUnknownKey},
    {"scenario BOM", testScenarioToleratesBom},
    {"scenario bad start day", testScenarioRejectsBadStartDay},
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
// targets from OD-33.
static void runBenchmark(std::int64_t steps) {
    const std::int64_t dt_us = 20000;

    {
        std::unique_ptr<IEvaluator> warm = evaluatorWith(1000, 42, 1, 8);
        for (std::int64_t i = 0; i < steps / 10 + 1; ++i) {
            warm->makeStep(dt_us);
        }
    }

    std::unique_ptr<IEvaluator> ev = evaluatorWith(1000, 42, 1, 8);
    const std::int64_t start = now_us();
    for (std::int64_t i = 0; i < steps; ++i) {
        ev->makeStep(dt_us);
    }
    const std::int64_t elapsed = now_us() - start;

    const double seconds = elapsed > 0 ? static_cast<double>(elapsed) / 1e6 : 1e-9;
    const double per_second = static_cast<double>(steps) / seconds;

    std::printf("\nthroughput\n");
    std::printf("  %lld steps at dt %lld us, 8 crops\n", static_cast<long long>(steps),
                static_cast<long long>(dt_us));
    std::printf("  wall     %.3f s\n", seconds);
    std::printf("  rate     %.0f steps/s\n", per_second);
    std::printf("  cost     %.4f us/step\n", seconds * 1e6 / static_cast<double>(steps));

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

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
#include "core/calendar.h"
#include "core/control_signal.h"
#include "core/evaluator.h"
#include "core/hash.h"
#include "core/plants.h"
#include "core/rng.h"
#include "core/scenario.h"
#include "core/month_curve.h"
#include "core/season.h"
#include "core/weather.h"
#include "core/world.h"
#include "runtime/scheduler.h"
#include "runtime/time_source.h"
#include "runtime/timer.h"

#ifdef THYKINGDOM_PROFILE
#include <easy/profiler.h>
#endif

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

// A curve with the same coefficient in every month: growth is then just base_rate
// times elapsed seconds, which keeps test expectations readable.
static GrowthCurve flatProfile(std::int64_t coefficient) {
    GrowthCurve profile;
    for (int month = 1; month <= kMonthsPerYear; ++month) {
        profile.setMonth(month, coefficient);
    }
    return profile;
}

// The same shape for temperature. It is a different tagged instantiation, so it cannot
// be passed where a growth curve is expected - which is the point of the tags.
static TemperatureCurve flatTemperature(std::int64_t coefficient) {
    TemperatureCurve curve;
    for (int month = 1; month <= kMonthsPerYear; ++month) {
        curve.setMonth(month, coefficient);
    }
    return curve;
}

// A run with a set number of crops and a chosen start day.
static std::unique_ptr<IEvaluator> evaluatorWith(
    std::int64_t crop_rate,
    std::uint64_t seed = 0,
    std::int64_t start_day = 1,
    std::int64_t crop_count = 1,
    const GrowthCurve& profile = flatProfile(100),
    const TemperatureCurve& temperature = flatTemperature(0)) {
    Scenario sc;
    sc.seed = seed;
    sc.start_day = start_day;
    sc.crop_count = crop_count;
    sc.crop_rate = crop_rate;
    for (int month = 0; month < kMonthsPerYear; ++month) {
        sc.coefficient[month] = profile.coefficient(month + 1);
        sc.temperature[month] = temperature.coefficient(month + 1);
    }
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

static const WeatherSystem* weather(const World& w) {
    for (const auto& system : w.systems()) {
        if (const auto* c = dynamic_cast<const WeatherSystem*>(system.get())) {
            return c;
        }
    }
    return nullptr;
}

static const WeatherSystem* weather(const IEvaluator& ev) { return weather(ev.world()); }

// Growth of the first crop, in whole units.
static std::int64_t firstCropGrowth(const IEvaluator& ev) {
    const PlantSystem* p = plants(ev);
    if (p == nullptr || p->crops().empty()) {
        return -1;
    }
    return p->crops()[0].growth;
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
    // Seasons are a label for the month that is passing, not a block of days.
    CHECK(seasonOfMonth(1) == Season::Winter);
    CHECK(seasonOfMonth(2) == Season::Winter);
    CHECK(seasonOfMonth(3) == Season::Spring);
    CHECK(seasonOfMonth(5) == Season::Spring);
    CHECK(seasonOfMonth(6) == Season::Summer);
    CHECK(seasonOfMonth(8) == Season::Summer);
    CHECK(seasonOfMonth(9) == Season::Autumn);
    CHECK(seasonOfMonth(11) == Season::Autumn);
    CHECK(seasonOfMonth(12) == Season::Winter);

    CHECK_EQ(monthOf(0), 1);
    CHECK_EQ(monthOf(kUsPerMonth), 2);
    CHECK_EQ(monthOf(11 * kUsPerMonth), 12);
    CHECK_EQ(monthOf(kUsPerYear), 1);  // the next year's January
    CHECK(seasonAt(2 * kUsPerMonth) == Season::Spring);
}

static void testMonthProfileInterpolates() {
    GrowthCurve profile;
    profile.setMonth(3, 40);
    profile.setMonth(4, 80);

    // Knots are exact at month starts.
    CHECK_EQ(profile.at(2 * kUsPerMonth), 40);
    CHECK_EQ(profile.at(3 * kUsPerMonth), 80);
    // And halfway between two knots the value is halfway between them.
    CHECK_EQ(profile.at(2 * kUsPerMonth + kUsPerMonth / 2), 60);
    // Months with no knot are zero.
    CHECK_EQ(profile.at(0), 0);

    // The integral over March is the trapezoid of its two knots.
    const std::int64_t march = profile.cumulative(3 * kUsPerMonth) - profile.cumulative(2 * kUsPerMonth);
    CHECK_EQ(march, (40 + 80) * kSecondsPerMonth / 2);
}

// --- the evaluator owns time ----------------------------------------------

static void testCalendarAdvances() {
    std::unique_ptr<IEvaluator> ev = evaluatorWith(0);
    const std::int64_t dt = 1000;
    for (int i = 0; i < 1000; ++i) {
        ev->makeStep(dt);
    }
    CHECK_EQ(ev->calendar().now_us(), 1000 * dt);
}

static void testCalendarCountsMonthBoundaries() {
    // A month boundary is crossed when the calendar enters a month, and the count is
    // what advance() reports. Nothing consumes it yet, which is exactly why it is
    // worth pinning down now.
    Calendar calendar;
    CHECK_EQ(calendar.month(), 1);
    CHECK_EQ(calendar.advance(kUsPerDay), 0);  // a day crosses nothing

    // The last microsecond of January, then one more: exactly one boundary.
    Calendar january(29 * kUsPerDay + kUsPerDay - 1);
    CHECK_EQ(january.month(), 1);
    CHECK_EQ(january.advance(1), 1);
    CHECK_EQ(january.month(), 2);

    // Crossing a year end counts the boundary, not the year.
    Calendar december(11 * kUsPerMonth + kUsPerMonth - 1);
    CHECK_EQ(december.month(), 12);
    CHECK_EQ(december.advance(1), 1);
    CHECK_EQ(december.month(), 1);
    CHECK_EQ(december.date().year, 2);

    // A long span reports every boundary it crossed.
    Calendar span;
    CHECK_EQ(span.advance(3 * kUsPerMonth), 3);

    // Season and date are readings of the same instant.
    CHECK(Calendar(2 * kUsPerMonth).season() == Season::Spring);
    CHECK_EQ(Calendar(2 * kUsPerMonth).date().month, 3);
}

static void testStartDayIsHonoured() {
    std::unique_ptr<IEvaluator> ev = evaluatorWith(0, 0, 178);
    CHECK_EQ(ev->calendar().now_us(), 177 * kUsPerDay);
    CHECK(seasonAt(ev->calendar().now_us()) == Season::Summer);
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
    CHECK_EQ(ev->calendar().now_us(), 1000);

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

// --- growth follows the month coefficients --------------------------------

static void testCropsGrowWithPositiveCoefficient() {
    std::unique_ptr<IEvaluator> ev = evaluatorWith(1, 0, 1);
    ev->makeStep(kUsPerDay);
    CHECK_EQ(firstCropGrowth(*ev), 86400);
}

static void testCropsStandStillAtZeroCoefficient() {
    std::unique_ptr<IEvaluator> ev = evaluatorWith(1, 0, 1, 1, flatProfile(0));
    ev->makeStep(kUsPerDay);
    CHECK_EQ(firstCropGrowth(*ev), 0);
}

static void testWitheringStopsAtZero() {
    std::unique_ptr<IEvaluator> ev = evaluatorWith(1, 0, 1, 1, flatProfile(-100));
    ev->makeStep(kUsPerDay);
    CHECK_EQ(firstCropGrowth(*ev), 0);
}

static void testGrowthFollowsTheInterpolatedKnot() {
    // Only May carries a knot (100). The interpolation is what matters: the ramp up
    // happens during April, from 0 to 100, and the ramp down during May, from 100
    // back to 0. A crop sown on day 1 collects half a month of full rate while April
    // runs, and the other half while May runs.
    GrowthCurve profile;
    profile.setMonth(5, 100);

    std::unique_ptr<IEvaluator> ev = evaluatorWith(1, 0, 1, 1, profile);

    ev->makeStep(59 * kUsPerDay);  // still February: no ramp has started
    CHECK_EQ(firstCropGrowth(*ev), 0);

    ev->makeStep(61 * kUsPerDay);  // through the end of April: half the ramp
    CHECK_EQ(firstCropGrowth(*ev), kSecondsPerMonth / 2);

    ev->makeStep(30 * kUsPerDay);  // through the end of May: the whole trapezoid
    CHECK_EQ(firstCropGrowth(*ev), kSecondsPerMonth);

    ev->makeStep(30 * kUsPerDay);  // June onward: the knot is behind us
    CHECK_EQ(firstCropGrowth(*ev), kSecondsPerMonth);
}

// --- the headline property: the step size cannot matter --------------------

static void testGrowthIsIndependentOfTheStep() {
    // Four days from day 100: April 0.8 rising towards May 1.0. The coefficient is
    // not constant, which is exactly why this must be evaluated and not accumulated.
    GrowthCurve profile;
    profile.setMonth(3, 40);
    profile.setMonth(4, 80);
    profile.setMonth(5, 100);
    profile.setMonth(6, 100);
    profile.setMonth(9, -50);
    profile.setMonth(10, -100);

    const std::int64_t total = 4 * kUsPerDay;
    auto run = [total, &profile](std::int64_t dt) {
        std::unique_ptr<IEvaluator> ev = evaluatorWith(1000, 42, 100, 8, profile);
        std::int64_t done = 0;
        while (done < total) {
            const std::int64_t slice = std::min(dt, total - done);
            ev->makeStep(slice);
            done += slice;
        }
        return ev->hash();
    };

    const std::uint64_t one_second = run(1000000);
    CHECK_EQ(run(3600000000), one_second);   // hour steps
    CHECK_EQ(run(25200000000), one_second);  // seven-hour steps
    CHECK_EQ(run(total), one_second);        // one single four-day step
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
    CHECK_EQ(run(total / 2400), once);
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

    // Weather is covered too, but only after a step: the temperature is computed in
    // step(), so two freshly built evaluators are both still at zero.
    std::unique_ptr<IEvaluator> cool = evaluatorWith(0, 0, 1, 0, flatProfile(0), flatTemperature(500));
    std::unique_ptr<IEvaluator> hot = evaluatorWith(0, 0, 1, 0, flatProfile(0), flatTemperature(1500));
    cool->makeStep(kUsPerDay);
    hot->makeStep(kUsPerDay);
    CHECK(cool->hash() != hot->hash());
}

// --- weather: temperature as a function of the clock ----------------------

static void testWeatherKnotsAreExactAtMonthStarts() {
    TemperatureCurve profile;
    profile.setMonth(3, 500);   // 5.00 degrees at the start of March
    profile.setMonth(4, 1200);  // 12.00 at the start of April

    std::unique_ptr<IEvaluator> ev = evaluatorWith(0, 0, 1, 0, flatProfile(0), profile);

    ev->makeStep(30 * kUsPerDay);  // start of February: no knot here, so zero
    CHECK(weather(*ev) != nullptr);
    CHECK_EQ(weather(*ev)->temperature(), 0);

    ev->makeStep(30 * kUsPerDay);  // start of March: the knot itself
    CHECK_EQ(weather(*ev)->temperature(), 500);

    ev->makeStep(30 * kUsPerDay);  // start of April: the next knot
    CHECK_EQ(weather(*ev)->temperature(), 1200);

    ev->makeStep(30 * kUsPerDay);  // start of May: unset again, so back to zero
    CHECK_EQ(weather(*ev)->temperature(), 0);
}

static void testWeatherInterpolatesBetweenKnots() {
    TemperatureCurve profile;
    profile.setMonth(3, 500);
    profile.setMonth(4, 1200);

    std::unique_ptr<IEvaluator> ev = evaluatorWith(0, 0, 1, 0, flatProfile(0), profile);

    // Halfway through March the value sits halfway between 500 and 1200.
    ev->makeStep(75 * kUsPerDay);
    CHECK_EQ(weather(*ev)->temperature(), 850);
}

static void testWeatherChangesWithTheClock() {
    TemperatureCurve profile;
    profile.setMonth(5, 1800);

    std::unique_ptr<IEvaluator> ev = evaluatorWith(0, 0, 1, 0, flatProfile(0), profile);
    ev->makeStep(kUsPerDay);
    const std::int64_t first = weather(*ev)->temperature();

    ev->makeStep(100 * kUsPerDay);
    const std::int64_t later = weather(*ev)->temperature();

    CHECK(first != later);
    CHECK(later > first);  // climbing towards the May knot
}

static void testWeatherIsReproducible() {
    TemperatureCurve profile;
    profile.setMonth(5, 1800);

    std::unique_ptr<IEvaluator> a = evaluatorWith(0, 7, 1, 0, flatProfile(0), profile);
    std::unique_ptr<IEvaluator> b = evaluatorWith(0, 7, 1, 0, flatProfile(0), profile);
    for (int i = 0; i < 100; ++i) {
        a->makeStep(kUsPerDay);
        b->makeStep(kUsPerDay);
    }
    CHECK_EQ(a->hash(), b->hash());
    CHECK_EQ(weather(*a)->temperature(), weather(*b)->temperature());
}

static void testZeroTemperatureLeavesPlantsAlone() {
    // All-zero temperature is the default everywhere else, so this pins two things at
    // once: zero weather is valid, and crops do not read it. Wiring them together is a
    // later decision and must not happen by accident.
    std::unique_ptr<IEvaluator> cold = evaluatorWith(1, 0, 1, 1, flatProfile(100), flatTemperature(0));
    cold->makeStep(kUsPerDay);
    CHECK_EQ(firstCropGrowth(*cold), 86400);

    std::unique_ptr<IEvaluator> warm = evaluatorWith(1, 0, 1, 1, flatProfile(100), flatTemperature(2000));
    warm->makeStep(kUsPerDay);
    CHECK_EQ(firstCropGrowth(*warm), 86400);
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
    CHECK_EQ(sc.start_day, 100);
    CHECK_EQ(sc.crop_count, 8);
    CHECK_EQ(sc.crop_rate, 1000);
    CHECK_EQ(sc.coefficient[2], 40);   // March
    CHECK_EQ(sc.coefficient[3], 80);   // April
    CHECK_EQ(sc.coefficient[8], -50);  // September
    CHECK_EQ(sc.coefficient[11], 0);   // December
    CHECK_EQ(sc.temperature[0], -500);  // January
    CHECK_EQ(sc.temperature[6], 2100);  // July
    CHECK_EQ(sc.temperature[11], -300); // December
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

    CHECK_EQ(ev->world().systems().size(), 2u);
    CHECK(plants(*ev) != nullptr);
    CHECK(weather(*ev) != nullptr);
    if (plants(*ev) != nullptr) {
        CHECK_EQ(plants(*ev)->crops().size(), 3u);
        CHECK_EQ(plants(*ev)->baseRate(), 250);
        CHECK_EQ(plants(*ev)->crops()[0].x, 0);
        CHECK_EQ(plants(*ev)->crops()[2].x, 2);
    }
    CHECK_EQ(ev->calendar().now_us(), 4 * kUsPerDay);
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
    CHECK_EQ(ev->calendar().now_us(), 0);
}

static void testTimerRunsDueSteps() {
    FakeTimeSource time;
    runtime::Timer timer(runtime::Scheduler(20000), time);
    std::unique_ptr<IEvaluator> ev = evaluatorWith(0);
    timer.attach(ev.get());

    time.advance(20000);
    CHECK_EQ(timer.poll(100), 1);
    CHECK_EQ(ev->calendar().now_us(), 20000);
    CHECK_EQ(ev->stepsTaken(), 1);

    time.advance(30000);
    CHECK_EQ(timer.poll(100), 1);
    CHECK_EQ(ev->calendar().now_us(), 40000);
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
    {"month profile", testMonthProfileInterpolates},
    {"calendar advances", testCalendarAdvances},
    {"calendar month boundaries", testCalendarCountsMonthBoundaries},
    {"start day honoured", testStartDayIsHonoured},
    {"signals apply on boundary", testSignalsApplyOnBoundary},
    {"acceleration mode", testAccelerationMode},
    {"accumulator exactness", testAccumulatorExactness},
    {"crops grow", testCropsGrowWithPositiveCoefficient},
    {"zero coefficient", testCropsStandStillAtZeroCoefficient},
    {"wither stops at zero", testWitheringStopsAtZero},
    {"growth follows the knot", testGrowthFollowsTheInterpolatedKnot},
    {"growth ignores the step", testGrowthIsIndependentOfTheStep},
    {"weather knots", testWeatherKnotsAreExactAtMonthStarts},
    {"weather interpolation", testWeatherInterpolatesBetweenKnots},
    {"weather follows the clock", testWeatherChangesWithTheClock},
    {"weather reproducible", testWeatherIsReproducible},
    {"zero temperature", testZeroTemperatureLeavesPlantsAlone},
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

    // The same span stepped, and then skipped. Plant growth is a closed form, so
    // evaluating it once at the end must be worth far more than evaluating it at
    // every step - that difference is the whole premise of the prediction graph.
    std::unique_ptr<IEvaluator> skipped = evaluatorWith(1000, 42, 1, 8);
    const std::int64_t s0 = now_us();
    skipped->makeStep(steps * dt_us);
    const std::int64_t s1 = now_us();
    const double skip_seconds = (s1 - s0) > 0 ? static_cast<double>(s1 - s0) / 1e6 : 1e-6;
    std::printf("\n  same span, stepped versus skipped:\n");
    std::printf("    stepped  %10.3f s\n", seconds);
    std::printf("    skipped  %10.6f s  (one evaluation)\n", skip_seconds);
    std::printf("    ratio    %10.0fx\n", seconds / skip_seconds);
}

int main(int argc, char** argv) {
    std::int64_t bench_steps = 0;
    std::string profile_file;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--bench" && i + 1 < argc) {
            bench_steps = std::strtoll(argv[++i], nullptr, 10);
        } else if (arg == "--profile" && i + 1 < argc) {
            profile_file = argv[++i];
        } else if (arg.rfind("--", 0) != 0) {
            g_scenario_dir = arg;
        }
    }

#ifdef THYKINGDOM_PROFILE
    // Switched on before the first measured step, so every system step in every test
    // is captured. The trace is written at the end, when the run is over.
    if (!profile_file.empty()) {
        profiler::setEnabled(true);
    }
#else
    if (!profile_file.empty()) {
        std::fprintf(stderr,
                     "tests: this build has no profiler. Reconfigure with "
                     "-DTHYKINGDOM_PROFILE=ON -DTHYKINGDOM_EASY_PROFILER_DIR=<path>\n");
        return 2;
    }
#endif

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

#ifdef THYKINGDOM_PROFILE
    if (!profile_file.empty()) {
        const std::uint32_t blocks = profiler::dumpBlocksToFile(profile_file.c_str());
        std::printf("\nprofile written: %s (%u blocks)\n", profile_file.c_str(), blocks);
    }
#endif

    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}

#include "core/evaluator.h"

#include "core/plants.h"
#include "core/scenario.h"
#include "core/weather.h"

namespace core {

// The generating mechanism, internal to this file.
//
// It is not declared in a header on purpose. A world exists inside the evaluator
// that owns it and nowhere else, so it cannot be created by one caller and then
// dropped by another; the only way to have one is to have an evaluator.
static World buildWorld(const Scenario& scenario, std::int64_t start_us) {
    World world;

    // The order of addSystem calls is the order systems run in, and it is stated
    // once, here. Weather goes first: 06 says the atmosphere is computed before
    // what grows under it, so a system that later reads the temperature sees the
    // value for this instant rather than the previous one.
    MonthProfile weather_profile;
    for (int month = 1; month <= kMonthsPerYear; ++month) {
        weather_profile.setMonth(month, scenario.temperature[month - 1]);
    }
    world.addSystem(std::unique_ptr<System>(new WeatherSystem(weather_profile)));

    MonthProfile profile;
    for (int month = 1; month <= kMonthsPerYear; ++month) {
        profile.setMonth(month, scenario.coefficient[month - 1]);
    }

    PlantSystem* plants = new PlantSystem(scenario.crop_rate, profile);
    for (std::int64_t i = 0; i < scenario.crop_count; ++i) {
        // A placeholder layout until the map exists: fertile cells will decide
        // where a crop is sown.
        plants->sow(static_cast<std::int32_t>(i % 8), static_cast<std::int32_t>(i / 8), start_us);
    }
    world.addSystem(std::unique_ptr<System>(plants));

    world.rng().reseed(scenario.seed, 1);
    return world;
}

Evaluator::Evaluator(const Scenario& scenario)
    : m_clock((scenario.start_day - 1) * kUsPerDay),
      m_world(buildWorld(scenario, (scenario.start_day - 1) * kUsPerDay)) {}

void Evaluator::pushSignal(const ControlSignal& signal) {
    m_pending.push_back(signal);
}

void Evaluator::applyPendingSignals() {
    for (const auto& signal : m_pending) {
        switch (signal.kind) {
        case SignalKind::BeginAcceleration:
            m_mode = Mode::Accelerating;
            m_acceleration_remaining_us = signal.span_us;
            break;
        case SignalKind::InterruptAcceleration:
            m_mode = Mode::Normal;
            m_acceleration_remaining_us = 0;
            break;
        }
    }
    m_pending.clear();
}

void Evaluator::makeStep(std::int64_t dt_us) {
    m_step_count++;
    applyPendingSignals();
    m_clock.advance(dt_us);
    m_world.step(m_clock, dt_us);

    if (m_mode == Mode::Accelerating) {
        m_acceleration_remaining_us -= dt_us;
        if (m_acceleration_remaining_us <= 0) {
            m_acceleration_remaining_us = 0;
            m_mode = Mode::Normal;
        }
    }
}

std::uint64_t Evaluator::hash() const {
    Hash64 h;
    h.mixI64(m_clock.now_us());
    m_world.hash(h);
    return h.value();
}

std::unique_ptr<IEvaluator> buildEvaluator(const Scenario& scenario) {
    return std::unique_ptr<IEvaluator>(new Evaluator(scenario));
}

}  // namespace core

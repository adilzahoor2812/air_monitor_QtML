#include "MockGenerator.h"

#include <algorithm>
#include <cmath>

MockGenerator::MockGenerator(QObject *parent)
    : QObject(parent)
{
    m_timer.setInterval(50);
    connect(&m_timer, &QTimer::timeout, this, &MockGenerator::tick);
}

void MockGenerator::setRunning(bool running)
{
    if (running == m_timer.isActive())
        return;

    if (running) {
        m_clock.start();
        m_timer.start();
        tick();
    } else {
        m_timer.stop();
    }
}

bool MockGenerator::isRunning() const
{
    return m_timer.isActive();
}

void MockGenerator::tick()
{
    const double t = static_cast<double>(m_clock.elapsed()) / 1000.0;
    const double cycle = std::fmod(t, 24.0);

    double distance = 52.0 + 14.0 * std::sin(t * 0.5);
    if (cycle > 16.0 && cycle <= 20.0) {
        const double blend = (cycle - 16.0) / 4.0;
        distance = distance * (1.0 - blend) + 16.0 * blend;
    } else if (cycle > 20.0 && cycle <= 22.5) {
        const double blend = (cycle - 20.0) / 2.5;
        distance = 16.0 * (1.0 - blend) + 7.0 * blend;
    } else if (cycle > 22.5) {
        const double blend = (cycle - 22.5) / 1.5;
        distance = 7.0 * (1.0 - blend) + 52.0 * blend;
    }
    distance = std::clamp(distance, 2.0, 400.0);

    SensorSample sample;
    sample.timestampMs = m_clock.elapsed();
    sample.distanceCm = distance;
    sample.ax = 2.4 * std::sin(t * 0.9) + 0.35 * std::sin(t * 2.6);
    sample.ay = 1.7 * std::cos(t * 0.7) + 0.25 * std::cos(t * 2.1);
    sample.az = 8.4 + 2.0 * std::sin(t * 0.45);
    sample.gx = 18.0 * std::sin(t * 0.6) + 3.0 * std::sin(t * 1.8);
    sample.gy = 11.0 * std::cos(t * 0.8);
    sample.gz = 6.0 * std::sin(t * 1.15);

    if (distance < 10.0)
        sample.state = QStringLiteral("FAULT");
    else if (distance < 20.0)
        sample.state = QStringLiteral("WARNING");
    else
        sample.state = QStringLiteral("OK");

    sample.valid = true;
    emit sampleReceived(sample);
}

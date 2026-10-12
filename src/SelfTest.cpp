#include "SelfTest.h"

#include "CsvParser.h"
#include "CsvRecorder.h"
#include "LineBuffer.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>

#include <iostream>

namespace
{
int g_failed = 0;

void expect(bool condition, const char *message)
{
    if (condition)
        return;
    std::cerr << "FAIL: " << message << '\n';
    ++g_failed;
}

void expectStatus(const ParseResult &result, ParseStatus status, const char *message)
{
    expect(result.status == status, message);
}
}

int runSelfTests()
{
    int argc = 1;
    char arg0[] = "SensorDashboard";
    char *argv[] = {arg0, nullptr};
    QCoreApplication app(argc, argv);
    Q_UNUSED(app);

    {
        const ParseResult parsed = parseSensorLine(
            "1000,42.5,0.10,-0.20,9.81,1.5,-2.5,0.25,OK");
        expectStatus(parsed, ParseStatus::Ok, "nominal row parses");
        expect(parsed.sample.timestampMs == 1000, "timestamp");
        expect(parsed.sample.distanceCm == 42.5, "distance");
        expect(parsed.sample.ax == 0.10, "ax");
        expect(parsed.sample.ay == -0.20, "ay");
        expect(parsed.sample.az == 9.81, "az");
        expect(parsed.sample.gx == 1.5, "gx");
        expect(parsed.sample.gy == -2.5, "gy");
        expect(parsed.sample.gz == 0.25, "gz");
        expect(parsed.sample.state == QLatin1String("OK"), "state OK");
        expect(parsed.sample.valid, "sample marked valid");
    }

    {
        const ParseResult parsed = parseSensorLine(
            "  20, 16.0, 0, 0, 9.8, 0, 0, 0, warning \r");
        expectStatus(parsed, ParseStatus::Ok, "whitespace and lowercase state");
        expect(parsed.sample.state == QLatin1String("WARNING"), "canonical WARNING");
        expect(parsed.sample.distanceCm == 16.0, "trimmed distance");
    }

    expectStatus(parseSensorLine(""), ParseStatus::Empty, "empty line");
    expectStatus(parseSensorLine("   "), ParseStatus::Empty, "blank line");
    expectStatus(parseSensorLine("# comment"), ParseStatus::Empty, "comment line");
    expectStatus(parseSensorLine("timestamp_ms,distance_cm,ax,ay,az,gx,gy,gz,state"),
                 ParseStatus::Header, "header row");
    expectStatus(parseSensorLine("\xEF\xBB\xBF" "1000,1,0,0,0,0,0,0,FAULT"),
                 ParseStatus::Ok, "bom prefix");
    expect(parseSensorLine("\xEF\xBB\xBF" "1000,1,0,0,0,0,0,0,FAULT").sample.state
               == QLatin1String("FAULT"),
           "fault state");

    expectStatus(parseSensorLine("1,2,3,4,5,6,7,OK"), ParseStatus::Invalid, "too few fields");
    expectStatus(parseSensorLine("1,2,3,4,5,6,7,8,OK,extra"), ParseStatus::Invalid, "too many fields");
    expectStatus(parseSensorLine("1.5,2,3,4,5,6,7,8,OK"), ParseStatus::Invalid, "fractional timestamp");
    expectStatus(parseSensorLine("nope,2,3,4,5,6,7,8,OK"), ParseStatus::Invalid, "bad timestamp");
    expectStatus(parseSensorLine("1,nan,3,4,5,6,7,8,OK"), ParseStatus::Invalid, "nan distance");
    expectStatus(parseSensorLine("1,inf,3,4,5,6,7,8,OK"), ParseStatus::Invalid, "infinite distance");
    expectStatus(parseSensorLine("1,,3,4,5,6,7,8,OK"), ParseStatus::Invalid, "empty distance");
    expectStatus(parseSensorLine("1,2,3,4,5,6,7,8,ERROR"), ParseStatus::Invalid, "unknown state");
    expectStatus(parseSensorLine("1,2,3,4,5,6,7,8,"), ParseStatus::Invalid, "blank state");

    {
        LineBuffer buffer;
        expect(buffer.feed(QByteArray("1,2")).lines.isEmpty(), "partial line is held");
        const LineBuffer::FeedResult rest = buffer.feed(QByteArray(",3,4,5,6,7,8,OK\n9,9"));
        expect(rest.lines.size() == 1, "split line completes once");
        expect(rest.lines.value(0) == QByteArray("1,2,3,4,5,6,7,8,OK"), "split line is joined");
        expect(buffer.pendingSize() == 3, "tail stays buffered");
    }

    {
        LineBuffer buffer;
        const LineBuffer::FeedResult fed = buffer.feed(
            QByteArray("1,1,0,0,0,0,0,0,OK\r\n2,2,0,0,0,0,0,0,FAULT\n"));
        expect(fed.lines.size() == 2, "two lines in one chunk");
        expect(fed.lines.value(0) == QByteArray("1,1,0,0,0,0,0,0,OK"), "crlf stripped");
        expect(parseSensorLine(fed.lines.value(1)).sample.state == QLatin1String("FAULT"),
               "second line parses");
    }

    {
        LineBuffer buffer;
        const LineBuffer::FeedResult fed = buffer.feed(QByteArray(70000, 'x'));
        expect(fed.overflow, "oversized chunk is rejected");
        expect(fed.lines.isEmpty(), "oversized chunk yields no line");
        const LineBuffer::FeedResult next = buffer.feed(QByteArray("5,3.25,0,0,1,0,0,0,ok\n"));
        expect(next.lines.size() == 1, "stream resynchronizes after overflow");
        const ParseResult parsed = parseSensorLine(next.lines.value(0));
        expectStatus(parsed, ParseStatus::Ok, "line after overflow parses");
        expect(parsed.sample.distanceCm == 3.25, "distance after resync");
    }

    {
        const QString directory = QDir::tempPath() + QStringLiteral("/sensor-dashboard-selftest");
        QDir(directory).removeRecursively();

        CsvRecorder recorder;
        expect(recorder.start(directory), "recorder creates a file");
        SensorSample sample;
        sample.timestampMs = 10;
        sample.distanceCm = 1.5;
        sample.ax = -0.25;
        sample.ay = 0.0;
        sample.az = 9.81;
        sample.gx = 1.0;
        sample.gy = 2.0;
        sample.gz = 3.0;
        sample.state = QStringLiteral("OK");
        sample.valid = true;
        expect(recorder.write(sample), "recorder writes a row");
        expect(recorder.rowCount() == 1, "recorded row count");
        const QString path = recorder.path();
        recorder.stop();
        expect(!recorder.isRecording(), "recorder stops");

        QFile file(path);
        expect(file.open(QIODevice::ReadOnly), "reopen recording");
        const QString text = QString::fromUtf8(file.readAll());
        expect(text.startsWith(QStringLiteral("timestamp_ms,distance_cm,ax,ay,az,gx,gy,gz,state\n")),
               "csv header");
        expect(text.contains(QStringLiteral("10,1.500000,-0.250000,0.000000,9.810000,1.000000,2.000000,3.000000,OK\n")),
               "csv row uses a C locale decimal point");
        file.close();
        QDir(directory).removeRecursively();
    }

    if (g_failed == 0) {
        std::cout << "Self-test passed\n";
        return 0;
    }
    std::cout << g_failed << " self-test(s) failed\n";
    return 1;
}

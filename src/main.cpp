#include "AirMonitorController.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

#include <iostream>

namespace
{
struct Options
{
    bool help = false;
    bool demo = false;
    QString host = QStringLiteral("192.168.10.2");
    int port = 5000;
    QString error;
};

Options parseOptions(int argc, char **argv)
{
    Options options;
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == QLatin1String("--help") || arg == QLatin1String("-h")) {
            options.help = true;
        } else if (arg == QLatin1String("--live") || arg == QLatin1String("--eth")
                   || arg == QLatin1String("--actual")) {
            // live is the default; accepted for clarity
        } else if (arg == QLatin1String("--demo")) {
            options.demo = true;
        } else if (arg == QLatin1String("--host")) {
            if (i + 1 >= argc) {
                options.error = QStringLiteral("--host requires an IP/hostname");
                return options;
            }
            options.host = QString::fromLocal8Bit(argv[++i]);
        } else if (arg == QLatin1String("--port")) {
            if (i + 1 >= argc) {
                options.error = QStringLiteral("--port requires a TCP port");
                return options;
            }
            bool ok = false;
            const int port = QByteArray(argv[++i]).toInt(&ok);
            if (!ok || port <= 0 || port > 65535) {
                options.error = QStringLiteral("--port must be 1..65535");
                return options;
            }
            options.port = port;
        } else {
            options.error = QStringLiteral("Unknown option: %1").arg(arg);
            return options;
        }
    }
    return options;
}

void printHelp()
{
    std::cout
        << "AIR MONITOR\n"
        << "\n"
        << "  Default is LIVE Ethernet mode (no demo values).\n"
        << "  --host <ip>     STM32 IP (default: 192.168.10.2)\n"
        << "  --port <port>   TCP port (default: 5000)\n"
        << "  --demo          Local demo values (offline UI only)\n"
        << "  --help          Show help\n"
        << "\n"
        << "STM32 CSV (LF-terminated):\n"
        << "  temp_c,hum_pct,distance_cm,zone,cpu_pct,heap_free_pct,nfc,ack_armed\n"
        << "  24.1,48.0,72,SAFE,18,62,0,0\n"
        << "Jetson acknowledge command: ACK\\n\n";
}
}

int main(int argc, char **argv)
{
    const Options options = parseOptions(argc, argv);
    if (!options.error.isEmpty()) {
        std::cerr << options.error.toStdString() << '\n';
        printHelp();
        return 2;
    }
    if (options.help) {
        printHelp();
        return 0;
    }

    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("AIR MONITOR"));
    QCoreApplication::setOrganizationName(QStringLiteral("AIR MONITOR"));
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    AirMonitorController vm(options.host, static_cast<quint16>(options.port), !options.demo);

    QQmlApplicationEngine engine;
    // Use a name that does not collide with child "property var vm" bindings.
    engine.rootContext()->setContextProperty(QStringLiteral("monitor"), &vm);

    const QUrl url(QStringLiteral("qrc:/qt/qml/AirMonitor/main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, []() { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.load(url);
    if (engine.rootObjects().isEmpty())
        return 1;

    if (vm.liveMode()) {
        std::cout << "LIVE mode -> " << vm.host().toStdString() << ':' << vm.port() << '\n';
        std::cout << "Jetson Ethernet must be on the same subnet (e.g. 192.168.10.10/24).\n";
    } else {
        std::cout << "DEMO mode\n";
    }
    return app.exec();
}

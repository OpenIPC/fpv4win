#include "src/QmlNativeAPI.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <src/QQuickRealTimePlayer.h>
#include<QQmlContext>

#pragma comment(lib, "ws2_32.lib")

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;

    qmlRegisterType<QQuickRealTimePlayer>("realTimePlayer", 1, 0, "QQuickRealTimePlayer");


    auto& qmlNativeApi = QmlNativeAPI::Instance();
    engine.rootContext()->setContextProperty("NativeApi", &qmlNativeApi);

    engine.load(QUrl(QStringLiteral("qrc:/qml/main.qml")));




    return QGuiApplication::exec();
}

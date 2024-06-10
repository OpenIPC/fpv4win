#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <src/QQuickRealTimePlayer.h>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;

    //注册播放器组件
    qmlRegisterType<QQuickRealTimePlayer>("realTimePlayer", 1, 0, "QQuickRealTimePlayer");
    engine.load(QUrl(QStringLiteral("qrc:/qml/main.qml")));

    return QGuiApplication::exec();
}

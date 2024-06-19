#include "src/QmlNativeAPI.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <player/QQuickRealTimePlayer.h>

#pragma comment(lib, "ws2_32.lib")

#ifdef DEBUG_MODE
#include <DbgHelp.h>
#pragma comment(lib, "DbgHelp.lib")
// 创建Dump文件
void CreateDumpFile(LPCWSTR lpstrDumpFilePathName, EXCEPTION_POINTERS *pException) {
    HANDLE hDumpFile = CreateFile(
        reinterpret_cast<LPCSTR>(lpstrDumpFilePathName), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    // Dump信息
    MINIDUMP_EXCEPTION_INFORMATION dumpInfo;
    dumpInfo.ExceptionPointers = pException;
    dumpInfo.ThreadId = GetCurrentThreadId();
    dumpInfo.ClientPointers = TRUE;
    // 写入Dump文件内容
    MiniDumpWriteDump(
        GetCurrentProcess(), GetCurrentProcessId(), hDumpFile, MiniDumpNormal, &dumpInfo, nullptr, nullptr);
    CloseHandle(hDumpFile);
}
// 处理Unhandled Exception的回调函数
LONG ApplicationCrashHandler(EXCEPTION_POINTERS *pException) {
    CreateDumpFile(L"dump.dmp", pException);
    return EXCEPTION_EXECUTE_HANDLER;
}

#endif

int main(int argc, char *argv[]) {
#ifdef DEBUG_MODE
    SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)ApplicationCrashHandler);
#endif

    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;

    qmlRegisterType<QQuickRealTimePlayer>("realTimePlayer", 1, 0, "QQuickRealTimePlayer");

    auto &qmlNativeApi = QmlNativeAPI::Instance();
    engine.rootContext()->setContextProperty("NativeApi", &qmlNativeApi);

    engine.load(QUrl(QStringLiteral("qrc:/qml/main.qml")));

    return QGuiApplication::exec();
}

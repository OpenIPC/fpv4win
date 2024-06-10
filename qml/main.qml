import QtQuick 2.15
import QtQuick.Controls 2.15
import realTimePlayer 1.0

ApplicationWindow {
    visible: true
    width: 640
    height: 480
    title: qsTr("")

    QQuickRealTimePlayer {
        x: 0
        y: 0
        width: parent.width
        height:parent.height
        Component.onCompleted: {
            play("rtsp://10.8.109.230:554/rtp/44050000001310000002");
        }
    }
}
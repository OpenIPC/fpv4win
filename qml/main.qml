import QtQuick 2.15
import QtQuick.Controls 2.15
import realTimePlayer 1.0

ApplicationWindow {
    visible: true
    width: 960
    height: 480
    title: qsTr("")

    QQuickRealTimePlayer {
        x: 0
        y: 0
        id: player
        width: parent.width - 200
        height:parent.height
        Component.onCompleted: {
            play("rtsp://10.8.109.230:554/rtp/44050000001310000002");
        }
    }
    Rectangle {
        x: parent.width - 200
        y: 0
        width: 200
        height: parent.height
        color: '#cccccc'


        Column {
            padding: 5
            anchors.left: parent.left

            Rectangle {
                // Size of the background adapts to the text size plus some padding
                width: 190
                height: textItem.height + 10
                color: "#1c80c9"

                Text {
                    id: textItem
                    x: 5
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Select RTL8812AU VID/PID"
                    font.pixelSize: 12
                    color: "#ffffff"
                }
            }
            ComboBox {
                id: selectDev
                width: 190
                model: ListModel {
                    id: comboBoxModel
                    Component.onCompleted: {
                        var dongleList = NativeApi.GetDongleList();
                        for (var i = 0; i < dongleList.length; i++) {
                            comboBoxModel.append({text: dongleList[i]});
                        }
                        selectDev.currentIndex = 0; // Set default selection
                    }
                }
                currentIndex: 0
            }
        }
    }
}
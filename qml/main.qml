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
                height: selDevText.height + 10
                color: "#1c80c9"

                Text {
                    id: selDevText
                    x: 5
                    anchors.verticalCenter: parent.verticalCenter
                    text: "RTL8812AU VID:PID"
                    font.pixelSize: 16
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
            Row{
                width: 190
                Column {
                    width:95
                    Rectangle {
                        // Size of the background adapts to the text size plus some padding
                        width: parent.width
                        height: selChText.height + 10
                        color: "#1c80c9"

                        Text {
                            width: parent.width
                            id: selChText
                            x: 5
                            anchors.verticalCenter: parent.verticalCenter
                            text: "Channel"
                            font.pixelSize: 16
                            color: "#ffffff"
                        }
                    }
                    ComboBox {
                        id: selectChannel
                        width: parent.width
                        model: [
                            '1','2','3','4','5','6','7','8','9','10','11','12','13',
                            '32','36','40','44','48','52','56','60','64','68','96','100','104','108','112','116','120',
                            '124','128','132','136','140','144','149','153','157','161','169','173','177'
                        ]
                        currentIndex: 39
                    }
                }
                Column {
                    width:95
                    Rectangle {
                        // Size of the background adapts to the text size plus some padding
                        width: parent.width
                        height: selCodecText.height + 10
                        color: "#1c80c9"

                        Text {
                            width: parent.width
                            id: selCodecText
                            x: 5
                            anchors.verticalCenter: parent.verticalCenter
                            text: "Codec"
                            font.pixelSize: 16
                            color: "#ffffff"
                        }
                    }
                    ComboBox {
                        id: selectCodec
                        width: parent.width
                        model: ['H264','H265']
                        currentIndex: 0
                    }
                }
            }
            Column {
                width:190
                Rectangle {
                    // Size of the background adapts to the text size plus some padding
                    width: parent.width
                    height: selBwText.height + 10
                    color: "#1c80c9"

                    Text {
                        width: parent.width
                        id: selBwText
                        x: 5
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Channel Width"
                        font.pixelSize: 16
                        color: "#ffffff"
                    }
                }
                ComboBox {
                    id: selectBw
                    width: parent.width
                    model: [
                        '20',
                        '40',
                        '80',
                        '160',
                        '80_80',
                        '5',
                        '10',
                        'MAX'
                    ]
                    currentIndex: 0
                }
            }
            Rectangle {
                // Size of the background adapts to the text size plus some padding
                width: 190
                height: actionText.height + 10
                color: "#1c80c9"

                Text {
                    id: actionText
                    x: 5
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Action"
                    font.pixelSize: 16
                    color: "#ffffff"
                }
            }
            Column {
                padding:5
                Rectangle {
                    // Size of the background adapts to the text size plus some padding
                    width: 180
                    height: actionStartText.height + 10
                    color: "#2fdcf3"
                    radius: 10

                    Text {
                        id: actionStartText
                        x: 5
                        anchors.centerIn: parent
                        text: "START"
                        font.pixelSize: 32
                        color: "#ffffff"
                    }
                }
            }
            Rectangle {
                // Size of the background adapts to the text size plus some padding
                width: 190
                height: countText.height + 10
                color: "#1c80c9"

                Text {
                    id: countText
                    x: 5
                    anchors.verticalCenter: parent.verticalCenter
                    text: "WFB Pkt / 802.11 Pkt"
                    font.pixelSize: 16
                    color: "#ffffff"
                }
            }
            Row {
                padding:5
                width: 190
                Text {
                    id: wfbPktCountText
                    x: 5
                    text: "0"
                    font.pixelSize: 32
                    color: "#000000"
                }
                Text {
                    x: 5
                    text: " / "
                    font.pixelSize: 32
                    color: "#000000"
                }
                Text {
                    id: airPktCountText
                    x: 5
                    text: "0"
                    font.pixelSize: 32
                    color: "#000000"
                }
            }
            Rectangle {
                // Size of the background adapts to the text size plus some padding
                width: 190
                height: logText.height + 10
                color: "#1c80c9"

                Text {
                    id: logText
                    x: 5
                    anchors.verticalCenter: parent.verticalCenter
                    text: "WiFi Driver Log"
                    font.pixelSize: 16
                    color: "#ffffff"
                }
            }
        }
    }
}
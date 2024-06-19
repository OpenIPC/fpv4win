import QtQuick 2.15
import QtQuick.Controls 2.15
import realTimePlayer 1.0
import Qt.labs.platform 1.1


ApplicationWindow {
    visible: true
    width: 1024
    height: 768
    id:window
    title: qsTr("")



    QQuickRealTimePlayer {
        x: 0
        y: 0
        id: player
        width: parent.width - 200
        height:parent.height
        Component.onCompleted: {
            NativeApi.onRtpStream.connect((sdpFile)=>{
                play(sdpFile)
            });
        }
        TipsBox{
            id:tips
            z:999
            tips:''
        }
        Rectangle {
            width: parent.width
            height:30
            anchors.bottom : parent.bottom
            color: Qt.rgba(0,0,0,0.3)
            border.color: "#55222222"
            border.width: 1
            Row{
                height:parent.height
                padding:5
                spacing:5
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "0bps"
                    font.pixelSize: 12
                    width:60
                    horizontalAlignment: Text.Center
                    color: "#ffffff"
                    Component.onCompleted: {
                        player.onBitrate.connect((btr)=>{
                            if(btr>1000*1000){
                                text = Number(btr/1000/1000).toFixed(2) + 'Mbps';
                            }else if(btr>1000){
                                text = Number(btr/1000).toFixed(2) + 'Kbps';
                            }else{
                                text = btr+ 'bps';
                            }
                        });
                    }
                }


            }
            Row{
                anchors.right:parent.right
                height:parent.height
                padding:5
                spacing:5
                Rectangle {
                    height:20
                    width:30
                    radius:5
                    color: "#55222222"
                    border.color: "#88ffffff"
                    border.width: 1
                    Text {
                        horizontalAlignment: Text.Center
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "JPG"
                        font.pixelSize: 12
                        color: "#ffffff"
                    }
                    MouseArea {
                        cursorShape: Qt.PointingHandCursor
                        anchors.fill: parent
                        onClicked:{
                            let f = player.captureJpeg();
                            if(f!==''){
                                tips.showPop('Saved '+f,3000);
                            }else{
                                tips.showPop('Capture failed! '+f,3000);
                            }
                        }
                    }
                }
                Rectangle {
                    height: 20
                    width: 50
                    radius: 5
                    color: "#55222222"
                    border.color: "#88ffffff"
                    border.width: 1
                    Text {
                        visible:!recordTimer.started
                        horizontalAlignment: Text.Center
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "MP4"
                        font.pixelSize: 12
                        color: "#ffffff"
                    }
                    RecordTimer{
                        id:recordTimer
                        width:parent.width
                        height: parent.height
                        property bool started:false
                        function clickEvent() {
                            if(!recordTimer.started){
                                recordTimer.started = player.startRecord();
                                if(recordTimer.started){
                                    recordTimer.start();
                                }else{
                                    tips.showPop('Record failed! ',3000);
                                }
                            }else{
                                recordTimer.started = false;
                                let f = player.stopRecord();
                                if(f!==''){
                                    tips.showPop('Saved '+f,3000);
                                }else{
                                    tips.showPop('Record failed! ',3000);
                                }
                                recordTimer.stop();
                            }
                        }
                    }
                    MouseArea {
                        cursorShape: Qt.PointingHandCursor
                        anchors.fill: parent
                        onClicked:{
                            recordTimer.clickEvent();
                        }
                    }
                }
            }
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
                        Component.onCompleted: {
                            let ch = NativeApi.GetConfig()["config.channel"];
                            if(ch&&ch!==''){
                                currentIndex = model.indexOf(ch);
                            }
                        }
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
                        model: ['AUTO','H264','H265']
                        currentIndex: 0
                        Component.onCompleted: {
                            let codec = NativeApi.GetConfig()["config.codec"];
                            if (codec&&codec !== '') {
                                currentIndex = model.indexOf(codec);
                            }
                        }
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
                    Component.onCompleted: {
                        let chw = NativeApi.GetConfig()["config.channelWidth"];
                        if (chw&&chw !== '') {
                            currentIndex = Number(chw);
                        }
                    }
                }
            }
            Rectangle {
                // Size of the background adapts to the text size plus some padding
                width: 190
                height: actionText.height + 10
                color: "#1c80c9"

                Text {
                    id: keyText
                    x: 5
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Key"
                    font.pixelSize: 16
                    color: "#ffffff"
                }
            }
            Column {
                FileDialog {
                    id: fileDialog
                    title: "Select key File"
                    nameFilters: ["Key Files (*.key)"]

                    onAccepted: {
                        keySelector.text = file;
                        keySelector.text = keySelector.text.replace('file:///','')
                    }
                }
                Button {
                    width: 190
                    id:keySelector
                    text: "gs.key"
                    onClicked: fileDialog.open()
                    Component.onCompleted: {
                        let key = NativeApi.GetConfig()["config.key"];
                        if (key && key !== '') {
                            text = key;
                        }
                    }
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
                        property bool started : false;
                        x: 5
                        anchors.centerIn: parent
                        text: started?"STOP":"START"
                        font.pixelSize: 32
                        color: "#ffffff"
                    }
                    MouseArea{
                        cursorShape: Qt.PointingHandCursor
                        anchors.fill: parent
                        Component.onCompleted: {
                            NativeApi.onWifiStop.connect(()=>{
                                actionStartText.started = false;
                                player.stop();
                            });
                        }
                        onClicked: function(){
                            if(!actionStartText.started){
                                actionStartText.started = NativeApi.Start(
                                    selectDev.currentText,
                                    Number(selectChannel.currentText),
                                    Number(selectBw.currentIndex),
                                    keySelector.text,
                                    selectCodec.currentText
                                );
                            }else{
                                NativeApi.Stop();
                                player.stop();
                                if(recordTimer.started){
                                    recordTimer.clickEvent();
                                }
                            }
                        }
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
                    text: "Packet(RTP/WFB/802.11)"
                    font.pixelSize: 16
                    color: "#ffffff"
                }
            }
            Row {
                padding:5
                width: 190
                Text {
                    id: rtpPktCountText
                    x: 5
                    text: ""+NativeApi.rtpPktCount
                    font.pixelSize: 16
                    color: "#000000"
                }
                Text {
                    x: 5
                    text: "/"
                    font.pixelSize: 16
                    color: "#000000"
                }
                Text {
                    id: wfbPktCountText
                    x: 5
                    text: ""+NativeApi.wfbFrameCount
                    font.pixelSize: 16
                    color: "#000000"
                }
                Text {
                    x: 5
                    text: "/"
                    font.pixelSize: 16
                    color: "#000000"
                }
                Text {
                    id: airPktCountText
                    x: 5
                    text: ""+NativeApi.wifiFrameCount
                    font.pixelSize: 16
                    color: "#000000"
                }
            }
            Rectangle {
                id:logTitle
                z:2
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
                    color: "#FFFFFF"
                }
            }
            Rectangle {
                width:190
                height:window.height - 430
                color:"#f3f1f1"
                clip:true

                Component {
                    id: contactDelegate
                    Item {
                        height:log.height
                        Row {
                            padding:2
                            Text {
                                id:log
                                width: 190
                                wrapMode: Text.Wrap
                                font.pixelSize: 10
                                text: '['+level+'] '+msg
                                color: {
                                    let colors = {
                                        error: "#ff0000",
                                        info: "#0f7340",
                                        warn: "#e8c538",
                                        debug: "#3296de",
                                    }
                                    return colors[level];
                                }
                            }
                        }
                    }
                }

                ListView {
                    z:1
                    anchors.top :logTitle.bottom
                    anchors.fill: parent
                    anchors.margins:5
                    model: ListModel {}
                    delegate: contactDelegate
                    Component.onCompleted: {
                        NativeApi.onLog.connect((level,msg)=>{
                            model.append({"level": level, "msg": msg});
                            positionViewAtIndex(count - 1, ListView.End)
                        });
                    }
                }
            }
        }
    }
}
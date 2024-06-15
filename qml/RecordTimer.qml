import QtQuick 2.12
import QtQuick.Controls 2.5
Rectangle{
    id:recordTimer
    visible: false
    property var startTime: 0
    property var recordLen: 0
    color:"#bb333333"
    width:childrenRect.width
    height:childrenRect.height
    radius: 5
    z:999
    Text {
        width:20
        id:redCyc
        visible: false
        padding: 5
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: time.left + 16
        text: '<font color="#ff0000" >●</font>'
        font.pixelSize: 16
        Timer {
           interval: 1000;
           running: true;
           repeat: true;
           onTriggered: ()=>{
                redCyc.visible = !redCyc.visible
           }
        }
    }
    Text {
        width:parent.width-20
        id:time
        padding: 5
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: parent.right
        text:parseInt(recordTimer.recordLen) + 'S'
        font.pixelSize: 12
        color: "#ffffff"
    }
    Timer {
       id:timer
       interval: 100;
       running: false;
       repeat: true;
       onTriggered: ()=>{
            recordTimer.recordLen = (new Date().getTime() - startTime)/1000;
       }
    }
    property var start :function(){
        recordTimer.visible = true;
        recordTimer.startTime = new Date().getTime();
        timer.start();
    }
    property var stop :function(){
        recordTimer.visible = false;
        timer.stop();
    }
}

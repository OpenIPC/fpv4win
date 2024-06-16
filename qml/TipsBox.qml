import QtQuick 2.12
import QtQuick.Controls 2.5

Rectangle{
    id:tipsBox
    visible: false
    color:"#bb333333"
    width:childrenRect.width
    height:childrenRect.height
    anchors.verticalCenter: parent.verticalCenter
    anchors.horizontalCenter: parent.horizontalCenter
    property string tips: ''
    property var timeout: 3000
    radius: 5
    property var showPop : function(msg,time){
        tips = msg;
        hideTimer.interval = time?time:timeout
        tipsBox.visible = true;
        hideTimer.restart();
    }
    property var hide : function(){
        tipsBox.visible = false;
        tipsBox.tips = ""
        hideTimer.stop();
    }
    Timer {
        id:hideTimer
        interval: tipsBox.timeout;
        running: false;
        repeat: false;
        onTriggered: ()=>{
            tipsBox.hide();
        }
     }
    Text {
        padding: 10
        anchors.verticalCenter: parent.verticalCenter
        anchors.horizontalCenter: parent.horizontalCenter
        text: tipsBox.tips
        font.pointSize: 16
        color: "#ffffff"
    }
}

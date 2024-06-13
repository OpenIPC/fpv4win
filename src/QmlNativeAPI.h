//
// Created by liangzhuohua on 2022/4/20.
//

#ifndef CTRLCENTER_QMLNATIVEAPI_H
#define CTRLCENTER_QMLNATIVEAPI_H
#include "WFBReceiver.h"
#include <QObject>
#include <QUdpSocket>
#include <fstream>

/**
 * C++封装留给qml使用的api
 */
class QmlNativeAPI : public QObject {
  Q_OBJECT
  Q_PROPERTY(qulonglong wifiFrameCount READ wifiFrameCount NOTIFY onWifiFrameCount )
  Q_PROPERTY(qulonglong wfbFrameCount READ wfbFrameCount NOTIFY onWfbFrameCount )
public:
  static QmlNativeAPI &Instance() {
    static QmlNativeAPI api;
    return api;
  }
  explicit QmlNativeAPI(QObject *parent = nullptr) : QObject(parent){};
  // get all dongle
  Q_INVOKABLE static QList<QString> GetDongleList() {
    QList<QString> l;
    for (auto &item : WFBReceiver::GetDongleList()) {
      l.push_back(QString(item.c_str()));
    }
    return l;
  };
  Q_INVOKABLE static bool Start(const QString &vidPid, int channel,
                                int channelWidth,const QString &keyPath,
                                const QString &codec) {
    // get free port
    QmlNativeAPI::Instance().playerCodec = codec;
    QmlNativeAPI::Instance().playerPort = QmlNativeAPI::Instance().GetFreePort();
    return WFBReceiver::Start(vidPid.toStdString(), channel, channelWidth,keyPath.toStdString());
  }
  Q_INVOKABLE static bool Stop() {
    return WFBReceiver::Stop();
  }
  Q_INVOKABLE static void BuildSdp(const QString &path,const QString &codec,int payloadType,int port) {
    std::ofstream sdpFos(path.toStdString());
    sdpFos<<"v=0\n";
    sdpFos<<"o=- 0 0 IN IP4 127.0.0.1\n";
    sdpFos<<"s=No Name\n";
    sdpFos<<"c=IN IP4 127.0.0.1\n";
    sdpFos<<"t=0 0\n";
    sdpFos<<"m=video "<<port<<" RTP/AVP "<<payloadType<<"\n";
    sdpFos<<"a=rtpmap:"<<payloadType<<" "<<codec.toStdString()<<"/90000\n";
    sdpFos.flush();
    sdpFos.close();
    // log
    QmlNativeAPI::Instance().PutLog("debug",
           "Build Player SDP: Codec:"+codec.toStdString()+
                   " PT:"+std::to_string(payloadType)+
                   " Port:"+std::to_string(port));
  }
  void PutLog(const std::string &level, const std::string &msg){
      emit onLog(QString(level.c_str()),QString(msg.c_str()));
  }
  void NotifyWifiStop(){
    emit onWifiStop();
  }
  void UpdateCount() {
    emit onWifiFrameCount(wifiFrameCount_);
    emit onWfbFrameCount(wfbFrameCount_);
  }
  qulonglong wfbFrameCount() {
    return wfbFrameCount_;
  }
  qulonglong wifiFrameCount() {
    return wifiFrameCount_;
  }
  Q_INVOKABLE int GetPlayerPort() {
    return playerPort;
  }
  Q_INVOKABLE QString GetPlayerCodec() const {
    return playerCodec;
  }
  int GetFreePort(){
    // bind any port
    QUdpSocket udpSocket(this);
    udpSocket.bind(QHostAddress::Any,0,QAbstractSocket::DefaultForPlatform);
    auto port = udpSocket.localPort();
    udpSocket.close();
    return port;
  }
  qulonglong wfbFrameCount_ = 0;
  qulonglong wifiFrameCount_ = 0;
  int playerPort = 0;
  QString playerCodec;
signals :
  // onlog
  void onLog(QString level, QString msg);
  void onWifiStop();
  void onWifiFrameCount(qulonglong count);
  void onWfbFrameCount(qulonglong count);
};

#endif // CTRLCENTER_QMLNATIVEAPI_H

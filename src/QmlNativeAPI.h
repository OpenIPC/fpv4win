//
// Created by liangzhuohua on 2022/4/20.
//

#ifndef CTRLCENTER_QMLNATIVEAPI_H
#define CTRLCENTER_QMLNATIVEAPI_H
#include "WFBReceiver.h"
#include <QObject>

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
                                int channelWidth,const QString &keyPath) {
    return WFBReceiver::Start(vidPid.toStdString(), channel, channelWidth,keyPath.toStdString());
  }
  Q_INVOKABLE static bool Stop() {
    return WFBReceiver::Stop();
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
  qulonglong wfbFrameCount_ = 0;
  qulonglong wifiFrameCount_ = 0;
signals :
  // onlog
  void onLog(QString level, QString msg);
  void onWifiStop();
  void onWifiFrameCount(qulonglong count);
  void onWfbFrameCount(qulonglong count);
};

#endif // CTRLCENTER_QMLNATIVEAPI_H

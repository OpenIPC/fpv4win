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
                                int channelWidth) {
    return WFBReceiver::Start(vidPid.toStdString(), channel, channelWidth);
  }
  void PutLog(const std::string &level, const std::string &msg){
      emit onLog(QString(level.c_str()),QString(msg.c_str()));
  }

signals :
  // onlog
  void onLog(QString level, QString msg);
};

#endif // CTRLCENTER_QMLNATIVEAPI_H

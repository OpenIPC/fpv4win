//
// Created by liangzhuohua on 2022/4/20.
//

#ifndef CTRLCENTER_QMLNATIVEAPI_H
#define CTRLCENTER_QMLNATIVEAPI_H
#include<QObject>
#include "WFBReceiver.h"

/**
 * C++封装留给qml使用的api
 */
class QmlNativeAPI : public QObject{
    Q_OBJECT
public:
    explicit QmlNativeAPI(QObject* parent = nullptr): QObject(parent){};
    //get all dongle
    Q_INVOKABLE static QList<QString> GetDongleList(){
      QList<QString> l;
      for (auto &item : WFBReceiver::GetDongleList()){
        l.push_back(QString(item.c_str()));
      }
      return l;
    };
    Q_INVOKABLE static bool Start(
        const QString &vidPid, int channel,int channelWidth){
      return WFBReceiver::Start(vidPid.toStdString(),channel,channelWidth);
    }
};


#endif //CTRLCENTER_QMLNATIVEAPI_H

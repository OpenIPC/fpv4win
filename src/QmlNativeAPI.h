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
    //获取本地所有IP
    Q_INVOKABLE static QList<QString> GetDongleList(){
      QList<QString> l;
      for (auto &item : WFBReceiver::GetDongleList()){
        l.push_back(QString(item.c_str()));
      }
      return l;
    };

};


#endif //CTRLCENTER_QMLNATIVEAPI_H

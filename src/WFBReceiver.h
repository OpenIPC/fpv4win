//
// Created by Talus on 2024/6/10.
//

#ifndef WFBRECEIVER_H
#define WFBRECEIVER_H
#include <string>
#include <vector>


class WFBReceiver {
public:
    static std::vector<std::string> GetDongleList();
    void Start(const std::string& vidPid);
};



#endif //WFBRECEIVER_H

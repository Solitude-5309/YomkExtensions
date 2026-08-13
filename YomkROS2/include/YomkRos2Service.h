#pragma once
#include <YomkServer/YomkAPI.h>

#include <string>

using namespace yomk;

// 定义完所有的结构体后，统一注册YomkMsg
// 占位消息包：后续 ROS2 集成时扩展
struct Ros2Msg
{
    std::string data;
};

// clang-format off
YomkMsg(Ros2Msg, YRos2Msg, req)

class YomkRos2Service : public YomkService
{
public:
    YomkRos2Service(YomkServer *server);
    virtual ~YomkRos2Service() {}
    virtual int init() override;

private:
    YomkResponse getVersion(YomkPkgPtr pkg);
};

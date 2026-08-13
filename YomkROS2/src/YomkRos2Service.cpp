#include "YomkRos2Service.h"

YomkRos2Service::YomkRos2Service(YomkServer *server)
    : YomkService(server)
{
    name("/YomkRos2Service");
}

int YomkRos2Service::init()
{
    YomkInstallFunc("/version", YomkRos2Service::getVersion);
    YOMK_INFO_TAG("YomkRos2Service::init", "install func [ /version ] to", name());
    return 0;
}

YomkResponse YomkRos2Service::getVersion(YomkPkgPtr pkg)
{
    std::string version = "YomkROS2 v" YOMKROS2_VERSION " (WIP)";
    return YomkResponse(YomkResponse::eOk, "ok", YomkMkPtr(String, version));
}

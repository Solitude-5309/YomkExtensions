#include "RpcService.h"

RpcService::RpcService(YomkServer *server)
    : YomkService(server)
{
    name("/RpcService");
}

int RpcService::init()
{
    YomkInstallFunc("/version", RpcService::getVersion);
    YomkInstallFunc("/create_node", RpcService::createNode);
    YomkInstallFunc("/delete_node", RpcService::deleteNode);
    YomkInstallFunc("/register_pub_topic", RpcService::registerPubTopic);
    YomkInstallFunc("/register_sub_topic", RpcService::registerSubTopic);
    YomkInstallFunc("/publish", RpcService::publish);
    YOMK_INFO_TAG("RpcService::init",
                  "install funcs [ /version /create_node /delete_node /register_pub_topic /register_sub_topic /publish ] to", name());

    return 0;
}

YomkResponse RpcService::getVersion(YomkPkgPtr pkg)
{
    std::string version = "YomkRpc v1.0.0 (WIP)";
    YOMK_INFO_TAG("RpcService::getVersion", version);
    return YomkResponse(YomkResponse::eOk, "ok", YomkMkPtr(String, version));
}

YomkResponse RpcService::createNode(YomkPkgPtr pkg)
{
    YomkUnPackPkgResponse(pkg, DDSNode, p);

    std::lock_guard<std::mutex> lock(mtx_);
    if (nodes_.find(p->msg.nodeName) != nodes_.end())
    {
        return YomkResponse(YomkResponse::eNo, "node [" + p->msg.nodeName + "] already exists");
    }

    auto node = std::make_unique<FastDDSManager>();
    if (!node->setDomainId(p->msg.domainId))
    {
        return YomkResponse(YomkResponse::eNo,
                            "create node [" + p->msg.nodeName + "] failed: setDomainId error");
    }
    nodes_[p->msg.nodeName] = std::move(node);

    YOMK_INFO_TAG("RpcService::createNode",
                  "node [" + p->msg.nodeName + "] created, domainId=" + std::to_string(p->msg.domainId));
    return YomkResponse(YomkResponse::eOk, "ok");
}

YomkResponse RpcService::deleteNode(YomkPkgPtr pkg)
{
    YomkUnPackPkgResponse(pkg, DDSNode, p);

    std::lock_guard<std::mutex> lock(mtx_);
    auto it = nodes_.find(p->msg.nodeName);
    if (it == nodes_.end())
    {
        return YomkResponse(YomkResponse::eNo, "node [" + p->msg.nodeName + "] not exists");
    }
    nodes_.erase(it);

    YOMK_INFO_TAG("RpcService::deleteNode", "node [" + p->msg.nodeName + "] deleted");
    return YomkResponse(YomkResponse::eOk, "ok");
}

YomkResponse RpcService::registerPubTopic(YomkPkgPtr pkg)
{
    YomkUnPackPkgResponse(pkg, DDSTopic, p);

    std::lock_guard<std::mutex> lock(mtx_);
    auto it = nodes_.find(p->msg.nodeName);
    if (it == nodes_.end())
    {
        return YomkResponse(YomkResponse::eNo, "node [" + p->msg.nodeName + "] not exists");
    }
    if (!it->second->registerPubTopic(p->msg.topicName, p->msg.type))
    {
        return YomkResponse(YomkResponse::eNo,
                            "registerPubTopic [" + p->msg.topicName + "] failed on node [" + p->msg.nodeName + "]");
    }

    YOMK_INFO_TAG("RpcService::registerPubTopic",
                  "node [" + p->msg.nodeName + "] register pub topic [" + p->msg.topicName + "]");
    return YomkResponse(YomkResponse::eOk, "ok");
}

YomkResponse RpcService::registerSubTopic(YomkPkgPtr pkg)
{
    YomkUnPackPkgResponse(pkg, DDSSubRequest, p);

    std::lock_guard<std::mutex> lock(mtx_);
    auto it = nodes_.find(p->msg.nodeName);
    if (it == nodes_.end())
    {
        return YomkResponse(YomkResponse::eNo, "node [" + p->msg.nodeName + "] not exists");
    }
    if (!it->second->registerSubTopic(p->msg.topicName, p->msg.type, p->msg.data, p->msg.callback))
    {
        return YomkResponse(YomkResponse::eNo,
                            "registerSubTopic [" + p->msg.topicName + "] failed on node [" + p->msg.nodeName + "]");
    }

    YOMK_INFO_TAG("RpcService::registerSubTopic",
                  "node [" + p->msg.nodeName + "] register sub topic [" + p->msg.topicName + "]");
    return YomkResponse(YomkResponse::eOk, "ok");
}

YomkResponse RpcService::publish(YomkPkgPtr pkg)
{
    YomkUnPackPkgResponse(pkg, DDSPublish, p);

    std::lock_guard<std::mutex> lock(mtx_);
    auto it = nodes_.find(p->msg.nodeName);
    if (it == nodes_.end())
    {
        return YomkResponse(YomkResponse::eNo, "node [" + p->msg.nodeName + "] not exists");
    }
    if (!it->second->publish(p->msg.topicName, p->msg.data))
    {
        return YomkResponse(YomkResponse::eNo,
                            "publish [" + p->msg.topicName + "] failed on node [" + p->msg.nodeName + "]");
    }

    return YomkResponse(YomkResponse::eOk, "ok");
}

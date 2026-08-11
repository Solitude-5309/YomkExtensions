#include "YomkRpcService.h"

YomkRpcService::YomkRpcService(YomkServer *server)
    : YomkService(server)
{
    name("/YomkRpcService");
}

int YomkRpcService::init()
{
    YomkInstallFunc("/version", YomkRpcService::getVersion);
    YomkInstallFunc("/create_node", YomkRpcService::createNode);
    YomkInstallFunc("/delete_node", YomkRpcService::deleteNode);
    YomkInstallFunc("/register_pub_topic", YomkRpcService::registerPubTopic);
    YomkInstallFunc("/register_sub_topic", YomkRpcService::registerSubTopic);
    YomkInstallFunc("/publish", YomkRpcService::publish);
    YOMK_INFO_TAG("YomkRpcService::init",
                  "install funcs [ /version /create_node /delete_node /register_pub_topic /register_sub_topic /publish ] to", name());

    return 0;
}

YomkResponse YomkRpcService::getVersion(YomkPkgPtr pkg)
{
    std::string version = "YomkRpc v1.0.0 (WIP)";
    YOMK_INFO_TAG("YomkRpcService::getVersion", version);
    return YomkResponse(YomkResponse::eOk, "ok", YomkMkPtr(String, version));
}

YomkResponse YomkRpcService::createNode(YomkPkgPtr pkg)
{
    YomkUnPackPkgResponse(pkg, DDSNode, p);

    std::lock_guard<std::mutex> lock(mtx_);
    if (nodes_.find(p->msg.nodeName) != nodes_.end())
    {
        return YomkResponse(YomkResponse::eNo, "node [" + p->msg.nodeName + "] already exists");
    }

    auto node = std::make_unique<FastDDSNode>();
    if (!node->setDomainId(p->msg.domainId))
    {
        return YomkResponse(YomkResponse::eNo,
                            "create node [" + p->msg.nodeName + "] failed: setDomainId error");
    }
    nodes_[p->msg.nodeName] = std::move(node);

    YOMK_INFO_TAG("YomkRpcService::createNode",
                  "node [" + p->msg.nodeName + "] created, domainId=" + std::to_string(p->msg.domainId));
    return YomkResponse(YomkResponse::eOk, "ok");
}

YomkResponse YomkRpcService::deleteNode(YomkPkgPtr pkg)
{
    YomkUnPackPkgResponse(pkg, DDSNode, p);

    std::lock_guard<std::mutex> lock(mtx_);
    auto it = nodes_.find(p->msg.nodeName);
    if (it == nodes_.end())
    {
        return YomkResponse(YomkResponse::eNo, "node [" + p->msg.nodeName + "] not exists");
    }
    nodes_.erase(it);

    YOMK_INFO_TAG("YomkRpcService::deleteNode", "node [" + p->msg.nodeName + "] deleted");
    return YomkResponse(YomkResponse::eOk, "ok");
}

YomkResponse YomkRpcService::registerPubTopic(YomkPkgPtr pkg)
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

    YOMK_INFO_TAG("YomkRpcService::registerPubTopic",
                  "node [" + p->msg.nodeName + "] register pub topic [" + p->msg.topicName + "]");
    return YomkResponse(YomkResponse::eOk, "ok");
}

YomkResponse YomkRpcService::registerSubTopic(YomkPkgPtr pkg)
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

    YOMK_INFO_TAG("YomkRpcService::registerSubTopic",
                  "node [" + p->msg.nodeName + "] register sub topic [" + p->msg.topicName + "]");
    return YomkResponse(YomkResponse::eOk, "ok");
}

YomkResponse YomkRpcService::publish(YomkPkgPtr pkg)
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

#pragma once
#include <YomkRpc/YomkRpcService.h>

#define YOMKRPC_NODE(domainId, nodeName) \
    YOMK_REQUEST("/YomkRpcService/create_node", YomkMkPtr(DDSNode, DDSNode{domainId, nodeName}))

#define YOMKRPC_VERSION()                                                          \
    do                                                                             \
    {                                                                              \
        auto __resp = YOMK_REQUEST("/YomkRpcService/version", nullptr);            \
        if (__resp.m_status == YomkResponse::eOk)                                  \
        {                                                                          \
            YomkUnPackPkg(__resp.m_data, String, __ver);                           \
            if (__ver)                                                             \
            {                                                                      \
                YOMK_INFO_TAG("YomkRpcService", __ver->d);                         \
            }                                                                      \
        }                                                                          \
        else                                                                       \
        {                                                                          \
            YOMK_ERROR_TAG("YomkRpcService", "getVersion failed: ", __resp.m_msg); \
        }                                                                          \
    } while (0)

#define YOMKRPC_PUB_TOPIC(nodeName, topicName, type) \
    YOMK_REQUEST("/YomkRpcService/register_pub_topic", YomkMkPtr(DDSTopic, DDSTopic{nodeName, topicName, type}))

#define YOMKRPC_SUB_TOPIC(nodeName, topicName, type, callback) \
    YOMK_REQUEST("/YomkRpcService/register_sub_topic", YomkMkPtr(DDSSubRequest, DDSSubRequest{nodeName, topicName, type, callback}))

#define YOMKRPC_PUB_MSG(nodeName, topicName, data) \
    YOMK_REQUEST("/YomkRpcService/publish", YomkMkPtr(DDSPublish, DDSPublish{nodeName, topicName, data}))

#define YOMKRPC_DEL_NODE(nodeName) \
    YOMK_REQUEST("/YomkRpcService/delete_node", YomkMkPtr(String, nodeName))

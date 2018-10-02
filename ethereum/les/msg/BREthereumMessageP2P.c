//
//  BREthereumMessageP2P.c
//  Core
//
//  Created by Ed Gamble on 9/1/18.
//  Copyright (c) 2018 breadwallet LLC
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.

#include "BREthereumMessageP2P.h"

/// MARK: - P2P (Peer-to-Peer) Messages

static const char *messageP2PNames[] = {
    "Hello",
    "Disconnect",
    "Ping",
    "Pong" };

extern const char *
messageP2PGetIdentifierName (BREthereumP2PMessageIdentifier identifier) {
    return messageP2PNames [identifier];
}

//
// P2P Hello
//
extern BRRlpItem
messageP2PHelloEncode (BREthereumP2PMessageHello message, BREthereumMessageCoder coder) {
    size_t capsCount = array_count(message.capabilities);
    BRRlpItem capItems[capsCount];

    for(int i = 0; i < capsCount; ++i)
        capItems[i] = rlpEncodeList (coder.rlp, 2,
                                     rlpEncodeString (coder.rlp, message.capabilities[i].name),
                                     rlpEncodeUInt64 (coder.rlp, message.capabilities[i].version, 1));

    BRRlpItem capsItem = rlpEncodeListItems(coder.rlp, capItems, capsCount);

    return rlpEncodeList (coder.rlp, 5,
                          rlpEncodeUInt64 (coder.rlp, message.version, 1),
                          rlpEncodeString (coder.rlp, message.clientId),
                          capsItem,
                          rlpEncodeUInt64 (coder.rlp, 0x00, 1),
                          rlpEncodeBytes (coder.rlp, message.nodeId.u8, sizeof (message.nodeId.u8)));
}

extern BREthereumP2PMessageHello
messageP2PHelloDecode (BRRlpItem item,
                       BREthereumMessageCoder coder,
                       BREthereumBoolean *failed) {
    size_t itemsCount = 0;
    const BRRlpItem *items = rlpDecodeList (coder.rlp, item, &itemsCount);
    if (5 != itemsCount) { *failed = ETHEREUM_BOOLEAN_TRUE; return (BREthereumP2PMessageHello) {}; }

    BREthereumP2PMessageHello message = {
        rlpDecodeUInt64 (coder.rlp, items[0], 1),
        rlpDecodeString (coder.rlp, items[1]),
        NULL,
        rlpDecodeUInt64 (coder.rlp, items[3], 1),
    };

    BRRlpData nodeData = rlpDecodeBytesSharedDontRelease (coder.rlp, items[4]);
    assert (sizeof (message.nodeId.u8) == nodeData.bytesCount);
    memcpy (message.nodeId.u8, nodeData.bytes, nodeData.bytesCount);

    size_t capsCount = 0;
    const BRRlpItem *capItems = rlpDecodeList (coder.rlp, items[2], &capsCount);
    array_new (message.capabilities, capsCount);

    for (size_t index = 0; index < capsCount; index++) {
        size_t capCount;
        const BRRlpItem *caps = rlpDecodeList (coder.rlp, capItems[index], &capCount);
        assert (2 == capCount);

        BREthereumP2PCapability cap;

        char *name = rlpDecodeString (coder.rlp, caps[0]);
        // We've seen 'hive' here - restrict to 3
        strncpy (cap.name, name, 3);
        cap.name[3] = '\0';

        cap.version = (uint32_t) rlpDecodeUInt64 (coder.rlp, caps[1], 1);

        array_add (message.capabilities, cap);
    }

    return message;
}

extern void
messageP2PHelloShow (BREthereumP2PMessageHello hello) {
    size_t nodeIdLen = 2 * sizeof (hello.nodeId.u8) + 1;
    char nodeId[nodeIdLen];
    encodeHex(nodeId, nodeIdLen, hello.nodeId.u8, sizeof (hello.nodeId.u8));

    eth_log (LES_LOG_TOPIC, "Hello%s", "");
    eth_log (LES_LOG_TOPIC, "    Version     : %llu", hello.version);
    eth_log (LES_LOG_TOPIC, "    ClientId    : %s",   hello.clientId);
    eth_log (LES_LOG_TOPIC, "    ListenPort  : %llu", hello.port);
    eth_log (LES_LOG_TOPIC, "    NodeId      : 0x%s", nodeId);
    eth_log (LES_LOG_TOPIC, "    Capabilities:%s", "");
    for (size_t index = 0; index < array_count(hello.capabilities); index++)
        eth_log (LES_LOG_TOPIC, "        %s = %u",
                 hello.capabilities[index].name,
                 hello.capabilities[index].version);
}

extern BREthereumBoolean
messageP2PCababilityEqual (const BREthereumP2PCapability *cap1,
                           const BREthereumP2PCapability *cap2) {
    return AS_ETHEREUM_BOOLEAN (0 == strcmp (cap1->name, cap2->name) &&
                                cap1->version == cap2->version);
}

extern BREthereumBoolean
messageP2PHelloHasCapability (const BREthereumP2PMessageHello *hello,
                              const BREthereumP2PCapability *capability) {
    for (size_t index = 0; index < array_count (hello->capabilities); index++)
        if (ETHEREUM_BOOLEAN_IS_TRUE(messageP2PCababilityEqual(capability, &hello->capabilities[index])))
            return ETHEREUM_BOOLEAN_TRUE;
    return ETHEREUM_BOOLEAN_FALSE;
}

static const char *disconnectReasonNames[] = {
    "Requested",
    "TCP Error",
    "Breach Proto",
    "Useless Peer",
    "Too Many Peers",
    "Already Connected",
    "Incompatible P2P",
    "Null Node",
    "Client Quit",
    "Unexpected ID",
    "ID Same",
    "Timeout",
    "", // 0x0c
    "", // 0x0d
    "", // 0x0e
    "", // 0x0f
    "Unknown"
};
extern const char *
messageP2PDisconnectDescription (BREthereumP2PDisconnectReason identifier) {
    return disconnectReasonNames [identifier];
}

//
static BREthereumP2PMessageDisconnect
messageP2PDisconnectDecode (BRRlpItem item, BREthereumMessageCoder coder) {
    BREthereumP2PMessageDisconnect disconnect;
//#if P2P_MESSAGE_VERSION == 0x04
    size_t itemsCount = 0;
    const BRRlpItem *items = rlpDecodeList (coder.rlp, item, &itemsCount);
    assert (1 == itemsCount);

    disconnect.reason = (BREthereumP2PDisconnectReason) rlpDecodeUInt64 (coder.rlp, items[0], 1);
//#elif P2P_MESSAGE_VERSION == 0x05
//    disconnect.reason = (BREthereumP2PDisconnectReason) rlpDecodeUInt64 (coder.rlp, item, 1);
//#endif
    return disconnect;
}

//
// P2P
//
extern BRRlpItem
messageP2PEncode (BREthereumP2PMessage message, BREthereumMessageCoder coder) {
    BRRlpItem messageBody = NULL;
    switch (message.identifier) {
        case P2P_MESSAGE_HELLO:
            messageBody = messageP2PHelloEncode(message.u.hello, coder);
            break;
        case P2P_MESSAGE_PING:
        case P2P_MESSAGE_PONG:
            messageBody = rlpEncodeList (coder.rlp, 0);
            break;
        case P2P_MESSAGE_DISCONNECT:
            messageBody = rlpEncodeUInt64 (coder.rlp, message.u.disconnect.reason, 1);
            break;
    }
    BRRlpItem identifierItem = rlpEncodeUInt64 (coder.rlp, message.identifier, 1);

    return rlpEncodeList2 (coder.rlp, identifierItem, messageBody);
}

extern BREthereumP2PMessage
messageP2PDecode (BRRlpItem item,
                  BREthereumMessageCoder coder,
                  BREthereumBoolean *failed,
                  BREthereumP2PMessageIdentifier identifer) {
    switch (identifer) {
        case P2P_MESSAGE_HELLO:
            return (BREthereumP2PMessage) {
                P2P_MESSAGE_HELLO,
                { .hello = messageP2PHelloDecode (item, coder, failed) }
            };

        case P2P_MESSAGE_PING:
            return (BREthereumP2PMessage) {
                P2P_MESSAGE_PING,
                { .ping = {}}
            };

        case P2P_MESSAGE_PONG:
            return (BREthereumP2PMessage) {
                P2P_MESSAGE_PONG,
                { .pong = {} }
            };

        case P2P_MESSAGE_DISCONNECT:
            return (BREthereumP2PMessage) {
                P2P_MESSAGE_DISCONNECT,
                { .disconnect = messageP2PDisconnectDecode (item, coder) }
            };
    }
}

/// MARK: - P2P Message Status Faker

extern int
messageP2PStatusExtractValue (BREthereumP2PMessageStatus *status,
                              BREthereumP2MessageStatusKey key,
                              BREthereumP2PMessageStatusValue *value) {
    if (NULL != status->pairs && NULL != value)
        for (size_t index = 0; index < array_count (status->pairs); index++)
            if (key == status->pairs[index].key) {
                *value = status->pairs[index].value;
                return 1;
            }
    return 0;
}

extern void
messageP2PStatusShow(BREthereumP2PMessageStatus *message) {
    BREthereumHashString headHashString, genesisHashString;
    hashFillString (message->headHash, headHashString);
    hashFillString (message->genesisHash, genesisHashString);

    char *headTotalDifficulty = coerceString (message->headTd, 10);

    BREthereumP2PMessageStatusValue value;

    eth_log (LES_LOG_TOPIC, "StatusMessage:%s", "");
    eth_log (LES_LOG_TOPIC, "    ProtocolVersion: %llu", message->protocolVersion);
    if (messageP2PStatusExtractValue(message, P2P_MESSAGE_STATUS_ANNOUNCE_TYPE, &value))
        eth_log (LES_LOG_TOPIC, "    announceType   : %llu", value.u.integer);
    eth_log (LES_LOG_TOPIC, "    NetworkId      : %llu", message->chainId);
    eth_log (LES_LOG_TOPIC, "    HeadNum        : %llu", message->headNum);
    eth_log (LES_LOG_TOPIC, "    HeadHash       : %s",   headHashString);
    eth_log (LES_LOG_TOPIC, "    HeadTd         : %s",   headTotalDifficulty);
    eth_log (LES_LOG_TOPIC, "    GenesisHash    : %s",   genesisHashString);

    free (headTotalDifficulty);


    if (messageP2PStatusExtractValue(message, P2P_MESSAGE_STATUS_SERVE_HEADERS, &value))
        eth_log (LES_LOG_TOPIC, "    ServeHeaders   : %s",
                 (ETHEREUM_BOOLEAN_IS_TRUE(value.u.boolean) ? "Yes" : "No"));

    if (messageP2PStatusExtractValue(message, P2P_MESSAGE_STATUS_SERVE_CHAIN_SINCE, &value))
        eth_log (LES_LOG_TOPIC, "    ServeChainSince: %llu", value.u.integer);

    if (messageP2PStatusExtractValue(message, P2P_MESSAGE_STATUS_SERVE_STATE_SINCE, &value))
        eth_log (LES_LOG_TOPIC, "    ServeStateSince: %llu", value.u.integer);

    if (messageP2PStatusExtractValue(message, P2P_MESSAGE_STATUS_TX_RELAY, &value))
        eth_log (LES_LOG_TOPIC, "    TxRelay        : %s",
                 (ETHEREUM_BOOLEAN_IS_TRUE(value.u.boolean) ? "Yes" : "No"));

    if (messageP2PStatusExtractValue(message, P2P_MESSAGE_STATUS_FLOW_CONTROL_BL, &value))
        eth_log (LES_LOG_TOPIC, "    FlowControl/BL : %llu", value.u.integer);

    if (messageP2PStatusExtractValue(message, P2P_MESSAGE_STATUS_FLOW_CONTROL_MRR, &value))
        eth_log (LES_LOG_TOPIC, "    FlowControl/MRR: %llu", value.u.integer);

#if 0
    size_t count = (NULL == message->flowControlMRCCount ? 0 : *(message->flowControlMRCCount));
    if (count != 0) {
        eth_log (LES_LOG_TOPIC, "    FlowControl/MRC:%s", "");
        for (size_t index = 0; index < count; index++) {
            const char *label = messageLESGetIdentifierName ((BREthereumLESMessageIdentifier) message->flowControlMRC[index].msgCode);
            if (NULL != label) {
                eth_log (LES_LOG_TOPIC, "      %2d", (BREthereumLESMessageIdentifier) message->flowControlMRC[index].msgCode);
                eth_log (LES_LOG_TOPIC, "        Request : %s", label);
                eth_log (LES_LOG_TOPIC, "        BaseCost: %llu", message->flowControlMRC[index].baseCost);
                eth_log (LES_LOG_TOPIC, "        ReqCost : %llu", message->flowControlMRC[index].reqCost);
            }
        }
    }
#endif
}

extern BRRlpItem
messageP2PStatusEncode (BREthereumP2PMessageStatus *status,
                        BREthereumMessageCoder coder) {

    size_t index = 0;
    BRRlpItem items[15];

    items[index++] = rlpEncodeList2 (coder.rlp,
                                     rlpEncodeString(coder.rlp, "protocolVersion"),
                                     rlpEncodeUInt64(coder.rlp, status->protocolVersion,1));

    items[index++] = rlpEncodeList2 (coder.rlp,
                                     rlpEncodeString(coder.rlp, "networkId"),
                                     rlpEncodeUInt64(coder.rlp, status->chainId,1));

    items[index++] = rlpEncodeList2 (coder.rlp,
                                     rlpEncodeString(coder.rlp, "headTd"),
                                     rlpEncodeUInt256(coder.rlp, status->headTd,1));

    items[index++] = rlpEncodeList2(coder.rlp,
                                    rlpEncodeString(coder.rlp, "headHash"),
                                    hashRlpEncode(status->headHash, coder.rlp));

    items[index++] = rlpEncodeList2(coder.rlp,
                                    rlpEncodeString(coder.rlp, "headNum"),
                                    rlpEncodeUInt64(coder.rlp, status->headNum,1));

    items[index++] = rlpEncodeList2 (coder.rlp,
                                     rlpEncodeString(coder.rlp, "genesisHash"),
                                     hashRlpEncode(status->genesisHash, coder.rlp));

    for (size_t index = 0; NULL != status->pairs && index < array_count(status->pairs); index++) {
        BREthereumP2PMessageStatusKeyValuePair *pair = &status->pairs[index];
        switch (pair->key) {
            case P2P_MESSAGE_STATUS_PROTOCOL_VERSION:
            case P2P_MESSAGE_STATUS_NETWORK_ID:
            case P2P_MESSAGE_STATUS_HEAD_TD:
            case P2P_MESSAGE_STATUS_HEAD_HASH:
            case P2P_MESSAGE_STATUS_HEAD_NUM:
            case P2P_MESSAGE_STATUS_GENESIS_HASH:
                break;

            case P2P_MESSAGE_STATUS_SERVE_HEADERS:
                items[index++] = rlpEncodeList1 (coder.rlp,
                                                 rlpEncodeString(coder.rlp, "serveHeaders"));
                break;
            case P2P_MESSAGE_STATUS_SERVE_CHAIN_SINCE:
                items[index++] = rlpEncodeList2 (coder.rlp,
                                                 rlpEncodeString(coder.rlp, "serveChainSince"),
                                                 rlpEncodeUInt64(coder.rlp, pair->value.u.integer, 1));
                break;
            case P2P_MESSAGE_STATUS_SERVE_STATE_SINCE:
                items[index++] = rlpEncodeList2 (coder.rlp,
                                                 rlpEncodeString(coder.rlp, "serveStateSince"),
                                                 rlpEncodeUInt64(coder.rlp,  pair->value.u.integer,1));
                break;
            case P2P_MESSAGE_STATUS_TX_RELAY:
                items[index++] = rlpEncodeList1 (coder.rlp, rlpEncodeString(coder.rlp, "txRelay"));
                break;

            case P2P_MESSAGE_STATUS_FLOW_CONTROL_BL:
                items[index++] = rlpEncodeList2 (coder.rlp,
                                                 rlpEncodeString(coder.rlp, "flowControl/BL"),
                                                 rlpEncodeUInt64(coder.rlp, pair->value.u.integer,1));
                break;
            case P2P_MESSAGE_STATUS_FLOW_CONTROL_MRC:
                assert (0);
#if 0
                if(status->flowControlBL != NULL) {
                    size_t count = *(status->flowControlMRCCount);
                    BRRlpItem mrcItems[count];

                    for(int idx = 0; idx < count; ++idx){
                        BRRlpItem mrcElements [3];
                        mrcElements[0] = rlpEncodeUInt64(coder.rlp,status->flowControlMRC[idx].msgCode,1);
                        mrcElements[1] = rlpEncodeUInt64(coder.rlp,status->flowControlMRC[idx].baseCost,1);
                        mrcElements[2] = rlpEncodeUInt64(coder.rlp,status->flowControlMRC[idx].reqCost,1);
                        mrcItems[idx] = rlpEncodeListItems(coder.rlp, mrcElements, 3);
                    }

                    items[index++] = rlpEncodeList2 (coder.rlp,
                                                     rlpEncodeString(coder.rlp, "flowControl/MRC"),
                                                     rlpEncodeListItems(coder.rlp, mrcItems, count));
                }
#endif
                break;

            case P2P_MESSAGE_STATUS_FLOW_CONTROL_MRR:
                items[index++] = rlpEncodeList2 (coder.rlp,
                                                 rlpEncodeString(coder.rlp, "flowControl/MRR"),
                                                 rlpEncodeUInt64(coder.rlp, pair->value.u.integer,1));

                break;
            case P2P_MESSAGE_STATUS_ANNOUNCE_TYPE:
                items[index++] = rlpEncodeList2 (coder.rlp,
                                                 rlpEncodeString(coder.rlp, "announceType"),
                                                 rlpEncodeUInt64(coder.rlp, pair->value.u.integer,1));
                break;
        }
    }

    return rlpEncodeListItems (coder.rlp, items, index);
}

extern BREthereumP2PMessageStatus
messageP2PStatusDecode (BRRlpItem item,
                        BREthereumMessageCoder coder) {
    BREthereumP2PMessageStatus status = {};

    size_t itemsCount = 0;
    const BRRlpItem *items = rlpDecodeList (coder.rlp, item, &itemsCount);

    array_new (status.pairs, 1);

    for(int i= 0; i < itemsCount; ++i) {
        size_t keyPairCount;
        const BRRlpItem *keyPairs = rlpDecodeList (coder.rlp, items[i], &keyPairCount);
        if (keyPairCount > 0) {
            char *key = rlpDecodeString(coder.rlp, keyPairs[0]);

            if (strcmp(key, "protocolVersion") == 0) {
                status.protocolVersion = rlpDecodeUInt64(coder.rlp, keyPairs[1], 1);
            }
            else if (strcmp(key, "networkId") == 0) {
                status.chainId = rlpDecodeUInt64(coder.rlp, keyPairs[1], 1);
            }
            else if (strcmp(key, "headTd") == 0) {
                status.headTd = rlpDecodeUInt256(coder.rlp, keyPairs[1], 1);
            }
            else if (strcmp(key, "headHash") == 0) {
                status.headHash = hashRlpDecode(keyPairs[1], coder.rlp);
            }
            else if (strcmp(key, "headNum") == 0) {
                status.headNum = rlpDecodeUInt64(coder.rlp, keyPairs[1], 1);
            }
            else if (strcmp(key, "genesisHash") == 0) {
                status.genesisHash = hashRlpDecode(keyPairs[1], coder.rlp);
            }
            // Optional

            else if (strcmp(key, "announceType") == 0) {
                BREthereumP2PMessageStatusKeyValuePair pair = {
                    P2P_MESSAGE_STATUS_ANNOUNCE_TYPE,
                    { P2P_MESSAGE_STATUS_VALUE_INTEGER,
                        { .integer = rlpDecodeUInt64(coder.rlp, keyPairs[1], 1) }}
                };
                array_add (status.pairs, pair);
            }

            else if (strcmp(key, "serveHeaders") == 0) {
                BREthereumP2PMessageStatusKeyValuePair pair = {
                    P2P_MESSAGE_STATUS_SERVE_HEADERS,
                    { P2P_MESSAGE_STATUS_VALUE_BOOLEAN,
                        { .boolean = ETHEREUM_BOOLEAN_TRUE }}
                };
                array_add (status.pairs, pair);
            }
            else if (strcmp(key, "serveChainSince") == 0) {
                BREthereumP2PMessageStatusKeyValuePair pair = {
                    P2P_MESSAGE_STATUS_SERVE_CHAIN_SINCE,
                    { P2P_MESSAGE_STATUS_VALUE_INTEGER,
                        { .integer = rlpDecodeUInt64(coder.rlp, keyPairs[1], 1) }}
                };
                array_add (status.pairs, pair);
            }
            else if (strcmp(key, "serveStateSince") == 0) {
                BREthereumP2PMessageStatusKeyValuePair pair = {
                    P2P_MESSAGE_STATUS_SERVE_STATE_SINCE,
                    { P2P_MESSAGE_STATUS_VALUE_INTEGER,
                        { .integer = rlpDecodeUInt64(coder.rlp, keyPairs[1], 1) }}
                };
                array_add (status.pairs, pair);
            }
            else if (strcmp(key, "txRelay") == 0) {
                BREthereumP2PMessageStatusKeyValuePair pair = {
                    P2P_MESSAGE_STATUS_TX_RELAY,
                    { P2P_MESSAGE_STATUS_VALUE_BOOLEAN,
                        { .integer = ETHEREUM_BOOLEAN_TRUE }}
                };
                array_add (status.pairs, pair);
            }
            else if (strcmp(key, "flowControl/BL") == 0) {
                BREthereumP2PMessageStatusKeyValuePair pair = {
                    P2P_MESSAGE_STATUS_FLOW_CONTROL_BL,
                    { P2P_MESSAGE_STATUS_VALUE_INTEGER,
                        { .integer = rlpDecodeUInt64(coder.rlp, keyPairs[1], 1) }}
                };
                array_add (status.pairs, pair);

            }
#if 0
            else if (strcmp(key, "flowControl/MRC") == 0) {
                // TODO: Wrong for PIP
                status.flowControlMRR = malloc(sizeof(uint64_t));
                size_t mrrItemsCount  = 0;
                const BRRlpItem* mrrItems = rlpDecodeList(coder.rlp, keyPairs[1], &mrrItemsCount);
                BREthereumLESMessageStatusMRC *mrcs = NULL;
                if(mrrItemsCount > 0){
                    mrcs = (BREthereumLESMessageStatusMRC*) calloc (mrrItemsCount, sizeof(BREthereumLESMessageStatusMRC));
                    for(int mrrIdx = 0; mrrIdx < mrrItemsCount; ++mrrIdx){
                        size_t mrrElementsCount  = 0;
                        const BRRlpItem* mrrElements = rlpDecodeList(coder.rlp, mrrItems[mrrIdx], &mrrElementsCount);
                        mrcs[mrrIdx].msgCode  = rlpDecodeUInt64(coder.rlp, mrrElements[0], 1);
                        mrcs[mrrIdx].baseCost = rlpDecodeUInt64(coder.rlp, mrrElements[1], 1);
                        mrcs[mrrIdx].reqCost  = rlpDecodeUInt64(coder.rlp, mrrElements[2], 1);
                    }
                }
                status.flowControlMRCCount = malloc (sizeof (size_t));
                *status.flowControlMRCCount = mrrItemsCount;
                status.flowControlMRC = mrcs;
            }
#endif
            else if (strcmp(key, "flowControl/MRR") == 0) {
                BREthereumP2PMessageStatusKeyValuePair pair = {
                    P2P_MESSAGE_STATUS_FLOW_CONTROL_MRR,
                    { P2P_MESSAGE_STATUS_VALUE_INTEGER,
                        { .integer = rlpDecodeUInt64(coder.rlp, keyPairs[1], 1) }}
                };
                array_add (status.pairs, pair);
            }
            free (key);
        }
    }
    return status;
}



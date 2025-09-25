//@file network.h

#ifndef NETWORK_H
#define NETWORK_H

#include "chat_room.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 初始化网络连接
 * @return 0:成功 -1:失败
 */
int Network_Init(void);

/**
 * 发送数据
 * @param data 要发送的数据
 * @return 发送的字节数，-1表示失败
 */
int Network_Send_Data(ChatData_t *data);

/**
 * 接收数据
 * @param data 接收数据的缓冲区
 * @return 接收的字节数，-1表示失败
 */
int Network_Recv_Data(ChatData_t *data);

/**
 * 发送UDP数据
 * @param data 要发送的数据
 * @param to 目标地址
 * @return 发送的字节数，-1表示失败
 */
int Network_Send_UDP(ChatData_t *data, struct sockaddr_in *to);

/**
 * 接收UDP数据
 * @param data 接收数据的缓冲区
 * @param from 发送者地址
 * @return 接收的字节数，-1表示失败
 */
int Network_Recv_UDP(ChatData_t *data, struct sockaddr_in *from);

/**
 * 关闭网络连接
 */
void Network_Close(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* NETWORK_H */

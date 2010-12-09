#ifndef PubSub_mbed_h
#define PubSub_mbed_h

#include "lwip/opt.h"
#include "lwip/stats.h"
#include "lwip/sys.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"
#include "lwip/tcp.h"
#include "lwip/dns.h"
#include "lwip/dhcp.h"
#include "lwip/init.h"
#include "lwip/netif.h"
#include "netif/etharp.h"
#include "netif/loopif.h"
#include "device.h"
#include "mbed.h"

#define MQTTCONNECT 1<<4
#define MQTTPUBLISH 3<<4
#define MQTTSUBSCRIBE 8<<4

#define MAX_PACKET_SIZE 128

#define KEEPALIVE 15000

typedef err_t (*tcp_accept_fn)(void *arg, struct tcp_pcb *newpcb, err_t err);

class PubSub_mbed {

private:
    Timer timer;
    struct ip_addr server;
    int port;
    int connected;
    struct tcp_pcb *pcb;
    uint8_t buffer[MAX_PACKET_SIZE];
    uint8_t nextMsgId;
    int lastActivity;
    tcp_accept_fn tcp_fun;
    //err_t (*callback)(void *arg, struct tcp_pcd *newpcb, err_t err);
    //err_t (*tcp_accept_fn)(void *arg, struct tcp_pcb *newpcb, err_t err);
    //err_t (*callback)(void*, struct tcp_pcb*, struct pbuf*, err_t)
   

public:
    //err_t recv_callback(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
    //err_t (*callback)(void*, struct tcp_pcb*, struct pbuf*, err_t)
    PubSub_mbed(struct ip_addr serverIp, int port, tcp_accept_fn fn);
    int connect(char *);
    void disconnect();
    int publish(char *, char *);
    void subscribe(char *);
    int live();
    
    //int connected();
};

#endif
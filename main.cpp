#include "mbed.h"
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

#include "PubSub_mbed.h"


Ethernet ethernet;

char buffer[1024];
char temp[1024];

int flag;

struct netif netif_data;

DigitalOut ledStage0 (LED1);
DigitalOut ledStage1 (LED2);
DigitalOut ledStage2 (LED3);
DigitalOut ledStage3 (LED4);

volatile char stage = 0;

Ticker stage_blinker;

void stageblinker()
{
   switch (stage)
    {
        case 0:
            ledStage0 = !ledStage0;
            ledStage1 = false;
            ledStage2 = false;
            ledStage3 = false;
            break;
        case 1:
            ledStage0 = true;
            ledStage1 = !ledStage1;
            ledStage2 = false;
            ledStage3 = false;
            break;
        case 2:
            ledStage0 = !ledStage0;
            ledStage1 = !ledStage1;
            ledStage2 = true;
            ledStage3 = false;
            break;
        case 3:
            ledStage0 = true;
            ledStage1 = false;
            ledStage2 = !ledStage2;
            ledStage3 = false;
            break;
        case 4:
            ledStage0 = true;
            ledStage1 = !ledStage1;
            ledStage2 = !ledStage2;
            ledStage3 = false;
            break;
        case 5:
            ledStage0 = true;
            ledStage1 = false;
            ledStage2 = false;
            ledStage3 = !ledStage3;
            break;
        case 6:
            ledStage0 = true;
            ledStage1 = true;
            ledStage2 = true;
            ledStage3 = true;
            stage_blinker.detach();
            break;
    }
}


err_t recv_callback(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err) {
    printf("TCP callback from %d.%d.%d.%d\r\n", ip4_addr1(&(pcb->remote_ip)),ip4_addr2(&(pcb->remote_ip)),ip4_addr3(&(pcb->remote_ip)),ip4_addr4(&(pcb->remote_ip)));
    char *data;
    /* Check if status is ok and data is arrived. */
    if (err == ERR_OK && p != NULL) {
        /* Inform TCP that we have taken the data. */
        tcp_recved(pcb, p->tot_len);
        data = static_cast<char *>(p->payload);
        printf("DATA: %s\r\n", data);
    }
    else 
    {
        /* No data arrived */
        /* That means the client closes the connection and sent us a packet with FIN flag set to 1. */
        /* We have to cleanup and destroy out TCPConnection. */
        printf("Connection closed by client.\r\n");
        tcp_close(pcb);
    }
    pbuf_free(p);
    return ERR_OK;
}

/* Accept an incomming call on the registered port */
err_t accept_callback(void *arg, struct tcp_pcb *npcb, err_t err) {

    LWIP_UNUSED_ARG(arg);
    
    flag++;
    
    printf("Recieve from broker...\r\n");
    printf("\r\n");
    
    /* Subscribe a receive callback function */
    tcp_recv(npcb, &recv_callback);
    
    /* Don't panic! Everything is fine. */
    return ERR_OK;
}

int main() {
    
    flag = 0;
    printf("\r\n");
    printf("############ mBed MQTT client Tester ############\r\n");
    printf("\r\n");
    
    //Attach stage blinker
    stage = 0;
    Ticker tickFast, tickSlow, tickARP, eth_tick, dns_tick, dhcp_coarse, dhcp_fine;
    stage_blinker.attach_us(&stageblinker, 1000*500);
    
    
    struct netif   *netif = &netif_data;
    struct ip_addr  ipaddr;
    struct ip_addr  netmask;
    struct ip_addr  gateway;

    printf("Configuring device for DHCP...\r\n");
    char *hostname = "my_mbed";
    /* Start Network with DHCP */
    IP4_ADDR(&netmask, 255,255,255,255);
    IP4_ADDR(&gateway, 0,0,0,0);
    IP4_ADDR(&ipaddr, 0,0,0,0);
    
    /* Initialise after configuration */
    lwip_init();
    netif->hwaddr_len = ETHARP_HWADDR_LEN;
    device_address((char *)netif->hwaddr);
    
    // debug MAC address
    printf("mbed HW address: %02x:%02x:%02x:%02x:%02x:%02x\r\n",
        (char*) netif->hwaddr[0],
        (char*) netif->hwaddr[1],
        (char*) netif->hwaddr[2],
        (char*) netif->hwaddr[3],
        (char*) netif->hwaddr[4],
        (char*) netif->hwaddr[5]);
    
    netif = netif_add(netif, &ipaddr, &netmask, &gateway, NULL, device_init, ip_input);
    netif->hostname = hostname;
    netif_set_default(netif);
    dhcp_start(netif); // <-- Use DHCP
    
     /* Initialise all needed timers */
    tickARP.attach_us( &etharp_tmr,  ARP_TMR_INTERVAL  * 1000);
    tickFast.attach_us(&tcp_fasttmr, TCP_FAST_INTERVAL * 1000);
    tickSlow.attach_us(&tcp_slowtmr, TCP_SLOW_INTERVAL * 1000);
    dns_tick.attach_us(&dns_tmr, DNS_TMR_INTERVAL * 1000);
    dhcp_coarse.attach_us(&dhcp_coarse_tmr, DHCP_COARSE_TIMER_MSECS * 1000);
    dhcp_fine.attach_us(&dhcp_fine_tmr, DHCP_FINE_TIMER_MSECS * 1000);
    
    
    stage = 1;
    
    while (!netif_is_up(netif)) { 
        device_poll(); 
    } 

    printf("Interface is up, local IP is %s\r\n", inet_ntoa(*(struct in_addr*)&(netif->ip_addr))); 
    printf("\r\n");
    printf("Start connecting to MQTT broker...\r\n");
    printf("\r\n");
    
    stage = 2;
    
    struct ip_addr serverIp;
    IP4_ADDR(&serverIp, 10,1,1,5);
   
    PubSub_mbed client(serverIp, 1883, &accept_callback);
    
    //MQTT identification header
    char clientID[] = "mbed";
    //Subscribe to topic
    char topic[] = "/mbed_r"; //"/mbed-reply";
    int flag = 0;
    
    flag = client.connect(clientID);
   
    if (flag) {
        printf("Connect to broker sucessed....\r\n");
    } else {
        printf("Failed...\r\n");
    }
    //printf("Flag(Connect) is : %d\r\n", flag); 
    printf("\r\n");    
    wait(3);
    
    stage = 3;
    
    client.subscribe(topic);
    
    //printf("Flag(Subscribe) is : %d\r\n", flag); 
    //printf("\r\n");
    wait(3);
    
    stage = 4;
    
    flag = client.publish("/mbed", "Hello, here is mbed!");
    
    //printf("Flag(Publish) is : %d\r\n", flag); 
    //printf("\r\n");
    wait(3);
    
    stage = 5;
    
       
    int i;
    for(i = 0; i < 5; i++){
    //while(flag != 10) {
        ; 
        flag = client.publish("/mbed", (char*)i);
        client.live();
        device_poll();
        wait(1);
    }
    
    while(i <= 20) {
        ; 
        flag = client.publish("/mbed", "Keep alive");
        client.live();
        device_poll();
        wait(10);
        i++;
    }
    
    //Close connection
    client.disconnect();
    
    printf("\r\n");
    printf("############  Test finish  #############\r\n");
    printf("\r\n");  
    
    
}


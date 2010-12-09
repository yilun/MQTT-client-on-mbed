#include "PubSub_mbed.h"


PubSub_mbed::PubSub_mbed(struct ip_addr serverIp, int port, tcp_accept_fn fn){
    this->tcp_fun = fn;
    this->pcb = tcp_new();
    this->server = serverIp;
    this->port = port;
    timer.start();
    connected = 0;
}

int PubSub_mbed::connect(char *id) {

    err_t err= tcp_connect(this->pcb, &server, 1883, tcp_fun);
    
    if(err != ERR_OK){
        printf("Connection Error : %d\r\n",err);
        return -1;
    } else {
        //printf("Connection sucessed..\r\n");
    }
    
    wait(1);
    tcp_accept(this->pcb, tcp_fun);   
    device_poll();
    
    // variable header
    uint8_t var_header[] = {0x00,0x06,0x4d,0x51,0x49,0x73,0x64,0x70,0x03,0x02,0x00,KEEPALIVE/500,0x00,strlen(id)};
    
    // fixed header: 2 bytes, big endian
    uint8_t fixed_header[] = {MQTTCONNECT,12+strlen(id)+2};

    char packet[sizeof(fixed_header)+sizeof(var_header)+strlen(id)];
    
    memset(packet,0,sizeof(packet));
    memcpy(packet,fixed_header,sizeof(fixed_header));
    memcpy(packet+sizeof(fixed_header),var_header,sizeof(var_header));
    memcpy(packet+sizeof(fixed_header)+sizeof(var_header),id,strlen(id));
    
    //Send MQTT identification message to broker.
    err = tcp_write(this->pcb, (void *)packet, sizeof(packet), 1);
    
    if (err == ERR_OK) {
        tcp_output(this->pcb);
        //printf("Identificaiton message sended correctlly...\r\n");
        return 1;
    } else {
        printf("Failed to send the identification message to broker...\r\n");
        printf("Error is: %d\r\n",err);
        tcp_close(this->pcb);
        return -2;
    }
    printf("\r\n");    
    
    device_poll();
    return 1;
}

int PubSub_mbed::publish(char* pub_topic, char* msg) {

    uint8_t var_header_pub[strlen(pub_topic)+3];
    strcpy((char *)&var_header_pub[2], pub_topic);
    var_header_pub[0] = 0;
    var_header_pub[1] = strlen(pub_topic);
    var_header_pub[sizeof(var_header_pub)-1] = 0;
    
    uint8_t fixed_header_pub[] = {MQTTPUBLISH,sizeof(var_header_pub)+strlen(msg)};
    
    uint8_t packet_pub[sizeof(fixed_header_pub)+sizeof(var_header_pub)+strlen(msg)];
    memset(packet_pub,0,sizeof(packet_pub));
    memcpy(packet_pub,fixed_header_pub,sizeof(fixed_header_pub));
    memcpy(packet_pub+sizeof(fixed_header_pub),var_header_pub,sizeof(var_header_pub));
    memcpy(packet_pub+sizeof(fixed_header_pub)+sizeof(var_header_pub),msg,strlen(msg));
    
    //Publish message
    err_t err = tcp_write(this->pcb, (void *)packet_pub, sizeof(packet_pub), 1); //TCP_WRITE_FLAG_MORE
    
    if (err == ERR_OK) {
        tcp_output(this->pcb);
        printf("Publish: %s ...\r\n", msg);
    } else {
        printf("Failed to publish...\r\n");
        printf("Error is: %d\r\n",err);
        tcp_close(this->pcb);
        return -1;
    }
    printf("\r\n");
    device_poll();
    return 1;
}


void PubSub_mbed::disconnect() {
    uint8_t packet_224[] = {2,2,4};
    tcp_write(this->pcb, (void *)packet_224, sizeof(packet_224), 1);
    tcp_write(this->pcb, (void *)(0), sizeof((0)), 1);
    //socket->send((char*)224,sizeof((char*)224));
    //socket->send((uint8_t)0,sizeof((uint8_t)0));
    tcp_close(this->pcb);
    lastActivity = timer.read_ms();
}

void PubSub_mbed::subscribe(char* topic) {

    if (connected) {
    
        uint8_t var_header_topic[] = {0,10};    
        uint8_t fixed_header_topic[] = {MQTTSUBSCRIBE,sizeof(var_header_topic)+strlen(topic)+3};
        
        // utf topic
        uint8_t utf_topic[strlen(topic)+3];
        strcpy((char *)&utf_topic[2], topic);

        utf_topic[0] = 0;
        utf_topic[1] = strlen(topic);
        utf_topic[sizeof(utf_topic)-1] = 0;
        
        char packet_topic[sizeof(var_header_topic)+sizeof(fixed_header_topic)+strlen(topic)+3];
        memset(packet_topic,0,sizeof(packet_topic));
        memcpy(packet_topic,fixed_header_topic,sizeof(fixed_header_topic));
        memcpy(packet_topic+sizeof(fixed_header_topic),var_header_topic,sizeof(var_header_topic));
        memcpy(packet_topic+sizeof(fixed_header_topic)+sizeof(var_header_topic),utf_topic,sizeof(utf_topic));
        
        //Send message
        err_t err = tcp_write(this->pcb, (void *)packet_topic, sizeof(packet_topic), 1); //TCP_WRITE_FLAG_MORE
        if (err == ERR_OK) {
            tcp_output(this->pcb);
            printf("Subscribe sucessfull to: %s...\r\n", topic);
        } else {
            printf("Failed to subscribe to: %s...\r\n", topic);
            printf("Error is: %d\r\n",err);
            tcp_close(pcb);
        }
        printf("\r\n");
        device_poll();
    }
}

int PubSub_mbed::live() {
    if (connected) {
        int t = timer.read_ms();
        if (t - lastActivity > KEEPALIVE) {
            //Send 192 0 to broker
            uint8_t packet_192[] = {2,2,4};
            tcp_write(this->pcb, (void *)packet_192, sizeof(packet_192), 1);
            tcp_write(this->pcb, (void *)(0), sizeof((0)), 1);
            lastActivity = t;
        }
        /*
        if (_client.available()) {
            uint8_t len = readPacket();
            if (len > 0) {
                uint8_t type = buffer[0]>>4;
                if (type == 3) { // PUBLISH
                    if (callback) {
                        uint8_t tl = (buffer[2]<<3)+buffer[3];
                        char topic[tl+1];
                        for (int i=0;i<tl;i++) {
                            topic[i] = buffer[4+i];
                        }
                        topic[tl] = 0;
                        // ignore msgID - only support QoS 0 subs
                        uint8_t *payload = buffer+4+tl;
                        callback(topic,payload,len-4-tl);
                    }
                } else if (type == 12) { // PINGREG
                    _client.write(208);
                    _client.write((uint8_t)0);
                    lastActivity = t;
                }
             }
          }*/
        return 1;
    }
    return 0;
}

/*
int PubSub_mbed::connected() {
    int rc = 0;(int)socket.connected();
    if (!rc) {
        socket.close();
    }
    return rc;
}
*/
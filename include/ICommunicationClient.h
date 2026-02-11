//
// Created by Johnathon Slightham on 2025-06-10.
//

#ifndef INETWORKCLIENT_H
#define INETWORKCLIENT_H

class ICommunicationClient {
  public:
    virtual ~ICommunicationClient() = default;
    virtual int init() = 0;
    virtual int send_msg(void *sendbuff, uint32_t len) = 0;
};

#endif // INETWORKCLIENT_H

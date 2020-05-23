#ifndef SIP_h
#define SIP_h

#include <SPI.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include "mbedtls/md.h"


#define SIP_STATE_INIT                         0
#define SIP_STATE_CONNECTING                   1
#define SIP_STATE_CONNECTED                    2 
#define SIP_STATE_UNAUTH_INVITE_SENT           3 
#define SIP_STATE_UNAUTH_REPLY_RECEIVING       4
#define SIP_STATE_UNAUTH_REPLY_RECEIVED        5
#define SIP_STATE_AUTH_INVITE_SENT             6  
#define SIP_STATE_AUTH_INVITE_RECEIVING_REPLY  7
#define SIP_STATE_TRYING                       8

#define SIP_STATE_DELAY_BEFORE_RETRY          10

#define SIP_STATE_CANCEL_SENT                 20
#define SIP_STATE_REQUEST_CANCELED            21

#define SIP_STATE_READ_ENTIRE_REPLY_THEN_DISCONNECT 30
#define SIP_STATE_DISCONNECTING                     31
#define SIP_STATE_DONE                              32


class SIP
{
public:
  void init(const char *serverIp, int port, boolean useTcp,const char *user,const char *password,const char *dialNr, int ringDurationInSeconds);
  void ring();
  void update();
private:
  void compute_response(char *nonce, char * realm, char *method);
  void sendSip(char *buffer);
  void sendSip(int i);
  void udpBeginPacket();
  void udpEndPacket();
  void sendSipLine( const __FlashStringHelper* format , ... );
  void sendSipHeader(char *command, int cseq);
  void sipInvite(int cseq);
  void sipAck(int cseq);
  void sipCancel(int cseq);
  void parseParameter(char *dest, char *name, char *line);
  void toHex(char *dest, const unsigned char *data, int len);
  int freeRam();
  boolean m_useTcp;
  const char *m_serverIp;
  int m_port;
  const char *m_user;
  const char *m_pwd;
  const char *m_dialNr;
  int m_state;
  char m_transportName[3+1];
  char m_transportNameLower[3+1];
  char *m_recBufWp;
  char m_response[32+1];
  unsigned long m_ringStart;
  int m_ringDurationSecs;
  char m_realm[20+1];
  char m_nonce[100+1];
  char m_recBuf[150+1];
  char m_uri[50+1];
  char m_myIp[20+1];
  char m_branch[70+1];
  char m_tag[20+1];
  char m_callId[70+1];
  WiFiClient m_tcpClient;
  WiFiUDP m_udpClient;
  char m_haData[100+1];
  char m_responseData[150+1];
  unsigned long lastFailure;
  int retries;
};

#endif

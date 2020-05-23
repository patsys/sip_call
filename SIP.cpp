#include "SIP.h"
#define SIP_LOG_DETAILS
#define SIP_LOG_STATUS
// log SIP call details
//#define SIP_LOG_DETAILS

// log SIP call status summary
//#define SIP_LOG_STATUS

void SIP::init(const char *serverIp, int port, boolean useTcp,const char *user,const char *password,const char *dialNr, int ringDurationInSeconds)
{
  m_serverIp = serverIp;
  m_port = port;
  m_user = user;
  m_pwd = password;
  m_dialNr = dialNr;
  m_state = SIP_STATE_INIT;
  m_useTcp = useTcp;
  m_ringDurationSecs = ringDurationInSeconds;

  sprintf_P(m_myIp, PSTR("%d.%d.%d.%d"), WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
  sprintf_P(m_uri, PSTR("sip:%s@%s"), m_dialNr, m_serverIp);

  lastFailure = 0;
}


void SIP::toHex(char *dest, const unsigned char *data, int len)
{
  static const char hexits[17] = "0123456789abcdef";
  int i;

  for (i = 0; i < len; i++) {
    dest[i * 2]       = hexits[data[i] >> 4];
    dest[(i * 2) + 1] = hexits[data[i] &  0x0F];
  }
  dest[len * 2] = '\0';
}

// http://en.wikipedia.org/wiki/Digest_access_authentication
void SIP::compute_response(char *nonce, char *realm, char *method) {
byte res[16];
  mbedtls_md_context_t ctx;
mbedtls_md_type_t md_type = MBEDTLS_MD_MD5;
 
 
  char ha1Text[32 + 1];
  char ha2Text[32 + 1];

  sprintf_P(m_haData, PSTR("%s:%s:%s"), m_user, realm, m_pwd);

  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
  mbedtls_md_starts(&ctx);
  mbedtls_md_update(&ctx, (const unsigned char *) m_haData, strlen(m_haData));
  mbedtls_md_finish(&ctx, res);
  mbedtls_md_free(&ctx);
  toHex(ha1Text,res, 16);


  sprintf_P(m_haData, PSTR("%s:%s"), method, m_uri);

  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
  mbedtls_md_starts(&ctx);
  mbedtls_md_update(&ctx, (const unsigned char *) m_haData, strlen(m_haData));
  mbedtls_md_finish(&ctx, res);
  mbedtls_md_free(&ctx);
  toHex(ha2Text,res, 16);

  sprintf_P(m_responseData, PSTR("%s:%s:%s"), ha1Text, nonce, ha2Text);

  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
  mbedtls_md_starts(&ctx);
  mbedtls_md_update(&ctx, (const unsigned char *) m_responseData, strlen(m_responseData));
  mbedtls_md_finish(&ctx, res);
  mbedtls_md_free(&ctx);
  toHex(m_response, res, 16);

#ifdef SIP_LOG_DETAILS
  Serial.print(F("HA1="));
  Serial.println(ha1Text);
  Serial.print(F("HA2=("));
  Serial.print(method);
  Serial.print(F(":"));
  Serial.print(m_uri);
  Serial.print(F(")="));
  Serial.println(ha2Text);
#endif
}


// send string to SIP server
void SIP::sendSip(char *buffer) {
  if (m_useTcp == false) {
    m_udpClient.print(buffer);
  }
  else {
    m_tcpClient.print(buffer);
  }

#ifdef SIP_LOG_DETAILS
  Serial.print(F(">"));
  Serial.print(buffer);
  Serial.flush();
#endif
}

// send i as string to SIP server
void SIP::sendSip(int i) {
  char b[20 + 1];
  sprintf_P(b, PSTR("%d"), i);
  if (m_useTcp == false) {
    m_udpClient.print(b);
  }
  else {
    m_tcpClient.print(b);
  }
#ifdef SIP_LOG_DETAILS
  Serial.print(b);
#endif
}

// if UDP, start packet
void SIP::udpBeginPacket() {
  if (m_useTcp == false) {
    m_udpClient.beginPacket(m_serverIp, m_port);
  }
}

void SIP::udpEndPacket() {
  if (m_useTcp == false) {
    m_udpClient.endPacket();
  }
}

// format according to constFormat (read from PROGMEM), then send to SIP server
void SIP::sendSipLine(const __FlashStringHelper* constFormat , ... ) {
  char stringFromProgmem[250];
  char line[250 + 1];

  va_list arglist;
  strcpy_P(stringFromProgmem, (char*)constFormat);
  va_start( arglist, stringFromProgmem );
  vsprintf(line, stringFromProgmem, arglist );
  va_end( arglist );
  sendSip(line);
  sendSip("\n");
}


void SIP::sendSipHeader(char *command, int cseq) {
  sendSipLine(F("%s %s SIP/2.0"), command, m_uri);
  sendSipLine(F("Call-ID: %s"), m_callId);
  sendSipLine(F("CSeq: %d %s"), cseq, command);
  sendSipLine(F("Max-Forwards: 70"));
  sendSipLine(F("From: \"%s\" <sip:%s@%s>;tag=%s"), m_user, m_user, m_serverIp, m_tag);
  if (m_useTcp == false) { // must remove ; for UDP for some unknown reason
    sendSipLine(F("Via: SIP/2.0/%s %s:%dbranch=%s"), m_transportName, m_myIp, m_port, m_branch);
  }
  else {
    sendSipLine(F("Via: SIP/2.0/%s %s:%d;branch=%s"), m_transportName, m_myIp, m_port, m_branch);
  }
  sendSipLine(F("To: <%s>"), m_uri);
}

void SIP::sipInvite(int cseq) {
  udpBeginPacket();

  sendSipHeader("INVITE", cseq);

  sendSipLine(F("Contact: \"%s\" <sip:%s@%s:%d;transport=%s>"), m_user, m_user, m_myIp, m_port, m_transportNameLower);
  sendSipLine(F("Content-Type: application/sdp"));
  if (cseq == 2) { // authentication?
    sendSipLine(F("Authorization: Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", uri=\"%s\", response=\"%s\""), m_user, m_realm, m_nonce, m_uri, m_response);
  }
  sendSipLine(F("Content-Length: 0"));
  sendSipLine(F(""));

  udpEndPacket();
}

void SIP::sipAck(int cseq) {

  udpBeginPacket();

  sendSipHeader("ACK", cseq);
  sendSipLine(F("Content-Length: 0"));
  sendSipLine(F(""));

  udpEndPacket();
}
void SIP::sipCancel(int cseq) {

  udpBeginPacket();

  sendSipHeader("CANCEL", cseq);

  sendSipLine(F("Content-Length: 0"));
  sendSipLine(F(""));

  udpEndPacket();
}

// call phone
void SIP::ring()
{
  if (m_ringStart-millis()>(m_ringDurationSecs*1000) && ((m_state == SIP_STATE_INIT) || (m_state == SIP_STATE_DONE))) {

    long branchLong = random(0, 2147483647);
    long tagLong = random(0, 2147483647);
    long callId = random(0, 2147483647);
    sprintf_P(m_branch, PSTR("z9hG4bK-%ld"), branchLong);
    sprintf_P(m_tag, PSTR("%ld"), tagLong);
    sprintf_P(m_callId, PSTR("%ld"), callId);

    retries = 0;
    lastFailure = 0;
    m_state = SIP_STATE_CONNECTING;
  }
}

// update call state. Must be called regularly from loop().
void SIP::update()
{
  int udpPacketSize;
  int avail;
  char c;

  if ((m_state == SIP_STATE_INIT) || (m_state == SIP_STATE_DONE)) {
    return;
  }

  if (m_state == SIP_STATE_CONNECTING) {
#ifdef SIP_LOG_STATUS
    Serial.print(F("connecting to SIP server "));
    Serial.println(m_serverIp);
    Serial.flush();
#endif

    if (m_useTcp == false) {
      strcpy(m_transportName, "UDP");
      strcpy(m_transportNameLower, "udp");
      if (m_udpClient.begin(m_port)) {
#ifdef SIP_LOG_STATUS
        Serial.println(F("UDP initialized"));
#endif
        m_state = SIP_STATE_CONNECTED;
      }
      else {
#ifdef SIP_LOG_STATUS
        Serial.println(F("UDP initialization failed"));
#endif
        lastFailure = millis();
        m_state = SIP_STATE_DELAY_BEFORE_RETRY;
      }
    }
    else {
      strcpy(m_transportName, "TCP");
      strcpy(m_transportNameLower, "tcp");

      if (m_tcpClient.connect(m_serverIp, m_port)) {
#ifdef SIP_LOG_STATUS
        Serial.println(F("TCP connected to SIP server"));
#endif
        m_state = SIP_STATE_CONNECTED;
      }
      else {
	
#ifdef SIP_LOG_STATUS
        Serial.println(F("TCP connection failed"));
#endif
        lastFailure = millis();
        m_state = SIP_STATE_DELAY_BEFORE_RETRY;
      }
    }

    m_recBufWp = &m_recBuf[0];
  }
  else if (m_state == SIP_STATE_DELAY_BEFORE_RETRY) {
    unsigned long delayInMillis = millis() - lastFailure;
    unsigned long delayInSeconds = delayInMillis / 1000;

    if (delayInSeconds >= 10) {
      retries++;
      if (retries > 5) {
        m_state = SIP_STATE_DONE; // no more retries
#ifdef SIP_LOG_STATUS
        Serial.print(F("Done. free RAM="));
        Serial.println(freeRam());
        Serial.flush();
#endif
      }
      else {
        m_state = SIP_STATE_CONNECTING; // try again
      }
    }

    return;
  }

  // if there's data available, read a packet
  if (m_useTcp == false) {
    udpPacketSize = m_udpClient.parsePacket();
    avail = m_udpClient.available();
  }
  else {
    avail = m_tcpClient.available();
  }
  if (avail > 0)
  {
    if (m_useTcp == false) {
#ifdef SIP_LOG_DETAILS
      Serial.print(F("Received UDP packet of size "));
      Serial.println(udpPacketSize);
#endif
    }


    while (avail) {
      if (m_useTcp == false) {
        c = m_udpClient.read();
      }
      else {
        c = m_tcpClient.read();
      }
      *(m_recBufWp++) = c;
      *(m_recBufWp) = 0;
      if (c == '\n') {
        *(m_recBufWp - 2) = 0; // remove cr/linefeed
        *(m_recBufWp - 1) = 0;

#ifdef SIP_LOG_DETAILS
        Serial.print(F("<"));
        Serial.println(m_recBuf);
#endif


        if (m_state == SIP_STATE_UNAUTH_INVITE_SENT) {
          if (strstr(m_recBuf, "SIP/2.0 401 Unauthorized") != NULL) {
            // unauthorized INVITE reply received completely
            m_state = SIP_STATE_UNAUTH_REPLY_RECEIVING;
          }
          else
          {
            m_state = SIP_STATE_READ_ENTIRE_REPLY_THEN_DISCONNECT;
#ifdef SIP_LOG_STATUS
            Serial.print(F("Call failed: "));
            Serial.println(m_recBuf);
#endif
          }
        }
        else if (m_state == SIP_STATE_UNAUTH_REPLY_RECEIVING) {
          if (strstr(m_recBuf, "WWW-Authenticate") != NULL) {
            parseParameter(m_nonce, "nonce=", m_recBuf);
            parseParameter(m_realm, "realm=", m_recBuf);

            compute_response(m_nonce, m_realm, "INVITE");

#ifdef SIP_LOG_DETAILS
            Serial.print(F("RESP="));
            Serial.println(m_response);
#endif
          }
          else if (strstr(m_recBuf, "Content-Length: 0") != NULL) {
            // unauthorized INVITE reply received completely
            m_state = SIP_STATE_UNAUTH_REPLY_RECEIVED;
          }
        }
        else if (m_state == SIP_STATE_AUTH_INVITE_SENT) {
          // authorized INVITE sent, expecting "SIP/2.0 100 Trying"
          // unauthorized INVITE reply
          if (strstr(m_recBuf, "SIP/2.0 100 Trying") != NULL) {
            m_state = SIP_STATE_TRYING;
            m_ringStart = millis();
#ifdef SIP_LOG_STATUS
            Serial.println(F("RINGING..."));
#endif
          }
          else {
            // not ringing, something went wrong
            m_state = SIP_STATE_READ_ENTIRE_REPLY_THEN_DISCONNECT;
#ifdef SIP_LOG_STATUS
            Serial.print(F("Call failed: "));
            Serial.println(m_recBuf);
#endif
          }
        }
        else if (m_state == SIP_STATE_TRYING) {
          if ((strstr(m_recBuf, "SIP/2.0 486 Busy Here") != NULL) || (strstr(m_recBuf, "SIP/2.0 603 Decline") != NULL)) {
            m_state = SIP_STATE_READ_ENTIRE_REPLY_THEN_DISCONNECT;
#ifdef SIP_LOG_STATUS
            Serial.print(F("Call failed: "));
            Serial.println(m_recBuf);
#endif
          }
          else if ((strstr(m_recBuf, "SIP/2.0 200 OK") != NULL)) {
            m_state = SIP_STATE_READ_ENTIRE_REPLY_THEN_DISCONNECT;
#ifdef SIP_LOG_STATUS
            Serial.print(F("Call answered: "));
            Serial.println(m_recBuf);
#endif
          }
        }
        else if (m_state == SIP_STATE_READ_ENTIRE_REPLY_THEN_DISCONNECT) {
          if (strstr(m_recBuf, "Content-Length: ") != NULL) {
            m_state = SIP_STATE_DISCONNECTING;
          }
        }
        else if (m_state == SIP_STATE_CANCEL_SENT) {
          // canceld, waiting for complete reply
          if (strstr(m_recBuf, "Content-Length: ") != NULL) {
            m_state = SIP_STATE_REQUEST_CANCELED;
          }
        }
        else if (m_state == SIP_STATE_REQUEST_CANCELED) {
          // canceld, waiting for complete reply
          if (strstr(m_recBuf, "Content-Length: ") != NULL) {
            m_state = SIP_STATE_DISCONNECTING;
          }
        }
        else if (m_state == SIP_STATE_DISCONNECTING) {
          // done, close TCP
          if (m_useTcp == true) {
            m_tcpClient.stop();
#ifdef SIP_LOG_STATUS
            Serial.println(F("TCP disconnected"));
#endif
          }
          m_state = SIP_STATE_DONE;
#ifdef SIP_LOG_STATUS
          Serial.print(F("Done. free RAM="));
          Serial.println(freeRam());
          Serial.flush();
#endif
        }

        // reset buffer
        m_recBufWp = &m_recBuf[0];
      }

      if (m_useTcp == false) {
        avail = m_udpClient.available();
      }
      else {
        avail = m_tcpClient.available();
      }
    }
  }

  if (m_state == SIP_STATE_CONNECTED) {
    sipInvite(1);  // unauthorized
    m_state = SIP_STATE_UNAUTH_INVITE_SENT;
  }
  else if (m_state == SIP_STATE_UNAUTH_REPLY_RECEIVED) {
    sipAck(1);
    sipInvite(2);  // authorized

    m_state = SIP_STATE_AUTH_INVITE_SENT;
  }
  // ringing?
  else if (m_state == SIP_STATE_TRYING) {
    unsigned long ringDuration = millis() - m_ringStart;
    unsigned long actualRingDurationInSeconds = ringDuration / 1000;

    //    Serial.println(actualRingDurationInSeconds);

    if (actualRingDurationInSeconds >= m_ringDurationSecs) {
      sipCancel(2);
      m_state = SIP_STATE_CANCEL_SENT;
    }
  }
}

// parse parameter value from http formated string
void SIP::parseParameter(char *dest, char *name, char *line) {
  char *qp;
  char *r;
  if ((r = strstr(line, name)) != NULL) {
    r = r + strlen(name) + 1;
    qp = strchr(r, '\"');
    int l = qp - r;
    strncpy(dest, r, l);
  }
}

int SIP::freeRam () {
  extern int __heap_start, *__brkval;
  int v;
  return 0;
//  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}

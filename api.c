#define API_C
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h> // Contains file controls like O_RDWR
#include <errno.h> // Error integer and strerror() function
#include <termios.h> // Contains POSIX terminal control definitions
#include <unistd.h> // write(), read(), close()
#include <ctype.h>
#include <byteswap.h>
#include "api.h"

#define CHECKSUM (0xFF)
#define THIS_IS_LITTLE_ENDIAN

typedef struct
{
  u_int8_t ft;
  const char const *ftn;
}ftnlup;

const ftnlup FrameTypeName[]=
{
  {0x08,"AT_COMMAND"},
  {0x09,"AT_COMMAND_QUEUED"},
  {0x10,"TX_REQUEST"},
  {0x17,"REMOTE_AT_COMMAND"},
  {0x88,"AT_COMMAND_RESPONSE"},
  {0x8A,"MODEM_STATUS"},
  {0x8B,"TX_RESPONSE"},
  {0x90,"RX_RECEIVED"},
  {0x92,"RX_DATA_RECEIVED"},
  {0x95,"NODE_IDENTIFICATION_INDICATOR"},
  {0x97,"REMOTE_AT_COMMAND_RESPONSE"},
  {0xff,"undefined"}
};

// ----------------------------------------------------------------------------
// chksum
// Input: Pointer to API Frame. Self describing frame.
// Output:
//  0 Checksum good.
// !0 Checksum bad.
// ----------------------------------------------------------------------------
int chksum(u_int8_t *buf)
{
  int i,len;
  u_int8_t csum;

  if(buf[0] != START_OF_FRAME){
    return(-1);
  }
  len = buf[1] * 256 + buf[2] + 1;
  for(csum=i=0;i<len;i++){
    csum += buf[i+3];
  }
  return(CHECKSUM != csum);
}

// ----------------------------------------------------------------------------
void genid(u_int8_t *buf)
{
  static u_int8_t id=1;
  buf[4] = id;
  id++;
  if(id == 0){
    id=1;
  }
}

// ----------------------------------------------------------------------------
void gensum(u_int8_t *buf)
{
  int i,len;
  u_int8_t csum;

  len = buf[1] * 256 + buf[2];
  for(csum=i=0;i<len;i++){
    csum += buf[i+3];
  }
  buf[3+len] = 0xFF - csum;
}

// ----------------------------------------------------------------------------
void prt(u_int8_t *buf,int size,char *msg)
{
  int len,i;
  if(chksum(buf)){
    if(size>14){
      msg += sprintf(msg,"Bad checksum: ");
      size -= 14;
    }
  }
  len = 3 + buf[1] * 256 + buf[2] + 1;
  for(i=0;i<len;i++){
    if(size < 3){
      *msg = 0;
      break;
    }
    msg += sprintf(msg,"%02X ",buf[i]);
    size -= 3;
  }
}

// ----------------------------------------------------------------------------
const char const *getFrameTypeName(u_int8_t ft)
{
  int i=0;
  for(;FrameTypeName[i].ft != ft && FrameTypeName[i].ft != 0xFF;i++){
    ;
  }
  return(FrameTypeName[i].ftn);
}

#define START_OF_FRAME (0x7E) 

typedef enum
{
  // Commands
  AT_COMMAND=0x08,
  AT_COMMAND_QUEUED=0x09,
  TX_REQUEST=0x10,
  REMOTE_AT_COMMAND=0x17,
  // Responses
  AT_COMMAND_RESPONSE=0x88,
  MODEM_STATUS=0x8A,		  
  TX_RESPONSE=0x8B,      
  RX_RECEIVED=0x90,           
  RX_DATA_RECEIVED=0x92,
  NODE_IDENTIFICATION_INDICATOR=0x95,
  REMOTE_AT_COMMAND_RESPONSE=0x97
}frame_type_t;

typedef u_int64_t API_DESTINATION_ADDRESS;
typedef u_int16_t API_DESTINATION_NETWORK_ADDRESS;

#define API_DA_BROADCAST (0xFFFF)
#define API_DNA_BROADCAST (0xFFFE)

int chksum(u_int8_t *buf);
void gensum(u_int8_t *buf);
void genid(u_int8_t *buf);
void prt(u_int8_t *buf,int size, char *msg);
const char const *getFrameTypeName(u_int8_t ft);

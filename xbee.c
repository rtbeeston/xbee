#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <errno.h> // Error integer and strerror() function
#include <byteswap.h>
#include "sp.h"
#include "api.h"
//#include "getch.h"

typedef enum
{
  coordinator,
  router,
  endpoint
}node_type_t;

typedef struct
{
  u_int16_t network_id;
  u_int64_t serial;
  char name[21];
  u_int16_t parent_network_id;
  node_type_t node_type; 
  int children_count;
  u_int8_t status;
  u_int16_t profile_id;
  u_int16_t mfg_id;
  
  u_int16_t digital_channel_mask;
#define DCM_DIO12 0x10 
#define DCM_DIO11 0x08
#define DCM_DIO10 0x04
#define DCM_DIO7  0x80
#define DCM_DIO6  0x40
#define DCM_DIO5  0x20
#define DCM_DIO4  0x10
#define DCM_DIO3  0x08
#define DCM_DIO2  0x04
#define DCM_DIO1  0x02
#define DCM_DIO0  0x01
  u_int8_t analog_channel_mask;
#define ACM_A3    0x08
#define ACM_A2    0x04
#define ACM_A1    0x02
#define ACM_A0    0x01
  u_int16_t digital_sample; // Same bit definitions as digital_channel_mask
  u_int16_t analog_sample[4]; // 0-1024
}node_t;

static node_t node[128];
static int local_node=0;
static int node_count=1;
static int find_node(u_int64_t serial);
static void *tinf(void *tinp);
static int bsm_at_command(char *at,unsigned int datacnt, u_int8_t *buf);
static int bsm_remote_at_command(char *at,u_int64_t da, u_int16_t nda, unsigned int datacnt, u_int8_t *buf);
static void process_frame(u_int8_t *buf);
static int discover_network(void);
static int print_network(void);
pthread_mutex_t apilock=PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  apicond=PTHREAD_COND_INITIALIZER;
struct timespec abstime;
static int resp_msg_id[256];
static bool option_verbose=false;

//#define loga(...) printf( __VA_ARGS__ )
#define loga(s,...) logaa( s , ##__VA_ARGS__ )


static void logaa(char *str, ...)
{
  va_list args;
  static FILE *apil=NULL;
  static int size=0;
  int i;

  if((apil == NULL) || (size > (2*1024*1024))){
    if(apil != NULL){
      fclose(apil);
    }
    rename("xbee.api.log","xbee.api.log.prior");
    apil = fopen("xbee.api.log","w");
    if(apil == NULL){
      fprintf(stderr,"ERROR: failed to open log file.\n");
      exit(-1);
    }
    size=0;
  }
  va_start(args,str);
  i = vfprintf(apil,str,args);
  va_end(args);
  if(i < 0){
    fprintf(stderr,"ERROR: logging.\n");
    exit(-1);
  }
  size+=i;
  fflush(apil);
  if(option_verbose){
    va_start(args,str);
    vprintf(str,args);
    va_end(args);
  }
}

// ----------------------------------------------------------------------------
int main(int argc,char **argv,char **envp)
{
  char sp_name[256]="/dev/ttyUSB0",command[256];
  int rc,index, c;
  extern int optind;
  extern char *optarg;
  pthread_t tin;

  while ((c = getopt(argc, argv, "l:hHv?")) != EOF){
    switch(c){
      case 'l' : 
        strcpy(sp_name,optarg);
        break;
      case 'v' : 
        option_verbose=true;
        break;
      default :
        printf("Usage: sp [-l </dev/ttyUSB0>] \n");
        break;
    } 
  }
  argc -= optind;
  argv += optind;

  // Open and configure the serial port
  if(sp_open(sp_name)){
    fprintf(stderr,"ERROR: opening serial port\n");
    exit(-1);
  }

  // Start the receiver thread
  rc = pthread_create(&tin,NULL,tinf,NULL);
  if(rc){
    fprintf(stderr,"ERROR: starting thread %d\n",errno);
    exit(-1);
  }
  
  printf("Initializing with connected node.\n");
  // bsm_remote_at_command("JV",API_DA_BROADCAST,API_DNA_BROADCAST,1,"\x01");
  bsm_at_command("MY",0,NULL);
  bsm_at_command("SH",0,NULL);
  bsm_at_command("SL",0,NULL);
  bsm_at_command("NI",0,NULL);
  discover_network();
  index=0;

  while(true){
    printf("\nIndex Node = %d\nMenu:\n"\
      "a\tAnalog dump\n"\
      "d\tDiscover network\n"\
      "i\tIndex node\n"\
      "n\tNework dump\n"\
      "q\tQuit\n"\
      "t\tToggle LED\n"\
      "u\tUpdate Name\n"\
      "-> ",index);
    if(fgets(command,256-1,stdin) == NULL){
      printf("ERROR geting input\n");
    }else if('a' == command[0]){
      bsm_remote_at_command("D1",node[index].serial,node[index].network_id,1,"\x02");  // Set dio1 to analong input
      bsm_remote_at_command("D3",node[index].serial,node[index].network_id,1,"\x02");  // Set dio1 to analong input
      bsm_remote_at_command("IR",node[index].serial,node[index].network_id,2,"\x27\x10");  // Set IO sample rate to 10,000 milliseconds
    }else if('d' == command[0]){
      discover_network();
    }else if('i' == command[0]){
      printf("Enter node index -> ");
      fgets(command,256-1,stdin);
      c=sscanf(command,"%d\n",&index);
      printf("read %d rc=%d\n",index,c);
      if(index < 0 || index > node_count-1){
        printf("Index out of range\n");
        index=0;
      }
    }else if('n' == command[0]){
      print_network();
    }else if('q' == command[0]){
      break;
    }else if('t' == command[0]){
      bsm_remote_at_command("D0",node[index].serial,node[index].network_id,1,node[index].digital_sample&DCM_DIO0?"\x04":"\x05"); 
      bsm_remote_at_command("IS",node[index].serial,node[index].network_id,0,NULL); 
    }else if('u' == command[0]){
      printf("Enter node name -> ");
      fgets(command,256-1,stdin);
      bsm_remote_at_command("NI",node[index].serial,node[index].network_id,strlen(command)-1,command);
      bsm_remote_at_command("WR",node[index].serial,node[index].network_id,0,NULL); 
      bsm_remote_at_command("NI",node[index].serial,node[index].network_id,0,command);
    }else{
      printf("Unknown command %c\n",command[0]);
    }
  }
  sp_close();
  return(0);
}

// ----------------------------------------------------------------------------
// Thread for reading input from attached xbee.
// Input tinp is the serial port file descriptor.
// This thread never exits.
// ----------------------------------------------------------------------------
void *tinf(void *tinp)
{
  int rc;
  // Maximum size of a single api frame is: 1 byte for start of frame, 2 bytes
  // for length, 65535 max payload, 1 byte for checksum.
  #define BUFSIZE (1 + 2 + 65535 + 1)
  static u_int8_t buf[BUFSIZE];
  u_int8_t inc;
  int fi=0,eom=0;
  char msg[256];

  while(1){
    // Wait forever if we're looking for a start of frame. Else timeout if we
    // don't get the rest of the frame in a reasonable time.
    // Read a byte.
    if(fi==0){
      sp_rcvto(0);
    }
    rc = sp_rcv(&inc);
    if(rc == 0){
      if(fi > 0){
        loga("ERROR: msg incomplete timeout. %d\n",fi);
  fi=0;
      }else{
        loga("ERROR: unexpected timeout.\n");
      }
    }else if(rc != 1) {
      loga("ERROR: read. %d %d %s\n",rc,errno,strerror(errno));
      sleep(1);
    }else if(fi>0){
      buf[fi++] = inc;
      if(fi == 3){
        eom=buf[1] * 256 + buf[2] + 3 + 1;
      }else if(fi == eom){
        if(chksum(buf)){
    int bcnt;
    bcnt=eom;
          while(bcnt > 0){    
            // checksum is bad and therefore this is not a valid message. See if we can
            // sync up on another start of frame.
            prt(buf,sizeof(msg),msg);
            loga("ERROR: bad frame. %d: %s\n",fi,msg);
            for(fi=1;fi<eom && buf[fi] != START_OF_FRAME ;fi++)
              ;
      if(fi==eom){
        bcnt = fi=eom=0;
      }else{
        bcnt=eom-fi;
        memcpy(buf,buf+fi,bcnt);
        fi = bcnt;
        if(fi >= 3){
                eom=buf[1] * 256 + buf[2] + 3 + 1;
        }else{
          eom=0;
        }
        if(fi >= eom){
                if(chksum(buf) == 0){
                  prt(buf,sizeof(msg),msg);
                  loga("Recv %3d bytes: %s\n",eom,msg);
      if(fi < bcnt){
              bcnt -= fi;
              memcpy(buf,buf+fi,bcnt);
        fi = bcnt;
        eom=0;
      }else{
                    bcnt = fi=eom=0;
            }
                }
        }
      }
    }
        }else{
          process_frame(buf);
          fi=eom=0;
        }
      }
    }else if(inc == START_OF_FRAME){
      buf[fi++] = inc;
      sp_rcvto(10);
    }else{
      loga("ERROR: unexpected character ignored. %d\n",inc);
    }
  }
  // Unreachable.
  return(NULL);
}

// ----------------------------------------------------------------------------
static int bsm_at_command(char *at,unsigned int datacnt, u_int8_t *data)
{
  int rc,i;
#define MAX_AT_COMMAND_FRAME (8)
  u_int8_t buf[MAX_AT_COMMAND_FRAME]; 
  char msg[256];

    buf[0]=START_OF_FRAME;
    buf[1]=0;           // MSB length
    buf[2]=4+datacnt;   // LSB length
    buf[3]=AT_COMMAND;  // Frame type
    genid(buf);         // Frame ID
    buf[5]=at[0];       // AT Command
    buf[6]=at[1];
    for(i=0;i<datacnt;i++){
      buf[7+i] = buf[i];
    }
    gensum(buf);
    rc = 3 + buf[1]*256+buf[2]+1; 
    resp_msg_id[buf[4]] = 0;
    rc = sp_snd(buf,rc);
    if(rc > 0){
      prt(buf,sizeof(msg),msg);
      loga("%-30s Sent %3d bytes: %c%c %16s %4s %s\n",getFrameTypeName(buf[3]),rc,buf[5],buf[6],"","",msg);
      pthread_mutex_lock(&apilock);
      if(resp_msg_id[buf[4]] == 0){
        clock_gettime(CLOCK_REALTIME,&abstime);
        abstime.tv_sec += 10;
        pthread_cond_timedwait(&apicond,&apilock,&abstime);
      }
      pthread_mutex_unlock(&apilock);
    }else{
      fprintf(stderr,"ERROR: sending: rc = %d\n",rc);
    }
  return(rc);
}

// ----------------------------------------------------------------------------
static int bsm_remote_at_command(char *at, u_int64_t da, u_int16_t nda, unsigned int datacnt, u_int8_t *data)
{
  int rc,i;
#define MAX_REMOTE_AT_COMMAND_FRAME (20)
  u_int8_t buf[MAX_REMOTE_AT_COMMAND_FRAME];
  char msg[256];

    buf[0]=START_OF_FRAME;
    buf[1]=0;           // MSB length
    buf[2]=15+datacnt;  // LSB length
    buf[3]=REMOTE_AT_COMMAND;  // Frame type
    genid(buf);         // Frame ID
    *(u_int64_t *)(buf+5) = bswap_64(da);
    *(u_int16_t *)(buf+13) = bswap_16(nda);
    buf[15]=0x02;         // AT Command
    buf[16]=at[0];
    buf[17]=at[1];
    for(i=0;i<datacnt;i++){
      buf[18+i]=data[i];
    }
    gensum(buf);
    rc = 3 + buf[1]*256+buf[2]+1; 
    resp_msg_id[buf[4]] = 0;
    rc = sp_snd(buf,rc);
    if(rc > 0){
      prt(buf,sizeof(msg),msg);
      loga("%-30s Sent %3d bytes: %c%c %016lX %04X %s\n",getFrameTypeName(buf[3]),
        rc,buf[16],buf[17],bswap_64(*(u_int64_t *)(buf +5)),
        bswap_16(*(u_int16_t *)(buf+13)),msg);
      pthread_mutex_lock(&apilock);
      if(resp_msg_id[buf[4]] == 0){
        clock_gettime(CLOCK_REALTIME,&abstime);
        abstime.tv_sec += 5;
        pthread_cond_timedwait(&apicond,&apilock,&abstime);
      }
      pthread_mutex_unlock(&apilock);
    }else{
      fprintf(stderr,"ERROR: sending: rc = %d\n",rc);
    }
  return(rc);
}

// ----------------------------------------------------------------------------
static void process_frame(u_int8_t *buf)
{
  int cnt;
  char msg[256];

  cnt=buf[1]*256+buf[2]+4;
  loga("%-30s Recv %3d bytes: ",getFrameTypeName(buf[3]),cnt);
  prt(buf,sizeof(msg),msg);
  switch(buf[3]){
    case AT_COMMAND_RESPONSE :
      if(buf[5]=='M' && buf[6]=='Y'){
        node[local_node].network_id = bswap_16(*(u_int16_t *)(buf+7));
        loga("%c%c %16s %4s %s\n",buf[5],buf[6],"","",msg);
      }else if(buf[5] == 'S' && buf[6] == 'H'){
        node[local_node].serial = ((u_int64_t)bswap_32(*(u_int32_t *)(buf+8))) * 0x100000000l;
        loga("%c%c %16s %4s %s\n",buf[5],buf[6],"","",msg);
      }else if(buf[5] == 'S' && buf[6] == 'L'){
        node[local_node].serial += (u_int64_t)bswap_32(*(u_int32_t *)(buf+8));
        loga("%c%c %16s %4s %s\n",buf[5],buf[6],"","",msg);
      }else if(buf[5] == 'N' && buf[6] == 'I'){
        memcpy(node[local_node].name,(buf+8),cnt-9);
        node[local_node].name[cnt-9+1] = 0;
        loga("%c%c %16s %4s %s\n",buf[5],buf[6],"","",msg);
      }else if(buf[5] == 'N' && buf[6] == 'D'){
        u_int64_t serial;
        int tn,nil,i;
        if(buf[7] != 0){
          loga("%c%c %16s %4s %s ERROR rc=%d\n",buf[5],buf[6],"","",msg,buf[7]);
        }else{
          serial = bswap_64(*(u_int64_t *)(buf+10));
          tn = find_node(serial);
          if(tn == -1){
            tn = node_count++;
          }
          node[tn].network_id = bswap_16(*(u_int16_t *)(buf+8));
          node[tn].serial = serial;
          for(i=0;buf[18+i] != 0 && i<=20;i++){
            node[tn].name[i] = buf[18+i];
          }
          node[tn].name[i++] = 0;
          i += 18;
          node[tn].parent_network_id = bswap_16(*(u_int16_t *)(buf+i));
          i+=2;
          node[tn].node_type = buf[i];
          i+=1;
          node[tn].status = buf[i];
          i+=1;
          node[tn].profile_id = bswap_16(*(u_int16_t *)(buf+i));
          i+=2;
          node[tn].mfg_id = bswap_16(*(u_int16_t *)(buf+i));
          i+=2;
          loga("%c%c %016lX %04X %s\n",buf[5],buf[6],serial,node[tn].network_id,msg);
        }
      }else if(buf[5] == 'M' && buf[6] == 'P'){
        node[local_node].parent_network_id = bswap_16(*(u_int32_t *)(buf+8));
        loga("%c%c %16s %4s %s\n",buf[5],buf[6],"","",msg);
      }
      break;
    case REMOTE_AT_COMMAND_RESPONSE :
      loga("%c%c %016lX %04X %s\n",buf[15],buf[16],bswap_64(*(u_int64_t *)(buf +5)),bswap_16(*(u_int16_t *)(buf+13)),msg);
      break;
    case RX_DATA_RECEIVED :
      {
      u_int64_t serial=bswap_64(*(u_int64_t *)(buf +4));
      int ni=find_node(serial);
      int vo=19,ai=3;
      if(ni>=0){
      node[ni].digital_channel_mask = bswap_16(*(u_int16_t *)(buf +16));
      node[ni].analog_channel_mask = buf[18];
      if(node[ni].digital_channel_mask){
        node[ni].digital_sample = bswap_16(*(u_int16_t *)(buf +vo));
        vo += 2;
      }
      if(node[ni].analog_channel_mask & ACM_A0){
        node[ni].analog_sample[0] = bswap_16(*(u_int16_t *)(buf +vo));
        vo += 2;
      }
      if(node[ni].analog_channel_mask & ACM_A1){
        node[ni].analog_sample[1] = bswap_16(*(u_int16_t *)(buf +vo));
        vo += 2;
      }
      if(node[ni].analog_channel_mask & ACM_A2){
        node[ni].analog_sample[2] = bswap_16(*(u_int16_t *)(buf +vo));
        vo += 2;
      }
      if(node[ni].analog_channel_mask & ACM_A3){
        node[ni].analog_sample[3] = bswap_16(*(u_int16_t *)(buf +vo));
        vo += 2;
      }
      }
      loga("-- %016lX %04X %s\n", serial,bswap_16(*(u_int16_t *)(buf+13)),msg);
      }
      break;
    case NODE_IDENTIFICATION_INDICATOR :
    case MODEM_STATUS :
    case TX_RESPONSE :
    case RX_RECEIVED :
    default :
      loga("%c%c %16s %4s %s\n",'-','-',"Unknown","----",msg);
      break;
  }
  switch(buf[3]){
    case AT_COMMAND_RESPONSE :
    case TX_RESPONSE :
    case REMOTE_AT_COMMAND_RESPONSE :
      if(buf[4]>0){
        pthread_mutex_lock(&apilock);
        resp_msg_id[buf[4]]++;
        pthread_cond_signal(&apicond);
        pthread_mutex_unlock(&apilock);
    }
  }
}

// ----------------------------------------------------------------------------
static int discover_network(void)
{
  int i;
  printf("Discovering network\n");
  bsm_at_command("ND",0,NULL);
  sleep(5);
	for(i=1;i<node_count;i++){
	  if(node[i].node_type == endpoint){
      bsm_remote_at_command("MP",node[i].serial,node[i].network_id,0,NULL);
		}
	}
  
  print_network();
  return(0);
}

// ----------------------------------------------------------------------------
static int print_network(void)
{
  int i;
  printf("Network with %d nodes:\n",node_count);
  for(i=0;i<node_count;i++){
    printf("Node %2d Name %-20s Serial %016lX NID %04X NT %c PRO_ID %04X MFG_ID %04X ",
      i,node[i].name,node[i].serial,node[i].network_id,
      node[i].node_type==0?'C':node[i].node_type==1?'R':'E',node[i].profile_id,node[i].mfg_id);
    if(node[i].node_type == endpoint){
      printf("PID %04X ",node[i].parent_network_id);
    }else{
      printf("PID ---- ");
    }
    printf("dcm=%02X acm=%1X ds=%02X as3=%02X as2=%02X as1=%02X as0=%02X\n",
      node[i].digital_channel_mask, 
      node[i].analog_channel_mask, 
      node[i].digital_sample, 
      node[i].analog_sample[3], 
      node[i].analog_sample[2], 
      node[i].analog_sample[1], 
      node[i].analog_sample[0]);
  }
  printf("Network end.\n");
  return(0);
}

// ----------------------------------------------------------------------------
static int find_node(u_int64_t serial)
{
  int i;
  for(i=0;i<node_count;i++){
    if(node[i].serial == serial){
      break;
    }
  }
  if(i >= node_count){
    return -1;
  }
  return(i);
}

int  sp_open(char *sp_name);
void sp_close(void);
int  sp_snd(u_int8_t *buf, int len);
int  sp_rcv(u_int8_t *buf);
int  sp_rcvto(int);

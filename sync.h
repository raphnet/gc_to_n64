#ifndef _sync_h__
#define _sync_h__

void sync_init(void);
void sync_master_polled_us(void);
char sync_may_poll(void);

#endif // _sync_h__


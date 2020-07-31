#ifndef __VM_API_H__
#define __VM_API_H__

#include "generic/typedef.h"
#include "app_config.h"

void vm_api_write_mult(u16 start_id, u16 end_id, void *buf, u16 len, u32 delay);
int vm_api_read_mult(u16 start_id, u16 end_id, void *buf, u16 len);

#endif//__VM_API_H__

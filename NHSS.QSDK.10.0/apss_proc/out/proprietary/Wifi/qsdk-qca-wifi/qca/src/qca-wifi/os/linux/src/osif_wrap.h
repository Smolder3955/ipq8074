#ifndef _OSIF_WRAP_H_
#define _OSIF_WRAP_H_

int osif_wrap_attach(wlan_dev_t comhandle);
int osif_wrap_detach(wlan_dev_t comhandle);
int osif_wrap_dev_add(osif_dev *osdev);
void osif_wrap_dev_remove(osif_dev *osdev);
void osif_wrap_dev_remove_vma(osif_dev *osdev);
#endif

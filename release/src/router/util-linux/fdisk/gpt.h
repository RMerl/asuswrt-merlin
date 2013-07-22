#ifndef FDISK_GPT_H
#define FDISK_GPT_H

extern int gpt_probe_signature_fd(int fd);
extern int gpt_probe_signature_devname(char *devname);

#endif /* FDISK_GPT_H */


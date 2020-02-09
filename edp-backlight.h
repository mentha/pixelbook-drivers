#ifndef edp_backlight_h_
#define edp_backlight_h_

int bl_open(void);
void bl_close(int fd);
int bl_setup(int fd);
int bl_max(int fd, int *value);
int bl_set(int fd, int value);
int bl_get(int fd, int *value);

#endif

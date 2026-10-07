#ifndef WSM_RECEIVER_MOUSE_H
#define WSM_RECEIVER_MOUSE_H
#include <stdint.h>

/* 1EA7:0064, mouse report ID 2. Descriptor verified on the attached receiver
 * with Windows HID APIs and synthetic reports (no live input capture):
 * buttons bits 8..15, X 16..27, Y 28..39, wheel 40..47, pan 48..55.
 * Vendor report 181 is not a mouse report. Do not use this for other devices.
 */
static inline int WSM_Decode1EA7Mouse(const uint8_t *data, unsigned size,
                                     uint8_t *buttons, int *x, int *y, int *wheel)
{
 if(!data || size!=7 || data[0]!=2)return 0;
 unsigned rx=(unsigned)data[2]|(((unsigned)data[3]&15)<<8);
 unsigned ry=((unsigned)data[3]>>4)|((unsigned)data[4]<<4);
 *buttons=data[1];*x=rx>=2048 ? (int)rx-4096 : (int)rx;
 *y=ry>=2048 ? (int)ry-4096 : (int)ry;
 *wheel=data[5]>=128 ? (int)data[5]-256 : (int)data[5];
 /* Horizontal pan is intentionally not mapped to menu navigation. */
 return 1;
}
#endif

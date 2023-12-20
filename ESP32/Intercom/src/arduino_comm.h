#ifndef ARDIONO_PROT_H_
#define ARDIONO_PROT_H_

#define  AC_PROTOCOL_VERSION "1.1 " __DATE__ "(" __TIME__ ")"

//Packet Prefix
#define  APP_1  0x55
#define  APP_2  0xBB

//Arduino commands
#define  ACC_VER    1
#define  ACC_ISTAT  2
#define  ACC_SET_RELAY    3
#define  ACC_OSTAT        4
#define  ACC_SET_BTN_LED  5
#define  ACC_GET_ISTAT      10
#define  ACC_GET_OSTAT      11
#define  ACC_PING           12
#define  ACC_SET_BTN2_LED  	13
#define  ACC_ULTRASONIC_DATA 14
#define  ACC_ERROR  0xFF

//Input mask
#define  AIM_BTN    0x1
#define  AIM_INP1   0x2
#define  AIM_INP2   0x4

//Output mask
#define  AOM_RELAY    0x1

//Led modes
#define  ALM_OFF     0x0
#define  ALM_ON      0x1
#define  ALM_SCRIPT_ID(A) ((A<<1)&0xF)


#endif /* ARDIONO_PROT_H_ */

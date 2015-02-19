#ifndef PJS_INTERCOM_MESSAGES_H_
#define PJS_INTERCOM_MESSAGES_H_

SWIG_BEGIN_DECL

//Intercom button state
#define IBS_DOWN	1
#define	IBS_UP		0


#define	IM_QUIT		0
//Main button on intercom. param1 - button state
#define	IM_MAIN_BUTTON	1
#define IM_ASYNC_PLAY_COMPLETED	2
#define IM_ASYNC_COMPLETED		3
#define IM_CONNECTED_CHANGED	4
#define	IM_INPUT1				5
#define	IM_INPUT2				6
#define	IM_ULTRASONIC_DATA 7

SWIG_END_DECL

#endif /* PJS_INTERCOM_MESSAGES_H_ */

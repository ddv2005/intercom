#include "pj_lock.h"
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>

#define THIS_FILE	"pj_support"

#if PJ_ANDROID==1
#include <errno.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <jni.h>
#include <jvm_wrapper.h>
#else
#include <sys/time.h>
#include <sys/resource.h>
#include <linux/sched.h>
#endif

#if PJ_ANDROID==1
extern "C" pj_status_t set_android_thread_priority(int priority){
	jclass process_class;
	jmethodID set_prio_method;
	jthrowable exc;
	JNIEnv *jni_env = 0;
	ATTACH_JVM(jni_env);
	pj_status_t result = PJ_SUCCESS;

	//Get pointer to the java class
	process_class = (jclass)jni_env->NewGlobalRef(jni_env->FindClass("android/os/Process"));
	if (process_class == 0) {
		PJ_LOG(1, (THIS_FILE, "Not able to find os process class"));
		result = PJ_EIGNORED;
		goto on_finish;
	}

	PJ_LOG(4, (THIS_FILE, "We have the class for process"));

	//Get the set priority function
	set_prio_method = jni_env->GetStaticMethodID(process_class, "setThreadPriority", "(I)V");
	if (set_prio_method == 0) {
		PJ_LOG(1, (THIS_FILE, "Not able to find setThreadPriority method"));
		result = PJ_EIGNORED;
		goto on_finish;
	}
	PJ_LOG(4, (THIS_FILE, "We have the method for setThreadPriority"));

	//Call it
	jni_env->CallStaticVoidMethod(process_class, set_prio_method, priority);

	exc = jni_env->ExceptionOccurred();
	if (exc) {
		jni_env->ExceptionDescribe();
		jni_env->ExceptionClear();
		PJ_LOG(2, (THIS_FILE, "Impossible to set priority using java API, fallback to setpriority"));
		setpriority(PRIO_PROCESS, 0, priority);
	}

	on_finish:
		DETACH_JVM(jni_env);
		return result;

}

extern "C" pj_status_t set_max_relative_thread_priority(int relative)
{
	pj_status_t result;
    struct sched_param param;
    pthread_t* thid;

    thid = (pthread_t*) pj_thread_get_os_handle (pj_thread_this());
    param.sched_priority = sched_get_priority_max (SCHED_RR)+relative;
    PJ_LOG (3,(THIS_FILE, "thread(%u): Set thread priority "
		          "to %d.",
		          (unsigned)gettid(),(int)param.sched_priority));
    result = pthread_setschedparam (*thid, SCHED_RR, &param);
    if (result) {
	if (result == EPERM)
	    PJ_LOG (3,(THIS_FILE, "Unable to increase thread priority, "
				  "root access needed."));
	else
	    PJ_LOG (3,(THIS_FILE, "Unable to increase thread priority, "
				  "error: %d",
				  result));
    }
    return result;
}

#else
#include <sys/syscall.h>
extern "C" pj_status_t set_max_relative_thread_priority(int relative)
{
	pj_status_t result;
    struct sched_param param;
    pthread_t* thid;

    thid = (pthread_t*) pj_thread_get_os_handle (pj_thread_this());
    param.__sched_priority = sched_get_priority_max (SCHED_RR)+relative;
    PJ_LOG (3,(THIS_FILE, "thread(%u): Set thread priority "
		          "to %d.",
		          (unsigned)syscall (SYS_gettid),(int)param.__sched_priority));
    result = pthread_setschedparam (*thid, SCHED_RR, &param);
    if (result) {
	if (result == EPERM)
	    PJ_LOG (3,(THIS_FILE, "Unable to increase thread priority, "
				  "root access needed."));
	else
	    PJ_LOG (3,(THIS_FILE, "Unable to increase thread priority, "
				  "error: %d",
				  result));
    }
    return result;
}
#endif

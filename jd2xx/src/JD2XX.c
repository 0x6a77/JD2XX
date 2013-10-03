/*
	Copyright (c) 2004 Pablo Bleyer Kocik.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:

	1. Redistributions of source code must retain the above copyright notice, this
	list of conditions and the following disclaimer.

	2. Redistributions in binary form must reproduce the above copyright notice,
	this list of conditions and the following disclaimer in the documentation
	and/or other materials provided with the distribution.

	3. The name of the author may not be used to endorse or promote products
	derived from this software without specific prior written permission.

	THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR IMPLIED
	WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
	MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
	EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
	SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
	PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
	BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
	IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
	ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
	POSSIBILITY OF SUCH DAMAGE.
*/

// #define DEBUG

#include <jni.h>
#include "jd2xx_JD2XX.h"

#ifdef WIN32
	#include <windows.h>
#endif

#undef WINAPI
#define WINAPI
#include "ftd2xx.h"

#ifndef INVALID_HANDLE_VALUE
#define INVALID_HANDLE_VALUE (-1)
#endif

/* Defines */
#define DESCRIPTION_SIZE 256 // size for serial numbers and descriptions
#define MAX_DEVICES 64 // maximum number of devices to list

/* GLoabl variables */
static JavaVM *javavm;
static jclass JD2XXCls, JD2XXEventListenerCls; // JD2XX class object reference
static jfieldID handleID, eventID, killID, listenerID; // id field object reference
static jclass StringCls; // java.lang.String class object reference
// static jclass pdCls; // ProgramData
// static jclass diCls; // DeviceInfo

/** Error message descriptions */
static char*
status_message[] = {
	"ok",
	"invalid handle",
	"device not found",
	"device not opened",
	"io error",
	"insufficient resources",
	"invalid parameter",
	"invalid baud rate",
	"device not opened for erase",
	"device not opened for write",
	"failed to write device",
	"eeprom read failed",
	"eeprom write failed",
	"eeprom erase failed",
	"eeprom not present",
	"eeprom not programmed",
	"invalid args",
	"not supported",
	"other error",
	"device list not ready"
};

/** Get object handle */
inline static jlong
get_handle(JNIEnv *env, jobject obj) {
	return (*env)->GetLongField(env, obj, handleID);
}

/** Set object handle */
inline static void
set_handle(JNIEnv *env, jobject obj, jlong val) {
	(*env)->SetLongField(env, obj, handleID, val);
}

/** Get event handle */
inline static jint
get_event(JNIEnv *env, jobject obj) {
	return (*env)->GetIntField(env, obj, eventID);
}

/** Set event handle */
inline static void
set_event(JNIEnv *env, jobject obj, jint val) {
	(*env)->SetIntField(env, obj, eventID, val);
}

/** Get kill value */
inline static jint
get_kill(JNIEnv *env, jobject obj) {
	return (*env)->GetBooleanField(env, obj, killID);
}

/** Get listener */
inline static jobject
get_listener(JNIEnv *env, jobject obj) {
	return (*env)->GetObjectField(env, obj, listenerID);
}

/** Throw exception */
inline static void
io_exception(JNIEnv *env, const char *msg) {
	// jclass exc = (*env)->FindClass(env, "java/lang/RuntimeException");
	jclass exc = (*env)->FindClass(env, "java/io/IOException");
	if (exc == 0) return;
	(*env)->ThrowNew(env, exc, msg);
	(*env)->DeleteLocalRef(env, exc);
}

/** Format exception error message */
inline static char*
format_status(char *buf, FT_STATUS st) {
	if (buf != NULL)
		sprintf(buf, "%s (%d)",
			status_message[(st <= FT_OTHER_ERROR) ? st : FT_OTHER_ERROR],
			st
		);
	return buf;
}

/** Format error message and throw exception */
inline static void
io_exception_status(JNIEnv *env, FT_STATUS st) {
	char msg[64];
	io_exception(env, format_status(msg, st));
}

/** Initialize JD2XX driver objects */
JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM *jvm, void *reserved) {
	JNIEnv *env;
	jclass cls;

	if ((*jvm)->GetEnv(jvm, (void **)&env, JNI_VERSION_1_2)) {
		return JNI_ERR; // not supported
	}

	// cls = (*env)->GetObjectClass(env, obj);
	cls = (*env)->FindClass(env, "Ljd2xx/JD2XX;");
	if (cls == 0) return JNI_ERR;
	JD2XXCls = (*env)->NewWeakGlobalRef(env, cls); // weak allows library unloading
	(*env)->DeleteLocalRef(env, cls);
	if (JD2XXCls == 0) return JNI_ERR;

	handleID = (*env)->GetFieldID(env, JD2XXCls, "handle", "J");
	if (handleID == 0) return JNI_ERR;

	eventID = (*env)->GetFieldID(env, JD2XXCls, "event", "I");
	if (eventID == 0) return JNI_ERR;

	killID = (*env)->GetFieldID(env, JD2XXCls, "kill", "Z");
	if (killID == 0) return JNI_ERR;

	listenerID = (*env)->GetFieldID(env, JD2XXCls, "listener", "Ljd2xx/JD2XXEventListener;");
	if (listenerID == 0) return JNI_ERR;

	cls = (*env)->FindClass(env, "Ljd2xx/JD2XXEventListener;");
	if (cls == 0) return JNI_ERR;
	JD2XXEventListenerCls = (*env)->NewWeakGlobalRef(env, cls);
	(*env)->DeleteLocalRef(env, cls);
	if (JD2XXEventListenerCls == 0) return JNI_ERR;

	cls = (*env)->FindClass(env, "java/lang/String");
	if (cls == 0) return JNI_ERR;
	StringCls = (*env)->NewWeakGlobalRef(env, cls);
	(*env)->DeleteLocalRef(env, cls);
	if (StringCls == 0) return JNI_ERR;

//	cls = (*env)->FindClass(env, "Ljd2xx/JD2XX$ProgramData;");
//	if (cls == 0) return JNI_ERR;
//	pdCls = (*env)->NewWeakGlobalRef(env, cls);
//	if (pdCls == 0) return JNI_ERR;

	javavm = jvm; // initialize jvm pointer

	return JNI_VERSION_1_2;
}

/** Finalize JD2xx driver */
JNIEXPORT void JNICALL
JNI_OnUnLoad(JavaVM *jvm, void *reserved) {
	JNIEnv *env;

	if ((*jvm)->GetEnv(jvm, (void **)&env, JNI_VERSION_1_2)) {
		return; // not supported
	}

	(*env)->DeleteWeakGlobalRef(env, JD2XXCls);
	(*env)->DeleteWeakGlobalRef(env, JD2XXEventListenerCls);
	(*env)->DeleteWeakGlobalRef(env, StringCls);

//	fprintf(stderr,  "Bye!\n");
//	fflush(stderr);
}


JNIEXPORT jint JNICALL
Java_jd2xx_JD2XX_getLibraryVersion(JNIEnv *env, jobject obj) {
	FT_STATUS st;
	volatile DWORD ver;

	if (!FT_SUCCESS(st = FT_GetLibraryVersion(&ver)))
		io_exception_status(env, st);

	return (jint)ver;
}

JNIEXPORT void JNICALL
Java_jd2xx_JD2XX_rescan(JNIEnv *env, jobject obj) {
	FT_STATUS st;

#ifdef WIN32
	if (!FT_SUCCESS(st = FT_Rescan()))
		io_exception_status(env, st);
#else
	// Not available outside of Win32.  See FTDI D2XX docs.
	io_exception_status(env, FT_NOT_SUPPORTED);
#endif
}

JNIEXPORT jint JNICALL
Java_jd2xx_JD2XX_createDeviceInfoList(JNIEnv *env, jobject obj) {
	FT_STATUS st;
	volatile DWORD n;

	if (!FT_SUCCESS(st = FT_CreateDeviceInfoList(&n)))
		io_exception_status(env, st);

	return (jint)n;
}

JNIEXPORT jobject JNICALL
Java_jd2xx_JD2XX_getDeviceInfoDetail(JNIEnv *env, jobject obj, jint dn) {
	FT_STATUS st;
	jobject result;

	jclass dicls;
	volatile jfieldID fid; //!!! volatile: MinGW bug hack! Access violation at fid
	jint deviceFlags, deviceType, deviceID, deviceLocation, deviceHandle;
	jstring str;

	char serialNumber[DESCRIPTION_SIZE];
	char description[DESCRIPTION_SIZE];

	if (!FT_SUCCESS(st = FT_GetDeviceInfoDetail(
		(DWORD)dn,
		(DWORD*)&deviceFlags, (DWORD*)&deviceType,
		(DWORD*)&deviceID, (DWORD*)&deviceLocation,
		serialNumber, description, (FT_HANDLE)&deviceHandle)
	)) {
		io_exception_status(env, st);
		return NULL;
	}

	// fprintf(stderr, "%x %x %s %s\n", deviceType, deviceID, serialNumber, description);

	dicls = (*env)->FindClass(env, "Ljd2xx/JD2XX$DeviceInfo;");
	if (dicls == 0) return NULL;

	result = (*env)->AllocObject(env, dicls);
	if (result == 0) goto panic;

	if ((fid = (*env)->GetFieldID(env, dicls, "flags", "I")) == 0) goto panic;
	(*env)->SetIntField(env, result, fid, deviceFlags);

	if ((fid = (*env)->GetFieldID(env, dicls, "type", "I")) == 0) goto panic;
	(*env)->SetIntField(env, result, fid, deviceType);

	if ((fid = (*env)->GetFieldID(env, dicls, "id", "I")) == 0) goto panic;
	(*env)->SetIntField(env, result, fid, deviceID);

	if ((fid = (*env)->GetFieldID(env, dicls, "location", "I")) == 0) goto panic;
	(*env)->SetIntField(env, result, fid, deviceLocation);

	if ((fid = (*env)->GetFieldID(env, dicls, "serial", "Ljava/lang/String;")) == 0) goto panic;
	if ((str = (*env)->NewStringUTF(env, serialNumber)) == 0) goto panic;
	// str = (*env)->GetObjectField(env, result, fid);
	// fprintf(stderr, "%x\n", fid);
	(*env)->SetObjectField(env, result, fid, str);

	if ((fid = (*env)->GetFieldID(env, dicls, "description", "Ljava/lang/String;")) == 0) goto panic;
	if ((str = (*env)->NewStringUTF(env, description)) == 0) goto panic;
	(*env)->SetObjectField(env, result, fid, str);

	if ((fid = (*env)->GetFieldID(env, dicls, "handle", "J")) == 0) goto panic;
	(*env)->SetIntField(env, result, fid, deviceHandle);

	(*env)->DeleteLocalRef(env, dicls);
	return result;

panic:
	(*env)->DeleteLocalRef(env, result);
	(*env)->DeleteLocalRef(env, dicls);
	return NULL;
}

JNIEXPORT void JNICALL
Java_jd2xx_JD2XX_open(JNIEnv *env, jobject obj, jint dn) {
	jlong hnd = get_handle(env, obj);

	if (hnd != (jint)INVALID_HANDLE_VALUE) // previously initialized!
		io_exception(env, "device already opened");
	else {
		FT_HANDLE h;
		FT_STATUS st = FT_Open(dn, &h);
		//fprintf(stderr, "FT_Open succeeded.  Handle is %p\n", h);

		if (FT_SUCCESS(st)) set_handle(env, obj, (jlong)h);
		else io_exception_status(env, st);
	}
}

JNIEXPORT void JNICALL
Java_jd2xx_JD2XX_openEx__Ljava_lang_String_2I(JNIEnv *env, jobject obj, jstring str, jint flg) {
	jlong hnd = get_handle(env, obj);

	if (hnd != (jint)INVALID_HANDLE_VALUE) // previously initialized!
		io_exception(env, "device already opened");
	else {
		const char *cstr = (*env)->GetStringUTFChars(env, str, 0);
		FT_HANDLE h;
		FT_STATUS st = FT_OpenEx((PVOID)cstr, (DWORD)flg, &h);
		(*env)->ReleaseStringUTFChars(env, str, cstr);

		if (FT_SUCCESS(st)) set_handle(env, obj, (jlong)h);
		else io_exception_status(env, st);
	}
}

JNIEXPORT void JNICALL
Java_jd2xx_JD2XX_openEx__II(JNIEnv *env, jobject obj, jint num, jint flg) {
	jlong hnd = get_handle(env, obj);

	if (hnd != (jint)INVALID_HANDLE_VALUE) // previously initialized!
		io_exception(env, "device already opened");
	else {
		FT_HANDLE h;
		FT_STATUS st = FT_OpenEx((PVOID)num, (DWORD)flg, &h);

		if (FT_SUCCESS(st)) set_handle(env, obj, (jlong)h);
		else io_exception_status(env, st);
	}
}

JNIEXPORT void JNICALL
Java_jd2xx_JD2XX_close(JNIEnv *env, jobject obj) {
	jlong hnd = get_handle(env, obj);

//	if (hnd == (jint)INVALID_HANDLE_VALUE) {
//		// not open!
//		// fprintf(stderr,  "Ouch!\n");
//		io_exception(env, "device not opened");
//	}
//	else {

	if (hnd != (jint)INVALID_HANDLE_VALUE) {
		FT_STATUS st = FT_Close((FT_HANDLE)hnd);
		if (!FT_SUCCESS(st)) io_exception_status(env, st);
		else set_handle(env, obj, (jlong)INVALID_HANDLE_VALUE);
	}
}

/**
	@todo Check LIST_BY_LOCATION (array of Integers) implementation
*/
JNIEXPORT jobjectArray JNICALL
Java_jd2xx_JD2XX_listDevices(JNIEnv *env, jobject obj, jint flg) {
	jobjectArray result;
	int r = 0;
	FT_STATUS st;
	volatile DWORD n; //!!! volatile: MinGW GCC bug hack, disable optimization
	int i;

	// first search for number of devices
	st = FT_ListDevices((PVOID)&n, NULL, FT_LIST_NUMBER_ONLY);
	if (!FT_SUCCESS(st)) {
		io_exception_status(env, st);
		return NULL;
	}

	n = (n <= MAX_DEVICES) ? n : MAX_DEVICES;

	if (flg & jd2xx_JD2XX_OPEN_BY_LOCATION) {
		DWORD ba[n];
		jclass icls;
		jmethodID mid;

		icls = (*env)->FindClass(env, "java/lang/Integer");
		if (icls == 0) return NULL;
		mid = (*env)->GetMethodID(env, icls, "<init>", "(I)V");
		if (mid == 0) return NULL;

		result = (*env)->NewObjectArray(env, n, icls, 0);
		if (result == NULL || n == 0) return result;

		st = FT_ListDevices((PVOID)ba, (PVOID)&n, FT_LIST_ALL | (DWORD)flg);
		if (!FT_SUCCESS(st)) {
			io_exception_status(env, st);
			return NULL;
		}

		for (i=0; i<n; ++i) {
			jobject iobj = (*env)->NewObject(env, icls, mid, (jint)ba[i]);
			if (iobj == 0) return NULL;
			(*env)->SetObjectArrayElement(env, result, i, iobj);
			// fprintf(stderr, "%d: %s\n", i, ba[i]);
		}

		(*env)->DeleteLocalRef(env, icls);
	}
	else {
		char *ba[MAX_DEVICES+1];
		char bd[MAX_DEVICES*DESCRIPTION_SIZE];

		result = (*env)->NewObjectArray(env, n, StringCls, 0);
		if (result == NULL || n == 0) return result;

		for (i=0; i<n; ++i) ba[i] = bd + i*DESCRIPTION_SIZE;
		ba[n] = NULL;

		st = FT_ListDevices((PVOID)ba, (PVOID)&n, FT_LIST_ALL | (DWORD)flg);
		if (!FT_SUCCESS(st)) {
			io_exception_status(env, st);
			return NULL;
		}

		// fprintf(stderr, "%s\n", bd);
		// fprintf(stderr, "%d\n", n);

		for (i=0; i<n; ++i) {
			jstring str = (*env)->NewStringUTF(env, ba[i]);
			if (str == 0) return NULL;
			(*env)->SetObjectArrayElement(env, result, i, str);
			// fprintf(stderr, "%d: %s\n", i, ba[i]);
		}
	}

	return result;
}

JNIEXPORT jint JNICALL
Java_jd2xx_JD2XX_read(JNIEnv *env, jobject obj, jbyteArray arr, jint off, jint len) {
	FT_STATUS st;
	volatile DWORD ret;
	jlong hnd = get_handle(env, obj);
	int alen = (*env)->GetArrayLength(env, arr);
	jbyte *buf;

	if (arr == 0) {
		jclass exc = (*env)->FindClass(env, "java/lang/NullPointerException");
		if (exc != 0) (*env)->ThrowNew(env, exc, NULL);
		(*env)->DeleteLocalRef(env, exc);
		return 0;
	} else if ((off < 0) || (off > alen) || (len < 0)
		|| ((off + len) > alen) || ((off + len) < 0)) {
		jclass exc = (*env)->FindClass(env, "java/lang/IndexOutOfBoundsException");
		if (exc != 0) (*env)->ThrowNew(env, exc, NULL);
		(*env)->DeleteLocalRef(env, exc);
		return 0;
	} else if (len == 0) return 0;

	buf = (*env)->GetByteArrayElements(env, arr, 0);

	if (!FT_SUCCESS(st = FT_Read((FT_HANDLE)hnd, (LPVOID)(buf+off), len, (LPDWORD)&ret)))
		io_exception_status(env, st);

//	result = (*env)->NewByteArray(env, ret);
//	if (result != 0) (*env)->SetByteArrayRegion(env, result, 0, ret, buf);
	(*env)->ReleaseByteArrayElements(env, arr, buf, 0);
	return (jint)ret;
}

JNIEXPORT jint JNICALL
Java_jd2xx_JD2XX_write(JNIEnv *env, jobject obj, jbyteArray arr, jint off, jint len) {
	FT_STATUS st;
	volatile DWORD ret;
	jlong hnd = get_handle(env, obj);
	int alen = (*env)->GetArrayLength(env, arr);
	jbyte *buf;

	if (arr == 0) {
		jclass exc = (*env)->FindClass(env, "java/lang/NullPointerException");
		if (exc != 0) (*env)->ThrowNew(env, exc, NULL);
		(*env)->DeleteLocalRef(env, exc);
		return 0;
	} else if ((off < 0) || (off > alen) || (len < 0)
		|| ((off + len) > alen) || ((off + len) < 0)) {
		jclass exc = (*env)->FindClass(env, "java/lang/IndexOutOfBoundsException");
		if (exc != 0) (*env)->ThrowNew(env, exc, NULL);
		(*env)->DeleteLocalRef(env, exc);
		return 0;
	} else if (len == 0) return 0;

 	buf = (*env)->GetByteArrayElements(env, arr, 0);

	if (!FT_SUCCESS(st = FT_Write((FT_HANDLE)hnd, (LPVOID)(buf+off), len, (LPDWORD)&ret)))
		io_exception_status(env, st);

	(*env)->ReleaseByteArrayElements(env, arr, buf, 0);
	return (jint)ret;
}

JNIEXPORT void JNICALL
Java_jd2xx_JD2XX_setBaudRate(JNIEnv *env, jobject obj, jint br) {
	FT_STATUS st;
	jlong hnd = get_handle(env, obj);

	if (!FT_SUCCESS(st = FT_SetBaudRate((FT_HANDLE)hnd, (DWORD)br)))
		io_exception_status(env, st);
}

JNIEXPORT void JNICALL
Java_jd2xx_JD2XX_setDivisor(JNIEnv *env, jobject obj, jint div) {
	FT_STATUS st;
	jlong hnd = get_handle(env, obj);

	if (!FT_SUCCESS(st = FT_SetDivisor((FT_HANDLE)hnd, (USHORT)div)))
		io_exception_status(env, st);
}

JNIEXPORT void JNICALL
Java_jd2xx_JD2XX_setDataCharacteristics(
	JNIEnv *env, jobject obj, jint wl, jint sb, jint pr) {
	FT_STATUS st;
	jlong hnd = get_handle(env, obj);

	if (!FT_SUCCESS(st = FT_SetDataCharacteristics((FT_HANDLE)hnd,
		(UCHAR)wl, (UCHAR)sb, (UCHAR)pr)))
		io_exception_status(env, st);
}

JNIEXPORT void JNICALL
Java_jd2xx_JD2XX_setFlowControl(
	JNIEnv *env, jobject obj, jint fc, jint xon, jint xoff) {
	FT_STATUS st;
	jlong hnd = get_handle(env, obj);

	if (!FT_SUCCESS(st = FT_SetFlowControl((FT_HANDLE)hnd,
		(USHORT)fc, (UCHAR)xon, (UCHAR)xoff)))
		io_exception_status(env, st);
}

JNIEXPORT void JNICALL
Java_jd2xx_JD2XX_resetDevice(JNIEnv *env, jobject obj) {
	FT_STATUS st;
	jlong hnd = get_handle(env, obj);

	if (!FT_SUCCESS(st = FT_ResetDevice((FT_HANDLE)hnd)))
		io_exception_status(env, st);
}

JNIEXPORT void JNICALL
Java_jd2xx_JD2XX_setDtr(JNIEnv *env, jobject obj) {
	FT_STATUS st;
	jlong hnd = get_handle(env, obj);

	if (!FT_SUCCESS(st = FT_SetDtr((FT_HANDLE)hnd)))
		io_exception_status(env, st);
}

JNIEXPORT void JNICALL
Java_jd2xx_JD2XX_clrDtr(JNIEnv *env, jobject obj) {
	FT_STATUS st;
	jlong hnd = get_handle(env, obj);

	if (!FT_SUCCESS(st = FT_ClrDtr((FT_HANDLE)hnd)))
		io_exception_status(env, st);
}

JNIEXPORT void JNICALL
Java_jd2xx_JD2XX_setRts(JNIEnv *env, jobject obj) {
	FT_STATUS st;
	jlong hnd = get_handle(env, obj);

	if (!FT_SUCCESS(st = FT_SetRts((FT_HANDLE)hnd)))
		io_exception_status(env, st);
}

JNIEXPORT void JNICALL
Java_jd2xx_JD2XX_clrRts(JNIEnv *env, jobject obj) {
	FT_STATUS st;
	jlong hnd = get_handle(env, obj);

	if (!FT_SUCCESS(st = FT_ClrRts((FT_HANDLE)hnd)))
		io_exception_status(env, st);
}

JNIEXPORT jint JNICALL
Java_jd2xx_JD2XX_getModemStatus(JNIEnv *env, jobject obj) {
	FT_STATUS st;
	jlong hnd = get_handle(env, obj);
	volatile ULONG ms;

	if (!FT_SUCCESS(st = FT_GetModemStatus((FT_HANDLE)hnd, &ms)))
		io_exception_status(env, st);

	return (jint)ms;
}

JNIEXPORT void JNICALL
Java_jd2xx_JD2XX_setChars(
	JNIEnv *env, jobject obj,
	jint evc, jboolean eve, jint erc, jboolean ere
) {
	FT_STATUS st;
	jlong hnd = get_handle(env, obj);

	if (!FT_SUCCESS(st = FT_SetChars((FT_HANDLE)hnd,
		(UCHAR)evc, eve ? 1 : 0, (UCHAR)erc, ere ? 1 : 0)))
		io_exception_status(env, st);
}

JNIEXPORT void JNICALL
Java_jd2xx_JD2XX_purge(JNIEnv *env, jobject obj, jint msk) {
	FT_STATUS st;
	jlong hnd = get_handle(env, obj);

	if (!FT_SUCCESS(st = FT_Purge((FT_HANDLE)hnd, (DWORD)msk)))
		io_exception_status(env, st);
}

JNIEXPORT void JNICALL
Java_jd2xx_JD2XX_setTimeouts(JNIEnv *env, jobject obj, jint rt, jint wt) {
	FT_STATUS st;
	jlong hnd = get_handle(env, obj);

	if (!FT_SUCCESS(st = FT_SetTimeouts((FT_HANDLE)hnd, (DWORD)rt, (DWORD)wt)))
		io_exception_status(env, st);
}

JNIEXPORT jint JNICALL
Java_jd2xx_JD2XX_getQueueStatus(JNIEnv *env, jobject obj) {
	FT_STATUS st;
	jlong hnd = get_handle(env, obj);
	volatile DWORD r;

	if (!FT_SUCCESS(st = FT_GetQueueStatus((FT_HANDLE)hnd, &r)))
		io_exception_status(env, st);

	return (jint)r;
}

JNIEXPORT void JNICALL
Java_jd2xx_JD2XX_setEventNotification(JNIEnv *env, jobject obj, jint msk, jint evh) {
	FT_STATUS st;
	jlong hnd = get_handle(env, obj);

	if (!FT_SUCCESS(
		st = FT_SetEventNotification((FT_HANDLE)hnd, (DWORD)msk, (HANDLE)evh)
	)) io_exception_status(env, st);
}

JNIEXPORT jintArray JNICALL
Java_jd2xx_JD2XX_getStatus(JNIEnv *env, jobject obj) {
	FT_STATUS st;
	jlong hnd = get_handle(env, obj);
	jintArray result;
	volatile DWORD rte[3]; // rx, tx, ev;

	if (!FT_SUCCESS(st = FT_GetStatus((FT_HANDLE)hnd, rte+0, rte+1, rte+2))) {
		io_exception_status(env, st);
		return NULL;
	}

	result = (*env)->NewIntArray(env, 3);
	if (result != 0) (*env)->SetIntArrayRegion(env, result, 0, 3, (jint *)rte);

	return result;
}

JNIEXPORT void JNICALL
Java_jd2xx_JD2XX_setBreakOn(JNIEnv *env, jobject obj) {
	FT_STATUS st;
	jlong hnd = get_handle(env, obj);

	if (!FT_SUCCESS(st = FT_SetBreakOn((FT_HANDLE)hnd)))
		io_exception_status(env, st);
}

JNIEXPORT void JNICALL
Java_jd2xx_JD2XX_setBreakOff(JNIEnv *env, jobject obj) {
	FT_STATUS st;
	jlong hnd = get_handle(env, obj);

	if (!FT_SUCCESS(st = FT_SetBreakOff((FT_HANDLE)hnd)))
		io_exception_status(env, st);
}

JNIEXPORT void JNICALL
Java_jd2xx_JD2XX_setWaitMask(JNIEnv *env, jobject obj, jint msk) {
	FT_STATUS st;
	jlong hnd = get_handle(env, obj);

	if (!FT_SUCCESS(st = FT_SetWaitMask((FT_HANDLE)hnd, (DWORD)msk)))
		io_exception_status(env, st);
}

JNIEXPORT jint JNICALL
Java_jd2xx_JD2XX_waitOnMask(JNIEnv *env, jobject obj) {
	FT_STATUS st;
	jlong hnd = get_handle(env, obj);
	volatile DWORD msk;

	if (!FT_SUCCESS(st = FT_WaitOnMask((FT_HANDLE)hnd, &msk)))
		io_exception_status(env, st);

	return (jint)msk;
}

JNIEXPORT jint JNICALL
Java_jd2xx_JD2XX_getEventStatus(JNIEnv *env, jobject obj) {
	FT_STATUS st;
	jlong hnd = get_handle(env, obj);
	volatile DWORD msk;

	if (!FT_SUCCESS(st = FT_GetEventStatus((FT_HANDLE)hnd, &msk)))
		io_exception_status(env, st);

	return (jint)msk;
}

JNIEXPORT void JNICALL
Java_jd2xx_JD2XX_setLatencyTimer(JNIEnv *env, jobject obj, jint tmr) {
	FT_STATUS st;
	jlong hnd = get_handle(env, obj);

	if (!FT_SUCCESS(st = FT_SetLatencyTimer((FT_HANDLE)hnd, (UCHAR)tmr)))
		io_exception_status(env, st);
}

JNIEXPORT jint JNICALL
Java_jd2xx_JD2XX_getLatencyTimer(JNIEnv *env, jobject obj) {
	FT_STATUS st;
	jlong hnd = get_handle(env, obj);
	volatile UCHAR tmr;

	if (!FT_SUCCESS(st = FT_GetLatencyTimer((FT_HANDLE)hnd, &tmr)))
		io_exception_status(env, st);

	return (jint)tmr;
}


/*
ucMask Required value for bit mode mask. This sets up which bits are
inputs and outputs. A bit value of 0 sets the corresponding pin
to an input, a bit value of 1 sets the corresponding pin to an
output.
In the case of CBUS Bit Bang, the upper nibble of this value
controls which pins are inputs and outputs, while the lower
nibble controls which of the outputs are high and low.
ucMode Mode value. Can be one of the following:
0x0 = Reset
0x1 = Asynchronous Bit Bang
0x2 = MPSSE (FT2232C devices only)
0x4 = Synchronous Bit Bang (FT232R, FT245R and FT2232C
devices only)
0x8 = MCU Host Bus Emulation Mode (FT2232C devices only)
0x10 = Fast Opto-Isolated Serial Mode (FT2232C devices only)
0x20 = CBUS Bit Bang Mode (FT232R devices only)
*/

JNIEXPORT void JNICALL
Java_jd2xx_JD2XX_setBitMode(JNIEnv *env, jobject obj, jint msk, jint mod) {
	FT_STATUS st;
	jlong hnd = get_handle(env, obj);

	if (!FT_SUCCESS(st = FT_SetBitMode((FT_HANDLE)hnd, (UCHAR)msk, (UCHAR)mod)))
		io_exception_status(env, st);
}

JNIEXPORT jint JNICALL
Java_jd2xx_JD2XX_getBitMode(JNIEnv *env, jobject obj) {
	FT_STATUS st;
	jlong hnd = get_handle(env, obj);
	volatile UCHAR msk;

	if (!FT_SUCCESS(st = FT_GetBitMode((FT_HANDLE)hnd, &msk)))
		io_exception_status(env, st);

	return (jint)msk;
}

JNIEXPORT void JNICALL
Java_jd2xx_JD2XX_setUSBParameters(JNIEnv *env, jobject obj, jint isz, jint osz) {
	FT_STATUS st;
	jlong hnd = get_handle(env, obj);

	if (!FT_SUCCESS(st = FT_SetUSBParameters((FT_HANDLE)hnd, (ULONG)isz, (ULONG)osz)))
		io_exception_status(env, st);
}

JNIEXPORT void JNICALL
Java_jd2xx_JD2XX_FT_setDeadmanTimeout(JNIEnv *env, jobject obj, jint dto) {
	FT_STATUS st;
	jlong hnd = get_handle(env, obj);

	if (!FT_SUCCESS(st = FT_SetDeadmanTimeout((FT_HANDLE)hnd, (ULONG)dto)))
		io_exception_status(env, st);
}

JNIEXPORT jobject JNICALL
Java_jd2xx_JD2XX_getDeviceInfo(JNIEnv *env, jobject obj) {
	FT_STATUS st;
	jlong hnd = get_handle(env, obj);
	jobject result;

	jclass dicls;
	volatile jfieldID fid; //!!! volatile: MinGW bug hack! Access violation at fid
	jint deviceType, deviceID;
	jstring str;

	char serialNumber[DESCRIPTION_SIZE];
	char description[DESCRIPTION_SIZE];

	if (!FT_SUCCESS(st = FT_GetDeviceInfo(
		(FT_HANDLE)hnd, (FT_DEVICE*)&deviceType, (DWORD*)&deviceID,
		serialNumber, description, NULL)
	)) {
		io_exception_status(env, st);
		return NULL;
	}

	// fprintf(stderr, "%x %x %s %s\n", deviceType, deviceID, serialNumber, description);

	dicls = (*env)->FindClass(env, "Ljd2xx/JD2XX$DeviceInfo;");
	if (dicls == 0) return NULL;

	result = (*env)->AllocObject(env, dicls);
	if (result == 0) goto panic;

	if ((fid = (*env)->GetFieldID(env, dicls, "type", "I")) == 0) goto panic;
	(*env)->SetIntField(env, result, fid, deviceType);

	if ((fid = (*env)->GetFieldID(env, dicls, "id", "I")) == 0) goto panic;
	(*env)->SetIntField(env, result, fid, deviceID);

	if ((fid = (*env)->GetFieldID(env, dicls, "serial", "Ljava/lang/String;")) == 0) goto panic;
	if ((str = (*env)->NewStringUTF(env, serialNumber)) == 0) goto panic;
	// str = (*env)->GetObjectField(env, result, fid);
	// fprintf(stderr, "%x\n", fid);
	(*env)->SetObjectField(env, result, fid, str);

	if ((fid = (*env)->GetFieldID(env, dicls, "description", "Ljava/lang/String;")) == 0) goto panic;
	if ((str = (*env)->NewStringUTF(env, description)) == 0) goto panic;
	(*env)->SetObjectField(env, result, fid, str);

	(*env)->DeleteLocalRef(env, dicls);
	return result;

panic:
	(*env)->DeleteLocalRef(env, result);
	(*env)->DeleteLocalRef(env, dicls);
	return NULL;
}

JNIEXPORT void JNICALL
Java_jd2xx_JD2XX_stopInTask(JNIEnv *env, jobject obj) {
	FT_STATUS st;
	jlong hnd = get_handle(env, obj);

	if (!FT_SUCCESS(st = FT_StopInTask((FT_HANDLE)hnd)))
		io_exception_status(env, st);
}

JNIEXPORT void JNICALL
Java_jd2xx_JD2XX_restartInTask(JNIEnv *env, jobject obj) {
	FT_STATUS st;
	jlong hnd = get_handle(env, obj);

	if (!FT_SUCCESS(st = FT_RestartInTask((FT_HANDLE)hnd)))
		io_exception_status(env, st);
}

JNIEXPORT void JNICALL
Java_jd2xx_JD2XX_setResetPipeRetryCount(JNIEnv *env, jobject obj, jint c) {
	FT_STATUS st;
	jlong hnd = get_handle(env, obj);

	if (!FT_SUCCESS(st = FT_SetResetPipeRetryCount((FT_HANDLE)hnd, (DWORD)c)))
		io_exception_status(env, st);
}

JNIEXPORT void JNICALL
Java_jd2xx_JD2XX_resetPort(JNIEnv *env, jobject obj) {
	FT_STATUS st;
	jlong hnd = get_handle(env, obj);

	if (!FT_SUCCESS(st = FT_ResetPort((FT_HANDLE)hnd)))
		io_exception_status(env, st);
}

JNIEXPORT void JNICALL
Java_jd2xx_JD2XX_cyclePort(JNIEnv *env, jobject obj) {
	FT_STATUS st;
	jlong hnd = get_handle(env, obj);

#ifdef WIN32
	if (!FT_SUCCESS(st = FT_CyclePort((FT_HANDLE)hnd)))
		io_exception_status(env, st);
#else
	// Not available in Linux or OS X
	// See FTDO docs for more details.
	io_exception_status(env, FT_NOT_SUPPORTED);
#endif
}

JNIEXPORT jint JNICALL
Java_jd2xx_JD2XX_getDriverVersion(JNIEnv *env, jobject obj) {
	FT_STATUS st;
	jlong hnd = get_handle(env, obj);
	volatile DWORD ver;

	if (!FT_SUCCESS(st = FT_GetDriverVersion((FT_HANDLE)hnd, &ver)))
		io_exception_status(env, st);

	return (jint)ver;
}

JNIEXPORT void JNICALL
Java_jd2xx_JD2XX_reload(JNIEnv *env, jobject obj, jint vid, jint pid) {
	FT_STATUS st;

#ifdef WIN32
	if (!FT_SUCCESS(st = FT_Reload(vid, pid)))
		io_exception_status(env, st);
#else
	// Not available outside of Win32.  See FTDI D2XX docs.
	io_exception_status(env, FT_NOT_SUPPORTED);
#endif
}

JNIEXPORT jint JNICALL
Java_jd2xx_JD2XX_getComPortNumber(JNIEnv *env, jobject obj) {
	FT_STATUS st;
	jlong hnd = get_handle(env, obj);
	volatile LONG pn;

#ifdef WIN32
	if (!FT_SUCCESS(st = FT_GetComPortNumber((FT_HANDLE)hnd, &pn)))
		io_exception_status(env, st);
#else
	// Not available outside of Win32.  See FTDI D2XX docs.
	io_exception_status(env, FT_NOT_SUPPORTED);
#endif

	return (jint)pn;
}

JNIEXPORT jint JNICALL
Java_jd2xx_JD2XX_eeReadConfig(JNIEnv *env, jobject obj, jint a) {
	FT_STATUS st;
	jlong hnd = get_handle(env, obj);
	UCHAR c;

	if (!FT_SUCCESS(st = FT_EE_ReadConfig((FT_HANDLE)hnd, (UCHAR)a, &c)))
		io_exception_status(env, st);

	return (jint)c;
}

JNIEXPORT void JNICALL
Java_jd2xx_JD2XX_eeWriteConfig(JNIEnv *env, jobject obj, jint a, jint c) {
	FT_STATUS st;
	jlong hnd = get_handle(env, obj);

	if (!FT_SUCCESS(st = FT_EE_WriteConfig((FT_HANDLE)hnd, (UCHAR)a, (UCHAR)c)))
		io_exception_status(env, st);
}

JNIEXPORT jint JNICALL
Java_jd2xx_JD2XX_eeReadEcc(JNIEnv *env, jobject obj, jint opt) {
	FT_STATUS st;
	jlong hnd = get_handle(env, obj);
	WORD v;

#ifdef WIN32
	if (!FT_SUCCESS(st = FT_EE_ReadEcc((FT_HANDLE)hnd, (UCHAR)opt, &v)))
		io_exception_status(env, st);
#else
	// Not available outside of Win32.  See FTDI D2XX docs.
	io_exception_status(env, FT_NOT_SUPPORTED);
#endif

	return (jint)v;
}

JNIEXPORT jint JNICALL
Java_jd2xx_JD2XX_getQueueStatusEx(JNIEnv *env, jobject obj) {
	FT_STATUS st;
	jlong hnd = get_handle(env, obj);
	volatile DWORD r;

	if (!FT_SUCCESS(st = FT_GetQueueStatusEx((FT_HANDLE)hnd, &r)))
		io_exception_status(env, st);

	return (jint)r;
}


JNIEXPORT void JNICALL
Java_jd2xx_JD2XX_eeProgram(JNIEnv *env, jobject obj, jobject pdo) {
	FT_STATUS st;
	FT_PROGRAM_DATA fpd;
	jlong hnd = get_handle(env, obj);

	jclass pdcls = (*env)->GetObjectClass(env, pdo);
	jfieldID fid;
	jstring mstr, istr, dstr, sstr;

	fpd.Signature1 = 0x00000000;
	fpd.Signature2 = 0xffffffff;
	fpd.Version = -1; // force error if not set

	fpd.Manufacturer = 0;
	fpd.ManufacturerId = 0;
	fpd.Description = 0;
	fpd.SerialNumber = 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "signature1", "I"))==0) goto end;
	fpd.Signature1 = (DWORD)(*env)->GetIntField(env, pdo, fid);

	if ((fid = (*env)->GetFieldID(env, pdcls, "signature2", "I"))==0) goto end;
	fpd.Signature2 = (DWORD)(*env)->GetIntField(env, pdo, fid);

	if ((fid = (*env)->GetFieldID(env, pdcls, "version", "I"))==0) goto end;
	fpd.Version = (DWORD)(*env)->GetIntField(env, pdo, fid);

	if ((fid = (*env)->GetFieldID(env, pdcls, "vendorID", "I"))==0) goto end;
	fpd.VendorId = (WORD)(*env)->GetIntField(env, pdo, fid);

	if ((fid = (*env)->GetFieldID(env, pdcls, "productID", "I"))==0) goto end;
	fpd.ProductId = (WORD)(*env)->GetIntField(env, pdo, fid);

	if ((fid = (*env)->GetFieldID(env, pdcls, "manufacturer", "Ljava/lang/String;"))==0) goto end;
	mstr = (*env)->GetObjectField(env, pdo, fid);
	fpd.Manufacturer = (*env)->GetStringUTFChars(env, mstr, 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "manufacturerID", "Ljava/lang/String;"))==0) goto end;
	istr = (*env)->GetObjectField(env, pdo, fid);
	fpd.ManufacturerId = (*env)->GetStringUTFChars(env, istr, 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "description", "Ljava/lang/String;"))==0) goto end;
	dstr = (*env)->GetObjectField(env, pdo, fid);
	fpd.Description = (*env)->GetStringUTFChars(env, dstr, 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "serialNumber", "Ljava/lang/String;"))==0) goto end;
	sstr = (*env)->GetObjectField(env, pdo, fid);
	fpd.SerialNumber = (*env)->GetStringUTFChars(env, sstr, 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "maxPower", "I"))==0) goto end;
	fpd.MaxPower = (WORD)(*env)->GetIntField(env, pdo, fid);

	if ((fid = (*env)->GetFieldID(env, pdcls, "pnp", "Z"))==0) goto end;
	fpd.PnP = (WORD)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "selfPowered", "Z"))==0) goto end;
	fpd.SelfPowered = (WORD)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "remoteWakeup", "Z"))==0) goto end;
	fpd.RemoteWakeup = (WORD)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "rev4", "Z"))==0) goto end;
	fpd.Rev4 = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "isoIn", "Z"))==0) goto end;
	fpd.IsoIn = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "isoOut", "Z"))==0) goto end;
	fpd.IsoOut = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "pullDownEnable", "Z"))==0) goto end;
	fpd.PullDownEnable = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "serNumEnable", "Z"))==0) goto end;
	fpd.SerNumEnable = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "usbVersionEnable", "Z"))==0) goto end;
	fpd.USBVersionEnable = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "usbVersion", "I"))==0) goto end;
	fpd.USBVersion = (WORD)(*env)->GetIntField(env, pdo, fid);

	if ((fid = (*env)->GetFieldID(env, pdcls, "rev5", "Z"))==0) goto end;
	fpd.Rev5 = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "isoInA", "Z"))==0) goto end;
	fpd.IsoInA = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "isoInB", "Z"))==0) goto end;
	fpd.IsoInB = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "isoOutA", "Z"))==0) goto end;
	fpd.IsoOutA = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "isoOutB", "Z"))==0) goto end;
	fpd.IsoOutB = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "pullDownEnable5", "Z"))==0) goto end;
	fpd.PullDownEnable5 = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "serNumEnable5", "Z"))==0) goto end;
	fpd.SerNumEnable5 = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "usbVersionEnable5", "Z"))==0) goto end;
	fpd.USBVersionEnable5 = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "usbVersion5", "I"))==0) goto end;
	fpd.USBVersion5 = (WORD)(*env)->GetIntField(env, pdo, fid);

	if ((fid = (*env)->GetFieldID(env, pdcls, "aIsHighCurrent", "Z"))==0) goto end;
	fpd.AIsHighCurrent = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "bIsHighCurrent", "Z"))==0) goto end;
	fpd.BIsHighCurrent = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "ifAIsFifo", "Z"))==0) goto end;
	fpd.IFAIsFifo = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "ifAIsFifoTar", "Z"))==0) goto end;
	fpd.IFAIsFifoTar = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "ifAIsFastSer", "Z"))==0) goto end;
	fpd.IFAIsFastSer = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "aIsVCP", "Z"))==0) goto end;
	fpd.AIsVCP = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "ifBIsFifo", "Z"))==0) goto end;
	fpd.IFBIsFifo = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "ifBIsFifoTar", "Z"))==0) goto end;
	fpd.IFBIsFifoTar = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "ifBIsFastSer", "Z"))==0) goto end;
	fpd.IFBIsFastSer = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "bIsVCP", "Z"))==0) goto end;
	fpd.BIsVCP = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "endpointSize", "I"))==0) goto end;
	fpd.EndpointSize = (UCHAR)((*env)->GetIntField(env, pdo, fid) & 0xff);

	if ((fid = (*env)->GetFieldID(env, pdcls, "useExtOsc", "Z"))==0) goto end;
	fpd.UseExtOsc = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "highDriveIOs", "Z"))==0) goto end;
	fpd.HighDriveIOs = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "pullDownEnableR", "Z"))==0) goto end;
	fpd.PullDownEnableR = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "serNumEnableR", "Z"))==0) goto end;
	fpd.SerNumEnableR = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "invertTXD", "Z"))==0) goto end;
	fpd.InvertTXD = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "invertRXD", "Z"))==0) goto end;
	fpd.InvertRXD = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "invertRTS", "Z"))==0) goto end;
	fpd.InvertRTS = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "invertCTS", "Z"))==0) goto end;
	fpd.InvertCTS = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "invertDTR", "Z"))==0) goto end;
	fpd.InvertDTR = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "invertDSR", "Z"))==0) goto end;
	fpd.InvertDSR = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "invertDCD", "Z"))==0) goto end;
	fpd.InvertDCD = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "invertRI", "Z"))==0) goto end;
	fpd.InvertRI = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "cbus0", "I"))==0) goto end;
	fpd.Cbus0 = (UCHAR)(*env)->GetIntField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "cbus1", "I"))==0) goto end;
	fpd.Cbus1 = (UCHAR)(*env)->GetIntField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "cbus2", "I"))==0) goto end;
	fpd.Cbus2 = (UCHAR)(*env)->GetIntField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "cbus3", "I"))==0) goto end;
	fpd.Cbus3 = (UCHAR)(*env)->GetIntField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "cbus4", "I"))==0) goto end;
	fpd.Cbus4 = (UCHAR)(*env)->GetIntField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "rIsD2XX", "Z"))==0) goto end;
	fpd.RIsD2XX = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "pullDownEnable7", "Z"))==0) goto end;
	fpd.PullDownEnable7 = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "serNumEnable7", "Z"))==0) goto end;
	fpd.SerNumEnable7 = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "alSlowSlew", "Z"))==0) goto end;
	fpd.ALSlowSlew = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "alSchmittInput", "Z"))==0) goto end;
	fpd.ALSchmittInput = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "alDriveCurrent", "I"))==0) goto end;
	fpd.ALDriveCurrent = (UCHAR)(*env)->GetIntField(env, pdo, fid);

	if ((fid = (*env)->GetFieldID(env, pdcls, "ahSlowSlew", "Z"))==0) goto end;
	fpd.AHSlowSlew = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "ahSchmittInput", "Z"))==0) goto end;
	fpd.AHSchmittInput = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "ahDriveCurrent", "I"))==0) goto end;
	fpd.AHDriveCurrent = (UCHAR)(*env)->GetIntField(env, pdo, fid);

	if ((fid = (*env)->GetFieldID(env, pdcls, "blSlowSlew", "Z"))==0) goto end;
	fpd.BLSlowSlew = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "blSchmittInput", "Z"))==0) goto end;
	fpd.BLSchmittInput = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "blDriveCurrent", "I"))==0) goto end;
	fpd.BLDriveCurrent = (UCHAR)(*env)->GetIntField(env, pdo, fid);

	if ((fid = (*env)->GetFieldID(env, pdcls, "bhSlowSlew", "Z"))==0) goto end;
	fpd.BHSlowSlew = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "bhSchmittInput", "Z"))==0) goto end;
	fpd.BHSchmittInput = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "bhDriveCurrent", "I"))==0) goto end;
	fpd.BHDriveCurrent = (UCHAR)(*env)->GetIntField(env, pdo, fid);

	if ((fid = (*env)->GetFieldID(env, pdcls, "ifAIsFifo7", "Z"))==0) goto end;
	fpd.IFAIsFifo7 = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "ifAIsFifoTar7", "Z"))==0) goto end;
	fpd.IFAIsFifoTar7 = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "ifAIsFastSer7", "Z"))==0) goto end;
	fpd.IFAIsFastSer7 = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "aIsVCP7", "Z"))==0) goto end;
	fpd.AIsVCP7 = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "ifBIsFifo7", "Z"))==0) goto end;
	fpd.IFBIsFifo7 = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "ifBIsFifoTar7", "Z"))==0) goto end;
	fpd.IFBIsFifoTar7 = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "ifBIsFastSer7", "Z"))==0) goto end;
	fpd.IFBIsFastSer7 = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "bIsVCP7", "Z"))==0) goto end;
	fpd.BIsVCP7 = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "powerSaveEnable", "Z"))==0) goto end;
	fpd.PowerSaveEnable = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "pullDownEnable8", "Z"))==0) goto end;
	fpd.PullDownEnable8 = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "serNumEnable8", "Z"))==0) goto end;
	fpd.SerNumEnable8 = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "aSlowSlew", "Z"))==0) goto end;
	fpd.ASlowSlew = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "aSchmittInput", "Z"))==0) goto end;
	fpd.ASchmittInput = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "aDriveCurrent", "I"))==0) goto end;
	fpd.ADriveCurrent = (UCHAR)(*env)->GetIntField(env, pdo, fid);

	if ((fid = (*env)->GetFieldID(env, pdcls, "bSlowSlew", "Z"))==0) goto end;
	fpd.BSlowSlew = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "bSchmittInput", "Z"))==0) goto end;
	fpd.BSchmittInput = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "bDriveCurrent", "I"))==0) goto end;
	fpd.BDriveCurrent = (UCHAR)(*env)->GetIntField(env, pdo, fid);

	if ((fid = (*env)->GetFieldID(env, pdcls, "cSlowSlew", "Z"))==0) goto end;
	fpd.CSlowSlew = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "cSchmittInput", "Z"))==0) goto end;
	fpd.CSchmittInput = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "cDriveCurrent", "I"))==0) goto end;
	fpd.CDriveCurrent = (UCHAR)(*env)->GetIntField(env, pdo, fid);

	if ((fid = (*env)->GetFieldID(env, pdcls, "dSlowSlew", "Z"))==0) goto end;
	fpd.DSlowSlew = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "dSchmittInput", "Z"))==0) goto end;
	fpd.DSchmittInput = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "dDriveCurrent", "I"))==0) goto end;
	fpd.DDriveCurrent = (UCHAR)(*env)->GetIntField(env, pdo, fid);

	if ((fid = (*env)->GetFieldID(env, pdcls, "aRIIsTXDEN", "Z"))==0) goto end;
	fpd.ARIIsTXDEN = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "bRIIsTXDEN", "Z"))==0) goto end;
	fpd.BRIIsTXDEN = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "cRIIsTXDEN", "Z"))==0) goto end;
	fpd.CRIIsTXDEN = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "dRIIsTXDEN", "Z"))==0) goto end;
	fpd.DRIIsTXDEN = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "aIsVCP8", "Z"))==0) goto end;
	fpd.AIsVCP8 = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "bIsVCP8", "Z"))==0) goto end;
	fpd.BIsVCP8 = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "cIsVCP8", "Z"))==0) goto end;
	fpd.CIsVCP8 = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "dIsVCP8", "Z"))==0) goto end;
	fpd.DIsVCP8 = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "pullDownEnableH", "Z"))==0) goto end;
	fpd.PullDownEnableH = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "serNumEnableH", "Z"))==0) goto end;
	fpd.SerNumEnableH = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "acSlowSlewH", "Z"))==0) goto end;
	fpd.ACSlowSlewH = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "acSchmittInputH", "Z"))==0) goto end;
	fpd.ACSchmittInputH = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "acDriveCurrentH", "I"))==0) goto end;
	fpd.ACDriveCurrentH = (UCHAR)(*env)->GetIntField(env, pdo, fid);

	if ((fid = (*env)->GetFieldID(env, pdcls, "adSlowSlewH", "Z"))==0) goto end;
	fpd.ADSlowSlewH = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "adSchmittInputH", "Z"))==0) goto end;
	fpd.ADSchmittInputH = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "adDriveCurrentH", "I"))==0) goto end;
	fpd.ADDriveCurrentH = (UCHAR)(*env)->GetIntField(env, pdo, fid);

	if ((fid = (*env)->GetFieldID(env, pdcls, "cbus0H", "I"))==0) goto end;
	fpd.Cbus0H = (UCHAR)(*env)->GetIntField(env, pdo, fid);

	if ((fid = (*env)->GetFieldID(env, pdcls, "cbus1H", "I"))==0) goto end;
	fpd.Cbus1H = (UCHAR)(*env)->GetIntField(env, pdo, fid);

	if ((fid = (*env)->GetFieldID(env, pdcls, "cbus2H", "I"))==0) goto end;
	fpd.Cbus2H = (UCHAR)(*env)->GetIntField(env, pdo, fid);

	if ((fid = (*env)->GetFieldID(env, pdcls, "cbus3H", "I"))==0) goto end;
	fpd.Cbus3H = (UCHAR)(*env)->GetIntField(env, pdo, fid);

	if ((fid = (*env)->GetFieldID(env, pdcls, "cbus4H", "I"))==0) goto end;
	fpd.Cbus4H = (UCHAR)(*env)->GetIntField(env, pdo, fid);

	if ((fid = (*env)->GetFieldID(env, pdcls, "cbus5H", "I"))==0) goto end;
	fpd.Cbus5H = (UCHAR)(*env)->GetIntField(env, pdo, fid);

	if ((fid = (*env)->GetFieldID(env, pdcls, "cbus6H", "I"))==0) goto end;
	fpd.Cbus6H = (UCHAR)(*env)->GetIntField(env, pdo, fid);

	if ((fid = (*env)->GetFieldID(env, pdcls, "cbus7H", "I"))==0) goto end;
	fpd.Cbus7H = (UCHAR)(*env)->GetIntField(env, pdo, fid);

	if ((fid = (*env)->GetFieldID(env, pdcls, "cbus8H", "I"))==0) goto end;
	fpd.Cbus8H = (UCHAR)(*env)->GetIntField(env, pdo, fid);

	if ((fid = (*env)->GetFieldID(env, pdcls, "cbus9H", "I"))==0) goto end;
	fpd.Cbus9H = (UCHAR)(*env)->GetIntField(env, pdo, fid);

	if ((fid = (*env)->GetFieldID(env, pdcls, "isFifoH", "Z"))==0) goto end;
	fpd.IsFifoH = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "isFifoTarH", "Z"))==0) goto end;
	fpd.IsFifoTarH = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "isFastSerH", "Z"))==0) goto end;
	fpd.IsFastSerH = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "isFt1248H", "Z"))==0) goto end;
	fpd.IsFT1248H = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "ft1248CpolH", "Z"))==0) goto end;
	fpd.FT1248CpolH = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "ft1248LsbH", "Z"))==0) goto end;
	fpd.FT1248LsbH = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "ft1248FlowControlH", "Z"))==0) goto end;
	fpd.FT1248FlowControlH = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "isVCPH", "Z"))==0) goto end;
	fpd.IsVCPH = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;

	if ((fid = (*env)->GetFieldID(env, pdcls, "powerSaveEnableH", "Z"))==0) goto end;
	fpd.PowerSaveEnableH = (UCHAR)(*env)->GetBooleanField(env, pdo, fid) ? 1 : 0;


	if (!FT_SUCCESS(st = FT_EE_Program((FT_HANDLE)hnd, &fpd)))
		io_exception_status(env, st);


end:
	if (fpd.Manufacturer)
		(*env)->ReleaseStringUTFChars(env, mstr, fpd.Manufacturer);
	if (fpd.ManufacturerId)
		(*env)->ReleaseStringUTFChars(env, istr, fpd.ManufacturerId);
	if (fpd.Description)
		(*env)->ReleaseStringUTFChars(env, dstr, fpd.Description);
	if (fpd.SerialNumber)
		(*env)->ReleaseStringUTFChars(env, sstr, fpd.SerialNumber);

	(*env)->DeleteLocalRef(env, pdcls);
}

JNIEXPORT jobject JNICALL
Java_jd2xx_JD2XX_eeRead(JNIEnv *env, jobject obj) {
	FT_STATUS st;
	FT_PROGRAM_DATA fpd;
	jobject result;
	jlong hnd = get_handle(env, obj);

	jclass pdcls;
	jfieldID fid;
	jstring str;

	char manufacturer[DESCRIPTION_SIZE];
	char manufacturerId[DESCRIPTION_SIZE];
	char description[DESCRIPTION_SIZE];
	char serialNumber[DESCRIPTION_SIZE];

	fpd.Signature1 = 0x00000000;
	fpd.Signature2 = 0xffffffff;
	fpd.Version = 2;

	fpd.Manufacturer = manufacturer;
	fpd.ManufacturerId = manufacturerId;
	fpd.Description = description;
	fpd.SerialNumber = serialNumber;

	if (!FT_SUCCESS(st = FT_EE_Read((FT_HANDLE)hnd, &fpd))) {
		io_exception_status(env, st);
		return NULL;
	}

	pdcls = (*env)->FindClass(env, "Ljd2xx/JD2XX$ProgramData;");
	if (pdcls == 0) return NULL;

	result = (*env)->AllocObject(env, pdcls);
	if (result == 0) goto panic;

	if ((fid = (*env)->GetFieldID(env, pdcls, "signature1", "I"))==0) goto panic;
	(*env)->SetIntField(env, result, fid, fpd.Signature1);

	if ((fid = (*env)->GetFieldID(env, pdcls, "signature2", "I"))==0) goto panic;
	(*env)->SetIntField(env, result, fid, fpd.Signature2);

	if ((fid = (*env)->GetFieldID(env, pdcls, "version", "I"))==0) goto panic;
	(*env)->SetIntField(env, result, fid, fpd.Version);

	if ((fid = (*env)->GetFieldID(env, pdcls, "vendorID", "I"))==0) goto panic;
	(*env)->SetIntField(env, result, fid, fpd.VendorId);

	if ((fid = (*env)->GetFieldID(env, pdcls, "productID", "I"))==0) goto panic;
	(*env)->SetIntField(env, result, fid, fpd.ProductId);

	if ((fid = (*env)->GetFieldID(env, pdcls, "manufacturer", "Ljava/lang/String;"))==0) goto panic;
	if ((str = (*env)->NewStringUTF(env, fpd.Manufacturer))==0) goto panic;
	(*env)->SetObjectField(env, result, fid, str);

	if ((fid = (*env)->GetFieldID(env, pdcls, "manufacturerID", "Ljava/lang/String;"))==0) goto panic;
	if ((str = (*env)->NewStringUTF(env, fpd.ManufacturerId))==0) goto panic;
	(*env)->SetObjectField(env, result, fid, str);

	if ((fid = (*env)->GetFieldID(env, pdcls, "description", "Ljava/lang/String;"))==0) goto panic;
	if ((str = (*env)->NewStringUTF(env, fpd.Description))==0) goto panic;
	(*env)->SetObjectField(env, result, fid, str);

	if ((fid = (*env)->GetFieldID(env, pdcls, "serialNumber", "Ljava/lang/String;"))==0) goto panic;
	if ((str = (*env)->NewStringUTF(env, fpd.SerialNumber))==0) goto panic;
	(*env)->SetObjectField(env, result, fid, str);

	if ((fid = (*env)->GetFieldID(env, pdcls, "maxPower", "I"))==0) goto panic;
	(*env)->SetIntField(env, result, fid, fpd.MaxPower);

	if ((fid = (*env)->GetFieldID(env, pdcls, "pnp", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.PnP ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "selfPowered", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.SelfPowered ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "remoteWakeup", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.RemoteWakeup ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "rev4", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.Rev4 ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "isoIn", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.IsoIn ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "isoOut", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.IsoOut ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "pullDownEnable", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.PullDownEnable ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "serNumEnable", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.SerNumEnable ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "usbVersionEnable", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.USBVersionEnable ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "usbVersion", "I"))==0) goto panic;
	(*env)->SetIntField(env, result, fid, fpd.USBVersion);

	if ((fid = (*env)->GetFieldID(env, pdcls, "rev5", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.Rev5 ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "isoInA", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.IsoInA ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "isoInB", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.IsoInB ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "isoOutA", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.IsoOutA ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "isoOutB", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.IsoOutB ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "pullDownEnable5", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.PullDownEnable5 ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "serNumEnable5", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.SerNumEnable5 ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "usbVersionEnable5", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.USBVersionEnable5 ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "usbVersion5", "I"))==0) goto panic;
	(*env)->SetIntField(env, result, fid, fpd.USBVersion5);

	if ((fid = (*env)->GetFieldID(env, pdcls, "aIsHighCurrent", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.AIsHighCurrent ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "bIsHighCurrent", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.BIsHighCurrent ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "ifAIsFifo", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.IFAIsFifo ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "ifAIsFifoTar", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.IFAIsFifoTar ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "ifAIsFastSer", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.IFAIsFastSer ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "aIsVCP", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.AIsVCP ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "ifBIsFifo", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.IFBIsFifo ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "ifBIsFifoTar", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.IFBIsFifoTar ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "ifBIsFastSer", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.IFBIsFastSer ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "bIsVCP", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.BIsVCP ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "endpointSize", "I"))==0) goto panic;
	(*env)->SetIntField(env, result, fid, (int)fpd.EndpointSize);

	if ((fid = (*env)->GetFieldID(env, pdcls, "useExtOsc", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.UseExtOsc ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "highDriveIOs", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.HighDriveIOs ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "pullDownEnableR", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.PullDownEnableR ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "serNumEnableR", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.SerNumEnableR ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "invertTXD", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.InvertTXD ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "invertRXD", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.InvertRXD ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "invertRTS", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.InvertRTS ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "invertCTS", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.InvertCTS ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "invertDTR", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.InvertDTR ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "invertDSR", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.InvertDSR ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "invertDCD", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.InvertDCD ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "invertRI", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.InvertRI ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "cbus0", "I"))==0) goto panic;
	(*env)->SetIntField(env, result, fid, fpd.Cbus0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "cbus1", "I"))==0) goto panic;
	(*env)->SetIntField(env, result, fid, fpd.Cbus1);

	if ((fid = (*env)->GetFieldID(env, pdcls, "cbus2", "I"))==0) goto panic;
	(*env)->SetIntField(env, result, fid, fpd.Cbus2);

	if ((fid = (*env)->GetFieldID(env, pdcls, "cbus3", "I"))==0) goto panic;
	(*env)->SetIntField(env, result, fid, fpd.Cbus3);

	if ((fid = (*env)->GetFieldID(env, pdcls, "cbus4", "I"))==0) goto panic;
	(*env)->SetIntField(env, result, fid, fpd.Cbus4);

	if ((fid = (*env)->GetFieldID(env, pdcls, "rIsD2XX", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.RIsD2XX ? 1 : 0);


	if ((fid = (*env)->GetFieldID(env, pdcls, "pullDownEnable7", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.PullDownEnable7 ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "serNumEnable7", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.SerNumEnable7 ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "alSlowSlew", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.ALSlowSlew ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "alSchmittInput", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.ALSchmittInput ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "alDriveCurrent", "I"))==0) goto panic;
	(*env)->SetIntField(env, result, fid, fpd.ALDriveCurrent);

	if ((fid = (*env)->GetFieldID(env, pdcls, "ahSlowSlew", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.AHSlowSlew ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "ahSchmittInput", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.AHSchmittInput ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "ahDriveCurrent", "I"))==0) goto panic;
	(*env)->SetIntField(env, result, fid, fpd.AHDriveCurrent);

	if ((fid = (*env)->GetFieldID(env, pdcls, "blSlowSlew", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.BLSlowSlew ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "blSchmittInput", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.BLSchmittInput ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "blDriveCurrent", "I"))==0) goto panic;
	(*env)->SetIntField(env, result, fid, fpd.BLDriveCurrent);

	if ((fid = (*env)->GetFieldID(env, pdcls, "bhSlowSlew", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.BHSlowSlew ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "bhSchmittInput", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.BHSchmittInput ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "bhDriveCurrent", "I"))==0) goto panic;
	(*env)->SetIntField(env, result, fid, fpd.BHDriveCurrent);

	if ((fid = (*env)->GetFieldID(env, pdcls, "ifAIsFifo7", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.IFAIsFifo7 ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "ifAIsFifoTar7", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.IFAIsFifoTar7 ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "ifAIsFastSer7", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.IFAIsFastSer7 ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "aIsVCP7", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.AIsVCP7 ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "ifBIsFifo7", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.IFBIsFifo7 ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "ifBIsFifoTar7", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.IFBIsFifoTar7 ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "ifBIsFastSer7", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.IFBIsFastSer7 ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "bIsVCP7", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.BIsVCP7 ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "powerSaveEnable", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.PowerSaveEnable ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "pullDownEnable8", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.PullDownEnable8 ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "serNumEnable8", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.SerNumEnable8 ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "aSlowSlew", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.ASlowSlew ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "aSchmittInput", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.ASchmittInput ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "aDriveCurrent", "I"))==0) goto panic;
	(*env)->SetIntField(env, result, fid, fpd.ADriveCurrent);

	if ((fid = (*env)->GetFieldID(env, pdcls, "bSlowSlew", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.BSlowSlew ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "bSchmittInput", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.BSchmittInput ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "bDriveCurrent", "I"))==0) goto panic;
	(*env)->SetIntField(env, result, fid, fpd.BDriveCurrent);

	if ((fid = (*env)->GetFieldID(env, pdcls, "cSlowSlew", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.CSlowSlew ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "cSchmittInput", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.CSchmittInput ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "cDriveCurrent", "I"))==0) goto panic;
	(*env)->SetIntField(env, result, fid, fpd.CDriveCurrent);

	if ((fid = (*env)->GetFieldID(env, pdcls, "dSlowSlew", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.DSlowSlew ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "dSchmittInput", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.DSchmittInput ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "dDriveCurrent", "I"))==0) goto panic;
	(*env)->SetIntField(env, result, fid, fpd.DDriveCurrent);

	if ((fid = (*env)->GetFieldID(env, pdcls, "aRIIsTXDEN", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.ARIIsTXDEN ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "bRIIsTXDEN", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.BRIIsTXDEN ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "cRIIsTXDEN", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.CRIIsTXDEN ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "dRIIsTXDEN", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.DRIIsTXDEN ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "aIsVCP8", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.AIsVCP8 ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "bIsVCP8", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.BIsVCP8 ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "cIsVCP8", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.CIsVCP8 ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "dIsVCP8", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.DIsVCP8 ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "pullDownEnableH", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.PullDownEnableH ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "serNumEnableH", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.SerNumEnableH ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "acSlowSlewH", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.ACSlowSlewH ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "acSchmittInputH", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.ACSchmittInputH ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "acDriveCurrentH", "I"))==0) goto panic;
	(*env)->SetIntField(env, result, fid, fpd.ACDriveCurrentH);

	if ((fid = (*env)->GetFieldID(env, pdcls, "adSlowSlewH", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.ADSlowSlewH ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "adSchmittInputH", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.ADSchmittInputH ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "adDriveCurrentH", "I"))==0) goto panic;
	(*env)->SetIntField(env, result, fid, fpd.ADDriveCurrentH);

	if ((fid = (*env)->GetFieldID(env, pdcls, "cbus0H", "I"))==0) goto panic;
	(*env)->SetIntField(env, result, fid, fpd.Cbus0H);

	if ((fid = (*env)->GetFieldID(env, pdcls, "cbus1H", "I"))==0) goto panic;
	(*env)->SetIntField(env, result, fid, fpd.Cbus1H);

	if ((fid = (*env)->GetFieldID(env, pdcls, "cbus2H", "I"))==0) goto panic;
	(*env)->SetIntField(env, result, fid, fpd.Cbus2H);

	if ((fid = (*env)->GetFieldID(env, pdcls, "cbus3H", "I"))==0) goto panic;
	(*env)->SetIntField(env, result, fid, fpd.Cbus3H);

	if ((fid = (*env)->GetFieldID(env, pdcls, "cbus4H", "I"))==0) goto panic;
	(*env)->SetIntField(env, result, fid, fpd.Cbus4H);

	if ((fid = (*env)->GetFieldID(env, pdcls, "cbus5H", "I"))==0) goto panic;
	(*env)->SetIntField(env, result, fid, fpd.Cbus5H);

	if ((fid = (*env)->GetFieldID(env, pdcls, "cbus6H", "I"))==0) goto panic;
	(*env)->SetIntField(env, result, fid, fpd.Cbus6H);

	if ((fid = (*env)->GetFieldID(env, pdcls, "cbus7H", "I"))==0) goto panic;
	(*env)->SetIntField(env, result, fid, fpd.Cbus7H);

	if ((fid = (*env)->GetFieldID(env, pdcls, "cbus8H", "I"))==0) goto panic;
	(*env)->SetIntField(env, result, fid, fpd.Cbus8H);

	if ((fid = (*env)->GetFieldID(env, pdcls, "cbus9H", "I"))==0) goto panic;
	(*env)->SetIntField(env, result, fid, fpd.Cbus9H);

	if ((fid = (*env)->GetFieldID(env, pdcls, "isFifoH", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.IsFifoH ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "isFifoTarH", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.IsFifoTarH ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "isFastSerH", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.IsFastSerH ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "isFt1248H", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.IsFT1248H ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "ft1248CpolH", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.FT1248CpolH ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "ft1248LsbH", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.FT1248LsbH ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "ft1248FlowControlH", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.FT1248FlowControlH ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "isVCPH", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.IsVCPH ? 1 : 0);

	if ((fid = (*env)->GetFieldID(env, pdcls, "powerSaveEnableH", "Z"))==0) goto panic;
	(*env)->SetBooleanField(env, result, fid, fpd.PowerSaveEnableH ? 1 : 0);


	(*env)->DeleteLocalRef(env, pdcls);
	return result;

panic:
	(*env)->DeleteLocalRef(env, result);
	(*env)->DeleteLocalRef(env, pdcls);
	return NULL;
}

JNIEXPORT jint JNICALL
Java_jd2xx_JD2XX_eeUASize(JNIEnv *env, jobject obj) {
	FT_STATUS st;
	jlong hnd = get_handle(env, obj);
	volatile DWORD siz;

	if (!FT_SUCCESS(st = FT_EE_UASize((FT_HANDLE)hnd, &siz)))
		io_exception_status(env, st);

	return (jint)siz;
}

JNIEXPORT void JNICALL
Java_jd2xx_JD2XX_eeUAWrite(JNIEnv *env, jobject obj, jbyteArray arr) {
	FT_STATUS st;
	jlong hnd = get_handle(env, obj);
	int len = (*env)->GetArrayLength(env, arr);
	jbyte *buf = (*env)->GetByteArrayElements(env, arr, 0);

	if (!FT_SUCCESS(st = FT_EE_UAWrite((FT_HANDLE)hnd, (PUCHAR)buf, (DWORD)len)))
		io_exception_status(env, st);

	(*env)->ReleaseByteArrayElements(env, arr, buf, 0);
}

JNIEXPORT jbyteArray JNICALL
Java_jd2xx_JD2XX_eeUARead(JNIEnv *env, jobject obj, jint len) {
	FT_STATUS st;
	jbyteArray result;
	volatile DWORD ret; // bytes returned
	jbyte buf[len];
	jlong hnd = get_handle(env, obj);

	if (!FT_SUCCESS(st =
		FT_EE_UARead((FT_HANDLE)hnd, (PUCHAR)buf, (DWORD)len, &ret)
	)) {
		io_exception_status(env, st);
		return NULL;
	}

	result = (*env)->NewByteArray(env, ret);
	if (result != 0) (*env)->SetByteArrayRegion(env, result, 0, ret, buf);

	return result;
}

/*
JNIEXPORT void JNICALL
Java_jd2xx_JD2XX_addEventListener(JNIEnv *env, jobject obj, jobject evo) {
	FT_STATUS st;
	jlong hnd = get_handle(env, obj);

}

JNIEXPORT void JNICALL
Java_jd2xx_JD2XX_removeEventListener(JNIEnv *env, jobject obj) {
	FT_STATUS st;
	jlong hnd = get_handle(env, obj);

}
*/

#ifdef WIN32
JNIEXPORT void JNICALL
Java_jd2xx_JD2XX_registerEvent(
	JNIEnv *env, jobject obj, jint msk
) {
	FT_STATUS st;
	jlong hnd = get_handle(env, obj);
	HANDLE evh = (HANDLE)get_event(env, obj);

	if (msk != 0) { // new events
		if (evh == INVALID_HANDLE_VALUE) {
			evh = CreateEvent(
				NULL,
				0, 0, // auto-reset, non-signaled
				""
			);
			if (evh == INVALID_HANDLE_VALUE) return io_exception(env, "invalid event handle");
		}

		if (!FT_SUCCESS(
			st = FT_SetEventNotification((FT_HANDLE)hnd, (DWORD)msk, (HANDLE)evh)
		)) {
			FT_SetEventNotification((FT_HANDLE)hnd, (DWORD)0, (HANDLE)INVALID_HANDLE_VALUE);
			CloseHandle(evh);
			evh = INVALID_HANDLE_VALUE;
			io_exception_status(env, st);
		}

		set_event(env, obj, (jint)evh);

#ifdef DEBUG
		fprintf(stderr, "JD2XX.registerEvent: %x\n", evh);
#endif
	}
	else if (evh != INVALID_HANDLE_VALUE) { // no more events
		st = FT_SetEventNotification(
			(FT_HANDLE)hnd, (DWORD)0, (HANDLE)INVALID_HANDLE_VALUE
		); //!
		CloseHandle(evh);
		set_event(env, obj, (jint)INVALID_HANDLE_VALUE);
		if (!FT_SUCCESS(st)) io_exception_status(env, st);
#ifdef DEBUG
		fprintf(stderr, "JD2XX.registerEvent: %x\n", evh);
#endif
	}
}

JNIEXPORT void JNICALL
Java_jd2xx_JD2XX_signalEvent(JNIEnv *env, jobject obj) {
	HANDLE evh = (HANDLE)get_event(env, obj);

#ifdef DEBUG
	fprintf(stderr, "JD2XX.signalEvent\n");
#endif
	SetEvent(evh);
}

JNIEXPORT jint JNICALL
Java_jd2xx_JD2XX_waitEvent(JNIEnv *env, jobject obj) {
	FT_STATUS st;
	jlong hnd = get_handle(env, obj), kill = get_kill(env, obj), ret = 0;
	volatile DWORD msk;
	HANDLE evh = (HANDLE)get_event(env, obj);

#ifdef DEBUG
	fprintf(stderr, "JD2XX.waitEvent: (waiting)\n");
#endif

	WaitForSingleObject(evh, INFINITE);

	if (!kill) {
		if (!FT_SUCCESS(st = FT_GetEventStatus((FT_HANDLE)hnd, &msk)))
			io_exception_status(env, st);

		ret = (jint)msk;
	}

#ifdef DEBUG
	fprintf(stderr, "JD2XX.waitEvent: %x\n", ret);
#endif
	return ret;
}

#else

#include "stdlib.h"

JNIEXPORT void JNICALL
Java_jd2xx_JD2XX_registerEvent(
		JNIEnv *env, jobject obj, jint msk
) {
	FT_STATUS st;
	jlong hnd = get_handle(env, obj);

	EVENT_HANDLE *evh = (EVENT_HANDLE *) get_event(env, obj);

	if (msk != 0) { // new events
		if (evh == INVALID_HANDLE_VALUE) {
			evh = (EVENT_HANDLE*) malloc(sizeof(EVENT_HANDLE));
			pthread_mutex_init(&(evh->eMutex), NULL);
			if (pthread_cond_init(&(evh->eCondVar), NULL) != 0)
				return io_exception(env, "invalid event handle");
		}

		if (!FT_SUCCESS(st = FT_SetEventNotification((FT_HANDLE)hnd, (DWORD)msk, (HANDLE)evh))) {
			FT_SetEventNotification((FT_HANDLE)hnd, (DWORD)0, (HANDLE)INVALID_HANDLE_VALUE);
			pthread_mutex_destroy(&(evh->eMutex));
			free(evh);
			evh = (EVENT_HANDLE *) INVALID_HANDLE_VALUE;
			io_exception_status(env, st);
		}

		set_event(env, obj, (jint)evh);

#ifdef DEBUG
		fprintf(stderr, "JD2XX.registerEvent: %x\n", evh);
#endif
	} else if (evh != INVALID_HANDLE_VALUE) { // no more events
		st = FT_SetEventNotification((FT_HANDLE)hnd, (DWORD)0, (HANDLE)INVALID_HANDLE_VALUE); //!
		pthread_mutex_destroy(&(evh->eMutex));
		set_event(env, obj, (jint)INVALID_HANDLE_VALUE);
		free(evh);
		if (!FT_SUCCESS(st))
			io_exception_status(env, st);
#ifdef DEBUG
		fprintf(stderr, "JD2XX.registerEvent: %x\n", evh);
#endif
	}
}

JNIEXPORT void JNICALL
Java_jd2xx_JD2XX_signalEvent(JNIEnv *env, jobject obj) {

	EVENT_HANDLE* evh = (EVENT_HANDLE*) get_event(env, obj);

#ifdef DEBUG
	fprintf(stderr, "JD2XX.signalEvent\n");
#endif
	pthread_cond_signal(&(evh->eCondVar));
}

JNIEXPORT jint JNICALL
Java_jd2xx_JD2XX_waitEvent(JNIEnv *env, jobject obj) {
	FT_STATUS st;
	jlong hnd = get_handle(env, obj), kill = get_kill(env, obj), ret = 0;
	volatile DWORD msk;

	EVENT_HANDLE* evh = (EVENT_HANDLE*) get_event(env, obj);

#ifdef DEBUG
	fprintf(stderr, "JD2XX.waitEvent: (waiting)\n");
#endif

	pthread_mutex_lock(&(evh->eMutex));
	pthread_cond_wait(&(evh->eCondVar), &(evh->eMutex));
	pthread_mutex_unlock(&(evh->eMutex));

	if (!kill) {
		if (!FT_SUCCESS(st = FT_GetEventStatus((FT_HANDLE)hnd, &msk)))
			io_exception_status(env, st);

		ret = (jint)msk;
	}

#ifdef DEBUG
	fprintf(stderr, "JD2XX.waitEvent: %x\n", ret);
#endif
	return ret;
}
#endif // WIN32

#include "telldus-core.h"
#include "Tellstick.h"

JNIEXPORT void JNICALL
Java_Tellstick_tdOpen(JNIEnv * env, jobject obj){
    return (void) tdInit();
}

JNIEXPORT void JNICALL 
Java_Tellstick_tdClose(JNIEnv * env, jobject obj){
    return (void) tdClose();
}

JNIEXPORT jboolean JNICALL 
Java_Tellstick_tdTurnOn(JNIEnv * env, jobject obj, jint intDeviceId){
    return (jboolean) tdTurnOn((int) intDeviceId);
}

JNIEXPORT jboolean JNICALL 
Java_Tellstick_tdTurnOff(JNIEnv * env, jobject obj, jint intDeviceId){
    return (jboolean) tdTurnOff((int) intDeviceId);
}

JNIEXPORT jint JNICALL 
Java_Tellstick_tdLearn(JNIEnv * env, jobject obj, jint intDeviceId){
    return (jint) tdLearn((int) intDeviceId);
}

JNIEXPORT jboolean JNICALL 
Java_Tellstick_tdBell(JNIEnv * env, jobject obj, jint intDeviceId){
    return (jboolean) tdBell((int) intDeviceId);
}

JNIEXPORT jboolean JNICALL 
Java_Tellstick_tdDim(JNIEnv * env, jobject obj, jint intDeviceId, jint level){
    return (jboolean) tdDim((int)intDeviceId,(unsigned char) level);
}

JNIEXPORT jint JNICALL 
Java_Tellstick_tdMethods(JNIEnv * env, jobject obj, jint id, jint methodsSupported){
    return (jint) tdMethods((int) id, (int) methodsSupported);
}

JNIEXPORT jint JNICALL 
Java_Tellstick_tdGetNumberOfDevices(JNIEnv * env, jobject obj){
    return (jint) tdGetNumberOfDevices();
}

JNIEXPORT jint JNICALL 
Java_Tellstick_tdGetDeviceId(JNIEnv * env, jobject obj, jint intDeviceIndex){
    return (jint) tdGetDeviceId((int) intDeviceIndex);
}

JNIEXPORT jstring JNICALL 
Java_Tellstick_tdGetName(JNIEnv * env, jobject obj, jint intDeviceId){
    return (jstring) tdGetName((int) intDeviceId);
}

JNIEXPORT jboolean JNICALL 
Java_Tellstick_tdSetName(JNIEnv * env, jobject obj, jint intDeviceId, jstring chNewName){
    return (jboolean) tdSetName((int) intDeviceId, (const char*) chNewName);
}

JNIEXPORT jint JNICALL 
Java_Tellstick_tdAddDevice(JNIEnv * env, jobject obj){
   return (jint) tdAddDevice();
}

JNIEXPORT jboolean JNICALL 
Java_Tellstick_tdRemoveDevice(JNIEnv * env, jobject obj, jint intDeviceId){
   return (jboolean) tdRemoveDevice((int) intDeviceId);
}

#ifndef _JAVA_H_
#define _JAVA_H_

#include "jni.h"

#ifdef __cplusplus
extern "C" {
#endif

// GameRenderer
JNIEXPORT void JNICALL Java_com_gameloft_android_GAND_GloftF5HP_ML_GameRenderer_nativeResize(JNIEnv*, jobject, jint, jint);
JNIEXPORT void JNICALL Java_com_gameloft_android_GAND_GloftF5HP_ML_GameRenderer_nativeInit(JNIEnv*, jobject, jobject, jstring, jstring, jint, jint);
JNIEXPORT void JNICALL Java_com_gameloft_android_GAND_GloftF5HP_ML_GameRenderer_nativeDestroy(JNIEnv*, jobject);
JNIEXPORT void JNICALL Java_com_gameloft_android_GAND_GloftF5HP_ML_GameRenderer_nativeRender(JNIEnv*, jobject);
JNIEXPORT void JNICALL Java_com_gameloft_android_GAND_GloftF5HP_ML_GameRenderer_onKeyboardFinish(JNIEnv*, jobject, jboolean);

// GameGLSurfaceView
JNIEXPORT void JNICALL Java_com_gameloft_android_GAND_GloftF5HP_ML_GameGLSurfaceView_nativeResume(JNIEnv*, jobject);
JNIEXPORT void JNICALL Java_com_gameloft_android_GAND_GloftF5HP_ML_GameGLSurfaceView_nativePause(JNIEnv*, jobject);
JNIEXPORT void JNICALL Java_com_gameloft_android_GAND_GloftF5HP_ML_GameGLSurfaceView_nativePauseAllSounds(JNIEnv*, jobject);

// GLMediaPlayer
JNIEXPORT void JNICALL Java_com_gameloft_android_GAND_GloftF5HP_ML_GLMediaPlayer_nativeSetStopOnMusic(JNIEnv*, jobject, jboolean);
JNIEXPORT void JNICALL Java_com_gameloft_android_GAND_GloftF5HP_ML_GLMediaPlayer_nativeInit(JNIEnv*, jobject, jobject, jstring);
JNIEXPORT jint JNICALL Java_com_gameloft_android_GAND_GloftF5HP_ML_GLMediaPlayer_nativeGetTotalSoundsOfSameInstance(JNIEnv*, jobject, jint);
JNIEXPORT jint JNICALL Java_com_gameloft_android_GAND_GloftF5HP_ML_GLMediaPlayer_nativeGetTotalSounds(JNIEnv*, jobject);

// GLGame
JNIEXPORT void JNICALL Java_com_gameloft_android_GAND_GloftF5HP_ML_GLGame_nativeMPOnlineLoginSet(JNIEnv*, jobject, jboolean);
JNIEXPORT void JNICALL Java_com_gameloft_android_GAND_GloftF5HP_ML_GLGame_nativePauseInGame(JNIEnv*, jobject);
JNIEXPORT void JNICALL Java_com_gameloft_android_GAND_GloftF5HP_ML_GLGame_nativeSetOnKeyUp(JNIEnv*, jobject, jobject);
JNIEXPORT void JNICALL Java_com_gameloft_android_GAND_GloftF5HP_ML_GLGame_nativeSetOnKeyDown(JNIEnv*, jobject, jobject);
JNIEXPORT void JNICALL Java_com_gameloft_android_GAND_GloftF5HP_ML_GLGame_nativeAccelerometer(JNIEnv*, jobject, jfloat, jfloat, jfloat);
JNIEXPORT void JNICALL Java_com_gameloft_android_GAND_GloftF5HP_ML_GLGame_nativeOrientation(JNIEnv*, jobject, jint);
JNIEXPORT void JNICALL Java_com_gameloft_android_GAND_GloftF5HP_ML_GLGame_nativeInit(JNIEnv*, jobject, jobject, jstring, jstring, jstring, jint, jint);
JNIEXPORT void JNICALL Java_com_gameloft_android_GAND_GloftF5HP_ML_GLGame_nativeTouchPressed(JNIEnv*, jobject, jint, jint, jfloat, jfloat);
JNIEXPORT void JNICALL Java_com_gameloft_android_GAND_GloftF5HP_ML_GLGame_nativeTouchMoved(JNIEnv*, jobject, jint, jint, jfloat, jfloat);
JNIEXPORT void JNICALL Java_com_gameloft_android_GAND_GloftF5HP_ML_GLGame_nativeTouchReleased(JNIEnv*, jobject, jint, jint, jfloat, jfloat);

// GLResLoader
JNIEXPORT void JNICALL Java_com_gameloft_android_GAND_GloftF5HP_ML_GLResLoader_nativeInit(JNIEnv*, jobject, jobject, jstring);

// GLUtils.Device
JNIEXPORT void JNICALL Java_com_gameloft_android_GAND_GloftF5HP_ML_GLUtils_Device_nativeInit(JNIEnv*, jobject);

#ifdef __cplusplus
}
#endif

#endif

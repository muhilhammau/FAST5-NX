#include "imports.h"

const struct JNINativeMethod game_jni_methods[] = {
    // GameRenderer
    { "nativeResize", "(II)V", (void*)Java_com_gameloft_android_GAND_GloftF5HP_ML_GameRenderer_nativeResize },
    { "nativeInit", "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;II)V", (void*)Java_com_gameloft_android_GAND_GloftF5HP_ML_GameRenderer_nativeInit },
    { "nativeDestroy", "()V", (void*)Java_com_gameloft_android_GAND_GloftF5HP_ML_GameRenderer_nativeDestroy },
    { "nativeRender", "()V", (void*)Java_com_gameloft_android_GAND_GloftF5HP_ML_GameRenderer_nativeRender },
    { "onKeyboardFinish", "(Z)V", (void*)Java_com_gameloft_android_GAND_GloftF5HP_ML_GameRenderer_onKeyboardFinish },

    // GameGLSurfaceView
    { "nativeResume", "()V", (void*)Java_com_gameloft_android_GAND_GloftF5HP_ML_GameGLSurfaceView_nativeResume },
    { "nativePause", "()V", (void*)Java_com_gameloft_android_GAND_GloftF5HP_ML_GameGLSurfaceView_nativePause },
    { "nativePauseAllSounds", "()V", (void*)Java_com_gameloft_android_GAND_GloftF5HP_ML_GameGLSurfaceView_nativePauseAllSounds },

    // GLMediaPlayer
    { "nativeSetStopOnMusic", "(Z)V", (void*)Java_com_gameloft_android_GAND_GloftF5HP_ML_GLMediaPlayer_nativeSetStopOnMusic },
    { "nativeInit", "(Landroid/content/Context;Ljava/lang/String;)V", (void*)Java_com_gameloft_android_GAND_GloftF5HP_ML_GLMediaPlayer_nativeInit },
    { "nativeGetTotalSoundsOfSameInstance", "(I)I", (void*)Java_com_gameloft_android_GAND_GloftF5HP_ML_GLMediaPlayer_nativeGetTotalSoundsOfSameInstance },
    { "nativeGetTotalSounds", "()I", (void*)Java_com_gameloft_android_GAND_GloftF5HP_ML_GLMediaPlayer_nativeGetTotalSounds },

    // GLGame
    { "nativeMPOnlineLoginSet", "(Z)V", (void*)Java_com_gameloft_android_GAND_GloftF5HP_ML_GLGame_nativeMPOnlineLoginSet },
    { "nativePauseInGame", "()V", (void*)Java_com_gameloft_android_GAND_GloftF5HP_ML_GLGame_nativePauseInGame },
    { "nativeSetOnKeyUp", "(Ljava/lang/Runnable;)V", (void*)Java_com_gameloft_android_GAND_GloftF5HP_ML_GLGame_nativeSetOnKeyUp },
    { "nativeSetOnKeyDown", "(Ljava/lang/Runnable;)V", (void*)Java_com_gameloft_android_GAND_GloftF5HP_ML_GLGame_nativeSetOnKeyDown },
    { "nativeAccelerometer", "(FFF)V", (void*)Java_com_gameloft_android_GAND_GloftF5HP_ML_GLGame_nativeAccelerometer },
    { "nativeOrientation", "(I)V", (void*)Java_com_gameloft_android_GAND_GloftF5HP_ML_GLGame_nativeOrientation },
    { "nativeInit", "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;II)V", (void*)Java_com_gameloft_android_GAND_GloftF5HP_ML_GLGame_nativeInit },
    { "nativeTouchPressed", "(IIFF)V", (void*)Java_com_gameloft_android_GAND_GloftF5HP_ML_GLGame_nativeTouchPressed },
    { "nativeTouchMoved", "(IIFF)V", (void*)Java_com_gameloft_android_GAND_GloftF5HP_ML_GLGame_nativeTouchMoved },
    { "nativeTouchReleased", "(IIFF)V", (void*)Java_com_gameloft_android_GAND_GloftF5HP_ML_GLGame_nativeTouchReleased },

    // GLResLoader
    { "nativeInit", "(Landroid/content/Context;Ljava/lang/String;)V", (void*)Java_com_gameloft_android_GAND_GloftF5HP_ML_GLResLoader_nativeInit },

    // GLUtils.Device
    { "nativeInit", "()V", (void*)Java_com_gameloft_android_GAND_GloftF5HP_ML_GLUtils_Device_nativeInit },

    { NULL, NULL, NULL }
};

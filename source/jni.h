#ifndef _JNI_H_
#define _JNI_H_

#include <stdint.h>

// Standard JNI types - opaque pointers that match Android NDK JNI ABI
typedef struct JNIEnv JNIEnv;
typedef struct _jobject *jobject;
typedef struct _jclass *jclass;
typedef struct _jstring *jstring;
typedef struct _jarray *jarray;
typedef struct _jthrowable *jthrowable;
typedef jobject jweak;

typedef jobject jbooleanArray;
typedef jobject jbyteArray;
typedef jobject jcharArray;
typedef jobject jshortArray;
typedef jobject jintArray;
typedef jobject jlongArray;
typedef jobject jfloatArray;
typedef jobject jdoubleArray;
typedef jobject jobjectArray;

typedef struct {
    uint8_t z;
    int8_t b;
    uint16_t c;
    int16_t s;
    int32_t i;
    int64_t j;
    float f;
    double d;
    void *l;
} jvalue;

typedef int32_t jsize;
typedef int jint;
typedef int64_t jlong;
typedef int8_t jbyte;
typedef uint16_t jchar;
typedef int16_t jshort;
typedef float jfloat;
typedef double jdouble;
typedef uint8_t jboolean;

#define JNI_FALSE 0
#define JNI_TRUE 1

#ifndef JNIEXPORT
#define JNIEXPORT
#endif
#ifndef JNICALL
#define JNICALL
#endif

#include "port_config.h"

#endif // _JNI_H_

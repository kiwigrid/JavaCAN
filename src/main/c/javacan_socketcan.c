/**
 * The MIT License
 * Copyright © 2018 Phillip Schichtel
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include "helpers.h"
#include <tel_schich_javacan_NativeInterface.h>
#include <unistd.h>
#include <sys/socket.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <stdlib.h>
#include <net/if.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <jni.h>
#include <limits.h>

JNIEXPORT jlong JNICALL Java_tel_schich_javacan_NativeInterface_resolveInterfaceName(JNIEnv *env, jclass class, jstring interface_name) {
    const char *ifname = (*env)->GetStringUTFChars(env, interface_name, false);
    unsigned int ifindex = interface_name_to_index(ifname);
    (*env)->ReleaseStringUTFChars(env, interface_name, ifname);
    return ifindex;
}

JNIEXPORT jint JNICALL Java_tel_schich_javacan_NativeInterface_createRawSocket(JNIEnv *env, jclass class) {
    return create_can_raw_socket();
}

JNIEXPORT jint JNICALL Java_tel_schich_javacan_NativeInterface_bindSocket(JNIEnv *env, jclass class, jint sock, jlong iface, jint rx, jint tx) {
    return bind_can_socket(sock, (unsigned int) (iface & 0xFFFFFFFF), (uint32_t) rx, (uint32_t) tx);
}

JNIEXPORT jint JNICALL Java_tel_schich_javacan_NativeInterface_close(JNIEnv *env, jclass class, jint sock) {
    return close(sock);
}

JNIEXPORT jint JNICALL Java_tel_schich_javacan_NativeInterface_errno(JNIEnv *env, jclass class) {
    return errno;
}

JNIEXPORT jstring JNICALL Java_tel_schich_javacan_NativeInterface_errstr(JNIEnv *env, jclass class, jint err) {
    return (*env)->NewStringUTF(env, strerror(err));
}

JNIEXPORT jint JNICALL Java_tel_schich_javacan_NativeInterface_setBlockingMode(JNIEnv *env, jclass class, jint sock, jboolean block) {
    return set_blocking_mode(sock, block);
}

JNIEXPORT jint JNICALL Java_tel_schich_javacan_NativeInterface_getBlockingMode(JNIEnv *env, jclass class, jint sock) {
    return is_blocking(sock);
}

JNIEXPORT jint JNICALL Java_tel_schich_javacan_NativeInterface_setTimeouts(JNIEnv *env, jclass class, jint sock, jlong read, jlong write) {
    static const size_t timeout_len = sizeof(struct timeval);
    struct timeval timeout;

    micros_to_timeval(&timeout, (uint64_t) read);
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, timeout_len) != 0) {
        return false;
    }

    micros_to_timeval(&timeout, (uint64_t) write);
    return setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, timeout_len);
}

JNIEXPORT jlong JNICALL Java_tel_schich_javacan_NativeInterface_write(JNIEnv *env, jclass class, jint sock, jbyteArray buf, jint offset, jint length) {
    void *raw_buf = (*env)->GetPrimitiveArrayCritical(env, buf, false);
    void *data_start = raw_buf + offset;
    ssize_t bytes_written = write(sock, data_start, (size_t) length);
    (*env)->ReleasePrimitiveArrayCritical(env, buf, raw_buf, 0);
    return bytes_written;
}

JNIEXPORT jlong JNICALL Java_tel_schich_javacan_NativeInterface_read(JNIEnv *env, jclass class, jint sock, jbyteArray buf, jint offset, jint length) {
    void *raw_buf = (*env)->GetPrimitiveArrayCritical(env, buf, false);
    void *data_start = raw_buf + offset;
    ssize_t bytes_read = read(sock, data_start, (size_t) length);
    (*env)->ReleasePrimitiveArrayCritical(env, buf, raw_buf, 0);
    return bytes_read;
}

JNIEXPORT jint JNICALL Java_tel_schich_javacan_NativeInterface_setFilters(JNIEnv *env, jclass class, jint sock, jbyteArray data) {
    void *rawData = (*env)->GetPrimitiveArrayCritical(env, data, false);
    int result = setsockopt(sock, SOL_CAN_RAW, CAN_RAW_FILTER, rawData, (socklen_t) (*env)->GetArrayLength(env, data));
    (*env)->ReleasePrimitiveArrayCritical(env, data, rawData, 0);

    return result;
}

JNIEXPORT jbyteArray JNICALL Java_tel_schich_javacan_NativeInterface_getFilters(JNIEnv *env, jclass class, jint sock) {
    // assign the signed integer max value to an unsigned integer, socketcan's getsockopt implementation uses int's
    // instead of uint's and resets the size to the actual size only if the given size is larger.
    socklen_t size = INT_MAX;
    // TODO this is a horrible idea, but it seems to be the only way to get all filters without knowing how many there are
    // see: https://github.com/torvalds/linux/blob/master/net/can/raw.c#L669-L683
    // surprisingly this does not increase the system memory usage given that this should be a significant chunk by numbers
    void* filters = malloc(size);
    if (filters == NULL) {
        return NULL;
    }
    int result = getsockopt(sock, SOL_CAN_RAW, CAN_RAW_FILTER, filters, &size);
    if (result == -1) {
        return NULL;
    }
    jbyteArray array = (*env)->NewByteArray(env, size);
    (*env)->SetByteArrayRegion(env, array, 0, size, (const jbyte *) filters);
    free(filters);
    return array;
}

JNIEXPORT jint JNICALL Java_tel_schich_javacan_NativeInterface_setLoopback(JNIEnv *env, jclass class, jint sock, jboolean enable) {
    return set_boolean_opt(sock, CAN_RAW_LOOPBACK, enable);
}

JNIEXPORT jint JNICALL Java_tel_schich_javacan_NativeInterface_getLoopback(JNIEnv *env, jclass class, jint sock) {
    return get_boolean_opt(sock, CAN_RAW_LOOPBACK);
}

JNIEXPORT jint JNICALL Java_tel_schich_javacan_NativeInterface_setReceiveOwnMessages(JNIEnv *env, jclass class, jint sock, jboolean enable) {
    return set_boolean_opt(sock, CAN_RAW_RECV_OWN_MSGS, enable);
}

JNIEXPORT jint JNICALL Java_tel_schich_javacan_NativeInterface_getReceiveOwnMessages(JNIEnv *env, jclass class, jint sock) {
    return get_boolean_opt(sock, CAN_RAW_RECV_OWN_MSGS);
}

JNIEXPORT jint JNICALL Java_tel_schich_javacan_NativeInterface_setJoinFilters(JNIEnv *env, jclass class, jint sock, jboolean enable) {
    return set_boolean_opt(sock, CAN_RAW_JOIN_FILTERS, enable);
}

JNIEXPORT jint JNICALL Java_tel_schich_javacan_NativeInterface_getJoinFilters(JNIEnv *env, jclass class, jint sock) {
    return get_boolean_opt(sock, CAN_RAW_JOIN_FILTERS);
}

JNIEXPORT jint JNICALL Java_tel_schich_javacan_NativeInterface_setAllowFDFrames(JNIEnv *env, jclass class, jint sock, jboolean enable) {
    return set_boolean_opt(sock, CAN_RAW_FD_FRAMES, enable);
}

JNIEXPORT jint JNICALL Java_tel_schich_javacan_NativeInterface_getAllowFDFrames(JNIEnv *env, jclass class, jint sock) {
    return get_boolean_opt(sock, CAN_RAW_FD_FRAMES);
}

JNIEXPORT jint JNICALL Java_tel_schich_javacan_NativeInterface_setErrorFilter(JNIEnv *env, jclass class, jint sock, jint mask) {
    can_err_mask_t err_mask = (can_err_mask_t) mask;
    return setsockopt(sock, SOL_CAN_RAW, CAN_RAW_ERR_FILTER, &err_mask, sizeof(err_mask));
}

JNIEXPORT jint JNICALL Java_tel_schich_javacan_NativeInterface_getErrorFilter(JNIEnv *env, jclass class, jint sock) {
    int mask = 0;
    socklen_t len = sizeof(mask);

    int result = getsockopt(sock, SOL_CAN_RAW, CAN_RAW_ERR_FILTER, &mask, &len);
    if (result == -1) {
        return -1;
    }
    return mask;
}

JNIEXPORT jshort JNICALL Java_tel_schich_javacan_NativeInterface_poll(JNIEnv *env, jclass class, jint sock, jint events, jint timeout) {
    return poll_single(sock, (short) events, timeout);
}
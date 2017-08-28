/***************************************************************************
 * Copyright (C) 2017 Kaloyan Raev
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ***************************************************************************/
#include <jni.h>
#include <string>
#include <storj.h>
#include <nettle/version.h>
#include <microhttpd/microhttpd.h>

static char cainfo_path[256];

typedef struct {
    JNIEnv *env;
    jobject callbackObject;
} jcallback_t;

typedef struct {
    jcallback_t base;
    jobject file;
    jstring localPath;
} jdownload_callback_t;

typedef struct {
    jcallback_t base;
    jstring filePath;
} jupload_callback_t;

extern "C"
JNIEXPORT void JNICALL
Java_name_raev_kaloyan_hellostorj_jni_Storj_setCAInfoPath(
        JNIEnv *env,
        jclass /* clazz */,
        jstring path_) {
    const char *path = env->GetStringUTFChars(path_, NULL);
    strcpy(cainfo_path, path);
    env->ReleaseStringUTFChars(path_, path);
}

static void error_callback(JNIEnv *env, jobject callbackObject, const char *message) {
    jclass callbackClass = env->GetObjectClass(callbackObject);
    jmethodID callbackMethod = env->GetMethodID(callbackClass,
                                                "onError",
                                                "(Ljava/lang/String;)V");
    env->CallVoidMethod(callbackObject,
                        callbackMethod,
                        env->NewStringUTF(message));
}

static void error_callback(JNIEnv *env, jobject callbackObject, jstring filePath, const char *message) {
    jclass callbackClass = env->GetObjectClass(callbackObject);
    jmethodID callbackMethod = env->GetMethodID(callbackClass,
                                                "onError",
                                                "(Ljava/lang/String;Ljava/lang/String;)V");
    env->CallVoidMethod(callbackObject,
                        callbackMethod,
                        filePath,
                        env->NewStringUTF(message));
}

static void error_callback(JNIEnv *env, jobject callbackObject, jobject file, const char *message) {
    jclass callbackClass = env->GetObjectClass(callbackObject);
    jmethodID callbackMethod = env->GetMethodID(callbackClass,
                                                "onError",
                                                "(Lname/raev/kaloyan/hellostorj/jni/File;Ljava/lang/String;)V");
    env->CallVoidMethod(callbackObject,
                        callbackMethod,
                        file,
                        env->NewStringUTF(message));
}

static void get_buckets_callback(uv_work_t *work_req, int status)
{
    assert(status == 0);
    get_buckets_request_t *req = (get_buckets_request_t *) work_req->data;
    jcallback_t *jcallback = (jcallback_t *) req->handle;
    JNIEnv *env = jcallback->env;
    jobject callbackObject = jcallback->callbackObject;

    if (req->status_code != 200 && req->status_code != 304) {
        char error_message[256];
        if (req->status_code == 401) {
            strcpy(error_message, "Invalid user credentials");
        } else {
            sprintf(error_message, "Request failed with status code: %i", req->status_code);
        }
        error_callback(env, callbackObject, error_message);
    } else {
        jclass bucketClass = env->FindClass("name/raev/kaloyan/hellostorj/jni/Bucket");
        jobjectArray bucketArray = env->NewObjectArray(req->total_buckets, bucketClass, NULL);
        jmethodID bucketInit = env->GetMethodID(bucketClass,
                                                "<init>",
                                                "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Z)V");

        for (int i = 0; i < req->total_buckets; i++) {
            storj_bucket_meta_t *bucket = &req->buckets[i];

            jstring id = env->NewStringUTF(bucket->id);
            jstring name = env->NewStringUTF(bucket->name);
            jstring created = env->NewStringUTF(bucket->created);

            jobject bucketObject = env->NewObject(bucketClass,
                                                  bucketInit,
                                                  id,
                                                  name,
                                                  created,
                                                  bucket->decrypted);
            env->SetObjectArrayElement(bucketArray, i, bucketObject);

            env->DeleteLocalRef(bucketObject);
            env->DeleteLocalRef(id);
            env->DeleteLocalRef(name);
            env->DeleteLocalRef(created);
        }

        jclass callbackClass = env->GetObjectClass(callbackObject);
        jmethodID callbackMethod = env->GetMethodID(callbackClass,
                                                    "onBucketsReceived",
                                                    "([Lname/raev/kaloyan/hellostorj/jni/Bucket;)V");
        env->CallVoidMethod(callbackObject, callbackMethod, bucketArray);
    }

    json_object_put(req->response);
    storj_free_get_buckets_request(req);
    free(work_req);
}

extern "C"
JNIEXPORT void JNICALL
Java_name_raev_kaloyan_hellostorj_jni_Storj_getBuckets(
        JNIEnv *env,
        jclass /* clazz */,
        jstring user_,
        jstring pass_,
        jstring mnemonic_,
        jobject callbackObject) {
    const char *user = env->GetStringUTFChars(user_, NULL);
    const char *pass = env->GetStringUTFChars(pass_, NULL);
    const char *mnemonic = env->GetStringUTFChars(mnemonic_, NULL);

    storj_http_options_t http_options = {
            .user_agent = "Hello Storj",
            .cainfo_path = cainfo_path,
            .low_speed_limit = STORJ_LOW_SPEED_LIMIT,
            .low_speed_time = STORJ_LOW_SPEED_TIME,
            .timeout = STORJ_HTTP_TIMEOUT
    };

    storj_bridge_options_t options = {
            .proto = "https",
            .host  = "api.storj.io",
            .port  = 443,
            .user  = user,
            .pass  = pass
    };

    storj_encrypt_options_t encrypt_options = {
            .mnemonic = mnemonic
    };

    storj_log_options_t log_options = {
            .logger = NULL,
            .level = 0
    };

    storj_env_t *storj_env = storj_init_env(&options, &encrypt_options, &http_options, &log_options);

    if (!storj_env) {
        error_callback(env, callbackObject, "Failed to initialize Storj environment");
    } else {
        jcallback_t jcallback = {
                .env = env,
                .callbackObject = callbackObject
        };
        storj_bridge_get_buckets(storj_env, &jcallback, get_buckets_callback);

        uv_run(storj_env->loop, UV_RUN_DEFAULT);

        storj_destroy_env(storj_env);
    }

    env->ReleaseStringUTFChars(user_, user);
    env->ReleaseStringUTFChars(pass_, pass);
    env->ReleaseStringUTFChars(mnemonic_, mnemonic);
}

static void create_bucket_callback(uv_work_t *work_req, int status)
{
    assert(status == 0);
    create_bucket_request_t *req = (create_bucket_request_t *) work_req->data;
    jcallback_t *jcallback = (jcallback_t *) req->handle;
    JNIEnv *env = jcallback->env;
    jobject callbackObject = jcallback->callbackObject;

    if (req->status_code != 201) {
        char error_message[256];
        if (req->status_code == 404) {
            sprintf(error_message, "Cannot create bucket [%s]. Name already exists.", req->bucket->name);
        } else if (req->status_code == 401) {
            strcpy(error_message, "Invalid user credentials");
        } else {
            sprintf(error_message, "Request failed with status code: %i", req->status_code);
        }
        error_callback(env, callbackObject, error_message);
    } else {
        jclass bucketClass = env->FindClass("name/raev/kaloyan/hellostorj/jni/Bucket");
        jmethodID bucketInit = env->GetMethodID(bucketClass,
                                                "<init>",
                                                "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Z)V");

        jstring id = env->NewStringUTF(req->bucket->id);
        jstring name = env->NewStringUTF(req->bucket->name);
        jstring created = env->NewStringUTF(req->bucket->created);
        jobject bucketObject = env->NewObject(bucketClass,
                                              bucketInit,
                                              id,
                                              name,
                                              created,
                                              req->bucket->decrypted);

        jclass callbackClass = env->GetObjectClass(callbackObject);
        jmethodID callbackMethod = env->GetMethodID(callbackClass,
                                                    "onBucketCreated",
                                                    "(Lname/raev/kaloyan/hellostorj/jni/Bucket;)V");
        env->CallVoidMethod(callbackObject, callbackMethod, bucketObject);
    }

    json_object_put(req->response);
    free((char *)req->encrypted_bucket_name);
    free(req->bucket);
    free(req);
    free(work_req);
}

extern "C"
JNIEXPORT void JNICALL
Java_name_raev_kaloyan_hellostorj_jni_Storj_createBucket(
        JNIEnv *env,
        jclass /* clazz */,
        jstring user_,
        jstring pass_,
        jstring mnemonic_,
        jstring bucketName_,
        jobject callbackObject) {
    const char *user = env->GetStringUTFChars(user_, NULL);
    const char *pass = env->GetStringUTFChars(pass_, NULL);
    const char *mnemonic = env->GetStringUTFChars(mnemonic_, NULL);
    const char *bucketName = env->GetStringUTFChars(bucketName_, NULL);

    storj_http_options_t http_options = {
            .user_agent = "Hello Storj",
            .cainfo_path = cainfo_path,
            .low_speed_limit = STORJ_LOW_SPEED_LIMIT,
            .low_speed_time = STORJ_LOW_SPEED_TIME,
            .timeout = STORJ_HTTP_TIMEOUT
    };

    storj_bridge_options_t options = {
            .proto = "https",
            .host  = "api.storj.io",
            .port  = 443,
            .user  = user,
            .pass  = pass
    };

    storj_encrypt_options_t encrypt_options = {
            .mnemonic = mnemonic
    };

    storj_log_options_t log_options = {
            .logger = NULL,
            .level = 0
    };

    storj_env_t *storj_env = storj_init_env(&options, &encrypt_options, &http_options, &log_options);

    if (!storj_env) {
        error_callback(env, callbackObject, "Failed to initialize Storj environment");
    } else {
        jcallback_t jcallback = {
                .env = env,
                .callbackObject = callbackObject
        };
        storj_bridge_create_bucket(storj_env, bucketName, &jcallback, create_bucket_callback);

        uv_run(storj_env->loop, UV_RUN_DEFAULT);

        storj_destroy_env(storj_env);
    }

    env->ReleaseStringUTFChars(user_, user);
    env->ReleaseStringUTFChars(pass_, pass);
    env->ReleaseStringUTFChars(mnemonic_, mnemonic);
    env->ReleaseStringUTFChars(bucketName_, bucketName);
}

static void list_files_callback(uv_work_t *work_req, int status)
{
    assert(status == 0);
    list_files_request_t *req = (list_files_request_t *) work_req->data;
    jcallback_t *jcallback = (jcallback_t *) req->handle;
    JNIEnv *env = jcallback->env;
    jobject callbackObject = jcallback->callbackObject;

    if (req->status_code != 200) {
        char error_message[256];
        if (req->status_code == 404) {
            sprintf(error_message, "Bucket id [%s] does not exist", req->bucket_id);
        } else if (req->status_code == 400) {
            sprintf(error_message, "Bucket id [%s] is invalid", req->bucket_id);
        } else if (req->status_code == 401) {
            strcpy(error_message, "Invalid user credentials");
        } else {
            sprintf(error_message, "Request failed with status code: %i", req->status_code);
        }
        error_callback(env, callbackObject, error_message);
    } else {
        jclass fileClass = env->FindClass("name/raev/kaloyan/hellostorj/jni/File");
        jobjectArray fileArray = env->NewObjectArray(req->total_files, fileClass, NULL);
        jmethodID fileInit = env->GetMethodID(fileClass,
                                              "<init>",
                                              "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;ZJLjava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");

        for (int i = 0; i < req->total_files; i++) {
            storj_file_meta_t *file = &req->files[i];

            jstring id = (file->id) ? env->NewStringUTF(file->id) : NULL;
            jstring filename = (file->filename) ? env->NewStringUTF(file->filename) : NULL;
            jstring created = (file->created) ? env->NewStringUTF(file->created) : NULL;
            jstring mimetype = (file->mimetype) ? env->NewStringUTF(file->mimetype) : NULL;
            jstring erasure = (file->erasure) ? env->NewStringUTF(file->erasure) : NULL;
            jstring index = (file->index) ? env->NewStringUTF(file->index) : NULL;
            jstring hmac = (file->hmac) ? env->NewStringUTF(file->hmac) : NULL;

            jobject fileObject = env->NewObject(fileClass,
                                                fileInit,
                                                id,
                                                filename,
                                                created,
                                                file->decrypted,
                                                file->size,
                                                mimetype,
                                                erasure,
                                                index,
                                                hmac);
            env->SetObjectArrayElement(fileArray, i, fileObject);

            env->DeleteLocalRef(fileObject);
            if (id) {
                env->DeleteLocalRef(id);
            }
            if (filename) {
                env->DeleteLocalRef(filename);
            }
            if (created) {
                env->DeleteLocalRef(created);
            }
            if (mimetype) {
                env->DeleteLocalRef(mimetype);
            }
            if (erasure) {
                env->DeleteLocalRef(erasure);
            }
            if (index) {
                env->DeleteLocalRef(index);
            }
            if (hmac) {
                env->DeleteLocalRef(hmac);
            }
        }

        jclass callbackClass = env->GetObjectClass(callbackObject);
        jmethodID callbackMethod = env->GetMethodID(callbackClass,
                                                    "onFilesReceived",
                                                    "([Lname/raev/kaloyan/hellostorj/jni/File;)V");
        env->CallVoidMethod(callbackObject, callbackMethod, fileArray);
    }

    json_object_put(req->response);
    storj_free_list_files_request(req);
    free(work_req);
}

extern "C"
JNIEXPORT void JNICALL
Java_name_raev_kaloyan_hellostorj_jni_Storj_listFiles(
        JNIEnv *env,
        jclass /* clazz */,
        jstring user_,
        jstring pass_,
        jstring mnemonic_,
        jstring bucketId_,
        jobject callbackObject) {
    const char *user = env->GetStringUTFChars(user_, NULL);
    const char *pass = env->GetStringUTFChars(pass_, NULL);
    const char *mnemonic = env->GetStringUTFChars(mnemonic_, NULL);
    const char *bucketId = env->GetStringUTFChars(bucketId_, NULL);

    storj_http_options_t http_options = {
            .user_agent = "Hello Storj",
            .cainfo_path = cainfo_path,
            .low_speed_limit = STORJ_LOW_SPEED_LIMIT,
            .low_speed_time = STORJ_LOW_SPEED_TIME,
            .timeout = STORJ_HTTP_TIMEOUT
    };

    storj_bridge_options_t options = {
            .proto = "https",
            .host  = "api.storj.io",
            .port  = 443,
            .user  = user,
            .pass  = pass
    };

    storj_encrypt_options_t encrypt_options = {
            .mnemonic = mnemonic
    };

    storj_log_options_t log_options = {
            .logger = NULL,
            .level = 0
    };

    storj_env_t *storj_env = storj_init_env(&options, &encrypt_options, &http_options, &log_options);

    if (!storj_env) {
        error_callback(env, callbackObject, "Failed to initialize Storj environment");
    } else {
        jcallback_t jcallback = {
                .env = env,
                .callbackObject = callbackObject
        };
        storj_bridge_list_files(storj_env, bucketId, &jcallback, list_files_callback);

        uv_run(storj_env->loop, UV_RUN_DEFAULT);

        storj_destroy_env(storj_env);
    }

    env->ReleaseStringUTFChars(user_, user);
    env->ReleaseStringUTFChars(pass_, pass);
    env->ReleaseStringUTFChars(mnemonic_, mnemonic);
    env->ReleaseStringUTFChars(bucketId_, bucketId);
}

static void download_file_progress_callback(double progress, uint64_t bytes, uint64_t total_bytes, void *handle)
{
    jcallback_t *jcallback = (jcallback_t *) handle;
    JNIEnv *env = jcallback->env;
    jobject callbackObject = jcallback->callbackObject;

    jclass callbackClass = env->GetObjectClass(callbackObject);
    jmethodID callbackMethod = env->GetMethodID(callbackClass,
                                                "onProgress",
                                                "(Lname/raev/kaloyan/hellostorj/jni/File;DJJ)V");

    jdownload_callback_t *cb_extension = (jdownload_callback_t *) handle;
    env->CallVoidMethod(callbackObject,
                        callbackMethod,
                        cb_extension->file,
                        progress,
                        bytes,
                        total_bytes);

    // this function is called multiple times during file download
    // cleanup is necessary to avoid local reference table overflow
    env->DeleteLocalRef(callbackClass);
}

static void download_file_complete_callback(int status, FILE *fd, void *handle)
{
    fclose(fd);

    jcallback_t *jcallback = (jcallback_t *) handle;
    JNIEnv *env = jcallback->env;
    jobject callbackObject = jcallback->callbackObject;
    jdownload_callback_t *cb_extension = (jdownload_callback_t *) handle;

    if (status) {
        char error_message[256];
        sprintf(error_message, "Download failed (status: %d)", status);
        error_callback(env, callbackObject, cb_extension->file, error_message);
    } else {
        jclass callbackClass = env->GetObjectClass(callbackObject);
        jmethodID callbackMethod = env->GetMethodID(callbackClass,
                                                    "onComplete",
                                                    "(Lname/raev/kaloyan/hellostorj/jni/File;Ljava/lang/String;)V");

        env->CallVoidMethod(callbackObject,
                            callbackMethod,
                            cb_extension->file,
                            cb_extension->localPath);
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_name_raev_kaloyan_hellostorj_jni_Storj_downloadFile(
        JNIEnv *env,
        jclass /* clazz */,
        jstring bucketId_,
        jobject file_,
        jstring path_,
        jstring user_,
        jstring pass_,
        jstring mnemonic_,
        jobject callbackObject) {
    jclass fileClass = env->GetObjectClass(file_);
    jmethodID mid = env->GetMethodID(fileClass, "getId", "()Ljava/lang/String;");
    jstring fileId_ = (jstring) env->CallObjectMethod(file_, mid);

    const char *bucket_id = env->GetStringUTFChars(bucketId_, NULL);
    const char *file_id = env->GetStringUTFChars(fileId_, NULL);
    const char *path = env->GetStringUTFChars(path_, NULL);
    const char *user = env->GetStringUTFChars(user_, NULL);
    const char *pass = env->GetStringUTFChars(pass_, NULL);
    const char *mnemonic = env->GetStringUTFChars(mnemonic_, NULL);

    storj_http_options_t http_options = {
            .user_agent = "Hello Storj",
            .cainfo_path = cainfo_path,
            .low_speed_limit = STORJ_LOW_SPEED_LIMIT,
            .low_speed_time = STORJ_LOW_SPEED_TIME,
            .timeout = STORJ_HTTP_TIMEOUT
    };

    storj_bridge_options_t options = {
            .proto = "https",
            .host  = "api.storj.io",
            .port  = 443,
            .user  = user,
            .pass  = pass
    };

    storj_encrypt_options_t encrypt_options = {
            .mnemonic = mnemonic
    };

    storj_log_options_t log_options = {
            .logger = NULL,
            .level = 0
    };

    storj_env_t *storj_env = storj_init_env(&options, &encrypt_options, &http_options, &log_options);

    if (!storj_env) {
        error_callback(env, callbackObject, file_, "Failed to initialize Storj environment");
    } else {
        jcallback_t jcallback = {
                .env = env,
                .callbackObject = callbackObject
        };

        jdownload_callback_t cb_extension = {
                .base = jcallback,
                .file = file_,
                .localPath = path_
        };

        FILE *fd = NULL;

        if (path) {
            if (access(path, F_OK) != -1) {
                // TODO ask user if file should be overwritten
                unlink(path);
            }

            fd = fopen(path, "w+");
        }

        if (fd == NULL) {
            char error_message[256];
            sprintf(error_message, "Unable to open %s: %s", path, strerror(errno));
            error_callback(env, callbackObject, file_, error_message);
        } else {
            uv_signal_t *sig = (uv_signal_t *) malloc(sizeof(uv_signal_t));
            uv_signal_init(storj_env->loop, sig);
//          uv_signal_start(sig, download_signal_handler, SIGINT);

            storj_download_state_t *state = (storj_download_state_t *) malloc(sizeof(storj_download_state_t));
            if (!state) {
                error_callback(env, callbackObject, file_, "Failed to allocate memory for storj_download_state_t.");
            } else {
                sig->data = state;

                int status = storj_bridge_resolve_file(storj_env,
                                                       state,
                                                       bucket_id,
                                                       file_id,
                                                       fd,
                                                       &cb_extension,
                                                       download_file_progress_callback,
                                                       download_file_complete_callback);
                assert(status == 0);

                uv_run(storj_env->loop, UV_RUN_DEFAULT);
            }
        }

        storj_destroy_env(storj_env);
    }

    env->ReleaseStringUTFChars(bucketId_, bucket_id);
    env->ReleaseStringUTFChars(fileId_, file_id);
    env->ReleaseStringUTFChars(path_, path);
    env->ReleaseStringUTFChars(user_, user);
    env->ReleaseStringUTFChars(pass_, pass);
    env->ReleaseStringUTFChars(mnemonic_, mnemonic);
}

static void upload_file_progress_callback(double progress, uint64_t bytes, uint64_t total_bytes, void *handle)
{
    jcallback_t *jcallback = (jcallback_t *) handle;
    JNIEnv *env = jcallback->env;
    jobject callbackObject = jcallback->callbackObject;

    jclass callbackClass = env->GetObjectClass(callbackObject);
    jmethodID callbackMethod = env->GetMethodID(callbackClass,
                                                "onProgress",
                                                "(Ljava/lang/String;DJJ)V");

    jupload_callback_t *cb_extension = (jupload_callback_t *) handle;
    env->CallVoidMethod(callbackObject,
                        callbackMethod,
                        cb_extension->filePath,
                        progress,
                        bytes,
                        total_bytes);

    // this function is called multiple times during file download
    // cleanup is necessary to avoid local reference table overflow
    env->DeleteLocalRef(callbackClass);
}

static void upload_file_complete_callback(int status, char *file_id, void *handle)
{
    jcallback_t *jcallback = (jcallback_t *) handle;
    JNIEnv *env = jcallback->env;
    jobject callbackObject = jcallback->callbackObject;
    jupload_callback_t *cb_extension = (jupload_callback_t *) handle;

    if (status) {
        char error_message[256];
        sprintf(error_message, "Upload failed (status: %d)", status);
        error_callback(env, callbackObject, cb_extension->filePath, error_message);
    } else {
        jclass callbackClass = env->GetObjectClass(callbackObject);
        jmethodID callbackMethod = env->GetMethodID(callbackClass,
                                                    "onComplete",
                                                    "(Ljava/lang/String;)V");

        env->CallVoidMethod(callbackObject,
                            callbackMethod,
                            cb_extension->filePath);
    }

    free(file_id);
}

extern "C"
JNIEXPORT void JNICALL
Java_name_raev_kaloyan_hellostorj_jni_Storj_uploadFile(
        JNIEnv *env,
        jclass /* clazz */,
        jstring bucketId_,
        jstring filePath_,
        jstring user_,
        jstring pass_,
        jstring mnemonic_,
        jobject callbackObject) {
    const char *bucket_id = env->GetStringUTFChars(bucketId_, NULL);
    const char *file_path = env->GetStringUTFChars(filePath_, NULL);
    const char *user = env->GetStringUTFChars(user_, NULL);
    const char *pass = env->GetStringUTFChars(pass_, NULL);
    const char *mnemonic = env->GetStringUTFChars(mnemonic_, NULL);

    storj_http_options_t http_options = {
            .user_agent = "Hello Storj",
            .cainfo_path = cainfo_path,
            .low_speed_limit = STORJ_LOW_SPEED_LIMIT,
            .low_speed_time = STORJ_LOW_SPEED_TIME,
            .timeout = STORJ_HTTP_TIMEOUT
    };

    storj_bridge_options_t options = {
            .proto = "https",
            .host  = "api.storj.io",
            .port  = 443,
            .user  = user,
            .pass  = pass
    };

    storj_encrypt_options_t encrypt_options = {
            .mnemonic = mnemonic
    };

    storj_log_options_t log_options = {
            .logger = NULL,
            .level = 0
    };

    storj_env_t *storj_env = storj_init_env(&options, &encrypt_options, &http_options, &log_options);

    if (!storj_env) {
        error_callback(env, callbackObject, filePath_, "Failed to initialize Storj environment");
    } else {
        jcallback_t jcallback = {
                .env = env,
                .callbackObject = callbackObject
        };

        jupload_callback_t cb_extension = {
                .base = jcallback,
                .filePath = filePath_
        };

        FILE *fd = fopen(file_path, "r");

        if (!fd) {
            error_callback(env, callbackObject, filePath_, "Invalid file path");
        } else {
            const char *file_name = strrchr(file_path, '/');;
            if (!file_name) {
                file_name = file_path;
            }
            if (file_name[0] == '/') {
                file_name++;
            }

            storj_upload_opts_t upload_opts = {
                    .prepare_frame_limit = 1,
                    .push_frame_limit = 64,
                    .push_shard_limit = 64,
                    .rs = true,
                    .bucket_id = bucket_id,
                    .file_name = file_name,
                    .fd = fd
            };

            uv_signal_t *sig = (uv_signal_t *) malloc(sizeof(uv_signal_t));
            if (!sig) {
                // TODO error
                return;
            }
            uv_signal_init(storj_env->loop, sig);
//            uv_signal_start(sig, upload_signal_handler, SIGINT);

            storj_upload_state_t *state = (storj_upload_state_t *) malloc(sizeof(storj_upload_state_t));
            if (!state) {
                // TODO error
                return;
            }

            sig->data = state;

            int status = storj_bridge_store_file(storj_env,
                                                 state,
                                                 &upload_opts,
                                                 &cb_extension,
                                                 upload_file_progress_callback,
                                                 upload_file_complete_callback);
            assert(status == 0);

            uv_run(storj_env->loop, UV_RUN_DEFAULT);
        }

        storj_destroy_env(storj_env);
    }

    env->ReleaseStringUTFChars(bucketId_, bucket_id);
    env->ReleaseStringUTFChars(filePath_, file_path);
    env->ReleaseStringUTFChars(user_, user);
    env->ReleaseStringUTFChars(pass_, pass);
    env->ReleaseStringUTFChars(mnemonic_, mnemonic);
}

static void get_info_callback(uv_work_t *work_req, int status)
{
    assert(status == 0);
    json_request_t *req = (json_request_t *) work_req->data;
    jcallback_t *jcallback = (jcallback_t *) req->handle;
    JNIEnv *env = jcallback->env;
    jobject callbackObject = jcallback->callbackObject;

    if (req->error_code || req->response == NULL) {
        char error_message[256];
        if (req->error_code) {
            sprintf(error_message, "Request failed, reason: %s",
                   curl_easy_strerror((CURLcode) req->error_code));
        } else {
            strcpy(error_message, "Failed to get info.");
        }
        error_callback(env, callbackObject, error_message);
    } else {
        struct json_object *info;
        json_object_object_get_ex(req->response, "info", &info);
        struct json_object *title;
        json_object_object_get_ex(info, "title", &title);
        struct json_object *description;
        json_object_object_get_ex(info, "description", &description);
        struct json_object *version;
        json_object_object_get_ex(info, "version", &version);
        struct json_object *host;
        json_object_object_get_ex(req->response, "host", &host);

        jclass callbackClass = env->GetObjectClass(callbackObject);
        jmethodID callbackMethod = env->GetMethodID(callbackClass,
                                                    "onInfoReceived",
                                                    "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
        env->CallVoidMethod(callbackObject,
                            callbackMethod,
                            env->NewStringUTF(json_object_get_string(title)),
                            env->NewStringUTF(json_object_get_string(description)),
                            env->NewStringUTF(json_object_get_string(version)),
                            env->NewStringUTF(json_object_get_string(host)));
    }

    json_object_put(req->response);
    free(req);
    free(work_req);
}

extern "C"
JNIEXPORT void JNICALL
Java_name_raev_kaloyan_hellostorj_jni_Storj_getInfo(
        JNIEnv *env,
        jclass /* clazz */,
        jobject callbackObject) {
    storj_bridge_options_t options = {
            .proto = "https",
            .host  = "api.storj.io",
            .port  = 443,
            .user  = NULL,
            .pass  = NULL
    };

    storj_http_options_t http_options = {
            .user_agent = "Hello Storj",
            .cainfo_path = cainfo_path,
            .low_speed_limit = STORJ_LOW_SPEED_LIMIT,
            .low_speed_time = STORJ_LOW_SPEED_TIME,
            .timeout = STORJ_HTTP_TIMEOUT
    };

    storj_log_options_t log_options = {
            .logger = NULL,
            .level = 0
    };

    storj_env_t *storj_env = NULL;
    storj_env = storj_init_env(&options, NULL, &http_options, &log_options);
    if (!storj_env) {
        error_callback(env, callbackObject, "Failed to initialize Storj environment");
    } else {
        jcallback_t jcallback = {
                .env = env,
                .callbackObject = callbackObject
        };
        storj_bridge_get_info(storj_env, &jcallback, get_info_callback);

        uv_run(storj_env->loop, UV_RUN_DEFAULT);

        storj_destroy_env(storj_env);
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_name_raev_kaloyan_hellostorj_jni_Storj_exportKeys(
        JNIEnv *env,
        jclass type,
        jstring location_,
        jstring passphrase_) {
    const char *location = env->GetStringUTFChars(location_, NULL);
    const char *passphrase = env->GetStringUTFChars(passphrase_, NULL);
    char *user = NULL;
    char *pass = NULL;
    char *mnemonic = NULL;

    jobject keysObject = NULL;
    if (!storj_decrypt_read_auth(location, passphrase, &user, &pass, &mnemonic)) {
        jclass keysClass = env->FindClass("name/raev/kaloyan/hellostorj/jni/Keys");
        jmethodID keysInit = env->GetMethodID(keysClass,
                                              "<init>",
                                              "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
        keysObject = env->NewObject(keysClass,
                                    keysInit,
                                    env->NewStringUTF(user),
                                    env->NewStringUTF(pass),
                                    env->NewStringUTF(mnemonic));
    }

    if (user) {
        free(user);
    }
    if (pass) {
        free(pass);
    }
    if (mnemonic) {
        free(mnemonic);
    }
    env->ReleaseStringUTFChars(location_, location);
    env->ReleaseStringUTFChars(passphrase_, passphrase);

    return keysObject;
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_name_raev_kaloyan_hellostorj_jni_Storj_writeAuthFile(
        JNIEnv *env,
        jclass /* clazz */,
        jstring location_,
        jstring user_,
        jstring pass_,
        jstring mnemonic_,
        jstring passphrase_) {
    const char *location = env->GetStringUTFChars(location_, NULL);
    const char *user = env->GetStringUTFChars(user_, NULL);
    const char *pass = env->GetStringUTFChars(pass_, NULL);
    const char *mnemonic = env->GetStringUTFChars(mnemonic_, NULL);
    const char *passphrase = env->GetStringUTFChars(passphrase_, NULL);

    int result = storj_encrypt_write_auth(location, passphrase, user, pass, mnemonic);

    env->ReleaseStringUTFChars(location_, location);
    env->ReleaseStringUTFChars(user_, user);
    env->ReleaseStringUTFChars(pass_, pass);
    env->ReleaseStringUTFChars(mnemonic_, mnemonic);
    env->ReleaseStringUTFChars(passphrase_, passphrase);

    return result == 0;
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_name_raev_kaloyan_hellostorj_jni_Storj_checkMnemonic(
        JNIEnv *env,
        jclass /* clazz */,
        jstring mnemonic_) {
    const char *mnemonic = env->GetStringUTFChars(mnemonic_, NULL);
    bool result = storj_mnemonic_check(mnemonic);
    env->ReleaseStringUTFChars(mnemonic_, mnemonic);
    return result;
}

extern "C"
JNIEXPORT jstring JNICALL
Java_name_raev_kaloyan_hellostorj_jni_Storj_generateMnemonic(
        JNIEnv *env,
        jclass /* clazz */,
        jint strength) {
    char *mnemonic = NULL;
    storj_mnemonic_generate(strength, &mnemonic);
    return env->NewStringUTF(mnemonic);
}

extern "C"
JNIEXPORT jlong JNICALL
Java_name_raev_kaloyan_hellostorj_jni_Storj_getTimestamp(
        JNIEnv * /* env */,
        jclass /* clazz */) {
    return storj_util_timestamp();
}

extern "C"
JNIEXPORT jstring JNICALL
Java_name_raev_kaloyan_hellostorj_jni_NativeLibraries_getJsonCVersion(
        JNIEnv *env,
        jclass /* clazz */) {
    return env->NewStringUTF(json_c_version());
}

extern "C"
JNIEXPORT jstring JNICALL
Java_name_raev_kaloyan_hellostorj_jni_NativeLibraries_getCurlVersion(
        JNIEnv *env,
        jclass /* clazz */) {
    return env->NewStringUTF(curl_version());
}

extern "C"
JNIEXPORT jstring JNICALL
Java_name_raev_kaloyan_hellostorj_jni_NativeLibraries_getLibuvVersion(
        JNIEnv *env,
        jclass /* clazz */) {
    return env->NewStringUTF(uv_version_string());
}

extern "C"
JNIEXPORT jstring JNICALL
Java_name_raev_kaloyan_hellostorj_jni_NativeLibraries_getNettleVersion(
        JNIEnv *env,
        jclass /* clazz */) {
    char version[5];
    sprintf(version, "%d.%d", nettle_version_major(), nettle_version_minor());
    return env->NewStringUTF(version);
}

extern "C"
JNIEXPORT jstring JNICALL
Java_name_raev_kaloyan_hellostorj_jni_NativeLibraries_getMHDVersion(
        JNIEnv *env,
        jclass /* clazz */) {
    return env->NewStringUTF(MHD_get_version());
}

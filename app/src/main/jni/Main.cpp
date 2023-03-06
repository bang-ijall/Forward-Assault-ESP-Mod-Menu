#include <list>
#include <vector>
#include <string.h>
#include <pthread.h>
#include <cstring>
#include <jni.h>
#include <unistd.h>
#include <fstream>
#include <iostream>
#include "Includes/Logger.h"
#include "Includes/obfuscate.h"
#include "Includes/Utils.h"
#include "KittyMemory/MemoryPatch.h"
#include "Menu.h"
#include "Unity/Color.h"
#include "Unity/Vector2.h"
#include "Unity/Vector3.h"
#include "Includes/ESPOverlay.h"
#include "Includes/ESPManager.h"
#include "Structs/Tools.h"
#include "Structs/fake_dlfcn.h"
#include "Structs/Il2Cpp.h"

#include <Substrate/SubstrateHook.h>
#include <Substrate/CydiaSubstrate.h>

bool ESP, ESPLine;

void *(*get_transform)(void *instance);
void (*get_position_Injected)(void *instance, Vector3 &ret);
void (*WorldToScreenPoint_Injected)(void *instance, Vector3 position, int eye, Vector3 &ret);
void *(*get_MainCamera)(void *instance);
void (*SetResolution)(int width, int height, bool fullscreen);

//Target lib here
#define targetLibName OBFUSCATE("libil2cpp.so")

Vector3 get_position(void *instance) {
    if (instance != NULL) {
        Vector3 ret;
        get_position_Injected(instance, ret);
        return ret;
    }
    return Vector3();
}

Vector3 WorldToScreenPoint(void *instance, Vector3 position) {
    if (instance != NULL) {
        Vector3 ret;
        WorldToScreenPoint_Injected(instance, position, 4, ret);
        return ret;
    }
    return Vector3();
}

void (*old_Update)(void *instance);
void Update(void *instance) {
    if (instance != NULL) {
        espManager->tryAddEnemy(instance);
    }
    old_Update(instance);
}

void (*old_OnDestroy)(void *instance);
void OnDestroy(void *instance) {
    if (instance != NULL) {
        espManager->removeEnemyGivenObject(instance);
    }
    old_OnDestroy(instance);
}

// we will run our hacks in a new thread so our while loop doesn't block process main thread
void *hack_thread(void *) {
    LOGI(OBFUSCATE("pthread created"));

    //Check if target lib is loaded
    do {
        sleep(1);
    } while (!isLibraryLoaded(targetLibName));

    //Anti-lib rename
    /*
    do {
        sleep(1);
    } while (!isLibraryLoaded("libYOURNAME.so"));*/

    LOGI(OBFUSCATE("%s has been loaded"), (const char *) targetLibName);

    espManager = new ESPManager();
    Il2CppAttach();

    MSHookFunction((void *) Il2CppGetMethodOffset("Assembly-CSharp.dll", "", "Player", "Update", 0), (void *) Update, (void **) &old_Update);
    MSHookFunction((void *) Il2CppGetMethodOffset("Assembly-CSharp.dll", "", "Player", "OnDestroy", 0), (void *) OnDestroy, (void **) &old_OnDestroy);

    get_transform = (void *(*)(void *)) Il2CppGetMethodOffset("UnityEngine.dll", "UnityEngine", "Component", "get_transform", 0);
    get_position_Injected = (void (*)(void *, Vector3 &)) Il2CppGetMethodOffset("UnityEngine.dll", "UnityEngine", "Transform", "get_position_Injected", 1);
    WorldToScreenPoint_Injected = (void (*)(void *, Vector3, int, Vector3 &)) Il2CppGetMethodOffset("UnityEngine.dll", "UnityEngine", "Camera", "WorldToScreenPoint_Injected", 3);
    SetResolution = (void (*)(int, int, bool)) Il2CppGetMethodOffset("UnityEngine.dll", "UnityEngine", "Screen", "SetResolution", 3);
    get_MainCamera = (void *(*)(void *)) Il2CppGetMethodOffset("Assembly-CSharp.dll", "", "CameraManager", "get_MainCamera", 0);

    LOGI(OBFUSCATE("Done"));

    return NULL;
}

//JNI calls
extern "C" {

// Do not change or translate the first text unless you know what you are doing
// Assigning feature numbers is optional. Without it, it will automatically count for you, starting from 0
// Assigned feature numbers can be like any numbers 1,3,200,10... instead in order 0,1,2,3,4,5...
// ButtonLink, Category, RichTextView and RichWebView is not counted. They can't have feature number assigned
// Toggle, ButtonOnOff and Checkbox can be switched on by default, if you add True_. Example: CheckBox_True_The Check Box
// To learn HTML, go to this page: https://www.w3schools.com/

JNIEXPORT jobjectArray
JNICALL
Java_uk_lgl_modmenu_FloatingModMenuService_getFeatureList(JNIEnv *env, jobject context) {
    jobjectArray ret;

    //Toasts added here so it's harder to remove it
    MakeToast(env, context, OBFUSCATE("Modded by LGL"), Toast::LENGTH_LONG);

    const char *features[] = {
            OBFUSCATE("ButtonOnOff_Enable ESP"),
            OBFUSCATE("Toggle_ESP Line")
    };

    //Now you dont have to manually update the number everytime;
    int Total_Feature = (sizeof features / sizeof features[0]);
    ret = (jobjectArray)
            env->NewObjectArray(Total_Feature, env->FindClass(OBFUSCATE("java/lang/String")),
                                env->NewStringUTF(""));

    for (int i = 0; i < Total_Feature; i++)
        env->SetObjectArrayElement(ret, i, env->NewStringUTF(features[i]));

    pthread_t ptid;
    pthread_create(&ptid, NULL, antiLeech, NULL);

    return (ret);
}

JNIEXPORT void JNICALL
Java_uk_lgl_modmenu_Preferences_Changes(JNIEnv *env, jclass clazz, jobject obj,
                                        jint featNum, jstring featName, jint value,
                                        jboolean boolean, jstring str) {

    LOGD(OBFUSCATE("Feature name: %d - %s | Value: = %d | Bool: = %d | Text: = %s"), featNum,
         env->GetStringUTFChars(featName, 0), value,
         boolean, str != NULL ? env->GetStringUTFChars(str, 0) : "");

    //BE CAREFUL NOT TO ACCIDENTLY REMOVE break;

    switch (featNum) {
        case 0:
            ESP = boolean;
            break;
        case 1:
            ESPLine = boolean;
            break;
    }
}

JNIEXPORT void JNICALL
Java_uk_lgl_modmenu_FloatingModMenuService_DrawOn(JNIEnv *env, jclass type, jobject espView, jobject canvas) {
    ESPOverlay esp = ESPOverlay(env, espView, canvas);
    if (esp.isValid()) {
        if (ESP) {
            if (espManager->enemies->empty()) {
		        return;
	        }
            SetResolution(esp.width(), esp.height(), true);
            for (int i = 0; i < espManager->enemies->size(); i++) {
                void *obj = (*espManager->enemies)[i]->object;
                Vector3 objPos = WorldToScreenPoint(get_MainCamera(NULL), get_position(get_transform(obj)));
				if (objPos.z < 1) continue;
                if (ESPLine) {
                    esp.drawLine(Color::White(), 1, Vector2(esp.width() / 2, esp.height()), Vector2(esp.width() - (esp.width() - objPos.x), esp.height() - objPos.y));
                }
            }
        }
    }
}
}

//No need to use JNI_OnLoad, since we don't use JNIEnv
//We do this to hide OnLoad from disassembler
__attribute__((constructor))
void lib_main() {
    // Create a new thread so it does not block the main thread, means the game would not freeze
    pthread_t ptid;
    pthread_create(&ptid, NULL, hack_thread, NULL);
}

/*
JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM *vm, void *reserved) {
    JNIEnv *globalEnv;
    vm->GetEnv((void **) &globalEnv, JNI_VERSION_1_6);
    return JNI_VERSION_1_6;
}
 */

// Registering the app with SAM, which is what a native webOS app has to do
// before the system treats it as running.
//
// libcbe does not do this for you: it opens its own Luna connections but never
// registers the app, and on webOS 3 nothing else will do it either - WAM's
// binary registers *itself* as com.palm.webappmanager before handing over to
// WebOSMain. SDL does it in SDL_webOSRegisterApp(), and this is the same call
// with the same library, minus SDL.
//
// dlopen rather than a link stub: libhelpers is one function here, and keeping
// it out of the ELF's NEEDED list means `-verify` has nothing new to check.

#include "luna_register.h"

#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

// From webosbrew's libhelpers, via SDL-webOS's SDL_webos_helpers_sym.h.
typedef struct LSHandle LSHandle;
typedef struct LSMessage LSMessage;
typedef struct HContext HContext;
typedef int (*HLSFilterFunc)(LSHandle *sh, LSMessage *reply, HContext *ctx);

struct HContext {
    HLSFilterFunc callback;
    void *userdata;
    void *unknown;
    int multiple;   /* subscription rather than one-shot */
    int pub;        /* public or private bus */
    unsigned long ret_token;
};

static int (*HLunaServiceCall)(const char *uri, const char *payload, HContext *context);
static const char *(*HLunaServiceMessage)(LSMessage *message);

static HContext g_lifecycle;

static int LifecycleCb(LSHandle *sh, LSMessage *reply, HContext *ctx)
{
    (void)sh;
    (void)ctx;
    if (HLunaServiceMessage != NULL) {
        printf("[luna] lifecycle: %s\n", HLunaServiceMessage(reply));
    }
    return 1;
}

int luna_register_app(const char *app_id)
{
    char payload[256];
    void *lib;
    int rc;

    lib = dlopen("libhelpers.so.2", RTLD_LAZY);
    if (lib == NULL) {
        printf("[luna] dlopen libhelpers.so.2: %s\n", dlerror());
        return -1;
    }
    HLunaServiceCall = dlsym(lib, "HLunaServiceCall");
    HLunaServiceMessage = dlsym(lib, "HLunaServiceMessage");
    if (HLunaServiceCall == NULL) {
        printf("[luna] no HLunaServiceCall in libhelpers\n");
        return -1;
    }

    snprintf(payload, sizeof(payload), "{\"id\":\"%s\"}", app_id);

    memset(&g_lifecycle, 0, sizeof(g_lifecycle));
    g_lifecycle.callback = LifecycleCb;
    g_lifecycle.multiple = 1;   /* stays subscribed for relaunch/close events */
    g_lifecycle.pub = 1;

    // nativeLifeCycleInterfaceVersion 1 is registerNativeApp; version 2 would be
    // registerApp. The sample's appinfo declares neither, which means 1.
    rc = HLunaServiceCall("luna://com.webos.applicationManager/registerNativeApp",
                          payload, &g_lifecycle);
    printf("[luna] registerNativeApp(%s) -> %d\n", app_id, rc);
    return rc;
}

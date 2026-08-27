#ifndef WEB_CBE_WEBOS3_LUNA_REGISTER_H_
#define WEB_CBE_WEBOS3_LUNA_REGISTER_H_

#ifdef __cplusplus
extern "C" {
#endif

// Registers this process with SAM as the running instance of app_id. Returns 0
// on success. See luna_register.c for why libcbe does not do this itself.
int luna_register_app(const char *app_id);

#ifdef __cplusplus
}
#endif

#endif  // WEB_CBE_WEBOS3_LUNA_REGISTER_H_

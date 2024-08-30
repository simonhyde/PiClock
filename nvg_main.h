#ifndef __NVG_INIT_HEADER
#define __NVG_INIT_HEADER
#ifdef __cplusplus
extern "C" {
#endif
int nvg_main(void(*drawFunc)(NVGcontext*,int,int), void(*nvgInit)(NVGcontext*), int, int);
#ifdef __cplusplus
}
#endif
#endif

#ifndef __CSTUFF_H
#define __CSTUFF_H

#include "m_fixed.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int M_CheckParm(const char *check);

int playeringameGet(int plr);
void playeringameSet(int plr, int val);

void x_setLinedefCheck(int linedef);
void x_setSectorCheck(int sector);
void x_setThingCheck(int index);

int x_getRngIndex(void);
void x_setAngleType(int type);
unsigned int x_getAngle(short type);
double x_getXPos(void);
double x_getYPos(void);
double x_getZPos(void);
double x_getPrevXPos(void);
double x_getPrevYPos(void);
double x_getPrevZPos(void);
double x_getXMom(void);
double x_getYMom(void);

int x_GetCompatibility(void);
void x_SetCompatibility(int compatflags);
void x_getOptions(int options[]);
void x_changeOptions(int options[]);
void x_initHeader(unsigned char* demo_p);
int x_initSize(void);

void x_clearMapSavepoints(void);
void x_clearUserSavepoint(void);
void x_clearAllSavepoints(void);
int x_getSavepointTic(int dest);
int x_setSavepoint(int isUserSave);
int x_loadSavepoint(int dest);
void x_shutdown(void);
void x_initLongticsDefault(void);

#ifdef __cplusplus
}
#endif

#endif /* __CSTUFF_H */
#include "3dc.h"
#include "module.h"
#include "stratdef.h"
#include "dynblock.h"
#include "gamedef.h"
#include "bh_types.h"
#include "acc_snap.h"
#include "acc_route.h"
#include "acc_speech.h"
#include <math.h>

extern DISPLAYBLOCK *Player;
extern VIEWDESCRIPTORBLOCK *Global_VDB_Ptr;

void AccSnap_FaceYaw(int yaw)
{
    STRATEGYBLOCK *s=Player?Player->ObStrategyBlock:NULL;
    PLAYER_STATUS *p=s?s->SBdataptr:NULL;
    DYNAMICSBLOCK *d=s?s->DynPtr:NULL;
    if(!p || !d || !p->IsAlive || p->DemoMode || AvP.LevelCompleted ||
       (AvP.PlayerType!=I_Marine && AvP.PlayerType!=I_Predator) || AvP.Network!=I_No_Network) return;
    d->OrientEuler.EulerY=yaw&4095;
    CreateEulerMatrix(&d->OrientEuler,&d->OrientMat);
    TransposeMatrixCH(&d->OrientMat);
    d->PrevOrientEuler=d->OrientEuler; d->PrevOrientMat=d->OrientMat;
    d->AngVelocity.EulerY=0;
    p->TurnInertia=0;
    p->Mvt_TurnIncrement=0;
    p->Mvt_InputRequests.Flags.Rqst_TurnLeft=p->Mvt_InputRequests.Flags.Rqst_TurnRight=0;
}

void AccSnap_Request(unsigned int nowMs)
{
    STRATEGYBLOCK *s=Player?Player->ObStrategyBlock:NULL;
    PLAYER_STATUS *p=s?s->SBdataptr:NULL;
    DYNAMICSBLOCK *d=s?s->DynPtr:NULL;
    VECTORCH target;
    double dx,dy,dz,horizontal;
    int aim=0,yaw,pitch=0;
    if(!p || !d || !Global_VDB_Ptr || !p->IsAlive || p->DemoMode || AvP.LevelCompleted ||
       (AvP.PlayerType!=I_Marine && AvP.PlayerType!=I_Predator) || AvP.Network!=I_No_Network) return;
    if(!AccRoute_GetSnapTarget(nowMs,&target,&aim)) {
        AccSpeech_Say("No guidance target to face.",0); return;
    }
    dx=(double)target.vx-Global_VDB_Ptr->VDB_World.vx;
    dy=(double)target.vy-Global_VDB_Ptr->VDB_World.vy;
    dz=(double)target.vz-Global_VDB_Ptr->VDB_World.vz;
    horizontal=hypot(dx,dz);
    if(horizontal<1 && (!aim || fabs(dy)<1)) {
        AccSpeech_Say("Already at the guidance point.",0); return;
    }
    yaw=horizontal<1?d->OrientEuler.EulerY:(int)lround(atan2(dx,dz)*4096.0/6.283185307179586);
    if(aim) pitch=(int)lround(atan2(dy,horizontal)*4096.0/6.283185307179586);
    /* Marine look limits from PlayerPanning: 128..1920 after adding 1024. */
    if(pitch>896) pitch=896;
    if(pitch< -896) pitch= -896;
    AccSnap_FaceYaw(yaw);
    p->ViewPanX=pitch&4095;
    /* Suppress competing look input for this frame only. Retain walk/fire. */
    p->Mvt_TurnIncrement=p->Mvt_PitchIncrement=0;
    p->Mvt_InputRequests.Flags.Rqst_TurnLeft=p->Mvt_InputRequests.Flags.Rqst_TurnRight=0;
    p->Mvt_InputRequests.Flags.Rqst_LookUp=1;
    p->Mvt_InputRequests.Flags.Rqst_LookDown=p->Mvt_InputRequests.Flags.Rqst_CentreView=0;
    AccSpeech_Say("Facing target.",0);
}

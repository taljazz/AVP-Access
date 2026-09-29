#include "3dc.h"
#include "module.h"
#include "stratdef.h"
#include "dynblock.h"
#include "gamedef.h"
#include "bh_types.h"
#include "acc_snap.h"
#include <stdio.h>
#include <string.h>
DISPLAYBLOCK *Player;
VIEWDESCRIPTORBLOCK *Global_VDB_Ptr;
AVP_GAME_DESC AvP;
static DISPLAYBLOCK display;
static VIEWDESCRIPTORBLOCK view;
static STRATEGYBLOCK strategy;
static PLAYER_STATUS status;
static DYNAMICSBLOCK dynamics;
static VECTORCH target;
static int available=1,aim,queries,matrices,transposes,checks,failures;
static char speech[128];
int AccRoute_GetSnapTarget(unsigned int now,VECTORCH *point,int *pitch)
{ (void)now; ++queries; *point=target; *pitch=aim; return available; }
void AccSpeech_Say(const char *s,int i) { (void)i; snprintf(speech,sizeof(speech),"%s",s); }
void CreateEulerMatrix(EULER *e,MATRIXCH *m) { ++matrices; m->mat11=e->EulerY; }
void TransposeMatrixCH(MATRIXCH *m) { (void)m; ++transposes; }
static void check(int ok,const char *label) { ++checks; if(!ok) { ++failures; printf("FAIL %s\n",label); } }
int main(void)
{
 int before;
 Player=&display; Global_VDB_Ptr=&view; display.ObStrategyBlock=&strategy;
 strategy.DynPtr=&dynamics; strategy.SBdataptr=&status; status.IsAlive=1;
 AvP.PlayerType=I_Marine; AvP.Network=I_No_Network;
 target=(VECTORCH){1000,0,0}; AccSnap_Request(0);
 check(dynamics.OrientEuler.EulerY==1024,"east yaw");
 target=(VECTORCH){-1000,0,0}; AccSnap_Request(0); check(dynamics.OrientEuler.EulerY==3072,"west wraps");
 target=(VECTORCH){0,0,-1000}; AccSnap_Request(0); check(dynamics.OrientEuler.EulerY==2048,"behind yaw");
 target=(VECTORCH){0,-1000,1000}; aim=1; AccSnap_Request(0); check(status.ViewPanX==3584,"elevated target looks up");
 target.vy=1000; AccSnap_Request(0); check(status.ViewPanX==512,"lower target looks down");
 target=(VECTORCH){0,-10000,0}; AccSnap_Request(0); check(status.ViewPanX==3200,"vertical up clamped to Marine limit");
 target.vy=10000; AccSnap_Request(0); check(status.ViewPanX==896,"vertical down clamped");
 target=(VECTORCH){1000,50000,1000}; aim=0; AccSnap_Request(0); check(status.ViewPanX==0,"navigation ignores floor elevation and levels view");
 check(dynamics.PrevOrientEuler.EulerY==dynamics.OrientEuler.EulerY && dynamics.PrevOrientMat.mat11==dynamics.OrientMat.mat11 && matrices==transposes,"both orientation representations and previous state synchronized");
 dynamics.Position=(VECTORCH){12,34,56}; status.Mvt_MotionIncrement=123; status.Mvt_InputRequests.Flags.Rqst_FirePrimaryWeapon=0;
 status.TurnInertia=900; status.Mvt_TurnIncrement=800; status.Mvt_PitchIncrement=700; AccSnap_Request(0);
 check(dynamics.Position.vx==12 && dynamics.Position.vy==34 && dynamics.Position.vz==56 && status.Mvt_MotionIncrement==123 && !status.Mvt_InputRequests.Flags.Rqst_FirePrimaryWeapon,"snap does not move or fire");
 check(!status.TurnInertia && !status.Mvt_TurnIncrement && !status.Mvt_PitchIncrement && status.Mvt_InputRequests.Flags.Rqst_LookUp,"old turn inertia/input and automatic centering cannot undo snap this frame");
 before=matrices; available=0; AccSnap_Request(0); check(matrices==before && strstr(speech,"No guidance"),"missing target leaves facing unchanged");
 available=1; target=(VECTORCH){0,0,0}; AccSnap_Request(0); check(matrices==before,"coincident navigation point does not rotate arbitrarily");
 before=queries; status.IsAlive=0; AccSnap_Request(0); check(queries==before,"dead player blocked");
 status.IsAlive=1; status.DemoMode=1; AccSnap_Request(0); check(queries==before,"demo blocked");
 status.DemoMode=0; AvP.PlayerType=I_Alien; AccSnap_Request(0); check(queries==before,"unsupported species blocked");
 AvP.PlayerType=I_Marine; AvP.Network=1; AccSnap_Request(0); check(queries==before,"multiplayer blocked");
 AvP.Network=I_No_Network; AvP.PlayerType=I_Predator;
 dynamics.Position=(VECTORCH){101,202,303}; dynamics.OrientEuler.EulerY=17; status.TurnInertia=44;
 status.Mvt_TurnIncrement=55; status.Mvt_InputRequests.Flags.Rqst_TurnLeft=1;
 before=matrices; AccSnap_FaceYaw(3072);
 check(dynamics.OrientEuler.EulerY==3072 && dynamics.PrevOrientEuler.EulerY==3072 && matrices==before+1 && transposes==matrices,
       "explicit assist yaw updates the normal orientation and previous orientation matrices");
 check(dynamics.Position.vx==101 && dynamics.Position.vy==202 && dynamics.Position.vz==303 &&
       !status.TurnInertia && !status.Mvt_TurnIncrement && !status.Mvt_InputRequests.Flags.Rqst_TurnLeft,
       "assist yaw does not move the player and suppresses only conflicting turn input");
 status.IsAlive=0; before=matrices; AccSnap_FaceYaw(1024);
 check(matrices==before,"assist yaw is blocked when the player is not alive");
 printf("snap: %d checks, %d failed\n",checks,failures); return failures!=0;
}

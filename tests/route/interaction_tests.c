#include "3dc.h"
#include "module.h"
#include "stratdef.h"
#include "gamedef.h"
#include "triggers.h"
#include "pvisible.h"
#include <stdio.h>
#include <string.h>
int NumOnScreenBlocks;
int NumActiveStBlocks;
STRATEGYBLOCK *ActiveStBlockList[maxstblocks];
DISPLAYBLOCK *OnScreenBlockList[4], *Player;
DISPLAYBLOCK *LOS_ObjectHitPtr;
static INANIMATEOBJECT_STATUSBLOCK scenery;
static STRATEGYBLOCK *lastRequest;
static DISPLAYBLOCK objects[3];
static STRATEGYBLOCK strategies[3];
static int visible=1, calls, requests, failures, checks;
AVP_GAME_DESC AvP;
int IsThisObjectVisibleFromThisPosition_WithIgnore(DISPLAYBLOCK *a,DISPLAYBLOCK *b,VECTORCH *p,int range)
{ (void)a;(void)b;(void)p;(void)range; ++calls;return visible; }
void RequestState(STRATEGYBLOCK *sb,int message,STRATEGYBLOCK *sender)
{ lastRequest=sb;(void)sender;if(message==1)++requests; }
void AddNetMsg_LOSRequestBinarySwitch(STRATEGYBLOCK *sb) { (void)sb; }
static void check(int ok,const char *name)
{++checks;if(!ok){++failures;printf("FAIL: %s\n",name);}}
int main(void) {
 int i;
 for(i=0;i<3;i++) {objects[i].ObStrategyBlock=&strategies[i];strategies[i].I_SBtype=I_BehaviourBinarySwitch;OnScreenBlockList[i]=&objects[i];objects[i].ObView.vz=2000;}
 NumOnScreenBlocks=1;
 check(GetOperableObjectInLineOfSight()==&objects[0] && requests==0,"read-only selector does not activate");
 objects[0].ObView.vz=3000; check(!GetOperableObjectInLineOfSight(),"exact far boundary rejected");
 objects[0].ObView.vz=0; check(!GetOperableObjectInLineOfSight(),"behind/on camera rejected");
 objects[0].ObView.vz=2000;objects[0].ObView.vx=1000;
 check(!GetOperableObjectInLineOfSight(),"horizontal alignment boundary");
 objects[0].ObView.vx=0;objects[0].ObView.vy=-1000;
 check(!GetOperableObjectInLineOfSight(),"vertical alignment boundary");
 objects[0].ObView.vy=0;visible=0; check(!GetOperableObjectInLineOfSight(),"occluded switch rejected");
 visible=1;NumOnScreenBlocks=2;objects[0].ObView.vx=500;
 check(GetOperableObjectInLineOfSight()==&objects[1],"more centered competing control wins");
 AvP.Network=I_No_Network;OperateObjectInLineOfSight();
 check(requests==1 && lastRequest==&strategies[1],"operate activates the same selected control once");
 visible=0;OperateObjectInLineOfSight();check(requests==1,"blocked operate activates nothing");
 NumOnScreenBlocks=0;check(!GetOperableObjectInLineOfSight(),"empty display list");
 Player=&objects[2];visible=1;
 check(GetInteractionObstruction(&objects[0])==0,"clear interaction line");
 visible=0;LOS_ObjectHitPtr=NULL;
 check(GetInteractionObstruction(&objects[0])==1,"world geometry is not called breakable");
 LOS_ObjectHitPtr=&objects[1];strategies[1].I_SBtype=I_BehaviourInanimateObject;
 strategies[1].SBdataptr=&scenery;scenery.typeId=IOT_Static;
 check(GetInteractionObstruction(&objects[0])==2,"breakable non-explosive scenery");
 scenery.Indestructable=1;
 check(GetInteractionObstruction(&objects[0])==1,"indestructible cover");
 scenery.Indestructable=0;scenery.explosionType=1;
 check(GetInteractionObstruction(&objects[0])==1,"explosive scenery never gets shoot instruction");
 scenery.explosionType=0;scenery.typeId=IOT_Ammo;
 check(GetInteractionObstruction(&objects[0])==1,"pickup is not breakable cover");
 printf("interaction: %d checks, %d failed\n",checks,failures);return failures!=0;
}

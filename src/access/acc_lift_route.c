#include "3dc.h"
#include "module.h"
#include "stratdef.h"
#include "dynblock.h"
#include "bh_types.h"
#include "gamedef.h"
#include "bh_plift.h"
#include "acc_lift_route.h"
#include <math.h>
#include <stdlib.h>
#include <ctype.h>
extern DISPLAYBLOCK *Player;
extern STRATEGYBLOCK *ActiveStBlockList[];
extern int NumActiveStBlocks;
extern char LevelName[];
static STRATEGYBLOCK *HeldLift;
static int HeldHeight;
static int FallExitActive;
static int Near(int a,int b,int tolerance);
static int IsFallPlatformSignature(const STRATEGYBLOCK *s,
                                  const PLATFORMLIFT_BEHAVIOUR_BLOCK *l);
static int InFallUpperExitCorridor(const VECTORCH *feet);
static int InFallUpperExitRegion(const VECTORCH *feet);
static int IsFallHandoffPoint(const VECTORCH *feet,const DYNAMICSBLOCK *playerDyn);
static int IsPredatorPlayer(void)
{
    return AvP.PlayerType==I_Predator;
}
static int FinalShaftStepOffArea(const STRATEGYBLOCK *s,
                                 const PLATFORMLIFT_BEHAVIOUR_BLOCK *l,
                                 const VECTORCH *feet,int heldHeight);
void AccLiftRoute_Reset(void) { HeldLift=NULL; FallExitActive=0; }
int AccLiftRoute_HoldAtLanding(STRATEGYBLOCK *lift)
{
    COLLISIONREPORT *c; VECTORCH *feet;
    if(!lift || lift!=HeldLift || lift->SBflags.please_destroy_me || !lift->DynPtr ||
       abs(lift->DynPtr->Position.vy-HeldHeight)>=200 || !Player ||
       !Player->ObStrategyBlock || !Player->ObStrategyBlock->DynPtr) return 0;
    feet=&Player->ObStrategyBlock->DynPtr->Position;
    for(c=Player->ObStrategyBlock->DynPtr->CollisionReportPtr;c;c=c->NextCollisionReportPtr)
        if(c->ObstacleSBPtr==lift && c->ObstacleNormal.vy<-40000) return 1;
    if(FinalShaftStepOffArea(lift,lift->SBdataptr,feet,HeldHeight)) return 1;
    return FallExitActive && IsFallPlatformSignature(lift,lift->SBdataptr) &&
           Near(HeldHeight,-2076,250) && Near(lift->DynPtr->Position.vy,HeldHeight,200) &&
           InFallUpperExitRegion(feet);
}
static int Near(int a,int b,int tolerance) { return abs(a-b)<=tolerance; }
static int DerelictLevel(void)
{
    const unsigned char *p=(const unsigned char *)LevelName;
    const char expected[]="derelict"; int i;
    if(!p) return 0;
    for(i=0;expected[i];++i) if(tolower(p[i])!=expected[i]) return 0;
    return p[i]==0;
}
static int FallLevel(void)
{
    const unsigned char *p=(const unsigned char *)LevelName;
    return p && tolower(p[0])=='f' && tolower(p[1])=='a' &&
           tolower(p[2])=='l' && tolower(p[3])=='l' && p[4]==0;
}
int AccLiftRoute_IsFallShaftRoom(AIMODULE *room)
{
    return IsPredatorPlayer() && FallLevel() && room &&
           (room->m_index==85 || room->m_index==87 || room->m_index==89 ||
            room->m_index==91 || room->m_index==92);
}
static int IsFallPlatformSignature(const STRATEGYBLOCK *s,
                                   const PLATFORMLIFT_BEHAVIOUR_BLOCK *l)
{
    return IsPredatorPlayer() && FallLevel() && s && l && s->DynPtr &&
           Near(s->DynPtr->Position.vx,187235,250) &&
           Near(s->DynPtr->Position.vz,70510,250) &&
           Near(l->upHeight,-2076,250) && Near(l->downHeight,25226,250);
}
STRATEGYBLOCK *AccLiftRoute_FindFallPlatform(AIMODULE *room)
{
    int i,matches=0;
    STRATEGYBLOCK *found=NULL;
    if(!AccLiftRoute_IsFallShaftRoom(room)) return NULL;
    for(i=0;i<NumActiveStBlocks;++i) {
        STRATEGYBLOCK *s=ActiveStBlockList[i];
        if(s && s->I_SBtype==I_BehaviourPlatform &&
           !s->SBflags.please_destroy_me && s->SBdataptr &&
           IsFallPlatformSignature(s,s->SBdataptr)) {
            found=s;
            ++matches;
        }
    }
    return matches==1?found:NULL;
}
static int HasPlatformContact(STRATEGYBLOCK *lift)
{
    COLLISIONREPORT *c;
    if(!lift || !Player || !Player->ObStrategyBlock || !Player->ObStrategyBlock->DynPtr) return 0;
    for(c=Player->ObStrategyBlock->DynPtr->CollisionReportPtr;c;c=c->NextCollisionReportPtr)
        if(c->ObstacleSBPtr==lift && c->ObstacleNormal.vy<-40000) return 1;
    return 0;
}
int AccLiftRoute_IsFallPlatformContact(AIMODULE *room,const VECTORCH *feet)
{
    STRATEGYBLOCK *s;
    if(!feet || !AccLiftRoute_IsFallShaftRoom(room)) return 0;
    s=AccLiftRoute_FindFallPlatform(room);
    return HasPlatformContact(s);
}
static int InFallUpperExitCorridor(const VECTORCH *feet)
{
    return feet && Near(feet->vy,845,450) && feet->vx>=187000 &&
           feet->vx<=189000 && feet->vz>=69500 && feet->vz<=73500;
}
static int InFallUpperExitRegion(const VECTORCH *feet)
{
    return feet && Near(feet->vy,845,450) && feet->vx>=180000 &&
           feet->vx<=189000 && feet->vz>=69500 && feet->vz<=73500;
}
static int IsFallHandoffPoint(const VECTORCH *feet,const DYNAMICSBLOCK *playerDyn)
{
    return feet && playerDyn && playerDyn->IsInContactWithFloor &&
           Near(feet->vy,845,450) && feet->vx>=178000 && feet->vx<=183100 &&
           feet->vz>=71000 && feet->vz<=73500;
}
int AccLiftRoute_GetFallExitPoint(AIMODULE *room,const VECTORCH *feet,
                                  const VECTORCH *target,VECTORCH *point)
{
    STRATEGYBLOCK *s; PLATFORMLIFT_BEHAVIOUR_BLOCK *l;
    DYNAMICSBLOCK *playerDyn;
    if(!AccLiftRoute_IsFallShaftRoom(room) || !feet || !target || !point ||
       !Player || !Player->ObStrategyBlock || !Player->ObStrategyBlock->DynPtr) return 0;
    s=AccLiftRoute_FindFallPlatform(room);
    if(!s) return 0;
    l=s->SBdataptr;
    playerDyn=Player->ObStrategyBlock->DynPtr;
    if(IsFallHandoffPoint(feet,playerDyn)) return 0;
    if(FallExitActive && !InFallUpperExitRegion(feet)) {
        FallExitActive=0;
        if(HeldLift && IsFallPlatformSignature(HeldLift,HeldLift->SBdataptr)) {
            HeldLift=NULL;
            HeldHeight=0;
        }
        return 0;
    }
    if(HasPlatformContact(s) && Near(s->DynPtr->Position.vy,l->upHeight,250) &&
       Near(feet->vy,l->upHeight+2922,500)) {
        FallExitActive=1;
        HeldLift=s;
        HeldHeight=l->upHeight;
    }
    /* A narrowly bounded grounded upper-floor checkpoint is enough to resume
       the exit itinerary after a route reset/save load; arbitrary room92 floors
       cannot reacquire the shaft because both platform signature and pose match. */
    if(!FallExitActive && InFallUpperExitCorridor(feet) &&
       Near(s->DynPtr->Position.vy,l->upHeight,250) &&
       playerDyn->IsInContactWithFloor) FallExitActive=1;
    if(!FallExitActive) return 0;
    if(feet->vz<71600) {
        point->vx=188123; point->vy=feet->vy; point->vz=72000;
        return 1;
    }
    point->vx=182800; point->vy=feet->vy; point->vz=72000;
    return 2;
}
int AccLiftRoute_CompleteFallExit(AIMODULE *room,const VECTORCH *feet)
{
    DYNAMICSBLOCK *playerDyn;
    STRATEGYBLOCK *lift;
    if(!AccLiftRoute_IsFallShaftRoom(room) || !feet ||
       !Player || !Player->ObStrategyBlock || !Player->ObStrategyBlock->DynPtr) return 0;
    playerDyn=Player->ObStrategyBlock->DynPtr;
    if(!IsFallHandoffPoint(feet,playerDyn)) return 0;
    lift=AccLiftRoute_FindFallPlatform(room);
    if(!lift) return 0;
    FallExitActive=0;
    if(HeldLift==lift && IsFallPlatformSignature(HeldLift,HeldLift->SBdataptr)) {
        HeldLift=NULL;
        HeldHeight=0;
    }
    return 1;
}
static int KnownDerelictShaft(const AIMODULE *room,const STRATEGYBLOCK *s,
                              const PLATFORMLIFT_BEHAVIOUR_BLOCK *l)
{
    int x=s->DynPtr->Position.vx,z=s->DynPtr->Position.vz;
    if(!DerelictLevel()) return 0;
    if((room->m_index==75 || room->m_index==76 || room->m_index==158) &&
       Near(x,-28055,250) && Near(z,-192793,250) &&
       Near(l->upHeight,1818,250) && Near(l->downHeight,30921,250)) return 1;
    if((room->m_index==171 || room->m_index==159) &&
       Near(x,-116427,250) && Near(z,-171227,250) &&
       Near(l->upHeight,29650,250) && Near(l->downHeight,45229,250)) return 1;
    return 0;
}
static int KnownFallShaft(const AIMODULE *room,const STRATEGYBLOCK *s,
                          const PLATFORMLIFT_BEHAVIOUR_BLOCK *l)
{
    return room && AccLiftRoute_IsFallShaftRoom((AIMODULE *)room) &&
           IsFallPlatformSignature(s,l) &&
           AccLiftRoute_FindFallPlatform((AIMODULE *)room)==s;
}
static int FinalShaftStepOffArea(const STRATEGYBLOCK *s,
                                 const PLATFORMLIFT_BEHAVIOUR_BLOCK *l,
                                 const VECTORCH *feet,int heldHeight)
{
    const VECTORCH *p;
    if(!s || !s->DynPtr || !l || !feet || !DerelictLevel()) return 0;
    p=&s->DynPtr->Position;
    return Near(p->vx,-116427,250) && Near(p->vz,-171227,250) &&
           Near(l->upHeight,29650,250) && Near(l->downHeight,45229,250) &&
           Near(heldHeight,l->downHeight,200) &&
           abs(p->vy-heldHeight)<200 &&
           abs(feet->vy-(l->downHeight+1945))<=500 &&
           abs(feet->vx-p->vx)<2700 && abs(feet->vz-p->vz)<2400;
}
static int HasLiftFloorContact(STRATEGYBLOCK *lift)
{
    COLLISIONREPORT *c;
    for(c=Player->ObStrategyBlock->DynPtr->CollisionReportPtr;c;c=c->NextCollisionReportPtr)
        if(c->ObstacleSBPtr==lift && c->ObstacleNormal.vy<-40000) return 1;
    return 0;
}
int AccLiftRoute_Find(AIMODULE *room,const VECTORCH *feet,const VECTORCH *target,VECTORCH *point)
{
    int i,result=0; double best=1e30;
    if(!room || !feet || !target || !point || !Player || !Player->ObStrategyBlock || !Player->ObStrategyBlock->DynPtr) return 0;
    if(HeldLift) {
        int active=0;
        for(i=0;i<NumActiveStBlocks;++i)
            if(ActiveStBlockList[i]==HeldLift) { active=1; break; }
        if(!active || HeldLift->SBflags.please_destroy_me || !HeldLift->DynPtr || !HeldLift->SBdataptr)
            HeldLift=NULL;
    }
    for(i=0;i<NumActiveStBlocks;++i) {
        STRATEGYBLOCK *s=ActiveStBlockList[i];
        PLATFORMLIFT_BEHAVIOUR_BLOCK *l;
        int aboard,destination,boarding,known,knownFall,floorOffset; double dx,dz,distance;
        if(!s || s->I_SBtype!=I_BehaviourPlatform || s->SBflags.please_destroy_me ||
           !s->DynPtr || !s->SBdataptr || !s->containingModule) continue;
        l=s->SBdataptr;
        if(IsFallPlatformSignature(s,l) && AccLiftRoute_IsFallShaftRoom(room) &&
           AccLiftRoute_FindFallPlatform(room)!=s) continue;
        if(s==HeldLift && !l->Enabled && !IsFallPlatformSignature(s,l)) HeldLift=NULL;
        if(l->downHeight-l->upHeight<1800) continue;
        /* Collision identity survives AI-room changes while riding a platform.
           Keep the final lower-landing hold through the short step-off gap. */
        aboard=HasLiftFloorContact(s);
        knownFall=KnownFallShaft(room,s,l);
        known=KnownDerelictShaft(room,s,l) || knownFall;
        destination=abs(target->vy-l->upHeight)<abs(target->vy-l->downHeight)?l->upHeight:l->downHeight;
        if(s==HeldLift) {
            if(destination!=HeldHeight) HeldLift=NULL;
            else if(!aboard && FinalShaftStepOffArea(s,l,feet,HeldHeight)) {
                aboard=1;
            } else if(!aboard && FallExitActive && IsFallPlatformSignature(s,l) &&
                      Near(HeldHeight,-2076,250) && InFallUpperExitRegion(feet)) {
                aboard=1;
            } else if(!aboard) HeldLift=NULL;
        }
        if(!aboard && s->containingModule->m_aimodule!=room && !known) continue;
        /* Switch height is above the landing. Don't mistake a distant floor
           for a terminal merely because it is nearer than the other one. */
        if(abs(target->vy-destination)>2500) continue;
        if(aboard) {
            /* An occupied automatic lift immediately starts its next activation
               delay. The landing remains usable during that delay. */
            if((l->state==PLBS_AtRest || l->state==PLBS_Activating) && abs(s->DynPtr->Position.vy-destination)<200) {
                HeldLift=s; HeldHeight=destination;
                *point=*target; return 3;
            }
            *point=s->DynPtr->Position; return 2;
        }
        if(!l->Enabled) continue;
        boarding=abs(feet->vy-l->upHeight)<abs(feet->vy-l->downHeight)?l->upHeight:l->downHeight;
        /* The surveyed final Derelict shaft's platform origin is about
           1945mm above the standing player's feet at both landings. Keep
           this offset limited to that exact level/shaft/room whitelist. */
        floorOffset=knownFall ? 2913 :
                    known && ((room->m_index==171 && boarding==l->upHeight) ||
                             (room->m_index==159 && boarding==l->downHeight)) ? 1945 : 0;
        if(knownFall && boarding==l->downHeight &&
           abs(s->DynPtr->Position.vy-l->downHeight)>400) {
            /* The exact Fall platform was only surveyed to support the east
               approach after it reaches the lower terminal. Stay at the
               player's current safe pose while it is elsewhere; never walk
               toward the empty shaft on an estimated boarding radius. */
            *point=*feet;
            return 2;
        }
        if(boarding==destination || (abs(feet->vy-boarding)>800 &&
                                   abs(feet->vy-(boarding+floorOffset))>200)) continue;
        dx=(double)feet->vx-s->DynPtr->Position.vx; dz=(double)feet->vz-s->DynPtr->Position.vz;
        distance=dx*dx+dz*dz;
        if(distance>=best) continue;
        best=distance; *point=s->DynPtr->Position;
        if(knownFall && boarding==l->downHeight) {
            point->vx=188123; point->vy=boarding+2913; point->vz=69715;
        } else point->vy=boarding-975;
        /* At the shaft, wait for the platform rather than walking into empty
           space. It calls itself automatically from the other terminal. */
        result=distance<(knownFall && boarding==l->downHeight?3000.0*3000.0:1200.0*1200) &&
               abs(s->DynPtr->Position.vy-boarding)>400?2:1;
    }
    return result;
}

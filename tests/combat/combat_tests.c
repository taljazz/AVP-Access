#include <stdio.h>
#include <string.h>
#include <math.h>
#include "3dc.h"
#include "prototyp.h"
#include "stratdef.h"
#include "gamedef.h"
#include "bh_marin.h"
#include "bh_pred.h"
#include "bh_fhug.h"
#include "bh_xeno.h"
#include "los.h"
#include "psnd.h"
#include "psndproj.h"
#include "acc_combat.h"

#define MAX_ACTORS 32
#define CHECK(x,n) do { ++checks; if (!(x)) { ++failures; printf("FAIL %s\n",n); } else printf("PASS %s\n",n); } while (0)

DISPLAYBLOCK *Player;
VIEWDESCRIPTORBLOCK *Global_VDB_Ptr;
VECTORCH GunMuzzleDirectionInWS;
int NumActiveStBlocks;
STRATEGYBLOCK *ActiveStBlockList[MAX_ACTORS];
AVP_GAME_DESC AvP;
VECTORCH LOS_Point, LOS_ObjectNormal;
int LOS_Lambda;
DISPLAYBLOCK *LOS_ObjectHitPtr;
SECTION_DATA *LOS_HModel_Section;
static int failures, checks, dead_calls, speech_count, cue_count, stop_count;
static int actorVisible[MAX_ACTORS], rayCalls, cueSound[32], cueAt[32], now;
static char speech[32][192];
static DISPLAYBLOCK playerDisplay, displays[MAX_ACTORS];
static STRATEGYBLOCK playersb, actors[MAX_ACTORS], world[MAX_ACTORS];
static MARINE_STATUS_BLOCK marines[MAX_ACTORS];
static PREDATOR_STATUS_BLOCK predators[MAX_ACTORS];
static FACEHUGGER_STATUS_BLOCK hugs[MAX_ACTORS];
static XENO_STATUS_BLOCK xenos[MAX_ACTORS];
static VIEWDESCRIPTORBLOCK view;

/* Use the engine's real weapon data layout for the Android flag. */
struct marine_weapon_data weapons[MAX_ACTORS];

int NPC_IsDead(STRATEGYBLOCK *s) { ++dead_calls; return s->integrity == -1; }
void GetTargetingPointOfObject(DISPLAYBLOCK *d, VECTORCH *p) { *p=d->ObWorld; }
void _RotateVector(VECTORCH *p, MATRIXCH *m)
{
    VECTORCH v=*p;
    p->vx=(int)(((long long)m->mat11*v.vx+(long long)m->mat21*v.vy+(long long)m->mat31*v.vz)/65536);
    p->vy=(int)(((long long)m->mat12*v.vx+(long long)m->mat22*v.vy+(long long)m->mat32*v.vz)/65536);
    p->vz=(int)(((long long)m->mat13*v.vx+(long long)m->mat23*v.vy+(long long)m->mat33*v.vz)/65536);
}
void FindPolygonInLineOfSight(VECTORCH *d, VECTORCH *o, int list, DISPLAYBLOCK *ignore)
{
    int i; double dx=d->vx/65536.0,dy=d->vy/65536.0,dz=d->vz/65536.0,best=1e30;
    (void)list; (void)ignore; ++rayCalls; LOS_ObjectHitPtr=NULL;
    /* Intersect a 50mm sphere per actor. This keeps each candidate's LOS
       independent and naturally chooses the first visible actor on a ray. */
    for(i=0;i<NumActiveStBlocks;i++) if(actorVisible[i]) {
        double x=displays[i].ObWorld.vx-o->vx, y=displays[i].ObWorld.vy-o->vy, z=displays[i].ObWorld.vz-o->vz;
        double dd=dx*dx+dy*dy+dz*dz;
        double along=x*dx+y*dy+z*dz, perpendicular=x*x+y*y+z*z-along*along/dd;
        if(along>0 && perpendicular<=2500.0 && along<best) { best=along; LOS_ObjectHitPtr=&displays[i]; }
    }
    LOS_Lambda=1234; LOS_Point.vx=91; LOS_Point.vy=92; LOS_Point.vz=93;
    LOS_ObjectNormal.vx=81; LOS_ObjectNormal.vy=82; LOS_ObjectNormal.vz=83;
    LOS_HModel_Section=(SECTION_DATA*)0x9876;
}
void AccSpeech_Say(const char *s,int interrupt) { (void)interrupt; if(speech_count<32) snprintf(speech[speech_count],192,"%s",s); ++speech_count; }
void AccBridge_BeginCue(const char *s,int id) { (void)s; (void)id; }
void AccBridge_EndCue(void) { }
void AccTracker_PlayContact(int id,const VECTORCH *p,int range,int *handle,int volume)
{ (void)p;(void)range;(void)volume; if(cue_count<32){cueSound[cue_count]=id;cueAt[cue_count]=now;} *handle=cue_count+1; ++cue_count; }
void Sound_Stop(int h) { (void)h; ++stop_count; }

static int check(const char *name,int want,int got) { CHECK(want==got,name); return want==got; }
static void reset(void)
{
    int i; AccCombat_Reset(); memset(&playerDisplay,0,sizeof playerDisplay); memset(&playersb,0,sizeof playersb);
    memset(displays,0,sizeof displays); memset(actors,0,sizeof actors); memset(world,0,sizeof world);
    memset(marines,0,sizeof marines); memset(predators,0,sizeof predators); memset(weapons,0,sizeof weapons); memset(hugs,0,sizeof hugs); memset(xenos,0,sizeof xenos);
    memset(&view,0,sizeof view); memset(&AvP,0,sizeof AvP); memset(ActiveStBlockList,0,sizeof ActiveStBlockList);
    memset(speech,0,sizeof speech); memset(cueSound,0,sizeof cueSound); memset(cueAt,0,sizeof cueAt);
    dead_calls=speech_count=cue_count=stop_count=rayCalls=0; memset(actorVisible,0,sizeof actorVisible); now=0;
    Player=&playerDisplay; playerDisplay.ObStrategyBlock=&playersb; playersb.SBdptr=&playerDisplay;
    Global_VDB_Ptr=&view; AvP.PlayerType=I_Marine; AvP.Network=I_No_Network; view.VDB_World=(VECTORCH){0,0,0};
    view.VDB_Mat.mat11=view.VDB_Mat.mat22=view.VDB_Mat.mat33=65536;
    GunMuzzleDirectionInWS=(VECTORCH){0,0,65536};
    for(i=0;i<MAX_ACTORS;i++) { actors[i].SBdptr=&displays[i]; displays[i].ObStrategyBlock=&actors[i]; actors[i].integrity=1; actors[i].SBDamageBlock.Health=100; actors[i].SBname[0]=(char)('A'+i); }
    NumActiveStBlocks=0;
}
static STRATEGYBLOCK *add(int type,int x,int y,int z)
{
    STRATEGYBLOCK *s=&actors[NumActiveStBlocks]; DISPLAYBLOCK *d=&displays[NumActiveStBlocks];
    s->I_SBtype=type; s->SBdptr=d; s->SBdataptr=&world[NumActiveStBlocks]; s->SBDamageBlock.Health=100;
    if(type==I_BehaviourMarine || type==I_BehaviourSeal) {
        s->SBdataptr=&marines[NumActiveStBlocks];
        marines[NumActiveStBlocks].My_Weapon=&weapons[NumActiveStBlocks];
    }
    if(type==I_BehaviourPredator) s->SBdataptr=&predators[NumActiveStBlocks];
    if(type==I_BehaviourFaceHugger) s->SBdataptr=&hugs[NumActiveStBlocks];
    if(type==I_BehaviourXenoborg) s->SBdataptr=&xenos[NumActiveStBlocks];
    d->ObStrategyBlock=s; d->ObWorld=(VECTORCH){x,y,z}; ActiveStBlockList[NumActiveStBlocks]=s; actorVisible[NumActiveStBlocks]=1; ++NumActiveStBlocks; return s;
}
static void visible(STRATEGYBLOCK *s) { int i; for(i=0;i<NumActiveStBlocks;i++) if(ActiveStBlockList[i]==s) actorVisible[i]=1; }

int main(void)
{
    STRATEGYBLOCK *s,*other; int before;
    setvbuf(stdout,NULL,_IONBF,0);
    reset();
    CHECK(AccCombat_HostileName(NULL)==NULL,"null excluded");
    s=add(I_BehaviourAlien,0,0,10000); CHECK(!strcmp(AccCombat_HostileName(s),"Alien"),"living alien included");
    s->integrity=-1; CHECK(AccCombat_HostileName(s)==NULL,"dead alien excluded");
    s=add(I_BehaviourPredator,0,0,10000); CHECK(!strcmp(AccCombat_HostileName(s),"Predator"),"living predator included");
    CHECK(!strcmp(AccCombat_HostileName(s),"Predator"),"Marine detects living NPC Predator regardless of AI target");
    s=add(I_BehaviourPredatorAlien,0,0,10000); CHECK(!strcmp(AccCombat_HostileName(s),"Predalien"),"predalien included");
    s=add(I_BehaviourQueenAlien,0,0,10000); CHECK(!strcmp(AccCombat_HostileName(s),"Queen alien"),"queen included");
    s=add(I_BehaviourFaceHugger,0,0,10000); ((FACEHUGGER_STATUS_BLOCK*)s->SBdataptr)->nearBehaviourState=FHNS_Dying; CHECK(AccCombat_HostileName(s)==NULL,"dying facehugger excluded");
    s=add(I_BehaviourXenoborg,0,0,10000); ((XENO_STATUS_BLOCK*)s->SBdataptr)->behaviourState=XS_Dying; CHECK(AccCombat_HostileName(s)==NULL,"dying xenoborg excluded");
    s=add(I_BehaviourMarine,0,0,10000); marines[NumActiveStBlocks-1].Target=&playersb; marines[NumActiveStBlocks-1].My_Weapon=(void*)&weapons[NumActiveStBlocks-1]; CHECK(!strcmp(AccCombat_HostileName(s),"Hostile soldier"),"Marine targeting player included");
    marines[NumActiveStBlocks-1].My_Weapon=NULL; CHECK(!strcmp(AccCombat_HostileName(s),"Hostile soldier"),"Marine target classification tolerates missing weapon data");
    marines[NumActiveStBlocks-1].Target=NULL; CHECK(AccCombat_HostileName(s)==NULL,"civilian Marine excluded");
    marines[NumActiveStBlocks-1].Target=&playersb; marines[NumActiveStBlocks-1].My_Weapon=&weapons[NumActiveStBlocks-1]; weapons[NumActiveStBlocks-1].Android=1; CHECK(!strcmp(AccCombat_HostileName(s),"Hostile android"),"hostile Android named");
    s=add(I_BehaviourSeal,0,0,10000); marines[NumActiveStBlocks-1].Target=&playersb; CHECK(!strcmp(AccCombat_HostileName(s),"Hostile soldier"),"Seal included only when targeting player");
    s=add(I_BehaviourMarine,0,0,10000); marines[NumActiveStBlocks-1].Target=&playersb; s->SBDamageBlock.Health=0; CHECK(AccCombat_HostileName(s)==NULL,"zero-health Marine excluded");
    reset(); AvP.PlayerType=I_Predator;
    s=add(I_BehaviourPredator,0,0,10000);
    CHECK(AccCombat_HostileName(s)==NULL,"Predator ignores allied NPC until it targets player");
    predators[0].Target=&playersb;
    CHECK(!strcmp(AccCombat_HostileName(s),"Hostile Predator"),"Predator recognizes an NPC Predator targeting player");
    s=add(I_BehaviourMarine,0,0,10000); weapons[1].id=MNPCW_PulseRifle;
    CHECK(!strcmp(AccCombat_HostileName(s),"Soldier"),"Predator can identify armed soldier before it targets player");
    s=add(I_BehaviourSeal,0,0,10000); weapons[2].id=MNPCW_Scientist_A;
    CHECK(AccCombat_HostileName(s)==NULL,"Predator excludes unarmed scientist");
    s=add(I_BehaviourMarine,0,0,10000); weapons[3].id=MNPCW_MUnarmed;
    CHECK(AccCombat_HostileName(s)==NULL,"Predator excludes unarmed civilian");
    reset(); s=add(I_BehaviourAlien,0,0,20000); visible(s); CHECK(AccCombat_Update(0)==1,"hostile at 20m accepted");
    reset(); s=add(I_BehaviourAlien,0,0,20001); visible(s); CHECK(AccCombat_Update(0)==0,"hostile beyond 20m rejected");
    reset(); s=add(I_BehaviourAlien,0,0,10000); actorVisible[0]=0; CHECK(AccCombat_Update(0)==0,"occluded candidate rejected");
    reset(); s=add(I_BehaviourAlien,0,0,11000); CHECK(AccCombat_Update(0)==1,"visible target acquires"); other=add(I_BehaviourPredator,1500,0,10000); CHECK(AccCombat_Update(100)==1 && strstr(speech[0],"Alien") && speech_count==1,"incumbent persists over closer second visible target");
    reset(); s=add(I_BehaviourAlien,0,0,15000); CHECK(AccCombat_Update(0)==1,"far target initially selected"); other=add(I_BehaviourPredator,3000,0,5000); CHECK(AccCombat_Update(100)==1 && speech_count==2 && strstr(speech[1],"Predator"),"substantially closer visible target takes over");
    reset(); s=add(I_BehaviourAlien,0,0,10000); visible(s); CHECK(AccCombat_Update(0)==1,"identity target acquired"); before=speech_count; CHECK(AccCombat_Update(50)==1 && speech_count==before,"100ms scan throttle");
    reset(); s=add(I_BehaviourAlien,1500,0,10000); CHECK(AccCombat_Update(0)==1 && strstr(speech[0],"Aim right")!=NULL,"right aim guidance");
    reset(); s=add(I_BehaviourAlien,-1500,0,10000); CHECK(AccCombat_Update(0)==1 && strstr(speech[0],"Aim left")!=NULL,"left aim guidance");
    reset(); s=add(I_BehaviourAlien,0,-4000,10000); CHECK(AccCombat_Update(0)==1 && strstr(speech[0],"Aim up")!=NULL,"up aim guidance");
    reset(); s=add(I_BehaviourAlien,0,4000,10000); CHECK(AccCombat_Update(0)==1 && strstr(speech[0],"Aim down")!=NULL,"down aim guidance");
    reset(); s=add(I_BehaviourAlien,0,0,10000); GunMuzzleDirectionInWS=(VECTORCH){0,1000,65536}; CHECK(AccCombat_Update(0)==1 && strstr(speech[0],"Adjust aim slightly")!=NULL,"centered target with gun ray miss does not claim on-target");
    reset(); s=add(I_BehaviourAlien,0,0,10000); GunMuzzleDirectionInWS=(VECTORCH){0,0,65536}; CHECK(AccCombat_Update(0)==1 && strstr(speech[0],"On target")!=NULL,"on target requires gun ray to intersect target");
    reset(); s=add(I_BehaviourAlien,10000,0,1500); view.VDB_Mat.mat11=view.VDB_Mat.mat33=0; view.VDB_Mat.mat13=65536; view.VDB_Mat.mat31=-65536; GunMuzzleDirectionInWS=(VECTORCH){65536,0,0}; CHECK(AccCombat_Update(0)==1 && strstr(speech[0],"Aim left")!=NULL,"quarter-turn camera makes world positive Z left of current facing");
    reset(); s=add(I_BehaviourAlien,0,-10000,0); CHECK(AccCombat_Update(0)==1 && strstr(speech[0],"Aim up")!=NULL,"directly overhead target never says turn zero degrees");
    reset(); s=add(I_BehaviourAlien,0,0,10000); CHECK(AccCombat_Update(0)==1,"lost-sight target acquired"); actorVisible[0]=0; CHECK(AccCombat_Update(100)==1,"lost sight grace holds control"); CHECK(AccCombat_Update(1200)==0,"1200ms lost sight resumes route"); visible(s); CHECK(AccCombat_Update(1300)==1,"visible target reacquires after grace");
    reset(); s=add(I_BehaviourAlien,0,0,10000); CHECK(AccCombat_Update(0)==1 && cue_count==1,"acquisition emits cue"); CHECK(AccCombat_Update(100)==1 && cue_count==1,"cue not repeated at 100ms"); CHECK(AccCombat_Update(200)==1 && cue_count==2,"aligned cue cadence 200ms");
    before=speech_count; AccCombat_Reset(); CHECK(stop_count>0 && speech_count==before,"reset stops cue silently");
    reset(); s=add(I_BehaviourAlien,0,0,10000); visible(s); LOS_Point=(VECTORCH){1,2,3}; LOS_ObjectNormal=(VECTORCH){4,5,6}; LOS_Lambda=777; LOS_ObjectHitPtr=&displays[7]; LOS_HModel_Section=(SECTION_DATA*)0x1234; AccCombat_Update(0); CHECK(LOS_Point.vx==1&&LOS_Point.vy==2&&LOS_Point.vz==3&&LOS_ObjectNormal.vx==4&&LOS_ObjectNormal.vy==5&&LOS_ObjectNormal.vz==6&&LOS_Lambda==777&&LOS_ObjectHitPtr==&displays[7]&&LOS_HModel_Section==(SECTION_DATA*)0x1234,"LOS shared globals restored");
    reset(); s=add(I_BehaviourAlien,0,0,19000); AccCombat_Update(0); s->SBdptr->ObWorld.vz=22000;
    CHECK(AccCombat_Update(100)==1 && speech_count==1,"selected visible target retained beyond acquisition range");
    s->SBdptr->ObWorld.vz=24001; before=cue_count; AccCombat_Update(200);
    CHECK(cue_count==before && strstr(speech[speech_count-1],"no longer in sight"),"retention limit stops target cues");
    CHECK(AccCombat_Update(1300)==0,"out-of-range target releases combat after grace");
    { VECTORCH snap;
      reset(); s=add(I_BehaviourAlien,1000,-1000,10000); AccCombat_Update(0);
      CHECK(AccCombat_GetSnapTarget(&snap) && snap.vx==1000 && snap.vy==-1000,"snap returns current visible targeting point");
      s->SBdptr->ObWorld.vx=2000;
      CHECK(AccCombat_GetSnapTarget(&snap) && snap.vx==2000,"snap follows live target movement between scans");
      actorVisible[0]=0; CHECK(!AccCombat_GetSnapTarget(&snap),"new obstruction blocks snap before next scan");
      actorVisible[0]=1; s->SBDamageBlock.Health=0; CHECK(!AccCombat_GetSnapTarget(&snap),"dead target blocks snap");
      s->SBDamageBlock.Health=100; s->SBname[0]='Z'; CHECK(!AccCombat_GetSnapTarget(&snap),"reused identity blocks snap");
      AccCombat_Reset(); CHECK(!AccCombat_GetSnapTarget(&snap),"reset clears snap target");
    }
    printf("Combat checks: %d, failures: %d\n",checks,failures); return failures?1:0;
}

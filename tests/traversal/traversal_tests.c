#include <stdio.h>
#include <string.h>
#include <math.h>
#include "3dc.h"
#include "stratdef.h"
#include "dynblock.h"
#include "bh_types.h"
#include "gamedef.h"
#include "bh_plift.h"
#include "extents.h"
#include "los.h"
#include "acc_traversal.h"

#define MAX_BOXES 32
#define EPS 1e-9

typedef struct { double min[3], max[3]; int solid; STRATEGYBLOCK *hitStrategy; } BOX;
static BOX boxes[MAX_BOXES];
static int boxCount, failures;
static int slopeEnabled, slopeZ0, slopeZ1;
static double slopeIntercept, slopePerZ;
static DYNAMICSBLOCK playerDyn;
static STRATEGYBLOCK playerStrategy;
static DISPLAYBLOCK playerDisplay;
static DYNAMICSBLOCK platformDyn;
static STRATEGYBLOCK platformStrategy;
static DISPLAYBLOCK platformDisplay;
static PLATFORMLIFT_BEHAVIOUR_BLOCK platformState;
static PLAYER_STATUS playerStatus;
DISPLAYBLOCK *Player;
AVP_GAME_DESC AvP;
COLLISION_EXTENTS CollisionExtents[MAX_NO_OF_COLLISION_EXTENTS];
VECTORCH LOS_Point, LOS_ObjectNormal;
int LOS_Lambda;
DISPLAYBLOCK *LOS_ObjectHitPtr;
SECTION_DATA *LOS_HModel_Section;

static int check(int condition, const char *name)
{
    if (!condition) { ++failures; printf("FAIL %s\n", name); return 0; }
    printf("PASS %s\n", name); return 1;
}

static void addBox(double x0,double y0,double z0,double x1,double y1,double z1,int solid)
{
    BOX *b = &boxes[boxCount++];
    b->min[0]=x0; b->min[1]=y0; b->min[2]=z0;
    b->max[0]=x1; b->max[1]=y1; b->max[2]=z1; b->solid=solid; b->hitStrategy=NULL;
}

static void addSlope(int z0,int z1,int yAtZero,double risePer1000)
{
    slopeEnabled=1; slopeZ0=z0; slopeZ1=z1;
    slopePerZ=risePer1000/1000.0; slopeIntercept=yAtZero;
}

/* Intersect a normalized ray with box faces. Floors/ceilings are box top/bottom
   faces; the nearest forward surface determines the reported LOS result. */
void FindPolygonInLineOfSight(VECTORCH *direction, VECTORCH *origin, int useList, DISPLAYBLOCK *ignore)
{
    double o[3]={origin->vx,origin->vy,origin->vz};
    double d[3]={direction->vx/65536.0,direction->vy/65536.0,direction->vz/65536.0};
    double best=1e30, hitNormal[3]={0,0,0};
    STRATEGYBLOCK *bestStrategy=NULL;
    int i,axis,side;
    (void)useList; (void)ignore;
    for(i=0;i<boxCount;i++) for(axis=0;axis<3;axis++) {
        if(fabs(d[axis])<EPS) continue;
        for(side=0;side<2;side++) {
            double plane=((side ? boxes[i].max[axis] : boxes[i].min[axis])-o[axis])/d[axis];
            double t=plane;
            double p[3]; int a,inside=1;
            if(t<0 || t>=best) continue;
            for(a=0;a<3;a++) { p[a]=o[a]+d[a]*t; if(a!=axis && (p[a]<boxes[i].min[a]-EPS || p[a]>boxes[i].max[a]+EPS)) inside=0; }
            if(inside) { best=t; bestStrategy=boxes[i].hitStrategy;
                hitNormal[0]=hitNormal[1]=hitNormal[2]=0; hitNormal[axis]=side?1:-1; }
        }
    }
    if(slopeEnabled && fabs(d[1]-slopePerZ*d[2])>EPS) {
        double t=(slopeIntercept+slopePerZ*o[2]-o[1])/(d[1]-slopePerZ*d[2]);
        double z=o[2]+d[2]*t;
        if(t>=0 && t<best && z>=slopeZ0 && z<=slopeZ1 && o[0]+d[0]*t>=-100000 && o[0]+d[0]*t<=100000) {
            best=t; bestStrategy=NULL;
            hitNormal[0]=0; hitNormal[1]=-1/sqrt(1+slopePerZ*slopePerZ);
            hitNormal[2]=slopePerZ/sqrt(1+slopePerZ*slopePerZ);
        }
    }
    LOS_Lambda=(best<1e29)?(int)(best+0.5):1000000;
    LOS_Point.vx=(best<1e29)?(int)(o[0]+d[0]*best):0;
    LOS_Point.vy=(best<1e29)?(int)(o[1]+d[1]*best):0;
    LOS_Point.vz=(best<1e29)?(int)(o[2]+d[2]*best):0;
    LOS_ObjectNormal.vx=(int)(hitNormal[0]*65536);
    LOS_ObjectNormal.vy=(int)(hitNormal[1]*65536);
    LOS_ObjectNormal.vz=(int)(hitNormal[2]*65536);
    LOS_ObjectHitPtr=bestStrategy ? &platformDisplay : NULL;
    LOS_HModel_Section=NULL;
}

static void reset(void)
{
    AccTraversal_Reset();
    memset(boxes,0,sizeof(boxes)); boxCount=0; slopeEnabled=0;
    memset(&playerDyn,0,sizeof(playerDyn));
    memset(&playerStrategy,0,sizeof(playerStrategy));
    memset(&playerDisplay,0,sizeof(playerDisplay));
    memset(&platformStrategy,0,sizeof(platformStrategy));
    memset(&platformDyn,0,sizeof(platformDyn));
    memset(&platformDisplay,0,sizeof(platformDisplay));
    memset(&platformState,0,sizeof(platformState));
    memset(&playerStatus,0,sizeof(playerStatus));
    memset(&AvP,0,sizeof(AvP)); AvP.PlayerType=I_Marine;
    memset(CollisionExtents,0,sizeof(CollisionExtents));
    CollisionExtents[CE_MARINE].CollisionRadius=450;
    CollisionExtents[CE_MARINE].Bottom=0;
    CollisionExtents[CE_MARINE].StandingTop=-1950;
    CollisionExtents[CE_MARINE].CrouchingTop=-1200;
    CollisionExtents[CE_PREDATOR].CollisionRadius=450;
    CollisionExtents[CE_PREDATOR].Bottom=0;
    CollisionExtents[CE_PREDATOR].StandingTop=-2098;
    CollisionExtents[CE_PREDATOR].CrouchingTop=-1300;
    playerDyn.GravityOn=1; playerDyn.UseStandardGravity=1;
    playerDyn.IsInContactWithFloor=1; playerDyn.IsInContactWithNearlyFlatFloor=1;
    playerDyn.IsFloating=0; playerDyn.OrientEuler.EulerY=0;
    playerStatus.Encumberance.JumpingMultiple=65536;
    playerStrategy.SBdptr=NULL;
    playerStrategy.SBdataptr=&playerStatus;
    playerStrategy.DynPtr=&playerDyn;
    playerDisplay.ObStrategyBlock=&playerStrategy;
    playerDisplay.ObMinY=-1950;
    Player=&playerDisplay;
    addBox(-100000,0,-100000,100000,100000,100000,1); /* solid floor */
}

static void platform_floor(int state)
{
    platformStrategy.I_SBtype=I_BehaviourPlatform;
    platformStrategy.SBdataptr=&platformState;
    platformStrategy.DynPtr=&platformDyn;
    platformDisplay.ObStrategyBlock=&platformStrategy;
    platformState.state=state;
    boxes[0].hitStrategy=&platformStrategy;
}

static VECTORCH target(int x,int z) { VECTORCH v={x,0,z}; return v; }
static void test(const char *name,int want,VECTORCH t)
{ check(AccTraversal_Probe(&t)==want,name); }

static int sharedResultsRestored(void)
{
    VECTORCH point={11,22,33},normal={44,55,66};
    DISPLAYBLOCK *obj=(DISPLAYBLOCK*)0x1234;
    SECTION_DATA *section=(SECTION_DATA*)0x5678;
    LOS_Point=point; LOS_ObjectNormal=normal; LOS_Lambda=777;
    LOS_ObjectHitPtr=obj; LOS_HModel_Section=section;
    (void)AccTraversal_Probe(&(VECTORCH){0,0,2000});
    return LOS_Point.vx==11&&LOS_Point.vy==22&&LOS_Point.vz==33&&
      LOS_ObjectNormal.vx==44&&LOS_ObjectNormal.vy==55&&LOS_ObjectNormal.vz==66&&
      LOS_Lambda==777&&LOS_ObjectHitPtr==obj&&LOS_HModel_Section==section;
}

static void notJump(const char *name,VECTORCH t)
{ check(AccTraversal_Probe(&t)!=ACC_TRAVERSAL_JUMP,name); }

static int samePoint(VECTORCH a,VECTORCH b)
{ return a.vx==b.vx && a.vy==b.vy && a.vz==b.vz; }

static void pillar(void)
{ addBox(-500,-2200,800,500,0,1800,1); }

static void steerTests(void)
{
    VECTORCH goal={0,0,6000}, out, first;
    int result;
    reset(); pillar(); out=(VECTORCH){-777,-888,-999};
    result=AccTraversal_Steer(&goal,0,&out);
    check(result==ACC_STEER_DETOUR && (out.vx < -500 || out.vx > 500),"pillar yields an off-axis detour waypoint");

    reset(); addBox(-30000,-2200,1000,30000,0,1400,1); out=(VECTORCH){111,222,333};
    result=AccTraversal_Steer(&goal,0,&out);
    check(result==ACC_STEER_BLOCKED && samePoint(out,(VECTORCH){111,222,333}),"wide wall with no exit is blocked and preserves output");

    reset(); addBox(-30000,-2200,1000,-350,0,1400,1); addBox(350,-2200,1000,30000,0,1400,1);
    out=(VECTORCH){111,222,333}; result=AccTraversal_Steer(&goal,0,&out);
    check(result==ACC_STEER_BLOCKED && samePoint(out,(VECTORCH){111,222,333}),"narrow opening fails Marine body-width clearance");

    reset(); boxCount=0; addBox(-30000,0,-10000,30000,100000,1000,1); addBox(-30000,0,2000,30000,100000,100000,1);
    pillar(); out=(VECTORCH){111,222,333}; result=AccTraversal_Steer(&goal,0,&out);
    check(result==ACC_STEER_BLOCKED && samePoint(out,(VECTORCH){111,222,333}),"floor gap rejects detour legs");

    reset(); goal=(VECTORCH){0,0,1500}; addBox(-30000,-2200,1200,30000,0,2200,1); out=(VECTORCH){0,0,0};
    result=AccTraversal_Steer(&goal,0,&out);
    check(result==ACC_STEER_BLOCKED,"wall-mounted control without stand-off is unreachable");
    result=AccTraversal_Steer(&goal,900,&out);
    check(result==ACC_STEER_DIRECT,"wall-mounted control stand-off leaves a reachable approach");

    reset(); goal=(VECTORCH){0,0,6000}; addBox(-500,-2200,2500,500,0,3200,1);
    result=AccTraversal_Steer(&goal,0,&out);
    check(result==ACC_STEER_DIRECT,"obstacle at 2.5m stays outside initial 1.2m lookahead");

    reset(); goal=(VECTORCH){0,0,6000}; pillar();
    result=AccTraversal_Steer(&goal,0,&out);
    check(result==ACC_STEER_DETOUR,"near pillar remains inside direct lookahead and detours");

    reset(); goal=(VECTORCH){0,0,20000};
    addBox(-30000,-2200,1200,4000,0,1700,1); /* only route around wall is beyond the cheap-search rings */
    out=(VECTORCH){111,222,333};
    result=AccTraversal_Steer(&goal,0,&out);
    check(result==ACC_STEER_DETOUR && !samePoint(out,goal) &&
          hypot((double)out.vx-playerDyn.Position.vx,(double)out.vz-playerDyn.Position.vz)<=5000 &&
          AccTraversal_WaypointLink(&playerDyn.Position,&out),
          "long blocked route returns a separately checked local prefix, never the distant goal");

    reset(); goal=(VECTORCH){3000,0,0};
    addBox(-1000,-2200,-1000,500,0,1000,1); /* Marine radius touches wall at x=950 */
    playerDyn.Position.vx=950;
    result=AccTraversal_Steer(&goal,0,&out);
    check(result==ACC_STEER_DIRECT && samePoint(out,goal),
          "Marine touching a wall can steer directly away without a false corner collision");

    /* Alternating openings force multiple turns around wall ends. */
    reset(); goal=(VECTORCH){0,0,7000};
    addBox(-30000,-2200,1200,1600,0,1600,1);
    addBox(-1600,-2200,4000,30000,0,4400,1);
    result=AccTraversal_Steer(&goal,0,&out); first=out;
    check(result==ACC_STEER_DETOUR && !samePoint(out,goal),
          "grid search finds an intermediate waypoint around the first wall");
    playerDyn.Position.vx=first.vx; playerDyn.Position.vz=first.vz;
    result=AccTraversal_Steer(&goal,0,&out);
    check(result==ACC_STEER_DETOUR && !samePoint(out,first) && !samePoint(out,goal),
          "cached multi-corner route advances to a distinct second intermediate after progress");
    first=out;
    addBox(first.vx-700,-2200,first.vz-700,first.vx+700,0,first.vz+700,1);
    result=AccTraversal_Steer(&goal,0,&out);
    check(result!=ACC_STEER_DETOUR || !samePoint(out,first),
          "new obstacle invalidates cached multi-corner leg instead of reusing its blocked waypoint");

    reset(); goal=(VECTORCH){0,0,6000}; pillar();
    result=AccTraversal_Steer(&goal,0,&out); first=out;
    check(result==ACC_STEER_DETOUR,"cache-clear scenario acquires detour");
    boxCount=1; /* remove pillar while preserving floor, without Reset */
    result=AccTraversal_Steer(&goal,0,&out);
    check(result==ACC_STEER_DIRECT && samePoint(out,goal) && !samePoint(out,first),
          "removing obstacle without Reset releases cached detour");

    goal=(VECTORCH){0,0,6000}; reset(); pillar(); result=AccTraversal_Steer(&goal,0,&out); first=out;
    playerDyn.OrientEuler.EulerY=512; playerDyn.Position.vz=100;
    result=AccTraversal_Steer(&goal,0,&out);
    check(result==ACC_STEER_DETOUR && samePoint(out,first),"cached detour stays stable through yaw change and progress");

    goal=(VECTORCH){0,0,6000}; reset(); pillar(); result=AccTraversal_Steer(&goal,0,&out); first=out;
    goal.vx=10000; result=AccTraversal_Steer(&goal,0,&out);
    check((result==ACC_STEER_DIRECT || result==ACC_STEER_DETOUR) && !samePoint(out,first),"changed goal invalidates cached waypoint");

    goal=(VECTORCH){0,0,6000}; reset(); pillar(); result=AccTraversal_Steer(&goal,0,&out); first=out;
    addBox(-1200,-2200,-100,1200,0,250,1);
    result=AccTraversal_Steer(&goal,0,&out);
    check(result!=ACC_STEER_DETOUR || !samePoint(out,first),"new wall invalidates blocked cached leg");

    goal=(VECTORCH){0,0,6000}; reset(); pillar(); result=AccTraversal_Steer(&goal,0,&out); first=out;
    check(result==ACC_STEER_DETOUR,"reset case begins with a cached detour");
    boxCount=1; /* remove only the pillar, retaining the broad floor */
    AccTraversal_Reset();
    result=AccTraversal_Steer(&goal,0,&out);
    check(result==ACC_STEER_DIRECT && samePoint(out,goal),"explicit reset clears detour after obstacle removal");
}

static void groundFollowingTests(void)
{
    VECTORCH goal={0,0,1000},out={111,222,333},a={0,0,0},b={0,0,6000};
    int result;
    reset(); boxCount=0; addSlope(-1000,10000,0,100);
    result=AccTraversal_Steer(&goal,0,&out);
    check(result==ACC_STEER_DIRECT && out.vy>=90 && out.vy<=110,
          "ground steering follows a modest ramp down and returns its actual endpoint height");

    reset(); boxCount=0; addSlope(-1000,10000,0,-100); goal=(VECTORCH){0,0,1000};
    result=AccTraversal_Steer(&goal,0,&out);
    check(result==ACC_STEER_DIRECT && out.vy<=-90 && out.vy>=-110,
          "ground steering follows a modest ramp up");

    reset(); boxCount=0; addSlope(-1000,10000,0,100);
    addBox(-30000,-2200,1000,1600,250,1600,1);
    addBox(-1600,-2200,4000,30000,400,4400,1);
    goal=(VECTORCH){0,0,7000}; result=AccTraversal_Steer(&goal,0,&out);
    check(result==ACC_STEER_DETOUR && out.vy>=-250 && out.vy<=950,
          "local grid routes around alternating walls while retaining ramp floor heights");

    reset(); boxCount=0; addSlope(-1000,2500,0,100); goal=(VECTORCH){0,0,6000};
    check(AccTraversal_WaypointLink(&a,&goal)==0,
          "ground-following link rejects unsupported ledge beyond ramp end");

    reset(); boxCount=0; addSlope(-1000,10000,0,100);
    addBox(-30000,-1900,1000,30000,-1800,5000,1); goal=(VECTORCH){0,0,6000};
    check(AccTraversal_WaypointLink(&a,&goal)==0,
          "ground-following link rejects low ceiling over ramp");

    reset(); AvP.PlayerType=I_Predator; playerDisplay.ObMinY=-2098;
    addBox(-30000,-2050,1000,30000,-2000,5000,1);
    goal=(VECTORCH){0,0,6000}; out=(VECTORCH){111,222,333};
    check(AccTraversal_Steer(&goal,0,&out)==ACC_STEER_BLOCKED,
          "Predator standing height rejects ceiling that Marine clearance accepts");
    reset(); addBox(-30000,-2050,1000,30000,-2000,5000,1);
    goal=(VECTORCH){0,0,6000}; out=(VECTORCH){111,222,333};
    check(AccTraversal_Steer(&goal,0,&out)==ACC_STEER_DIRECT,
          "Marine standing clearance remains unchanged under same low ceiling");

    reset(); boxCount=0; addSlope(-1000,10000,0,100); b=(VECTORCH){0,0,6000};
    check(AccTraversal_WaypointLink(&a,&b),
          "waypoint link accepts continuous ramp when sampled end reaches target floor");
}

int main(void)
{
    VECTORCH t;
    /* Open lateral routes use engine yaw basis: yaw=0 faces +Z, right is +X. */
    reset(); t=target(1200,0); test("open floor suggests lateral right relative yaw",ACC_TRAVERSAL_RIGHT,t);
    reset(); t=target(-1200,0); test("open floor suggests lateral left relative yaw",ACC_TRAVERSAL_LEFT,t);
    reset(); playerDyn.OrientEuler.EulerY=1024; t=target(0,-1200); test("open floor suggests lateral right at quarter yaw",ACC_TRAVERSAL_RIGHT,t);

    reset(); addBox(450,-2200, -500,650,0,500,1); t=target(1200,0); test("blocked lateral route rejected",ACC_TRAVERSAL_NONE,t);
    reset(); boxes[0].min[2]=500; t=target(1200,0); test("lateral floor gap rejected",ACC_TRAVERSAL_NONE,t);

    /* Candidate obstacle is 700mm high (inside the 550..850 probe band). */
    reset(); addBox(-250,-700,1000,250,0,1500,1); t=target(0,3000); test("low solid obstacle with landing suggests jump",ACC_TRAVERSAL_JUMP,t);
    /* Make the tall wall span the full three-lane probe, so no sidestep is possible. */
    reset(); addBox(-2000,-2200,1000,2000,0,1500,1); t=target(0,3000); test("tall wall spanning corridor rejected",ACC_TRAVERSAL_NONE,t);
    reset(); addBox(-2000,-2700,-1000,2000,-2500,5000,1); addBox(-250,-700,1000,250,0,1500,1); t=target(0,3000); notJump("standing clearance but insufficient jump headroom rejects jump hint",t);
    /* A 1m wide gap interrupts only the far landing floor; start stays solid. */
    reset(); boxCount=0; addBox(-100000,0,-100000,100000,100000,1900,1); addBox(-100000,0,3200,100000,100000,100000,1); addBox(-250,-700,1000,250,0,1500,1); t=target(0,3000); notJump("far landing gap rejects jump hint",t);

    reset(); playerDyn.IsInContactWithFloor=0; t=target(0,3000); test("airborne Marine rejected",ACC_TRAVERSAL_NONE,t);
    reset(); playerDyn.IsInContactWithNearlyFlatFloor=0; t=target(0,3000); test("non-flat floor contact rejected",ACC_TRAVERSAL_NONE,t);
    reset(); playerDyn.GravityOn=0; t=target(0,3000); test("gravity disabled rejected",ACC_TRAVERSAL_NONE,t);
    reset(); playerDyn.UseStandardGravity=0; t=target(0,3000); test("non-standard gravity rejected",ACC_TRAVERSAL_NONE,t);
    reset(); playerDisplay.ObMinY=-1200; t=target(0,3000); test("crouched Marine rejected",ACC_TRAVERSAL_NONE,t);
    reset(); playerStatus.Encumberance.JumpingMultiple=32768; addBox(-250,-700,1000,250,0,1500,1); t=target(0,3000); notJump("reduced jump strength rejects jump hint",t);
    reset(); check(sharedResultsRestored(),"shared LOS globals restored after probes");
    {
        VECTORCH a={0,-1000,0}, b={0,-1000,5000};
        reset(); boxCount=0; addBox(-10000,0,-10000,10000,300,10000,1); platform_floor(PLBS_AtRest);
        check(AccTraversal_WaypointLink(&a,&b),"stopped platform supplies walkable floor support");
        reset(); boxCount=0; addBox(-10000,0,-10000,10000,300,10000,1); platform_floor(PLBS_Activating);
        check(AccTraversal_WaypointLink(&a,&b),"platform activating while stopped remains walkable support");
        reset(); boxCount=0; addBox(-10000,0,-10000,10000,300,10000,1); platform_floor(PLBS_GoingUp);
        check(!AccTraversal_WaypointLink(&a,&b),"moving platform is not accepted as stable floor support");
        reset(); boxCount=0; addBox(-10000,0,-10000,10000,300,10000,1); platform_floor(PLBS_GoingDown);
        check(!AccTraversal_WaypointLink(&a,&b),"descending platform is not accepted as stable floor support");
    }
    {
        VECTORCH goal={0,0,10000}, out={111,222,333};
        int normal, exitLift;
        reset(); boxCount=0;
        addBox(-30000,0,-10000,30000,100000,2500,1);
        addBox(-30000,0,3500,30000,100000,30000,1); /* gap begins beyond 1.2m lookahead */
        normal=AccTraversal_Steer(&goal,0,&out);
        check(normal==ACC_STEER_DIRECT,"normal steering accepts clear first 1.2m before distant floor gap");
        AccTraversal_Reset();
        exitLift=AccTraversal_ExitLift(&goal,&out);
        check(exitLift!=ACC_STEER_DIRECT,"exit-lift long lookahead rejects direct departure across distant floor gap");
    }
    steerTests();
    groundFollowingTests();
    {
        VECTORCH a={0,-1000,0}, b={0,-1000,10000};
        reset();
        check(AccTraversal_WaypointLink(&a,&b),"long supplemental link accepts continuous clear floor");
        reset(); boxCount=0;
        addBox(-100000,0,-100000,100000,100000,4100,1);
        addBox(-100000,0,4600,100000,100000,100000,1);
        check(!AccTraversal_WaypointLink(&a,&b),"long supplemental link rejects a middle floor gap");
        reset(); addBox(-2000,-2200,7000,2000,0,7300,1);
        check(!AccTraversal_WaypointLink(&a,&b),"long supplemental link rejects a distant wall");
    }
    printf("Traversal checks: %d failures\n",failures);
    return failures?1:0;
}

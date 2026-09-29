#include "3dc.h"
#include "module.h"
#include "stratdef.h"
#include "dynblock.h"
#include "los.h"
#include "extents.h"
#include "gamedef.h"
#include "bh_types.h"
#include "bh_plift.h"
#include "acc_traversal.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

extern DISPLAYBLOCK *Player;

/* Millimetres. Deliberately narrower than the nominal 1620mm Marine jump
   apex (9000mm/s impulse, 25000mm/s^2 gravity). No gap-jump inference. */
#define LOW_OBSTACLE 550
#define CLEAR_ABOVE 850
#define JUMP_HEADROOM 1800
#define SIDE_STEP 900
static int HaveDetour, DetourStandOff;
static VECTORCH Detour, DetourGoal;
/* Used only when the cheap two-leg search fails. Owned route state, never AI
   scratch space. A finite grid bounds both work and the maximum local detour. */
#define GRID_SIDE 31
#define GRID_COUNT (GRID_SIDE*GRID_SIDE)
#define GRID_STEP 800
static VECTORCH LocalPath[GRID_COUNT], LocalGoal;
static int LocalCount, LocalStandOff;

void AccTraversal_Reset(void) { HaveDetour = 0; LocalCount = 0; }

static int PlayerExtent(void)
{ return AvP.PlayerType==I_Predator ? CE_PREDATOR : CE_MARINE; }

static VECTORCH Offset(VECTORCH p, double x, int y, double z)
{
    p.vx += (int)x; p.vy += y; p.vz += (int)z;
    return p;
}

/* Preserve the shared raycast result: other gameplay systems own it too. */
static int Ray(VECTORCH p, double x, double y, double z, int range, int *flat)
{
    VECTORCH direction = {(int)(x*65536), (int)(y*65536), (int)(z*65536)};
    VECTORCH savedPoint = LOS_Point, savedNormal = LOS_ObjectNormal;
    DISPLAYBLOCK *savedObject = LOS_ObjectHitPtr;
    SECTION_DATA *savedSection = LOS_HModel_Section;
    int savedDistance = LOS_Lambda, hit;
    FindPolygonInLineOfSight(&direction, &p, 0, Player);
    hit = LOS_Lambda >= 0 && LOS_Lambda < range;
    if (flat) {
        STRATEGYBLOCK *sb = LOS_ObjectHitPtr ? LOS_ObjectHitPtr->ObStrategyBlock : NULL;
        int stoppedLift=0;
        if(sb && sb->I_SBtype==I_BehaviourPlatform && sb->SBdataptr) {
            PLATFORMLIFT_BEHAVIOUR_BLOCK *lift=sb->SBdataptr;
            stoppedLift=lift->state==PLBS_AtRest || lift->state==PLBS_Activating;
        }
        *flat = hit && LOS_ObjectNormal.vy < -60000 &&
            (!sb || !sb->DynPtr || sb->DynPtr->IsStatic || stoppedLift);
    }
    if (hit) hit = LOS_Lambda + 1; /* zero-distance hit remains distinguishable */
    LOS_Point = savedPoint; LOS_ObjectNormal = savedNormal;
    LOS_ObjectHitPtr = savedObject; LOS_HModel_Section = savedSection;
    LOS_Lambda = savedDistance;
    return hit;
}

/* Three lanes span the standing Marine's width, including a small margin.
   Floor samples reject drops and steep/moving surfaces, including at the start. */
static int Walkable(VECTORCH p, double dx, double dz, int length, int radius, int height)
{
    int lane, step, level;
    int steps=(length+299)/300;
    if(steps<1) steps=1;
    for (lane = -1; lane <= 1; ++lane) {
        VECTORCH edge = Offset(p, dz*lane*radius, 0, -dx*lane*radius);
        int heights[3] = {500, height/2, height-50};
        for (level = 0; level < 3; ++level)
            if (Ray(Offset(edge, 0, -heights[level], 0), dx, 0, dz, length+radius, NULL)) return 0;
        for (step = 0; step <= steps; ++step) {
            int flat, hit;
            /* Sample the swept supporting strip. Sampling a square behind the
               start falsely traps a round player touching a wall or corner. */
            double travel = (double)length*step/steps;
            VECTORCH sample = Offset(edge, dx*travel, -400, dz*travel);
            hit = Ray(sample, 0, 1, 0, 650, &flat);
            if (!hit || !flat || hit < 151 || hit > 651) return 0;
        }
    }
    return 1;
}

static int LowJump(VECTORCH p, double dx, double dz, int radius, int height)
{
    int lane, step, level, lowHit = 0;
    /* A solid obstacle higher than an automatic 450mm step, close enough to
       act now. All three lanes above it must be clear. */
    for (lane = -1; lane <= 1; ++lane) {
        VECTORCH edge = Offset(p, dz*lane*radius, 0, -dx*lane*radius);
        int hit = Ray(Offset(edge, 0, -LOW_OBSTACLE, 0), dx, 0, dz, 1400, NULL);
        if (hit) lowHit = 1;
        for (level = CLEAR_ABOVE; level <= height+JUMP_HEADROOM; level += 400)
            if (Ray(Offset(edge, 0, -level, 0), dx, 0, dz, 4000, NULL)) return 0;
        for (step = 0; step <= 8; ++step) {
            int flat, floorHit;
            VECTORCH sample = Offset(edge, dx*500*step, 0, dz*500*step);
            /* Vertical clearance at the start and along the full envelope. */
            if (Ray(Offset(sample, 0, -CLEAR_ABOVE, 0), 0, -1, 0,
                    height+JUMP_HEADROOM-CLEAR_ABOVE, NULL)) return 0;
            floorHit = Ray(Offset(sample, 0, -CLEAR_ABOVE, 0), 0, 1, 0,
                           CLEAR_ABOVE+150, &flat);
            if (!floorHit || !flat) return 0;
            /* Beyond the obstacle require broad, nearly level landing ground.
               No higher ledges, missing floor or drops are called jumpable. */
            if (step >= 4 && floorHit < CLEAR_ABOVE-200) return 0;
        }
    }
    return lowHit;
}

static double HorizontalDistance(VECTORCH a, VECTORCH b)
{
    double x = (double)a.vx-b.vx, z = (double)a.vz-b.vz;
    return sqrt(x*x+z*z);
}

static int SegmentGround(VECTORCH from,VECTORCH to,int limit,int radius,int height,VECTORCH *outEnd);
static int Segment(VECTORCH from, VECTORCH to, int limit, int radius, int height)
{
    VECTORCH end;
    return SegmentGround(from,to,limit,radius,height,&end);
}

/* Follow only continuous, modest floor changes. Each lane reacquires nearby
   static/stopped support relative to its previous sample; a missing floor,
   >250mm local step, moving surface, steep normal, or blocked body rejects it. */
static int SegmentGround(VECTORCH from,VECTORCH to,int limit,int radius,int height,VECTORCH *outEnd)
{
    double distance = HorizontalDistance(from, to);
    double dx,dz;
    int steps,step,lane,previous[3],endY=from.vy,travelLimit;
    if(distance<1) { if(outEnd)*outEnd=from; return 1; }
    dx=((double)to.vx-from.vx)/distance; dz=((double)to.vz-from.vz)/distance;
    travelLimit=(int)fmin(distance,limit);
    steps=(travelLimit+299)/300; if(steps<1)steps=1;
    for(lane=0;lane<3;++lane) previous[lane]=from.vy;
    for(step=0;step<=steps;++step) {
        double travel=(double)travelLimit*step/steps;
        int floorY[3],flat,lvl;
        const int bodyLevels[3]={500,height/2,height-50};
        for(lane=0;lane<3;++lane) {
            VECTORCH edge=Offset(from,dz*(lane-1)*radius,0,-dx*(lane-1)*radius);
            int oldFloor=previous[lane];
            VECTORCH sample=Offset(edge,dx*travel,oldFloor-400-from.vy,dz*travel);
            int hit=Ray(sample,0,1,0,651,&flat),delta;
            int nextFloor;
            if(!hit||!flat) return 0;
            delta=hit-1-400;
            if(delta < -250 || delta > 250) return 0;
            nextFloor=sample.vy+hit-1;
            if(step && abs(nextFloor-oldFloor)>250) return 0;
            floorY[lane]=nextFloor;
            if(step) {
                double priorTravel=(double)travelLimit*(step-1)/steps;
                int highFloor=oldFloor<nextFloor?oldFloor:nextFloor;
                VECTORCH prior=Offset(edge,dx*priorTravel,highFloor-from.vy,dz*priorTravel);
                for(lvl=0;lvl<3;++lvl)
                    if(Ray(Offset(prior,0,-bodyLevels[lvl],0),dx,0,dz,
                           (int)ceil(travel-priorTravel)+2,NULL)) return 0;
            }
            previous[lane]=nextFloor;
        }
        if(abs(floorY[0]-floorY[1])>250 || abs(floorY[2]-floorY[1])>250) return 0;
        endY=floorY[1];
        /* A vertical probe from just above the floor through the standing
           body catches ceilings without requiring arbitrary extra headroom. */
        for(lane=0;lane<3;++lane) {
            VECTORCH edge=Offset(from,dz*(lane-1)*radius,0,-dx*(lane-1)*radius);
            VECTORCH here=Offset(edge,dx*travel,floorY[lane]-from.vy,dz*travel);
            if(Ray(Offset(here,0,-50,0),0,-1,0,height-100,NULL)) return 0;
        }
    }
    if(outEnd) { *outEnd=from; outEnd->vx+=(int)(dx*travelLimit); outEnd->vz+=(int)(dz*travelLimit); outEnd->vy=endY; }
    return 1;
}
int AccTraversal_WaypointLink(const VECTORCH *a,const VECTORCH *b)
{
    VECTORCH from=*a,to=*b,end; int hit,flat,extent=PlayerExtent();
    double distance=HorizontalDistance(from,to);
    if(distance<1 || distance>20000) return 0;
    hit=Ray(from,0,1,0,6000,&flat);
    if(!hit || !flat) return 0;
    from.vy+=hit-1;
    hit=Ray(to,0,1,0,6000,&flat);
    if(!hit || !flat) return 0;
    to.vy+=hit-1;
    if(!SegmentGround(from,to,20000,CollisionExtents[extent].CollisionRadius,
        CollisionExtents[extent].Bottom-CollisionExtents[extent].StandingTop,&end)) return 0;
    return abs(end.vy-to.vy)<=200;
}

static int FollowLocal(VECTORCH p, int radius, int height, VECTORCH *out)
{
    int i;
    for(i=LocalCount-1;i>=0;--i) {
        if(HorizontalDistance(p,LocalPath[i])<350) { LocalCount=0; return 0; }
        if(HorizontalDistance(p,LocalPath[i])<=5000 && Segment(p,LocalPath[i],5000,radius,height)) {
            *out=LocalPath[i];
            /* Retain the rest of the path, including this current waypoint. */
            if(i) { memmove(LocalPath,LocalPath+i,(LocalCount-i)*sizeof(*LocalPath)); LocalCount-=i; }
            return 1;
        }
    }
    LocalCount=0; return 0;
}

static int SearchLocal(VECTORCH p,VECTORCH goal,int radius,int height,VECTORCH *out)
{
    float cost[GRID_COUNT]; int floorY[GRID_COUNT]; short parent[GRID_COUNT]; unsigned char closed[GRID_COUNT]={0};
    const int offsets[4]={-1,1,-GRID_SIDE,GRID_SIDE};
    int i,iteration,found=-1,centre=GRID_COUNT/2,progress=-1,partial=0;
    double closest=HorizontalDistance(p,goal)-2400;
    for(i=0;i<GRID_COUNT;++i) { cost[i]=1e30f; parent[i]=-1; }
    cost[centre]=0; floorY[centre]=p.vy;
    for(iteration=0;iteration<400;++iteration) {
        int at=-1,k; double best=1e30; VECTORCH a;
        for(i=0;i<GRID_COUNT;++i) if(!closed[i] && cost[i]<1e29f) {
            VECTORCH v=Offset(p,(i%GRID_SIDE-GRID_SIDE/2)*GRID_STEP,floorY[i]-p.vy,(i/GRID_SIDE-GRID_SIDE/2)*GRID_STEP);
            double score=cost[i]+HorizontalDistance(v,goal);
            if(score<best) { best=score; at=i; }
        }
        if(at<0) break;
        a=Offset(p,(at%GRID_SIDE-GRID_SIDE/2)*GRID_STEP,floorY[at]-p.vy,(at/GRID_SIDE-GRID_SIDE/2)*GRID_STEP);
        closed[at]=1;
        if(HorizontalDistance(a,goal)<closest) { closest=HorizontalDistance(a,goal); progress=at; }
        if(HorizontalDistance(a,goal)<=2400) {
            VECTORCH end;
            if(SegmentGround(a,goal,2400,radius,height,&end)) { found=at; break; }
        }
        for(k=0;k<4;++k) {
            int next=at+offsets[k]; VECTORCH b;
            if(next<0 || next>=GRID_COUNT || closed[next] ||
               (k<2 && next/GRID_SIDE!=at/GRID_SIDE) || cost[next]<=cost[at]+GRID_STEP) continue;
            b=Offset(p,(next%GRID_SIDE-GRID_SIDE/2)*GRID_STEP,floorY[at]-p.vy,(next/GRID_SIDE-GRID_SIDE/2)*GRID_STEP);
            { VECTORCH end; if(!SegmentGround(a,b,GRID_STEP,radius,height,&end)) continue; floorY[next]=end.vy; }
            cost[next]=cost[at]+GRID_STEP; parent[next]=(short)at;
        }
    }
    /* Long room legs can extend beyond the grid. Return only a checked prefix,
       never append an unchecked final edge to the distant destination. */
    if(found<0 && HorizontalDistance(p,goal)>GRID_STEP*(GRID_SIDE/2) && progress>=0) { found=progress; partial=1; }
    if(getenv("AVP_NAV_TRACE")) fprintf(stderr,"NAVGRID from=%d,%d,%d goal=%d,%d,%d radius=%d expanded=%d found=%d partial=%d\n",p.vx,p.vy,p.vz,goal.vx,goal.vy,goal.vz,radius,iteration,found,partial);
    if(found<0) return 0;
    LocalCount=0;
    if(!partial) {
        VECTORCH end;
        if(SegmentGround(Offset(p,(found%GRID_SIDE-GRID_SIDE/2)*GRID_STEP,floorY[found]-p.vy,(found/GRID_SIDE-GRID_SIDE/2)*GRID_STEP),goal,2400,radius,height,&end)) goal.vy=end.vy;
        LocalPath[LocalCount++]=goal;
    }
    for(i=found;i!=centre && i>=0 && LocalCount<GRID_COUNT;i=parent[i])
        LocalPath[LocalCount++]=Offset(p,(i%GRID_SIDE-GRID_SIDE/2)*GRID_STEP,floorY[i]-p.vy,(i/GRID_SIDE-GRID_SIDE/2)*GRID_STEP);
    for(i=0;i<LocalCount/2;++i) { VECTORCH v=LocalPath[i];LocalPath[i]=LocalPath[LocalCount-1-i];LocalPath[LocalCount-1-i]=v; }
    return FollowLocal(p,radius,height,out);
}

static int Steer(const struct vectorch *target, int standOff, struct vectorch *waypoint, int directLimit)
{
    STRATEGYBLOCK *sb = Player ? Player->ObStrategyBlock : NULL;
    DYNAMICSBLOCK *dyn = sb ? sb->DynPtr : NULL;
    VECTORCH p, goal, best = {0,0,0};
    int extent=PlayerExtent();
    int radius = CollisionExtents[extent].CollisionRadius;
    int height = CollisionExtents[extent].Bottom-CollisionExtents[extent].StandingTop;
    int ring, direction, found = 0;
    double distance, bearing, bestCost = 1e30;
    if (!target || !waypoint || !dyn || !dyn->GravityOn || !dyn->UseStandardGravity ||
        !dyn->IsInContactWithFloor || !dyn->IsInContactWithNearlyFlatFloor || dyn->IsFloating ||
        Player->ObMinY > CollisionExtents[extent].StandingTop+100) {
        HaveDetour = 0; LocalCount=0; return ACC_STEER_UNAVAILABLE;
    }
    p = dyn->Position; p.vy += CollisionExtents[extent].Bottom;
    goal = *target; goal.vy = p.vy;
    distance = HorizontalDistance(p, goal);
    if (standOff < 0) standOff = 0;
    if (standOff && distance > standOff) {
        goal.vx -= (int)((goal.vx-(double)p.vx)*standOff/distance);
        goal.vz -= (int)((goal.vz-(double)p.vz)*standOff/distance);
    } else if (standOff) {
        HaveDetour = 0; LocalCount=0; *waypoint = *target; return ACC_STEER_DIRECT;
    }
    if(LocalCount) {
        if(LocalStandOff==standOff && HorizontalDistance(*target,LocalGoal)<200 &&
           abs(target->vy-LocalGoal.vy)<500 && abs(p.vy-LocalPath[0].vy)<200 &&
           FollowLocal(p,radius,height,waypoint)) return ACC_STEER_DETOUR;
        LocalCount=0;
    }
    /* Prefer the original route again once its longer approach is clear. */
    if (HaveDetour) {
        VECTORCH end;
        if(SegmentGround(p,goal,directLimit>3000?directLimit:3000,radius,height,&end)) {
            HaveDetour = 0; *waypoint = *target;
            if(distance<=directLimit) waypoint->vy=end.vy;
            return ACC_STEER_DIRECT;
        }
    }
    /* Finish a chosen side of the obstacle before reconsidering the other.
       Revalidate the remaining leg, so a closing door invalidates the cache. */
    if (HaveDetour && DetourStandOff == standOff && HorizontalDistance(*target, DetourGoal) < 200 &&
        abs(target->vy-DetourGoal.vy) < 500 && abs(p.vy-Detour.vy) < 200 &&
        HorizontalDistance(p, Detour) > 350 && HorizontalDistance(p, Detour) < 5000 &&
        Segment(p, Detour, 5000, radius, height)) {
        *waypoint = Detour; return ACC_STEER_DETOUR;
    }
    HaveDetour = 0;
    {
        VECTORCH end;
        if(SegmentGround(p,goal,directLimit,radius,height,&end)) {
            *waypoint = *target;
            if(distance<=directLimit) waypoint->vy=end.vy;
            return ACC_STEER_DIRECT;
        }
    }
    bearing = atan2((double)goal.vx-p.vx, (double)goal.vz-p.vz);
    /* Bounded two-leg visibility search, not merely an empty ray to either
       side. Both legs require body clearance and supporting level ground. */
    for (ring = 1; ring <= 3; ++ring) {
        for (direction = 1; direction < 16; ++direction) {
            double angle = bearing + direction*(6.283185307179586/16);
            VECTORCH candidate = Offset(p, sin(angle)*ring*1200, 0, cos(angle)*ring*1200);
            double remainder = HorizontalDistance(candidate, goal);
            double cost = ring*1200+remainder;
            {
                VECTORCH firstEnd, secondEnd;
                if (cost >= bestCost || remainder >= distance+400 ||
                    !SegmentGround(p,candidate,4000,radius,height,&firstEnd) ||
                    !SegmentGround(firstEnd,goal,directLimit>3000?directLimit:3000,radius,height,&secondEnd)) continue;
                candidate.vy=firstEnd.vy;
            }
            best = candidate; bestCost = cost; found = 1;
        }
    }
    if (!found) {
        if(SearchLocal(p,goal,radius,height,waypoint)) {
            LocalGoal=*target; LocalStandOff=standOff;
            return ACC_STEER_DETOUR;
        }
        return ACC_STEER_BLOCKED;
    }
    Detour = best; DetourGoal = *target; DetourStandOff = standOff; HaveDetour = 1;
    *waypoint = best;
    return ACC_STEER_DETOUR;
}

int AccTraversal_Steer(const struct vectorch *target,int standOff,struct vectorch *waypoint)
{ return Steer(target,standOff,waypoint,1200); }
int AccTraversal_ExitLift(const struct vectorch *target,struct vectorch *waypoint)
{ return Steer(target,0,waypoint,20000); }

static int Probe(const VECTORCH *target)
{
    STRATEGYBLOCK *sb = Player ? Player->ObStrategyBlock : NULL;
    DYNAMICSBLOCK *dyn = sb ? sb->DynPtr : NULL;
    VECTORCH p;
    double yaw, fx, fz, rx, rz, dx, dz, ahead, side, distance;
    int extent=PlayerExtent();
    int radius = CollisionExtents[extent].CollisionRadius + 30;
    int height = CollisionExtents[extent].Bottom - CollisionExtents[extent].StandingTop;
    if (!target || !dyn || !dyn->IsInContactWithFloor || !dyn->UseStandardGravity ||
        !dyn->GravityOn || dyn->IsFloating || !dyn->IsInContactWithNearlyFlatFloor) return 0;
    if (Player->ObMinY > CollisionExtents[extent].StandingTop+100) return 0;
    p = dyn->Position; p.vy += CollisionExtents[extent].Bottom;
    yaw = (dyn->OrientEuler.EulerY & 4095) * (6.283185307179586/4096.0);
    fx = sin(yaw); fz = cos(yaw); rx = fz; rz = -fx;
    dx = (double)target->vx-p.vx; dz = (double)target->vz-p.vz;
    ahead = dx*fx+dz*fz; side = dx*rx+dz*rz; distance = sqrt(dx*dx+dz*dz);
    /* Nearby lateral alignment, not a replacement for turning down a corridor. */
    if (fabs(side) >= 600 && fabs(side) <= 2500 && fabs(ahead) <= 800) {
        int sign = side > 0 ? 1 : -1;
        int length = (int)fmin(fabs(side), SIDE_STEP);
        if (Walkable(p, rx*sign, rz*sign, length, radius, height))
            return sign > 0 ? ACC_TRAVERSAL_RIGHT : ACC_TRAVERSAL_LEFT;
    }
    if (ahead < 1000 || distance < 1 || ahead/distance < 0.94) return 0;
    if (Walkable(p, fx, fz, 1200, radius, height)) return 0;
    /* Weapon encumbrance can reduce jumping. Do not promise the normal
       clearance when the movement code will apply a smaller impulse. */
#if !defined(LOAD_IN_MOVEMENT_VALUES) || !LOAD_IN_MOVEMENT_VALUES
    if (sb->SBdataptr && ((PLAYER_STATUS *)sb->SBdataptr)->Encumberance.JumpingMultiple == 65536 &&
        LowJump(p, fx, fz, radius, height)) return ACC_TRAVERSAL_JUMP;
#endif
    /* Sidestep around a local obstruction only when both the sideways motion
       and forward space at its end have floor and body clearance. */
    {
        int first = side < 0 ? -1 : 1, attempt;
        for (attempt = 0; attempt < 2; ++attempt) {
            int sign = attempt ? -first : first;
            VECTORCH end = Offset(p, rx*sign*SIDE_STEP, 0, rz*sign*SIDE_STEP);
            if (Walkable(p, rx*sign, rz*sign, SIDE_STEP, radius, height) &&
                Walkable(end, fx, fz, 1200, radius, height))
                return sign > 0 ? ACC_TRAVERSAL_RIGHT : ACC_TRAVERSAL_LEFT;
        }
    }
    return 0;
}

int AccTraversal_Probe(const struct vectorch *target) { return Probe(target); }

const char *AccTraversal_Text(int hint)
{
    switch (hint) {
        case ACC_TRAVERSAL_LEFT: return "Strafe left.";
        case ACC_TRAVERSAL_RIGHT: return "Strafe right.";
        case ACC_TRAVERSAL_JUMP: return "Jump forward a little.";
        default: return "";
    }
}

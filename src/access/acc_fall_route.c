#include "acc_fall_route.h"
#include <math.h>
#include <string.h>

#define FLOOR_Y_MIN 4200
#define FLOOR_Y_MAX 5200
#define FIRST_LAUNCH_X 17680
#define FIRST_LAUNCH_Y 4741
#define FIRST_LAUNCH_Z (-460)
#define FIRST_LANDING_X 24311
#define FIRST_LANDING_Y 4739
#define FIRST_LANDING_Z (-2274)
#define SECOND_LAUNCH_X 40372
#define SECOND_LAUNCH_Y 4741
#define SECOND_LAUNCH_Z (-2528)
#define SECOND_LANDING_X 40183
#define SECOND_LANDING_Y 4737
#define SECOND_LANDING_Z 8665

typedef struct FALL_POINT { int x, y, z; } FALL_POINT;

static const FALL_POINT FirstStaging = { 15750, 4741, 0 };
static const FALL_POINT FirstLaunch = { FIRST_LAUNCH_X, FIRST_LAUNCH_Y, FIRST_LAUNCH_Z };
static const FALL_POINT FirstLanding = { FIRST_LANDING_X, FIRST_LANDING_Y, FIRST_LANDING_Z };
static const FALL_POINT NorthDeckWaypoint = { 33242, 4739, -4654 };
static const FALL_POINT NorthEastStaging = { 40416, 4738, -5049 };
static const FALL_POINT SecondLaunch = { SECOND_LAUNCH_X, SECOND_LAUNCH_Y, SECOND_LAUNCH_Z };
static const FALL_POINT SecondLanding = { SECOND_LANDING_X, SECOND_LANDING_Y, SECOND_LANDING_Z };
/* Cross the opening far enough for room 9 to resume ordinary route guidance. */
static const FALL_POINT GateEntry = { 41427, 4740, 21739 };

static double HorizontalDistance(int x1, int z1, int x2, int z2)
{ return hypot((double)x2-x1, (double)z2-z1); }

static int InBounds(int x, int z, int x0, int x1, int z0, int z1)
{ return x >= x0 && x <= x1 && z >= z0 && z <= z1; }

static int Near(const ACC_FALL_ROUTE_INPUT *in, const FALL_POINT *p, int radius)
{ return HorizontalDistance(in->x,in->z,p->x,p->z) <= radius; }

static double SegmentDistance(const ACC_FALL_ROUTE_INPUT *in,
    const FALL_POINT *a, const FALL_POINT *b)
{
    double dx=(double)b->x-a->x, dz=(double)b->z-a->z;
    double length2=dx*dx+dz*dz;
    double t=(((double)in->x-a->x)*dx+((double)in->z-a->z)*dz)/length2;
    double px,pz;
    if(t<0.0)t=0.0; else if(t>1.0)t=1.0;
    px=a->x+t*dx; pz=a->z+t*dz;
    return hypot((double)in->x-px,(double)in->z-pz);
}

static int WrappedYaw(int value)
{ value %= 4096; if(value < 0) value += 4096; return value; }

static int YawTo(int fromX, int fromZ, int toX, int toZ)
{
    double radians=atan2((double)toX-fromX,(double)toZ-fromZ);
    return WrappedYaw((int)lround(radians*(4096.0/(2.0*3.14159265358979323846))));
}

static int YawError(int a, int b)
{
    int d=WrappedYaw(a)-WrappedYaw(b);
    if(d>2048) d-=4096;
    if(d< -2048) d+=4096;
    return d<0?-d:d;
}

static void Target(ACC_FALL_ROUTE_OUTPUT *out, const FALL_POINT *p)
{
    out->has_target=1; out->target_x=p->x; out->target_y=p->y; out->target_z=p->z;
}

static int FirstLandingZone(const ACC_FALL_ROUTE_INPUT *in)
{
    return InBounds(in->x,in->z,23500,25500,-3800,-1500);
}

static int SecondLandingZone(const ACC_FALL_ROUTE_INPUT *in)
{
    return InBounds(in->x,in->z,39200,41000,7600,10100);
}

static int FirstRunupRegion(const ACC_FALL_ROUTE_INPUT *in)
{ return InBounds(in->x,in->z,14500,21500,-3000,2500); }

static int SecondRunupRegion(const ACC_FALL_ROUTE_INPUT *in)
{ return InBounds(in->x,in->z,39200,41500,-6000,-1800); }

static int OnNorthDeckRoute(const ACC_FALL_ROUTE_INPUT *in)
{
    return FirstLandingZone(in) ||
        SegmentDistance(in,&FirstLanding,&NorthDeckWaypoint)<=1200.0 ||
        SegmentDistance(in,&NorthDeckWaypoint,&NorthEastStaging)<=1200.0;
}

static int FirstFlightCorridor(const ACC_FALL_ROUTE_INPUT *in)
{
    return InBounds(in->x,in->z,15500,27000,-5200,3200) && in->y>=2200 && in->y<=7000;
}

static int SecondFlightCorridor(const ACC_FALL_ROUTE_INPUT *in)
{
    return InBounds(in->x,in->z,39000,41800,-3500,10500) && in->y>=2200 && in->y<=7000;
}

static int Reconcile(const ACC_FALL_ROUTE_INPUT *in)
{
    if(!in->grounded) {
        if(FirstFlightCorridor(in)) return ACC_FALL_PHASE_FIRST_JUMP;
        if(SecondFlightCorridor(in)) return ACC_FALL_PHASE_SECOND_JUMP;
        return ACC_FALL_PHASE_RECOVERY;
    }
    if(in->y<FLOOR_Y_MIN || in->y>FLOOR_Y_MAX) return ACC_FALL_PHASE_RECOVERY;
    if(SecondLandingZone(in) ||
       (in->x>=39200 && in->z>=7600 && in->z<=22000)) return ACC_FALL_PHASE_GATE_WALK;
    if(OnNorthDeckRoute(in)) return ACC_FALL_PHASE_NORTH_DECK;
    if(SecondRunupRegion(in)) return ACC_FALL_PHASE_SECOND_RUNUP;
    if(InBounds(in->x,in->z,14500,19500,-1900,1700)) {
        if(HorizontalDistance(in->x,in->z,FIRST_LAUNCH_X,FIRST_LAUNCH_Z)<=400)
            return ACC_FALL_PHASE_FIRST_RUNUP;
        return ACC_FALL_PHASE_FIRST_APPROACH;
    }
    return ACC_FALL_PHASE_NONE;
}

void AccFallRoute_Reset(ACC_FALL_ROUTE_STATE *state)
{ if(state) { state->phase=ACC_FALL_PHASE_NONE; state->waypoint=0; } }

static void SetRunup(ACC_FALL_ROUTE_OUTPUT *out, const ACC_FALL_ROUTE_INPUT *in,
    const FALL_POINT *staging, const FALL_POINT *launch, const FALL_POINT *landing,
    int minSpeed, int earlyWindow, int lateWindow)
{
    double dx=(double)landing->x-launch->x, dz=(double)landing->z-launch->z;
    double length=hypot(dx,dz), ux=dx/length, uz=dz/length;
    double before=((double)launch->x-in->x)*ux+((double)launch->z-in->z)*uz;
    double lateral=fabs(((double)in->x-launch->x)*uz-((double)in->z-launch->z)*ux);
    double vx=in->velocity_x, vz=in->velocity_z;
    double projectedSpeed=vx*ux+vz*uz;
    double lateralSpeed=fabs(vx*uz-vz*ux);
    int cueEarlyWindow=earlyWindow;
    int maxFrameAdvance=(int)ceil(fmax(0.0,projectedSpeed)*34.0/1000.0);
    int phaseSafeWindow=maxFrameAdvance-lateWindow+50;
    int speed=(int)lround(hypot(vx,vz));
    int yaw=YawTo(launch->x,launch->z,landing->x,landing->z);
    out->desired_yaw=yaw;
    out->horizontal_speed=speed;
    out->projected_run_speed_mm_per_s=(int)lround(projectedSpeed);
    out->lateral_speed_mm_per_s=(int)lround(lateralSpeed);
    out->launch_distance_mm=(int)lround(before);
    out->cross_track_mm=(int)lround(lateral);
    out->min_run_speed_mm_per_s=minSpeed;
    /* At high run speed the fixed manual cue window could be narrower than a
       single 30 Hz sample step. Expand only the early edge, using the measured
       forward speed and a 34 ms frame bound, so every frame phase observes at
       least one actionable grounded sample. */
    if(phaseSafeWindow>cueEarlyWindow) cueEarlyWindow=phaseSafeWindow;
    out->jump_window_start_mm=cueEarlyWindow;
    out->jump_window_end_mm=lateWindow;
    Target(out,launch);
    if(!in->grounded) return;
    if(before < -lateWindow || (before<=0.0 && projectedSpeed<minSpeed) ||
       (before <= cueEarlyWindow && (lateral>500.0 || YawError(in->yaw,yaw)>140 ||
        lateralSpeed>fmax(2200.0,fabs(projectedSpeed)*0.22)))) {
        out->action=ACC_FALL_ACTION_RECOVER;
        Target(out,staging);
        return;
    }
    if(before<=cueEarlyWindow && before>=-lateWindow && projectedSpeed>=minSpeed &&
       lateral<=500.0 && lateralSpeed<=fmax(2200.0,fabs(projectedSpeed)*0.22) &&
       YawError(in->yaw,yaw)<=140) {
        out->action=ACC_FALL_ACTION_JUMP_NOW;
        out->jump_intent=1;
    } else out->action=ACC_FALL_ACTION_RUN_UP;
}

int AccFallRoute_Update(const ACC_FALL_ROUTE_INPUT *in,
    ACC_FALL_ROUTE_STATE *state, ACC_FALL_ROUTE_OUTPUT *out)
{
    int phase;
    if(!state || !out) return 0;
    memset(out,0,sizeof(*out));
    if(!in || !in->level_name || strcmp(in->level_name,"fall") ||
       !in->is_predator || in->room_index!=94 || !in->opening_gate_unlocked) {
        state->phase=ACC_FALL_PHASE_NONE; return 0;
    }
    if(in->grounded && (in->y<FLOOR_Y_MIN || in->y>FLOOR_Y_MAX)) {
        state->phase=ACC_FALL_PHASE_NONE; state->waypoint=0; return 0;
    }
    phase=state->phase;
    if(in->grounded) {
        int inferred=Reconcile(in);
        /* Keep a run-up when still on its known deck, but reloads or stale
           later phases must be reconstructed from this grounded position. */
        if(inferred==ACC_FALL_PHASE_FIRST_APPROACH &&
           phase==ACC_FALL_PHASE_FIRST_RUNUP && FirstRunupRegion(in)) inferred=phase;
        if(inferred==ACC_FALL_PHASE_NONE &&
           phase==ACC_FALL_PHASE_FIRST_RUNUP && FirstRunupRegion(in)) inferred=phase;
        if(inferred==ACC_FALL_PHASE_NORTH_DECK &&
           phase==ACC_FALL_PHASE_SECOND_RUNUP && SecondRunupRegion(in)) inferred=phase;
        if(inferred==ACC_FALL_PHASE_NONE &&
           phase==ACC_FALL_PHASE_SECOND_RUNUP && SecondRunupRegion(in)) inferred=phase;
        if(inferred==ACC_FALL_PHASE_NONE && phase==ACC_FALL_PHASE_NORTH_DECK &&
           OnNorthDeckRoute(in)) inferred=phase;
        if(inferred==ACC_FALL_PHASE_NONE || inferred==ACC_FALL_PHASE_RECOVERY) {
            state->phase=ACC_FALL_PHASE_RECOVERY; state->waypoint=0;
            out->phase=state->phase; out->action=ACC_FALL_ACTION_RECOVER; return 1;
        }
        if(phase!=inferred) state->waypoint=0;
        if(inferred==ACC_FALL_PHASE_NORTH_DECK &&
           (Near(in,&NorthDeckWaypoint,800) || in->x>NorthDeckWaypoint.x+800 ||
            Near(in,&NorthEastStaging,700)))
            state->waypoint=1;
        phase=inferred; state->phase=phase;
    } else if(phase==ACC_FALL_PHASE_NONE || phase==ACC_FALL_PHASE_RECOVERY) {
        phase=Reconcile(in); state->phase=phase;
    }
    out->phase=phase;
    if(phase==ACC_FALL_PHASE_NONE) return 0;
    if(phase==ACC_FALL_PHASE_RECOVERY) {
        out->action=in->grounded?ACC_FALL_ACTION_RECOVER:ACC_FALL_ACTION_FALLING;
        return 1;
    }

    if(phase==ACC_FALL_PHASE_FIRST_APPROACH) {
        if(!in->grounded) { state->phase=ACC_FALL_PHASE_RECOVERY; out->phase=state->phase; out->action=ACC_FALL_ACTION_FALLING; return 1; }
        out->action=ACC_FALL_ACTION_WALK; Target(out,&FirstStaging);
        if(in->grounded && Near(in,&FirstStaging,500)) {
            state->phase=ACC_FALL_PHASE_FIRST_RUNUP;
            out->phase=ACC_FALL_PHASE_FIRST_RUNUP;
            SetRunup(out,in,&FirstStaging,&FirstLaunch,&FirstLanding,11000,350,150);
        }
        return 1;
    }
    if(phase==ACC_FALL_PHASE_FIRST_RUNUP) {
        if(!in->grounded) {
            if(!FirstFlightCorridor(in)) { state->phase=ACC_FALL_PHASE_RECOVERY; out->phase=state->phase; out->action=ACC_FALL_ACTION_FALLING; return 1; }
            state->phase=ACC_FALL_PHASE_FIRST_JUMP; out->phase=state->phase;
            out->action=ACC_FALL_ACTION_LAND; Target(out,&FirstLanding);
            out->landing_x=FirstLanding.x; out->landing_y=FirstLanding.y; out->landing_z=FirstLanding.z;
            return 1;
        }
        SetRunup(out,in,&FirstStaging,&FirstLaunch,&FirstLanding,11000,350,150);
        return 1;
    }
    if(phase==ACC_FALL_PHASE_FIRST_JUMP) {
        out->landing_x=FirstLanding.x; out->landing_y=FirstLanding.y; out->landing_z=FirstLanding.z;
        if(!in->grounded) {
            if(!FirstFlightCorridor(in)) { state->phase=ACC_FALL_PHASE_RECOVERY; out->phase=state->phase; out->action=ACC_FALL_ACTION_FALLING; return 1; }
            out->action=ACC_FALL_ACTION_LAND; Target(out,&FirstLanding); return 1;
        }
        if(!FirstLandingZone(in) || in->y<FLOOR_Y_MIN || in->y>FLOOR_Y_MAX) {
            state->phase=ACC_FALL_PHASE_RECOVERY; out->phase=state->phase;
            out->action=ACC_FALL_ACTION_RECOVER; return 1;
        }
        state->phase=ACC_FALL_PHASE_NORTH_DECK; phase=state->phase; out->phase=phase;
    }
    if(phase==ACC_FALL_PHASE_NORTH_DECK) {
        if(!in->grounded || in->y<FLOOR_Y_MIN || in->y>FLOOR_Y_MAX) {
            out->action=in->grounded?ACC_FALL_ACTION_RECOVER:ACC_FALL_ACTION_FALLING; return 1;
        }
        if(state->waypoint==1 && Near(in,&NorthEastStaging,700)) {
            state->phase=ACC_FALL_PHASE_SECOND_RUNUP; out->phase=state->phase;
            SetRunup(out,in,&NorthEastStaging,&SecondLaunch,&SecondLanding,14000,900,180);
            return 1;
        }
        out->action=ACC_FALL_ACTION_WALK;
        if(Near(in,&NorthDeckWaypoint,800) || in->x>NorthDeckWaypoint.x+800 ||
           Near(in,&NorthEastStaging,700)) state->waypoint=1;
        Target(out,state->waypoint?&NorthEastStaging:&NorthDeckWaypoint);
        return 1;
    }
    if(phase==ACC_FALL_PHASE_SECOND_RUNUP) {
        if(!in->grounded) {
            if(!SecondFlightCorridor(in)) { state->phase=ACC_FALL_PHASE_RECOVERY; out->phase=state->phase; out->action=ACC_FALL_ACTION_FALLING; return 1; }
            state->phase=ACC_FALL_PHASE_SECOND_JUMP; out->phase=state->phase;
            out->action=ACC_FALL_ACTION_LAND; Target(out,&SecondLanding);
            out->landing_x=SecondLanding.x; out->landing_y=SecondLanding.y; out->landing_z=SecondLanding.z;
            return 1;
        }
        SetRunup(out,in,&NorthEastStaging,&SecondLaunch,&SecondLanding,14000,900,180);
        return 1;
    }
    if(phase==ACC_FALL_PHASE_SECOND_JUMP) {
        out->landing_x=SecondLanding.x; out->landing_y=SecondLanding.y; out->landing_z=SecondLanding.z;
        if(!in->grounded) {
            if(!SecondFlightCorridor(in)) { state->phase=ACC_FALL_PHASE_RECOVERY; out->phase=state->phase; out->action=ACC_FALL_ACTION_FALLING; return 1; }
            out->action=ACC_FALL_ACTION_LAND; Target(out,&SecondLanding); return 1;
        }
        if(!SecondLandingZone(in) || in->y<FLOOR_Y_MIN || in->y>FLOOR_Y_MAX) {
            state->phase=ACC_FALL_PHASE_RECOVERY; out->phase=state->phase;
            out->action=ACC_FALL_ACTION_RECOVER; return 1;
        }
        state->phase=ACC_FALL_PHASE_GATE_WALK; phase=state->phase; out->phase=phase;
    }
    if(phase==ACC_FALL_PHASE_GATE_WALK) {
        if(!in->grounded || in->y<FLOOR_Y_MIN || in->y>FLOOR_Y_MAX) {
            out->action=in->grounded?ACC_FALL_ACTION_RECOVER:ACC_FALL_ACTION_FALLING; return 1;
        }
        out->action=ACC_FALL_ACTION_WALK; Target(out,&GateEntry);
        return 1;
    }
    return 0;
}

/* Compile the actual tracker functions and eligibility statements from hud.c. */
#include <stdio.h>
#include <string.h>
#include "hud_source.generated.h"

AVP_GAME_DESC AvP;
DISPLAYBLOCK *Player;
STRATEGYBLOCK *ActiveStBlockList[maxstblocks];
int NumActiveStBlocks, NormalFrameTime, Observer;
enum VISION_MODE_ID CurrentVisionMode;
static DISPLAYBLOCK player_display;
static STRATEGYBLOCK player_strategy, objects[12];
static DYNAMICSBLOCK player_dynamics, dynamics[12];
static PLAYER_STATUS player_status;
static int failures, checks, reset_calls, stop_calls, accepts_controls, menus;
static int beep_calls, beep_id, beep_range, beep_volume, click_calls;
static VECTORCH beep_position;
static ACC_TRACKER_CONTACT published[ACC_TRACKER_MAX_CONTACTS];
static int published_count, published_range;

void AccTracker_Reset(void) { reset_calls++; published_count=0; }
void AccTracker_SetContacts(const ACC_TRACKER_CONTACT *contacts,int count,int range)
{ published_count=count; published_range=range; memcpy(published,contacts,count*sizeof(*contacts)); }
void AccTracker_PlayContact(int sound,const struct vectorch *position,int range,int *handle,int volume)
{ beep_calls++; beep_id=sound; beep_position=*position; beep_range=range; beep_volume=volume; *handle=17; }
void Sound_Stop(int handle) { if(handle==MTSoundHandle) stop_calls++; }
void Sound_Play(SOUNDINDEX sound,char *format,...) { (void)format; if(sound==SID_TRACKER_CLICK) click_calls++; }
void BLTMotionTrackerToHUD(int size) { (void)size; }
void BLTMotionTrackerBlipToHUD(int x,int y,int brightness) { (void)x; (void)y; (void)brightness; }
OurBool IOFOCUS_AcceptControls(void) { return accepts_controls ? Yes : No; }
int InGameMenusAreRunning(void) { return menus; }

static void check(int condition,const char *description)
{ checks++; if(!condition) { failures++; printf("FAIL: %s\n",description); } }

static void setup(void)
{
    AccTracker_ResetHUD();
    memset(&player_display,0,sizeof(player_display));
    memset(&player_strategy,0,sizeof(player_strategy));
    memset(&player_dynamics,0,sizeof(player_dynamics));
    memset(&player_status,0,sizeof(player_status));
    memset(objects,0,sizeof(objects)); memset(dynamics,0,sizeof(dynamics));
    Player=&player_display; Player->ObStrategyBlock=&player_strategy;
    player_strategy.DynPtr=&player_dynamics; player_strategy.SBdataptr=&player_status;
    NumActiveStBlocks=0; NormalFrameTime=0;
    AvP.PlayerType=I_Marine; AvP.LevelCompleted=0;
    player_status.IsAlive=1; CurrentVisionMode=VISION_MODE_NORMAL;
    Observer=0; menus=0; accepts_controls=1;
    reset_calls=stop_calls=beep_calls=click_calls=0;
    MTScanLineSize=ONE_FIXED; PreviousMTScanLineSize=0;
}

static STRATEGYBLOCK *add_object(int x,int z)
{
    int i=NumActiveStBlocks++;
    objects[i].I_SBtype=I_BehaviourMarine; objects[i].DynPtr=&dynamics[i];
    dynamics[i].Position.vx=x; dynamics[i].Position.vz=z;
    dynamics[i].PrevPosition=dynamics[i].Position;
    ActiveStBlockList[i]=&objects[i]; return &objects[i];
}

static int detect(void) { return DoMotionTrackerBlips(NULL); }

static void detection_tests(void)
{
    VECTORCH position={-1,-1,-1}; STRATEGYBLOCK *object;
    setup(); add_object(-3000,6000); add_object(4000,10000);
    check(DoMotionTrackerBlips(&position)==7000 && position.vx==-3000 && position.vz==6000 && NoOfMTBlips==2,
          "nearest swept contact copies exact world coordinates");
    setup(); add_object(-3000,6000); add_object(3000,6000);
    detect(); NoOfMTBlips=0; DoMotionTrackerBlips(&position);
    check(position.vx==3000,"equal-distance tie preserves reverse traversal and strict comparison");
    setup(); add_object(0,-10000); check(detect()==MOTIONTRACKER_RANGE && !NoOfMTBlips,"behind is excluded");
    setup(); add_object(0,30001); check(detect()==MOTIONTRACKER_RANGE && !NoOfMTBlips,"beyond range is excluded");
    setup(); add_object(24000,24000); check(detect()==MOTIONTRACKER_RANGE && !NoOfMTBlips,"outside sweep radius is excluded inside bounding box");
    setup(); add_object(10000,0); player_dynamics.OrientEuler.EulerY=3072;
    check(detect()==MOTIONTRACKER_RANGE,"player yaw rotates front-half eligibility");
    setup(); add_object(0,10000); MTScanLineSize=20000;
    check(detect()==MOTIONTRACKER_RANGE,"unswept contact remains undetected");
    setup(); add_object(0,10000); PreviousMTScanLineSize=30000;
    check(detect()==MOTIONTRACKER_RANGE,"already passed stationary contact is not detected again");
    dynamics[0].PrevPosition.vz=20000;
    check(detect()==10000,"moving inward across previous scan is detected");
    setup(); add_object(0,10000); NoOfMTBlips=MOTIONTRACKER_MAXBLIPS;
    check(detect()==MOTIONTRACKER_RANGE && NoOfMTBlips==MOTIONTRACKER_MAXBLIPS,"full blip capacity prevents additional detection");
    setup(); object=add_object(0,10000); object->DynPtr->IsStatic=1;
    check(detect()==MOTIONTRACKER_RANGE,"static object is excluded");
    object->DynPtr->IsStatic=0; object->SBflags.not_on_motiontracker=1;
    check(detect()==MOTIONTRACKER_RANGE,"explicit tracker exclusion is respected");
    object->SBflags.not_on_motiontracker=0; object->I_SBtype=I_BehaviourInanimateObject;
    check(detect()==MOTIONTRACKER_RANGE,"inanimate object is excluded");
    object->I_SBtype=I_BehaviourRubberDuck;
    check(detect()==MOTIONTRACKER_RANGE,"rubber duck is excluded");
    object->I_SBtype=I_BehaviourPlatform;
    check(detect()==MOTIONTRACKER_RANGE,"stationary platform is excluded");
    object->DynPtr->PrevPosition.vz=9999;
    check(detect()==10000,"moving platform remains detectable");
    {
        ALIEN_STATUS_BLOCK alien; NETGHOSTDATABLOCK ghost; PROXDOOR_BEHAV_BLOCK door;
        memset(&alien,0,sizeof(alien)); memset(&ghost,0,sizeof(ghost)); memset(&door,0,sizeof(door));
        setup(); object=add_object(0,10000); object->I_SBtype=I_BehaviourAlien;
        object->SBdataptr=&alien; alien.BehaviourState=ABS_Dormant;
        check(detect()==MOTIONTRACKER_RANGE,"dormant alien is excluded");
        object->I_SBtype=I_BehaviourNetGhost; object->SBdataptr=&ghost;
        ghost.type=I_BehaviourAlienPlayer;
        check(detect()==MOTIONTRACKER_RANGE,"stationary alien ghost is excluded");
        object->DynPtr->PrevPosition.vz=9999; object->DynPtr->IsStatic=1; object->DynPtr->IsNetGhost=1;
        check(detect()==10000,"moving ghost preserves static-ghost exception");
        NoOfMTBlips=0; object->DynPtr->IsStatic=0; object->I_SBtype=I_BehaviourProximityDoor; object->SBdataptr=&door;
        door.door_state=I_door_open; check(detect()==MOTIONTRACKER_RANGE,"open door is excluded");
        door.door_state=I_door_closed; check(detect()==MOTIONTRACKER_RANGE,"closed door is excluded");
    }
}

static void sweep_and_reset_tests(void)
{
    int distances[]={5000,15000,25000};
    int sounds[]={SID_TRACKER_WHEEP_HIGH,SID_TRACKER_WHEEP,SID_TRACKER_WHEEP_LOW};
    int i;
    for(i=0;i<3;i++) {
        setup(); add_object(-3000,distances[i]); DoMotionTracker();
        check(beep_calls==1 && beep_id==sounds[i] && beep_position.vx==-3000 && beep_position.vz==distances[i]
              && beep_range==MOTIONTRACKER_RANGE && beep_volume==MOTIONTRACKERVOLUME && MTSoundHandle==17,
              "sweep beep retains sound band, volume, handle and selected position");
        check(published_count==1 && published[0].x==-3000 && published[0].z==distances[i] && published_range==MOTIONTRACKER_RANGE,
              "post-fade contacts publish copied world coordinates");
        MTSoundHandle=SOUND_NOACTIVEINDEX; DoMotionTracker();
        check(beep_calls==1,"locked sweep does not emit another beep after sound completion");
    }
    setup(); add_object(0,5000); MTSoundHandle=17; DoMotionTracker();
    check(!beep_calls && !MTDistanceNotLocked,"active sound suppresses new beep while distance still locks");
    setup(); MTDistanceNotLocked=0; MTDelayBetweenScans=1; NormalFrameTime=2; DoMotionTracker();
    check(click_calls==1 && MTDistanceNotLocked && MTScanLineSize==MOTIONTRACKER_SMALLESTSCANLINESIZE,
          "scan reset retains click and re-arms distance lock");
    setup(); NoOfMTBlips=2; MotionTrackerBlips[0].Brightness=0;
    MotionTrackerBlips[1].Brightness=ONE_FIXED; MotionTrackerBlips[1].X=21; MotionTrackerBlips[1].Y=42;
    NormalFrameTime=100; DoMotionTracker();
    check(published_count==1 && published[0].x==21 && published[0].z==42,"expired blip is removed before snapshot publication");
    MTSoundHandle=17; MTDistance=123; MTDistanceNotLocked=0; MTDelayBetweenScans=99;
    AccTracker_ResetHUD();
    check(reset_calls==1 && stop_calls==1 && MTSoundHandle==SOUND_NOACTIVEINDEX && !published_count && !NoOfMTBlips
          && MTScanLineSize==MOTIONTRACKER_SMALLESTSCANLINESIZE && PreviousMTScanLineSize==MTScanLineSize
          && !MTDelayBetweenScans && MTDistanceNotLocked && !MTDistance,
          "reset stops beep and clears cache, blips, delay, radius and locked distance");
    AccTracker_ResetHUD(); check(stop_calls==1,"repeated reset never stops an inactive handle");
}

static void eligibility_tests(void)
{
    setup(); check(TestHUDTrackerEligibility(&player_status) && !reset_calls,"ordinary Marine gameplay keeps tracker active");
#define INELIGIBLE(setup_statement,description) do { setup(); setup_statement; MTSoundHandle=17; NoOfMTBlips=1; published_count=1; \
    check(!TestHUDTrackerEligibility(&player_status) && reset_calls==1 && stop_calls==1 && !published_count && !NoOfMTBlips,description); } while(0)
    INELIGIBLE(menus=1,"menus suppress and reset tracker, including live multiplayer menus");
    INELIGIBLE(accepts_controls=0,"console/input focus suppresses and resets tracker");
    INELIGIBLE(player_status.IsAlive=0,"death suppresses and resets tracker");
    INELIGIBLE(player_status.DemoMode=1,"demo playback suppresses and resets tracker");
    INELIGIBLE(AvP.LevelCompleted=1,"completed level suppresses and resets tracker");
    INELIGIBLE(Observer=1,"observer suppresses and resets tracker");
    INELIGIBLE(player_status.MyFaceHugger=&objects[0],"facehugger suppresses and resets tracker");
    INELIGIBLE(AvP.PlayerType=I_Alien,"other species suppresses and resets tracker");
    INELIGIBLE(CurrentVisionMode=(enum VISION_MODE_ID)(VISION_MODE_NORMAL+1),"other vision suppresses and resets tracker");
#undef INELIGIBLE
}

int main(void)
{
    detection_tests(); sweep_and_reset_tests(); eligibility_tests();
    printf("Tracker HUD: %d checks, %d failures\n",checks,failures);
    return failures ? 1 : 0;
}

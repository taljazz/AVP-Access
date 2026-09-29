#include "3dc.h"
#include "module.h"
#include "stratdef.h"
#include "dynblock.h"
#include "gamedef.h"
#include "pfarlocs.h"
#include "pvisible.h"
#include "pheromon.h"
#include "bh_types.h"
#include "bh_binsw.h"
#include "bh_lnksw.h"
#include "bh_swdor.h"
#include "bh_ldoor.h"
#include "bh_plift.h"
#include "acc_bridge_core.h"
#include "acc_map.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern char LevelName[];
extern DISPLAYBLOCK *Player;
extern STRATEGYBLOCK *ActiveStBlockList[];
extern int NumActiveStBlocks;

static void String(FILE *f, const char *s)
{
    char escaped[2048];
    AccBridge_JsonEscape(s ? s : "", escaped, sizeof(escaped));
    fprintf(f, "\"%s\"", escaped);
}
static void Vector(FILE *f, const VECTORCH *v)
{ fprintf(f,"[%d,%d,%d]",v->vx,v->vy,v->vz); }
static int ObjectID(const STRATEGYBLOCK *s)
{
    int i;
    if (s) for(i=0;i<NumActiveStBlocks;++i) if(ActiveStBlockList[i]==s) return i;
    return -1;
}
static int IsMeshPolygon(int t)
{
    switch(t) {
    case I_Polygon: case I_GouraudPolygon: case I_PhongPolygon:
    case I_2dTexturedPolygon: case I_Gouraud2dTexturedPolygon: case I_3dTexturedPolygon:
    case I_CloakedPolygon: case I_ZB_Polygon: case I_ZB_GouraudPolygon:
    case I_ZB_PhongPolygon: case I_ZB_2dTexturedPolygon: case I_ZB_Gouraud2dTexturedPolygon:
    case I_ZB_3dTexturedPolygon: case I_Gouraud3dTexturedPolygon: case I_ZB_Gouraud3dTexturedPolygon:
        return 1;
    default: return 0;
    }
}
/* Base mesh, never a claimed snapshot of animated/morphed collision. Vertices
   remain local; the owner carries translation and Euler rotation explicitly. */
static void Mesh(FILE *f,int index)
{
    SHAPEHEADER *s=index>=0 ? GetShapeData(index) : NULL;
    int i,first=1,skipped=0;
    if(!s || !s->points || !*s->points || !s->items || s->numpoints<0 ||
       s->numpoints>1000000 || s->numitems<0 || s->numitems>1000000) {
        fputs("null",f); return;
    }
    fprintf(f,"{\"shape\":%d,\"vertices\":[",index);
    for(i=0;i<s->numpoints;++i) {
        if(i) fputc(',',f);
        Vector(f,((VECTORCH*)*s->points)+i);
    }
    fputs("],\"faces\":[",f);
    for(i=0;i<s->numitems;++i) {
        POLYHEADER *p=(POLYHEADER*)s->items[i];
        int *v,n,k,valid=1;
        if(!p || !IsMeshPolygon(p->PolyItemType)) { ++skipped; continue; }
        v=&p->Poly1stPt; n=v[3]==Term ? 3:4;
        for(k=0;k<n;++k) if(v[k]<0 || v[k]>=s->numpoints) valid=0;
        if(!valid) { ++skipped; continue; }
        fprintf(f,"%s{\"flags\":%d,\"indices\":[",first?"":",",p->PolyFlags); first=0;
        for(k=0;k<n;++k) fprintf(f,"%s%d",k?",":"",v[k]);
        fputs("]}",f);
    }
    fprintf(f,"],\"skipped_items\":%d}",skipped);
}
static void Trigger(FILE *f,int flags,const VECTORCH *lo,const VECTORCH *hi)
{
    fprintf(f,",\"switch_flags\":%d,\"trigger_volume\":",flags);
    if(flags&SwitchFlag_UseTriggerVolume) {
        fputc('[',f); Vector(f,lo); fputc(',',f); Vector(f,hi); fputc(']',f);
    } else fputs("null",f);
}
static void Object(FILE *f,STRATEGYBLOCK *s,int id)
{
    int k; char name[SB_NAME_LENGTH+1];
    memcpy(name,s->SBname,SB_NAME_LENGTH); name[SB_NAME_LENGTH]=0;
    fprintf(f,"{\"id\":%d,\"name\":",id); String(f,name);
    fprintf(f,",\"type\":%d,\"destroy_pending\":%d,\"module\":%d,\"position\":",
        s->I_SBtype,s->SBflags.please_destroy_me,s->containingModule?s->containingModule->m_index:-1);
    if(s->DynPtr) Vector(f,&s->DynPtr->Position); else fputs("null",f);
    if(s->DynPtr) fprintf(f,",\"euler\":[%d,%d,%d]",s->DynPtr->OrientEuler.EulerX,
        s->DynPtr->OrientEuler.EulerY,s->DynPtr->OrientEuler.EulerZ);
    fprintf(f,",\"shape\":%d",s->shapeIndex);
    if(s->DynPtr && (s->DynPtr->IsStatic || s->I_SBtype==I_BehaviourInanimateObject ||
       s->I_SBtype==I_BehaviourBinarySwitch || s->I_SBtype==I_BehaviourLinkSwitch)) {
        fputs(",\"base_mesh\":",f); Mesh(f,s->shapeIndex);
    }
    if(s->SBdataptr && s->I_SBtype==I_BehaviourBinarySwitch) {
        BINARY_SWITCH_BEHAV_BLOCK *b=s->SBdataptr;
        fprintf(f,",\"kind\":\"binary_switch\",\"state\":%d,\"mode\":%d,\"security\":%d",
            b->state,b->bs_mode,b->security_clerance);
        Trigger(f,b->switch_flags,&b->trigger_volume_min,&b->trigger_volume_max);
        fputs(",\"targets\":[",f);
        if(b->bs_targets && b->request_messages && b->num_targets>=0 && b->num_targets<=100000)
            for(k=0;k<b->num_targets;++k) fprintf(f,"%s{\"id\":%d,\"request\":%d}",k?",":"",
                ObjectID(b->bs_targets[k]),b->request_messages[k]);
        fputc(']',f);
    } else if(s->SBdataptr && s->I_SBtype==I_BehaviourLinkSwitch) {
        LINK_SWITCH_BEHAV_BLOCK *l=s->SBdataptr;
        fprintf(f,",\"kind\":\"link_switch\",\"state\":%d,\"system_state\":%d,\"mode\":%d,\"security\":%d",
            l->state,l->system_state,l->ls_mode,l->security_clerance);
        Trigger(f,l->switch_flags,&l->trigger_volume_min,&l->trigger_volume_max);
        fputs(",\"targets\":[",f);
        if(l->ls_targets && l->num_targets>=0 && l->num_targets<=100000)
            for(k=0;k<l->num_targets;++k) fprintf(f,"%s{\"id\":%d,\"request\":%d}",k?",":"",
                ObjectID(l->ls_targets[k].sbptr),l->ls_targets[k].request_message);
        fputs("],\"prerequisites\":[",f);
        if(l->lswitch_list && l->num_linked_switches>=0 && l->num_linked_switches<=100000)
            for(k=0;k<l->num_linked_switches;++k) fprintf(f,"%s%d",k?",":"",ObjectID(l->lswitch_list[k].bswitch));
        fputc(']',f);
    } else if(s->SBdataptr && s->I_SBtype==I_BehaviourSwitchDoor) {
        SWITCH_DOOR_BEHAV_BLOCK *d=s->SBdataptr;
        fprintf(f,",\"kind\":\"switch_door\",\"state\":%d,\"request_open\":%d,\"request_close\":%d",
            d->doorState,d->requestOpen,d->requestClose);
    } else if(s->SBdataptr && s->I_SBtype==I_BehaviourPlatform) {
        PLATFORMLIFT_BEHAVIOUR_BLOCK *l=s->SBdataptr;
        fprintf(f,",\"kind\":\"platform_lift\",\"state\":%d,\"enabled\":%d,\"one_use\":%d,\"up_y\":%d,\"down_y\":%d",
            l->state,l->Enabled,l->OneUse,l->upHeight,l->downHeight);
    } else if(s->SBdataptr && s->I_SBtype==I_BehaviourProximityDoor) {
        PROXDOOR_BEHAV_BLOCK *d=s->SBdataptr;
        fprintf(f,",\"kind\":\"proximity_door\",\"state\":%d,\"locked\":%d,\"lock_target\":%d,\"marine_trigger\":%d,\"alien_trigger\":%d",
            d->door_state,d->door_locked,ObjectID(d->door_lock_target),d->marineTrigger,d->alienTrigger);
    } else if(s->SBdataptr && s->I_SBtype==I_BehaviourLiftDoor) {
        LIFT_DOOR_BEHAV_BLOCK *d=s->SBdataptr;
        fprintf(f,",\"kind\":\"lift_door\",\"state\":%d",d->door_state);
    }
    fputc('}',f);
}
int AccMap_Export(const char *path,char *error,size_t errorSize)
{
    FILE *f; int i,j,first; char temporary[1024];
    if(errorSize) error[0]=0;
    if(!path || !Player || !AIModuleArray || AIModuleArraySize<=0 || AIModuleArraySize>65536) {
        snprintf(error,errorSize,"no loaded level to export"); return 0;
    }
    if(snprintf(temporary,sizeof(temporary),"%s.tmp",path)>=(int)sizeof(temporary)) {
        snprintf(error,errorSize,"map path too long"); return 0;
    }
    f=fopen(temporary,"wb");
    if(!f) { snprintf(error,errorSize,"cannot create map export"); return 0; }
    fputs("{\"schema\":1,\"units\":\"millimetres\",\"y_axis\":\"down\",\"euler_turn\":4096,\"geometry\":\"base meshes in local coordinates; animated collision not resolved\",\"level\":",f);
    String(f,LevelName);
    fprintf(f,",\"species\":%d,\"player\":",AvP.PlayerType);
    if(Player->ObStrategyBlock && Player->ObStrategyBlock->DynPtr) Vector(f,&Player->ObStrategyBlock->DynPtr->Position);
    else fputs("null",f);
    fputs(",\"rooms\":[",f);
    for(i=0;i<AIModuleArraySize;++i) {
        AIMODULE *a=&AIModuleArray[i];
        fprintf(f,"%s{\"id\":%d,\"position\":",i?",":"",i); Vector(f,&a->m_world);
        fprintf(f,",\"physical\":%d,\"ai_passable_now\":%d,\"links\":[",AIModuleIsPhysical(a),
            a->m_module_ptrs && *a->m_module_ptrs ? AIModuleAdmitsPheromones(a):0);
        first=1;
        if(a->m_link_ptrs) for(j=0;j<65536 && a->m_link_ptrs[j];++j) {
            AIMODULE *b=a->m_link_ptrs[j];
            FARENTRYPOINT *ep=FALLP_EntryPoints?GetAIModuleEP(b,a):NULL;
            fprintf(f,"%s{\"to\":%d,\"entry\":",first?"":",",b->m_index); first=0;
            if(ep) { VECTORCH v=ep->position; v.vx+=b->m_world.vx; v.vy+=b->m_world.vy; v.vz+=b->m_world.vz; Vector(f,&v); }
            else fputs("null",f);
            fprintf(f,",\"alien_only\":%d}",ep?ep->alien_only:-1);
        }
        fputs("],\"waypoints\":[",f);
        if(a->m_waypoints && a->m_waypoints->first_waypoint && a->m_waypoints->num_waypoints<=65536)
            for(j=0;j<a->m_waypoints->num_waypoints;++j) {
                WAYPOINT_VOLUME *w=&a->m_waypoints->first_waypoint[j]; int k;
                fprintf(f,"%s{\"id\":%d,\"centre_local\":",j?",":"",j); Vector(f,&w->centre);
                fputs(",\"bounds_relative\":[",f); Vector(f,&w->min_extents); fputc(',',f); Vector(f,&w->max_extents);
                fprintf(f,"],\"flags\":%d,\"links\":[",w->flags);
                if(w->first_link && w->num_links<=65536) for(k=0;k<w->num_links;++k)
                    fprintf(f,"%s{\"to\":%d,\"flags\":%d}",k?",":"",w->first_link[k].link_target_index,w->first_link[k].link_flags);
                fputs("]}",f);
            }
        fputs("],\"modules\":[",f);
        if(a->m_module_ptrs) for(j=0;j<65536 && a->m_module_ptrs[j];++j) {
            MODULE *m=a->m_module_ptrs[j]; MODULEMAPBLOCK *map=m->m_mapptr;
            fprintf(f,"%s{\"id\":%d,\"name\":",j?",":"",m->m_index); String(f,m->name);
            fputs(",\"position\":",f); Vector(f,&m->m_world);
            fprintf(f,",\"bounds_local\":[[%d,%d,%d],[%d,%d,%d]],\"flags\":%u,\"object\":%d,\"mesh_position\":",
                m->m_minx,m->m_miny,m->m_minz,m->m_maxx,m->m_maxy,m->m_maxz,(unsigned)m->m_flags,ObjectID(m->m_sbptr));
            Vector(f,map?&map->MapWorld:&m->m_world);
            fprintf(f,",\"mesh_euler\":[%d,%d,%d],\"mesh\":",map?map->MapEuler.EulerX:0,map?map->MapEuler.EulerY:0,map?map->MapEuler.EulerZ:0);
            Mesh(f,map?map->MapShape:-1); fputc('}',f);
        }
        fputs("]}",f);
    }
    fputs("],\"objects\":[",f); first=1;
    for(i=0;i<NumActiveStBlocks;++i) if(ActiveStBlockList[i]) {
        if(!first) fputc(',',f); first=0; Object(f,ActiveStBlockList[i],i);
    }
    fputs("]}\n",f);
    i=ferror(f); if(fclose(f)) i=1;
    if(i || !SDL_RenamePath(temporary,path)) {
        remove(temporary); snprintf(error,errorSize,"cannot publish map export"); return 0;
    }
    return 1;
}

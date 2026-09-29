#include "3dc.h"
#include "module.h"
#include "acc_wayroute.h"
#include <float.h>
#include <math.h>
#include <stdlib.h>

static double AxisDistance(double p,double lo,double hi)
{ return p<lo ? lo-p : p>hi ? p-hi : 0; }
static double Distance(const WAYPOINT_VOLUME *w,const VECTORCH *p,const VECTORCH *origin,int body)
{
    double lo[3],hi[3],v[3],d=0; int i;
    lo[0]=(double)w->centre.vx+w->min_extents.vx;
    lo[1]=(double)w->centre.vy+w->min_extents.vy;
    lo[2]=(double)w->centre.vz+w->min_extents.vz;
    hi[0]=(double)w->centre.vx+w->max_extents.vx;
    hi[1]=(double)w->centre.vy+w->max_extents.vy;
    hi[2]=(double)w->centre.vz+w->max_extents.vz;
    v[0]=(double)p->vx-origin->vx; v[1]=(double)p->vy-origin->vy; v[2]=(double)p->vz-origin->vz;
    for(i=0;i<3;++i) {
        double a;
        if(lo[i]>hi[i]) return DBL_MAX;
        /* Require room for a Marine's width before advancing past a corner. */
        if(body && i!=1 && hi[i]-lo[i]>900) { lo[i]+=450; hi[i]-=450; }
        a=AxisDistance(v[i],lo[i],hi[i]); d+=a*a;
    }
    return d;
}
static int Neighbours(const WAYPOINT_VOLUME *a,const WAYPOINT_VOLUME *b)
{
    double dx=fmax(0,fmax((double)a->centre.vx+a->min_extents.vx-b->centre.vx-b->max_extents.vx,
                         (double)b->centre.vx+b->min_extents.vx-a->centre.vx-a->max_extents.vx));
    double dz=fmax(0,fmax((double)a->centre.vz+a->min_extents.vz-b->centre.vz-b->max_extents.vz,
                         (double)b->centre.vz+b->min_extents.vz-a->centre.vz-a->max_extents.vz));
    /* Bounds overlap or meet, and centres belong to the same height band.
       This only nominates an edge; the collision/floor probe must approve it. */
    return dx<=100 && dz<=100 && abs(a->centre.vy-b->centre.vy)<=1000 &&
        a->min_extents.vy<=0 && a->max_extents.vy>=0 && b->min_extents.vy<=0 && b->max_extents.vy>=0;
}
int AccWayRoute_FindWithProbe(AIMODULE *room,const VECTORCH *body,const VECTORCH *target,int alien,VECTORCH *out,ACC_WAYROUTE_PROBE probe)
{
    WAYPOINT_HEADER *h=room?room->m_waypoints:NULL;
    int *queue,*parent,head=0,tail=0,i,found=-1,result=0;
    double startBest=DBL_MAX,endBest=DBL_MAX;
    if(!body || !target || !out || !h || !h->first_waypoint || h->num_waypoints<=0 || h->num_waypoints>65536) return 0;
    for(i=0;i<h->num_waypoints;++i) {
        double a=Distance(&h->first_waypoint[i],body,&room->m_world,1);
        double b=Distance(&h->first_waypoint[i],target,&room->m_world,0);
        if(a<startBest) startBest=a;
        if(b<endBest) endBest=b;
    }
    /* Do not snap to a different floor or invent a long connector to a graph. */
    if(startBest>500.0*500 || endBest>1200.0*1200) return 0;
    queue=malloc(sizeof(int)*h->num_waypoints); parent=malloc(sizeof(int)*h->num_waypoints);
    if(!queue || !parent) { free(queue); free(parent); return 0; }
    for(i=0;i<h->num_waypoints;++i) {
        parent[i]=-1;
        if(Distance(&h->first_waypoint[i],body,&room->m_world,1)<=startBest+1) {
            parent[i]=i; queue[tail++]=i;
        }
    }
    while(head<tail) {
        int at=queue[head++],k; WAYPOINT_VOLUME *w=&h->first_waypoint[at];
        if(Distance(w,target,&room->m_world,0)<=endBest+1) { found=at; break; }
        if(w->num_links<0 || w->num_links>65536) continue;
        for(k=0;w->first_link && k<w->num_links;++k) {
            WAYPOINT_LINK *link=&w->first_link[k]; int next=link->link_target_index;
            /* Forward search: reversed_oneway forbids this direction. */
            if((link->link_flags&linkflag_reversed_oneway) ||
               (!alien && (link->link_flags&linkflag_alienonly)) ||
               next<0 || next>=h->num_waypoints || parent[next]>=0) continue;
            parent[next]=at; queue[tail++]=next;
        }
        if(probe) for(k=0;k<h->num_waypoints;++k) {
            int e,forbidden=0; VECTORCH a,b;
            if(parent[k]>=0 || !Neighbours(w,&h->first_waypoint[k])) continue;
            for(e=0;w->first_link && e<w->num_links;++e)
                if(w->first_link[e].link_target_index==k &&
                   ((w->first_link[e].link_flags&linkflag_reversed_oneway) ||
                    (!alien && (w->first_link[e].link_flags&linkflag_alienonly)))) forbidden=1;
            if(forbidden) continue;
            a=w->centre; b=h->first_waypoint[k].centre;
            a.vx+=room->m_world.vx; a.vy+=room->m_world.vy; a.vz+=room->m_world.vz;
            b.vx+=room->m_world.vx; b.vy+=room->m_world.vy; b.vz+=room->m_world.vz;
            if(probe(&a,&b)) { parent[k]=at; queue[tail++]=k; }
        }
    }
    if(found<0) result=-1;
    else if(parent[found]==found) { *out=*target; result=1; }
    else {
        int next=found;
        while(parent[parent[next]]!=parent[next]) next=parent[next];
        *out=h->first_waypoint[next].centre;
        out->vx+=room->m_world.vx; out->vy+=room->m_world.vy; out->vz+=room->m_world.vz;
        if(probe) {
            WAYPOINT_VOLUME *w=&h->first_waypoint[next];
            VECTORCH sample=*out,adjacentPoint; double best=DBL_MAX; int x,z;
            /* These are NPC volumes, not a player navmesh. Their centres can
               lie inside scenery at standing height. Choose a checked standing
               point inside the volume rather than insisting on that centre. */
            sample.vy=body->vy; adjacentPoint=sample; adjacentPoint.vx+=1;
            if(body->vy>=room->m_world.vy+w->centre.vy+w->min_extents.vy &&
               body->vy<=room->m_world.vy+w->centre.vy+w->max_extents.vy && !probe(&sample,&adjacentPoint)) {
                double loX=(double)out->vx+w->min_extents.vx+450,hiX=(double)out->vx+w->max_extents.vx-450;
                double loZ=(double)out->vz+w->min_extents.vz+450,hiZ=(double)out->vz+w->max_extents.vz-450;
                VECTORCH chosen=*out;
                if(loX<=hiX && loZ<=hiZ) for(x=0;x<7;++x) for(z=0;z<7;++z) {
                    double score;
                    sample.vx=(int)(loX+(hiX-loX)*x/6); sample.vz=(int)(loZ+(hiZ-loZ)*z/6);
                    adjacentPoint=sample; adjacentPoint.vx+=1;
                    score=hypot((double)body->vx-sample.vx,(double)body->vz-sample.vz);
                    if(score<best && probe(&sample,&adjacentPoint)) { best=score; chosen=sample; }
                }
                if(best<DBL_MAX) *out=chosen;
            }
        }
        result=2;
    }
    free(queue); free(parent); return result;
}
int AccWayRoute_Find(AIMODULE *room,const VECTORCH *body,const VECTORCH *target,int alien,VECTORCH *out)
{ return AccWayRoute_FindWithProbe(room,body,target,alien,out,NULL); }

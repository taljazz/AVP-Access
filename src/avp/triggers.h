extern void OperateObjectInLineOfSight(void);
/* Read-only selection using precisely the same range, alignment and visibility
   checks as the Operate action. Does not activate the returned object. */
extern DISPLAYBLOCK *GetOperableObjectInLineOfSight(void);
/* 0 = clear, 1 = obstructed, 2 = non-explosive breakable scenery in the way.
   Read-only; callers still use the selector above to determine readiness. */
extern int GetInteractionObstruction(DISPLAYBLOCK *control);
extern BOOL AnythingInMyModule(MODULE* my_mod);

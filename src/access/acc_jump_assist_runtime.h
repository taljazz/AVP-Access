#ifndef ACC_JUMP_ASSIST_RUNTIME_H
#define ACC_JUMP_ASSIST_RUNTIME_H

/* True while the explicitly started Predator jump assist owns this input
 * frame's movement. Route automation should pause automatic combat steering. */
int AccJumpAssist_IsActive(void);
/* Reset volatile input ownership at test/setup boundaries. */
void AccJumpAssist_ResetRuntime(void);

#endif

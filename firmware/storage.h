#ifndef ROBOT_STORAGE_H
#define ROBOT_STORAGE_H
#include "robot_config.h"

bool storage_load(RobotConfig *config);
/* -1=error, 0=unchanged (no flash write), 1=saved. Call only while idle. */
int storage_save(const RobotConfig *config);
bool storage_erase(void);
bool storage_present(void);
#endif

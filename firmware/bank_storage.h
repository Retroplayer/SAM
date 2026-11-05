#ifndef ROBOT_BANK_STORAGE_H
#define ROBOT_BANK_STORAGE_H
#include "robot_bank.h"
bool bank_storage_load(RobotBank *bank, bool *migrated);
int bank_storage_save(const RobotBank *bank);
bool bank_storage_erase(void);
#endif

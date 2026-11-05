#include "robot_bank.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static RobotBank bank, loaded;
static uint8_t a[ROBOT_BANK_RECORD_SIZE], b[ROBOT_BANK_RECORD_SIZE],
    bad[ROBOT_BANK_RECORD_SIZE];
int main(void) {
  robot_bank_defaults(&bank);
  assert(robot_bank_valid(&bank));
  assert(bank.baud == 9600 && bank.startup == 1);
  bank.volume = 350;
  bank.baud = 1200;
  bank.framing = 1;
  for (unsigned i = 0; i < 64; i++) {
    char term[16];
    snprintf(term, sizeof(term), "TERM%u", i);
    assert(robot_bank_rule(&bank, term, "test pronunciation"));
  }
  assert(!robot_bank_rule(&bank, "EXTRA", "fail"));
  assert(robot_bank_rule(&bank, "term63", "updated"));
  assert(robot_bank_encode(&bank, 1, a));
  uint32_t seq = 0;
  assert(robot_bank_decode(a, &loaded, &seq) && seq == 1);
  assert(!memcmp(&bank, &loaded, sizeof(bank)));
  bank.volume = 200;
  assert(robot_bank_encode(&bank, 2, b));
  assert(robot_bank_select(a, b, &loaded, &seq) == 1 && seq == 2);
  for (unsigned cut = 0; cut <= sizeof(b); cut += 256) {
    memset(bad, 0xff, sizeof(bad));
    memcpy(bad, b, cut);
    assert(robot_bank_select(a, bad, NULL, &seq) == (cut == sizeof(b) ? 1 : 0));
  }
  for (unsigned offset = 0; offset < sizeof(b); offset++) {
    memcpy(bad, b, sizeof(b));
    bad[offset] ^= 1;
    seq = 99;
    assert(!robot_bank_decode(bad, NULL, &seq));
    assert(seq == 99);
  }
  assert(robot_bank_encode(&bank, UINT32_MAX, a));
  assert(robot_bank_encode(&bank, 0, b));
  assert(robot_bank_select(a, b, NULL, &seq) == 1 && seq == 0);
  assert(robot_bank_delete_rule(&bank, "TERM0"));
  assert(bank.rule_count == 63 && !strcmp(bank.rules[0].term, "TERM1"));
  assert(!robot_bank_rule(&bank, "bad!", "word"));
  assert(!robot_bank_rule(&bank, "bad", "symbols123"));
  assert(!robot_bank_rule(&bank, "blank", "   "));
  RobotConfig legacy = {0};
  robot_init(&legacy.robot);
  legacy.volume = 200;
  legacy.autoload = true;
  robot_set(&legacy.robot, "PITCH", 93);
  robot_pronounce(&legacy.robot, "operational", "opperational");
  robot_bank_migrate(&legacy, &bank);
  assert(bank.baud == 115200 && bank.volume == 200 && bank.rule_count == 1 &&
         bank.characters[1].voice.pitch == 93);
  assert(!strcmp(bank.characters[1].name, "ROBOT"));
  assert(robot_bank_valid(&bank));
  RobotPlan plan;
  assert(!robot_plan_with_rules(&legacy.robot, bank.rules, bank.rule_count,
                                "operational", false, &plan));
  assert(strstr(plan.segments[0].text, "opperational"));
  bank.startup = 7;
  assert(!robot_bank_encode(&bank, 3, a));
  bank.startup = 1;
  bank.framing = 2;
  assert(!robot_bank_encode(&bank, 3, a));
  bank.framing = 0;
  bank.baud = 12345;
  assert(!robot_bank_encode(&bank, 3, a));
  bank.baud = 9600;
  bank.rule_count = 65;
  assert(!robot_bank_encode(&bank, 3, a));
  puts("Bank checks passed: 8 slots/64 rules, serialized round trip, page-cut "
       "recovery, 6144 corruptions, sequence rollover, legacy migration, "
       "planner dictionary.");
}

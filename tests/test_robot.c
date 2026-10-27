#include "robot.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static Robot robot;
static RobotPlan plan;
static void prepare(const char *text) {
    const char *error=robot_plan(&robot,text,false,&plan);
    if (error) fprintf(stderr,"Unexpected error for %s: %s\n",text,error);
    assert(!error); assert(plan.count);
}
static bool contains(const char *text) {
    char joined[4096]="";
    for (size_t i=0;i<plan.count;i++) {
        strcat(joined,plan.segments[i].text);strcat(joined," ");
    }
    return strstr(joined,text)!=NULL;
}
int main(void) {
    robot_init(&robot);
    prepare("Check the CPU and USB. NASA is ready.");
    assert(contains("see pee you"));assert(contains("you ess bee"));assert(contains("nasa"));
    prepare("Check XYZ and RX-7.");
    assert(contains("ex why zee"));assert(contains("ar ex seven"));
    prepare("CHECK THE CPU NOW");
    assert(contains("CHECK THE"));assert(contains("see pee you"));
    for (size_t i=0;i+1<plan.count;i++) assert(plan.segments[i].voice.speed==robot.voice.speed);
    prepare("The voltage is 12 V at 50 Hz and 3.3 V.");
    assert(contains("twelve volts"));assert(contains("fifty hertz"));assert(contains("three point three volts"));
    prepare("Code 007 and 1000.");
    assert(contains("zero zero seven"));assert(contains("one thousand"));
    prepare("Code 12-7 and 1.2.3.");assert(contains("one two seven"));assert(contains("one two three"));
    assert(robot_pronounce(&robot,"ROS","ross"));
    prepare("ROS is ready.");assert(contains("ross"));
    assert(robot_pronounce(&robot,"ROS","ar oh ess"));assert(robot.rule_count==1);
    assert(!robot_pronounce(&robot,"BAD!","invalid"));
    assert(!robot_pronounce(&robot,"BAD","invalid!"));
    prepare("Dr. Smith is ready.");
    assert(contains("doctor Smith"));
    prepare("Are you ready?");
    assert(plan.segments[plan.count-1].voice.pitch<robot.voice.pitch);
    prepare("Where are you going?");
    assert(plan.segments[plan.count-1].voice.pitch>robot.voice.pitch);
    prepare("You did WHAT?");
    assert(plan.segments[plan.count-1].voice.speed>robot.voice.speed);
    prepare("Wait... Stay here!");assert(plan.segments[0].pause_ms==450);
    assert(robot_character(&robot,"DICTATOR"));
    assert(robot.rule_count==1);prepare("You have been warned.");
    assert(plan.segments[0].voice.metallic==45);
    assert(!robot_character(&robot,"UNKNOWN"));
    assert(robot_set(&robot,"PITCH",0));assert(robot_set(&robot,"PITCH",255));
    assert(!robot_set(&robot,"PITCH",256));
    assert(!robot_set(&robot,"SING",2));assert(!robot_set(&robot,"METALLIC",101));
    assert(robot_character(&robot,"CLASSIC"));prepare("Hello my friend");
    assert(plan.count==1);assert(plan.segments[0].voice.pitch==64);
    assert(robot_set(&robot,"PITCH",0));assert(robot_set(&robot,"SPEED",255));prepare("Hello");
    assert(plan.segments[0].voice.pitch==0);assert(plan.segments[0].voice.speed==255);
    assert(!robot_plan(&robot,"/HEHLOW",true,&plan));assert(plan.segments[0].phonetic);
    const char *bad[]={"","...","hello @friend","caf\xc3\xa9","\nhello"};
    for (size_t i=0;i<sizeof(bad)/sizeof(bad[0]);i++) {
        assert(robot_plan(&robot,bad[i],false,&plan));assert(plan.count==0);
    }
    char too_long[257];memset(too_long,'a',256);too_long[256]=0;
    assert(robot_plan(&robot,too_long,false,&plan));assert(plan.count==0);
    puts("Planner checks passed: pronunciation, numbers, contours, profiles, and bounds.");
    printf("Memory: Robot=%zu bytes, RobotPlan=%zu bytes (host ABI; not RP2040 measurement).\n",sizeof(robot),sizeof(plan));
    return 0;
}

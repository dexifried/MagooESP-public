#include "WinchSafety.h"
#include <stdio.h>
int main(){
 WinchSafety s;
 if(s.pulse(1)!=1500) return 1;
 s.set('O',100); if(s.pulse(100)!=1100) return 2;
 if(s.pulse(1001)!=1500) return 3;
 s.set('I',1100); if(s.pulse(1200)!=1900) return 4;
 s.set('S',1200); if(s.pulse(1200)!=1500) return 5;
 s.setReversed(true); s.set('O',1300); if(s.pulse(1300)!=1900) return 6;
 s.set('I',0xfffffff0U); if(s.pulse(20)!=1100) return 7;
 if(s.pulse(1000)!=1500) return 8;
 puts("PASS: winch neutral, direction, reversal and lost-command stop");
}

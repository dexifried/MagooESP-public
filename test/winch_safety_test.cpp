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
 s.setReversed(false); s.set('O',2000,50); if(s.pulse(2000)!=1300) return 9;
 s.set('I',2001,50); if(s.pulse(2001)!=1700) return 10;
 s.set('I',2002,0); if(s.pulse(2002)!=1500) return 11;
 s.set('O',2003,255); if(s.pulse(2003)!=1100) return 12;
 s.set('O',2004,50); if(s.pulse(2905)!=1500) return 13;
 s.setReversed(true); s.set('O',3000,50); if(s.pulse(3000)!=1700) return 14;
 puts("PASS: winch neutral, direction, reversal and lost-command stop");
}

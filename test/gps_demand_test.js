const assert=require('node:assert/strict');
const {GpsDemand}=require('../expo-app/gps-demand');
const tick=()=>new Promise(resolve=>setImmediate(resolve));
(async()=>{
 let starts=0,stops=0,pauses=0,resolveStart;
 const demand=new GpsDemand({
  start:()=>{starts++;return new Promise(resolve=>{resolveStart=()=>resolve(()=>stops++)})},
  pause:()=>pauses++,error:e=>{throw e}
 });
 demand.setEnabled(true);await tick();assert.equal(starts,0);
 demand.setDemand(true);await tick();assert.equal(starts,1);
 demand.setDemand(false);resolveStart();await tick();assert.equal(stops,1);
 demand.setDemand(true);await tick();assert.equal(starts,2);
 resolveStart();await tick();
 demand.setDemand(true);await tick();assert.equal(starts,2);
 demand.setActive(false);assert.equal(stops,2);
 demand.setActive(true);await tick();assert.equal(starts,3);
 resolveStart();await tick();demand.setEnabled(false);assert.equal(stops,3);
 assert.equal(pauses,3);
 let pending=[],raceStops=[];
 const racing=new GpsDemand({start:()=>new Promise(resolve=>pending.push(resolve)),pause:()=>{},error:e=>{throw e}});
 racing.setEnabled(true);racing.setDemand(true);await tick();
 racing.setDemand(false);racing.setDemand(true);await tick();
 pending[0](()=>raceStops.push('old'));await tick();assert.deepEqual(raceStops,['old']);
 pending[1](()=>raceStops.push('new'));await tick();
 racing.setEnabled(false);assert.deepEqual(raceStops,['old','new']);
 console.log('PASS: GPS demand pauses, resumes and cleans up racing subscriptions');
})().catch(error=>{console.error(error);process.exitCode=1});

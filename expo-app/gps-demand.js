/* Run a location subscription only while it is enabled, visible and needed. */
class GpsDemand {
  constructor({start,pause,error}) {
    this.start=start;this.pause=pause;this.error=error;
    this.enabled=false;this.demand=false;this.active=true;
    this.generation=0;this.pending=false;this.cleanup=null;
  }
  setEnabled(value) { this.enabled=!!value;this.sync(); }
  setDemand(value) { this.demand=!!value;this.sync(); }
  setActive(value) { this.active=!!value;this.sync(); }
  sync() {
    const wanted=this.enabled&&this.demand&&this.active;
    if (!wanted) {
      if (!this.pending&&!this.cleanup) return;
      ++this.generation;this.pending=false;
      const cleanup=this.cleanup;this.cleanup=null;
      if (cleanup) cleanup();
      this.pause();return;
    }
    if (this.pending||this.cleanup) return;
    const generation=++this.generation;
    const current=()=>generation===this.generation&&this.enabled&&this.demand&&this.active;
    this.pending=true;
    Promise.resolve().then(()=>current()?this.start(current):null).then(cleanup=>{
      if (!current()) { if(cleanup)cleanup();return; }
      this.pending=false;this.cleanup=cleanup||null;
    }).catch(error=>{
      if (current()) { this.pending=false;this.error(error); }
    });
  }
}
module.exports={GpsDemand};

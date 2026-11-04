/* Exact, per-client cache of the ESP's RGB565 screen. */
(function (root, factory) {
  const api=factory();
  if (typeof module==='object' && module.exports) module.exports=api;
  if (root) root.MagooFrames=api;
})(typeof window==='undefined' ? null : window, function () {
  const WIDTH=240, HEIGHT=135, TILE_WIDTH=16, TILE_HEIGHT=15, COLS=15, COUNT=135;
  const FRAME_BYTES=WIDTH*HEIGHT*2, TILE_BYTES=1+TILE_WIDTH*TILE_HEIGHT*2;
  const bytes=value=>value instanceof Uint8Array ? value : new Uint8Array(value);
  class FrameCache {
    constructor() { this.clear(); }
    clear() { this.pixels=null;this.hashes=new Uint32Array(COUNT);this.updates=0; }
    get ready() { return this.pixels!==null; }
    get needsFull() { return !this.ready || this.updates>=120; }
    hash(tile) {
      const x=(tile%COLS)*TILE_WIDTH, y=Math.floor(tile/COLS)*TILE_HEIGHT;
      let h=2166136261;
      for(let row=0;row<TILE_HEIGHT;row++) {
        let pos=((y+row)*WIDTH+x)*2;
        for(let i=0;i<TILE_WIDTH*2;i++) h=Math.imul(h^this.pixels[pos++],16777619)>>>0;
      }
      return h>>>0;
    }
    setFull(value) {
      const data=bytes(value);
      if(data.length!==FRAME_BYTES) throw new Error('Invalid full frame');
      this.pixels=data.slice();
      for(let i=0;i<COUNT;i++) this.hashes[i]=this.hash(i);
      this.updates=0;
    }
    hashText() {
      if(!this.ready) throw new Error('No cached frame');
      return Array.from(this.hashes,h=>h.toString(16).padStart(8,'0')).join('');
    }
    apply(value) {
      const data=bytes(value);
      if(!this.ready || data.length<4 || data[0]!==77 || data[1]!==68 || data[3]!==49) throw new Error('Invalid frame update');
      if(data[2]===70) {
        if(data.length!==FRAME_BYTES+4) throw new Error('Invalid replacement frame');
        this.setFull(data.subarray(4));return true;
      }
      if(data[2]!==76 || data.length<6 || data[5]!==0 || data[4]>COUNT || data.length!==6+data[4]*TILE_BYTES)
        throw new Error('Invalid frame patches');
      const count=data[4],seen=new Uint8Array(COUNT);
      for(let i=0;i<count;i++) {
        const tile=data[6+i*TILE_BYTES];
        if(tile>=COUNT || seen[tile]) throw new Error('Invalid frame tile');
        seen[tile]=1;
      }
      for(let i=0;i<count;i++) {
        let pos=6+i*TILE_BYTES;
        const tile=data[pos++], x=(tile%COLS)*TILE_WIDTH,y=Math.floor(tile/COLS)*TILE_HEIGHT;
        for(let row=0;row<TILE_HEIGHT;row++) {
          const dest=((y+row)*WIDTH+x)*2;
          this.pixels.set(data.subarray(pos,pos+TILE_WIDTH*2),dest);pos+=TILE_WIDTH*2;
        }
        this.hashes[tile]=this.hash(tile);
      }
      this.updates++;
      return count>0;
    }
  }
  return { FrameCache };
});

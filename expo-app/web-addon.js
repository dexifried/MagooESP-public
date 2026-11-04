/* Expo-only controls and phone-to-dashboard map rendering. */
(function () {
  const send = data => window.ReactNativeWebView.postMessage(JSON.stringify(data));
  document.addEventListener('pointerdown', event => {
    if (event.target.closest('#winch-out, #winch-in')) send({ type: 'winch-active', value: true });
  }, true);
  for (const type of ['pointerup', 'pointercancel', 'lostpointercapture']) {
    document.addEventListener(type, event => {
      if (event.target.closest('#winch-out, #winch-in')) send({ type: 'winch-active', value: false });
    }, true);
  }
  document.addEventListener('click', event => {
    const link = event.target.closest('a');
    if (!link) return;
    if (link.getAttribute('href') === '/app.apk') { event.preventDefault(); send({ type: 'gps-controls' }); }
    if (link.getAttribute('href') === 'magoo://address') { event.preventDefault(); send({ type: 'device-address' }); }
  });
  const setup = document.getElementById('pair-open-setup');
  if (setup) setup.onclick = () => send({ type: 'setup-address' });
  window.__magooGpsViewChanged = () => send({type:'gps-demand',value:!sleeping&&(currentView==='navigation'||(currentView==='home'&&currentLayout!==2))});
  window.__magooGpsViewChanged();
  const pending = new Map(); let nextTile = 1, rendering = false, lastSent = 0, gpsGeneration = 0, mapController = null;
  let mapKey='', mapPixels='', mapToken='', mapBuilt=0, refreshSupported=true;
  window.__magooStopGps = () => { gpsGeneration++; if (mapController) mapController.abort(); mapKey=mapPixels=mapToken='';mapBuilt=lastSent=0;refreshSupported=true; };
  function tile(z, x, y) {
    const id = nextTile++;
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => { pending.delete(id); reject(new Error('Map tile timed out')); }, 15000);
      pending.set(id, { resolve, reject, timer }); send({ type: 'tile', id, z, x, y });
    });
  }
  window.__magooTileResult = result => {
    const p = pending.get(result.id); if (!p) return;
    pending.delete(result.id); clearTimeout(p.timer);
    if (!result.data) { p.reject(new Error(result.error || 'Map tile unavailable')); return; }
    const image = new Image(); image.onload = () => p.resolve(image); image.onerror = () => p.reject(new Error('Invalid map tile')); image.src = result.data;
  };
  window.__magooApplyLocation = async fix => {
    if (rendering || !fix || Date.now() - lastSent < 10000) return;
    if (currentView !== 'navigation' && !(currentView === 'home' && currentLayout !== 2)) return;
    const age = Date.now() - fix.timestamp;
    if (!Number.isFinite(age) || age < 0 || age > 25000) return;
    rendering = true; const generation = gpsGeneration;
    try {
      const z=16, limit=2**z, world=256*limit;
      const lat=Math.max(-85.0511,Math.min(85.0511,fix.latitude));
      const px=(fix.longitude+180)/360*world, rad=lat*Math.PI/180;
      const py=(1-Math.log(Math.tan(rad)+1/Math.cos(rad))/Math.PI)/2*world;
      const left=Math.floor(px)-72, top=Math.floor(py)-52;
      const key=z+':'+left+':'+top;
      const built=key!==mapKey || !mapPixels || Date.now()-mapBuilt>=300000;
      let pixels=mapPixels;
      if (built) {
        const canvas=document.createElement('canvas'); canvas.width=144; canvas.height=104;
        const context=canvas.getContext('2d'); const jobs=[];
        for(let y=Math.floor(top/256);y<=Math.floor((top+103)/256);y++) {
          if(y<0||y>=limit)throw new Error('Location outside map area');
          for(let x=Math.floor(left/256);x<=Math.floor((left+143)/256);x++) {
            const normalized=((x%limit)+limit)%limit;
            jobs.push(tile(z,normalized,y).then(image=>({image,x,y})));
          }
        }
        for (const item of await Promise.all(jobs)) context.drawImage(item.image,item.x*256-left,item.y*256-top);
        if (generation !== gpsGeneration) return;
        const small=document.createElement('canvas');small.width=72;small.height=52;
        const sc=small.getContext('2d');sc.drawImage(canvas,0,0,72,52);
        const bytes=sc.getImageData(0,0,72,52).data;pixels='';
        for(let i=0;i<bytes.length;i+=4) pixels+=(((bytes[i]>>5)<<5)|((bytes[i+1]>>5)<<2)|(bytes[i+2]>>6)).toString(16).padStart(2,'0');
      }
      const bearing=fix.speed>0.8 && Number.isFinite(fix.heading) && fix.heading>=0 && fix.heading<360 ? fix.heading : -1;
      if (generation !== gpsGeneration) return;
      mapController=new AbortController();
      const post=async(path,data)=>{
        const ageMs=Date.now()-fix.timestamp;if(ageMs<0||ageMs>25000)throw new Error('Location is too old');
        return fetch(path,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams({...data,bearing,ageMs}),signal:mapController.signal});
      };
      let refreshed=false;
      if (!built && mapToken && refreshSupported) {
        const response=await post('/map/refresh',{token:mapToken});
        if(response.status===404||response.status===405)refreshSupported=false;
        else if(response.status!==409&&!response.ok)throw new Error('Could not send map to Magoo');
        else refreshed=response.ok;
      }
      if(!refreshed) {
        const response=await post('/map',{pixels});
        if(!response.ok)throw new Error('Could not send map to Magoo');
        const reply=await response.text(), match=/^OK:([0-9a-f]{1,8})$/.exec(reply);
        mapToken=match?match[1]:'';
      }
      if(generation!==gpsGeneration)return;
      if(built){mapKey=key;mapPixels=pixels;mapBuilt=Date.now()}
      lastSent=Date.now();send({type:'gps-status',message:'Sharing phone location with Magoo.'});
    } catch(error) { if(generation===gpsGeneration)send({type:'gps-status',message:error.message || 'Map unavailable. Check internet and Magoo connection.'}); }
    finally { rendering=false;mapController=null; }
  };
})();

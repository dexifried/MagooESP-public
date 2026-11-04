/* Relay dashboard API requests through Expo's native network stack. */
(function () {
  const pending=new Map();let sequence=0;
  const notify=message=>window.ReactNativeWebView.postMessage(JSON.stringify(message));
  window.__magooHttpResult=result=>{
    const item=pending.get(result.id);if(!item)return;pending.delete(result.id);
    if(item.signal)item.signal.removeEventListener('abort',item.abort);
    if(result.error){item.reject(new TypeError(result.error));return;}
    const text=result.text||'',base64=result.base64||'',headers=result.headers||{};
    item.resolve({status:result.status,ok:result.status>=200&&result.status<300,
      headers:{get:name=>headers[String(name).toLowerCase()]||null},
      text:async()=>text,json:async()=>JSON.parse(text),
      arrayBuffer:async()=>{const raw=atob(base64),bytes=new Uint8Array(raw.length);for(let i=0;i<raw.length;i++)bytes[i]=raw.charCodeAt(i);return bytes.buffer;}});
  };
  window.fetch=(path,options={})=>new Promise((resolve,reject)=>{
    if(typeof path!=='string'||!path.startsWith('/')){reject(new TypeError('Invalid Magoo request'));return;}
    const signal=options.signal;if(signal?.aborted){reject(new DOMException('Aborted','AbortError'));return;}
    const id=++sequence,abort=()=>{if(!pending.has(id))return;pending.delete(id);notify({type:'http-cancel',id});reject(new DOMException('Aborted','AbortError'));};
    pending.set(id,{resolve,reject,signal,abort});if(signal)signal.addEventListener('abort',abort,{once:true});
    const headers={};new Headers(options.headers||{}).forEach((value,key)=>{headers[key]=value});
    notify({type:'http-request',id,path,method:options.method||'GET',headers,body:options.body==null?null:String(options.body)});
  });
})();

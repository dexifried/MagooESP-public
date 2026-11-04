// Guided Wi-Fi setup. Credentials never leave the browser except in the
// existing direct-to-device pairing request.
let pairData = {saved:[], networks:[], activeSlot:-1, setupAccess:false};
let pairMode = 0, pairSelection = null, pairStep = 'welcome', pairGeneration = 0;
let pairScanning = false, pairDifferentPassword = false;
const pairDialog = $('pairing');
const pairWifiIcon = '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M2 9a15 15 0 0 1 20 0M5 12a10 10 0 0 1 14 0M8 15a6 6 0 0 1 8 0"/><circle cx="12" cy="19" r="1"/></svg>';
const pairArrow = '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="m9 5 7 7-7 7"/></svg>';

function pairTheme() {
  pairDialog.style.setProperty('--pair-accent', colors[currentTheme] || colors[0]);
  pairDialog.style.setProperty('--pair-ink', currentTheme === 0 ? '#fff' : '#101214');
}
function pairShow(step) {
  pairStep = step;
  for (const el of pairDialog.querySelectorAll('[data-pair-screen]')) el.hidden = el.dataset.pairScreen !== step;
  const titles = {welcome:'Connect Magoo', setup:'Connect your phone', networks:'Choose a network', password:'Connect to Wi-Fi', connecting:'Connecting', success:'Connected', failed:'Connection help'};
  $('pair-header').textContent = titles[step] || 'Connect Magoo';
  $('pair-back').style.visibility = step === 'welcome' || step === 'connecting' ? 'hidden' : 'visible';
  pairDialog.scrollTop = 0;
  const heading = pairDialog.querySelector('[data-pair-screen="' + step + '"] h1');
  if (heading) heading.focus({preventScroll:true});
}
function pairUpdateModes() {
  for (let slot=0;slot<2;slot++) {
    const saved=pairData.saved.find(n=>n.slot===slot);
    $('pair-mode-'+slot).setAttribute('aria-pressed',String(pairMode===slot));
    $('pair-mode-note-'+slot).textContent = saved ? saved.ssid + (pairData.activeSlot===slot?' · Connected':' · Saved') : (slot===0?'For use around home':'For use away from home');
  }
}
async function pairLoad() {
  const response = await fetchTimed('/networks',{cache:'no-store'},3500);
  if (!response.ok) throw Error('Could not load networks');
  pairData = await response.json();
  pairData.saved = pairData.saved || [];
  pairData.networks = pairData.networks || [];
  pairUpdateModes();
  return pairData;
}
async function openPairing() {
  releaseWinch(); pairGeneration++; pairMode=0; pairSelection=null; pairDifferentPassword=false;
  pairTheme(); pairShow('welcome');
  if (!pairDialog.open) pairDialog.showModal();
  try { await pairLoad(); } catch (_) { $('pair-welcome-status').textContent='Could not check saved networks. You can still continue.'; }
}
function closePairing() { pairGeneration++; $('pair-password').value=''; if(pairDialog.open)pairDialog.close(); $('wifi-open').focus(); }
$('wifi-open').onclick=openPairing;
$('pair-close').onclick=closePairing;
pairDialog.addEventListener('cancel',event=>{event.preventDefault();closePairing()});
for (const slot of [0,1]) $('pair-mode-'+slot).onclick=()=>{pairMode=slot;pairUpdateModes()};
$('pair-continue').onclick=async()=>{
  try { await pairLoad(); } catch (_) { pairShow('setup'); return; }
  if (!pairData.setupAccess) { pairShow('setup'); return; }
  await pairEnterNetworks();
};
$('pair-open-setup').onclick=()=>{location.href='http://192.168.4.1/'};
$('pair-back').onclick=()=>{
  if(pairStep==='password'){pairShow('networks');return}
  if(pairStep==='networks'||pairStep==='setup'||pairStep==='success'||pairStep==='failed'){pairShow('welcome');return}
};

function pairAddNetwork(name, saved, network) {
  const row=document.createElement('button'); row.type='button';row.className='pair-choice';row.disabled=pairScanning;
  const icon=document.createElement('span');icon.className='pair-icon';icon.innerHTML=pairWifiIcon;
  const text=document.createElement('span');text.className='pair-copy';
  const title=document.createElement('strong');title.textContent=name;
  const sub=document.createElement('small');
  sub.textContent=saved ? (pairData.activeSlot===pairMode?'Connected · saved':'Saved network') : (network.secured?'Password required':'Open network');
  text.append(title,sub);const arrow=document.createElement('span');arrow.className='pair-chevron';arrow.innerHTML=pairArrow;
  row.append(icon,text,arrow);
  row.onclick=()=>{pairSelection={ssid:name,slot:pairMode,saved:!!saved,secured:network?!!network.secured:true};pairDifferentPassword=false;pairPasswordScreen()};
  $('pair-networks-list').append(row);
}
function pairRenderNetworks() {
  const list=$('pair-networks-list');list.replaceChildren();
  const saved=pairData.saved.find(n=>n.slot===pairMode);
  if(saved)pairAddNetwork(saved.ssid,true,pairData.networks.find(n=>n.ssid===saved.ssid));
  for(const n of pairData.networks)if(!saved||n.ssid!==saved.ssid)pairAddNetwork(n.ssid,false,n);
  if(!list.children.length&&!pairScanning){const note=document.createElement('p');note.className='pair-note';note.textContent='No networks found. Make sure your network or hotspot is on, then search again.';list.append(note)}
  $('pair-scan').disabled=pairScanning;
}
async function pairEnterNetworks(){
  pairShow('networks');$('pair-network-subtitle').textContent=pairMode===0?'Choose your home Wi-Fi network.':'Choose your phone’s hotspot.';
  $('pair-network-hint').textContent=pairMode===0?'Magoo supports 2.4 GHz Wi-Fi.':'Keep the hotspot on. If this phone can’t use its hotspot and connect to Magoo at the same time, use another phone for setup.';
  pairRenderNetworks();await pairScan();
}
async function pairScan(){
  if(pairScanning)return;pairScanning=true;const generation=pairGeneration;
  $('pair-scan-status').innerHTML='<span class="pair-spinner" aria-hidden="true"></span>Looking for networks…';pairRenderNetworks();
  try {
    const response=await fetchTimed('/scan',{method:'POST'},5000);
    if(response.status===403){pairShow('setup');return}
    if(response.status===409){$('pair-scan-status').textContent='Magoo is connecting. Try searching again in a moment.';return}
    if(!response.ok)throw Error('scan');
    for(let i=0;i<35&&generation===pairGeneration;i++){
      await new Promise(resolve=>setTimeout(resolve,1000));
      const data=await pairLoad();
      if(!data.scanning){$('pair-scan-status').textContent='Showing nearby 2.4 GHz networks.';return}
    }
    if(generation===pairGeneration)throw Error('timeout');
  } catch(_){if(generation===pairGeneration)$('pair-scan-status').textContent='Couldn’t search. Your saved network is still shown.'}
  finally {pairScanning=false;if(generation===pairGeneration)pairRenderNetworks()}
}
$('pair-scan').onclick=pairScan;
function pairPasswordScreen(){
  $('pair-selected-name').textContent=pairSelection.ssid;
  $('pair-selected-detail').textContent=pairSelection.saved?'Saved network':pairSelection.secured?'Password-protected network':'Open network';
  $('pair-password').value='';$('pair-password').type='password';$('pair-show-password').textContent='Show';
  $('pair-password-area').hidden=pairSelection.saved||!pairSelection.secured;
  $('pair-saved-note').hidden=!pairSelection.saved;
  $('pair-change-password').hidden=!pairSelection.saved;
  $('pair-form-error').textContent='';pairShow('password');
}
$('pair-change-password').onclick=()=>{pairDifferentPassword=true;$('pair-password-area').hidden=false;$('pair-saved-note').hidden=true;$('pair-change-password').hidden=true;$('pair-password').focus()};
$('pair-show-password').onclick=()=>{const field=$('pair-password'),show=field.type==='password';field.type=show?'text':'password';$('pair-show-password').textContent=show?'Hide':'Show';$('pair-show-password').setAttribute('aria-label',show?'Hide password':'Show password')};
$('pair-form').onsubmit=async event=>{
  event.preventDefault();if(!pairSelection)return;
  const password=$('pair-password').value,useSaved=pairSelection.saved&&!pairDifferentPassword;
  if(!useSaved&&pairSelection.secured&&(password.length<8||password.length>63)){$('pair-form-error').textContent='Enter a Wi-Fi password between 8 and 63 characters.';return}
  const selection=pairSelection,generation=++pairGeneration,pairId=Math.floor(Math.random()*4294967295)+1;
  pairShow('connecting');$('pair-connecting-name').textContent=selection.ssid;
  let requestAccepted=false;
  try {
    const response=await fetchTimed('/wifi',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams({ssid:selection.ssid,password,slot:String(selection.slot),useSaved:useSaved?'1':'0',pairId:String(pairId)})},5000);
    if(response.status===403){pairShow('setup');return}
    if(response.status===409){pairFailure('Magoo is busy connecting or searching. Wait a moment, then try again.');return}
    if(!response.ok){pairFailure('Check the network details and try again.');return}
    requestAccepted=true;
  } catch(_) { /* Switching Wi-Fi can interrupt the response; check the device. */ }
  finally {$('pair-password').value=''}
  const deadline=Date.now()+40000;
  while(generation===pairGeneration&&Date.now()<deadline){
    await new Promise(resolve=>setTimeout(resolve,1000));
    try {
      const data=await pairLoad();
      if(data.pairingSlot!==selection.slot||data.pairingId!==pairId)continue;
      if(data.pairing==='connected'){pairShow('success');$('pair-success-name').textContent=selection.ssid;return}
      if(data.pairing==='failed'){pairFailure('Magoo couldn’t join that network. Check the password and make sure it’s a 2.4 GHz network.');return}
    } catch(_) { /* The setup Wi-Fi can briefly disappear when channels change. */ }
  }
  if(generation===pairGeneration)pairFailure(requestAccepted?'The connection changed before Magoo could confirm. Reconnect your phone to MagooESP, open 192.168.4.1, and check Wi-Fi setup.':'We couldn’t confirm that Magoo received the request. Reconnect to MagooESP and try again.');
};
function pairFailure(message){$('pair-failure-message').textContent=message;pairShow('failed')}
$('pair-done').onclick=closePairing;
$('pair-try-again').onclick=()=>pairShow('welcome');

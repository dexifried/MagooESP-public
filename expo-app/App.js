import React, { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import { Alert, AppState, KeyboardAvoidingView, Linking, Modal, Platform, Pressable, StyleSheet, Text, TextInput, View } from 'react-native';
import { SafeAreaProvider, SafeAreaView } from 'react-native-safe-area-context';
import { StatusBar } from 'expo-status-bar';
import * as Location from 'expo-location';
import AsyncStorage from '@react-native-async-storage/async-storage';
import { WebView } from 'react-native-webview';
import dashboard from './dashboard.generated';
const { GpsDemand } = require('./gps-demand');

const HOST_KEY = 'magoo:address';
const DEFAULT_HOST = 'magooesp.local';
const AP_HOST = '192.168.4.1';
const MAX_TILES = 100;
const GET_PATHS = new Set(['/status', '/winch/status', '/screen', '/playlist', '/networks', '/modules/music']);
const POST_PATHS = new Set(['/appearance', '/gear/calibrate', '/layout', '/map', '/map/refresh', '/music', '/modules/music/start', '/modules/music/chunk', '/modules/music/finish', '/scan', '/screen-delta', '/sleep', '/startup', '/theme', '/tilt/zero', '/touch', '/view', '/wifi', '/winch', '/winch/reverse']);
const postOptions = body => ({ method: 'POST', headers: { 'Content-Type': 'application/x-www-form-urlencoded' }, body });

function validHost(value) { return /^[a-zA-Z0-9.-]{1,253}$/.test(value) && !value.startsWith('.') && !value.endsWith('.'); }
async function reachable(host) {
  const controller = new AbortController(), timer = setTimeout(() => controller.abort(), 1800);
  try { const response = await fetch('http://' + host + '/status', { signal: controller.signal }); return response.ok; }
  catch (_) { return false; } finally { clearTimeout(timer); }
}
function blobData(blob) { return new Promise((resolve, reject) => { const reader = new FileReader(); reader.onload = () => resolve(reader.result); reader.onerror = () => reject(new Error('Could not read map tile')); reader.readAsDataURL(blob); }); }

function Magoo() {
  const web = useRef(null), hostRef = useRef(DEFAULT_HOST), active = useRef(true), foreground = useRef(true), winchHeld = useRef(false);
  const gpsRun = useRef(0), lastFix = useRef(null), tileWork = useRef(new Map());
  const requests = useRef(new Map()), networkEpoch = useRef(0);
  const [host, setHost] = useState(null), [address, setAddress] = useState(DEFAULT_HOST), [modal, setModal] = useState(null);
  const [gpsOn, setGpsOn] = useState(false), [gpsStatus, setGpsStatus] = useState('Location sharing is off.');
  const source = useMemo(() => host ? ({ html: dashboard, baseUrl: 'http://' + host + '/' }) : null, [host]);
  const inject = useCallback(script => web.current?.injectJavaScript(script + ';true;'), []);
  const setDevice = useCallback(async value => {
    const next = value.trim();
    if (!validHost(next)) { Alert.alert('Check the address', 'Enter a hostname or IP address, without http://.'); return; }
    inject("if(typeof releaseWinch==='function')releaseWinch()");
    if (winchHeld.current) { winchHeld.current = false; fetch('http://' + hostRef.current + '/winch', postOptions('direction=stop')).catch(() => {}); }
    networkEpoch.current++; for (const item of requests.current.values()) item.controller.abort(); requests.current.clear();
    hostRef.current = next; setAddress(next); setHost(next); setModal(null);
    try { await AsyncStorage.setItem(HOST_KEY, next); } catch (_) {}
  }, [inject]);
  useEffect(() => { let mounted = true; (async () => {
    let saved = DEFAULT_HOST; try { saved = (await AsyncStorage.getItem(HOST_KEY)) || DEFAULT_HOST; } catch (_) {}
    if (!validHost(saved)) saved = DEFAULT_HOST;
    let selected = saved;
    if (!(await reachable(saved)) && saved !== AP_HOST && await reachable(AP_HOST)) selected = AP_HOST;
    if (mounted) { hostRef.current = selected; setAddress(selected); setHost(selected); }
  })(); return () => { mounted = false; }; }, []);

  const deliverFix = useCallback(fix => {
    if (!fix || !active.current) return;
    const c = fix.coords;
    inject('window.__magooApplyLocation && window.__magooApplyLocation(' + JSON.stringify({ latitude: c.latitude, longitude: c.longitude, heading: c.heading, speed: c.speed, timestamp: fix.timestamp }) + ')');
  }, [inject]);
  const gpsController = useMemo(() => new GpsDemand({
    start: async isCurrent => {
      if (active.current) setGpsStatus('Waiting for a location…');
      const watcher = await Location.watchPositionAsync({ accuracy: Location.Accuracy.Balanced, timeInterval: 4000, distanceInterval: 0 }, fix => {
        if (!isCurrent() || !active.current) return;
        lastFix.current=fix;deliverFix(fix);
      }, () => { if (isCurrent() && active.current) setGpsStatus('Could not get location. Check phone location settings.'); });
      if (!isCurrent()) return () => watcher.remove();
      let acquiring=false;
      const timer=setInterval(async () => {
        if (!isCurrent() || !active.current || acquiring) return;
        acquiring=true;
        try {
          let fix=lastFix.current;
          if (!fix || Date.now()-fix.timestamp>8000) fix=await Location.getCurrentPositionAsync({accuracy:Location.Accuracy.Balanced});
          if (isCurrent() && active.current) { lastFix.current=fix;deliverFix(fix); }
        } catch (_) { if(isCurrent() && active.current)setGpsStatus('Waiting for a fresh phone location.'); }
        finally { acquiring=false; }
      },10000);
      return () => { watcher.remove();clearInterval(timer); };
    },
    pause: () => { inject('window.__magooStopGps && window.__magooStopGps()');lastFix.current=null;if(active.current)setGpsStatus('Location paused until the map is shown.'); },
    error: () => { if(active.current)setGpsStatus('Could not start location sharing.'); }
  }), [deliverFix,inject]);
  const stopGps = useCallback((message = 'Location sharing is off.') => {
    gpsRun.current++;gpsController.setEnabled(false);
    inject('window.__magooStopGps && window.__magooStopGps()');lastFix.current=null;
    setGpsOn(false);setGpsStatus(message);
  }, [gpsController,inject]);
  const startGps = useCallback(async () => {
    const run=++gpsRun.current;setGpsStatus('Requesting location permission…');
    try {
      const result=await Location.requestForegroundPermissionsAsync();
      if(run!==gpsRun.current)return;
      if(!active.current){setGpsStatus('Open Magoo and try starting location again.');return;}
      if(result.status!=='granted'){setGpsStatus('Location permission was not granted.');return;}
      setGpsOn(true);gpsController.setEnabled(true);
      if(!gpsController.demand)setGpsStatus('Location paused until the map is shown.');
    } catch (_) { if(run===gpsRun.current)setGpsStatus('Could not start location sharing.'); }
  }, [gpsController]);
  useEffect(() => {
    const initial=AppState.currentState;
    active.current=initial!=='background';foreground.current=!initial||initial==='active';
    gpsController.setActive(foreground.current);
    const listener = AppState.addEventListener('change', state => {
      active.current = state !== 'background';
      foreground.current = state === 'active';
      gpsController.setActive(state === 'active');
      inject('window.__magooSetAppActive && window.__magooSetAppActive(' + (state === 'active') + ')');
      if (state !== 'active') {
        for (const item of requests.current.values()) if (['/status','/screen','/screen-delta'].includes(item.path)) item.controller.abort();
        inject("if(typeof releaseWinch==='function')releaseWinch()");
        if (winchHeld.current) {
          winchHeld.current = false;
          for (const item of requests.current.values()) if (item.path === '/winch' && item.body !== 'direction=stop') item.controller.abort();
          fetch('http://' + hostRef.current + '/winch', postOptions('direction=stop')).catch(() => {});
        }
        if (state === 'background') stopGps('Location sharing stopped when Magoo went to the background.');
      }
    });
    return () => { listener.remove(); active.current=false; gpsRun.current++; gpsController.setEnabled(false); for (const item of requests.current.values()) item.controller.abort(); requests.current.clear(); };
  }, [inject, stopGps, gpsController]);

  const getTile = useCallback(async (z, x, y) => {
    if (!Number.isInteger(z) || !Number.isInteger(x) || !Number.isInteger(y) || z < 14 || z > 18 || x < 0 || y < 0 || x >= 2**z || y >= 2**z) throw new Error('Invalid map tile');
    const key = 'magoo:tile:' + z + ':' + x + ':' + y;
    if (tileWork.current.has(key)) return tileWork.current.get(key);
    const work = (async () => {
      try { const saved = await AsyncStorage.getItem(key); if (saved) { const tile = JSON.parse(saved); if (tile.expires > Date.now() && tile.data) return tile.data; } } catch (_) {}
      const response = await fetch('https://tile.openstreetmap.org/' + z + '/' + x + '/' + y + '.png', { headers: { 'User-Agent': 'MagooDashboard/1.0 (+https://magoo.dexifyz.com/)' } });
      if (!response.ok) throw new Error('Map tile unavailable');
      const data = await blobData(await response.blob()), cc = response.headers.get('cache-control') || '';
      if (!/no-store|no-cache/i.test(cc)) {
        const match = cc.match(/max-age=(\d+)/i), expires = Date.now() + (match ? Number(match[1]) * 1000 : 7 * 24 * 60 * 60 * 1000);
        try {
          await AsyncStorage.setItem(key, JSON.stringify({ data, expires }));
          const all = await AsyncStorage.getAllKeys(), keys = all.filter(x => x.startsWith('magoo:tile:'));
          if (keys.length > MAX_TILES) await AsyncStorage.multiRemove(keys.slice(0, keys.length - MAX_TILES));
        } catch (_) {}
      }
      return data;
    })();
    tileWork.current.set(key, work); try { return await work; } finally { tileWork.current.delete(key); }
  }, []);

  const onMessage = useCallback(async event => {
    let message; try { message = JSON.parse(event.nativeEvent.data); } catch (_) { return; }
    if (message.type === 'gps-controls') { setModal('gps'); return; }
    if (message.type === 'gps-demand' && typeof message.value === 'boolean') { gpsController.setDemand(message.value); return; }
    if (message.type === 'device-address') { setAddress(hostRef.current); setModal('address'); return; }
    if (message.type === 'setup-address') { await setDevice(AP_HOST); return; }
    if (message.type === 'winch-active') { winchHeld.current = !!message.value; return; }
    if (message.type === 'gps-status') { setGpsStatus(message.message); return; }
    if (message.type === 'http-cancel') { requests.current.get(message.id)?.controller.abort(); return; }
    if (message.type === 'http-request') {
      const id = message.id, path = message.path, method = String(message.method || 'GET').toUpperCase();
      const result = { id }, epoch = networkEpoch.current;
      if (!Number.isSafeInteger(id) || !((method === 'GET' && GET_PATHS.has(path)) || (method === 'POST' && POST_PATHS.has(path)))) {
        result.error = 'Unsupported Magoo request';
      } else if (path === '/winch' && !foreground.current && message.body !== 'direction=stop') {
        result.error = 'Winch controls are unavailable in the background';
      } else {
        const controller = new AbortController(), timeout = setTimeout(() => controller.abort(), path === '/modules/music/start' ? 30000 : 6000);
        requests.current.set(id, { controller, path, body: message.body });
        try {
          const options = { method, signal: controller.signal };
          if (method === 'POST') { options.headers = { 'Content-Type': 'application/x-www-form-urlencoded' }; options.body = message.body || ''; }
          const response = await fetch('http://' + hostRef.current + path, options);
          result.status = response.status; result.headers = { 'x-display-state': response.headers.get('x-display-state') };
          if ((path === '/screen' || path === '/screen-delta') && response.status === 200) {
            const data = await blobData(await response.blob()); result.base64 = String(data).split(',')[1] || '';
          } else result.text = await response.text();
        } catch (error) { result.error = error.message || 'Magoo connection unavailable'; }
        finally { clearTimeout(timeout); if (requests.current.get(id)?.controller === controller) requests.current.delete(id); }
      }
      if (epoch === networkEpoch.current) inject('window.__magooHttpResult && window.__magooHttpResult(' + JSON.stringify(result) + ')');
      return;
    }
    if (message.type === 'tile') {
      const result = { id: message.id }, epoch = networkEpoch.current;
      try { result.data = await getTile(message.z, message.x, message.y); } catch (error) { result.error = error.message; }
      if (epoch === networkEpoch.current) inject('window.__magooTileResult && window.__magooTileResult(' + JSON.stringify(result) + ')');
    }
  }, [getTile, inject, setDevice, gpsController]);

  return <SafeAreaProvider><SafeAreaView style={styles.screen} edges={['top', 'bottom']}><StatusBar style="light" />
    {source ? <WebView ref={web} source={source} originWhitelist={['*']} onMessage={onMessage} javaScriptEnabled domStorageEnabled
      setSupportMultipleWindows={false} onShouldStartLoadWithRequest={request => {
        const url = request.url || '';
        if (url === 'about:blank' || url.startsWith('http://' + host + '/') || url.startsWith('data:')) return true;
        if (url === 'magoo://address') { setAddress(hostRef.current); setModal('address'); return false; }
        if (url.startsWith('https://')) { Linking.openURL(url).catch(() => {}); return false; } return false;
      }} style={styles.web} /> : <View style={styles.loading}><Text style={styles.muted}>Connecting to Magoo…</Text></View>}
    <Modal visible={modal !== null} transparent animationType="fade" onRequestClose={() => setModal(null)}>
      <KeyboardAvoidingView behavior={Platform.OS === 'ios' ? 'padding' : undefined} style={styles.overlay}>
        <View style={styles.card}>
          {modal === 'address' ? <>
            <Text style={styles.title}>Magoo address</Text>
            <Text style={styles.muted}>Use magooesp.local on your Wi-Fi, or 192.168.4.1 on Magoo’s Wi-Fi.</Text>
            <TextInput value={address} onChangeText={setAddress} autoCapitalize="none" autoCorrect={false} keyboardType="url" style={styles.input} accessibilityLabel="Magoo address" />
            <Pressable style={styles.primary} onPress={() => setDevice(address)}><Text style={styles.buttonText}>Connect</Text></Pressable>
          </> : <>
            <Text style={styles.title}>Phone GPS</Text>
            <Text style={styles.muted}>Share this phone’s location with Magoo while this app is open. Map tiles require internet.</Text>
            <Text style={styles.status}>{gpsStatus}</Text>
            <Pressable style={styles.primary} onPress={gpsOn ? () => stopGps() : startGps}><Text style={styles.buttonText}>{gpsOn ? 'Stop sharing' : 'Start sharing'}</Text></Pressable>
          </>}
          <Pressable style={styles.secondary} onPress={() => setModal(null)}><Text style={styles.buttonText}>Done</Text></Pressable>
        </View>
      </KeyboardAvoidingView>
    </Modal>
  </SafeAreaView></SafeAreaProvider>;
}
export default Magoo;
const styles = StyleSheet.create({ screen: { flex: 1, backgroundColor: '#090a0b' }, web: { flex: 1, backgroundColor: '#090a0b' }, loading: { flex: 1, alignItems: 'center', justifyContent: 'center' }, overlay: { flex: 1, backgroundColor: '#000b', justifyContent: 'center', padding: 22 }, card: { padding: 22, borderRadius: 16, backgroundColor: '#1a1b1d', borderColor: '#3b3d3f', borderWidth: 1 }, title: { fontSize: 20, fontWeight: '700', color: '#fff', marginBottom: 10 }, muted: { fontSize: 14, lineHeight: 21, color: '#adafb2' }, status: { fontSize: 13, color: '#e0e1e2', marginTop: 14 }, input: { minHeight: 48, color: '#fff', backgroundColor: '#101113', borderColor: '#505254', borderWidth: 1, borderRadius: 9, paddingHorizontal: 12, marginTop: 18, fontSize: 16 }, primary: { minHeight: 48, backgroundColor: '#bd2029', borderRadius: 9, alignItems: 'center', justifyContent: 'center', marginTop: 18 }, secondary: { minHeight: 46, borderRadius: 9, borderColor: '#4a4c4f', borderWidth: 1, alignItems: 'center', justifyContent: 'center', marginTop: 9 }, buttonText: { color: '#fff', fontSize: 14, fontWeight: '700' } });

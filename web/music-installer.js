(function (root) {
  async function installMusic({ request, loadPackage, progress = () => {} }) {
    const status = await request('/modules/music');
    if (status.status === 404 || status.status === 405) return; // Older firmware already includes Music.
    if (!status.ok) throw new Error('Could not check Music on Magoo');
    const state = await status.json();
    const raw = atob(await loadPackage());
    if (raw.length < 16 || raw.slice(0, 4) !== 'MGMP') throw new Error('Music package is invalid');
    const checksum = (raw.charCodeAt(12) | raw.charCodeAt(13) << 8 | raw.charCodeAt(14) << 16 | raw.charCodeAt(15) << 24) >>> 0;
    if (state.installed && state.version === 1 && state.checksum === checksum) return;
    if (state.version !== 1) throw new Error('This Music package requires a compatible Magoo update');
    if (state.installing) throw new Error('Another Music installation is in progress');
    const send = async (path, data) => {
      const response = await request(path, data);
      if (!response.ok) throw new Error((await response.text()) || 'Music installation failed');
      return response;
    };
    progress('Preparing Music…');
    const start = await send('/modules/music/start', { size: raw.length });
    const { token } = await start.json();
    if (!Number.isSafeInteger(token) || token <= 0) throw new Error('Invalid installation response');
    for (let offset = 0; offset < raw.length; offset += 1024) {
      let hex = '';
      for (let i = offset; i < Math.min(offset + 1024, raw.length); i++) hex += raw.charCodeAt(i).toString(16).padStart(2, '0');
      await send('/modules/music/chunk', { token, offset, data: hex });
      progress('Installing Music… ' + Math.round(100 * Math.min(offset + 1024, raw.length) / raw.length) + '%');
    }
    await send('/modules/music/finish', { token });
    progress('Music installed');
  }
  if (typeof module !== 'undefined' && module.exports) module.exports = { installMusic };
  else root.magooInstallMusic = installMusic;
})(typeof window !== 'undefined' ? window : globalThis);

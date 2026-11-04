package ca.magoo.gps;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.graphics.Color;
import android.net.Uri;
import android.os.Bundle;
import android.os.Build;
import android.text.InputType;
import android.view.ViewGroup;
import android.webkit.WebResourceRequest;
import android.webkit.WebResourceResponse;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.EditText;
import android.widget.TextView;
import java.io.IOException;
import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicBoolean;
import org.json.JSONObject;

/** The dashboard is bundled in the app; only API requests go to Magoo. */
public class MainActivity extends Activity {
  private static final String UPDATE_URL="https://magoo.dexifyz.com/api/android/update";
  private static final String DOWNLOAD_URL="https://magoo.dexifyz.com/";
  private static final Set<String> ASSETS = new HashSet<>(Arrays.asList(
      "dashboard.html", "dashboard-polish.css", "pairing.css", "pairing.js", "frame-delta.js",
      "hero.jpg", "home.jpg", "speed.jpg", "navigation.jpg", "music.jpg",
      "album/0.jpg", "album/1.jpg", "album/2.jpg"));
  private final ExecutorService network = Executors.newSingleThreadExecutor();
  private final ExecutorService updates = Executors.newSingleThreadExecutor();
  private final AtomicBoolean updateChecking = new AtomicBoolean();
  private boolean resumed = false;
  private long shownUpdate = 0;
  private WebView web;
  private String host = "magooesp.local";

  @Override public void onCreate(Bundle state) {
    super.onCreate(state);
    TextView loading = new TextView(this);
    loading.setText("Connecting to Magoo…"); loading.setTextColor(Color.WHITE);
    loading.setGravity(android.view.Gravity.CENTER); loading.setBackgroundColor(Color.rgb(9,10,11));
    setContentView(loading);
    String saved = getPreferences(0).getString("host", "magooesp.local");
    network.execute(() -> {
      String selected = saved;
      if (!reachable(saved) && !"192.168.4.1".equals(saved) && reachable("192.168.4.1")) selected="192.168.4.1";
      final String chosen=selected;
      runOnUiThread(() -> { if (!isFinishing() && !isDestroyed()) openDashboard(chosen); });
    });
  }

  private boolean reachable(String address) {
    HttpURLConnection c=null;
    try {
      c=(HttpURLConnection)new URL("http://"+address+"/status").openConnection();
      c.setConnectTimeout(1800); c.setReadTimeout(1800);
      return c.getResponseCode()==200;
    } catch (IOException e) { return false; }
    finally { if(c!=null)c.disconnect(); }
  }

  private void openDashboard(String address) {
    host=address;
    if(web!=null){web.stopLoading();web.destroy();}
    web=new WebView(this);
    web.setBackgroundColor(Color.rgb(9,10,11));
    web.getSettings().setJavaScriptEnabled(true);
    web.getSettings().setDomStorageEnabled(true);
    web.getSettings().setAllowFileAccess(false);
    web.getSettings().setAllowContentAccess(false);
    web.setWebViewClient(new WebViewClient() {
      @Override public void onPageFinished(WebView view, String url) {
        super.onPageFinished(view,url);
        Uri uri=Uri.parse(url);
        if ("http".equals(uri.getScheme()) && host.equalsIgnoreCase(uri.getHost()) && "/".equals(uri.getPath())) checkForUpdate();
      }
      @Override public WebResourceResponse shouldInterceptRequest(WebView view, WebResourceRequest request) {
        Uri uri=request.getUrl();
        if (!"GET".equals(request.getMethod()) || !"http".equals(uri.getScheme()) || !host.equalsIgnoreCase(uri.getHost())) return null;
        String path=uri.getPath();
        String name="/".equals(path)?"dashboard.html":path.substring(1);
        if(!ASSETS.contains(name))return null;
        try {
          String mime=name.endsWith(".html")?"text/html":name.endsWith(".css")?"text/css":name.endsWith(".js")?"text/javascript":"image/jpeg";
          return new WebResourceResponse(mime,name.endsWith(".jpg")?null:"UTF-8",getAssets().open(name));
        } catch (IOException e) { return null; }
      }
      @Override public boolean shouldOverrideUrlLoading(WebView view, WebResourceRequest request) {
        if(!request.isForMainFrame())return false;
        Uri uri=request.getUrl();
        if("magoo".equals(uri.getScheme()) && "address".equals(uri.getHost())){showAddressDialog();return true;}
        if("/app.apk".equals(uri.getPath()) && host.equalsIgnoreCase(uri.getHost())){
          startActivity(new Intent(MainActivity.this,GpsActivity.class).putExtra("host",host)); return true;
        }
        if("http".equals(uri.getScheme()) && "192.168.4.1".equals(uri.getHost()) && "/".equals(uri.getPath())){
          openDashboard("192.168.4.1");return true;
        }
        if(("http".equals(uri.getScheme()) || "https".equals(uri.getScheme())) && !host.equalsIgnoreCase(uri.getHost())){
          startActivity(new Intent(Intent.ACTION_VIEW,uri)); return true;
        }
        return false;
      }
    });
    setContentView(web,new ViewGroup.LayoutParams(-1,-1));
    web.loadUrl("http://"+host+"/?app=1");
  }

  private void checkForUpdate() {
    if (isFinishing() || isDestroyed() || !updateChecking.compareAndSet(false,true)) return;
    updates.execute(() -> {
      HttpURLConnection c=null;
      try {
        c=(HttpURLConnection)new URL(UPDATE_URL).openConnection();
        c.setInstanceFollowRedirects(false); c.setConnectTimeout(4000); c.setReadTimeout(4000);
        c.setRequestProperty("Cache-Control","no-cache");
        if (c.getResponseCode()!=200 || c.getContentLength()>4096) return;
        ByteArrayOutputStream data=new ByteArrayOutputStream();
        try (InputStream in=c.getInputStream()) {
          byte[] buf=new byte[512]; int n;
          while ((n=in.read(buf))!=-1) { if (data.size()+n>4096) return; data.write(buf,0,n); }
        }
        JSONObject json=new JSONObject(new String(data.toByteArray(),StandardCharsets.UTF_8));
        Object version=json.opt("versionCode");
        if (!(version instanceof Number)) return;
        final long available=((Number)version).longValue();
        final String name=json.optString("versionName","");
        if (available<=0 || available>2100000000L || name.length()>64) return;
        PackageInfo info=getPackageManager().getPackageInfo(getPackageName(),0);
        long installed=Build.VERSION.SDK_INT>=28 ? info.getLongVersionCode() : info.versionCode;
        if (available<=installed) return;
        runOnUiThread(() -> {
          if (!resumed || isFinishing() || isDestroyed() || shownUpdate==available) return;
          shownUpdate=available;
          new AlertDialog.Builder(this).setTitle("Magoo update available")
            .setMessage(name.isEmpty()?"A new version is ready to download.":"Version "+name+" is ready to download.")
            .setNegativeButton("Later",null)
            .setPositiveButton("Download",(dialog,which)->startActivity(new Intent(Intent.ACTION_VIEW,Uri.parse(DOWNLOAD_URL))))
            .show();
        });
      } catch (Exception ignored) {
        // Magoo remains usable when internet or the update service is unavailable.
      } finally { if(c!=null)c.disconnect(); updateChecking.set(false); }
    });
  }

  private void showAddressDialog() {
    EditText field=new EditText(this);field.setSingleLine(true);field.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI);field.setText(host);
    AlertDialog dialog=new AlertDialog.Builder(this).setTitle("Magoo address")
      .setMessage("Use magooesp.local on your network, or 192.168.4.1 when connected to Magoo’s Wi-Fi.")
      .setView(field).setNegativeButton("Cancel",null).setPositiveButton("Connect",null).create();
    dialog.setOnShowListener(v->dialog.getButton(AlertDialog.BUTTON_POSITIVE).setOnClickListener(button->{
      String value=field.getText().toString().trim();
      if(!value.matches("[A-Za-z0-9.-]+") || value.length()>253){field.setError("Enter a hostname or IP address");return;}
      getPreferences(0).edit().putString("host",value).apply();dialog.dismiss();openDashboard(value);
    }));
    dialog.show();
  }

  @Override protected void onResume(){super.onResume();resumed=true;if(web!=null)web.evaluateJavascript("window.__magooSetAppActive&&window.__magooSetAppActive(true)",null);checkForUpdate();}
  @Override protected void onPause(){
    resumed=false;
    if(web!=null)web.evaluateJavascript("if(typeof releaseWinch==='function')releaseWinch();if(window.__magooSetAppActive)window.__magooSetAppActive(false)",null);
    super.onPause();
  }
  @Override protected void onDestroy(){network.shutdownNow();updates.shutdownNow();if(web!=null){web.stopLoading();web.destroy();web=null;}super.onDestroy();}
  @Override public void onBackPressed(){if(web!=null && web.canGoBack())web.goBack();else super.onBackPressed();}
}

package ca.magoo.gps;

import java.io.IOException;

/** Reuse the ESP's map only while its token and the exact pixels still match. */
final class MapTransport {
  interface Age { long milliseconds() throws IOException; }
  interface Poster { Response post(String address,String path,String body) throws IOException; }
  static final class Response {
    final int status;final String body;
    Response(int status,String body) { this.status=status;this.body=body; }
  }
  private String lastAddress="",lastPixels="",token="";
  private boolean refreshSupported=true;

  void send(String address,String pixels,String bearing,Age age,Poster poster) throws IOException {
    if (!address.equals(lastAddress)) { token="";refreshSupported=true; }
    if (refreshSupported && !token.isEmpty() && pixels.equals(lastPixels) && address.equals(lastAddress)) {
      Response response=poster.post(address,"/map/refresh","token="+token+"&bearing="+bearing+"&ageMs="+freshAge(age));
      if (response.status==200) return;
      if (response.status==404 || response.status==405) refreshSupported=false;
      else if (response.status!=409) throw new IOException("Map refresh failed");
    }
    Response response=poster.post(address,"/map","pixels="+pixels+"&bearing="+bearing+"&ageMs="+freshAge(age));
    if (response.status!=200) throw new IOException("Map upload failed");
    String result=response.body==null?"":response.body.trim();
    token=result.matches("OK:[0-9a-f]{1,8}")?result.substring(3):"";
    lastAddress=address;lastPixels=pixels;
  }
  private static long freshAge(Age age) throws IOException {
    long value=age.milliseconds();
    if (value<0 || value>25000) throw new IOException("Location fix expired");
    return value;
  }
}

package ca.magoo.gps;

public class MapTransportTest {
  static class Fake implements MapTransport.Poster {
    int full,refresh; int refreshStatus=200; String reply="OK:abc";
    public MapTransport.Response post(String address,String path,String body) {
      if (path.equals("/map/refresh")) {
        refresh++;
        if (!body.contains("token=abc") || !body.contains("bearing=91")) throw new AssertionError("refresh payload");
        return new MapTransport.Response(refreshStatus,"OK");
      }
      full++;
      if (!body.contains("pixels=1234")) throw new AssertionError("full payload");
      return new MapTransport.Response(200,reply);
    }
  }
  public static void main(String[] args) throws Exception {
    MapTransport transport=new MapTransport();Fake fake=new Fake();
    transport.send("magooesp.local","1234","90",()->10,fake);
    if (fake.full!=1 || fake.refresh!=0) throw new AssertionError("first map");
    transport.send("magooesp.local","1234","91",()->20,fake);
    if (fake.full!=1 || fake.refresh!=1) throw new AssertionError("stationary refresh");
    fake.refreshStatus=409;
    transport.send("magooesp.local","1234","91",()->30,fake);
    if (fake.full!=2 || fake.refresh!=2) throw new AssertionError("server reset fallback");
    fake.refreshStatus=404;
    transport.send("magooesp.local","1234","91",()->30,fake);
    transport.send("magooesp.local","1234","91",()->30,fake);
    if (fake.full!=4 || fake.refresh!=3) throw new AssertionError("legacy firmware fallback");
    transport.send("192.168.4.1","1234","91",()->30,fake);
    if (fake.full!=5) throw new AssertionError("different device must get a full map");
    try { transport.send("magooesp.local","1234","91",()->26000,fake);throw new AssertionError("stale location accepted"); }
    catch (java.io.IOException expected) {}
    System.out.println("PASS: map refresh, stale rejection and compatibility fallback");
  }
}

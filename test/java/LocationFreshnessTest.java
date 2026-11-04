package ca.magoo.gps;

import java.lang.reflect.Method;

public final class LocationFreshnessTest {
  public static void main(String[] args) throws Exception {
    final Method fresh;
    try {
      fresh = Class.forName("ca.magoo.gps.LocationFreshness")
          .getDeclaredMethod("isFresh", long.class, long.class);
      fresh.setAccessible(true);
    } catch (ClassNotFoundException | NoSuchMethodException ex) {
      throw new AssertionError("GPS freshness policy is missing", ex);
    }
    long now = 100_000_000_000L;
    check((Boolean) fresh.invoke(null, now - 5_000_000_000L, now), "recent fix");
    check((Boolean) fresh.invoke(null, now - 25_000_000_000L, now), "boundary fix");
    check(!(Boolean) fresh.invoke(null, now - 25_000_000_001L, now), "stale fix");
    check(!(Boolean) fresh.invoke(null, 0L, now), "missing fix timestamp");
    check(!(Boolean) fresh.invoke(null, now + 1L, now), "future fix timestamp");
    System.out.println("PASS: GPS fix freshness");
  }
  private static void check(boolean condition, String name) {
    if (!condition) throw new AssertionError(name);
  }
}

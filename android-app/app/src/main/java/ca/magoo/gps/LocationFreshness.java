package ca.magoo.gps;

final class LocationFreshness {
  static final long MAX_AGE_NANOS = 25_000_000_000L;

  private LocationFreshness() {}

  static boolean isFresh(long fixNanos, long nowNanos) {
    return fixNanos > 0 && nowNanos >= fixNanos && nowNanos - fixNanos <= MAX_AGE_NANOS;
  }
}

package serializer.example;

import java.util.Arrays;

/** Reuse application-owned output storage while preserving exact native message bytes. */
public final class Main {
  /** Encode all four protocols into a nonzero-offset window and consume only the returned byte count. */
  public static void main(String[] args) {
    final int capacityBytes = 1024;
    final int prefixBytes = 7;
    final int suffixBytes = 8;
    final byte sentinel = 90;
    byte[] storage = new byte[capacityBytes];
    Arrays.fill(storage, sentinel);
    var account = new AccountSchema.Demo.Account();
    account.accountId = -1L;
    account.samples = new int[]{0, 42, -1};
    for (var protocol : AccountSchema.Protocol.values()) {
      account.displayName = "reused " + protocol;
      int written = account.encode(protocol, storage, prefixBytes, storage.length-prefixBytes-suffixBytes);
      byte[] message = Arrays.copyOfRange(storage, prefixBytes, prefixBytes+written);
      if (!Arrays.equals(message, account.encode(protocol))) throw new AssertionError("Wire bytes changed");
      var decoded = AccountSchema.Demo.Account.decode(message, protocol);
      if (decoded.accountId != account.accountId || !decoded.displayName.equals(account.displayName) ||
          !Arrays.equals(decoded.samples, account.samples)) throw new AssertionError("Value mismatch");
      for (int index = 0; index < prefixBytes; ++index) {
        if (storage[index] != sentinel) throw new AssertionError("Prefix changed");
      }
      for (int index = storage.length-suffixBytes; index < storage.length; ++index) {
        if (storage[index] != sentinel) throw new AssertionError("Suffix changed");
      }
      System.out.println(protocol + ": " + written + " bytes in reused storage");
    }
  }
}

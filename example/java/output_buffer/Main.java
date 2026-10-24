package serializer.example;

import java.nio.ByteBuffer;
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
    ByteBuffer[] nioStorage = {ByteBuffer.allocate(capacityBytes), ByteBuffer.allocateDirect(capacityBytes)};
    for (var nio : nioStorage) {
      for (int index = 0; index < nio.capacity(); ++index) {
        nio.put(index, sentinel);
      }
    }
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
      for (var nio : nioStorage) {
        nio.clear(); nio.position(prefixBytes); nio.limit(nio.capacity() - suffixBytes);
        int nioWritten = account.encode(protocol, nio);
        if (nioWritten != written || nio.position() != prefixBytes + written) {
          throw new AssertionError("NIO count/cursor mismatch");
        }
        for (int index = 0; index < written; ++index) {
          if (nio.get(prefixBytes + index) != message[index]) {
            throw new AssertionError("NIO wire bytes changed");
          }
        }
        var inspection = nio.duplicate(); inspection.clear();
        for (int index = 0; index < inspection.capacity(); ++index) {
          if ((index < prefixBytes || index >= inspection.capacity() - suffixBytes) && inspection.get(index) != sentinel) {
            throw new AssertionError("NIO outside-window bytes changed");
          }
        }
      }
      System.out.println(protocol + ": " + written + " bytes in reused array and heap/direct NIO storage");
    }
  }
}

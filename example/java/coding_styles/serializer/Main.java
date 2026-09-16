package serializer.example;

import java.nio.charset.StandardCharsets;
import java.util.Arrays;

/** Run a dependency-free example of each supported wire protocol. */
public final class Main {
  /** Populate and round-trip an owning object through all four protocols. */
  public static void main(String[] args) {
    AccountSchema.Demo.Account account = new AccountSchema.Demo.Account();
    account.base0.recordId = 42;
    account.accountId = -1L;
    account.displayName = "Ada \"Lovelace\" \uD83D\uDE80";
    account.scores = new int[] {-1, 0, 100};
    account.counters.put("visits", 7);
    account.payloadIndex = 1;
    account.payloadRatio = 1.5f;
    account.history = new AccountSchema.Demo.AccountState[] {
        AccountSchema.Demo.AccountState.WAITING_FOR_REVIEW, AccountSchema.Demo.AccountState.ACTIVE};
    for (AccountSchema.Protocol protocol : AccountSchema.Protocol.values()) {
      byte[] encoded = account.encode(protocol);
      AccountSchema.Demo.Account decoded = AccountSchema.Demo.Account.decode(encoded, protocol);
      if (decoded.base0.recordId != 42 || decoded.accountId != -1L
          || !decoded.displayName.equals(account.displayName)
          || !Arrays.equals(decoded.scores, account.scores)
          || !decoded.counters.equals(account.counters)
          || decoded.payloadIndex != 1 || decoded.payloadRatio != 1.5f
          || decoded.small != -128 || Short.toUnsignedInt(decoded.medium) != 65535
          || decoded.balance != 12.5 || !decoded.enabled
          || !Arrays.equals(decoded.history, account.history)) {
        throw new AssertionError("Round trip failed: " + protocol);
      }
      System.out.println(protocol + ": " + encoded.length + " bytes");
      if (protocol == AccountSchema.Protocol.JSON) {
        System.out.println(new String(encoded, StandardCharsets.UTF_8));
      }
    }
  }
}

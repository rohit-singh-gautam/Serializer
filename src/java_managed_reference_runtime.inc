// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"java(
  /** Nonowning typed wire identity; construction/decoding never establishes target liveness. */
  public static final class ManagedReference<T> {
    public final long documentHigh;
    public final long documentLow;
    public final long classId;
    public final long objectId;
    /** Retain untrusted raw unsigned bits until the owning field validates its exact target. */
    public ManagedReference(long documentHigh, long documentLow, long classId, long objectId) {
      this.documentHigh = documentHigh;
      this.documentLow = documentLow;
      this.classId = classId;
      this.objectId = objectId;
    }
    /** Validate document scope, exact type and the selected unsigned native object-ID width. */
    private void validate(long expectedClassId, int idBytes) {
      if ((documentHigh == 0 && documentLow == 0) || objectId == 0 || classId != expectedClassId
          || (idBytes == java.lang.Integer.BYTES && (objectId < 0 || objectId > 0xffffffffL))) {
        throw new IllegalArgumentException("Invalid managed entity reference envelope");
      }
    }
    /** Encode all metadata in C++ order; only object identity follows the compact policy. */
    private void write(Writer out, long expectedClassId, int idBytes, boolean compact) {
      validate(expectedClassId, idBytes);
      out.beginObject();
      out.field(1, "document_high", true); out.integer(documentHigh, java.lang.Long.BYTES, true);
      out.field(2, "document_low", false); out.integer(documentLow, java.lang.Long.BYTES, true);
      out.field(3, "class_id", false); out.integer(classId, java.lang.Long.BYTES, true);
      out.field(4, "object_id", false);
      if (compact) out.compactValue(objectId, idBytes, false, true);
      else out.integer(objectId, idBytes, true);
      out.endObject();
    }
    /** Decode a fresh complete envelope and reject unknown, duplicate or missing metadata. */
    private static <T> ManagedReference<T> read(Reader in, long expectedClassId, int idBytes, boolean compact) {
      in.beginObject();
      in.consume("allocation", 4L * java.lang.Long.BYTES);
      long[] values = new long[4];
      int seen = 0;
      if (in.protocol == Protocol.BINARY_NONE) {
        for (int index = 0; index < values.length; ++index) {
          values[index] = index == 3 && compact ? in.compactValue(idBytes, false)
              : in.integer(index == 3 ? idBytes : java.lang.Long.BYTES, true);
        }
        seen = (1 << values.length) - 1;
      } else {
        boolean first = true;
        while (in.protocol == Protocol.BINARY_INTEGER || !in.json() || in.nextObject(first)) {
          first = false;
          int field;
          if (in.protocol == Protocol.BINARY_INTEGER) {
            field = in.compact();
            if (field == 0) break;
          } else {
            String key = in.key();
            if (!in.json() && key.isEmpty()) break;
            switch (key) {
              case "document_high": field = 1; break;
              case "document_low": field = 2; break;
              case "class_id": field = 3; break;
              case "object_id": field = 4; break;
              default: throw in.error("Unknown managed reference metadata name");
            }
          }
          if (field < 1 || field > values.length) throw in.error("Unknown managed reference metadata field");
          int mask = 1 << (field - 1);
          if ((seen & mask) != 0) throw in.error("Duplicate managed reference metadata field");
          seen |= mask;
          values[field - 1] = field == 4 && compact ? in.compactValue(idBytes, false)
              : in.integer(field == 4 ? idBytes : java.lang.Long.BYTES, true);
        }
      }
      in.endObject();
      if (seen != (1 << values.length) - 1) throw in.error("Managed reference metadata is incomplete");
      var result = new ManagedReference<T>(values[0], values[1], values[2], values[3]);
      result.validate(expectedClassId, idBytes);
      return result;
    }
  }
)java"

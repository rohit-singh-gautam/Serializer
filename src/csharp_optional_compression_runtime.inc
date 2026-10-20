// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"compresscs(    /// <summary>Caller-selected managed provider assembly; contexts remain private to each operation.</summary>
    public sealed class Provider {
      public Format Format { get; }
      public int MinimumLevel { get; }
      public int MaximumLevel { get; }
      internal readonly object Api;
      // Resolve public capabilities without probing unrelated assemblies or accepting mutable codec contexts.
      private Provider(global::System.Reflection.Assembly assembly, Format format) {
        if (assembly == null) throw new Error(Code.Unavailable, "Optional provider assembly is absent");
        Format = format;
        if (format == Format.Zstd) {
          var api = new ZstdApi(assembly); Api = api;
          MinimumLevel = api.MinimumLevel; MaximumLevel = api.MaximumLevel;
        } else {
          var api = new Lz4Api(assembly); Api = api;
          MinimumLevel = api.MinimumLevel; MaximumLevel = api.MaximumLevel;
        }
      }
      /// <summary>Bind an explicitly supplied ZstdSharp.Port assembly; the caller owns its load context.</summary>
      public static Provider ZstdSharp(global::System.Reflection.Assembly assembly) {
        try { return new Provider(assembly, Format.Zstd); }
        catch (global::System.MissingMemberException) { throw new Error(Code.Unavailable, "ZstdSharp public API is unavailable"); }
        catch (global::System.TypeLoadException) { throw new Error(Code.Unavailable, "ZstdSharp public API is unavailable"); }
      }
      /// <summary>Bind the safe byte-array API of an explicitly supplied K4os.Compression.LZ4 assembly.</summary>
      public static Provider Lz4(global::System.Reflection.Assembly assembly) {
        try { return new Provider(assembly, Format.Lz4); }
        catch (global::System.MissingMemberException) { throw new Error(Code.Unavailable, "K4os LZ4 public API is unavailable"); }
        catch (global::System.TypeLoadException) { throw new Error(Code.Unavailable, "K4os LZ4 public API is unavailable"); }
      }
      // Reject mismatched selection and provider-native level values before application callbacks.
      internal void Admit(Format format, int? level) {
        if (format != Format) throw new Error(Code.InvalidOptions, "Optional provider format mismatch");
        if (level.HasValue) {
          if (Api is ZstdApi zstd) zstd.ValidateLevel(level.Value);
          else ((Lz4Api)Api).ValidateLevel(level.Value);
        }
      }
    }
    /// <summary>Probe only the explicitly selected provider; default availability remains dependency-free.</summary>
    public static bool Available(Format format, Provider? provider) {
      CheckFormat(format);
      return provider == null ? Available(format) : provider.Format == format;
    }
    // Preserve the exact first policy exception if a managed provider wraps or catches a callback failure.
    private sealed class OptionalCheck {
      private readonly global::System.Action? callback;
      internal global::System.Exception? Failure { get; private set; }
      // Retain the immutable policy without invoking application code during private setup.
      internal OptionalCheck(global::System.Action? callback) { this.callback = callback; }
      // Check retained failure on both sides of every policy call.
      internal void Poll() {
        if (Failure != null) global::System.Runtime.ExceptionServices.ExceptionDispatchInfo.Capture(Failure).Throw();
        try { callback?.Invoke(); } catch (global::System.Exception failure) { Failure = failure; throw; }
        if (Failure != null) global::System.Runtime.ExceptionServices.ExceptionDispatchInfo.Capture(Failure).Throw();
      }
      // Rethrow the first callback error without changing its object or original stack provenance.
      internal void Rethrow() {
        if (Failure != null) global::System.Runtime.ExceptionServices.ExceptionDispatchInfo.Capture(Failure).Throw();
      }
    }
    // Resolve only exact public overloads; absence is a provider-deployment admission failure.
    private static global::System.Type OptionalType(global::System.Reflection.Assembly assembly, string name) {
      return assembly.GetType(name, false) ?? throw new global::System.TypeLoadException();
    }
    // Resolve public methods without selecting unsafe pointer or byref-like Span overloads.
    private static global::System.Reflection.MethodInfo OptionalMethod(global::System.Type type, string name, params global::System.Type[] parameters) {
      return type.GetMethod(name, parameters) ?? throw new global::System.MissingMethodException();
    }
    // Treat absent public enum capabilities as unavailable without masking unrelated provider exceptions.
    private static object OptionalEnum(global::System.Type type, string name) {
      if (!type.IsEnum || global::System.Enum.GetUnderlyingType(type) != typeof(int) || !global::System.Enum.IsDefined(type, name))
        throw new global::System.MissingFieldException();
      return global::System.Enum.Parse(type, name);
    }
    // Unwrap only reflection's transport exception and preserve application/provider object identity.
    private static object? OptionalInvoke(global::System.Reflection.MethodInfo method, object? target, params object?[] parameters) {
      try { return method.Invoke(target, parameters); }
      catch (global::System.Reflection.TargetInvocationException failure) when (failure.InnerException != null) {
        global::System.Runtime.ExceptionServices.ExceptionDispatchInfo.Capture(failure.InnerException).Throw(); throw;
      }
    }
    // Construct one operation-owned managed codec without imposing a package reference on emitted source.
    private static object OptionalConstruct(global::System.Reflection.ConstructorInfo constructor, params object?[] parameters) {
      try { return constructor.Invoke(parameters); }
      catch (global::System.Reflection.TargetInvocationException failure) when (failure.InnerException != null) {
        global::System.Runtime.ExceptionServices.ExceptionDispatchInfo.Capture(failure.InnerException).Throw(); throw;
      }
    }
    // Read one checked unsigned standard field independently of CLR host byte order.
    private static ulong OptionalLittle(byte[] input, int offset, int count) {
      Require(input, offset, count); ulong value = 0;
      for (int index = 0; index < count; ++index) value |= (ulong)input[offset + index] << (8 * index);
      return value;
    }
    // Emit an explicit four-byte standard field without depending on signed intermediate values.
    private static void OptionalWrite(BoundedOutput output, uint value) {
      byte[] field = new byte[4];
      global::System.Buffers.Binary.BinaryPrimitives.WriteUInt32LittleEndian(field, value); output.Write(field, 0, field.Length);
    }
    // Release private contexts without masking a previously observed application/resource failure.
    private static void OptionalDispose(global::System.IDisposable? context) { try { context?.Dispose(); } catch { } }
)compresscs"

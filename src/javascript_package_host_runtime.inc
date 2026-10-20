// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"packagejs(
  let filesystemHost = null, hostLoading = null;
  /** Explicit Node filesystem admission; module import itself never resolves Node builtins. */
  async function nodeHost() {
    if (filesystemHost) return filesystemHost;
    if (!hostLoading) hostLoading = (async () => {
      let fs, path;
      try { [fs, path] = await NativePromise.all([import('node:fs/promises'), import('node:path')]); }
      catch { throw new PackageError(Code.Unavailable, 'Node filesystem host is unavailable'); }
      // Register cleanup privately so borrowed lookalike sources cannot transfer ownership.
      async function opened(file, cleanup) {
        try {
          const stat = await file.stat(); if (!stat.isFile()) throw new PackageError(Code.InvalidOptions, 'Package source must be a regular file');
          const source = Object.freeze({size: integer(stat.size, 'file size'), async readAt(offset, count) { const data = new Uint8Array(count); const read = await file.read(data, 0, count, offset); return data.subarray(0, read.bytesRead); }});
          let live = true; privateSources.set(source, async () => { if (!live) return; live = false; privateSources.delete(source); try { await file.close(); } finally { if (cleanup) await cleanup(); } }); return source;
        } catch (error) { try { await file.close(); } finally { if (cleanup) await cleanup(); } throw error; }
      }
      // Sequential inputs require an explicit existing spool directory and independent byte ceiling.
      async function open(input, options, budget) {
        if (typeof input === 'string') { budget.check(); const file = await fs.open(input, 'r'); try { budget.check(); } catch (error) { await file.close(); throw error; } return opened(file, null); }
        if (!input || typeof input.read !== 'function' || typeof options.spoolDirectory !== 'string' || !options.spoolDirectory) throw new PackageError(Code.InvalidOptions, 'Sequential package input requires an explicit spool directory');
        const maximum = options.maxSpoolBytes === undefined ? budget.limits.maxArchiveBytes : integer(options.maxSpoolBytes, 'spool limit');
        const state = new State(budget); state.poll(); const directory = await fs.mkdtemp(path.join(options.spoolDirectory, 'serializer-package-')); let file = null, transferred = false;
        const cleanup = async () => { await fs.rm(directory, {recursive: true, force: true}); };
        try {
          state.poll(); await fs.chmod(directory, 0o700); state.poll(); file = await fs.open(path.join(directory, 'archive.zip'), 'wx+', 0o600); state.poll(); let size = 0;
          while (true) {
            state.poll(); const request = Math.min(chunkBytes, Math.min(maximum, budget.limits.maxArchiveBytes) - size + 1); budget.consume('workUnits', 1);
            let data = input.read(request); if (data && typeof data.then === 'function') data = await data; state.poll();
            if (data === null) break;
            if (!(data instanceof Uint8Array) || !data.length || data.length > request) throw new PackageError(Code.InvalidArchive, 'Sequential source must make bounded progress');
            if (data.length > maximum - size || data.length > budget.limits.maxArchiveBytes - size) throw new PackageError(Code.ResourceLimit, 'Package spool extent exceeds limit');
            const release = budget.reserve(data.length); try {
              const owned = copyBytes(data, state); budget.consume('ioBytes', owned.length);
              let at = 0; while (at < owned.length) { state.poll(); const result = await file.write(owned, at, owned.length - at, size + at); state.poll(); if (!result.bytesWritten) throw new PackageError(Code.InvalidArchive, 'Spool write made no progress'); budget.consume('ioBytes', result.bytesWritten); at += result.bytesWritten; }
            } finally { release(); } size += data.length;
          }
          state.poll(); const result = await opened(file, cleanup); transferred = true; return result;
        } finally { if (!transferred) { if (file) await file.close(); await cleanup(); } }
      }
      // Same-directory exclusive staging, validation before rename and explicit uncertain durability.
      async function save(destination, data, budget, fault = null) {
        if (typeof destination !== 'string' || !destination || !(data instanceof Uint8Array) || fault !== null && typeof fault !== 'function') throw new PackageError(Code.InvalidOptions, 'Invalid atomic package save arguments');
        const state = new State(budget); let directory = null, file = null, published = false;
        // Fault callbacks are synchronous and fenced exactly like cancellation callbacks.
        const checkpoint = stage => { state.poll(); if (fault) { const result = fault(stage); if (result && typeof result.then === 'function') { NativePromise.resolve(result).catch(() => {}); throw new PackageError(Code.InvalidOptions, 'Fault callback must be synchronous'); } } state.poll(); };
        try {
          checkpoint('before_create'); directory = await fs.mkdtemp(path.join(path.dirname(destination), '.serializer-package-')); state.poll(); await fs.chmod(directory, 0o700); state.poll();
          const staged = path.join(directory, 'archive.zip'); file = await fs.open(staged, 'wx', 0o600); state.poll();
          for (let at = 0; at < data.length;) { state.poll(); const count = Math.min(chunkBytes, data.length - at); budget.consume('ioBytes', count); const result = await file.write(data, at, count, null); state.poll(); if (!result.bytesWritten) throw new PackageError(Code.InvalidArchive, 'Save write made no progress'); at += result.bytesWritten; }
          checkpoint('before_file_sync'); await file.sync(); state.poll(); await file.close(); file = null; checkpoint('before_replace');
          await fs.rename(staged, destination); published = true; checkpoint('after_replace');
          // POSIX directory synchronization is meaningful; Windows exposes no equivalent portable handle.
          if (path.sep === '/') { const parent = await fs.open(path.dirname(destination), 'r'); try { await parent.sync(); } finally { await parent.close(); } }
          checkpoint('after_directory_sync');
        } catch (error) { if (published) { const uncertain = new PackageError(Code.PublicationUncertain, 'Package was replaced but final durability is uncertain'); uncertain.cause = error; throw uncertain; } throw error; }
        finally { if (file) await file.close(); if (directory) await fs.rm(directory, {recursive: true, force: true}); }
      }
      filesystemHost = Object.freeze({open, save}); return filesystemHost;
    })();
    return hostLoading;
  }
)packagejs"

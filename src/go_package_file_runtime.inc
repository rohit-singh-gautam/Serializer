// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"packagego(
// SerializerPackageSaveFault injects deterministic publication-boundary failures without payload access.
// Points are before_temporary, after_temporary, before_file_sync, after_file_sync,
// before_replace, after_replace and after_directory_sync. Callbacks require sequential execution.
type SerializerPackageSaveFault func(point string) error

// srlPackageSavePoint polls around an application fault hook before continuing publication.
func srlPackageSavePoint(point string,fault SerializerPackageSaveFault,state *srlPackageState) error {
  if err:=state.poll();err!=nil { return err }
  if fault!=nil { if err:=fault(point);err!=nil { return state.fail(err) } }
  return state.poll()
}
// SerializerPackageWrite borrows a sink for one already prepared archive without close/flush calls.
// Failure can leave a prefix; returned n counts only bytes accepted by the caller's Writer.
func SerializerPackageWrite(destination srlPackageIO.Writer,data []byte,budgets ...*SerializerPackageBudget) (n int64,err error) {
  budget,err:=srlPackageBudget(budgets);if err!=nil { return 0,err }
  state:=srlPackageState{budget:budget};if err=state.begin();err!=nil { return 0,err };complete:=false
  defer func(){state.end(&err,complete)}()
  if destination==nil { return 0,srlPackageFailure(SerializerPackageInvalidOptions,"Missing package sink","") }
  if int64(len(data))>budget.limits.MaxArchiveBytes { return 0,srlPackageFailure(SerializerPackageResourceLimit,"Archive byte limit exceeded","") }
  n,err=srlPackageWrite(destination,data,&state,true);if err!=nil { return n,err };complete=true;return n,nil
}

// SerializerPackageSaveBytes validates a private snapshot before creating a synchronized sibling.
// The destination uses same-directory os.Rename replacement; atomicity/durability depend on the
// selected native filesystem. POSIX directory Sync is required; Windows has no portable Go directory Sync.
// Post-replacement failures return publication_uncertain with the exact original cause.
// Application panics propagate unchanged; an abandoned post-replacement acknowledgement is uncertain.
func SerializerPackageSaveBytes(path string,data []byte,budget *SerializerPackageBudget,fault SerializerPackageSaveFault) (err error) {
  budget,err=srlPackageBudget([]*SerializerPackageBudget{budget});if err!=nil { return err }
  state:=srlPackageState{budget:budget};if err=state.begin();err!=nil { return err };complete:=false
  var temporary string;var owned *srlPackageOS.File;published:=false;held:=int64(0)
  defer func(){
    if owned!=nil { owned.Close() }
    if temporary!="" { srlPackageOS.Remove(temporary) }
    budget.release(held)
    if published&&err!=nil {
      cause:=err;if state.first!=nil { cause=state.first }
      err=&SerializerPackageError{Code:SerializerPackagePublicationUncertain,Message:"Final package replaced; durability/publication acknowledgement failed",Cause:cause}
      state.first=err
    }
    state.end(&err,complete)
  }()
  if path=="" { return srlPackageFailure(SerializerPackageInvalidOptions,"Missing package destination path","") }
  size:=int64(len(data))
  if size>budget.limits.MaxArchiveBytes { return srlPackageFailure(SerializerPackageResourceLimit,"Archive byte limit exceeded","") }
  if err=state.consume(SerializerPackageDecoderAllocationBytes,size,"");err!=nil { return err }
  if err=budget.hold(size,"");err!=nil { return err };held=size
  snapshot:=append([]byte(nil),data...)
  // Borrow our own immutable snapshot through ReaderAt, avoiding a second full archive copy.
  reader,err:=srlPackageOpen(SerializerPackageSource{Reader:srlPackageBytes.NewReader(snapshot),SizeBytes:size},budget,&state)
  if err!=nil { return err }
  validation:=func() error {
    defer reader.Close()
    for _,entry:=range reader.entries { if _,failure:=reader.Read(entry.Name);failure!=nil { return failure } }
    return state.poll()
  }
  if err=validation();err!=nil { return err }
  if err=srlPackageSavePoint("before_temporary",fault,&state);err!=nil { return err }
  absolute,err:=srlPackageFilepath.Abs(path);if err!=nil { return err };directory:=srlPackageFilepath.Dir(absolute)
  owned,err=srlPackageOS.CreateTemp(directory,".serializer-save-*");if err!=nil { return err };temporary=owned.Name()
  if err=srlPackageSavePoint("after_temporary",fault,&state);err!=nil { return err }
  if _,err=srlPackageWrite(owned,snapshot,&state,true);err!=nil { return err }
  if err=srlPackageSavePoint("before_file_sync",fault,&state);err!=nil { return err }
  if err=owned.Sync();err!=nil { return err }
  if err=srlPackageSavePoint("after_file_sync",fault,&state);err!=nil { return err }
  if err=owned.Close();err!=nil { owned=nil;return err };owned=nil
  if err=srlPackageSavePoint("before_replace",fault,&state);err!=nil { return err }
  if err=srlPackageOS.Rename(temporary,absolute);err!=nil { return err };published=true;temporary=""
  if err=srlPackageSavePoint("after_replace",fault,&state);err!=nil { return err }
  if srlPackageRuntime.GOOS!="windows" {
    parent,failure:=srlPackageOS.Open(directory);if failure!=nil { return failure }
    syncFailure:=parent.Sync();closeFailure:=parent.Close()
    if syncFailure!=nil { return syncFailure };if closeFailure!=nil { return closeFailure }
  }
  if err=srlPackageSavePoint("after_directory_sync",fault,&state);err!=nil { return err }
  complete=true;return nil
}
)packagego"

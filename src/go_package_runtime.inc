// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"packagego(
// SerializerPackageCode classifies ZIP/OPC framing, metadata, resource and publication failures.
type SerializerPackageCode string
const (
  SerializerPackageInvalidOptions SerializerPackageCode = "invalid_options"
  SerializerPackageInvalidState SerializerPackageCode = "invalid_state"
  SerializerPackageInvalidArchive SerializerPackageCode = "invalid_archive"
  SerializerPackageUnsupportedFeature SerializerPackageCode = "unsupported_feature"
  SerializerPackageInvalidName SerializerPackageCode = "invalid_name"
  SerializerPackageDuplicateName SerializerPackageCode = "duplicate_name"
  SerializerPackageIntegrity SerializerPackageCode = "integrity"
  SerializerPackageResourceLimit SerializerPackageCode = "resource_limit"
  SerializerPackageInvalidMetadata SerializerPackageCode = "invalid_metadata"
  SerializerPackageInvalidRelationship SerializerPackageCode = "invalid_relationship"
  SerializerPackageMissingPart SerializerPackageCode = "missing_part"
  SerializerPackageSemantic SerializerPackageCode = "semantic"
  SerializerPackageCodec SerializerPackageCode = "codec"
  SerializerPackagePublicationUncertain SerializerPackageCode = "publication_uncertain"
  SerializerPackageDeadlineExceeded SerializerPackageCode = "deadline_exceeded"
)

// SerializerPackageError retains bounded logical context and never includes payload excerpts.
// Ordinary application cancellation and transport errors retain their exact error values.
type SerializerPackageError struct {
  Code SerializerPackageCode
  Message, Part, FieldPath, Reference, RelationshipID string
  OffsetBytes int64
  Cause error
}
// Error returns a payload-free diagnostic independent of native filesystem paths.
func (failure *SerializerPackageError) Error() string { return string(failure.Code)+": "+failure.Message }
// Unwrap preserves application causes for errors.Is/errors.As, including uncertain publication.
func (failure *SerializerPackageError) Unwrap() error { return failure.Cause }

// SerializerPackageDescriptor chooses exact seek-free descriptor framing for generated output.
type SerializerPackageDescriptor uint8
const (
  SerializerPackageDescriptorNone SerializerPackageDescriptor = iota
  SerializerPackageDescriptorSigned
  SerializerPackageDescriptorUnsigned
)
// SerializerPackageResource selects one cumulative logical counter shared by a complete operation.
type SerializerPackageResource uint8
const (
  SerializerPackageIOBytes SerializerPackageResource = iota
  SerializerPackageMetadataBytes
  SerializerPackageExpandedBytes
  SerializerPackageEntries
  SerializerPackageRelationships
  SerializerPackageDecoderAllocationBytes
  SerializerPackageWorkUnits
  SerializerPackageReferences
  SerializerPackagePreservedBytes
  srlPackageResourceCount
)
// SerializerPackageLimits is copied before source access; a zero value deliberately grants no capacity.
// Buffer charges bound live logical byte lengths. Allocation charges count byte results and
// conservative per-entry metadata estimates; Go maps, slice capacity, provider workspace and RSS
// require an independently bounded process when a hard physical-memory ceiling is necessary.
// Callbacks must return promptly; wrappers and ledgers require caller synchronization across goroutines.
type SerializerPackageLimits struct {
  MaxArchiveBytes, MaxIOBytes, MaxMetadataBytes, MaxPartBytes int64
  MaxTotalExpandedBytes, MaxBufferBytes, MaxEntries, MaxRelationships int64
  MaxDecoderAllocationBytes, MaxWorkUnits, MaxReferences, MaxPreservedBytes int64
  CheckCancelled func() error
  Deadline srlPackageTime.Time
  AllowSpool bool
  SpoolDirectory string
  MaxSpoolBytes int64
  ForceZIP64 bool
  Descriptor SerializerPackageDescriptor
}
// SerializerPackageDefaultLimits returns a fresh bounded standard-library profile without I/O.
func SerializerPackageDefaultLimits() SerializerPackageLimits {
  const mib = int64(1024*1024)
  return SerializerPackageLimits{MaxArchiveBytes:256*mib,MaxIOBytes:1024*mib,
    MaxMetadataBytes:16*mib,MaxPartBytes:64*mib,MaxTotalExpandedBytes:256*mib,
    MaxBufferBytes:128*mib,MaxEntries:4096,MaxRelationships:16384,
    MaxDecoderAllocationBytes:128*mib,MaxWorkUnits:512*mib,MaxReferences:65536,
    MaxPreservedBytes:64*mib,MaxSpoolBytes:256*mib,Descriptor:SerializerPackageDescriptorSigned}
}
// SerializerPackageBudget retains a sequential policy snapshot and cumulative charges.
// It owns no transport; sharing it across overlapping independent operations is unsupported.
type SerializerPackageBudget struct {
  limits SerializerPackageLimits
  used [srlPackageResourceCount]int64
  held int64
  checking bool
  first error
}
// SerializerPackageNewBudget validates one optional snapshot before callbacks, allocation or I/O.
func SerializerPackageNewBudget(policies ...SerializerPackageLimits) (*SerializerPackageBudget,error) {
  if len(policies)>1 { return nil,srlPackageFailure(SerializerPackageInvalidOptions,"Too many package policies","") }
  limits:=SerializerPackageDefaultLimits();if len(policies)==1 { limits=policies[0] }
  budget:=&SerializerPackageBudget{limits:limits}
  for resource:=SerializerPackageResource(0);resource<srlPackageResourceCount;resource++ {
    if budget.maximum(resource)<0 { return nil,srlPackageFailure(SerializerPackageInvalidOptions,"Negative package resource limit","") }
  }
  if limits.MaxArchiveBytes<0||limits.MaxPartBytes<0||limits.MaxBufferBytes<0||limits.MaxSpoolBytes<0||
      limits.Descriptor>SerializerPackageDescriptorUnsigned {
    return nil,srlPackageFailure(SerializerPackageInvalidOptions,"Invalid package byte or descriptor policy","")
  }
  if err:=budget.Check();err!=nil { return nil,err };return budget,nil
}
// Configured returns a detached policy copy; changing it cannot relax the retained ledger.
func (budget *SerializerPackageBudget) Configured() SerializerPackageLimits { return budget.limits }
// maximum derives one resource ceiling directly from its retained public policy field.
func (budget *SerializerPackageBudget) maximum(resource SerializerPackageResource) int64 {
  switch resource {
  case SerializerPackageIOBytes:return budget.limits.MaxIOBytes
  case SerializerPackageMetadataBytes:return budget.limits.MaxMetadataBytes
  case SerializerPackageExpandedBytes:return budget.limits.MaxTotalExpandedBytes
  case SerializerPackageEntries:return budget.limits.MaxEntries
  case SerializerPackageRelationships:return budget.limits.MaxRelationships
  case SerializerPackageDecoderAllocationBytes:return budget.limits.MaxDecoderAllocationBytes
  case SerializerPackageWorkUnits:return budget.limits.MaxWorkUnits
  case SerializerPackageReferences:return budget.limits.MaxReferences
  case SerializerPackagePreservedBytes:return budget.limits.MaxPreservedBytes
  default:return -1
  }
}
// Remaining inspects unspent capacity without invoking application callbacks; invalid keys return zero.
func (budget *SerializerPackageBudget) Remaining(resource SerializerPackageResource) int64 {
  if resource>=srlPackageResourceCount { return 0 };return budget.maximum(resource)-budget.used[resource]
}
// Check polls cancellation/deadline and fences recursive or recovered-panicking policy calls.
// A panic propagates unchanged; the abandoned ledger cannot subsequently resume work.
func (budget *SerializerPackageBudget) Check() (err error) {
  if budget.first!=nil { return budget.first }
  if budget.checking { budget.first=srlPackageFailure(SerializerPackageInvalidState,"Reentrant package policy","");return budget.first }
  budget.checking=true;complete:=false
  defer func(){budget.checking=false;if !complete&&budget.first==nil { budget.first=srlPackageFailure(SerializerPackageInvalidState,"Abandoned package policy","") }}()
  if budget.limits.CheckCancelled!=nil { err=budget.limits.CheckCancelled() }
  if budget.first!=nil { err=budget.first }
  if err==nil&&!budget.limits.Deadline.IsZero()&&!srlPackageTime.Now().Before(budget.limits.Deadline) {
    err=srlPackageFailure(SerializerPackageDeadlineExceeded,"Package deadline exceeded","")
  }
  if err!=nil&&budget.first==nil { budget.first=err };complete=true;return err
}
// Consume validates a cumulative charge before the corresponding allocation or I/O.
func (budget *SerializerPackageBudget) Consume(resource SerializerPackageResource,amount int64,part string) error {
  if err:=budget.Check();err!=nil { return err };return budget.charge(resource,amount,part)
}
// charge updates a counter only after an enclosing wrapper has polled its lifecycle.
func (budget *SerializerPackageBudget) charge(resource SerializerPackageResource,amount int64,part string) error {
  if resource>=srlPackageResourceCount||amount<0||amount>budget.Remaining(resource) {
    return srlPackageFailure(SerializerPackageResourceLimit,"Package cumulative resource limit exceeded",part)
  }
  budget.used[resource]+=amount;return nil
}
// hold admits simultaneous private byte storage before allocating it.
func (budget *SerializerPackageBudget) hold(amount int64,part string) error {
  if amount<0||amount>budget.limits.MaxBufferBytes-budget.held {
    return srlPackageFailure(SerializerPackageResourceLimit,"Package private buffer limit exceeded",part)
  }
  budget.held+=amount;return nil
}
// release drops exactly one previously admitted internal byte reservation.
func (budget *SerializerPackageBudget) release(amount int64) { budget.held-=amount }

const (
  srlPackageChunkBytes = 32768
  srlPackageUint32Max int64 = 0xffffffff
  srlPackageEndBytes = 22
  srlPackageMaxCommentBytes = 65535
  srlPackageMaxEmptyReads = 100
)
// srlPackageFailure creates bounded logical diagnostics without retaining input payloads.
func srlPackageFailure(code SerializerPackageCode,message,part string) error {
  return &SerializerPackageError{Code:code,Message:message,Part:part}
}
// srlPackageBudget selects one borrowed ledger or validates an independent default profile.
func srlPackageBudget(budgets []*SerializerPackageBudget) (*SerializerPackageBudget,error) {
  if len(budgets)>1 { return nil,srlPackageFailure(SerializerPackageInvalidOptions,"Too many package ledgers","") }
  if len(budgets)==0||budgets[0]==nil { return SerializerPackageNewBudget() }
  if err:=budgets[0].Check();err!=nil { return nil,err };return budgets[0],nil
}
// srlPackageState connects nested parser/provider polls to its enclosing unpublished operation.
type srlPackageState struct {
  budget *SerializerPackageBudget
  parent *srlPackageState
  busy,closed bool
  first error
}
// fail preserves the first application, transport or package error without resuming a failed wrapper.
func (state *srlPackageState) fail(err error) error { if state.first==nil { state.first=err };return state.first }
// begin admits one synchronous operation and latches swallowed same-wrapper reentry.
func (state *srlPackageState) begin() error {
  if state.busy { return state.fail(srlPackageFailure(SerializerPackageInvalidState,"Reentrant package operation","")) }
  if state.closed||state.first!=nil {
    return &SerializerPackageError{Code:SerializerPackageInvalidState,Message:"Inactive package operation",Cause:state.first}
  }
  state.busy=true;return nil
}
// active checks enclosing lifetime/failure before another borrowed callback or result publication.
func (state *srlPackageState) active() error {
  if state.first!=nil { return state.first }
  if state.closed { return state.fail(srlPackageFailure(SerializerPackageInvalidState,"Closed package operation","")) }
  if state.parent!=nil { if err:=state.parent.active();err!=nil { return state.fail(err) } }
  return nil
}
// poll checks activity on both sides of every application policy callback.
func (state *srlPackageState) poll() error {
  if err:=state.active();err!=nil { return err }
  if err:=state.budget.Check();err!=nil { return state.fail(err) };return state.active()
}
// consume admits a charge only after the complete wrapper hierarchy remains active.
func (state *srlPackageState) consume(resource SerializerPackageResource,amount int64,part string) error {
  if err:=state.poll();err!=nil { return err };return state.budget.charge(resource,amount,part)
}
// close fences callbacks immediately; resource owners release private staging after their borrow ends.
func (state *srlPackageState) close() {
  state.closed=true;if state.busy&&state.first==nil { state.first=srlPackageFailure(SerializerPackageInvalidState,"Closed active package operation","") }
}
// end latches normal errors and fences recovered panics without replacing their original panic values.
func (state *srlPackageState) end(err *error,complete bool) {
  if *err!=nil { *err=state.fail(*err);state.closed=true } else if !complete {
    state.close();if state.first==nil { state.first=srlPackageFailure(SerializerPackageInvalidState,"Abandoned package operation","") }
  }
  state.busy=false
}
// srlPackageArray validates signed logical and native allocation widths before narrowing.
func srlPackageArray(amount,maximum int64,part string) (int,error) {
  if amount<0||amount>maximum||uint64(amount)>uint64(^uint(0)>>1) {
    return 0,srlPackageFailure(SerializerPackageResourceLimit,"Package byte extent exceeds native allocation policy",part)
  }
  return int(amount),nil
}
// srlPackageNumber reads an exact little-endian wire field without overflow or unchecked indexing.
func srlPackageNumber(data []byte,position,width int) (int64,error) {
  if position<0||width<0||width>8||position>len(data)||width>len(data)-position {
    return 0,srlPackageFailure(SerializerPackageInvalidArchive,"Truncated ZIP wire field","")
  }
  var value uint64;for index:=0;index<width;index++ { value|=uint64(data[position+index])<<uint(index*8) }
  if value>uint64(1<<63-1) { return 0,srlPackageFailure(SerializerPackageResourceLimit,"Unsigned ZIP64 extent exceeds native range","") }
  return int64(value),nil
}
)packagego"

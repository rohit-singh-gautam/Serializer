// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD

//go:build windows

package main

import (
  "fmt"
  "path/filepath"
  "runtime"
  "sync"
  "sync/atomic"
  "syscall"
  "unsafe"
)

// Load only the explicitly selected test DLL; generated models retain their existing provider callback API.
type providerBridge struct {
  library *syscall.DLL
  prepare, validate, release, diagnostic, version *syscall.Proc
}

// Resolve every fixed C ABI entry before publishing a usable provider bridge.
func loadProviderBridge(path string) (*providerBridge,error) {
  if !filepath.IsAbs(path) || unsafe.Sizeof(uintptr(0))!=8 {return nil,fmt.Errorf("Prepared provider qualification requires an absolute native64 Windows DLL")}
  // Resolve dependencies beside this explicitly selected test DLL without changing process search state.
  wide,err:=syscall.UTF16PtrFromString(path);if err!=nil{return nil,err}
  const loadLibrarySearchDllLoadDir=0x00000100
  const loadLibrarySearchDefaultDirs=0x00001000
  load:=syscall.NewLazyDLL("kernel32.dll").NewProc("LoadLibraryExW")
  handle,_,failure:=load.Call(uintptr(unsafe.Pointer(wide)),0,loadLibrarySearchDllLoadDir|loadLibrarySearchDefaultDirs)
  runtime.KeepAlive(wide)
  if handle==0{return nil,fmt.Errorf("Load prepared provider DLL: %w",failure)}
  library:=&syscall.DLL{Name:path,Handle:syscall.Handle(handle)}
  result:=&providerBridge{library:library}
  for name,target:=range map[string]**syscall.Proc{
    "serializer_xml_provider_prepare":&result.prepare,"serializer_xml_provider_validate":&result.validate,
    "serializer_xml_provider_release":&result.release,"serializer_xml_provider_error":&result.diagnostic,
    "serializer_xml_provider_version":&result.version,
  } {
    entry,err:=library.FindProc(name);if err!=nil{_ = library.Release();return nil,err};*target=entry
  }
  return result,nil
}

// Copy bounded native text while its calling-thread lifetime remains controlled by the client operation.
func (b *providerBridge) message(entry *syscall.Proc)string {
  buffer:=make([]byte,512);count,_,_:=entry.Call(uintptr(unsafe.Pointer(unsafe.SliceData(buffer))),uintptr(len(buffer)))
  runtime.KeepAlive(buffer)
  if count>=uintptr(len(buffer)){count=uintptr(len(buffer)-1)}
  return string(buffer[:count])
}

// Release the loaded bridge only after every uniquely owned schema has been closed.
func (b *providerBridge) Close()error {return b.library.Release()}

// Share immutable provider state across independent validations while closing waits for all borrowers.
type preparedProviderState struct {mutex sync.RWMutex;handle uintptr;calls atomic.Uint64}
type preparedProvider struct {bridge *providerBridge;state *preparedProviderState;limits XmlLimits}

// Prepare once, preserving native thread-local diagnostics across the factory and its error copy.
func newPreparedProvider(bridge *providerBridge,source []byte)(*preparedProvider,error) {
  runtime.LockOSThread();defer runtime.UnlockOSThread()
  handle,_,_:=bridge.prepare.Call(uintptr(unsafe.Pointer(unsafe.SliceData(source))),uintptr(len(source)))
  runtime.KeepAlive(source)
  if handle==0{return nil,fmt.Errorf("Prepared XML schema: %s",bridge.message(bridge.diagnostic))}
  return &preparedProvider{bridge:bridge,state:&preparedProviderState{handle:handle},limits:DefaultXmlLimits()},nil
}

// Validate the complete message through a real prepared libxml2 schema and a fresh per-call context.
func (p *preparedProvider) ValidateXML(data []byte)error {
  p.state.mutex.RLock();defer p.state.mutex.RUnlock()
  if p.state.handle==0{return fmt.Errorf("Prepared XML schema is closed")}
  limits:=p.limits
  if limits.MaxBytes<0||limits.MaxDepth<0||limits.MaxNodes<0||limits.MaxAttributes<0||limits.MaxTextBytes<0{return fmt.Errorf("Negative prepared XML limit")}
  runtime.LockOSThread();defer runtime.UnlockOSThread()
  p.state.calls.Add(1)
  status,_,_:=p.bridge.validate.Call(p.state.handle,uintptr(unsafe.Pointer(unsafe.SliceData(data))),uintptr(len(data)),
    uintptr(limits.MaxBytes),uintptr(limits.MaxDepth),uintptr(limits.MaxNodes),uintptr(limits.MaxAttributes),uintptr(limits.MaxTextBytes))
  runtime.KeepAlive(data)
  if status!=0{return fmt.Errorf("Prepared XML validation status %d: %s",status,p.bridge.message(p.bridge.diagnostic))}
  return nil
}

// Close is idempotent and cannot race an in-flight callback borrowing the prepared handle.
func (p *preparedProvider) Close() {
  p.state.mutex.Lock();defer p.state.mutex.Unlock()
  if p.state.handle!=0{_,_,_=p.bridge.release.Call(p.state.handle);p.state.handle=0}
}

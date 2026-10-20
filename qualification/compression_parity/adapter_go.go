// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
package main
import("bytes";"encoding/json";"errors";"fmt";"os";"path/filepath";"strconv";"strings")
var formats=map[string]SerializerCompressionFormat{"NONE":SerializerCompressionNone,"GZIP":SerializerCompressionGzip,"ZLIB":SerializerCompressionZlib,"DEFLATE":SerializerCompressionDeflate,"ZSTD":SerializerCompressionZstd,"LZ4":SerializerCompressionLz4}
var protocols=map[string]Protocol{"JSON":JSON,"BINARY_NONE":BINARY_NONE,"BINARY_INTEGER":BINARY_INTEGER,"BINARY_STRING":BINARY_STRING}
// Read only the explicit shared corpus paths and preserve unexpected transport failures.
func read(root,name string)[]byte{value,err:=os.ReadFile(filepath.Join(root,name));if err!=nil{panic(err)};return value}
// Keep optional providers caller-selected and scoped to one operation.
func selectProvider(format SerializerCompressionFormat) SerializerCompressionProvider{if format==SerializerCompressionZstd{return ZstdProvider{}};if format==SerializerCompressionLz4{return Lz4Provider{}};return nil}
// Apply the identical compressed, expanded and conservative history policy in every backend.
func decoding(format SerializerCompressionFormat,input,output,window int64)SerializerCompressionDecodeOptions{options:=SerializerCompressionDefaultDecodeOptions(format);options.MaxInputBytes=input;options.MaxOutputBytes=output;options.MaxWindowBytes=window;options.Provider=selectProvider(format);return options}
// Fail qualification independently of assertion settings.
func require(value bool,message string){if !value{panic(message)}}
// Parse exact nonnegative manifest bounds rather than accepting coercions.
func number(value string)int64{result,err:=strconv.ParseInt(value,10,64);if err!=nil{panic(err)};return result}
// Execute every required shared decision and owning native-model exchange.
func main(){root:=os.Args[2];if os.Args[1]=="produce"{
 value:=NewAccountRecord();value.AccountId=42;value.Label="shared \u03bb \U0001f642"
 for name,protocol:=range protocols{for kind,format:=range formats{
  options:=SerializerCompressionDefaultEncodeOptions(format);options.Provider=selectProvider(format);options.Level=3;if format==SerializerCompressionNone||format==SerializerCompressionLz4{options.Level=0}
  frame,err:=SerializerCompressionEncode(value,protocol,options);if err!=nil{panic(err)};plain,err:=value.Encode(protocol);if err!=nil{panic(err)};stem:="go-"+name+"-"+kind
  exact:=options;exact.MaxInputBytes=int64(len(plain));exact.MaxOutputBytes=int64(len(frame));same,err:=SerializerCompressionEncode(value,protocol,exact);if err!=nil{panic(err)};require(bytes.Equal(same,frame),"Exact encode admission")
  for _,limits:=range [][2]int64{{int64(len(plain)-1),int64(len(frame))},{int64(len(plain)),int64(len(frame)-1)}}{bounded:=options;bounded.MaxInputBytes=limits[0];bounded.MaxOutputBytes=limits[1];data,err:=SerializerCompressionEncode(value,protocol,bounded);var failure *SerializerCompressionError;require(data==nil&&errors.As(err,&failure)&&failure.Code==SerializerCompressionResourceLimit,"One-tight encode admission")}
  if err=os.WriteFile(filepath.Join(root,stem+".frame"),frame,0600);err!=nil{panic(err)};if err=os.WriteFile(filepath.Join(root,stem+".plain"),plain,0600);err!=nil{panic(err)}
 }};fmt.Println(`{"produced":24,"encode_bounds":72}`);return}
 decisions,exchanges:=0,0
 for _,line:=range strings.Split(strings.TrimSpace(string(read(root,"cases.tsv"))),"\n"){
  row:=strings.Split(strings.TrimSpace(line),"\t");actual,err:=SerializerCompressionDecompress(read(root,row[2]),decoding(formats[row[1]],number(row[5]),number(row[6]),number(row[7])));observed:="ok"
  if err!=nil{var failure *SerializerCompressionError;if !errors.As(err,&failure){panic(err)};observed=string(failure.Code)}else if row[4]=="ok"{require(bytes.Equal(actual,read(root,row[3])),row[0])}
  require(observed==row[4],row[0]+": "+observed+" != "+row[4]);decisions++
 }
 for _,line:=range strings.Split(strings.TrimSpace(string(read(root,"exchange.tsv"))),"\n"){
  row:=strings.Split(strings.TrimSpace(line),"\t");policy:=decoding(formats[row[2]],8388608,8388608,8388608);frame:=read(root,row[3]);value,err:=SerializerCompressionDecode(frame,protocols[row[1]],DecodeAccountRecord,policy);if err!=nil{panic(err)}
  require(value.AccountId==42&&value.Label=="shared \u03bb \U0001f642",row[0]);plain,err:=SerializerCompressionDecompress(frame,policy);if err!=nil{panic(err)};require(bytes.Equal(plain,read(root,row[4])),row[0]);exchanges++
 };result,err:=json.Marshal(map[string]int{"decisions":decisions,"exchanges":exchanges});if err!=nil{panic(err)};fmt.Println(string(result))
}

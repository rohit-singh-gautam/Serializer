"""Qualify native bodies, abstract adapters, external methods and edit receivers in real SDKs."""
import argparse
import importlib.util
import os
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("behavior", ROOT / "test/behavior_languages.py")
behavior = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(behavior)
examples = behavior.examples


def consumer(language):
    """Keep application-owned method implementations outside regenerated files."""
    sources = {
        "cpp": r'''#include "message.hpp"
#include <type_traits>
// External definitions survive regeneration; the data class is still constructible.
double DocumentData::score() const {return measure+5;}
class provider final : public DocumentData_behavior {
public:
  // Dispatch operates on the supplied data, not an implicit serialized implementation object.
  double adjustment(const DocumentData& receiver,double extra) const override {return receiver.measure+extra+10;}
  double weight(const DocumentData& receiver) const override {return receiver.measure*3;}
};
int main(){
  static_assert(!std::is_abstract_v<DocumentBase>);
  static_assert(std::is_same_v<DocumentData::number_type,double>);
  DocumentData::serializer_require_behavior_definitions();
  DocumentData value{};value.measure=3;
  if(value.adjustment(2)!=5 || value.weight()!=6 || value.score()!=8)return 1;
  value.update(4);provider implementation;DocumentData_behavior_adapter bound(value,implementation);
  if(bound.adjustment(2)!=16 || bound.weight()!=12 || value.score()!=9)return 2;
  return 0;
}
''',
        "java": r'''/** Application-owned implementation of the generated checked dispatch interface. */
public final class Main {
  private static final class Provider implements Schema.DocumentData.Behavior {
    public double adjustment(Schema.DocumentData receiver,double extra){return receiver.measure+extra+10;}
    public double weight(Schema.DocumentData receiver){return receiver.measure*3;}
    public double score(Schema.DocumentData receiver){return receiver.measure+5;}
  }
  /** The owning model remains constructible without an abstract implementation. */
  public static void main(String[] args){
    var base=new Schema.DocumentBase();base.base_measure=2;
    var value=new Schema.DocumentData();value.measure=3;
    if(value.adjustment(2)!=5 || value.weight()!=6)throw new AssertionError();
    boolean missing=false;try{value.score();}catch(IllegalStateException exception){missing=true;}
    if(!missing)throw new AssertionError();
    value.update(4);var provider=new Provider();value.attachBehavior(provider);
    var bound=new Schema.DocumentData.BehaviorAdapter(value,provider);
    if(value.adjustment(2)!=16 || bound.adjustment(2)!=16 || bound.weight()!=12 || value.score()!=9)throw new AssertionError();
  }
}
''',
        "javascript": r'''import {DocumentBase,DocumentData,DocumentData_behavior_adapter} from './schema.mjs';
const base=new DocumentBase();base.base_measure=2;
const value=new DocumentData();value.measure=3;
if(value.adjustment(2)!==5 || value.weight()!==6)throw Error('Native behavior mismatch');
let missing=false;try{value.score();}catch(error){missing=error instanceof Error;}if(!missing)throw Error('Missing attachment accepted');
class Provider {
  adjustment(receiver,extra){return receiver.measure+extra+10;}
  weight(receiver){return receiver.measure*3;}
  score(receiver){return receiver.measure+5;}
}
value.update(4);const provider=new Provider();value.attachBehavior(provider);
const bound=new DocumentData_behavior_adapter(value,provider);
if(value.adjustment(2)!==16 || bound.adjustment(2)!==16 || bound.weight()!==12 || value.score()!==9)throw Error('Adapter mismatch');
''',
        "go": r'''package main
// Same-package external receiver methods are compile-time required by generated checks.
func(value *DocumentData) score()float64{return value.measure+5}
type provider struct{}
func(provider) adjustment(receiver *DocumentData,extra float64)float64{return receiver.measure+extra+10}
func(provider) weight(receiver *DocumentData)float64{return receiver.measure*3}
func main(){
  base:=NewDocumentBase();base.base_measure=2
  value:=NewDocumentData();value.measure=3
  if value.adjustment(2)!=5 || value.weight()!=6 || value.score()!=8{panic("Native behavior mismatch")}
  value.update(4);bound:=DocumentData_behavior_adapter{Receiver:value,Implementation:provider{}}
  if bound.adjustment(2)!=16 || bound.weight()!=12 || value.score()!=9{panic("Adapter mismatch")}
}
''',
        "csharp": r'''using System;
using static SerializerGenerated.Schema;
namespace SerializerGenerated {
  public static partial class Schema {
    public sealed partial class DocumentData {
      // A required partial definition remains in this application-owned file.
      public partial double score()=>measure+5;
    }
  }
}
internal sealed class Provider : DocumentData.Behavior {
  public double adjustment(DocumentData receiver,double extra)=>receiver.measure+extra+10;
  public double weight(DocumentData receiver)=>receiver.measure*3;
}
internal static class Program {
  private static void Main(){
    var basis=new DocumentBase{base_measure=2};var value=new DocumentData{measure=3};
    if(value.adjustment(2)!=5 || value.weight()!=6 || value.score()!=8)throw new Exception("Native mismatch");
    value.update(4);var provider=new Provider();value.AttachBehavior(provider);
    var bound=new DocumentData.BehaviorAdapter(value,provider);
    if(value.adjustment(2)!=16 || bound.adjustment(2)!=16 || bound.weight()!=12 || value.score()!=9)throw new Exception("Adapter mismatch");
    GC.KeepAlive(basis);
  }
}
''',
        "rust": r'''mod schema;
use schema::*;
impl DocumentData {pub fn score(&self)->f64{self.measure+5.0}}
struct Provider;
impl DocumentDataBehavior for Provider {
  fn adjustment(&self,receiver:&DocumentData,extra:f64)->f64{receiver.measure+extra+10.0}
  fn weight(&self,receiver:&DocumentData)->f64{receiver.measure*3.0}
}
fn main(){
  let mut base=DocumentBase::default();base.base_measure=2.0;assert_eq!(base.base_measure,2.0);
  let mut value=DocumentData::default();value.measure=3.0;
  assert_eq!(value.adjustment(2.0),5.0);assert_eq!(value.weight(),6.0);assert_eq!(value.score(),8.0);
  value.update(4.0);let bound=DocumentDataBehaviorAdapter{receiver:&value,implementation:&Provider};
  assert_eq!(bound.adjustment(2.0),16.0);assert_eq!(bound.weight(),12.0);assert_eq!(value.score(),9.0);
}
''',
        "python": r'''from schema import DocumentBase,DocumentData,DocumentDataBehavior,DocumentDataBehaviorAdapter
base=DocumentBase();base.base_measure=2
value=DocumentData();value.measure=3
assert value.adjustment(2)==5 and value.weight()==6
try:value.score()
except RuntimeError:pass
else:raise AssertionError('Missing attachment accepted')
class Incomplete(DocumentDataBehavior):pass
try:Incomplete()
except TypeError:pass
else:raise AssertionError('Incomplete ABC accepted')
class Provider(DocumentDataBehavior):
    def adjustment(self,receiver,extra):return receiver.measure+extra+10
    def weight(self,receiver):return receiver.measure*3
provider=Provider();value.update(4)
value.attach_behavior({'adjustment':provider.adjustment,'weight':provider.weight,'score':lambda receiver:receiver.measure+5})
bound=DocumentDataBehaviorAdapter(value,provider)
assert value.adjustment(2)==16 and bound.adjustment(2)==16 and bound.weight()==12 and value.score()==9
''',
        "swift": r'''import Foundation
extension DocumentData {public func score()->Double{return measure+5}}
struct Provider:DocumentDataBehavior {
  func adjustment(_ receiver:DocumentData,_ extra:Double)->Double{return receiver.measure+extra+10}
  func weight(_ receiver:DocumentData)->Double{return receiver.measure*3}
}
var base=DocumentBase();base.baseMeasure=2
var value=DocumentData();value.measure=3
let weight=try value.weight();precondition(value.adjustment(2)==5 && weight==6 && value.score()==8)
value.update(4);let bound=DocumentDataBehaviorAdapter(value,Provider())
precondition(bound.adjustment(2)==16 && bound.weight()==12 && value.score()==9)
''',
        "kotlin": r'''class Provider:DocumentData.Behavior {
  override fun adjustment(receiver:DocumentData,extra:Double):Double=receiver.measure+extra+10
  override fun weight(receiver:DocumentData):Double=receiver.measure*3
  override fun score(receiver:DocumentData):Double=receiver.measure+5
}
fun main(){
  val base=DocumentBase();base.baseMeasure=2.0
  val value=DocumentData();value.measure=3.0
  check(value.adjustment(2.0)==5.0 && value.weight()==6.0)
  check(runCatching{value.score()}.isFailure)
  value.update(4.0);val provider=Provider();value.attachBehavior(provider)
  val bound=DocumentData.BehaviorAdapter(value,provider)
  check(value.adjustment(2.0)==16.0 && bound.adjustment(2.0)==16.0 && bound.weight()==12.0 && value.score()==9.0)
}
''',
        "c": r'''#include "schema.h"
/* External ownership remains explicit; generated codecs never call this implementation. */
double document_data_score(const document_data* receiver){return receiver->measure+5;}
static double adjustment(const document_data* receiver,double extra){return receiver->measure+extra+10;}
static double weight(const document_data* receiver){return receiver->measure*3;}
int main(void){
  document_base base={0};if(document_base_init(&base)!=srl_ok)return 1;
  document_data_require_behavior_definitions();
  document_data value={0};if(document_data_init(&value)!=srl_ok)return 2;value.measure=3;
  if(document_data_adjustment(&value,2)!=5 || document_data_weight(&value)!=6 || document_data_score(&value)!=8)return 3;
  document_data_update(&value,4);document_data_behavior table={adjustment,weight};document_data_behavior_adapter bound={0};
  if(document_data_behavior_adapter_init(&bound,&value,&table)!=srl_ok)return 4;
  double adjusted=0,weighted=0;
  if(document_data_behavior_adapter_adjustment_checked(&bound,2,&adjusted)!=srl_ok || adjusted!=16)return 5;
  if(document_data_behavior_adapter_weight_checked(&bound,&weighted)!=srl_ok || weighted!=12)return 6;
  table.weight=NULL;if(document_data_behavior_adapter_init(&bound,&value,&table)==srl_ok)return 7;
  document_data_free(&value);document_base_free(&base);return 0;
}
''',
    }
    if language == "typescript":
        return sources["javascript"].replace("adjustment(receiver,extra)","adjustment(receiver: DocumentData,extra: number)").replace("weight(receiver)","weight(receiver: DocumentData)").replace("score(receiver)","score(receiver: DocumentData)")
    if language == "go":
        value=sources[language]
        for old,new in (("base_measure","BaseMeasure"),("measure","Measure"),("adjustment","Adjustment"),("weight","Weight"),("update","Update"),("score","Score")):
            value=value.replace(old,new)
        return value
    return sources[language]


def qualify_unused_cpp_definition(runner, args):
    """Linking qualification must reject an absent method even though no method invocation exists."""
    directory=args.build/"unused-definition-negative";directory.mkdir(parents=True,exist_ok=True)
    (directory/"message.hpp").write_bytes((args.build/"cpp/message.hpp").read_bytes())
    (directory/"main.cpp").write_text('#include "message.hpp"\nint main(){DocumentData::serializer_require_behavior_definitions();}\n')
    source=runner.path("cpp",directory/"main.cpp");destination=runner.path("cpp",directory/"main.exe")
    library=args.cpp_library or args.compiler.parent/("serializer_runtime.lib" if os.name=="nt" and "cpp" not in runner.wsl else "libserializer_runtime.a")
    if os.name=="nt" and "cpp" not in runner.wsl:
        command=runner.command("cpp","cl","/nologo","/MD","/O2","/W4","/WX","/std:c++20","/EHsc","/utf-8",
          "/I"+runner.path("cpp",ROOT/"include"),source,str(library),"/Fe:"+destination)
    else:
        command=runner.command("cpp","c++","-std=c++20","-O2","-Wall","-Wextra","-Werror","-I",
          runner.path("cpp",ROOT/"include"),source,runner.path("cpp",library),"-o",destination)
    result=subprocess.run([str(value) for value in command],cwd=directory,capture_output=True,text=True,
      encoding="utf-8",errors="replace",timeout=120,creationflags=examples.PROCESS_CREATION_FLAGS)
    diagnostics=result.stdout+result.stderr
    if result.returncode==0 or "score" not in diagnostics or not any(word in diagnostics.lower() for word in ("unresolved","undefined")):
        raise AssertionError("Unused required external definition was not rejected by the linker: "+diagnostics)
    print("PASS: cpp, unused missing external definition rejected by generated qualification hook",flush=True)


def qualify_missing_definition(language, runner, args):
    """A real SDK must reject a removed external definition or incomplete typed attachment."""
    if language not in ("java","go","csharp","rust","swift","kotlin","c"):return
    tokens={"java":"public double score(","go":"func(value *DocumentData) Score(",
      "csharp":"public partial double score(","rust":"impl DocumentData {",
      "swift":"extension DocumentData {","kotlin":"override fun score(",
      "c":"double document_data_score("}
    original=consumer(language)
    lines=original.splitlines(keepends=True)
    removed=[line for line in lines if tokens[language] in line]
    if len(removed)!=1:raise AssertionError("Negative fixture must remove exactly one definition: "+language)
    invalid="".join(line for line in lines if tokens[language] not in line)
    directory=args.build/"missing-definition-negative"/language;directory.mkdir(parents=True,exist_ok=True)
    (directory/examples.OUTPUTS[language]).write_bytes((args.build/language/examples.OUTPUTS[language]).read_bytes())
    source=args.build/"consumers-negative"/language;source.mkdir(parents=True,exist_ok=True)
    (source/behavior.FILES[language]).write_text(invalid,encoding="utf-8")
    original_run=examples.run
    def capture_sdk(command, *, cwd=None, capture=False):
        """Retain SDK diagnostics without accepting an unrelated process failure."""
        return subprocess.run([str(value) for value in command],cwd=cwd,check=True,capture_output=True,
          text=True,encoding="utf-8",errors="replace",timeout=180,creationflags=examples.PROCESS_CREATION_FLAGS)
    examples.run=capture_sdk
    diagnostics=""
    try:
        runner.build(language,"missing external definition",directory,source)
    except subprocess.CalledProcessError as error:
        diagnostics=(error.stdout or "")+(error.stderr or "")
    finally:
        examples.run=original_run
    if not diagnostics or "score" not in diagnostics.lower():
        raise AssertionError("SDK did not diagnose the removed required definition: "+language+"\n"+diagnostics)
    print(f"PASS: {language}, missing external definition/incomplete typed implementation rejected",flush=True)


def main():
    """Generate native bodies with their documented preserved/fixed naming and run all contracts."""
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler",type=Path,required=True)
    parser.add_argument("--cpp-library",type=Path)
    parser.add_argument("--build",type=Path,default=ROOT/"out/native-behavior-languages")
    parser.add_argument("--language",default="all")
    parser.add_argument("--wsl-languages",default="")
    args=parser.parse_args();args.sanitize=False;args.compiler=args.compiler.resolve();args.build=args.build.resolve()
    if args.cpp_library:args.cpp_library=args.cpp_library.resolve()
    languages=list(examples.LANGUAGES if args.language=="all" else args.language.split(","))
    targets={("js" if language=="javascript" else language):args.build/language/examples.OUTPUTS[language] for language in languages if language!="go"}
    if "typescript" in languages and "javascript" not in languages:targets["js"]=args.build/"typescript/schema.mjs"
    command=[args.compiler,"--input",ROOT/"test/resources/document_native_behavior.serializer","--language",",".join(targets),
             "--cpp.format=false","--cpp.naming=preserve","--java.naming=preserve","--js.naming=preserve",
             "--go.naming=preserve","--csharp.naming=preserve","--go.package=main","--kotlin.package="]
    for language,destination in targets.items():
        destination.parent.mkdir(parents=True,exist_ok=True);command += [f"--{language}.output",destination]
    if targets:examples.run(command)
    if "go" in languages:
        destination=args.build/"go/schema.go";destination.parent.mkdir(parents=True,exist_ok=True)
        examples.run([args.compiler,"--input",ROOT/"test/resources/document_native_behavior_go.serializer","--language","go",
                      "--go.naming=preserve","--go.package=main","--go.output",destination])
    if "typescript" in languages and "javascript" in languages:(args.build/"typescript/schema.mjs").write_bytes(targets["js"].read_bytes())
    runner=examples.Runner(args)
    for language in languages:
        source=args.build/"consumers"/language;source.mkdir(parents=True,exist_ok=True)
        (source/behavior.FILES[language]).write_text(consumer(language),encoding="utf-8")
        executable=runner.build(language,"native behavior",args.build/language,source)
        examples.run(executable)
        print(f"PASS: {language}, native bodies/hooks, abstract/virtual/override adapters, external definition, edit",flush=True)
        qualify_missing_definition(language,runner,args)

    if "cpp" in languages:qualify_unused_cpp_definition(runner,args)


if __name__=="__main__":main()

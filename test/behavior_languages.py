"""Compile and run portable behavior expressions against all eleven real language SDKs."""
import argparse
import importlib.util
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("examples", ROOT / "example/run.py")
examples = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(examples)

FILES = {"cpp": "main.cpp", "java": "Main.java", "javascript": "main.mjs", "typescript": "main.mts",
         "go": "main.go", "csharp": "Program.cs", "rust": "main.rs", "python": "main.py",
         "swift": "main.swift", "kotlin": "Main.kt", "c": "main.c"}


def checked_consumer(language, source):
    """Exercise checked division/math and lazy boolean guards using each SDK's failure API."""
    checks = {
        "cpp": ("  if(value.page_area()", r'''  if(value.ratio(2)!=2 || value.guarded_root(-1)!=0 || value.guarded_root(4)!=2 || value.wave(0)!=1 || value.valid_ratio(0) || !value.valid_ratio(2) || !value.guarded_choice(false,-1) || value.guarded_choice(true,-1) || !value.guarded_choice(true,4)) return 7;
  for(int index=0;index<4;++index){bool rejected=false;try{if(index==0)(void)value.ratio(0);else if(index==1)(void)value.ratio(-0.0);else if(index==2)(void)value.square_root(-1);else(void)value.overflow_guard(std::numeric_limits<double>::max());}catch(const std::domain_error&){rejected=true;}if(!rejected)return 8;}
'''),
        "java": ("    if(value.pageArea()", r'''    if(value.ratio(2)!=2 || value.guardedRoot(-1)!=0 || value.guardedRoot(4)!=2 || value.wave(0)!=1 || value.validRatio(0) || !value.validRatio(2) || !value.guardedChoice(false,-1) || value.guardedChoice(true,-1) || !value.guardedChoice(true,4))throw new AssertionError("Checked math");
    for(int index=0;index<4;++index){boolean rejected=false;try{if(index==0)value.ratio(0);else if(index==1)value.ratio(-0.0);else if(index==2)value.squareRoot(-1);else value.overflowGuard(Double.MAX_VALUE);}catch(ArithmeticException exception){rejected=true;}if(!rejected)throw new AssertionError("Domain accepted");}
'''),
        "javascript": ("if(value.pageArea()", r'''if(value.ratio(2)!==2 || value.guardedRoot(-1)!==0 || value.guardedRoot(4)!==2 || value.wave(0)!==1 || value.validRatio(0) || !value.validRatio(2) || !value.guardedChoice(false,-1) || value.guardedChoice(true,-1) || !value.guardedChoice(true,4))throw Error('Checked math');
for(const operation of [()=>value.ratio(0),()=>value.ratio(-0),()=>value.squareRoot(-1),()=>value.overflowGuard(Number.MAX_VALUE)]){let rejected=false;try{operation()}catch(error){rejected=error instanceof RangeError}if(!rejected)throw Error('Domain accepted');}
'''),
        "go": ("  if value.PageArea()", r'''  if value.Ratio(2)!=2 || value.GuardedRoot(-1)!=0 || value.GuardedRoot(4)!=2 || value.Wave(0)!=1 || value.ValidRatio(0) || !value.ValidRatio(2) || !value.GuardedChoice(false,-1) || value.GuardedChoice(true,-1) || !value.GuardedChoice(true,4){panic("Checked math")}
  for _,operation:=range []func(){func(){value.Ratio(0)},func(){value.Ratio(math.Copysign(0,-1))},func(){value.SquareRoot(-1)},func(){value.OverflowGuard(math.MaxFloat64)}}{rejected:=func()(failed bool){defer func(){if recover()!=nil{failed=true}}();operation();return}();if !rejected{panic("Domain accepted")}}
'''),
        "csharp": ("    if(value.PageArea()", r'''    if(value.Ratio(2)!=2 || value.GuardedRoot(-1)!=0 || value.GuardedRoot(4)!=2 || value.Wave(0)!=1 || value.ValidRatio(0) || !value.ValidRatio(2) || !value.GuardedChoice(false,-1) || value.GuardedChoice(true,-1) || !value.GuardedChoice(true,4))throw new Exception("Checked math");
    foreach(Action operation in new Action[]{()=>value.Ratio(0),()=>value.Ratio(-0.0),()=>value.SquareRoot(-1),()=>value.OverflowGuard(double.MaxValue)}){bool rejected=false;try{operation();}catch(ArithmeticException){rejected=true;}if(!rejected)throw new Exception("Domain accepted");}
'''),
        "rust": ("  assert_eq!(value.page_area()", r'''  assert_eq!(value.ratio(2.0),2.0);assert_eq!(value.guarded_root(-1.0),0.0);assert_eq!(value.guarded_root(4.0),2.0);assert_eq!(value.wave(0.0),1.0);assert!(!value.valid_ratio(0.0));assert!(value.valid_ratio(2.0));assert!(value.guarded_choice(false,-1.0));assert!(!value.guarded_choice(true,-1.0));assert!(value.guarded_choice(true,4.0));
  for index in 0..4{assert!(std::panic::catch_unwind(||{match index{0=>{value.ratio(0.0);},1=>{value.ratio(-0.0);},2=>{value.square_root(-1.0);},_=>{value.overflow_guard(f64::MAX);}}}).is_err());}
'''),
        "python": ("assert value.page_area()", r'''assert value.ratio(2)==2 and value.guarded_root(-1)==0 and value.guarded_root(4)==2 and value.wave(0)==1 and not value.valid_ratio(0) and value.valid_ratio(2) and value.guarded_choice(False,-1) and not value.guarded_choice(True,-1) and value.guarded_choice(True,4)
for operation in (lambda:value.ratio(0),lambda:value.ratio(-0.0),lambda:value.square_root(-1),lambda:value.overflow_guard(1.7976931348623157e308)):
    try:operation()
    except ValueError:pass
    else:raise AssertionError('Domain accepted')
'''),
        "swift": ("let page=try value.pageArea()", r'''let ratio=try value.ratio(2);let emptyRoot=try value.guardedRoot(-1);let root=try value.guardedRoot(4);let wave=try value.wave(0);let invalid=try value.validRatio(0);let valid=try value.validRatio(2);let disabled=try value.guardedChoice(false,-1);let negative=try value.guardedChoice(true,-1);let positive=try value.guardedChoice(true,4)
precondition(ratio==2 && emptyRoot==0 && root==2 && wave==1 && !invalid && valid && disabled && !negative && positive)
for index in 0..<4{var rejected=false;do{switch index{case 0:_=try value.ratio(0);case 1:_=try value.ratio(-0.0);case 2:_=try value.squareRoot(-1);default:_=try value.overflowGuard(Double.greatestFiniteMagnitude)}}catch{rejected=true};precondition(rejected)}
'''),
        "kotlin": ("  check(value.pageArea()", r'''  check(value.ratio(2.0)==2.0 && value.guardedRoot(-1.0)==0.0 && value.guardedRoot(4.0)==2.0 && value.wave(0.0)==1.0 && !value.validRatio(0.0) && value.validRatio(2.0) && value.guardedChoice(false,-1.0) && !value.guardedChoice(true,-1.0) && value.guardedChoice(true,4.0))
  for(index in 0..3){check(runCatching{when(index){0->value.ratio(0.0);1->value.ratio(-0.0);2->value.squareRoot(-1.0);else->value.overflowGuard(Double.MAX_VALUE)}}.isFailure)}
'''),
        "c": ("  if(document_metrics_page_area(", r'''  if(document_metrics_ratio(&value,2)!=2 || document_metrics_guarded_root(&value,-1)!=0 || document_metrics_guarded_root(&value,4)!=2 || document_metrics_wave(&value,0)!=1 || document_metrics_valid_ratio(&value,0) || !document_metrics_valid_ratio(&value,2) || !document_metrics_guarded_choice(&value,false,-1) || document_metrics_guarded_choice(&value,true,-1) || !document_metrics_guarded_choice(&value,true,4))return 7;
  double unchanged=17;bool boolean_unchanged=true;
  if(document_metrics_ratio_checked(&value,0,&unchanged)!=srl_range || unchanged!=17 || document_metrics_ratio_checked(&value,-0.0,&unchanged)!=srl_range || document_metrics_square_root_checked(&value,-1,&unchanged)!=srl_range || unchanged!=17 || document_metrics_overflow_guard_checked(&value,DBL_MAX,&boolean_unchanged)!=srl_range || !boolean_unchanged)return 8;
  errno=0;if(document_metrics_overflow_guard(&value,DBL_MAX) || errno!=ERANGE)return 9;
'''),
    }
    anchor, extra = checks[language]
    helper_checks = {
        "cpp": "  if(value.helper_parameter(3,4,2)!=8)return 10;\n",
        "java": '    if(value.helperParameter(3,4,2)!=8)throw new AssertionError("Helper lookup");\n',
        "javascript": "if(value.helperParameter(3,4,2)!==8)throw Error('Helper lookup');\n",
        "go": '  if value.HelperParameter(3,4,2)!=8{panic("Helper lookup")}\n',
        "csharp": '    if(value.HelperParameter(3,4,2)!=8)throw new Exception("Helper lookup");\n',
        "rust": "  assert_eq!(value.helper_parameter(3.0,4.0,2.0),8.0);\n",
        "python": "assert value.helper_parameter(3,4,2)==8\n",
        "swift": "let helper=try value.helperParameter(3,4,2);precondition(helper==8)\n",
        "kotlin": "  check(value.helperParameter(3.0,4.0,2.0)==8.0)\n",
        "c": "  if(document_metrics_helper_parameter(&value,3,4,2)!=8)return 10;\n",
    }
    assert anchor in source, language
    return source.replace(anchor, helper_checks[language] + extra + anchor, 1)


def preserve_names(source, language):
    """Qualify the same checked behavior contract with Java/C# schema spelling preserved."""
    schema_names = ("document_metrics", "page_width", "page_height", "page_count", "annotation_count",
                    "page_area", "total_area", "annotation_weight", "guarded_root", "square_root",
                    "valid_ratio", "guarded_choice", "overflow_guard", "helper_parameter")
    for name in schema_names:
        words = name.split("_")
        if name == "document_metrics" or language == "csharp":
            emitted = "".join(word.capitalize() for word in words)
        else:
            emitted = words[0] + "".join(word.capitalize() for word in words[1:])
        source = source.replace(emitted, name)
    if language == "csharp":
        source = source.replace(".Ratio(", ".ratio(").replace(".Wave(", ".wave(")
    return source


def consumer(language):
    """Check equal values, exact int32/uint32 conversion and failure before publishing a result."""
    sources = {
        "cpp": r'''#include "message.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>
// Qualify portable methods independently of the generated codec implementation.
int main() {
  document_metrics value{}; value.page_width=4; value.page_height=3; value.page_count=2; value.annotation_count=4;
  if(value.page_area()!=12 || value.total_area(1.5)!=36 || value.annotation_weight()!=2) return 1;
  value.page_count=std::numeric_limits<std::int32_t>::min();
  if(value.total_area(1)!=12.0*std::numeric_limits<std::int32_t>::min()) return 2;
  value.annotation_count=std::numeric_limits<std::uint32_t>::max();
  if(value.annotation_weight()!=2147483647.5) return 3;
  for(double bad : {std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity(),1.7976931348623157e308}) {
    value.page_width=bad; bool rejected=false;
    try { (void)value.page_area(); } catch(const std::domain_error&) {rejected=true;}
    if(!rejected) return 4;
  }
  return 0;
}
''',
        "java": r'''/** Qualify generated methods with Java's actual type and exception contracts. */
public final class Main {
  /** Arithmetic failure must be observable while integral conversions remain exact. */
  public static void main(String[] args) {
    var value=new Schema.DocumentMetrics(); value.pageWidth=4; value.pageHeight=3; value.pageCount=2; value.annotationCount=4;
    if(value.pageArea()!=12 || value.totalArea(1.5)!=36 || value.annotationWeight()!=2)throw new AssertionError();
    value.pageCount=Integer.MIN_VALUE;
    if(value.totalArea(1)!=12.0*Integer.MIN_VALUE)throw new AssertionError();
    value.annotationCount=-1;
    if(value.annotationWeight()!=2147483647.5)throw new AssertionError();
    for(double bad:new double[]{Double.NaN,Double.POSITIVE_INFINITY,Double.MAX_VALUE}) {
      value.pageWidth=bad; boolean rejected=false;
      try{value.pageArea();}catch(ArithmeticException exception){rejected=true;}
      if(!rejected)throw new AssertionError();
    }
  }
}
''',
        "javascript": r'''import {DocumentMetrics} from './schema.mjs';
const value=new DocumentMetrics(); value.pageWidth=4; value.pageHeight=3; value.pageCount=2; value.annotationCount=4;
if(value.pageArea()!==12 || value.totalArea(1.5)!==36 || value.annotationWeight()!==2)throw Error('Value mismatch');
value.pageCount=-2147483648;
if(value.totalArea(1)!==12*(-2147483648))throw Error('Signed conversion');
value.annotationCount=4294967295;
if(value.annotationWeight()!==2147483647.5)throw Error('Unsigned conversion');
for(const invalid of [true,1.5,-2147483649,2147483648]) {
  value.pageCount=invalid;let rejected=false;try{value.totalArea(1)}catch(error){rejected=error instanceof RangeError}if(!rejected)throw new Error('Invalid int32 accepted');
}
for(const invalid of [-1,1.5,4294967296,false]) {
  value.annotationCount=invalid;let rejected=false;try{value.annotationWeight()}catch(error){rejected=error instanceof RangeError}if(!rejected)throw new Error('Invalid uint32 accepted');
}
for(const bad of [NaN,Infinity,Number.MAX_VALUE]) {
  value.pageWidth=bad; let rejected=false;
  try{value.pageArea();}catch(error){rejected=error instanceof RangeError;}
  if(!rejected)throw Error('Non-finite arithmetic accepted');
}
''',
        "go": r'''package main
import "math"
// Confirm failure is a language-native panic and not a non-finite returned value.
func rejects(value *DocumentMetrics) (result bool) {defer func(){if recover()!=nil{result=true}}();value.PageArea();return}
// Run the same independent arithmetic cases used by every SDK consumer.
func main(){
  value:=NewDocumentMetrics();value.PageWidth=4;value.PageHeight=3;value.PageCount=2;value.AnnotationCount=4
  if value.PageArea()!=12 || value.TotalArea(1.5)!=36 || value.AnnotationWeight()!=2{panic("Value mismatch")}
  value.PageCount=math.MinInt32;if value.TotalArea(1)!=12.0*math.MinInt32{panic("Signed conversion")}
  value.AnnotationCount=math.MaxUint32;if value.AnnotationWeight()!=2147483647.5{panic("Unsigned conversion")}
  for _,bad:=range []float64{math.NaN(),math.Inf(1),math.MaxFloat64}{value.PageWidth=bad;if !rejects(value){panic("Non-finite accepted")}}
}
''',
        "csharp": r'''using System;
using static SerializerGenerated.Schema;
internal static class Program {
  // Exercise generated methods through C#'s actual owning class and exception model.
  private static void Main(){
    var value=new DocumentMetrics{PageWidth=4,PageHeight=3,PageCount=2,AnnotationCount=4};
    if(value.PageArea()!=12 || value.TotalArea(1.5)!=36 || value.AnnotationWeight()!=2)throw new Exception("Value mismatch");
    value.PageCount=int.MinValue;if(value.TotalArea(1)!=12.0*int.MinValue)throw new Exception("Signed conversion");
    value.AnnotationCount=uint.MaxValue;if(value.AnnotationWeight()!=2147483647.5)throw new Exception("Unsigned conversion");
    foreach(double bad in new[]{double.NaN,double.PositiveInfinity,double.MaxValue}){
      value.PageWidth=bad;bool rejected=false;
      try{value.PageArea();}catch(ArithmeticException){rejected=true;}
      if(!rejected)throw new Exception("Non-finite accepted");
    }
  }
}
''',
        "rust": r'''mod schema;
use schema::*;
// Validate portable method contracts without accepting arithmetic panics as successful values.
fn main(){
  let mut value=DocumentMetrics::default();value.page_width=4.0;value.page_height=3.0;value.page_count=2;value.annotation_count=4;
  assert_eq!(value.page_area(),12.0);assert_eq!(value.total_area(1.5),36.0);assert_eq!(value.annotation_weight(),2.0);
  value.page_count=i32::MIN;assert_eq!(value.total_area(1.0),12.0*(i32::MIN as f64));
  value.annotation_count=u32::MAX;assert_eq!(value.annotation_weight(),2147483647.5);
  for bad in [f64::NAN,f64::INFINITY,f64::MAX]{value.page_width=bad;assert!(std::panic::catch_unwind(|| value.page_area()).is_err());}
}
''',
        "python": r'''from schema import DocumentMetrics
value=DocumentMetrics();value.page_width=4;value.page_height=3;value.page_count=2;value.annotation_count=4
assert value.page_area()==12 and value.total_area(1.5)==36 and value.annotation_weight()==2
value.page_count=-2147483648;assert value.total_area(1)==12*(-2147483648)
value.annotation_count=4294967295;assert value.annotation_weight()==2147483647.5
# Double schema values must use binary64 arithmetic even if callers assign Python ints.
value.page_width=2**53+1;value.page_height=1
assert type(value.page_area()) is float and value.page_area()==float(2**53+1)
value.page_width=2**1023;value.page_height=2
try:value.page_area()
except ValueError:pass
else:raise AssertionError('Integer intermediate bypassed binary64 overflow')
value.page_width=4;value.page_height=3
for invalid in (True,1.5,-2147483649,2147483648):
    value.page_count=invalid
    try:value.total_area(1)
    except ValueError:pass
    else:raise AssertionError('Invalid int32 conversion accepted')
for invalid in (-1,1.5,4294967296,False):
    value.annotation_count=invalid
    try:value.annotation_weight()
    except ValueError:pass
    else:raise AssertionError('Invalid uint32 conversion accepted')
for bad in (float('nan'),float('inf'),1.7976931348623157e308):
    value.page_width=bad
    try: value.page_area()
    except ValueError: pass
    else: raise AssertionError('Non-finite accepted')
''',
        "swift": r'''import Foundation
var value=DocumentMetrics();value.pageWidth=4;value.pageHeight=3;value.pageCount=2;value.annotationCount=4
let page=try value.pageArea();let total=try value.totalArea(1.5);let annotations=try value.annotationWeight()
precondition(page==12 && total==36 && annotations==2)
value.pageCount=Int32.min;let signed=try value.totalArea(1);precondition(signed==12*Double(Int32.min))
value.annotationCount=UInt32.max;let unsigned=try value.annotationWeight();precondition(unsigned==2147483647.5)
for bad in [Double.nan,Double.infinity,Double.greatestFiniteMagnitude]{
  value.pageWidth=bad;var rejected=false
  do{_=try value.pageArea()}catch{rejected=true}
  precondition(rejected)
}
''',
        "kotlin": r'''// Run exact conversions and explicit arithmetic rejection against native Kotlin methods.
fun main(){
  val value=DocumentMetrics();value.pageWidth=4.0;value.pageHeight=3.0;value.pageCount=2;value.annotationCount=4u
  check(value.pageArea()==12.0 && value.totalArea(1.5)==36.0 && value.annotationWeight()==2.0)
  value.pageCount=Int.MIN_VALUE;check(value.totalArea(1.0)==12*Int.MIN_VALUE.toDouble())
  value.annotationCount=UInt.MAX_VALUE;check(value.annotationWeight()==2147483647.5)
  for(bad in listOf(Double.NaN,Double.POSITIVE_INFINITY,Double.MAX_VALUE)){
    value.pageWidth=bad;check(runCatching{value.pageArea()}.isFailure)
  }
}
''',
        "c": r'''#include "schema.h"
/* Checked functions leave the caller's output unchanged when finite arithmetic fails. */
int main(void){
  document_metrics value={0};if(document_metrics_init(&value)!=srl_ok)return 1;
  value.page_width=4;value.page_height=3;value.page_count=2;value.annotation_count=4;
  if(document_metrics_page_area(&value)!=12 || document_metrics_total_area(&value,1.5)!=36 || document_metrics_annotation_weight(&value)!=2)return 2;
  value.page_count=INT32_MIN;if(document_metrics_total_area(&value,1)!=12.0*INT32_MIN)return 3;
  value.annotation_count=UINT32_MAX;if(document_metrics_annotation_weight(&value)!=2147483647.5)return 4;
  const double bads[]={NAN,INFINITY,1.7976931348623157e308};
  for(size_t index=0;index<sizeof(bads)/sizeof(*bads);++index){
    value.page_width=bads[index];double result=17;
    if(document_metrics_page_area_checked(&value,&result)==srl_ok || result!=17)return 5;
    errno=0;if(!isnan(document_metrics_page_area(&value)) || errno==0)return 6;
  }
  document_metrics_free(&value);return 0;
}
''',
    }
    if language == "typescript":
        return checked_consumer("javascript", sources["javascript"]).replace("catch(error){", "catch(error: unknown){").replace("value.pageCount=invalid", "value.pageCount=invalid as number").replace("value.annotationCount=invalid", "value.annotationCount=invalid as number")
    return checked_consumer(language, sources[language])


def main():
    """Generate from the shared fixture, compile real consumers and require all cases to pass."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--cpp-library", type=Path)
    parser.add_argument("--build", type=Path, default=ROOT / "out/behavior-languages")
    parser.add_argument("--language", default="all")
    parser.add_argument("--wsl-languages", default="")
    parser.add_argument("--preserve-names", action="store_true")
    args = parser.parse_args()
    args.sanitize = False
    args.compiler = args.compiler.resolve()
    if args.cpp_library:
        args.cpp_library = args.cpp_library.resolve()
    args.build = args.build.resolve()
    languages = list(examples.LANGUAGES if args.language == "all" else args.language.split(","))
    if args.preserve_names and not set(languages).issubset({"java", "csharp"}):
        parser.error("--preserve-names qualifies Java and C# outputs only")
    targets = {("js" if language == "javascript" else language): args.build / language / examples.OUTPUTS[language] for language in languages}
    if "typescript" in languages and "javascript" not in languages:
        targets["js"] = args.build / "typescript/schema.mjs"
    command = [args.compiler, "--input", ROOT / "test/resources/document_behavior.serializer", "--language", ",".join(targets),
               "--cpp.format=false", "--go.package=main", "--kotlin.package="]
    if args.preserve_names:
        command += [f"--{language}.naming=preserve" for language in languages]
    for language, destination in targets.items():
        destination.parent.mkdir(parents=True, exist_ok=True)
        command += [f"--{language}.output", destination]
    examples.run(command)
    if "typescript" in languages and "javascript" in languages:
        (args.build / "typescript/schema.mjs").write_bytes(targets["js"].read_bytes())
    runner = examples.Runner(args)
    for language in languages:
        source = args.build / "consumers" / language
        source.mkdir(parents=True, exist_ok=True)
        authored = consumer(language)
        if args.preserve_names:
            authored = preserve_names(authored, language)
        (source / FILES[language]).write_text(authored, encoding="utf-8")
        executable = runner.build(language, "behavior", args.build / language, source)
        examples.run(executable)
        print(f"PASS: {language}, portable arithmetic/math domains, boolean short circuit, integral boundaries, overflow and helper lookup", flush=True)


if __name__ == "__main__":
    main()

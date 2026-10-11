"""Qualify exact numeric expression boundaries in every ordinary generated language."""
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


def consumer(language):
    """Use native failures while requiring matching full-width and fractional-result rejection."""
    if language in ("javascript", "typescript"):
        return '''import {NumericValuesScalar,NumericValuesFloatingQuantity} from './schema.mjs';
const value=new NumericValuesScalar();
const check=(ok: boolean)=>{if(!ok)throw Error('Checked numeric mismatch')};
check(value.add(9223372036854775807n,0n)===9223372036854775807n);
check(value.multiply(-9223372036854775808n,1n)===-9223372036854775808n);
check(value.unsignedAdd(18446744073709551615n,0n)===18446744073709551615n);
check(value.floatingUnsigned(9223372036854775808n)===9223372036854775808);
check(value.signedFloating(-9223372036854775808)===-9223372036854775808n);
check(value.unsignedFloating(18446744073709549568)===18446744073709549568n);
check(value.narrow(0.5)===0.5 && value.widen(0.5)===0.5);
const floating=new NumericValuesFloatingQuantity();floating.value=16777217;
check(floating.added(1)===16777216 && value.widen(16777217)===16777216);
check(value.floatEqual(16777217) && value.floatNonfinite(Infinity) && value.floatNonfinite(NaN));
check(!value.exactHalf(3n) && value.exactHalf(4n) && !value.exactUnsigned(-1) && !value.exactUnsigned(18446744073709551616));
for(const operation of [()=>value.add(9223372036854775807n,1n),()=>value.subtract(-9223372036854775808n,1n),()=>value.multiply(-9223372036854775808n,-1n),()=>value.divide(-9223372036854775808n,-1n),()=>value.divide(3n,2n),()=>value.divide(0n,0n),()=>value.negate(-9223372036854775808n),()=>value.unsignedAdd(18446744073709551615n,1n),()=>value.unsignedSubtract(0n,1n),()=>value.unsignedMultiply(18446744073709551615n,2n),()=>value.bindUnsigned(-1n),()=>value.bindSigned(18446744073709551615n),()=>value.floatingUnsigned(18446744073709551615n),()=>value.floatingSigned(9223372036854775807n),()=>value.unsignedFloating(18446744073709551616),()=>value.signedFloating(9223372036854775808),()=>value.signedFloating(1.5),()=>value.narrow(0.1),()=>value.narrow(Infinity),()=>value.inexactFloatLiteral(0),()=>value.inexactDoubleLiteral(0)]){let failed=false;try{operation()}catch{failed=true}check(failed)}
'''.replace("ok: boolean", "ok: boolean" if language == "typescript" else "ok")
    if language == "python":
        return '''from schema import NumericValuesScalar, NumericValuesFloatingQuantity
value=NumericValuesScalar()
minimum=-9223372036854775808;maximum=9223372036854775807;unsigned=18446744073709551615
assert value.add(maximum,0)==maximum and value.multiply(minimum,1)==minimum and value.unsigned_add(unsigned,0)==unsigned
assert value.floating_unsigned(9223372036854775808)==9223372036854775808.0
assert value.signed_floating(-9223372036854775808.0)==minimum and value.unsigned_floating(18446744073709549568.0)==18446744073709549568
assert value.narrow(0.5)==0.5 and value.widen(0.5)==0.5
floating=NumericValuesFloatingQuantity();floating.value=16777217
assert floating.added(1)==16777216 and value.widen(16777217)==16777216
assert value.float_equal(16777217) and value.float_nonfinite(float('inf')) and value.float_nonfinite(float('nan'))
assert not value.exact_half(3) and value.exact_half(4) and not value.exact_unsigned(-1.0) and not value.exact_unsigned(18446744073709551616.0)
for operation in (lambda:value.add(maximum,1),lambda:value.subtract(minimum,1),lambda:value.multiply(minimum,-1),lambda:value.divide(minimum,-1),lambda:value.divide(3,2),lambda:value.divide(0,0),lambda:value.negate(minimum),lambda:value.unsigned_add(unsigned,1),lambda:value.unsigned_subtract(0,1),lambda:value.unsigned_multiply(unsigned,2),lambda:value.bind_unsigned(-1),lambda:value.bind_signed(unsigned),lambda:value.floating_unsigned(unsigned),lambda:value.floating_signed(maximum),lambda:value.unsigned_floating(18446744073709551616.0),lambda:value.signed_floating(9223372036854775808.0),lambda:value.signed_floating(1.5),lambda:value.narrow(0.1),lambda:value.narrow(float('inf')),lambda:value.inexact_float_literal(0.0),lambda:value.inexact_double_literal(0.0)):
    try:operation()
    except (ArithmeticError,ValueError):pass
    else:raise AssertionError('Invalid numeric value accepted')
'''
    if language == "java":
        return '''public final class Main {
private static void check(boolean ok){if(!ok)throw new AssertionError("Checked numeric mismatch");}
public static void main(String[] args){var value=new Schema.NumericValues.Scalar();
check(value.add(Long.MAX_VALUE,0)==Long.MAX_VALUE && value.multiply(Long.MIN_VALUE,1)==Long.MIN_VALUE && value.unsignedAdd(-1L,0)==-1L);
check(value.floatingUnsigned(Long.MIN_VALUE)==9223372036854775808.0 && value.signedFloating(-9223372036854775808.0)==Long.MIN_VALUE && value.unsignedFloating(18446744073709549568.0)==-2048L);
check(value.narrow(0.5)==0.5f && value.widen(0.5f)==0.5 && !value.exactHalf(3) && value.exactHalf(4) && !value.exactUnsigned(-1.0) && !value.exactUnsigned(18446744073709551616.0));
for(Runnable operation:new Runnable[]{()->value.add(Long.MAX_VALUE,1),()->value.subtract(Long.MIN_VALUE,1),()->value.multiply(Long.MIN_VALUE,-1),()->value.divide(Long.MIN_VALUE,-1),()->value.divide(3,2),()->value.divide(0,0),()->value.negate(Long.MIN_VALUE),()->value.unsignedAdd(-1L,1),()->value.unsignedSubtract(0,1),()->value.unsignedMultiply(-1L,2),()->value.bindUnsigned(-1),()->value.bindSigned(-1L),()->value.floatingUnsigned(-1L),()->value.floatingSigned(Long.MAX_VALUE),()->value.unsignedFloating(18446744073709551616.0),()->value.signedFloating(9223372036854775808.0),()->value.signedFloating(1.5),()->value.narrow(0.1),()->value.narrow(Double.POSITIVE_INFINITY),()->value.inexactFloatLiteral(0.0f),()->value.inexactDoubleLiteral(0.0)}){boolean failed=false;try{operation.run();}catch(ArithmeticException failure){failed=true;}check(failed);}}
}
'''
    if language == "csharp":
        return '''using System;
using static SerializerGenerated.Schema;
internal static class Program {
private static void Check(bool ok){if(!ok)throw new Exception("Checked numeric mismatch");}
private static void Main(){var value=new NumericValuesScalar();
Check(value.Add(long.MaxValue,0)==long.MaxValue && value.Multiply(long.MinValue,1)==long.MinValue && value.UnsignedAdd(ulong.MaxValue,0)==ulong.MaxValue);
Check(value.FloatingUnsigned(9223372036854775808UL)==9223372036854775808.0 && value.SignedFloating(-9223372036854775808.0)==long.MinValue && value.UnsignedFloating(18446744073709549568.0)==18446744073709549568UL);
Check(value.Narrow(0.5)==0.5f && value.Widen(0.5f)==0.5 && !value.ExactHalf(3) && value.ExactHalf(4) && !value.ExactUnsigned(-1) && !value.ExactUnsigned(18446744073709551616.0));
foreach(Action operation in new Action[]{()=>value.Add(long.MaxValue,1),()=>value.Subtract(long.MinValue,1),()=>value.Multiply(long.MinValue,-1),()=>value.Divide(long.MinValue,-1),()=>value.Divide(3,2),()=>value.Divide(0,0),()=>value.Negate(long.MinValue),()=>value.UnsignedAdd(ulong.MaxValue,1),()=>value.UnsignedSubtract(0,1),()=>value.UnsignedMultiply(ulong.MaxValue,2),()=>value.BindUnsigned(-1),()=>value.BindSigned(ulong.MaxValue),()=>value.FloatingUnsigned(ulong.MaxValue),()=>value.FloatingSigned(long.MaxValue),()=>value.UnsignedFloating(18446744073709551616.0),()=>value.SignedFloating(9223372036854775808.0),()=>value.SignedFloating(1.5),()=>value.Narrow(0.1),()=>value.Narrow(double.PositiveInfinity),()=>value.InexactFloatLiteral(0.0f),()=>value.InexactDoubleLiteral(0.0)}){bool failed=false;try{operation();}catch(ArithmeticException){failed=true;}Check(failed);}}
}
'''
    if language == "go":
        return '''package main
import "math"
func check(ok bool){if !ok{panic("Checked numeric mismatch")}}
func main(){value:=NewNumericValuesScalar()
check(value.Add(math.MaxInt64,0)==math.MaxInt64 && value.Multiply(math.MinInt64,1)==math.MinInt64 && value.UnsignedAdd(math.MaxUint64,0)==math.MaxUint64)
check(value.FloatingUnsigned(9223372036854775808)==9223372036854775808.0 && value.SignedFloating(-9223372036854775808.0)==math.MinInt64 && value.UnsignedFloating(18446744073709549568.0)==18446744073709549568)
check(value.Narrow(0.5)==0.5 && value.Widen(0.5)==0.5 && !value.ExactHalf(3) && value.ExactHalf(4) && !value.ExactUnsigned(-1) && !value.ExactUnsigned(18446744073709551616.0))
for _,operation:=range []func(){func(){value.Add(math.MaxInt64,1)},func(){value.Subtract(math.MinInt64,1)},func(){value.Multiply(math.MinInt64,-1)},func(){value.Divide(math.MinInt64,-1)},func(){value.Divide(3,2)},func(){value.Divide(0,0)},func(){value.Negate(math.MinInt64)},func(){value.UnsignedAdd(math.MaxUint64,1)},func(){value.UnsignedSubtract(0,1)},func(){value.UnsignedMultiply(math.MaxUint64,2)},func(){value.BindUnsigned(-1)},func(){value.BindSigned(math.MaxUint64)},func(){value.FloatingUnsigned(math.MaxUint64)},func(){value.FloatingSigned(math.MaxInt64)},func(){value.UnsignedFloating(18446744073709551616.0)},func(){value.SignedFloating(9223372036854775808.0)},func(){value.SignedFloating(1.5)},func(){value.Narrow(0.1)},func(){value.Narrow(math.Inf(1))},func(){value.InexactFloatLiteral(0)},func(){value.InexactDoubleLiteral(0)}}{failed:=func()(failed bool){defer func(){if recover()!=nil{failed=true}}();operation();return}();check(failed)}
}
'''
    if language == "rust":
        return '''mod schema;use schema::*;
fn main(){let value=NumericValuesScalar::default();
assert_eq!(value.add(i64::MAX,0),i64::MAX);assert_eq!(value.multiply(i64::MIN,1),i64::MIN);assert_eq!(value.unsigned_add(u64::MAX,0),u64::MAX);
assert_eq!(value.floating_unsigned(9223372036854775808),9223372036854775808.0);assert_eq!(value.signed_floating(-9223372036854775808.0),i64::MIN);assert_eq!(value.unsigned_floating(18446744073709549568.0),18446744073709549568);
assert_eq!(value.narrow(0.5),0.5);assert_eq!(value.widen(0.5),0.5);assert!(!value.exact_half(3));assert!(value.exact_half(4));assert!(!value.exact_unsigned(-1.0));assert!(!value.exact_unsigned(18446744073709551616.0));
for index in 0..21{assert!(std::panic::catch_unwind(||{match index{0=>{value.add(i64::MAX,1);},1=>{value.subtract(i64::MIN,1);},2=>{value.multiply(i64::MIN,-1);},3=>{value.divide(i64::MIN,-1);},4=>{value.divide(3,2);},5=>{value.divide(0,0);},6=>{value.negate(i64::MIN);},7=>{value.unsigned_add(u64::MAX,1);},8=>{value.unsigned_subtract(0,1);},9=>{value.unsigned_multiply(u64::MAX,2);},10=>{value.bind_unsigned(-1);},11=>{value.bind_signed(u64::MAX);},12=>{value.floating_unsigned(u64::MAX);},13=>{value.floating_signed(i64::MAX);},14=>{value.unsigned_floating(18446744073709551616.0);},15=>{value.signed_floating(9223372036854775808.0);},16=>{value.signed_floating(1.5);},17=>{value.narrow(0.1);},18=>{value.narrow(f64::INFINITY);},19=>{value.inexact_float_literal(0.0);},_=>{value.inexact_double_literal(0.0);}}}).is_err());}}
'''
    if language == "swift":
        return '''import Foundation
let value=NumericValuesScalar()
let add=try value.add(Int64.max,0);let multiply=try value.multiply(Int64.min,1);let unsigned=try value.unsignedAdd(UInt64.max,0);precondition(add==Int64.max && multiply==Int64.min && unsigned==UInt64.max)
let floating=try value.floatingUnsigned(9223372036854775808);let signed=try value.signedFloating(-9223372036854775808.0);let upper=try value.unsignedFloating(18446744073709549568.0);precondition(floating==9223372036854775808.0 && signed==Int64.min && upper==18446744073709549568)
let narrow=try value.narrow(0.5);let wide=try value.widen(0.5);let odd=value.exactHalf(3);let even=value.exactHalf(4);let negative=value.exactUnsigned(-1);let overflow=value.exactUnsigned(18446744073709551616.0);precondition(narrow==0.5 && wide==0.5 && !odd && even && !negative && !overflow)
for index in 0..<21{var failed=false;do{switch index{case 0:_=try value.add(Int64.max,1);case 1:_=try value.subtract(Int64.min,1);case 2:_=try value.multiply(Int64.min,-1);case 3:_=try value.divide(Int64.min,-1);case 4:_=try value.divide(3,2);case 5:_=try value.divide(0,0);case 6:_=try value.negate(Int64.min);case 7:_=try value.unsignedAdd(UInt64.max,1);case 8:_=try value.unsignedSubtract(0,1);case 9:_=try value.unsignedMultiply(UInt64.max,2);case 10:_=try value.bindUnsigned(-1);case 11:_=try value.bindSigned(UInt64.max);case 12:_=try value.floatingUnsigned(UInt64.max);case 13:_=try value.floatingSigned(Int64.max);case 14:_=try value.unsignedFloating(18446744073709551616.0);case 15:_=try value.signedFloating(9223372036854775808.0);case 16:_=try value.signedFloating(1.5);case 17:_=try value.narrow(0.1);case 18:_=try value.narrow(Double.infinity);case 19:_=try value.inexactFloatLiteral(0);default:_=try value.inexactDoubleLiteral(0)}}catch{failed=true};precondition(failed)}
'''
    if language == "kotlin":
        return '''fun main(){val value=NumericValuesScalar()
check(value.add(Long.MAX_VALUE,0)==Long.MAX_VALUE && value.multiply(Long.MIN_VALUE,1)==Long.MIN_VALUE && value.unsignedAdd(ULong.MAX_VALUE,0u)==ULong.MAX_VALUE)
check(value.floatingUnsigned(9223372036854775808uL)==9223372036854775808.0 && value.signedFloating(-9223372036854775808.0)==Long.MIN_VALUE && value.unsignedFloating(18446744073709549568.0)==18446744073709549568uL)
check(value.narrow(0.5)==0.5f && value.widen(0.5f)==0.5 && !value.exactHalf(3) && value.exactHalf(4) && !value.exactUnsigned(-1.0) && !value.exactUnsigned(18446744073709551616.0))
for(index in 0..20){check(runCatching{when(index){0->value.add(Long.MAX_VALUE,1);1->value.subtract(Long.MIN_VALUE,1);2->value.multiply(Long.MIN_VALUE,-1);3->value.divide(Long.MIN_VALUE,-1);4->value.divide(3,2);5->value.divide(0,0);6->value.negate(Long.MIN_VALUE);7->value.unsignedAdd(ULong.MAX_VALUE,1u);8->value.unsignedSubtract(0u,1u);9->value.unsignedMultiply(ULong.MAX_VALUE,2u);10->value.bindUnsigned(-1);11->value.bindSigned(ULong.MAX_VALUE);12->value.floatingUnsigned(ULong.MAX_VALUE);13->value.floatingSigned(Long.MAX_VALUE);14->value.unsignedFloating(18446744073709551616.0);15->value.signedFloating(9223372036854775808.0);16->value.signedFloating(1.5);17->value.narrow(0.1);18->value.narrow(Double.POSITIVE_INFINITY);19->value.inexactFloatLiteral(0.0f);else->value.inexactDoubleLiteral(0.0)}}.isFailure)}
}
'''
    if language == "c":
        return '''#include "schema.h"
int main(void){numeric_values_scalar value={0};int64_t signed_result=17;uint64_t unsigned_result=17;float float_result=17;double double_result=17;bool boolean_result=true;
if(numeric_values_scalar_add(&value,INT64_MAX,0)!=INT64_MAX || numeric_values_scalar_multiply(&value,INT64_MIN,1)!=INT64_MIN || numeric_values_scalar_unsigned_add(&value,UINT64_MAX,0)!=UINT64_MAX)return 1;
if(numeric_values_scalar_floating_unsigned(&value,UINT64_C(9223372036854775808))!=9223372036854775808.0 || numeric_values_scalar_signed_floating(&value,-9223372036854775808.0)!=INT64_MIN || numeric_values_scalar_unsigned_floating(&value,18446744073709549568.0)!=UINT64_C(18446744073709549568))return 2;
if(numeric_values_scalar_exact_half(&value,3) || !numeric_values_scalar_exact_half(&value,4) || numeric_values_scalar_exact_unsigned(&value,-1.0) || numeric_values_scalar_exact_unsigned(&value,18446744073709551616.0))return 3;
if(numeric_values_scalar_add_checked(&value,INT64_MAX,1,&signed_result)!=srl_range || signed_result!=17 || numeric_values_scalar_subtract_checked(&value,INT64_MIN,1,&signed_result)!=srl_range || numeric_values_scalar_multiply_checked(&value,INT64_MIN,-1,&signed_result)!=srl_range || numeric_values_scalar_divide_checked(&value,INT64_MIN,-1,&signed_result)!=srl_range || numeric_values_scalar_divide_checked(&value,3,2,&signed_result)!=srl_range || numeric_values_scalar_divide_checked(&value,0,0,&signed_result)!=srl_range || numeric_values_scalar_negate_checked(&value,INT64_MIN,&signed_result)!=srl_range || signed_result!=17)return 4;
if(numeric_values_scalar_unsigned_add_checked(&value,UINT64_MAX,1,&unsigned_result)!=srl_range || numeric_values_scalar_unsigned_subtract_checked(&value,0,1,&unsigned_result)!=srl_range || numeric_values_scalar_unsigned_multiply_checked(&value,UINT64_MAX,2,&unsigned_result)!=srl_range || unsigned_result!=17)return 5;
if(numeric_values_scalar_bind_unsigned_checked(&value,-1,&unsigned_result)!=srl_range || numeric_values_scalar_bind_signed_checked(&value,UINT64_MAX,&signed_result)!=srl_range || numeric_values_scalar_unsigned_floating_checked(&value,18446744073709551616.0,&unsigned_result)!=srl_range || numeric_values_scalar_signed_floating_checked(&value,9223372036854775808.0,&signed_result)!=srl_range || numeric_values_scalar_signed_floating_checked(&value,1.5,&signed_result)!=srl_range || numeric_values_scalar_narrow_checked(&value,0.1,&float_result)!=srl_range || float_result!=17)return 6;
if(numeric_values_scalar_exact_unsigned_checked(&value,-1,&boolean_result)!=srl_ok || boolean_result){return 7;}
if(numeric_values_scalar_floating_unsigned_checked(&value,UINT64_MAX,&double_result)!=srl_range || numeric_values_scalar_floating_signed_checked(&value,INT64_MAX,&double_result)!=srl_range || double_result!=17 || numeric_values_scalar_narrow_checked(&value,INFINITY,&float_result)!=srl_range || numeric_values_scalar_narrow_checked(&value,0.5,&float_result)!=srl_ok || float_result!=0.5f){return 8;}
if(numeric_values_scalar_inexact_float_literal_checked(&value,0,&float_result)!=srl_range || numeric_values_scalar_inexact_double_literal_checked(&value,0,&double_result)!=srl_range){return 9;}
return 0;}
'''
    if language == "cpp":
        return '''#include "message.hpp"
#include <limits>
int main(){const numeric_values::scalar value{};if(value.add(std::numeric_limits<std::int64_t>::max(),0)!=std::numeric_limits<std::int64_t>::max() || value.exact_half(3) || !value.exact_half(4))return 1;
bool invalid_float=false,invalid_double=false;
try{(void)value.inexact_float_literal(0.0f);}catch(const std::domain_error&){invalid_float=true;}
try{(void)value.inexact_double_literal(0.0);}catch(const std::domain_error&){invalid_double=true;}
return invalid_float && invalid_double ? 0 : 2;}
'''
    raise ValueError(language)


def main():
    """Generate through the real compiler and require SDK builds rather than source-only checks."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--cpp-library", type=Path)
    parser.add_argument("--build", type=Path, default=ROOT / "out/numeric-languages")
    parser.add_argument("--language", default="all")
    parser.add_argument("--wsl-languages", default="")
    args = parser.parse_args()
    args.sanitize = False
    args.compiler = args.compiler.resolve()
    args.build = args.build.resolve()
    languages = list(examples.LANGUAGES if args.language == "all" else args.language.split(","))
    targets = {("js" if language == "javascript" else language): args.build / language / examples.OUTPUTS[language] for language in languages}
    if "typescript" in languages and "javascript" not in languages:
        targets["js"] = args.build / "typescript/schema.mjs"
    command = [args.compiler, "--input", ROOT / "test/resources/numeric_values.serializer", "--language", ",".join(targets),
               "--cpp.format=false", "--go.package=main", "--kotlin.package="]
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
        (source / FILES[language]).write_text(consumer(language), encoding="utf-8")
        examples.run(runner.build(language, "numeric", args.build / language, source))
        print(f"PASS: {language}, checked exact numeric casts, full-width arithmetic and predicate failures", flush=True)


if __name__ == "__main__":
    main()

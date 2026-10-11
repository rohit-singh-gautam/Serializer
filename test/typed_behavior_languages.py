"""Compile exact typed predicates and bounded reductions against every supported SDK."""
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
    """Exercise native exact values, reached-only failures and shared traversal limits."""
    if language in ("cpp", "c", "rust", "python"):
        from typed_behavior_consumers_native import consumer as native_consumer
        return native_consumer(language)
    if language in ("swift", "kotlin"):
        from typed_behavior_consumers_mobile import consumer as mobile_consumer
        return mobile_consumer(language)
    if language in ("javascript", "typescript"):
        return r'''import {TypedProbe,ProbePoint,ProbeMode,Protocol} from './schema.mjs';
const check=(ok:boolean)=>{if(!ok)throw Error('Typed predicate mismatch')};
const value=new TypedProbe();
check(value.constantValue(18446744073709551615n));
const matrix=new TypedProbe();matrix.batches=[[1n,2n,3n],[4n,5n,6n]];for(const protocol of Object.values(Protocol)){const restored=TypedProbe.decode(matrix.encode(protocol),protocol);check(restored.batches.length===2 && restored.batches[0][2]===3n && restored.batches[1][0]===4n && restored.batches[1][2]===6n);}
check(value.fixedMatches(2n,0n) && !value.fixedMatches(3n,0n) && !value.fixedMatches(18446744073709551615n,0n));
value.token=18446744073709551615n;value.neighbor=18446744073709551614n;
check(value.exactIdentity() && value.exactOrder() && value.fixedValid());value.token=9007199254740993n;value.neighbor=9007199254740992n;check(value.exactOrder());
value.token=0n;value.neighbor=0n;check(!value.exactIdentity());value.signedToken=-9223372036854775808n;check(value.signedMinimum());
value.mode=ProbeMode.Active;check(value.modeActive());value.inherited.base0.token=value.token;check(value.inheritedIdentity());
value.point.coordinate=NaN;value.point.enabled=false;check(!value.finiteObservation() && !value.finiteValue() && value.guardedValue());value.point.enabled=true;check(!value.guardedValue());
value.point.coordinate=Infinity;check(!value.finiteValue());value.point.coordinate=4;check(value.finiteValue() && !value.divisionValid(0) && !value.divisionValid(-0) && value.divisionValid(2));
check(value.allFinite() && !value.anyEnabled() && !value.safeIndex(0n) && !value.reachedIndex(18446744073709551615n) && !value.signedIndex(-1n));
const point=new ProbePoint();point.coordinate=2;point.enabled=true;value.samples=[point];check(value.safeIndex(0n) && value.reachedIndex(0n) && value.allFinite() && value.anyEnabled());
point.coordinate=NaN;check(!value.safeIndex(0n) && !value.reachedIndex(0n) && !value.allFinite());value.samples=Array(17).fill(point);check(!value.allFinite() && !value.anyEnabled());
value.masks=[0n,18446744073709551615n];check(value.masksValid());value.masks=[9007199254740993n];check(!value.masksValid());
value.switches=[true,true];check(value.switchesValid());value.switches=[true,false];check(!value.switchesValid());
value.batches=[[0n,0n],[0n,0n]];check(value.nestedBudget());value.batches=Array(1000).fill(Array(1000).fill(0n));check(!value.nestedBudget());
'''.replace("ok:boolean", "ok: boolean" if language == "typescript" else "ok")
    if language == "java":
        return r'''public final class Main {
  private static void check(boolean value){if(!value)throw new AssertionError("Typed predicate mismatch");}
  public static void main(String[] args){
    var value=new Schema.TypedProbe();check(value.constantValue(-1L));check(value.fixedMatches(2,0) && !value.fixedMatches(3,0) && !value.fixedMatches(-1L,0));value.token=-1L;value.neighbor=-2L;check(value.exactIdentity() && value.exactOrder() && value.fixedValid());
    value.token=9007199254740993L;value.neighbor=9007199254740992L;check(value.exactOrder());value.token=0;value.neighbor=0;check(!value.exactIdentity());
    value.signedToken=Long.MIN_VALUE;check(value.signedMinimum());value.mode=Schema.ProbeMode.ACTIVE;check(value.modeActive());value.inherited.base0.token=value.token;check(value.inheritedIdentity());
    value.point.coordinate=Double.NaN;value.point.enabled=false;check(!value.finiteObservation() && !value.finiteValue() && value.guardedValue());value.point.enabled=true;check(!value.guardedValue());
    value.point.coordinate=Double.POSITIVE_INFINITY;check(!value.finiteValue());value.point.coordinate=4;check(value.finiteValue() && !value.divisionValid(0) && !value.divisionValid(-0.0) && value.divisionValid(2));
    check(value.allFinite() && !value.anyEnabled() && !value.safeIndex(0) && !value.reachedIndex(-1L) && !value.signedIndex(-1));
    var point=new Schema.ProbePoint();point.coordinate=2;point.enabled=true;value.samples=new Schema.ProbePoint[]{point};check(value.safeIndex(0) && value.reachedIndex(0) && value.allFinite() && value.anyEnabled());
    point.coordinate=Double.NaN;check(!value.safeIndex(0) && !value.reachedIndex(0) && !value.allFinite());value.samples=new Schema.ProbePoint[17];java.util.Arrays.fill(value.samples,point);check(!value.allFinite() && !value.anyEnabled());
    value.masks=new long[]{0,-1L};check(value.masksValid());value.masks=new long[]{9007199254740993L};check(!value.masksValid());
    value.switches=new boolean[]{true,true};check(value.switchesValid());value.switches=new boolean[]{true,false};check(!value.switchesValid());
    value.batches=java.util.List.of(java.util.List.of(0L,0L),java.util.List.of(0L,0L));check(value.nestedBudget());value.batches=java.util.Collections.nCopies(1000,java.util.Collections.nCopies(1000,0L));check(!value.nestedBudget());
  }
}
'''
    if language == "csharp":
        return r'''using System;
using System.Collections.Generic;
using static SerializerGenerated.Schema;
internal static class Program {
  private static void Check(bool value){if(!value)throw new Exception("Typed predicate mismatch");}
  private static void Main(){
    var matrix=new TypedProbe{Batches=new List<List<ulong>>{new(){1,2,3},new(){4,5,6}}};foreach(Protocol protocol in Enum.GetValues<Protocol>()){var restored=TypedProbe.Decode(matrix.Encode(protocol),protocol);Check(restored.Batches.Count==2 && restored.Batches[0][2]==3 && restored.Batches[1][0]==4 && restored.Batches[1][2]==6);}
    var value=new TypedProbe();Check(value.ConstantValue(ulong.MaxValue));Check(value.FixedMatches(2,0) && !value.FixedMatches(3,0) && !value.FixedMatches(ulong.MaxValue,0));value.Token=ulong.MaxValue;value.Neighbor=ulong.MaxValue-1;Check(value.ExactIdentity() && value.ExactOrder() && value.FixedValid());
    value.Token=9007199254740993;value.Neighbor=9007199254740992;Check(value.ExactOrder());value.Token=0;value.Neighbor=0;Check(!value.ExactIdentity());
    value.SignedToken=long.MinValue;Check(value.SignedMinimum());value.Mode=ProbeMode.Active;Check(value.ModeActive());value.Inherited.Base0.Token=value.Token;Check(value.InheritedIdentity());
    value.Point.Coordinate=double.NaN;value.Point.Enabled=false;Check(!value.FiniteObservation() && !value.FiniteValue() && value.GuardedValue());value.Point.Enabled=true;Check(!value.GuardedValue());
    value.Point.Coordinate=double.PositiveInfinity;Check(!value.FiniteValue());value.Point.Coordinate=4;Check(value.FiniteValue() && !value.DivisionValid(0) && !value.DivisionValid(-0.0) && value.DivisionValid(2));
    Check(value.AllFinite() && !value.AnyEnabled() && !value.SafeIndex(0) && !value.ReachedIndex(ulong.MaxValue) && !value.SignedIndex(-1));
    var point=new ProbePoint{Coordinate=2,Enabled=true};value.Samples=new List<ProbePoint>{point};Check(value.SafeIndex(0) && value.ReachedIndex(0) && value.AllFinite() && value.AnyEnabled());
    point.Coordinate=double.NaN;Check(!value.SafeIndex(0) && !value.ReachedIndex(0) && !value.AllFinite());value.Samples=new List<ProbePoint>();for(int index=0;index<17;index++)value.Samples.Add(point);Check(!value.AllFinite() && !value.AnyEnabled());
    value.Masks=new List<ulong>{0,ulong.MaxValue};Check(value.MasksValid());value.Masks=new List<ulong>{9007199254740993};Check(!value.MasksValid());value.Switches=new List<bool>{true,true};Check(value.SwitchesValid());value.Switches=new List<bool>{true,false};Check(!value.SwitchesValid());
    value.Batches=new List<List<ulong>>{new(){0,0},new(){0,0}};Check(value.NestedBudget());var inner=new List<ulong>();for(int index=0;index<1000;index++)inner.Add(0);value.Batches=new List<List<ulong>>();for(int index=0;index<1000;index++)value.Batches.Add(inner);Check(!value.NestedBudget());
  }
}
'''
    return r'''package main
import "math"
func check(value bool){if !value{panic("Typed predicate mismatch")}}
func main(){
 matrix:=NewTypedProbe();matrix.Batches=[][]uint64{{1,2,3},{4,5,6}};for _,protocol:=range []Protocol{JSON,BINARY_NONE,BINARY_INTEGER,BINARY_STRING}{encoded,err:=matrix.Encode(protocol);if err!=nil{panic(err)};restored,err:=DecodeTypedProbe(encoded,protocol);if err!=nil{panic(err)};check(len(restored.Batches)==2 && restored.Batches[0][2]==3 && restored.Batches[1][0]==4 && restored.Batches[1][2]==6)}
 value:=NewTypedProbe();check(value.ConstantValue(math.MaxUint64));check(value.FixedMatches(2,0) && !value.FixedMatches(3,0) && !value.FixedMatches(math.MaxUint64,0));value.Token=math.MaxUint64;value.Neighbor=math.MaxUint64-1;check(value.ExactIdentity() && value.ExactOrder() && value.FixedValid());value.Token=9007199254740993;value.Neighbor=9007199254740992;check(value.ExactOrder());value.Token=0;value.Neighbor=0;check(!value.ExactIdentity());
 value.SignedToken=math.MinInt64;check(value.SignedMinimum());value.Mode=ProbeModeActive;check(value.ModeActive());value.Inherited.Base0.Token=value.Token;check(value.InheritedIdentity());
 value.Point.Coordinate=math.NaN();value.Point.Enabled=false;check(!value.FiniteObservation() && !value.FiniteValue() && value.GuardedValue());value.Point.Enabled=true;check(!value.GuardedValue());value.Point.Coordinate=math.Inf(1);check(!value.FiniteValue());value.Point.Coordinate=4;check(value.FiniteValue() && !value.DivisionValid(0) && !value.DivisionValid(math.Copysign(0,-1)) && value.DivisionValid(2));
 check(value.AllFinite() && !value.AnyEnabled() && !value.SafeIndex(0) && !value.ReachedIndex(math.MaxUint64) && !value.SignedIndex(-1));point:=NewProbePoint();point.Coordinate=2;point.Enabled=true;value.Samples=[]*ProbePoint{point};check(value.SafeIndex(0) && value.ReachedIndex(0) && value.AllFinite() && value.AnyEnabled());point.Coordinate=math.NaN();check(!value.SafeIndex(0) && !value.ReachedIndex(0) && !value.AllFinite());value.Samples=make([]*ProbePoint,17);for index:=range value.Samples{value.Samples[index]=point};check(!value.AllFinite() && !value.AnyEnabled());
 value.Masks=[]uint64{0,math.MaxUint64};check(value.MasksValid());value.Masks=[]uint64{9007199254740993};check(!value.MasksValid());value.Switches=[]bool{true,true};check(value.SwitchesValid());value.Switches=[]bool{true,false};check(!value.SwitchesValid());value.Batches=[][]uint64{{0,0},{0,0}};check(value.NestedBudget());inner:=make([]uint64,1000);value.Batches=make([][]uint64,1000);for index:=range value.Batches{value.Batches[index]=inner};check(!value.NestedBudget());
}
'''


def main():
    """Generate one shared fixture and require real strict SDK builds and runtime checks."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--cpp-library", type=Path)
    parser.add_argument("--build", type=Path, default=ROOT / "out/typed-behavior-languages")
    parser.add_argument("--language", default="all")
    parser.add_argument("--wsl-languages", default="")
    args = parser.parse_args()
    args.sanitize = False
    args.compiler = args.compiler.resolve()
    if args.cpp_library:
        args.cpp_library = args.cpp_library.resolve()
    args.build = args.build.resolve()
    languages = list(examples.LANGUAGES if args.language == "all" else args.language.split(","))
    targets = {("js" if language == "javascript" else language): args.build / language / examples.OUTPUTS[language] for language in languages}
    if "typescript" in languages and "javascript" not in languages:
        targets["js"] = args.build / "typescript/schema.mjs"
    command = [args.compiler, "--input", ROOT / "test/resources/typed_behavior.serializer", "--language", ",".join(targets),
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
        executable = runner.build(language, "typed_behavior", args.build / language, source)
        examples.run(executable)
        print(f"PASS: {language}, exact scalar predicates, typed projections, finite observations and bounded reductions", flush=True)
        if language in ("java", "csharp"):
            qualify_preserved_budget(args, runner, language)


def qualify_preserved_budget(args, runner, language):
    """Compile a referenced parameter that previously shadowed the private reduction state."""
    directory = args.build / (language + "-preserve")
    directory.mkdir(parents=True, exist_ok=True)
    examples.run([args.compiler, "--input", ROOT / "test/resources/typed_behavior_shadow.serializer",
                  "--language", language, f"--{language}.naming=preserve",
                  f"--{language}.output", directory / examples.OUTPUTS[language]])
    source = args.build / "consumers" / (language + "-preserve")
    source.mkdir(parents=True, exist_ok=True)
    if language == "java":
        authored = '''public final class Main {
  public static void main(String[] args) {
    var value = new Schema.typed_budget_shadow();
    value.values = new long[]{-1L};
    if (!value.valid(-1L) || value.valid(0L)) throw new AssertionError("Private budget changed parameter binding");
  }
}
'''
    else:
        authored = '''using System;
using System.Collections.Generic;
using static SerializerGenerated.Schema;
internal static class Program {
  private static void Main() {
    var value = new typed_budget_shadow { values = new List<ulong>{ulong.MaxValue} };
    if (!value.valid(ulong.MaxValue) || value.valid(0)) throw new Exception("Private budget changed parameter binding");
  }
}
'''
    (source / FILES[language]).write_text(authored, encoding="utf-8")
    examples.run(runner.build(language, "typed_behavior_preserve", directory, source))
    print(f"PASS: {language}, preserved parameter binding and private reduction state", flush=True)


if __name__ == "__main__":
    main()

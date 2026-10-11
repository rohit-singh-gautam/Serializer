# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: GPL-3.0-or-later
"""Real Swift and Kotlin consumers for exact and bounded schema predicates."""


def consumer(language):
    """Exercise reached failures, lazy branches and one shared nested reduction budget."""
    if language == "swift":
        return r'''import Foundation
var value=TypedProbe()
precondition(value.constantValue(0) && value.constantValue(UInt64.max))
value.token=UInt64.max;value.neighbor=UInt64.max-1
let identity=try value.exactIdentity();let ordered=try value.exactOrder()
precondition(identity && ordered)
value.token=9007199254740993;value.neighbor=9007199254740992
let adjacent=try value.exactOrder();precondition(adjacent)
value.token=0;value.neighbor=0
let emptyIdentity=try value.exactIdentity();precondition(!emptyIdentity)
value.signedToken=Int64.min
let minimum=try value.signedMinimum();precondition(minimum)
value.signedToken=Int64.max
let maximum=try value.signedMinimum();precondition(!maximum)
value.mode = .Active
let active=try value.modeActive();precondition(active)
value.mode = .Inactive
let inactive=try value.modeActive();precondition(!inactive)
value.token=UInt64.max;value.inherited.base0.token=value.token
let inherited=try value.inheritedIdentity();precondition(inherited)
value.inherited.base0.token=0
let different=try value.inheritedIdentity();precondition(!different)
value.point.coordinate=Double.nan;value.point.enabled=false
let observed=try value.finiteObservation();precondition(!observed)
precondition(!value.finiteValue() && value.guardedValue())
value.point.enabled=true;precondition(!value.guardedValue())
value.point.coordinate=Double.infinity;precondition(!value.finiteValue())
value.point.coordinate=4;precondition(value.finiteValue() && value.guardedValue())
precondition(value.divisionValid(2) && !value.divisionValid(0) && !value.divisionValid(-0.0))
precondition(!value.divisionValid(Double.nan) && !value.divisionValid(Double.infinity))
precondition(!value.safeIndex(0) && !value.reachedIndex(UInt64.max))
precondition(value.allFinite() && !value.anyEnabled())
var point=ProbePoint();point.coordinate=2;point.enabled=true
value.samples=[point]
precondition(value.safeIndex(0) && value.reachedIndex(0) && !value.safeIndex(UInt64.max))
precondition(value.signedIndex(0) && !value.signedIndex(-1) && !value.signedIndex(Int64.min))
precondition(!value.reachedIndex(1) && value.allFinite() && value.anyEnabled())
value.samples[0].coordinate=Double.nan
precondition(!value.safeIndex(0) && !value.reachedIndex(0) && !value.allFinite() && value.anyEnabled())
value.samples=Array(repeating:point,count:17)
precondition(!value.allFinite() && !value.anyEnabled())
value.masks=[0,UInt64.max];precondition(value.masksValid())
value.masks=[1];precondition(!value.masksValid())
precondition(value.fixedValid() && value.fixedMatches(2,0) && !value.fixedMatches(3,0) && !value.fixedMatches(UInt64.max,0))
value.fixedMasks[1]=1;precondition(!value.fixedValid())
value.switches=[true,true];precondition(value.switchesValid())
value.switches=[true,false];precondition(!value.switchesValid())
value.batches=[[0,0],[0,0]];precondition(value.nestedBudget())
value.batches=Array(repeating:Array(repeating:UInt64(0),count:1000),count:1000)
precondition(!value.nestedBudget())
value.batches=[];precondition(value.nestedBudget())
'''
    if language == "kotlin":
        return r'''@file:OptIn(kotlin.ExperimentalUnsignedTypes::class)
fun main(){
  val value=TypedProbe()
  check(value.constantValue(0uL) && value.constantValue(ULong.MAX_VALUE))
  value.token=ULong.MAX_VALUE;value.neighbor=ULong.MAX_VALUE-1uL
  check(value.exactIdentity() && value.exactOrder())
  value.token=9007199254740993uL;value.neighbor=9007199254740992uL
  check(value.exactOrder())
  value.token=0uL;value.neighbor=0uL;check(!value.exactIdentity())
  value.signedToken=Long.MIN_VALUE;check(value.signedMinimum())
  value.signedToken=Long.MAX_VALUE;check(!value.signedMinimum())
  value.mode=ProbeMode.Active;check(value.modeActive())
  value.mode=ProbeMode.Inactive;check(!value.modeActive())
  value.token=ULong.MAX_VALUE;value.inherited.base0.token=value.token
  check(value.inheritedIdentity())
  value.inherited.base0.token=0uL;check(!value.inheritedIdentity())
  value.point.coordinate=Double.NaN;value.point.enabled=false
  check(!value.finiteObservation() && !value.finiteValue() && value.guardedValue())
  value.point.enabled=true;check(!value.guardedValue())
  value.point.coordinate=Double.POSITIVE_INFINITY;check(!value.finiteValue())
  value.point.coordinate=4.0;check(value.finiteValue() && value.guardedValue())
  check(value.divisionValid(2.0) && !value.divisionValid(0.0) && !value.divisionValid(-0.0))
  check(!value.divisionValid(Double.NaN) && !value.divisionValid(Double.POSITIVE_INFINITY))
  check(!value.safeIndex(0uL) && !value.reachedIndex(ULong.MAX_VALUE))
  check(value.allFinite() && !value.anyEnabled())
  val point=ProbePoint();point.coordinate=2.0;point.enabled=true
  value.samples=mutableListOf(point)
  check(value.safeIndex(0uL) && value.reachedIndex(0uL) && !value.safeIndex(ULong.MAX_VALUE))
  check(value.signedIndex(0L) && !value.signedIndex(-1L) && !value.signedIndex(Long.MIN_VALUE))
  check(!value.reachedIndex(1uL) && value.allFinite() && value.anyEnabled())
  value.samples[0].coordinate=Double.NaN
  check(!value.safeIndex(0uL) && !value.reachedIndex(0uL) && !value.allFinite() && value.anyEnabled())
  val fresh=ProbePoint();fresh.coordinate=2.0;fresh.enabled=true
  value.samples=MutableList(17){fresh}
  check(!value.allFinite() && !value.anyEnabled())
  value.masks=ulongArrayOf(0uL,ULong.MAX_VALUE);check(value.masksValid())
  value.masks=ulongArrayOf(1uL);check(!value.masksValid())
  check(value.fixedValid() && value.fixedMatches(2uL,0uL) && !value.fixedMatches(3uL,0uL) && !value.fixedMatches(ULong.MAX_VALUE,0uL))
  value.fixedMasks[1]=1uL;check(!value.fixedValid())
  value.switches=booleanArrayOf(true,true);check(value.switchesValid())
  value.switches=booleanArrayOf(true,false);check(!value.switchesValid())
  value.batches=mutableListOf(mutableListOf(0uL,0uL),mutableListOf(0uL,0uL))
  check(value.nestedBudget())
  value.batches=MutableList(1000){MutableList(1000){0uL}}
  check(!value.nestedBudget())
  value.batches=mutableListOf();check(value.nestedBudget())
}
'''
    raise ValueError("Unsupported mobile predicate language: " + language)

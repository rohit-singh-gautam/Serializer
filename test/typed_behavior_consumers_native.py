"""Real SDK consumers for exact native readonly predicates and bounded projections."""


def consumer(language):
    """Exercise native values, lazy reached failures, finite observations and shared traversal limits."""
    sources = {
        "cpp": r'''#include "message.hpp"
#include <limits>
#include <utility>
constexpr bool qualify(){typed_probe value{};value.token=UINT64_MAX;value.neighbor=UINT64_MAX-1;value.signed_token=INT64_MIN;value.mode=probe_mode::active;value.inherited.token=UINT64_MAX;return value.exact_identity()&&value.exact_order()&&value.signed_minimum()&&value.mode_active()&&value.inherited_identity()&&!value.reached_index(UINT64_MAX)&&value.all_finite()&&!value.any_enabled()&&value.fixed_valid()&&value.fixed_matches(2,0)&&!value.fixed_matches(3,0)&&!value.fixed_matches(UINT64_MAX,0);}
static_assert(qualify());
static_assert(typed_probe{}.constant_value(0));
static_assert(noexcept(std::declval<const typed_probe&>().exact_order()));
static_assert(noexcept(std::declval<const typed_probe&>().reached_index(0)));
static_assert(noexcept(std::declval<const typed_probe&>().division_valid(0)));
static_assert(noexcept(std::declval<const typed_probe&>().finite_observation()));
int main(){
  typed_probe value{};value.token=UINT64_MAX;value.neighbor=UINT64_MAX-1;value.signed_token=INT64_MIN;value.mode=probe_mode::active;value.inherited.token=value.token;
  if(!value.constant_value(0))return 18;
  if(!value.exact_identity()||!value.exact_order()||!value.signed_minimum()||!value.mode_active()||!value.inherited_identity())return 1;
  if(!value.fixed_valid()||!value.fixed_matches(2,0)||value.fixed_matches(3,0)||value.fixed_matches(UINT64_MAX,0))return 16;value.fixed_masks[1]=1;if(value.fixed_valid())return 17;
  value.token=9007199254740993ULL;value.neighbor=9007199254740992ULL;if(!value.exact_order())return 2;
  value.point.coordinate=std::numeric_limits<double>::quiet_NaN();if(value.finite_observation()||value.finite_value()||!value.guarded_value())return 3;
  value.point.enabled=true;if(value.guarded_value())return 4;value.point.coordinate=std::numeric_limits<double>::infinity();if(value.finite_value()||value.finite_observation())return 5;
  value.point.coordinate=2;if(!value.finite_value()||!value.division_valid(2)||value.division_valid(0)||value.division_valid(-0.0)||value.division_valid(std::numeric_limits<double>::infinity()))return 6;
  if(value.safe_index(UINT64_MAX)||value.reached_index(0)||!value.all_finite()||value.any_enabled())return 7;
  probe_point point{};point.coordinate=1;point.enabled=true;value.samples={point};
  if(!value.safe_index(0)||!value.reached_index(0)||value.reached_index(UINT64_MAX)||!value.all_finite()||!value.any_enabled())return 8;
  value.samples[0].coordinate=std::numeric_limits<double>::quiet_NaN();if(value.safe_index(0)||value.reached_index(0)||value.all_finite())return 9;
  value.samples.assign(17,point);if(value.all_finite()||value.any_enabled())return 10;
  value.masks={0,UINT64_MAX};value.switches={true,true};if(!value.masks_valid()||!value.switches_valid())return 11;
  value.masks[1]=UINT64_MAX-1;value.switches[1]=false;if(value.masks_valid()||value.switches_valid())return 12;
  value.batches={{0,0},{0,0}};if(!value.nested_budget())return 13;value.batches.assign(1000,std::vector<std::uint64_t>(1000,0));if(value.nested_budget())return 14;value.batches={{0}};if(!value.nested_budget())return 15;
}
''',
        "rust": r'''mod schema;
use schema::*;
fn main(){
  let mut value=TypedProbe::default();value.token=u64::MAX;value.neighbor=u64::MAX-1;value.signed_token=i64::MIN;value.mode=ProbeMode::Active;value.inherited.base0.token=value.token;
  assert!(value.constant_value(0));
  assert!(value.exact_identity()&&value.exact_order()&&value.signed_minimum()&&value.mode_active()&&value.inherited_identity());
  assert!(value.fixed_valid()&&value.fixed_matches(2,0)&&!value.fixed_matches(3,0)&&!value.fixed_matches(u64::MAX,0));value.fixed_masks[1]=1;assert!(!value.fixed_valid());
  value.token=9007199254740993;value.neighbor=9007199254740992;assert!(value.exact_order());
  value.point.coordinate=f64::NAN;assert!(!value.finite_observation()&&!value.finite_value()&&value.guarded_value());
  value.point.enabled=true;assert!(!value.guarded_value());value.point.coordinate=f64::INFINITY;assert!(!value.finite_value()&&!value.finite_observation());
  value.point.coordinate=2.0;assert!(value.finite_value()&&value.division_valid(2.0)&&!value.division_valid(0.0)&&!value.division_valid(-0.0)&&!value.division_valid(f64::INFINITY));
  assert!(!value.safe_index(u64::MAX)&&!value.reached_index(0)&&value.all_finite()&&!value.any_enabled());
  let mut point=ProbePoint::default();point.coordinate=1.0;point.enabled=true;value.samples=vec![point.clone()];
  assert!(value.safe_index(0)&&value.reached_index(0)&&!value.reached_index(u64::MAX)&&value.all_finite()&&value.any_enabled());
  value.samples[0].coordinate=f64::NAN;assert!(!value.safe_index(0)&&!value.reached_index(0)&&!value.all_finite());
  value.samples=vec![point;17];assert!(!value.all_finite()&&!value.any_enabled());
  value.masks=vec![0,u64::MAX];value.switches=vec![true,true];assert!(value.masks_valid()&&value.switches_valid());value.masks[1]=u64::MAX-1;value.switches[1]=false;assert!(!value.masks_valid()&&!value.switches_valid());
  value.batches=vec![vec![0,0],vec![0,0]];assert!(value.nested_budget());value.batches=vec![vec![0;1000];1000];assert!(!value.nested_budget());value.batches=vec![vec![0]];assert!(value.nested_budget());
}
''',
        "python": r'''from schema import *
value=TypedProbe();value.token=(1<<64)-1;value.neighbor=value.token-1;value.signed_token=-(1<<63);value.mode=ProbeMode.Active;value.inherited.base0.token=value.token
assert value.constant_value(0)
assert value.exact_identity() and value.exact_order() and value.signed_minimum() and value.mode_active() and value.inherited_identity()
assert value.fixed_valid() and value.fixed_matches(2,0) and not value.fixed_matches(3,0) and not value.fixed_matches((1<<64)-1,0);value.fixed_masks[1]=1;assert not value.fixed_valid()
value.token=(1<<53)+1;value.neighbor=1<<53;assert value.exact_order()
value.point.coordinate=float('nan');assert not value.finite_observation() and not value.finite_value() and value.guarded_value()
value.point.enabled=True;assert not value.guarded_value();value.point.coordinate=float('inf');assert not value.finite_value() and not value.finite_observation()
value.point.coordinate=2;assert value.finite_value() and value.division_valid(2) and not value.division_valid(0) and not value.division_valid(-0.0) and not value.division_valid(float('inf'))
assert not value.safe_index((1<<64)-1) and not value.reached_index(0) and value.all_finite() and not value.any_enabled()
point=ProbePoint();point.coordinate=1;point.enabled=True;value.samples=[point]
assert value.safe_index(0) and value.reached_index(0) and not value.reached_index((1<<64)-1) and value.all_finite() and value.any_enabled()
point.coordinate=float('nan');assert not value.safe_index(0) and not value.reached_index(0) and not value.all_finite()
point.coordinate=1;value.samples=[point]*17;assert not value.all_finite() and not value.any_enabled()
value.masks=[0,(1<<64)-1];value.switches=[True,True];assert value.masks_valid() and value.switches_valid()
value.masks[1]=(1<<64)-2;value.switches[1]=False;assert not value.masks_valid() and not value.switches_valid()
value.batches=[[0,0],[0,0]];assert value.nested_budget();value.batches=[[0]*1000 for _ in range(1000)];assert not value.nested_budget();value.batches=[[0]];assert value.nested_budget()
value.point.enabled=False;value.point.coordinate=object();assert value.guarded_value();value.point.enabled=True;assert not value.guarded_value()
value.point.coordinate=True;assert not value.finite_value();value.samples=[];assert not value.safe_index(True) and not value.reached_index(-1)
value.point.coordinate=1<<2048;assert not value.finite_observation() and not value.finite_value()
value.masks=[True];assert not value.masks_valid();value.switches=[1];assert not value.switches_valid()
value.mode=1
try:value.mode_active()
except ValueError:pass
else:raise AssertionError('Malformed enum accepted')
value.token=True
try:value.exact_order()
except ValueError:pass
else:raise AssertionError('Malformed integer accepted')
''',
        "c": r'''#include "schema.h"
int main(void){
  typed_probe value={0};if(typed_probe_init(&value)!=srl_ok){return 30;}
  if(!typed_probe_constant_value(&value,0)){return 18;}
  value.token=UINT64_MAX;value.neighbor=UINT64_MAX-1;value.signed_token=INT64_MIN;value.mode=probe_mode_active;value.inherited.base0.token=value.token;
  if(!typed_probe_exact_identity(&value)||!typed_probe_exact_order(&value)||!typed_probe_signed_minimum(&value)||!typed_probe_mode_active(&value)||!typed_probe_inherited_identity(&value)){return 1;}
  if(!typed_probe_fixed_valid(&value)||!typed_probe_fixed_matches(&value,2,0)||typed_probe_fixed_matches(&value,3,0)||typed_probe_fixed_matches(&value,UINT64_MAX,0)){return 16;}value.fixed_masks.data[1]=1;if(typed_probe_fixed_valid(&value)){return 17;}
  value.token=UINT64_C(9007199254740993);value.neighbor=UINT64_C(9007199254740992);if(!typed_probe_exact_order(&value)){return 2;}
  value.point.coordinate=NAN;if(typed_probe_finite_observation(&value)||typed_probe_finite_value(&value)||!typed_probe_guarded_value(&value)){return 3;}
  value.point.enabled=true;if(typed_probe_guarded_value(&value)){return 4;}value.point.coordinate=INFINITY;if(typed_probe_finite_value(&value)||typed_probe_finite_observation(&value)){return 5;}
  value.point.coordinate=2;if(!typed_probe_finite_value(&value)||!typed_probe_division_valid(&value,2)||typed_probe_division_valid(&value,0)||typed_probe_division_valid(&value,-0.0)||typed_probe_division_valid(&value,INFINITY)){return 6;}
  bool result=true;errno=0;if(typed_probe_division_valid_checked(&value,0,&result)!=srl_ok||result||errno!=0){return 20;}
  result=true;if(typed_probe_division_valid_checked(NULL,0,&result)!=srl_invalid||!result||typed_probe_division_valid_checked(&value,0,NULL)!=srl_invalid){return 21;}
  if(typed_probe_safe_index(&value,UINT64_MAX)||typed_probe_reached_index(&value,0)||!typed_probe_all_finite(&value)||typed_probe_any_enabled(&value)){return 7;}
  value.samples.data=calloc(17,sizeof(*value.samples.data));if(!value.samples.data){return 31;}value.samples.size=1;value.samples.data[0].coordinate=1;value.samples.data[0].enabled=true;
  if(!typed_probe_safe_index(&value,0)||!typed_probe_reached_index(&value,0)||typed_probe_reached_index(&value,UINT64_MAX)||!typed_probe_all_finite(&value)||!typed_probe_any_enabled(&value)){return 8;}
  value.samples.data[0].coordinate=NAN;if(typed_probe_safe_index(&value,0)||typed_probe_reached_index(&value,0)||typed_probe_all_finite(&value)){return 9;}
  value.samples.size=17;for(size_t index=0;index<17;++index){value.samples.data[index].coordinate=1;value.samples.data[index].enabled=true;}if(typed_probe_all_finite(&value)||typed_probe_any_enabled(&value)){return 10;}
  value.masks.data=calloc(2,sizeof(*value.masks.data));value.switches.data=calloc(2,sizeof(*value.switches.data));if(!value.masks.data||!value.switches.data){return 32;}
  value.masks.size=value.switches.size=2;value.masks.data[1]=UINT64_MAX;value.switches.data[0]=value.switches.data[1]=true;if(!typed_probe_masks_valid(&value)||!typed_probe_switches_valid(&value)){return 11;}
  value.masks.data[1]=UINT64_MAX-1;value.switches.data[1]=false;if(typed_probe_masks_valid(&value)||typed_probe_switches_valid(&value)){return 12;}
  value.batches.data=calloc(1000,sizeof(*value.batches.data));if(!value.batches.data){return 33;}value.batches.size=2;
  for(size_t index=0;index<1000;++index){value.batches.data[index].data=calloc(1000,sizeof(*value.batches.data[index].data));if(!value.batches.data[index].data){return 34;}value.batches.data[index].size=2;}
  if(!typed_probe_nested_budget(&value)){return 13;}value.batches.size=1000;for(size_t index=0;index<1000;++index){value.batches.data[index].size=1000;}if(typed_probe_nested_budget(&value)){return 14;}
  value.batches.size=1;value.batches.data[0].size=1;if(!typed_probe_nested_budget(&value)){return 15;}value.batches.size=1000;value.batches.data[0].size=1000;
  value.mode=99;result=true;if(typed_probe_mode_active_checked(&value,&result)!=srl_range||!result){return 22;}
  typed_probe_free(&value);return 0;
}
''',
    }
    return sources[language]

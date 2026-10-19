// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"serializer(
// XmlMixedLimits bounds one native ordered mixed sequence independently of ordinary XML/map limits.
type XmlMixedLimits struct { MaxFragments, MaxTextBytes int }
// DefaultXmlMixedLimits derives immutable per-operation defaults from the shared XML contract.
func DefaultXmlMixedLimits()XmlMixedLimits{return XmlMixedLimits{srlXmlMixedMaxFragments,srlXmlMixedMaxTextBytes}}
// Reject invalid policies before touching input or model storage; zero remains exact admission.
func srlXmlMixedPolicy(limits XmlMixedLimits)error{if limits.MaxFragments<0||limits.MaxTextBytes<0{return fmt.Errorf("Negative XML mixed limit")};return nil}
type srlXmlMixedBudget struct{limits XmlMixedLimits;fragments,textBytes int}
// Charge each child/logical text run before retaining its native fragment.
func (b *srlXmlMixedBudget) fragment()error{if b.fragments>=b.limits.MaxFragments{return fmt.Errorf("XML mixed fragment limit exceeded")};b.fragments++;return nil}
// Admit cumulative UTF8 text and XML Unicode before appending another source chunk.
func (b *srlXmlMixedBudget) text(text string)error{
  if len(text)>b.limits.MaxTextBytes-b.textBytes{return fmt.Errorf("XML mixed text limit exceeded")};if !utf8.ValidString(text){return fmt.Errorf("Invalid XML UTF-8")}
  for _,value:=range text{if !(value==9||value==10||value==13||value>=32&&value<=0xd7ff||value>=0xe000&&value<=0xfffd||value>=0x10000&&value<=0x10ffff){return fmt.Errorf("Invalid XML character")}}
  b.textBytes+=len(text);return nil
}
// Observe one raw event without discarding significant mixed whitespace.
func (r *srlXmlInput) mixedEvent()(*srlXmlInputToken,error){if r.look==nil{event,err:=r.read();if err!=nil{return nil,err};r.look=event};return r.look,nil}
// Coalesce one logical text run across provider chunk/entity/CDATA/comment boundaries.
func (r *srlXmlInput) mixedText(budget *srlXmlMixedBudget)(string,error){
  var text strings.Builder;charged:=false
  for{event,err:=r.mixedEvent();if err!=nil{return "",err};if event.kind!='t'{return text.String(),nil};if event.text!=""{if !charged{if err=budget.fragment();err!=nil{return "",err};charged=true};if err=budget.text(event.text);err!=nil{return "",err};text.WriteString(event.text)};r.look=nil}
}
)serializer"

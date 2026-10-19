// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"htmlgo(
// HtmlLimits bounds approved ordinary HTML independently of XML and native wire policies.
type HtmlLimits struct{MaxInputBytes,MaxOutputBytes,MaxEvents,MaxDepth,MaxAttributesPerElement,MaxTextBytes int}
// DefaultHtmlLimits derives immutable operation defaults from the shared approved HTML contract.
func DefaultHtmlLimits()HtmlLimits{return HtmlLimits{srlHtmlMaxInputBytes,srlHtmlMaxOutputBytes,srlHtmlMaxEvents,srlHtmlMaxDepth,srlHtmlMaxAttributesPerElement,srlHtmlMaxTextBytes}}
// HtmlAttribute owns one lowercase attribute; hidden uses explicit Boolean presence.
type HtmlAttribute struct{Name,Value string;Boolean bool}
// HtmlEvent owns a semantic fragment/start/text/end event without XML namespaces or xsi metadata.
type HtmlEvent struct{Kind,Name string;Attributes []HtmlAttribute;Text string}
// HtmlSupportedSubset reports the exact boundary; browser recovery and URL sanitization are outside this contract.
func HtmlSupportedSubset()string{return "UTF-8 HTML fragments; explicit balanced approved flow/phrasing/list tags; br/hr/img void tags; ASCII case-insensitive names; quoted/unquoted and hidden Boolean attributes; amp/lt/gt/quot/apos/nbsp and numeric references; comments omitted; no doctype, implied closes, raw text, foreign content or browser recovery"}
// Reject negative or ambiguous operation policies before reading input or traversing authored events.
func srlHtmlPolicy(policies []HtmlLimits)(HtmlLimits,error){limits:=DefaultHtmlLimits();if len(policies)>1{return limits,fmt.Errorf("Ambiguous HTML limits")};if len(policies)==1{limits=policies[0]};if limits.MaxInputBytes<0||limits.MaxOutputBytes<0||limits.MaxEvents<0||limits.MaxDepth<0||limits.MaxAttributesPerElement<0||limits.MaxTextBytes<0{return limits,fmt.Errorf("Negative HTML limit")};return limits,nil}
var srlHtmlTags=map[string]bool{"div":true,"p":true,"span":true,"b":true,"i":true,"strong":true,"em":true,"a":true,"ul":true,"ol":true,"li":true,"section":true,"article":true,"h1":true,"h2":true,"h3":true,"h4":true,"h5":true,"h6":true,"br":true,"hr":true,"img":true,"small":true,"s":true,"u":true,"code":true,"blockquote":true,"nav":true}
var srlHtmlVoids=map[string]bool{"br":true,"hr":true,"img":true}
var srlHtmlPhrasing=map[string]bool{"span":true,"b":true,"i":true,"strong":true,"em":true,"a":true,"br":true,"img":true,"small":true,"s":true,"u":true,"code":true}
const srlHtmlSpace=" \t\r\n\f"
// Require valid Unicode scalars and shared HTML whitespace before encoding can substitute characters.
func srlHtmlText(value string)error{if !utf8.ValidString(value){return fmt.Errorf("Invalid HTML UTF8")};for _,scalar:=range value{if scalar==0||scalar>=0x7f&&scalar<=0x9f||scalar<32&&scalar!=9&&scalar!=10&&scalar!=13&&scalar!=12{return fmt.Errorf("Unsupported HTML character")}};return nil}
// Decode only approved terminated references; raw CR normalization leaves numeric CR values intact.
func srlHtmlDecode(input string)(string,error){
  value:=strings.ReplaceAll(strings.ReplaceAll(input,"\r\n","\n"),"\r","\n");var result strings.Builder
  for position:=0;position<len(value);{if value[position]!='&'{result.WriteByte(value[position]);position++;continue};offset:=strings.IndexByte(value[position+1:],';');if offset<0||offset+1>12{return "",fmt.Errorf("HTML references require supported terminated spelling")};end:=position+offset+1;entity:=value[position+1:end];decoded:=""
    switch entity{case "amp":decoded="&";case "lt":decoded="<";case "gt":decoded=">";case "quot":decoded="\"";case "apos":decoded="'";case "nbsp":decoded="\u00a0";default:
      if !strings.HasPrefix(entity,"#"){return "",fmt.Errorf("Unsupported HTML entity")};hex:=len(entity)>1&&(entity[1]=='x'||entity[1]=='X');start,base:=1,10;if hex{start,base=2,16};if start==len(entity){return "",fmt.Errorf("Empty HTML numeric reference")};scalar:=0;for index:=start;index<len(entity);index++{item:=entity[index];digit:=-1;if item>='0'&&item<='9'{digit=int(item-'0')}else if hex&&item>='a'&&item<='f'{digit=int(item-'a')+10}else if hex&&item>='A'&&item<='F'{digit=int(item-'A')+10};if digit<0||scalar>(utf8.MaxRune-digit)/base{return "",fmt.Errorf("Unsupported HTML numeric reference")};scalar=scalar*base+digit};if scalar>=0xd800&&scalar<=0xdfff{return "",fmt.Errorf("Unsupported HTML character reference")};decoded=string(rune(scalar))
    };if err:=srlHtmlText(decoded);err!=nil{return "",err};result.WriteString(decoded);position=end+1
  };text:=result.String();return text,srlHtmlText(text)
}
// Reject nesting that independent HTML parsers would implicitly close or rearrange.
func srlHtmlChild(open []string,child string)error{if !srlHtmlTags[child]{return fmt.Errorf("HTML tag outside approved subset")};parent:="";if len(open)!=0{parent=open[len(open)-1]};if child=="li"&&parent!="ul"&&parent!="ol"{return fmt.Errorf("HTML list item requires direct list parent")};if srlHtmlVoids[parent]||(parent=="ul"||parent=="ol")&&child!="li"||(srlHtmlPhrasing[parent]||parent=="p"||len(parent)==2&&parent[0]=='h'&&parent[1]>='1'&&parent[1]<='6')&&!srlHtmlPhrasing[child]{return fmt.Errorf("HTML child requires unsupported browser recovery")};if child=="a"{for _,ancestor:=range open{if ancestor=="a"{return fmt.Errorf("Nested HTML anchor outside approved subset")}}};return nil}
// Approve attribute identities without interpreting application values or URL schemes.
func srlHtmlAttribute(tag,key string)bool{switch key{case "id","class","title","lang","dir","hidden":return true};return tag=="a"&&(key=="href"||key=="rel")||tag=="img"&&(key=="src"||key=="alt"||key=="width"||key=="height")}
type srlHtmlOutput struct{limits HtmlLimits;result strings.Builder;bytes int}
// Charge UTF8 length before retaining another owned output fragment.
func (w *srlHtmlOutput) append(value string)error{if len(value)>w.limits.MaxOutputBytes-w.bytes{return fmt.Errorf("HTML output byte limit")};w.bytes+=len(value);w.result.WriteString(value);return nil}
// Escape syntax and raw CR in bounded Unicode-complete batches.
func (w *srlHtmlOutput) escape(value string,attribute bool)error{if err:=srlHtmlText(value);err!=nil{return err};var batch strings.Builder;for _,scalar:=range value{switch scalar{case '&':batch.WriteString("&amp;");case '<':batch.WriteString("&lt;");case '>':batch.WriteString("&gt;");case '\r':batch.WriteString("&#13;");case '"':if attribute{batch.WriteString("&quot;")}else{batch.WriteRune(scalar)};default:batch.WriteRune(scalar)};if batch.Len()>=4096{if err:=w.append(batch.String());err!=nil{return err};batch.Reset()}};if batch.Len()!=0{return w.append(batch.String())};return nil}
// HtmlWriteFragment validates the complete owning trace before publishing deterministic ordinary HTML UTF8.
func HtmlWriteFragment(events []HtmlEvent,policies ...HtmlLimits)([]byte,error){
  limits,err:=srlHtmlPolicy(policies);if err!=nil{return nil,err};if len(events)>limits.MaxEvents||len(events)<2||events[0].Kind!="start_fragment"||events[len(events)-1].Kind!="end_fragment"{return nil,fmt.Errorf("HTML requires bounded explicit fragment boundaries")};output:=srlHtmlOutput{limits:limits};open:=[]string{};textBytes:=0
  for index,event:=range events{switch event.Kind{
    case "start_fragment","end_fragment":if index!=0&&index!=len(events)-1||len(open)!=0||event.Name!=""||len(event.Attributes)!=0||event.Text!=""{return nil,fmt.Errorf("Malformed HTML fragment boundary")}
    case "start_element":if err=srlHtmlChild(open,event.Name);err!=nil{return nil,err};if event.Text!=""||len(open)>=limits.MaxDepth||len(event.Attributes)>limits.MaxAttributesPerElement{return nil,fmt.Errorf("HTML element payload/depth/attribute limit")};if err=output.append("<"+event.Name);err!=nil{return nil,err};names:=make(map[string]bool);for _,item:=range event.Attributes{key:=item.Name;if !srlHtmlAttribute(event.Name,key)||names[key]||item.Boolean!=(key=="hidden")||item.Boolean&&item.Value!=""{return nil,fmt.Errorf("Unsupported or duplicate HTML attribute")};if err=srlHtmlText(item.Value);err!=nil{return nil,err};names[key]=true;if err=output.append(" "+key);err!=nil{return nil,err};if !item.Boolean{if err=output.append("=\"");err!=nil{return nil,err};if err=output.escape(item.Value,true);err!=nil{return nil,err};if err=output.append("\"");err!=nil{return nil,err}}};if err=output.append(">");err!=nil{return nil,err};open=append(open,event.Name)
    case "end_element":if len(open)==0||open[len(open)-1]!=event.Name||len(event.Attributes)!=0||event.Text!=""{return nil,fmt.Errorf("Unbalanced HTML close")};if !srlHtmlVoids[event.Name]{if err=output.append("</"+event.Name+">");err!=nil{return nil,err}};open=open[:len(open)-1]
    case "text":parent:="";if len(open)!=0{parent=open[len(open)-1]};if event.Name!=""||len(event.Attributes)!=0||srlHtmlVoids[parent]||len(event.Text)>limits.MaxTextBytes-textBytes{return nil,fmt.Errorf("Invalid HTML text event or cumulative byte limit")};if parent=="ul"||parent=="ol"{for _,item:=range event.Text{if !strings.ContainsRune(srlHtmlSpace,item){return nil,fmt.Errorf("HTML list text requires browser recovery")}}};textBytes+=len(event.Text);if err=output.escape(event.Text,false);err!=nil{return nil,err}
    default:return nil,fmt.Errorf("Unknown HTML event kind")
  }};return []byte(output.result.String()),nil
}
)htmlgo"
R"htmlgo(
type srlHtmlParser struct{source string;limits HtmlLimits;events []HtmlEvent;open []string;position,textBytes int}
// Admit every event/text run before retaining another owning payload in the candidate trace.
func (r *srlHtmlParser) add(event HtmlEvent)error{if len(r.events)>=r.limits.MaxEvents{return fmt.Errorf("HTML event count limit")};if event.Kind=="text"{if len(event.Text)>r.limits.MaxTextBytes-r.textBytes{return fmt.Errorf("HTML text byte limit")};r.textBytes+=len(event.Text)};r.events=append(r.events,event);return nil}
// Skip only shared HTML whitespace, including form feed.
func (r *srlHtmlParser) space(offset int)int{for offset<len(r.source)&&strings.IndexByte(srlHtmlSpace,r.source[offset])>=0{offset++};return offset}
// Scan ASCII tag/attribute names without locale-dependent Unicode folding.
func (r *srlHtmlParser) name(offset int)(string,error){end:=offset;for end<len(r.source){value:=r.source[end];if !(value>='A'&&value<='Z'||value>='a'&&value<='z'||value>='0'&&value<='9'||value=='-'){break};end++};if end==offset{return "",fmt.Errorf("Unsupported HTML name syntax")};return r.source[offset:end],nil}
// Observe a byte without indexing truncated source.
func (r *srlHtmlParser) current()byte{if r.position<len(r.source){return r.source[r.position]};return 0}
// Parse explicit nesting and require checked complete EOF before publishing an owning trace.
func (r *srlHtmlParser) parse()([]HtmlEvent,error){
  if err:=r.add(HtmlEvent{Kind:"start_fragment"});err!=nil{return nil,err}
  for r.position<len(r.source){
    if r.current()!='<'{end:=strings.IndexByte(r.source[r.position:],'<');if end<0{end=len(r.source)}else{end+=r.position};value,err:=srlHtmlDecode(r.source[r.position:end]);if err!=nil{return nil,err};if err=r.add(HtmlEvent{Kind:"text",Text:value});err!=nil{return nil,err};r.position=end;continue}
    if strings.HasPrefix(r.source[r.position:],"<!--"){end:=strings.Index(r.source[r.position+4:],"-->");if end<0{return nil,fmt.Errorf("Malformed HTML comment")};end+=r.position+4;if strings.Contains(r.source[r.position+4:end],"--"){return nil,fmt.Errorf("Malformed HTML comment")};r.position=end+3;continue}
    r.position++;closing:=r.current()=='/';if closing{r.position++};original,err:=r.name(r.position);if err!=nil{return nil,err};tag:=strings.ToLower(original);r.position+=len(original)
    if closing{r.position=r.space(r.position);if r.current()!='>'||srlHtmlVoids[tag]||len(r.open)==0||r.open[len(r.open)-1]!=tag{return nil,fmt.Errorf("HTML close requires matching explicit nonvoid start")};r.position++;r.open=r.open[:len(r.open)-1];if err=r.add(HtmlEvent{Kind:"end_element",Name:tag});err!=nil{return nil,err};continue}
    if err=srlHtmlChild(r.open,tag);err!=nil{return nil,err};if len(r.open)>=r.limits.MaxDepth{return nil,fmt.Errorf("HTML nesting limit")};attributes:=[]HtmlAttribute{};selfClosing:=false
    for{separator:=r.position;r.position=r.space(r.position);if r.position>=len(r.source){return nil,fmt.Errorf("Truncated HTML start tag")};if r.current()=='>'{r.position++;break};if strings.HasPrefix(r.source[r.position:],"/>"){selfClosing=true;r.position+=2;break};if separator==r.position||len(attributes)>=r.limits.MaxAttributesPerElement{return nil,fmt.Errorf("HTML attributes require separation and bounded cardinality")};originalKey,err:=r.name(r.position);if err!=nil{return nil,err};key:=strings.ToLower(originalKey);r.position+=len(originalKey);keyEnd:=r.position;r.position=r.space(r.position);assigned:=r.current()=='=';value:=""
      if assigned{r.position=r.space(r.position+1);if r.position>=len(r.source){return nil,fmt.Errorf("Truncated HTML attribute")};quote:=byte(0);if r.current()=='"'||r.current()=='\''{quote=r.current();r.position++};end:=r.position;if quote!=0{found:=strings.IndexByte(r.source[r.position:],quote);if found<0{return nil,fmt.Errorf("Unterminated HTML attribute")};end+=found}else{for end<len(r.source)&&strings.IndexByte(srlHtmlSpace,r.source[end])<0&&r.source[end]!='>'{if strings.IndexByte("\"'< =`",r.source[end])>=0{return nil,fmt.Errorf("Invalid unquoted HTML attribute")};end++};if end==r.position{return nil,fmt.Errorf("Empty unquoted HTML attribute")}};value,err=srlHtmlDecode(r.source[r.position:end]);if err!=nil{return nil,err};r.position=end;if quote!=0{r.position++}}else{r.position=keyEnd}
      if key=="hidden"{if assigned&&value!=""&&!strings.EqualFold(value,"hidden"){return nil,fmt.Errorf("HTML hidden requires Boolean spelling")};value=""}else if !assigned{return nil,fmt.Errorf("Only hidden may omit HTML attribute value")};attributes=append(attributes,HtmlAttribute{key,value,key=="hidden"})
    };if selfClosing&&!srlHtmlVoids[tag]{return nil,fmt.Errorf("Self-closing nonvoid HTML requires browser recovery")};if err=r.add(HtmlEvent{Kind:"start_element",Name:tag,Attributes:attributes});err!=nil{return nil,err};if srlHtmlVoids[tag]{if err=r.add(HtmlEvent{Kind:"end_element",Name:tag});err!=nil{return nil,err}}else{r.open=append(r.open,tag)}
  };if len(r.open)!=0{return nil,fmt.Errorf("Unclosed HTML fragment")};if err:=r.add(HtmlEvent{Kind:"end_fragment"});err!=nil{return nil,err};if _,err:=HtmlWriteFragment(r.events,r.limits);err!=nil{return nil,err};return r.events,nil
}
// HtmlParseFragment fatal-decodes ordinary UTF8 and publishes only a checked balanced approved trace.
func HtmlParseFragment(input []byte,policies ...HtmlLimits)([]HtmlEvent,error){limits,err:=srlHtmlPolicy(policies);if err!=nil{return nil,err};if len(input)>limits.MaxInputBytes{return nil,fmt.Errorf("HTML input byte limit")};source:=string(input);if err=srlHtmlText(source);err!=nil{return nil,err};parser:=srlHtmlParser{source:source,limits:limits};return parser.parse()}
)htmlgo"

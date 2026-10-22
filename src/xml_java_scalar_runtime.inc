// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"xmlscalar(
  private final String kind, whitespace;
  private final int bits;
  private final String[] bounds, enumeration;
  private final long length, minimum, maximum;
  private final boolean finite;
  private final Pattern[][] groups;
  private final String listKind, fixed;
  private final SrlXmlRule[] members;
  private String[][] qnames, fixedNames, defaultNames;
  private String[][][] qnameLists;
  private String defaultText;
  private boolean defaultAttribute;
  private UnionValue[] unionValues;
  private UnionValue unionFixed;
  private SrlXmlRule listItem;
  /** Prepare one complete atomic or union list item rule without changing builtin list checks. */
  SrlXmlRule listItem(SrlXmlRule item){listItem=item;return this;}
  /** Publish recursively prepared selected list/union constants. */
  SrlXmlRule unionValues(UnionValue[] values){unionValues=values;return this;}
  /** Publish a recursively prepared fixed scalar/list identity. */
  SrlXmlRule unionFixed(UnionValue value){unionFixed=value;return this;}
  private java.util.Map<String,String> defaultScope=java.util.Map.of();
  /** Bind schema-owned expanded identities before publishing this immutable prepared field rule. */
  SrlXmlRule namespaceValues(String[][] names,String[][][] lists){qnames=names;qnameLists=lists;return this;}
  /** Prepare immutable selected-member identities using each literal's original schema scope. */
  SrlXmlRule unionValues(String[][] values,String[][][] names){unionValues=new UnionValue[values.length];for(int index=0;index<values.length;++index)unionValues[index]=new UnionValue(values[index],names[index]);return this;}
  /** Bind a fixed union value independently of the instance's unrelated namespace aliases. */
  SrlXmlRule unionFixed(String[] value,String[][] names){unionFixed=new UnionValue(value,names);return this;}
  /** Retain only the selected primitive and expanded names, without any provider tree or scope map. */
  private static final class UnionValue {
    final SrlXmlRule rule;final String text;final String[][] names;final UnionValue[] items;
    /** Complete trusted static preparation before publishing the generated rule. */
    UnionValue(String[] value,String[][] names){rule=new SrlXmlRule(value[0],"preserve",0,new String[]{"","","",""},-1,-1,-1,new String[0],false,new Pattern[0][],value[1],new SrlXmlRule[0],null);text=value[2];this.names=names;items=new UnionValue[0];}
    /** Retain ordered primitive identities for a list literal including heterogeneous union items. */
    UnionValue(String[] value,String[][] names,UnionValue[] items){rule=new SrlXmlRule(value[0],"preserve",0,new String[]{"","","",""},-1,-1,-1,new String[0],false,new Pattern[0][],value[1],new SrlXmlRule[0],null);text=value[2];this.names=names;this.items=items;}
  }
  /** Preserve prefix spellings observed by a pattern or lexical prefix-list enumeration. */
  SrlXmlRule namespaceLexical(){keepNamespaceLexical=true;return this;}
  private boolean keepNamespaceLexical;
  /** Retain only schema lexical aliases needed to materialize an absent attribute or empty element. */
  SrlXmlRule namespaceDefault(String text,String[][] bindings,String[][] names,boolean fixedValue,boolean attribute,boolean content){defaultContent=content;defaultText=text;defaultNames=names;defaultAttribute=attribute;fixedNames=fixedValue?names:null;var scope=new java.util.LinkedHashMap<String,String>();for(var binding:bindings)scope.put(binding[0],binding[1]);defaultScope=java.util.Map.copyOf(scope);return this;}
  private boolean defaultContent;
  /** Reuse an empty element only when the exact native schema literal retains all used bindings. */
  boolean matchesSchemaDefault(String text,java.util.Map<String,String> scope){if(!(defaultContent||defaultAttribute)||!text.equals(defaultText))return false;for(var binding:defaultScope.entrySet())if(!scope.getOrDefault(binding.getKey(),"").equals(binding.getValue()))return false;return true;}
  /** Identify the selected leaf without interpreting an ordinary string union member as a QName. */
  int namespaceMode(){return listKind.equals("QName")?1:kind.equals("QName")?0:-1;}
  /** Omit an optional default only when its complete expanded value is schema-equivalent. */
  boolean matchesNamespaceDefault(String text,java.util.Map<String,String> scope,int maximum,SrlXmlBudget budget){if(!defaultAttribute||defaultNames==null)return false;var work=new Work(MAX_WORK_UNITS,maximum,budget);work.charge("allocation",128L+text.length()*4L);String[] tokens=tokens(text,maximum);String[][] names=new String[tokens.length][];boolean empty=false;for(int index=0;index<tokens.length;++index){names[index]=SrlXmlNamespaceValues.resolve(tokens[index],namespaceMode(),scope);empty|=names[index][1].isEmpty();}return empty&&equalNames(names,defaultNames,work);}
  /** Retain compiler-owned immutable scalar metadata once per field. */
  SrlXmlRule(String kind, String whitespace, int bits, String[] bounds, long length,
      long minimum, long maximum, String[] enumeration, boolean finite, Pattern[][] groups,
      String listKind, SrlXmlRule[] members, String fixed) {
    this.kind = kind; this.whitespace = whitespace; this.bits = bits; this.bounds = bounds;
    this.length = length; this.minimum = minimum; this.maximum = maximum;
    this.enumeration = enumeration; this.finite = finite; this.groups = groups;
    this.listKind = listKind; this.members = members; this.fixed = fixed;
  }
  /** Normalize XML whitespace without changing other Unicode separator characters. */
  private String normalize(String text) {
    if (!whitespace.equals("replace") && !whitespace.equals("collapse")) return text;
    text = text.replace('\t', ' ').replace('\r', ' ').replace('\n', ' ');
    if (!whitespace.equals("collapse")) return text;
    var output = new StringBuilder(); boolean pending = false;
    for (int index = 0; index < text.length(); ++index) {
      char value = text.charAt(index);
      if (value == ' ') { pending = output.length() != 0; continue; }
      if (pending) output.append(' '); output.append(value); pending = false;
    }
    return output.toString();
  }
  /** Compare exact value spaces; null denotes an unordered NaN comparison. */
  private Integer compare(String left, String right) {
    if (!listKind.isEmpty()) {
      String[] first = tokens(left, Integer.MAX_VALUE), second = tokens(right, Integer.MAX_VALUE);
      if (first.length != second.length) return 1;
      var item = itemRule(listKind);
      for (int index = 0; index < first.length; ++index) if (!Integer.valueOf(0).equals(item.compare(first[index], second[index]))) return 1;
      return 0;
    }
    if (kind.equals("float") || kind.equals("double")) {
      double first = srlXmlFloat(left, kind.equals("float")), second = srlXmlFloat(right, kind.equals("float"));
      if (Double.isNaN(first) || Double.isNaN(second)) return Double.isNaN(first) && Double.isNaN(second) ? Integer.valueOf(0) : null;
      return first < second ? -1 : first > second ? 1 : 0;
    }
    if (kind.startsWith("unsigned") || kind.equals("byte") || kind.equals("short") || kind.equals("int") || kind.equals("long")) {
      int width = integerWidth(kind);
      srlXmlInteger(left, width, kind.startsWith("unsigned")); srlXmlInteger(right, width, kind.startsWith("unsigned"));
      return new java.math.BigInteger(srlXmlTrim(left)).compareTo(new java.math.BigInteger(srlXmlTrim(right)));
    }
    if (kind.equals("boolean")) return srlXmlBoolean(left) == srlXmlBoolean(right) ? 0 : 1;
    if (kind.equals("decimal") || integerKind(kind)) return new java.math.BigDecimal(left).compareTo(new java.math.BigDecimal(right));
    return left.compareTo(right);
  }
  /** Split only the four XML whitespace characters; an empty lexical list has no items. */
  static String[] tokens(String text) { return tokens(text, MAX_LIST_ITEMS); }
  /** Admit the complete item count before split allocates its token array and item strings. */
  static String[] tokens(String text, int maximum) {
    int count = 0; boolean item = false;
    for (int index = 0; index < text.length(); ++index) {
      char value = text.charAt(index); boolean space = value == ' ' || value == '\t' || value == '\r' || value == '\n';
      if (!space && !item) { if (count >= maximum) throw new LimitFailure("XML list item limit exceeded"); ++count; }
      item = !space;
    }
    text = srlXmlTrim(text); return count == 0 ? new String[0] : text.split("[ \\t\\r\\n]+");
  }
  /** Materialize XSD defaults without treating a present empty attribute as missing. */
  static String defaultText(String text, String fallback, boolean attribute) { return text == null || !attribute && text.isEmpty() ? fallback : text; }
  /** Build one immutable unrestricted builtin list-item contract. */
  private static SrlXmlRule itemRule(String kind) { return new SrlXmlRule(kind, "collapse", 0, new String[]{"", "", "", ""}, -1, -1, -1, new String[0], false, new Pattern[0][], "", new SrlXmlRule[0], null); }
  /** Derive each builtin integer's range from its XML type, independently of native string storage. */
  private static int integerWidth(String kind) {
    return switch (kind) { case "byte", "unsignedByte" -> 8; case "short", "unsignedShort" -> 16; case "int", "unsignedInt" -> 32; case "long", "unsignedLong" -> 64; default -> 0; };
  }
  /** Recognize the unbounded signed integer family. */
  private static boolean integerKind(String kind) { return kind.equals("integer") || kind.equals("positiveInteger") || kind.equals("negativeInteger") || kind.equals("nonPositiveInteger") || kind.equals("nonNegativeInteger"); }
  /** Apply XML1.0 NameStartChar and NameChar ranges to Unicode scalars. */
  private static boolean nameCharacter(int value, boolean first, boolean colon) {
    boolean start = value == '_' || colon && value == ':' || value >= 'A' && value <= 'Z' || value >= 'a' && value <= 'z' || value >= 0xc0 && value <= 0xd6 || value >= 0xd8 && value <= 0xf6 || value >= 0xf8 && value <= 0x2ff || value >= 0x370 && value <= 0x37d || value >= 0x37f && value <= 0x1fff || value >= 0x200c && value <= 0x200d || value >= 0x2070 && value <= 0x218f || value >= 0x2c00 && value <= 0x2fef || value >= 0x3001 && value <= 0xd7ff || value >= 0xf900 && value <= 0xfdcf || value >= 0xfdf0 && value <= 0xfffd || value >= 0x10000 && value <= 0xeffff;
    return start || !first && (value == '-' || value == '.' || value >= '0' && value <= '9' || value == 0xb7 || value >= 0x300 && value <= 0x36f || value >= 0x203f && value <= 0x2040);
  }
  /** Check builtin lexical and value-space rules before facets can compare candidates. */
  private void lexical(String text) {
    int width = integerWidth(kind);
    if (width != 0) { srlXmlInteger(text, width, kind.startsWith("unsigned")); return; }
    if (integerKind(kind)) {
      if (!text.matches("[+-]?[0-9]+")) throw new IllegalArgumentException("Invalid XML integer");
      int sign = new java.math.BigInteger(text).signum();
      if (kind.equals("positiveInteger") && sign <= 0 || kind.equals("negativeInteger") && sign >= 0 || kind.equals("nonPositiveInteger") && sign > 0 || kind.equals("nonNegativeInteger") && sign < 0) throw new IllegalArgumentException("XML integer sign failed");
    } else if (kind.equals("decimal")) {
      if (!text.matches("[+-]?(?:[0-9]+(?:\\.[0-9]*)?|\\.[0-9]+)")) throw new IllegalArgumentException("Invalid XML decimal");
    } else if (kind.equals("boolean")) srlXmlBoolean(text);
    else if (kind.equals("float") || kind.equals("double")) srlXmlFloat(text, kind.equals("float"));
    else if (kind.equals("language")) { if (!text.matches("[A-Za-z]{1,8}(?:-[A-Za-z0-9]{1,8})*")) throw new IllegalArgumentException("Invalid XML language"); }
    else if (kind.equals("Name") || kind.equals("NCName") || kind.equals("NMTOKEN") || kind.equals("ID") || kind.equals("IDREF") || kind.equals("ENTITY")) {
      if (text.isEmpty()) throw new IllegalArgumentException("Empty XML name token");
      for (int offset = 0; offset < text.length();) { int value = text.codePointAt(offset); boolean first = offset == 0 && !kind.equals("NMTOKEN"); offset += Character.charCount(value); if (!nameCharacter(value, first, kind.equals("Name") || kind.equals("NMTOKEN"))) throw new IllegalArgumentException("Invalid XML name token"); }
    }
  }
  /** Apply the same normalization and facets before input conversion or output publication. */
  String check(String text) {
    return check(text, MAX_LIST_ITEMS);
  }
)xmlscalar"
R"xmlscalar(  /** Carry the operation's native collection admission limit through every union/list branch. */
  String check(String text, int maximumItems) {
    if (text == null) throw new IllegalArgumentException("Null XML scalar value");
    return check(text,maximumItems,null);
  }
  /** Share native scalar and Thompson execution with the enclosing package ledger. */
  String check(String text,int maximumItems,SrlXmlBudget budget){
    String checked=check(text, new Work(MAX_WORK_UNITS, maximumItems,budget));
    return members.length==0?checked:text;
  }
  /** Preserve present normalized empty tokens without activating an element's nonempty default. */
  String output(String text, String fallback, int maximumItems) {
    return output(text,fallback,maximumItems,null);
  }
  /** Preserve default semantics while charging every scalar branch to the caller operation. */
  String output(String text,String fallback,int maximumItems,SrlXmlBudget budget){
    text = check(text, maximumItems,budget);
    if (text.isEmpty() && !normalize(fallback).isEmpty()) {
      if (whitespace.equals("collapse")) return " ";
      throw new IllegalArgumentException("Empty XML value would activate a different XSD default");
    }
    return text;
  }
  /** Share work across recursive union alternatives; resource failures cannot select another branch. */
  private String check(String text, Work work) {
    work.spend(text.length()); if(members.length==0){text = normalize(text); lexical(text);}
    SrlXmlRule selected=this;
    if (members.length != 0) {
      boolean matched = false;
      for (SrlXmlRule member : members) {
        try { text=member.check(text, work); selected=work.selected;matched = true; break; }
        catch (LimitFailure failure) { throw failure; }
        catch (IllegalArgumentException rejected) { if(work.callbackFailure!=null)throw work.callbackFailure; /* A different complete union branch may match. */ }
      }
      if (!matched) throw new IllegalArgumentException("XML union has no matching member");
    }
    long count = text.codePointCount(0, text.length());
    if (!listKind.isEmpty()) { String[] items = tokens(text, work.maximumItems); count = items.length; var item = listItem!=null?listItem:itemRule(listKind); for (String value : items) item.check(value, work); }
    if (length >= 0 && count != length || minimum >= 0 && count < minimum || maximum >= 0 && count > maximum) throw new IllegalArgumentException("XML length facet failed");
    for (int index = 0; index < bounds.length; ++index) {
      if (bounds[index].isEmpty()) continue;
      Integer order = compare(text, bounds[index]);
      if (order == null || index == 0 && order < 0 || index == 1 && order > 0 || index == 2 && order <= 0 || index == 3 && order >= 0) throw new IllegalArgumentException("XML numeric facet failed");
    }
    if (enumeration.length != 0 && (qnames==null||qnames.length==0) && (qnameLists==null||qnameLists.length==0)) {
      boolean matched = false;
      for (int index=0;index<enumeration.length;++index) {String allowed=enumeration[index];if(unionValues!=null?constantEqual(text,selected,unionValues[index],work):members.length!=0?unionEqual(text,selected,allowed,work):Integer.valueOf(0).equals(compare(text,normalize(allowed)))) { matched = true; break; } }
      if (!matched) throw new IllegalArgumentException("XML enumeration facet failed");
    }
    if (fixed != null && fixedNames==null && !(unionFixed!=null?constantEqual(text,selected,unionFixed,work):members.length!=0?unionEqual(text,selected,fixed,work):Integer.valueOf(0).equals(compare(text,normalize(fixed))))) throw new IllegalArgumentException("XML fixed value differs");
    if(work.scope!=null&&namespaceMode()>=0)checkNames(text,work);
    if (finite && !Double.isFinite(srlXmlFloat(text, kind.equals("float")))) throw new IllegalArgumentException("XML finite facet failed");
    if (groups.length != 0) {
      for (Pattern[] group : groups) {
        boolean matched = false;
        for (Pattern pattern : group) { if (pattern.matches(text, work)) { matched = true; break; } }
        if (!matched) throw new IllegalArgumentException("XML portable pattern facet failed");
      }
    }
    work.selected=selected;return text;
  }
  /** Validate a complete scalar in its original scope, retaining the first successful leaf. */
  Work checkNamespace(String text,java.util.Map<String,String> scope,int maximum,SrlXmlBudget budget){var work=new Work(MAX_WORK_UNITS,maximum,budget);work.scope=scope;String checked=check(text,work);work.checked=members.length==0?checked:text;return work;}
  /** Keep primitive value families distinct when different union alternatives accept the same spelling. */
  private String valueFamily(){if(!listKind.isEmpty())return "list:"+itemRule(listKind).valueFamily();if(integerWidth(kind)!=0||integerKind(kind)||kind.equals("decimal"))return "decimal";if(kind.equals("boolean")||kind.equals("float")||kind.equals("double")||kind.equals("anyURI")||kind.equals("QName"))return kind;return "string";}
  /** Compare normalized leaf values without imposing the other alternative's native integer width. */
  private boolean identityEqual(String first,SrlXmlRule other,String second,Work work){
    work.spend(first.length()+second.length()+1);if(!valueFamily().equals(other.valueFamily()))return false;
    if(!listKind.isEmpty()){String[] left=tokens(first,work.maximumItems),right=tokens(second,work.maximumItems);if(left.length!=right.length)return false;var item=itemRule(listKind);var otherItem=itemRule(other.listKind);for(int index=0;index<left.length;++index)if(!item.identityEqual(left[index],otherItem,right[index],work))return false;return true;}
    if(valueFamily().equals("decimal"))return new java.math.BigDecimal(first).compareTo(new java.math.BigDecimal(second))==0;
    return Integer.valueOf(0).equals(compare(first,second));
  }
  /** Select a schema literal through the same ordered member contracts as the instance. */
  private boolean unionEqual(String text,SrlXmlRule selected,String allowed,Work work){
    for(var member:members){String expected;try{expected=member.check(allowed,work);}catch(LimitFailure failure){throw failure;}catch(IllegalArgumentException rejected){if(work.callbackFailure!=null)throw work.callbackFailure;continue;}return selected.identityEqual(text,work.selected,expected,work);}return false;
  }
  /** Compare expanded names only after preserving the selected primitive family and ordered list shape. */
  private boolean constantEqual(String text,SrlXmlRule selected,UnionValue expected,Work work){
    if(selected.listKind.isEmpty()!=expected.rule.listKind.isEmpty())return false;
    if(!selected.listKind.isEmpty()&&(expected.items.length!=0||expected.text.isEmpty())){
      String[] values=tokens(text,work.maximumItems);if(values.length!=expected.items.length)return false;
      var item=selected.listItem!=null?selected.listItem:itemRule(selected.listKind);
      for(int index=0;index<values.length;++index){String value=item.check(values[index],work);if(!constantEqual(value,work.selected,expected.items[index],work))return false;}return true;
    }
    if(!selected.valueFamily().equals(expected.rule.valueFamily()))return false;
    if(selected.namespaceMode()<0)return selected.identityEqual(text,expected.rule,expected.text,work);
    if(work.scope==null)throw new IllegalArgumentException("Missing XML union namespace scope");
    String[] tokens=tokens(text,work.maximumItems);work.charge("allocation",tokens.length*64L+text.length()*4L);
    String[][] names=new String[tokens.length][];for(int index=0;index<tokens.length;++index)names[index]=SrlXmlNamespaceValues.resolve(tokens[index],selected.namespaceMode(),work.scope);
    return equalNames(names,expected.names,work);
  }
  /** Admit temporary identities before resolving aliases and comparing schema-owned values. */
  private void checkNames(String text,Work work){
    final int identityEntryBytes=64;int count=0;boolean inside=false;
    for(int index=0;index<text.length();++index){char character=text.charAt(index);boolean space=character==' '||character=='\t'||character=='\r'||character=='\n';if(!space&&!inside)++count;inside=!space;}
    work.charge("allocation",(long)identityEntryBytes*count+text.length()*4L);
    String[] tokens=tokens(text,work.maximumItems);int mode=namespaceMode();if(mode==0&&tokens.length!=1)throw new IllegalArgumentException("XML QName requires one name");
    String[][] names=new String[tokens.length][];for(int index=0;index<tokens.length;++index)names[index]=SrlXmlNamespaceValues.resolve(tokens[index],mode,work.scope);
    if(fixedNames!=null&&!equalNames(names,fixedNames,work))throw new IllegalArgumentException("XML fixed QName value differs");
    if(qnames!=null&&qnames.length!=0){for(var allowed:qnames)if(equalNames(names,new String[][]{allowed},work))return;throw new IllegalArgumentException("XML QName enumeration facet failed");}
    if(qnameLists!=null&&qnameLists.length!=0){for(var allowed:qnameLists)if(equalNames(names,allowed,work))return;throw new IllegalArgumentException("XML QName enumeration facet failed");}
  }
  /** Compare ordered expanded identities, excluding the source prefix from value equality. */
  private static boolean equalNames(String[][] actual,String[][] expected,Work work){work.spend(1);if(actual.length!=expected.length)return false;for(int index=0;index<actual.length;++index){var name=actual[index];work.spend(name[1].length()+name[2].length()+1);if(!name[1].equals(expected[index][0])||!name[2].equals(expected[index][1]))return false;}return true;}
  /** Preserve bounded evaluation failures across union branch probing. */
  private static final class LimitFailure extends IllegalArgumentException {
    private static final long serialVersionUID = 1L;
    /** Retain the precise resource diagnostic exposed by ordinary pattern checks. */
    LimitFailure(String message) { super(message); }
  }
  private static volatile ThreadLocal<Boolean> packageQuota;
  /** Allocate a thread-local marker only when a package invokes a synchronous native codec. */
  private static synchronized ThreadLocal<Boolean> packageQuotaContext(){if(packageQuota==null)packageQuota=new ThreadLocal<>();return packageQuota;}
  /** Nest package codecs without changing ordinary XML calls on this or other threads. */
  static boolean enterPackageQuota(){var active=packageQuotaContext();boolean previous=Boolean.TRUE.equals(active.get());active.set(Boolean.TRUE);return previous;}
  /** Restore a nested marker or remove this operation's private thread-local entry. */
  static void leavePackageQuota(boolean previous){if(previous)packageQuota.set(Boolean.TRUE);else packageQuota.remove();}
  /** Keep ordinary quota exception classes/messages while tagging package-owned admissions precisely. */
  static IllegalArgumentException quota(String message){var active=packageQuota;return active!=null&&Boolean.TRUE.equals(active.get())?new LimitFailure(message):new IllegalArgumentException(message);}
)xmlscalar"
R"xmlscalar(
  /** Bound all alternatives and inherited restriction groups within one scalar match. */
  private static final class Work {
    private int remaining;
    final int maximumItems;final SrlXmlBudget budget;
    java.util.Map<String,String> scope;SrlXmlRule selected;String checked;
    RuntimeException callbackFailure;
    /** Retain one private finite allowance. */
    Work(int remaining, int maximumItems) {this(remaining,maximumItems,null);}
    /** Borrow only operation-local admission, never mutate static scalar metadata. */
    Work(int remaining,int maximumItems,SrlXmlBudget budget){this.remaining=remaining;this.maximumItems=maximumItems;this.budget=budget;}
    /** Charge before clearing state storage, following edges or inspecting characters. */
    void spend(int units) { charge("work",units);if (units < 0 || units > remaining) throw new LimitFailure("Portable XML regex work limit exceeded"); remaining -= units; }
    /** Preserve the caller's first admission exception across speculative union alternatives. */
    void charge(String resource,long units){try{if(budget!=null)budget.charge(resource,units);}catch(RuntimeException failure){callbackFailure=failure;throw failure;}}
  }
  /** Execute only compiler-lowered portable Thompson instructions, never host regex syntax. */
  static final class Pattern {
    private static final int CONSUMES = 1, INVERTED = 2, DOT = 4, ACCEPTING = 8;
    private final int start;
    private final State[] states;
    /** Own immutable instructions prepared once during generated class initialization. */
    private Pattern(int start, State[] states) { this.start = start; this.states = states; }
    /** Retain one character predicate or epsilon instruction and its checked successors. */
    private static final class State {
      final int[] epsilon, ranges; final int next, flags;
      /** Bind compiler-owned arrays that are never exposed outside the prepared program. */
      State(int[] epsilon, int[] ranges, int next, int flags) { this.epsilon = epsilon; this.ranges = ranges; this.next = next; this.flags = flags; }
    }
    /** Parse one compiler-owned integer list without retaining its source fragments. */
    private static int[] integers(String text) {
      if (text.isEmpty()) return new int[0];
      String[] pieces = text.split(",", -1); int[] result = new int[pieces.length];
      for (int index = 0; index < result.length; ++index) result[index] = Integer.parseInt(pieces[index]);
      return result;
    }
    /** Split bounded literals to avoid JVM constant and initialization method size limits. */
    static Pattern prepare(String[] fragments) {
      String[] records = String.join("", fragments).split(";", -1);
      int count = records.length - 1;
      if (count <= 0 || count > MAX_STATES) throw SrlXmlRule.quota("Portable XML regex state limit exceeded");
      int start = Integer.parseInt(records[0]);
      if (start < 0 || start >= count) throw new IllegalArgumentException("Invalid portable XML regex start");
      State[] states = new State[count];
      for (int index = 0; index < count; ++index) {
        String[] fields = records[index + 1].split("\\|", -1);
        if (fields.length != 4) throw new IllegalArgumentException("Invalid portable XML regex instruction");
        int[] epsilon = integers(fields[0]), ranges = integers(fields[1]);
        int next = Integer.parseInt(fields[2]), flags = Integer.parseInt(fields[3]);
        if ((ranges.length & 1) != 0 || next < 0 || next >= count || flags < 0 || flags > (CONSUMES | INVERTED | DOT | ACCEPTING)) throw new IllegalArgumentException("Invalid portable XML regex instruction");
        for (int target : epsilon) if (target < 0 || target >= count) throw new IllegalArgumentException("Invalid portable XML regex edge");
        for (int range = 0; range < ranges.length; range += 2) if (ranges[range] < 0 || ranges[range] > ranges[range + 1] || ranges[range + 1] > Character.MAX_CODE_POINT) throw new IllegalArgumentException("Invalid portable XML regex range");
        states[index] = new State(epsilon, ranges, next, flags);
      }
      return new Pattern(start, states);
    }
    /** Mark states on insertion so nullable loops cannot overflow the bounded stack. */
    private int closure(int[] selected, int count, boolean[] visited, int[] stack, Work work) {
      work.spend(visited.length); java.util.Arrays.fill(visited, false); int pending = 0, kept = 0;
      for (int index = 0; index < count; ++index) { int target = selected[index]; work.spend(1); if (!visited[target]) { visited[target] = true; stack[pending++] = target; } }
      while (pending != 0) {
        work.spend(1); int current = stack[--pending]; State state = states[current];
        if ((state.flags & (CONSUMES | ACCEPTING)) != 0) selected[kept++] = current;
        for (int target : state.epsilon) { work.spend(1); if (!visited[target]) { visited[target] = true; stack[pending++] = target; } }
      }
      return kept;
    }
    /** Match Unicode scalars with fixed state arrays and a deterministic operation budget. */
    private boolean matches(String text, Work work) {
      int bytes = 0;
      for (int offset = 0; offset < text.length();) {
        work.spend(1); int scalar = text.codePointAt(offset); offset += Character.charCount(scalar);
        if (!(scalar == 9 || scalar == 10 || scalar == 13 || scalar >= 32 && scalar <= 0xd7ff || scalar >= 0xe000 && scalar <= 0xfffd || scalar >= 0x10000 && scalar <= 0x10ffff)) throw new IllegalArgumentException("Invalid XML character");
        int width = scalar < 0x80 ? 1 : scalar < 0x800 ? 2 : scalar < 0x10000 ? 3 : 4;
        if (width > MAX_INPUT_BYTES - bytes) throw new LimitFailure("Portable XML regex input limit exceeded"); bytes += width;
      }
      boolean[] visited = new boolean[states.length]; int[] active = new int[states.length], next = new int[states.length], stack = new int[states.length];
      active[0] = start; int count = closure(active, 1, visited, stack, work);
      for (int offset = 0; offset < text.length();) {
        work.spend(1); int scalar = text.codePointAt(offset); offset += Character.charCount(scalar); int following = 0;
        for (int index = 0; index < count; ++index) {
          work.spend(1); State state = states[active[index]]; if ((state.flags & CONSUMES) == 0) continue;
          boolean matched = false;
          if ((state.flags & DOT) != 0) matched = scalar != '\n' && scalar != '\r';
          else {
            for (int range = 0; range < state.ranges.length; range += 2) { work.spend(1); matched |= scalar >= state.ranges[range] && scalar <= state.ranges[range + 1]; }
            if ((state.flags & INVERTED) != 0) matched = !matched;
          }
          if (matched) next[following++] = state.next;
        }
        count = closure(next, following, visited, stack, work); int[] previous = active; active = next; next = previous;
      }
      for (int index = 0; index < count; ++index) if ((states[active[index]].flags & ACCEPTING) != 0) return true;
      return false;
    }
  }
}
)xmlscalar"

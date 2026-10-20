// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"xmlnamespace(
// Keep lexical namespace presentation outside every ordinary native wire format.
internal class SrlXmlNamespaceState {
    var owner: Map<String,String> = emptyMap()
    val fields = mutableMapOf<String,MutableList<Map<String,String>>>()
    // Retain a bounded source snapshot in field occurrence order before its provider scope closes.
    fun <T> scalar(input: SrlXmlInput, name: SrlXmlName, key: String, read: (String,Map<String,String>) -> T): T {
        require(input.has(name)) { "Unexpected XML element" }
        val scope=input.namespaceValueScope()
        val text=read(input.scalar(name),scope)
        fields.getOrPut(key) { mutableListOf() }.add(scope)
        return text
    }
}
// Own fresh traversal counters so repeated encodes leave the caller's model reusable.
internal class SrlXmlNamespaceOutput(private val state: SrlXmlNamespaceState, preserved: Map<String,String>? = null, operation: SrlXmlBudget.Operation? = null) {
    val owner: Map<String,String>
    private val next = mutableMapOf<String,Int>()
    init {
        if (operation != null) for (scope in listOfNotNull(preserved,state.owner)) for ((prefix,uri) in scope) {
            operation.charge("work",prefix.length+uri.length+1L); operation.charge("allocation",64L+prefix.length*2L+uri.length*2L)
        }
        val merged=preserved?.toMutableMap() ?: mutableMapOf()
        for ((prefix,uri) in state.owner) {
            require(merged[prefix] == null || merged[prefix] == uri) { "Conflicting XML namespace sidecars" }
            merged[prefix]=uri
        }
        owner=merged
    }
    // Recover one scalar's complete original scope; new values inherit the owner's bound vocabulary.
    fun scope(key: String): Map<String,String> {
        val index=next[key] ?: 0; next[key]=index+1
        return state.fields[key]?.getOrNull(index) ?: owner
    }
}
// Validate plain QName/QName-list/prefix-list values in the exact active source or output scope.
internal fun srlXmlNamespaceChecked(text: String, list: Boolean, prefixes: Boolean, namespace: (String) -> String?, maximum: Int): String {
    val values=if (list || prefixes) SrlXmlRule.tokens(text,maximum) else listOf(text)
    for (value in values) {
        if (prefixes) {
            require(srlXmlNcName(value) && value != "xmlns" && (value == "xml" || !namespace(value).isNullOrEmpty())) { "Unbound XML prefix-list token" }
        } else { srlXmlTypeQName(value,namespace) }
    }
    return text
}
// Borrow an admitted input snapshot while checking its scalar without another namespace-map copy.
internal fun srlXmlNamespaceChecked(text: String, list: Boolean, prefixes: Boolean, scope: Map<String,String>, maximum: Int): String =
    srlXmlNamespaceChecked(text,list,prefixes,{ scope[it] },maximum)
)xmlnamespace"

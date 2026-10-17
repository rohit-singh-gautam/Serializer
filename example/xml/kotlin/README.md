# Generated Kotlin XML example

Generate the shared model with `--language kotlin --config example/xml/codegen.ini`
and output `Schema.kt` in an ignored build directory. Build it with `Main.kt` using
`kotlinc Schema.kt Main.kt -include-runtime -d xml-kotlin.jar`.

Run `java -jar xml-kotlin.jar example/xml/order.xml OUTPUT.xml`. The generated
provider uses JAXP from the JDK, with DTD and external resolution disabled. Checks
cover mapping, round trip, independent binary codec and malformed input.

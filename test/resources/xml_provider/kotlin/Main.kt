import java.nio.file.Files
import java.nio.file.Path
import java.util.concurrent.Executors

private const val document = "<order xmlns='urn:serializer:xsd' id='7'><customer>R&amp;D</customer><value>3</value></order>"
private const val xsd_start = "<xs:schema xmlns:xs='http://www.w3.org/2001/XMLSchema'>"

// Require an adapter/provider failure before the operation can return a candidate.
private fun rejected(action: () -> Unit) {
    try { action() } catch (_: IllegalArgumentException) { return } catch (_: org.xml.sax.SAXException) { return }
    error("Provider accepted invalid input")
}

// Exercise real JAXP schema reuse through Kotlin's separately generated owning APIs.
fun main(args: Array<String>) {
    val schema = PreparedXmlSchema(Files.readAllBytes(Path.of(args[0])))
    val bytes = document.toByteArray(Charsets.UTF_8)
    val limits = XmlLimits()
    val value = XmlProviderFixtureOrder.decodeXml(bytes, validator = schema)
    check(value.id == 7u && value.customer == "R&D" && value.values.contentEquals(intArrayOf(3)))
    schema.validate(value.encodeXml(validator = schema), limits)
    // Cancellation activated by a successful real provider must prevent candidate publication.
    var completed = false
    val canceled = IllegalStateException("Canceled inside the prepared provider")
    val operationLimits = XmlLimits().also {
        it.srlPackageContext = SrlXmlBudget.Operation({ if (completed) throw canceled }, { _, _, _ -> })
    }
    val cancelingProvider = XmlValidator { data, policy -> schema.validate(data, policy); completed = true }
    for (decode in listOf(false, true)) {
        completed = false
        var failure: Throwable? = null
        try {
            if (decode) XmlProviderFixtureOrder.decodeXml(bytes, operationLimits, validator = cancelingProvider)
            else value.encodeXml(operationLimits, validator = cancelingProvider)
        } catch (error: Throwable) { failure = error }
        check(completed && failure === canceled)
    }
    for (bad in listOf("<wrong xmlns='urn:serializer:xsd'/>", document.replace("urn:serializer:xsd", "urn:wrong"),
        document + "<extra/>", document.replace("<value>3</value>", ""), "<!DOCTYPE order SYSTEM 'file:///must-not-open'>" + document)) {
        rejected { schema.validate(bad.toByteArray(Charsets.UTF_8), limits) }
    }
    val mismatch = PreparedXmlSchema("<xs:schema xmlns:xs='http://www.w3.org/2001/XMLSchema' targetNamespace='urn:serializer:xsd'><xs:element name='order' type='xs:string'/></xs:schema>".toByteArray(Charsets.UTF_8))
    rejected { value.encodeXml(validator = mismatch) }
    rejected { XmlProviderFixtureOrder.decodeXml(bytes, validator = mismatch) }
    for (directive in listOf("include", "import", "redefine", "override")) {
        for (location in listOf("file:///must-not-open.xsd", "https://invalid.example/must-not-open.xsd")) {
            rejected { PreparedXmlSchema((xsd_start + "<xs:" + directive + " schemaLocation='" + location + "'/></xs:schema>").toByteArray(Charsets.UTF_8)) }
        }
    }
    val hinted = document.replace(" id=", " xmlns:xsi='http://www.w3.org/2001/XMLSchema-instance' xsi:schemaLocation='urn:serializer:xsd https://invalid.example/must-not-open.xsd' id=")
    schema.validate(hinted.toByteArray(Charsets.UTF_8), limits)
    for (bounded in listOf(XmlLimits(maxBytes = bytes.size - 1), XmlLimits(maxNodes = 1), XmlLimits(maxDepth = 1), XmlLimits(maxTextBytes = 1))) {
        rejected { schema.validate(bytes, bounded) }
    }
    val workers = Executors.newFixedThreadPool(8)
    try {
        val tasks = (0 until 8).map {
            workers.submit {
                repeat(16) {
                    schema.validate(bytes, limits)
                    rejected { schema.validate("<wrong/>".toByteArray(Charsets.UTF_8), limits) }
                }
            }
        }
        tasks.forEach { it.get() }
    } finally { workers.shutdown() }
    println("Kotlin prepared XSD provider checks passed")
}

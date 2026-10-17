// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
import Foundation

// Supply an explicit failing trusted-validator hook to test generated direction/scope dispatch.
struct RejectingXmlValidator: XmlValidator {
    // Reject every payload; a generated operation must propagate this before publishing a result.
    func validate(_ bytes: [UInt8], _ limits: XmlLimits) throws { throw SerializerError.invalid("Explicit validator rejected input") }
}

// Qualify complete typed XML values against the independent binary codec.
var value = ShapesRecord()
value.big = UInt64.max
value.label = "\tX\r\n\u{1f600}"
value.flag = true
value.nested.value = Int16.min
var child = ShapesChild()
child.value = Int16.max
value.children = [child]
value.states = [.Zero, .One]
value.amount = .infinity
value.small = 1.25
let encoded = try value.encodeXml()
let copy = try ShapesRecord.decodeXml(encoded)
guard try copy.encode(.BINARY_NONE) == value.encode(.BINARY_NONE) else { throw SerializerError.invalid("Typed XML round trip failed") }
value.flag = nil
guard try ShapesRecord.decodeXml(value.encodeXml()).flag == nil else { throw SerializerError.invalid("Optional absence failed") }
let zero = "<record big='-000'><label>x</label><nested><value>1</value></nested><states>zero</states><states>one</states><amount>INF</amount><small>1.25</small></record>"
guard try ShapesRecord.decodeXml(Array(zero.utf8)).big == 0 else { throw SerializerError.invalid("Unsigned negative zero failed") }
let validator = RejectingXmlValidator()
var rejected = false
do { _ = try ShapesRecord.decodeXml(encoded, validator: validator) } catch { rejected = true }
guard rejected else { throw SerializerError.invalid("Input validator hook ignored") }
rejected = false
do { _ = try value.encodeXml(validator: validator) } catch { rejected = true }
guard rejected else { throw SerializerError.invalid("Output validator hook ignored") }
print("Swift generated typed XML and explicit validator hooks passed")

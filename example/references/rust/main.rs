// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
mod schema;
use schema::*;
use std::cell::RefCell;
use std::rc::Rc;

// Export original buffers and load their ordinary wire values into an owning consumer.
fn main() {
    // Independent shared values let unrelated parent records retain the same payload handle.
    let independent = Rc::new(RefCell::new(Payload::default()));
    independent.borrow_mut().values = vec![11.0];
    let mut first = SharedOwner::default();
    let mut second = SharedValueConsumer::default();
    first.b.bind(&independent).unwrap();
    second.d.bind(&independent).unwrap();
    first.b.with_value(|value| assert!(std::ptr::eq(value, &*independent.borrow()))).unwrap();
    drop(first);
    drop(independent);
    second.d.with_value(|value| assert_eq!(value.values, [11.0])).unwrap();

    let source = Rc::new(RefCell::new(Owner::default()));
    source.borrow_mut().b.values = vec![0.0, 1.0, 2.0];
    source.borrow_mut().b.codes = vec![0, 1, 2];
    // A plain application owner needs no Rc or participation in reference tracking for a borrow.
    let mut application_owner = Owner::default();
    application_owner.b.values = vec![9.0, 1.0, 2.0];
    application_owner.b.codes = vec![0, 1, 2];
    let mut borrowed = BorrowedConsumer::default();
    assert!(borrowed.encode(Protocol::Json).is_err());
    // Rust prevents owner mutation or destruction for the duration of this immutable borrow.
    borrowed.d.bind(&application_owner).unwrap();
    borrowed.d.with_value(|value| assert!(std::ptr::eq(value, &application_owner.b))).unwrap();
    let mut shared = SharedConsumer::default();
    shared.d.bind(&source).unwrap();
    let mut weak = WeakConsumer::default();
    weak.d.bind(&source).unwrap();
    source.borrow_mut().b.values[0] = 9.0;
    shared.d.with_value(|value| {
        assert_eq!(value.values.as_ptr(), source.borrow().b.values.as_ptr());
    }).unwrap();

    // This explicit copy exists only to provide an independent owning-codec baseline.
    let mut expected = OwningConsumer::default();
    expected.d = source.borrow().b.clone();
    for protocol in [Protocol::Json, Protocol::BinaryNone, Protocol::BinaryInteger, Protocol::BinaryString] {
        let bytes = expected.encode(protocol).unwrap();
        assert_eq!(borrowed.encode(protocol).unwrap(), bytes);
        assert_eq!(shared.encode(protocol).unwrap(), bytes);
        assert_eq!(weak.encode(protocol).unwrap(), bytes);
        {
            let original = source.borrow();
            original.with_as_consumer(|view| {
                assert!(std::ptr::eq(view.get_d(), &original.b));
                assert_eq!(view.encode(protocol).unwrap(), bytes);
            });
        }
        // Output-only references reject input; an ordinary target owns decoded buffers.
        assert!(SharedConsumer::decode(&bytes, protocol).is_err());
        let decoded = OwningConsumer::decode(&bytes, protocol).unwrap();
        assert_eq!(decoded.d.values, expected.d.values);
        assert_ne!(decoded.d.values.as_ptr(), source.borrow().b.values.as_ptr());
        source.borrow_mut().b.values[0] = 10.0;
        assert_eq!(decoded.encode(protocol).unwrap(), bytes);
        source.borrow_mut().b.values[0] = 9.0;
    }
    println!("{}", std::str::from_utf8(&shared.encode(Protocol::Json).unwrap()).unwrap());
    drop(source);
    assert!(shared.d.is_bound() && weak.d.is_bound());
    shared.d.unbind();
    borrowed.d.unbind();
    assert!(!borrowed.d.is_bound() && !weak.d.is_bound());
    assert!(shared.encode(Protocol::Json).is_err());
    assert!(weak.encode(Protocol::Json).is_err());
}

// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
mod schema;

use schema::*;
use std::cell::RefCell;
use std::rc::Rc;

// Keep the owner behind checked interior mutation and borrow its live field during projection.
fn main() {
  let storage = Rc::new(RefCell::new(Owner::default()));
  storage.borrow_mut().b.text = "original".into();
  storage.borrow_mut().b.count = 1;
  let mut links = Consumer::default();
  links.d.bind(&storage, "document-1", 7, 1).unwrap();
  let mut borrowed = BorrowedB::default();
  borrowed.bind(&storage, "document-1", 7, 1).unwrap();
  let mut weak = WeakB::default();
  weak.bind(&storage, "document-1", 7, 1).unwrap();

  storage.borrow_mut().b.text = "updated".into();
  storage.borrow_mut().b.count = 2;
  // The checked immutable borrow and the translator's source lifetime end in this scope.
  let payload = links.d.with_value(|value| {
    value.with_to_wire(|projected| projected.encode(Protocol::Json))
  }).unwrap().unwrap();
  let received = ProjectedValue::decode(&payload, Protocol::Json).unwrap();
  assert_eq!((received.label.as_str(), received.count), ("updated", 2));
  println!("{}", std::str::from_utf8(&payload).unwrap());

  // Owner construction and identity lookup belong to the application.
  let owner_bytes = storage.borrow().encode(Protocol::Json).unwrap();
  let loaded_owner = Rc::new(RefCell::new(Owner::decode(&owner_bytes, Protocol::Json).unwrap()));
  let mut loaded = Consumer::decode(&links.encode(Protocol::Json).unwrap(), Protocol::Json).unwrap();
  assert!(!loaded.d.is_bound());
  assert!(loaded.d.resolve(&loaded_owner, "document-1", 7, 1));
  assert_eq!(loaded.d.with_value(|value| value.text.clone()).unwrap(), "updated");

  // Shared retains an Rc; the other modes retain Weak handles.
  drop(storage);
  assert!(links.d.is_bound());
  links.d.unbind();
  assert!(!borrowed.is_bound() && !weak.is_bound());
}

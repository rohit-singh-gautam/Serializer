mod schema;
use schema::{CustomId, EntityBinding, Protocol, RegistryState};
use std::collections::HashSet;

const WIDGET_DOMAIN: u32 = 1;
const GADGET_DOMAIN: u32 = 2;

/// Own durable bindings and retired identities; all lookup keys contain the complete scope tuple.
struct HostRegistry {
    state: RegistryState,
}

/// Build an application identity without tying it to a receiver address or array position.
fn id(scope: u32, domain: u32, local: u64) -> CustomId {
    CustomId { scope, domain, local }
}

/// Return the complete durable key, suitable for application hash or database indexes.
fn key(value: &CustomId) -> (u32, u32, u64) {
    (value.scope, value.domain, value.local)
}

/// Reject zero identities and domain tags that this application does not recognize.
fn valid(value: &CustomId) -> bool {
    value.scope != 0 && value.local != 0 &&
        (value.domain == WIDGET_DOMAIN || value.domain == GADGET_DOMAIN)
}

impl HostRegistry {
    /// Validate decoded state before publishing it as a usable registry.
    fn restore(state: RegistryState) -> Result<Self, &'static str> {
        let mut identities = HashSet::new();
        for binding in &state.bindings {
            if !valid(&binding.id) || !identities.insert(key(&binding.id)) {
                return Err("Invalid or duplicate live identity");
            }
        }
        for retired in &state.retired {
            if !valid(retired) || !identities.insert(key(retired)) {
                return Err("Invalid, duplicate, or live retired identity");
            }
        }
        Ok(Self { state })
    }

    /// Locate a live binding only when both the exact scope and the expected domain match.
    fn locate(&self, value: &CustomId, expected_domain: u32) -> Option<usize> {
        if !valid(value) || value.domain != expected_domain {
            return None;
        }
        self.state.bindings.iter().position(|binding| key(&binding.id) == key(value))
    }

    /// Copy a new identity into owned state, rejecting live duplicates and durable tombstones.
    fn bind(&mut self, value: &CustomId, payload: u32) -> bool {
        if !valid(value) || self.state.bindings.iter().any(|binding| key(&binding.id) == key(value)) ||
            self.state.retired.iter().any(|retired| key(retired) == key(value)) {
            return false;
        }
        self.state.bindings.push(EntityBinding { id: value.clone(), payload });
        true
    }

    /// Read a payload without exposing mutable registry storage.
    fn lookup(&self, value: &CustomId, expected_domain: u32) -> Option<u32> {
        self.locate(value, expected_domain).map(|index| self.state.bindings[index].payload)
    }

    /// Replace only the payload; the durable identity stays unchanged.
    fn rebind(&mut self, value: &CustomId, expected_domain: u32, payload: u32) -> bool {
        if let Some(index) = self.locate(value, expected_domain) {
            self.state.bindings[index].payload = payload;
            return true;
        }
        false
    }

    /// Remove a live binding and preserve its identity permanently as a tombstone.
    fn retire(&mut self, value: &CustomId, expected_domain: u32) -> bool {
        if let Some(index) = self.locate(value, expected_domain) {
            self.state.retired.push(self.state.bindings.remove(index).id);
            return true;
        }
        false
    }
}

/// Qualify scope, domain, rebinding, tombstones and decoded policy across every wire protocol.
fn main() -> Result<(), Box<dyn std::error::Error>> {
    let first = id(7, WIDGET_DOMAIN, 42);
    let second = id(8, WIDGET_DOMAIN, 42);
    let gadget = id(7, GADGET_DOMAIN, 42);
    let copied = first.clone();
    assert_eq!(key(&copied), key(&first));
    let mut registry = HostRegistry::restore(RegistryState::default())?;
    assert!(registry.bind(&first, 10) && registry.bind(&second, 20) && registry.bind(&gadget, 30));
    assert!(!registry.bind(&first, 99));
    assert_eq!(registry.lookup(&first, WIDGET_DOMAIN), Some(10));
    assert_eq!(registry.lookup(&second, WIDGET_DOMAIN), Some(20));
    assert_eq!(registry.lookup(&gadget, GADGET_DOMAIN), Some(30));
    assert_eq!(registry.lookup(&first, GADGET_DOMAIN), None);
    assert_eq!(registry.lookup(&id(9, WIDGET_DOMAIN, 42), WIDGET_DOMAIN), None);
    assert_eq!(registry.lookup(&id(7, WIDGET_DOMAIN, 43), WIDGET_DOMAIN), None);
    assert!(!registry.bind(&id(0, WIDGET_DOMAIN, 42), 99));
    assert!(!registry.bind(&id(7, WIDGET_DOMAIN, 0), 99));
    assert!(!registry.bind(&id(7, 3, 42), 99));
    assert!(registry.rebind(&first, WIDGET_DOMAIN, 11));
    assert_eq!(registry.lookup(&copied, WIDGET_DOMAIN), Some(11));
    assert!(registry.retire(&first, WIDGET_DOMAIN));
    assert_eq!(registry.lookup(&first, WIDGET_DOMAIN), None);
    assert!(!registry.rebind(&first, WIDGET_DOMAIN, 99) && !registry.bind(&first, 99));

    let canonical = registry.state.encode(Protocol::BinaryNone)?;
    for protocol in [Protocol::Json, Protocol::BinaryNone, Protocol::BinaryInteger, Protocol::BinaryString] {
        let decoded = RegistryState::decode(&registry.state.encode(protocol)?, protocol)?;
        assert_eq!(decoded.encode(Protocol::BinaryNone)?, canonical);
        let mut restored = HostRegistry::restore(decoded)?;
        assert_eq!(restored.lookup(&second, WIDGET_DOMAIN), Some(20));
        assert_eq!(restored.lookup(&gadget, GADGET_DOMAIN), Some(30));
        assert!(!restored.bind(&first, 99));
    }
    let mut duplicate = registry.state.clone();
    duplicate.bindings.push(duplicate.bindings[0].clone());
    assert!(HostRegistry::restore(duplicate).is_err());
    let mut reused = registry.state.clone();
    reused.bindings.push(EntityBinding { id: first, payload: 99 });
    assert!(HostRegistry::restore(reused).is_err());
    println!("Scoped custom ID registry passed four protocols");
    Ok(())
}

# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Reject incomplete compiler-tool evidence and any implied full-backend qualification."""

import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('serializer_xsd_tooling', ROOT / 'qualification/xsd_tooling.py')
TOOLING = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(TOOLING)


def complete_report():
    """Declare every required semantic checkpoint without running SDKs in evidence-boundary unit tests."""
    bundle = []
    for identity in TOOLING.BUNDLE_IDS:
        rejected = TOOLING.bundle_rejects(identity)
        bundle.append({'id': identity, 'status': 'passed', 'expected': 'reject' if rejected else 'accept',
                       'returncode': 1 if rejected else 0, 'stderr': 'Expected diagnostic',
                       'expected_diagnostic': 'Expected diagnostic',
                       'before': {'input': 'unchanged'}, 'after': {'input': 'unchanged'}})
    comparison = [{'id': identity, 'native_contract': {'literal_native_bytes': 'verified'},
                   'classification': TOOLING.comparison_classification(identity),
                   'returncode': 0 if identity in TOOLING.COMPATIBLE_COMPARISONS else 1}
                  for identity in TOOLING.COMPARISON_IDS]
    exports = TOOLING.load_component('export_cases')
    families = []
    for name, expected in exports.FAMILIES.items():
        families.append({'name': name, 'native_contract_unchanged_after_rebind': True,
                         'staging_residue': [], 'authored_models': {'model': 'preserved'},
                         'fresh_imports': [{'root': root, 'models': {'fresh': 'independent'},
                                            'native_ids_are_independently_imported': True}
                                           for root in expected['roots']],
                         'documents': [{'valid': True}, {'valid': False}]})
    literals = json.loads((TOOLING.EVOLUTION / 'manifest.json').read_text(encoding='utf-8'))['native_hex']
    native = {name: hashlib.sha256(bytes.fromhex(payload)).hexdigest() for name, payload in literals.items()}
    sdk_models = {}
    for variant in TOOLING.VARIANTS:
        sdk_models[variant] = {'status': 'passed', 'source_byte_identical': True,
                               'source_sha256': TOOLING.sha(TOOLING.EVOLUTION / variant / 'order.serializer'),
                               'xsd_sha256': 'exported-schema', 'native_ast_sha256': 'native-contract',
                               'generated': {'model': 'unchanged'},
                               'original': {'native_sha256': dict(native)},
                               'rebound': {'native_sha256': dict(native)}}
    provider = {'artifacts': {'actual-provider': 'same'}, 'independent_xml_artifact': 'actual-lxml-extension'}
    binding_profiles = []
    proof_digest = '0' * 64
    for name in TOOLING.ENUM_BINDING_PROFILES:
        diagnostic = exports.BINDING_FAILURES[name]
        generations = [{'language': language, 'publication': publication,
                        'label': name + '-' + language + '-' + publication,
                        'returncode': 1, 'diagnostic': diagnostic,
                        'stdout': '', 'stderr': diagnostic,
                        'protected_snapshot': {'prior': 'unchanged'}, 'staging_residue': [],
                        'after_snapshot': {'prior': 'unchanged'},
                        'output_exists': publication == 'protected', 'depfile_exists': publication == 'protected'}
                       for language in ('cpp', 'python') for publication in ('fresh', 'protected')]
        binding_profiles.append({'profile': name, 'expected_diagnostic': diagnostic,
                                 'independent_xsd': {'label': name + '-xsd-valid', 'valid': True,
                                                     'entry': 'root.xsd', 'entry_sha256': proof_digest,
                                                     'fixture': 'valid.xml', 'sha256': proof_digest},
                                 'generations': generations})
    binding_controls = [{'label': 'enum-binding-control-' + language, 'language': language, 'returncode': 0,
                         'generated': 'model.' + language, 'sha256': proof_digest,
                         'depfile': 'model.d', 'depfile_sha256': proof_digest} for language in ('cpp', 'python')]
    return {'status': 'passed', 'scope': 'compiler_tooling_only', 'full_inventory': True,
            'runtime_followup_required': True, 'runtime_backend_completion': False, 'failures': [],
            'qualification_sources_sha256': TOOLING.source_snapshot(),
            'source_verification': {'changed': [], 'compiler_unchanged': True,
                                    'changed_artifacts': [], 'conflicting_artifacts': []},
            'providers_before': copy.deepcopy(provider), 'providers_after': copy.deepcopy(provider),
            'sdk_schema_directory': 'separately-generated-sdk-schemas', 'sdk_models': sdk_models,
            'modules': {
                'bundle_limits': {'status': 'passed', 'decisions': bundle},
                'comparison_cases': {'status': 'passed', 'decisions': comparison, 'decision_count': len(comparison)},
                'export_cases': {'status': 'passed', 'decision_ids': list(exports.required_ids()),
                                 'families': families,
                                 'enum_binding_profiles': binding_profiles, 'enum_binding_controls': binding_controls,
                                 'artifacts': {'root.xsd': proof_digest, 'valid.xml': proof_digest,
                                               'model.cpp': proof_digest, 'model.python': proof_digest, 'model.d': proof_digest},
                                 'unsupported_profiles': [{'profile': name, 'returncode': 1, 'diagnostic': 'Unsupported',
                                                           'protected_snapshot': {'prior': 'unchanged'}, 'staging_residue': []}
                                                          for name in exports.UNSUPPORTED for _ in range(2)],
                                 'fixture_inputs_unchanged': True, 'fixture_sha256': {'fixture': 'unchanged'}}},
            'decision_count': len(bundle) + len(comparison) + len(exports.required_ids())}


class tooling_guards(unittest.TestCase):
    """Exercise the maintained admission function with plausible but incomplete/contradictory reports."""

    def setUp(self):
        """Give each rejection control an independent complete inventory."""
        self.report = complete_report()

    def reject(self):
        """Require evidence admission to reject a modified nominal passed marker."""
        with self.assertRaises(AssertionError):
            TOOLING.admit_tooling(self.report)

    def test_complete_compiler_tool_report(self):
        """Every mandatory compiler checkpoint and both prepared models qualify tool scope only."""
        result = TOOLING.admit_tooling(self.report)
        self.assertEqual(result['modules'], 3)
        self.assertEqual(result['decisions'], self.report['decision_count'])

    def test_subset_diagnostic(self):
        """A passing diagnostic subset cannot close the default full tool gate."""
        self.report.update(status='diagnostic_passed', full_inventory=False)
        self.reject()

    def test_missing_module(self):
        """A full marker cannot replace an omitted independent component."""
        del self.report['modules']['comparison_cases']
        self.reject()

    def test_missing_case(self):
        """A partial positive/negative corpus cannot pass by retaining a module's passed marker."""
        self.report['modules']['bundle_limits']['decisions'].pop()
        self.reject()

    def test_duplicate_case(self):
        """A duplicate successful check cannot stand in for another required decision."""
        rows = self.report['modules']['comparison_cases']['decisions']
        rows[-1] = copy.deepcopy(rows[0])
        self.reject()

    def test_blocked_case(self):
        """Failed prerequisites remain failures even if all module IDs are present."""
        self.report['modules']['bundle_limits']['decisions'][0]['status'] = 'blocked'
        self.reject()

    def test_rejection_mislabeled_as_acceptance(self):
        """Matching ID counts cannot hide the absence of an actual required rejection."""
        row = next(row for row in self.report['modules']['bundle_limits']['decisions']
                   if TOOLING.bundle_rejects(row['id']))
        row.update(expected='accept', returncode=0)
        self.reject()

    def test_failed_publication_snapshot(self):
        """A negative exit cannot excuse an output change during a rejected operation."""
        row = next(row for row in self.report['modules']['bundle_limits']['decisions']
                   if TOOLING.bundle_rejects(row['id']))
        row['after'] = {'published': 'unexpected bytes'}
        self.reject()

    def test_empty_decision_inventory(self):
        """Two absent inventories are not evidence of complete compiler behavior."""
        self.report['modules']['export_cases']['decision_ids'] = []
        self.reject()

    def test_empty_export_families(self):
        """Semantic checkpoint labels cannot replace actual rich-family results."""
        self.report['modules']['export_cases']['families'] = []
        self.reject()

    def test_empty_independent_documents(self):
        """Nominal export success requires both independent valid and invalid document observations."""
        self.report['modules']['export_cases']['families'][0]['documents'] = []
        self.reject()

    def test_empty_comparison_native_proof(self):
        """Successful CLI exits cannot replace preserved native identities/defaults/behavior."""
        self.report['modules']['comparison_cases']['decisions'][0]['native_contract'] = {}
        self.reject()

    def test_missing_enum_binding_profile(self):
        """Six named attribute/text policies remain mandatory despite complete declared checkpoint labels."""
        self.report['modules']['export_cases']['enum_binding_profiles'].pop()
        self.reject()

    def test_duplicate_enum_binding_profile(self):
        """A repeated default check cannot stand in for a fixed/intersection binding failure."""
        rows = self.report['modules']['export_cases']['enum_binding_profiles']
        rows[-1] = copy.deepcopy(rows[0])
        self.reject()

    def test_missing_enum_generation(self):
        """Each failure must reject both backends with fresh and pre-existing destinations."""
        self.report['modules']['export_cases']['enum_binding_profiles'][0]['generations'].pop()
        self.reject()

    def test_duplicate_enum_generation(self):
        """Repeated backend/publication rows cannot substitute for the complete Cartesian control set."""
        rows = self.report['modules']['export_cases']['enum_binding_profiles'][0]['generations']
        rows[-1] = copy.deepcopy(rows[0])
        self.reject()

    def test_enum_generation_accepted(self):
        """A zero compiler exit cannot be called a successful negative binding control."""
        self.report['modules']['export_cases']['enum_binding_profiles'][0]['generations'][0]['returncode'] = 0
        self.reject()

    def test_enum_generation_wrong_diagnostic(self):
        """An unrelated compiler error cannot qualify the intended enum default/fixed preflight."""
        row = self.report['modules']['export_cases']['enum_binding_profiles'][0]['generations'][0]
        row.update(diagnostic='Unrelated error', stderr='Unrelated error')
        self.reject()

    def test_enum_generation_changed_protected_output(self):
        """Nonzero exits still fail qualification when rejected publication changes the protected tree."""
        self.report['modules']['export_cases']['enum_binding_profiles'][0]['generations'][0]['after_snapshot'] = {'new': 'bytes'}
        self.reject()

    def test_enum_fresh_output_was_published(self):
        """Fresh negative generation must leave its destination unpublished."""
        self.report['modules']['export_cases']['enum_binding_profiles'][0]['generations'][0]['output_exists'] = True
        self.reject()

    def test_enum_fresh_depfile_was_published(self):
        """A failed fresh bind cannot publish its dependency file separately."""
        self.report['modules']['export_cases']['enum_binding_profiles'][0]['generations'][0]['depfile_exists'] = True
        self.reject()

    def test_enum_protected_output_was_removed(self):
        """The protected-file negative control must retain both prior destination and dependencies."""
        self.report['modules']['export_cases']['enum_binding_profiles'][0]['generations'][1]['output_exists'] = False
        self.reject()

    def test_enum_independent_schema_rejected_fixture(self):
        """An invalid external XSD/XML pair cannot independently establish the binding-only failure."""
        self.report['modules']['export_cases']['enum_binding_profiles'][0]['independent_xsd']['valid'] = False
        self.reject()

    def test_enum_independent_schema_missing_hash(self):
        """Independent provider verdicts require exact actual schema and XML proof bytes."""
        self.report['modules']['export_cases']['enum_binding_profiles'][0]['independent_xsd']['entry_sha256'] = ''
        self.reject()

    def test_enum_control_missing_backend(self):
        """Known-name defaults and fixed values must remain accepted by both native output branches."""
        self.report['modules']['export_cases']['enum_binding_controls'].pop()
        self.reject()

    def test_enum_control_generation_failed(self):
        """A positive control cannot qualify merely because a generated path was recorded."""
        self.report['modules']['export_cases']['enum_binding_controls'][0]['returncode'] = 1
        self.reject()

    def test_enum_control_artifact_inventory_missing(self):
        """Nominal positive/negative markers cannot replace captured actual enum proof artifacts."""
        self.report['modules']['export_cases']['artifacts'] = {}
        self.reject()

    def test_changed_source(self):
        """A changed source digest cannot inherit an earlier successful execution."""
        key = next(iter(self.report['qualification_sources_sha256']))
        self.report['qualification_sources_sha256'][key] = 'changed'
        self.reject()

    def test_empty_source_inventory(self):
        """An empty source guard cannot qualify any claimed input revision."""
        self.report['qualification_sources_sha256'] = {}
        self.reject()

    def test_markdown_is_excluded_from_proof_inputs(self):
        """Final evidence prose can change without changing executable qualification inputs."""
        self.assertFalse(any(Path(name).suffix.lower() == '.md'
                             for name in self.report['qualification_sources_sha256']))

    def test_focused_native_proof_inputs_are_captured(self):
        """The enum/path/contract tests and their build registration are mandatory portable source keys."""
        expected = {'test/xsd_enum_export_test.cpp', 'test/xml_schema_test.cpp',
                    'test/xml_xsd_contract_test.cpp', 'test/CMakeLists.txt'}
        self.assertTrue(expected <= self.report['qualification_sources_sha256'].keys())

    def test_missing_native_proof_source(self):
        """A nominal clean snapshot cannot omit the new native enum export evidence."""
        del self.report['qualification_sources_sha256']['test/xsd_enum_export_test.cpp']
        self.reject()

    def test_changed_provider(self):
        """A provider mutation invalidates otherwise matching compiler decisions."""
        self.report['source_verification']['changed_artifacts'] = ['provider']
        self.reject()

    def test_conflicting_observations(self):
        """Later captures cannot hide an earlier differing provider digest."""
        artifacts, conflicts = TOOLING.merge_artifacts([{'provider': 'first'}, {'provider': 'later'}])
        self.assertEqual(len(conflicts), 1)
        self.assertEqual(set(artifacts.values()), {'first'})

    def test_missing_independent_provider(self):
        """An independent parser version string cannot replace its captured extension artifact."""
        self.report['providers_before']['artifacts'] = {}
        self.reject()

    def test_missing_sdk_schema(self):
        """Both exported versions must be available for the separate actual SDK replay."""
        del self.report['sdk_models']['v2']
        self.reject()

    def test_empty_native_inventory(self):
        """Equal empty before/after maps cannot prove unchanged native wire identities."""
        for label in ('original', 'rebound'):
            self.report['sdk_models']['v1'][label]['native_sha256'] = {}
        self.reject()

    def test_rewritten_authored_source(self):
        """Binding exported XSD must retain the exact authored native source bytes."""
        self.report['sdk_models']['v1']['source_byte_identical'] = False
        self.reject()

    def test_no_implied_backend_completion(self):
        """Compiler/Python checks never imply the pending eleven-SDK runtime followup passed."""
        self.report['runtime_backend_completion'] = True
        self.reject()

    def test_missing_runtime_followup_marker(self):
        """The compiler-tool report must explicitly retain its separate SDK qualification boundary."""
        self.report['runtime_followup_required'] = False
        self.reject()


if __name__ == '__main__':
    unittest.main()

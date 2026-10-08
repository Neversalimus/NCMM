#!/usr/bin/env python3
"""Independent source-graph regression for Survivor's production perk catalog.

This is deliberately independent of the bundled smoke_host copy in the payload.
It rejects missing/duplicate prerequisites, impossible unlock ordering, cycles
(including same-level cycles), missing mod Prime roots and accidental count drift.
"""
from __future__ import annotations

import collections
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
SOURCE = ROOT / "mods/SurvivorProgression/src/survivor_progression.cpp"
EXPECTED_COUNT = 378
ROW = re.compile(
    r'^\s*\{\s*"([^"]+)",\s*branch_id::(\w+),\s*\d+,\s*(\d+),'
    r'\s*currency_id::\w+,\s*"([^"]*)",\s*"([^"]*)",'
)
ROOTS = {
    "magiclysm": ("mg_prime_arcanist", "mg_prime_channeler", "mg_prime_warcaster"),
    "mindovermatter": ("mom_prime_kinetic", "mom_prime_overclock", "mom_prime_ascetic"),
    "xedra_evolved": ("xe_prime_analyst", "xe_prime_resonant", "xe_prime_riftwalker"),
    "aftershock_exoplanet": ("af_prime_smartgun", "af_prime_systems", "af_prime_phase"),
    "aftershock_prime": ("afp_prime_gunslinger", "afp_prime_systems_specialist", "afp_prime_translocator"),
    "secronom": ("sec_prime_hunter", "sec_prime_bulwark", "sec_prime_crimson"),
    "secronom_plus": ("secx_prime_architect", "secx_prime_predator", "secx_prime_vessel"),
}


def parse_catalog(source: str) -> dict[str, tuple[str, int, tuple[str, ...]]]:
    try:
        section = source.split("const perk_def perks[] = {", 1)[1].split("\n};", 1)[0]
    except IndexError as exc:
        raise ValueError("Production perk catalog declaration changed") from exc
    graph = {}
    for line in section.splitlines():
        if not line.strip().startswith('{ "'):
            continue
        match = ROW.search(line)
        if not match:
            raise ValueError("Unrecognized perk row: " + line[:140])
        ident, branch, level, first, second = match.groups()
        if ident in graph:
            raise ValueError("Duplicate perk: " + ident)
        graph[ident] = (branch, int(level), tuple(p for p in (first, second) if p))
    return graph


def check_graph(graph: dict, require_catalog: bool = True) -> None:
    if require_catalog and len(graph) != EXPECTED_COUNT:
        raise ValueError(f"Expected {EXPECTED_COUNT} perks, got {len(graph)}")
    for name, (_branch, level, deps) in graph.items():
        for dep in deps:
            if dep not in graph:
                raise ValueError(f"Missing prerequisite: {name} -> {dep}")
            if graph[dep][1] > level:
                raise ValueError(f"Unreachable prerequisite level: {name} -> {dep}")
    # 0=unvisited; 1=active DFS stack; 2=finished. Catch cycles at equal levels.
    state = {}
    def visit(name: str, stack: tuple[str, ...]) -> None:
        if state.get(name) == 2:
            return
        if state.get(name) == 1:
            raise ValueError("Prerequisite cycle: " + " -> ".join(stack + (name,)))
        state[name] = 1
        for dep in graph[name][2]:
            visit(dep, stack + (name,))
        state[name] = 2
    for perk in graph:
        visit(perk, ())
    if require_catalog:
        for mod, roots in ROOTS.items():
            for ident in roots:
                if ident not in graph:
                    raise ValueError(f"Missing {mod} Prime root: {ident}")
            if len(set(roots)) != 3:
                raise ValueError(f"Duplicate {mod} Prime root")



def check_prime_contracts(source: str, graph: dict) -> None:
    """Verify the 21 mod Prime roots against production gating and identity."""
    try:
        slot_body = source.split("int mod_prime_root_slot(", 1)[1].split(
            "bool mod_prime_specialization_root(", 1
        )[0]
    except IndexError as exc:
        raise ValueError("Production mod Prime slot resolver changed") from exc
    slots: dict[str, int] = {}
    for condition, slot in re.findall(
        r"if\s*\((.*?)\)\s*return\s+([123]);", slot_body, flags=re.DOTALL
    ):
        for ident in re.findall(r'id\s*==\s*"([^"]+)"', condition):
            if ident in slots:
                raise ValueError(f"Duplicate mod Prime slot mapping: {ident}")
            slots[ident] = int(slot)
    expected_slots = {
        ident: slot for roots in ROOTS.values()
        for slot, ident in enumerate(roots, start=1)
    }
    if slots != expected_slots:
        raise ValueError(
            f"Mod Prime slot mapping drift: missing={set(expected_slots.items()) - set(slots.items())}, "
            f"unexpected={set(slots.items()) - set(expected_slots.items())}"
        )

    integration_rows: dict[str, list[str]] = collections.defaultdict(list)
    for ident, mod in re.findall(
        r'\{\s*"([^"]+)",\s*integration_id::(\w+)\s*\}', source
    ):
        integration_rows[ident].append(mod)

    for mod, roots in ROOTS.items():
        shared_prerequisites = None
        for ident in roots:
            if ident not in graph:
                raise ValueError(f"Missing Prime catalog row: {ident}")
            rows = re.findall(
                rf'^\s*\{{\s*"{re.escape(ident)}",\s*branch_id::(\w+),\s*'
                r'(\d+),\s*(\d+),\s*currency_id::(\w+),\s*"([^"]*)",\s*"([^"]*)",',
                source, flags=re.MULTILINE
            )
            if len(rows) != 1:
                raise ValueError(f"Expected exactly one complete Prime definition: {ident}")
            branch, tier, level, currency, first, second = rows[0]
            if (branch, int(tier), int(level), currency) != ("mastery", 9, 45, "major"):
                raise ValueError(f"Mod Prime unlock/currency drift: {ident}")
            if not first or graph[ident][2] != tuple(p for p in (first, second) if p):
                raise ValueError(f"Mod Prime prerequisite drift: {ident}")
            if shared_prerequisites is None:
                shared_prerequisites = (first, second)
            elif (first, second) != shared_prerequisites:
                raise ValueError(f"Prime roots disagree on prerequisites: {mod}")
            if integration_rows.get(ident) != [mod]:
                raise ValueError(f"Mod Prime integration mapping drift: {ident}")


def prime_mutation_tests(source: str, graph: dict) -> None:
    mutations = {
        "slot": (
            'id == "mg_prime_arcanist"',
            'id == "mg_prime_arcanist_wrong"'
        ),
        "integration": (
            '{ "secx_prime_vessel", integration_id::secronom_plus }',
            '{ "secx_prime_vessel", integration_id::magiclysm }'
        ),
        "currency": (
            '{ "mg_prime_arcanist", branch_id::mastery, 9, 45, currency_id::major',
            '{ "mg_prime_arcanist", branch_id::mastery, 9, 45, currency_id::perk'
        ),
    }
    for name, (old, new) in mutations.items():
        if source.count(old) != 1:
            raise AssertionError(f"Prime mutation fixture changed: {name}")
        try:
            check_prime_contracts(source.replace(old, new, 1), graph)
        except ValueError:
            continue
        raise AssertionError(f"Prime {name} regression was accepted")


def self_test() -> None:
    fixture = {
        "a": ("combat", 5, ("b",)),
        "b": ("combat", 5, ("a",)),
    }
    try:
        check_graph(fixture, require_catalog=False)
    except ValueError as exc:
        assert "cycle" in str(exc).lower()
    else:
        raise AssertionError("Cycle mutation was accepted")
    check_graph({"a": ("combat", 1, ()), "b": ("combat", 5, ("a",))}, require_catalog=False)
    try:
        check_graph({"a": ("combat", 1, ("b",)), "b": ("combat", 5, ())}, require_catalog=False)
    except ValueError as exc:
        assert "level" in str(exc).lower()
    else:
        raise AssertionError("Level regression was accepted")


def main() -> int:
    self_test()
    source = SOURCE.read_text(encoding="utf-8-sig")
    graph = parse_catalog(source)
    check_graph(graph)
    check_prime_contracts(source, graph)
    prime_mutation_tests(source, graph)
    edges = sum(len(v[2]) for v in graph.values())
    print(f"Survivor source DAG: PASS ({len(graph)} perks, {edges} prerequisite edges, "
          f"{len(ROOTS) * 3} integration Prime roots (slots, currency, gates, ownership), "
          "mutation tests PASS)")
    return 0


if __name__ == "__main__":
    sys.exit(main())

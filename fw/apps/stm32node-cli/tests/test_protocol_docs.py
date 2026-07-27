"""PROTOCOL.md must stay in sync with the spec (run `task proto` to refresh)."""

from __future__ import annotations

from stm32node_cli.protocol import gen_docs


def test_protocol_md_is_up_to_date():
    assert gen_docs.PROTOCOL_MD.exists(), "PROTOCOL.md missing - run `task proto`"
    on_disk = gen_docs.PROTOCOL_MD.read_text(encoding="utf-8")
    assert on_disk == gen_docs.render(), "PROTOCOL.md is stale - run `task proto`"

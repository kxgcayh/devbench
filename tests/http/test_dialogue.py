"""Read the existing conversation; no actor activation or topic selection by default."""

import os

import pytest

from conftest import require_tool


@pytest.fixture
def dialogue(tool_schema):
    require_tool(tool_schema, "dialogue")


def test_inspect_reports_closed_or_supported_or_unknown_layout(client, dialogue):
    view = client.ok("dialogue", {"action": "inspect"})
    assert isinstance(view["open"], bool), view
    assert isinstance(view["supported"], bool), view
    assert isinstance(view["ready"], bool), view
    assert isinstance(view["topics"], list), view
    if not view["open"]:
        assert not view["supported"] and not view["ready"], view
    elif view["supported"]:
        assert isinstance(view["snapshot"], str) and view["snapshot"], view
        for index, topic in enumerate(view["topics"]):
            assert topic["index"] == index and isinstance(topic["text"], str), view
    else:
        assert view["reason"] and not view["ready"], view


def test_invalid_action_is_400(client, dialogue):
    for action in ("skip", "force", 12, None):
        status, body = client.call("dialogue", {"action": action})
        assert status == 400, body


def test_stale_snapshot_cannot_select_a_topic(client, dialogue):
    view = client.ok("dialogue", {})
    if not view["ready"]:
        pytest.skip("needs an open dialogue topic list ready for input")
    status, body = client.call(
        "dialogue", {"action": "select", "index": 0, "snapshot": "deliberately-stale"}
    )
    assert status == 409, body


def test_select_explicitly_chosen_topic(client, dialogue):
    raw_index = os.environ.get("DEVBENCH_TEST_DIALOGUE_SELECT_INDEX")
    if raw_index is None:
        pytest.skip("opt in with DEVBENCH_TEST_DIALOGUE_SELECT_INDEX; choosing a topic changes gameplay")
    view = client.ok("dialogue", {})
    assert view["ready"], view
    index = int(raw_index)
    assert 0 <= index < len(view["topics"]), view
    body = client.ok("dialogue", {"action": "select", "index": index, "snapshot": view["snapshot"]})
    assert body["submitted"] is True and body["completed"] is False, body
    assert body["topic"] == view["topics"][index], body
